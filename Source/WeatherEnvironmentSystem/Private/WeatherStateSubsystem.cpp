// Copyright James Joslin. All Rights Reserved.

#include "WeatherStateSubsystem.h"

#include "Actors/WeatherEnvironmentController.h"
#include "Actors/WeatherWindDirector.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

void UWeatherStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentDateTime = FDateTime(2026, 6, 21, 8, 0, 0);
	TimeScale = 60.0;
	bPaused = false;
	bInitialClockSettingsApplied = false;
	WeatherGrid.Clear();
	bLastGridBuildSucceeded = false;
	LastGridBuildMessage = TEXT("Grid has not been built.");
	WindSettings = FWeatherWindSettings();
	WindSimulationTimeSeconds = 0.0;
	WindSimulationAccumulator = 0.0f;
	bWindConfigured = false;
	bWindFieldDirty = false;
	WindFieldTexture = nullptr;
	WindMaterialParameterCollection = nullptr;
	LastWindFieldPixels.Reset();
	WindDirector.Reset();
	ActiveController.Reset();
}

void UWeatherStateSubsystem::Deinitialize()
{
	WeatherGrid.Clear();
	WindFieldTexture = nullptr;
	WindMaterialParameterCollection = nullptr;
	LastWindFieldPixels.Reset();
	WindDirector.Reset();
	ActiveController.Reset();
	Super::Deinitialize();
}

void UWeatherStateSubsystem::Tick(const float DeltaTime)
{
	if (!bPaused && TimeScale > 0.0 && DeltaTime > 0.0f)
	{
		AdvanceWorldSeconds(static_cast<double>(DeltaTime) * TimeScale);
	}

	TickWind(DeltaTime);
}

TStatId UWeatherStateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWeatherStateSubsystem, STATGROUP_Tickables);
}

UWorld* UWeatherStateSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

bool UWeatherStateSubsystem::IsTickable() const
{
	return !IsTemplate();
}

void UWeatherStateSubsystem::InitializeClock(
	const FWeatherClockSettings& Settings,
	const bool bForceReset)
{
	if (bInitialClockSettingsApplied && !bForceReset)
	{
		return;
	}

	FDateTime Start;
	if (!Settings.StartDateTime.ToDateTime(Start))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Weather clock rejected invalid start date/time; retaining %s."),
			*CurrentDateTime.ToString());
		return;
	}

	SetNativeDateTime(Start, bInitialClockSettingsApplied);
	TimeScale = FMath::Max(0.0, Settings.TimeScale);
	bPaused = Settings.bStartPaused;
	bInitialClockSettingsApplied = true;
}

bool UWeatherStateSubsystem::SetDateTime(const FWeatherDateTime& NewDateTime)
{
	FDateTime Native;
	if (!NewDateTime.ToDateTime(Native))
	{
		return false;
	}

	SetNativeDateTime(Native, true);
	bInitialClockSettingsApplied = true;
	return true;
}

void UWeatherStateSubsystem::AdvanceWorldSeconds(const double WorldSeconds)
{
	if (FMath::IsNearlyZero(WorldSeconds))
	{
		return;
	}

	const FDateTime Previous = CurrentDateTime;
	const FTimespan Delta = FTimespan::FromSeconds(WorldSeconds);

	const FDateTime Minimum = FDateTime::MinValue();
	const FDateTime Maximum = FDateTime::MaxValue();
	if (Delta.GetTicks() > 0 && CurrentDateTime > Maximum - Delta)
	{
		CurrentDateTime = Maximum;
	}
	else if (Delta.GetTicks() < 0 && CurrentDateTime < Minimum - Delta)
	{
		CurrentDateTime = Minimum;
	}
	else
	{
		CurrentDateTime += Delta;
	}

	BroadcastBoundaryChanges(Previous, CurrentDateTime);
}

void UWeatherStateSubsystem::SetTimeScale(const double NewTimeScale)
{
	TimeScale = FMath::Max(0.0, NewTimeScale);
}

void UWeatherStateSubsystem::SetClockPaused(const bool bNewPaused)
{
	bPaused = bNewPaused;
}

FWeatherDateTime UWeatherStateSubsystem::GetCurrentDateTime() const
{
	return FWeatherDateTime::FromDateTime(CurrentDateTime);
}

double UWeatherStateSubsystem::GetNormalizedDayFraction() const
{
	return GetCurrentDateTime().GetNormalizedDayFraction();
}

FString UWeatherStateSubsystem::GetClockDisplayString(const bool bIncludeSeconds) const
{
	return GetCurrentDateTime().ToDisplayString(bIncludeSeconds);
}

bool UWeatherStateSubsystem::RebuildGridFromLandscape(
	const FWeatherGridDefinition& Definition)
{
	return RebuildGridFromLandscapeSources(Definition, TArray<ALandscapeProxy*>());
}

bool UWeatherStateSubsystem::RebuildGridFromBounds(
	const FBox& WorldBounds,
	const FWeatherGridDefinition& Definition)
{
	FString Message;
	const bool bBuilt = WeatherGrid.Rebuild(WorldBounds, Definition, &Message);
	bLastGridBuildSucceeded = bBuilt;
	LastGridBuildMessage = Message;
	if (bBuilt)
	{
		UE_LOG(LogTemp, Display, TEXT("WeatherEnvironment: %s"), *Message);
		bWindFieldDirty = true;
		if (bWindConfigured)
		{
			ForceWindUpdate();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("WeatherEnvironment: %s"), *Message);
	}
	return bBuilt;
}

bool UWeatherStateSubsystem::RebuildGridFromLandscapeSources(
	const FWeatherGridDefinition& Definition,
	const TArray<ALandscapeProxy*>& LandscapeSources)
{
	FBox SourceBounds(ForceInit);
	FString Message;
	if (!FWeatherGrid::ResolveSourceBounds(
		GetWorld(),
		Definition,
		LandscapeSources,
		SourceBounds,
		Message))
	{
		bLastGridBuildSucceeded = false;
		LastGridBuildMessage = Message;
		UE_LOG(LogTemp, Warning, TEXT("WeatherEnvironment: %s"), *Message);
		return false;
	}

	if (!Message.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("WeatherEnvironment: %s"), *Message);
	}

	return RebuildGridFromBounds(SourceBounds, Definition);
}

void UWeatherStateSubsystem::ClearGrid()
{
	WeatherGrid.Clear();
	LastWindFieldPixels.Reset();
	bWindFieldDirty = true;
	bLastGridBuildSucceeded = false;
	LastGridBuildMessage = TEXT("Grid cleared. Adjust the grid definition if needed, then press Rebuild Grid.");
	PublishWindMaterialParameters();
}

bool UWeatherStateSubsystem::WorldToCell(
	const FVector& WorldLocation,
	FWeatherCellCoord& OutCell) const
{
	return WeatherGrid.WorldToCell(WorldLocation, OutCell);
}

bool UWeatherStateSubsystem::CellToWorld(
	const FWeatherCellCoord Cell,
	FVector& OutWorldLocation) const
{
	return WeatherGrid.CellToWorld(Cell, OutWorldLocation);
}

bool UWeatherStateSubsystem::IsValidCell(const FWeatherCellCoord Cell) const
{
	return WeatherGrid.IsValidCell(Cell);
}

bool UWeatherStateSubsystem::GetCellState(
	const FWeatherCellCoord Cell,
	FWeatherCellState& OutState) const
{
	return WeatherGrid.GetCellState(Cell, OutState);
}

FWeatherSample UWeatherStateSubsystem::GetWeatherAtLocation(
	const FVector& WorldLocation) const
{
	return WeatherGrid.GetWeatherAtLocation(WorldLocation);
}

TArray<FWeatherCellState> UWeatherStateSubsystem::GetCellStatesForBounds(
	const FBox& WorldBounds) const
{
	TArray<FWeatherCellCoord> Coordinates;
	WeatherGrid.GetCellsIntersectingBounds(WorldBounds, Coordinates);

	TArray<FWeatherCellState> States;
	States.Reserve(Coordinates.Num());
	for (const FWeatherCellCoord& Coordinate : Coordinates)
	{
		if (const FWeatherCellState* State = WeatherGrid.FindCell(Coordinate))
		{
			States.Add(*State);
		}
	}
	return States;
}

void UWeatherStateSubsystem::ConfigureWind(const FWeatherWindSettings& Settings)
{
	WindSettings = Settings;
	WindSettings.FixedUpdateIntervalSeconds = FMath::Max(WindSettings.FixedUpdateIntervalSeconds, 0.01f);
	WindSettings.MaximumWindSpeed = FMath::Max(WindSettings.MaximumWindSpeed, 1.0f);
	WindSettings.BaseWindSpeed = FMath::Clamp(
		WindSettings.BaseWindSpeed,
		0.0f,
		WindSettings.MaximumWindSpeed);
	WindSimulationAccumulator = 0.0f;
	bWindConfigured = true;
	bWindFieldDirty = true;

	WindFieldTexture = WindSettings.FieldTexture.LoadSynchronous();
	WindMaterialParameterCollection = WindSettings.MaterialParameterCollection.LoadSynchronous();
	EnsureWindFieldTexture();
	ForceWindUpdate();
}

void UWeatherStateSubsystem::SetWindDirector(AWeatherWindDirector* NewWindDirector)
{
	WindDirector = IsValid(NewWindDirector) ? NewWindDirector : nullptr;
	bWindFieldDirty = true;
	if (bWindConfigured)
	{
		ForceWindUpdate();
	}
}

AWeatherWindDirector* UWeatherStateSubsystem::GetWindDirector() const
{
	return WindDirector.Get();
}

void UWeatherStateSubsystem::ForceWindUpdate()
{
	if (!bWindConfigured || !WindSettings.bEnabled)
	{
		PublishWindMaterialParameters();
		return;
	}

	StepWind(WindSettings.FixedUpdateIntervalSeconds);
}

bool UWeatherStateSubsystem::GetWindAtLocation(
	const FVector& WorldLocation,
	FVector& OutWindVector,
	float& OutGust) const
{
	const FWeatherSample Sample = WeatherGrid.GetWeatherAtLocation(WorldLocation);
	if (!Sample.bIsValid)
	{
		OutWindVector = FVector::ZeroVector;
		OutGust = 0.0f;
		return false;
	}

	OutWindVector = Sample.State.WindVector;
	OutGust = Sample.State.WindGust;
	return true;
}

FLinearColor UWeatherStateSubsystem::GetWindFieldOriginSize() const
{
	const FWeatherGridInfo& Info = WeatherGrid.GetInfo();
	if (!Info.bIsValid)
	{
		return FLinearColor::Transparent;
	}

	const FVector Size = Info.GridBounds.GetSize();
	return FLinearColor(
		static_cast<float>(Info.GridBounds.Min.X),
		static_cast<float>(Info.GridBounds.Min.Y),
		static_cast<float>(Size.X),
		static_cast<float>(Size.Y));
}

void UWeatherStateSubsystem::TickWind(const float DeltaTime)
{
	if (!bWindConfigured || !WindSettings.bEnabled || DeltaTime <= 0.0f)
	{
		return;
	}

	const float Interval = FMath::Max(WindSettings.FixedUpdateIntervalSeconds, 0.01f);
	WindSimulationAccumulator += DeltaTime;
	int32 Steps = 0;
	while (WindSimulationAccumulator >= Interval && Steps < 4)
	{
		StepWind(Interval);
		WindSimulationAccumulator -= Interval;
		++Steps;
	}

	if (Steps == 4 && WindSimulationAccumulator >= Interval)
	{
		WindSimulationAccumulator = FMath::Fmod(WindSimulationAccumulator, Interval);
	}
}

void UWeatherStateSubsystem::StepWind(const float StepSeconds)
{
	const FWeatherGridInfo& Info = WeatherGrid.GetInfo();
	if (!Info.bIsValid || WeatherGrid.GetMutableCells().IsEmpty())
	{
		PublishWindMaterialParameters();
		return;
	}

	WindSimulationTimeSeconds += FMath::Max(StepSeconds, 0.0f);
	const AWeatherWindDirector* Director = WindDirector.Get();
	const FVector DirectorForward = Director
		? Director->GetActorForwardVector()
		: WindSettings.DefaultDirection;
	const FVector DefaultDirection = FVector(
		WindSettings.DefaultDirection.X,
		WindSettings.DefaultDirection.Y,
		0.0).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	for (FWeatherCellState& State : WeatherGrid.GetMutableCells())
	{
		const FVector DirectorLocation = Director
			? Director->GetActorLocation()
			: State.WorldCenter + DefaultDirection * FMath::Max(WindSettings.DirectorDeadZoneRadius + 1.0f, 1.0f);
		float TargetGust = 0.0f;
		const FVector TargetWind = FWeatherWindMath::CalculateTargetWind(
			State.WorldCenter,
			DirectorLocation,
			DirectorForward,
			State.WindVector,
			WindSimulationTimeSeconds,
			WindSettings,
			TargetGust);
		State.WindVector = FWeatherWindMath::SmoothWind(
			State.WindVector,
			TargetWind,
			StepSeconds,
			WindSettings.DirectionSmoothingRate);

		const float GustAlpha = WindSettings.DirectionSmoothingRate <= 0.0f
			? 1.0f
			: 1.0f - FMath::Exp(-WindSettings.DirectionSmoothingRate * FMath::Max(StepSeconds, 0.0f));
		State.WindGust = FMath::Lerp(
			State.WindGust,
			TargetGust,
			FMath::Clamp(GustAlpha, 0.0f, 1.0f));
	}

	bWindFieldDirty = true;
	UpdateWindFieldTexture();
	PublishWindMaterialParameters();
}

void UWeatherStateSubsystem::EnsureWindFieldTexture()
{
	if (!WindFieldTexture)
	{
		WindFieldTexture = UTexture2D::CreateTransient(
			64,
			64,
			PF_B8G8R8A8,
			TEXT("WeatherWindFieldRuntime"));
	}

	if (!WindFieldTexture)
	{
		return;
	}

	WindFieldTexture->SRGB = false;
	WindFieldTexture->Filter = TF_Bilinear;
	WindFieldTexture->AddressX = TA_Clamp;
	WindFieldTexture->AddressY = TA_Clamp;
	WindFieldTexture->NeverStream = true;
	WindFieldTexture->UpdateResource();
	LastWindFieldPixels.Reset();
}

void UWeatherStateSubsystem::UpdateWindFieldTexture()
{
	if (!bWindFieldDirty || !WindFieldTexture || !WeatherGrid.GetInfo().bIsValid)
	{
		return;
	}

	const int32 Width = WindFieldTexture->GetSizeX();
	const int32 Height = WindFieldTexture->GetSizeY();
	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	const FWeatherGridInfo& Info = WeatherGrid.GetInfo();
	const FVector FieldSize = Info.GridBounds.GetSize();
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FVector SampleLocation(
				Info.GridBounds.Min.X + (static_cast<double>(X) + 0.5) / Width * FieldSize.X,
				Info.GridBounds.Min.Y + (static_cast<double>(Y) + 0.5) / Height * FieldSize.Y,
				Info.GridBounds.GetCenter().Z);
			const FWeatherSample Sample = WeatherGrid.GetWeatherAtLocation(SampleLocation);
			Pixels[Y * Width + X] = Sample.bIsValid
				? FWeatherWindMath::EncodeFieldTexel(
					Sample.State.WindVector,
					Sample.State.WindGust,
					WindSettings.MaximumWindSpeed)
				: FColor(255, 128, 0, 0);
		}
	}

	if (Pixels == LastWindFieldPixels)
	{
		bWindFieldDirty = false;
		return;
	}

	const int64 DataSize = static_cast<int64>(Pixels.Num()) * sizeof(FColor);
	uint8* UploadData = new uint8[DataSize];
	FMemory::Memcpy(UploadData, Pixels.GetData(), DataSize);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
	WindFieldTexture->UpdateTextureRegions(
		0,
		1,
		Region,
		Width * sizeof(FColor),
		sizeof(FColor),
		UploadData,
		[](uint8* Data, const FUpdateTextureRegion2D* Regions)
		{
			delete[] Data;
			delete Regions;
		});
	LastWindFieldPixels = MoveTemp(Pixels);
	bWindFieldDirty = false;
}

void UWeatherStateSubsystem::PublishWindMaterialParameters()
{
	UWorld* World = GetWorld();
	if (!World || !WindMaterialParameterCollection)
	{
		return;
	}

	UMaterialParameterCollectionInstance* Instance =
		World->GetParameterCollectionInstance(WindMaterialParameterCollection);
	if (!Instance)
	{
		return;
	}

	const FLinearColor OriginSize = GetWindFieldOriginSize();
	Instance->SetVectorParameterValue(TEXT("WeatherFieldOriginSize"), OriginSize);

	FVector LocalWind = WindSettings.DefaultDirection.GetSafeNormal() * WindSettings.BaseWindSpeed;
	float LocalGust = 0.0f;
	float LocalRain = 0.0f;
	float LocalStorminess = 0.0f;
	const FWeatherGridInfo& Info = WeatherGrid.GetInfo();
	if (Info.bIsValid)
	{
		FVector LocalLocation = Info.GridBounds.GetCenter();
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			LocalLocation.X = Camera->GetCameraLocation().X;
			LocalLocation.Y = Camera->GetCameraLocation().Y;
		}

		const FWeatherSample Sample = WeatherGrid.GetWeatherAtLocation(LocalLocation);
		if (Sample.bIsValid)
		{
			LocalWind = Sample.State.WindVector;
			LocalGust = Sample.State.WindGust;
			LocalRain = Sample.State.RainIntensity;
			LocalStorminess = Sample.State.Storminess;
		}
	}

	const FVector LocalDirection = FVector(LocalWind.X, LocalWind.Y, 0.0)
		.GetSafeNormal(
			UE_SMALL_NUMBER,
			WindSettings.DefaultDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
	const float LocalSpeed = LocalWind.Size2D();
	Instance->SetVectorParameterValue(
		TEXT("WeatherLocalWind"),
		FLinearColor(
			static_cast<float>(LocalDirection.X),
			static_cast<float>(LocalDirection.Y),
			LocalSpeed,
			FMath::Clamp(LocalSpeed / FMath::Max(WindSettings.MaximumWindSpeed, 1.0f), 0.0f, 1.0f)));
	Instance->SetScalarParameterValue(TEXT("WeatherLocalGust"), LocalGust);
	Instance->SetScalarParameterValue(TEXT("WeatherLocalRain"), LocalRain);
	Instance->SetScalarParameterValue(TEXT("WeatherLocalStorminess"), LocalStorminess);

	// These are authored base values and conversion constants, not values derived
	// from the player-local fallback. Legacy foliage functions combine them with
	// MF_WeatherWindSample so every foliage instance receives the exact same
	// displacement response evaluated at its own world-space location. Their
	// authored time rates remain stable inside the migrated legacy functions.
	const FWeatherFoliageSpatialMaterialState SpatialFoliage =
		FWeatherWindMath::BuildFoliageSpatialMaterialState(
			WindSettings.MaximumWindSpeed,
			WindSettings.FoliageMaterials);
	Instance->SetVectorParameterValue(TEXT("WeatherFoliageMapping"), SpatialFoliage.Mapping);
	Instance->SetVectorParameterValue(TEXT("WeatherFoliageBase"), SpatialFoliage.Base);
	Instance->SetVectorParameterValue(TEXT("WeatherFoliageGustBase"), SpatialFoliage.GustBase);

	if (WindSettings.FoliageMaterials.bPublishCompatibilityParameters)
	{
		const FWeatherFoliageMaterialState Foliage = FWeatherWindMath::BuildFoliageMaterialState(
			LocalDirection,
			LocalSpeed,
			LocalGust,
			WindSettings.FoliageMaterials);
		Instance->SetScalarParameterValue(TEXT("Grass Wind Small Size"), Foliage.GrassWindSmallSize);
		Instance->SetScalarParameterValue(TEXT("Grass Wind Large Size"), Foliage.GrassWindLargeSize);
		Instance->SetScalarParameterValue(
			TEXT("Grass Wind Small Amplification"),
			Foliage.GrassWindSmallAmplification);
		Instance->SetScalarParameterValue(
			TEXT("Grass Wind Large Amplification"),
			Foliage.GrassWindLargeAmplification);
		Instance->SetScalarParameterValue(TEXT("Simple Wind Intensity"), Foliage.SimpleWindIntensity);
		Instance->SetScalarParameterValue(TEXT("Simple Wind Speed"), Foliage.SimpleWindSpeed);
		Instance->SetScalarParameterValue(TEXT("Wind Sway Gradient"), Foliage.WindSwayGradient);
		Instance->SetScalarParameterValue(
			TEXT("Wind Sway Gust Frequency"),
			Foliage.WindSwayGustFrequency);
		Instance->SetScalarParameterValue(TEXT("Wind Sway Intensity"), Foliage.WindSwayIntensity);
		Instance->SetScalarParameterValue(TEXT("Wind Sway Offset"), Foliage.WindSwayOffset);
		Instance->SetVectorParameterValue(TEXT("Wind Sway Direction"), Foliage.WindSwayDirection);
	}
}

bool UWeatherStateSubsystem::RegisterController(AWeatherEnvironmentController* Controller)
{
	if (!IsValid(Controller))
	{
		return false;
	}

	if (ActiveController.IsValid() && ActiveController.Get() != Controller)
	{
		return false;
	}

	ActiveController = Controller;
	return true;
}

void UWeatherStateSubsystem::UnregisterController(AWeatherEnvironmentController* Controller)
{
	if (ActiveController.Get() == Controller)
	{
		ActiveController.Reset();
	}
}

AWeatherEnvironmentController* UWeatherStateSubsystem::GetActiveController() const
{
	return ActiveController.Get();
}

void UWeatherStateSubsystem::SetNativeDateTime(
	const FDateTime& NewDateTime,
	const bool bBroadcastBoundaries)
{
	const FDateTime Previous = CurrentDateTime;
	CurrentDateTime = NewDateTime;

	if (bBroadcastBoundaries)
	{
		BroadcastBoundaryChanges(Previous, CurrentDateTime);
	}
}

void UWeatherStateSubsystem::BroadcastBoundaryChanges(
	const FDateTime& Previous,
	const FDateTime& Current)
{
	const FWeatherDateTime BlueprintDateTime = FWeatherDateTime::FromDateTime(Current);

	if (Previous.GetYear() != Current.GetYear()
		|| Previous.GetMonth() != Current.GetMonth()
		|| Previous.GetDay() != Current.GetDay())
	{
		OnDayChanged.Broadcast(BlueprintDateTime);
	}

	if (Previous.GetYear() != Current.GetYear()
		|| Previous.GetMonth() != Current.GetMonth()
		|| Previous.GetDay() != Current.GetDay()
		|| Previous.GetHour() != Current.GetHour())
	{
		OnHourChanged.Broadcast(BlueprintDateTime);
	}

	if (Previous.GetYear() != Current.GetYear()
		|| Previous.GetMonth() != Current.GetMonth()
		|| Previous.GetDay() != Current.GetDay()
		|| Previous.GetHour() != Current.GetHour()
		|| Previous.GetMinute() != Current.GetMinute())
	{
		OnMinuteChanged.Broadcast(BlueprintDateTime);
	}
}
