// Copyright James Joslin. All Rights Reserved.

#include "Actors/WeatherEnvironmentController.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "DrawDebugHelpers.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "WeatherStateSubsystem.h"

AWeatherEnvironmentController::AWeatherEnvironmentController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MoonMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MoonMesh"));
	MoonMeshComponent->SetupAttachment(SceneRoot);
	MoonMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoonMeshComponent->SetGenerateOverlapEvents(false);
	MoonMeshComponent->SetCastShadow(false);
	MoonMeshComponent->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMoonMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DefaultMoonMesh.Succeeded())
	{
		MoonMeshComponent->SetStaticMesh(DefaultMoonMesh.Object);
	}

	SkyDomeMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyDomeMesh"));
	SkyDomeMeshComponent->SetupAttachment(SceneRoot);
	SkyDomeMeshComponent->SetAbsolute(true, true, true);
	SkyDomeMeshComponent->SetMobility(EComponentMobility::Movable);
	SkyDomeMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyDomeMeshComponent->SetGenerateOverlapEvents(false);
	SkyDomeMeshComponent->SetCastShadow(false);
	SkyDomeMeshComponent->SetReceivesDecals(false);
	SkyDomeMeshComponent->SetCanEverAffectNavigation(false);
	SkyDomeMeshComponent->bAffectDistanceFieldLighting = false;
	SkyDomeMeshComponent->SetHiddenInGame(true);
	if (DefaultMoonMesh.Succeeded())
	{
		SkyDomeMeshComponent->SetStaticMesh(DefaultMoonMesh.Object);
	}

	SkyPostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("SkyPostProcess"));
	SkyPostProcessComponent->SetupAttachment(SceneRoot);
	SkyPostProcessComponent->bUnbound = true;
	SkyPostProcessComponent->bEnabled = false;
	SkyPostProcessComponent->BlendWeight = 1.0f;
	SkyPostProcessComponent->Priority = 100.0f;
}

void AWeatherEnvironmentController::BeginPlay()
{
	Super::BeginPlay();

	UWeatherStateSubsystem* StateSubsystem = GetWeatherStateSubsystem();
	if (!StateSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("WeatherEnvironmentController '%s' has no GameInstance weather subsystem."), *GetName());
		SetActorTickEnabled(false);
		return;
	}

	if (!StateSubsystem->RegisterController(this))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("WeatherEnvironmentController '%s' disabled because '%s' is already registered for this GameInstance."),
			*GetName(),
			*GetNameSafe(StateSubsystem->GetActiveController()));
		SetActorTickEnabled(false);
		return;
	}

	bControllerRegistered = true;
	WeatherStateSubsystem = StateSubsystem;
	StateSubsystem->InitializeClock(GetClockSettings(), false);
	if (bRebuildGridOnBeginPlay)
	{
		RebuildGrid();
	}

	ResolveWorldReferences();
	ConfigureMoonMesh();
	ConfigureSkyDomeMesh();
	InitializeSkyboxMID();
	UpdateEnvironment(0.0f, true);
}

void AWeatherEnvironmentController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bControllerRegistered && WeatherStateSubsystem.IsValid())
	{
		WeatherStateSubsystem->UnregisterController(this);
	}

	bControllerRegistered = false;
	WeatherStateSubsystem.Reset();
	if (SkyDomeMeshComponent)
	{
		SkyDomeMeshComponent->SetHiddenInGame(true);
	}
	SkyboxMID = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AWeatherEnvironmentController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bControllerRegistered)
	{
		UpdateEnvironment(DeltaSeconds, false);
	}

#if ENABLE_DRAW_DEBUG
	DrawWeatherGridDebug();
#endif
}

void AWeatherEnvironmentController::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureMoonMesh();
	ConfigureSkyDomeMesh();

	const FWeatherSkyboxSettings& SkySettings = GetSkyboxSettings();
	SkyPostProcessComponent->Priority = SkySettings.PostProcessPriority;
	SkyPostProcessComponent->BlendWeight = FMath::Clamp(SkySettings.BlendWeight, 0.0f, 1.0f);

	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		RebuildEditorPreviewGrid();
	}
}

#if WITH_EDITOR
bool AWeatherEnvironmentController::ShouldTickIfViewportsOnly() const
{
	return GridDebugSettings.bEnabled;
}
#endif

void AWeatherEnvironmentController::RefreshEnvironment()
{
	ResolveWorldReferences();
	ConfigureMoonMesh();
	ConfigureSkyDomeMesh();
	InitializeSkyboxMID();
	UpdateEnvironment(0.0f, true);
}

void AWeatherEnvironmentController::SetEnvironmentProfile(UWeatherEnvironmentProfile* NewProfile)
{
	EnvironmentProfile = NewProfile;
	RefreshEnvironment();
	RebuildGrid();
}

UWeatherStateSubsystem* AWeatherEnvironmentController::GetWeatherStateSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UWeatherStateSubsystem>() : nullptr;
}

const FWeatherClockSettings& AWeatherEnvironmentController::GetClockSettings() const
{
	return EnvironmentProfile ? EnvironmentProfile->Clock : ClockSettings;
}

const FWeatherAstronomySettings& AWeatherEnvironmentController::GetAstronomySettings() const
{
	return EnvironmentProfile ? EnvironmentProfile->Astronomy : AstronomySettings;
}

const FWeatherMoonVisualSettings& AWeatherEnvironmentController::GetMoonVisualSettings() const
{
	return EnvironmentProfile ? EnvironmentProfile->MoonVisual : MoonVisualSettings;
}

const FWeatherSkyboxSettings& AWeatherEnvironmentController::GetSkyboxSettings() const
{
	return EnvironmentProfile ? EnvironmentProfile->Skybox : SkyboxSettings;
}

const FWeatherGridDefinition& AWeatherEnvironmentController::GetGridDefinition() const
{
	return EnvironmentProfile ? EnvironmentProfile->Grid : GridDefinition;
}

void AWeatherEnvironmentController::RebuildGrid()
{
	TArray<ALandscapeProxy*> Sources;
	Sources.Reserve(LandscapeSources.Num());
	for (ALandscapeProxy* Landscape : LandscapeSources)
	{
		if (IsValid(Landscape))
		{
			Sources.Add(Landscape);
		}
	}

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (UWeatherStateSubsystem* StateSubsystem = GetWeatherStateSubsystem())
		{
			StateSubsystem->RebuildGridFromLandscapeSources(GetGridDefinition(), Sources);
			bLastGridBuildSucceeded = StateSubsystem->WasLastGridBuildSuccessful();
			LastGridBuildMessage = StateSubsystem->GetLastGridBuildMessage();
			return;
		}
	}

	RebuildEditorPreviewGrid();
}

void AWeatherEnvironmentController::ClearGrid()
{
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (UWeatherStateSubsystem* StateSubsystem = GetWeatherStateSubsystem())
		{
			StateSubsystem->ClearGrid();
			bLastGridBuildSucceeded = false;
			LastGridBuildMessage = StateSubsystem->GetLastGridBuildMessage();
		}
	}
	else
	{
		EditorPreviewGrid.Clear();
		bLastGridBuildSucceeded = false;
		LastGridBuildMessage = TEXT("Grid cleared. Adjust the grid definition if needed, then press Rebuild Grid.");
	}
}

FWeatherGridInfo AWeatherEnvironmentController::GetGridInfo() const
{
	const FWeatherGrid* Grid = GetGridForDebug();
	return Grid ? Grid->GetInfo() : FWeatherGridInfo();
}

bool AWeatherEnvironmentController::RebuildEditorPreviewGrid()
{
	TArray<ALandscapeProxy*> Sources;
	Sources.Reserve(LandscapeSources.Num());
	for (ALandscapeProxy* Landscape : LandscapeSources)
	{
		if (IsValid(Landscape))
		{
			Sources.Add(Landscape);
		}
	}

	FBox SourceBounds(ForceInit);
	FString Message;
	if (!FWeatherGrid::ResolveSourceBounds(
		GetWorld(),
		GetGridDefinition(),
		Sources,
		SourceBounds,
		Message))
	{
		bLastGridBuildSucceeded = false;
		LastGridBuildMessage = Message;
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("WeatherEnvironmentController '%s': %s"),
			*GetName(),
			*Message);
		return false;
	}

	if (!Message.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("WeatherEnvironmentController '%s': %s"),
			*GetName(),
			*Message);
	}

	const bool bBuilt = EditorPreviewGrid.Rebuild(SourceBounds, GetGridDefinition(), &Message);
	bLastGridBuildSucceeded = bBuilt;
	LastGridBuildMessage = Message;
	if (!bBuilt)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("WeatherEnvironmentController '%s': %s"),
			*GetName(),
			*Message);
	}
	return bBuilt;
}

const FWeatherGrid* AWeatherEnvironmentController::GetGridForDebug() const
{
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (const UWeatherStateSubsystem* StateSubsystem = GetWeatherStateSubsystem())
		{
			return &StateSubsystem->GetWeatherGrid();
		}
	}
	return &EditorPreviewGrid;
}

#if ENABLE_DRAW_DEBUG
void AWeatherEnvironmentController::DrawWeatherGridDebug() const
{
	if (!GridDebugSettings.bEnabled || !GetWorld())
	{
		return;
	}

#if WITH_EDITOR
	if (GridDebugSettings.bDrawOnlyWhenSelected && !IsSelectedInEditor())
	{
		return;
	}
#endif

	const FWeatherGrid* Grid = GetGridForDebug();
	if (!bLastGridBuildSucceeded && !LastGridBuildMessage.IsEmpty())
	{
		DrawDebugString(
			GetWorld(),
			GetActorLocation() + FVector(0.0, 0.0, 2000.0),
			LastGridBuildMessage,
			nullptr,
			FColor::Red,
			0.0f,
			true,
			1.25f);
	}

	if (!Grid || !Grid->GetInfo().bIsValid)
	{
		return;
	}

	FVector ViewLocation = GetActorLocation();
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		ViewLocation = Camera->GetCameraLocation();
	}

	const FWeatherGridInfo& Info = Grid->GetInfo();
	const double MaximumDrawDistanceSquared = FMath::Square(GridDebugSettings.DrawDistance);
	const bool bLimitDistance = GridDebugSettings.DrawDistance > 0.0;
	for (int32 Y = 0; Y < Info.Dimensions.Y; ++Y)
	{
		for (int32 X = 0; X < Info.Dimensions.X; ++X)
		{
			const FWeatherCellCoord Coord(X, Y);
			const FWeatherCellState* State = Grid->FindCell(Coord);
			if (!State)
			{
				continue;
			}

			const FVector2D Offset(
				State->WorldCenter.X - ViewLocation.X,
				State->WorldCenter.Y - ViewLocation.Y);
			if (bLimitDistance && Offset.SizeSquared() > MaximumDrawDistanceSquared)
			{
				continue;
			}

			FColor CellColor = FColor(80, 180, 255);
			switch (State->WeatherType)
			{
			case EWeatherType::Rain:
			case EWeatherType::HeavyRain:
				CellColor = FColor(60, 100, 255);
				break;
			case EWeatherType::Storm:
				CellColor = FColor(180, 80, 255);
				break;
			case EWeatherType::Overcast:
				CellColor = FColor(160, 160, 170);
				break;
			case EWeatherType::PartlyCloudy:
				CellColor = FColor(130, 200, 220);
				break;
			default:
				break;
			}

			if (GridDebugSettings.bDrawGridLines)
			{
				const FBox CellBounds = Grid->GetCellBounds(Coord);
				const double Z = State->WorldCenter.Z;
				const FVector A(CellBounds.Min.X, CellBounds.Min.Y, Z);
				const FVector B(CellBounds.Max.X, CellBounds.Min.Y, Z);
				const FVector C(CellBounds.Max.X, CellBounds.Max.Y, Z);
				const FVector D(CellBounds.Min.X, CellBounds.Max.Y, Z);
				DrawDebugLine(GetWorld(), A, B, CellColor, false, 0.0f, 0, GridDebugSettings.LineThickness);
				DrawDebugLine(GetWorld(), B, C, CellColor, false, 0.0f, 0, GridDebugSettings.LineThickness);
				DrawDebugLine(GetWorld(), C, D, CellColor, false, 0.0f, 0, GridDebugSettings.LineThickness);
				DrawDebugLine(GetWorld(), D, A, CellColor, false, 0.0f, 0, GridDebugSettings.LineThickness);
			}

			if (GridDebugSettings.bDrawInfluenceSpheres)
			{
				DrawDebugSphere(
					GetWorld(),
					State->WorldCenter,
					State->InfluenceRadius,
					FMath::Clamp(GridDebugSettings.SphereSegments, 4, 64),
					CellColor,
					false,
					0.0f,
					0,
					GridDebugSettings.LineThickness);
			}

			if (GridDebugSettings.bDrawWindArrows && !State->WindVector.IsNearlyZero())
			{
				DrawDebugDirectionalArrow(
					GetWorld(),
					State->WorldCenter,
					State->WorldCenter + State->WindVector * GridDebugSettings.WindArrowScale,
					GridDebugSettings.ArrowHeadSize,
					FColor::Green,
					false,
					0.0f,
					0,
					GridDebugSettings.LineThickness);
			}

			FString Label;
			if (GridDebugSettings.bDrawCoordinates)
			{
				Label += FString::Printf(TEXT("(%d, %d)"), X, Y);
			}
			if (GridDebugSettings.bDrawWeatherType)
			{
				Label += FString::Printf(TEXT("\n%s"), *UEnum::GetValueAsString(State->WeatherType));
			}
			if (GridDebugSettings.bDrawWindSpeed)
			{
				Label += FString::Printf(TEXT("\nWind %.1f cm/s"), State->WindVector.Size());
			}
			if (GridDebugSettings.bDrawCloudCoverage)
			{
				Label += FString::Printf(TEXT("\nCloud %.2f"), State->CloudCoverage);
			}
			if (GridDebugSettings.bDrawHumidity)
			{
				Label += FString::Printf(TEXT("\nHumidity %.2f"), State->Humidity);
			}
			if (GridDebugSettings.bDrawTemperature)
			{
				Label += FString::Printf(TEXT("\nTemp %.1f C"), State->TemperatureCelsius);
			}
			if (GridDebugSettings.bDrawPressure)
			{
				Label += FString::Printf(TEXT("\nPressure %.1f hPa"), State->PressureHpa);
			}
			if (GridDebugSettings.bDrawRainIntensity)
			{
				Label += FString::Printf(TEXT("\nRain %.2f"), State->RainIntensity);
			}
			if (GridDebugSettings.bDrawStorminess)
			{
				Label += FString::Printf(TEXT("\nStorm %.2f"), State->Storminess);
			}

			if (!Label.IsEmpty())
			{
				DrawDebugString(
					GetWorld(),
					State->WorldCenter + FVector(0.0, 0.0, 1000.0),
					Label,
					nullptr,
					CellColor,
					0.0f,
					true,
					1.0f);
			}
		}
	}
}
#endif

void AWeatherEnvironmentController::ResolveWorldReferences()
{
	if (!bAutoDiscoverLights || !GetWorld())
	{
		return;
	}

	TArray<ADirectionalLight*> UntaggedDirectionalLights;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		ADirectionalLight* Light = *It;
		if (!IsValid(Light))
		{
			continue;
		}

		if (!SunLight && Light->ActorHasTag(TEXT("WeatherSun")))
		{
			SunLight = Light;
			continue;
		}

		if (!MoonLight && Light->ActorHasTag(TEXT("WeatherMoon")))
		{
			MoonLight = Light;
			continue;
		}

		const UDirectionalLightComponent* Component =
			Cast<UDirectionalLightComponent>(Light->GetLightComponent());
		if (Component && Component->bAtmosphereSunLight)
		{
			if (!SunLight && Component->AtmosphereSunLightIndex == 0)
			{
				SunLight = Light;
				continue;
			}
			if (!MoonLight && Component->AtmosphereSunLightIndex == 1)
			{
				MoonLight = Light;
				continue;
			}
		}

		UntaggedDirectionalLights.Add(Light);
	}

	for (ADirectionalLight* Light : UntaggedDirectionalLights)
	{
		if (!SunLight)
		{
			SunLight = Light;
		}
		else if (!MoonLight && Light != SunLight)
		{
			MoonLight = Light;
			break;
		}
	}

	if (!SkyLight)
	{
		for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
		{
			SkyLight = *It;
			break;
		}
	}

	if (!SunLight)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeatherEnvironmentController '%s' could not resolve a sun DirectionalLight."), *GetName());
	}
	if (!MoonLight)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeatherEnvironmentController '%s' could not resolve a moon DirectionalLight."), *GetName());
	}
}

void AWeatherEnvironmentController::ConfigureMoonMesh()
{
	if (!MoonMeshComponent)
	{
		return;
	}

	const FWeatherMoonVisualSettings& Settings = GetMoonVisualSettings();
	if (Settings.MoonMesh)
	{
		MoonMeshComponent->SetStaticMesh(Settings.MoonMesh);
	}

	const bool bVisible = Settings.bEnabled && MoonMeshComponent->GetStaticMesh() != nullptr;
	MoonMeshComponent->SetVisibility(bVisible, true);
	MoonMeshComponent->SetHiddenInGame(!bVisible);
	MoonMeshComponent->SetWorldScale3D(FVector(FMath::Max(Settings.UniformScale, 0.0001)));
}

void AWeatherEnvironmentController::ConfigureSkyDomeMesh()
{
	if (!SkyDomeMeshComponent)
	{
		return;
	}

	// Engine/BasicShapes/Sphere has a 50 cm local radius.
	constexpr double DefaultSphereRadius = 50.0;
	const double WorldRadius = FMath::Max(GetSkyboxSettings().SkyDomeRadius, 10000.0);
	SkyDomeMeshComponent->SetWorldScale3D(FVector(WorldRadius / DefaultSphereRadius));

	// Avoid displaying the Engine default surface before the runtime MID exists.
	if (!SkyboxMID)
	{
		SkyDomeMeshComponent->SetHiddenInGame(true);
	}
}

void AWeatherEnvironmentController::InitializeSkyboxMID()
{
	if (!SkyPostProcessComponent || !SkyDomeMeshComponent)
	{
		return;
	}

	const FWeatherSkyboxSettings& Settings = GetSkyboxSettings();
	SkyPostProcessComponent->Settings.WeightedBlendables.Array.Reset();
	SkyPostProcessComponent->Priority = Settings.PostProcessPriority;
	SkyPostProcessComponent->BlendWeight = FMath::Clamp(Settings.BlendWeight, 0.0f, 1.0f);
	SkyPostProcessComponent->bUnbound = true;
	SkyPostProcessComponent->bEnabled = false;
	SkyDomeMeshComponent->SetHiddenInGame(true);
	SkyboxMID = nullptr;

	if (!Settings.bEnabled)
	{
		return;
	}

	if (Settings.bUseLegacyPostProcessSky)
	{
		UMaterialInterface* PostProcessMaterial = Settings.PostProcessMaterial;
		if (!PostProcessMaterial)
		{
			PostProcessMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/WeatherEnvironmentSystem/Materials/M_WeatherSkyboxPostProcess.M_WeatherSkyboxPostProcess"));
		}

		if (PostProcessMaterial)
		{
			SkyboxMID = UMaterialInstanceDynamic::Create(
				PostProcessMaterial,
				this,
				TEXT("MID_WeatherSkyboxLegacyPostProcess"));
		}

		if (!SkyboxMID)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("WeatherEnvironmentController '%s' could not create its legacy post-process sky MID."),
				*GetName());
			return;
		}

		SkyPostProcessComponent->Settings.AddBlendable(SkyboxMID, 1.0f);
		SkyPostProcessComponent->bEnabled = true;
	}
	else
	{
		UMaterialInterface* SkyDomeMaterial = Settings.SkyDomeMaterial;
		if (!SkyDomeMaterial)
		{
			// Load after module startup so the plugin shader mapping already exists.
			SkyDomeMaterial = LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/WeatherEnvironmentSystem/Materials/M_WeatherSkyDome.M_WeatherSkyDome"));
		}

		if (SkyDomeMaterial)
		{
			SkyboxMID = UMaterialInstanceDynamic::Create(
				SkyDomeMaterial,
				this,
				TEXT("MID_WeatherSkyDome"));
		}

		if (!SkyboxMID)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("WeatherEnvironmentController '%s' could not create its sky-dome MID. Generate M_WeatherSkyDome or assign a compatible material."),
				*GetName());
			return;
		}

		SkyDomeMeshComponent->SetMaterial(0, SkyboxMID);
		SkyDomeMeshComponent->SetHiddenInGame(false);
		UpdateSkyDomeVisual();
	}

	UpdateSkyboxParameters(GetWeatherStateSubsystem() ? GetWeatherStateSubsystem()->GetNormalizedDayFraction() : 0.0);
}

void AWeatherEnvironmentController::UpdateEnvironment(
	const float DeltaSeconds,
	const bool bForce)
{
	UWeatherStateSubsystem* StateSubsystem = WeatherStateSubsystem.Get();
	if (!StateSubsystem)
	{
		return;
	}

	const FWeatherDateTime DateTime = StateSubsystem->GetCurrentDateTime();
	FDateTime NativeDateTime;
	if (!DateTime.ToDateTime(NativeDateTime))
	{
		return;
	}

	const FWeatherCelestialState CelestialState =
		FWeatherAstronomy::Calculate(NativeDateTime, GetAstronomySettings());

	CurrentSunDirection = CelestialState.SunDirection;
	CurrentMoonDirection = CelestialState.MoonDirection;
	CurrentSunElevationDegrees = static_cast<float>(CelestialState.SunElevationDegrees);
	CurrentMoonElevationDegrees = static_cast<float>(CelestialState.MoonElevationDegrees);
	CurrentMoonPhase = CalculateMoonPhase(NativeDateTime);

	const double DayFraction = DateTime.GetNormalizedDayFraction();
	UpdateDirectionalLights(CelestialState, DayFraction);
	UpdateMoonVisual(CelestialState);
	UpdateSkyDomeVisual();
	UpdateSkyboxParameters(DayFraction);
	UpdateCelestialTransitionEvents(DateTime, CelestialState.SunElevationDegrees);

	const FWeatherAstronomySettings& Settings = GetAstronomySettings();
	if (Settings.bRecaptureSkyLight && SkyLight && SkyLight->GetLightComponent())
	{
		SkyLightRecaptureAccumulator += DeltaSeconds;
		if (bForce || SkyLightRecaptureAccumulator >= Settings.SkyLightRecaptureIntervalSeconds)
		{
			SkyLight->GetLightComponent()->RecaptureSky();
			SkyLightRecaptureAccumulator = 0.0f;
		}
	}
}

void AWeatherEnvironmentController::UpdateDirectionalLights(
	const FWeatherCelestialState& CelestialState,
	const double DayFraction)
{
	const FWeatherAstronomySettings& Settings = GetAstronomySettings();

	const float DefaultSunMultiplier = SmoothRange(
		static_cast<float>(CelestialState.SunElevationDegrees),
		-6.0f,
		3.0f);
	const float DefaultMoonMultiplier =
		SmoothRange(static_cast<float>(CelestialState.MoonElevationDegrees), -2.0f, 4.0f)
		* (1.0f - DefaultSunMultiplier);

	if (Settings.bDriveSunLight && SunLight)
	{
		SunLight->SetActorRotation((-CelestialState.SunDirection).Rotation());
		if (UDirectionalLightComponent* Component =
			Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
		{
			if (Settings.bConfigureAtmosphereLightIndices)
			{
				Component->SetAtmosphereSunLight(true);
				Component->SetAtmosphereSunLightIndex(0);
			}

			const float Multiplier = Settings.SunIntensityByElevation
				? FMath::Max(0.0f, Settings.SunIntensityByElevation->GetFloatValue(
					static_cast<float>(CelestialState.SunElevationDegrees)))
				: DefaultSunMultiplier;
			const FLinearColor Color = Settings.SunColorByDayFraction
				? Settings.SunColorByDayFraction->GetLinearColorValue(static_cast<float>(DayFraction))
				: Settings.DefaultSunColor;
			Component->SetIntensity(Settings.SunMaximumIntensity * Multiplier);
			Component->SetLightColor(Color, false);
		}
	}

	if (Settings.bDriveMoonLight && MoonLight)
	{
		MoonLight->SetActorRotation((-CelestialState.MoonDirection).Rotation());
		if (UDirectionalLightComponent* Component =
			Cast<UDirectionalLightComponent>(MoonLight->GetLightComponent()))
		{
			if (Settings.bConfigureAtmosphereLightIndices)
			{
				Component->SetAtmosphereSunLight(true);
				Component->SetAtmosphereSunLightIndex(1);
			}

			const float Multiplier = Settings.MoonIntensityByElevation
				? FMath::Max(0.0f, Settings.MoonIntensityByElevation->GetFloatValue(
					static_cast<float>(CelestialState.MoonElevationDegrees)))
				: DefaultMoonMultiplier;
			const FLinearColor Color = Settings.MoonColorByDayFraction
				? Settings.MoonColorByDayFraction->GetLinearColorValue(static_cast<float>(DayFraction))
				: Settings.DefaultMoonColor;
			Component->SetIntensity(Settings.MoonMaximumIntensity * Multiplier);
			Component->SetLightColor(Color, false);
		}
	}
}

void AWeatherEnvironmentController::UpdateMoonVisual(const FWeatherCelestialState& CelestialState)
{
	const FWeatherMoonVisualSettings& Settings = GetMoonVisualSettings();
	if (!Settings.bEnabled || !MoonMeshComponent || !MoonMeshComponent->GetStaticMesh())
	{
		return;
	}

	FVector OrbitCenter = GetActorLocation();
	if (MoonOrbitCenter)
	{
		OrbitCenter = MoonOrbitCenter->GetActorLocation();
	}
	else if (Settings.bCenterOrbitOnPlayerCamera)
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			OrbitCenter = Camera->GetCameraLocation();
		}
	}

	const FVector MoonLocation =
		OrbitCenter + CelestialState.MoonDirection * FMath::Max(Settings.OrbitRadius, 1000.0);
	MoonMeshComponent->SetWorldLocation(MoonLocation);

	if (Settings.bFaceOrbitCenter)
	{
		const FRotator FacingRotation = (OrbitCenter - MoonLocation).Rotation();
		MoonMeshComponent->SetWorldRotation(FacingRotation + Settings.FacingRotationOffset);
	}
}

void AWeatherEnvironmentController::UpdateSkyDomeVisual()
{
	const FWeatherSkyboxSettings& Settings = GetSkyboxSettings();
	if (!Settings.bEnabled
		|| Settings.bUseLegacyPostProcessSky
		|| !SkyDomeMeshComponent
		|| !SkyboxMID)
	{
		return;
	}

	FVector SkyCenter = GetActorLocation();
	if (Settings.bCenterSkyDomeOnPlayerCamera)
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			SkyCenter = Camera->GetCameraLocation();
		}
	}

	SkyDomeMeshComponent->SetWorldLocation(SkyCenter);
}

void AWeatherEnvironmentController::UpdateSkyboxParameters(const double DayFraction)
{
	if (!SkyboxMID)
	{
		return;
	}

	const FWeatherSkyboxSettings& Settings = GetSkyboxSettings();
	const float NightVisibility = 1.0f - SmoothRange(
		CurrentSunElevationDegrees,
		FMath::Min(
			Settings.NightSkyFullyVisibleSunElevationDegrees,
			Settings.NightSkyHiddenSunElevationDegrees),
		FMath::Max(
			Settings.NightSkyFullyVisibleSunElevationDegrees,
			Settings.NightSkyHiddenSunElevationDegrees))
		* FMath::Clamp(Settings.DirectionalDaylightFadeStrength, 0.0f, 1.0f);
	FLinearColor HueShifts(0.0f, 0.0f, 0.0f, 0.0f);
	FLinearColor Saturations(1.0f, 1.0f, 1.0f, 1.0f);
	FLinearColor Luminosities(0.0f, 0.0f, 0.0f, 0.0f);
	FLinearColor MipLevels(0.0f, 0.0f, 0.0f, 0.0f);

	float* HueValues[4] = { &HueShifts.R, &HueShifts.G, &HueShifts.B, &HueShifts.A };
	float* SaturationValues[4] = { &Saturations.R, &Saturations.G, &Saturations.B, &Saturations.A };
	float* LuminosityValues[4] = { &Luminosities.R, &Luminosities.G, &Luminosities.B, &Luminosities.A };
	float* MipValues[4] = { &MipLevels.R, &MipLevels.G, &MipLevels.B, &MipLevels.A };

	for (int32 LayerIndex = 0; LayerIndex < 4; ++LayerIndex)
	{
		if (!Settings.Layers.IsValidIndex(LayerIndex))
		{
			continue;
		}

		const FWeatherSkyboxLayerSettings& Layer = Settings.Layers[LayerIndex];
		if (Layer.Cubemap)
		{
			SkyboxMID->SetTextureParameterValue(
				FName(*FString::Printf(TEXT("WeatherSkybox%d"), LayerIndex + 1)),
				Layer.Cubemap);
		}

		*HueValues[LayerIndex] = Layer.HueShift;
		*SaturationValues[LayerIndex] = FMath::Max(0.0f, Layer.Saturation);
		const float CurveMultiplier = Layer.LuminosityByDayFraction
			? Layer.LuminosityByDayFraction->GetFloatValue(static_cast<float>(DayFraction))
			: 1.0f;
		*LuminosityValues[LayerIndex] = FMath::Max(0.0f, Layer.Luminosity * CurveMultiplier);
		*MipValues[LayerIndex] = FMath::Max(0.0f, Layer.MipLevel);
	}

	SkyboxMID->SetVectorParameterValue(TEXT("WeatherHueShifts"), HueShifts);
	SkyboxMID->SetVectorParameterValue(TEXT("WeatherSaturations"), Saturations);
	SkyboxMID->SetVectorParameterValue(TEXT("WeatherLuminosities"), Luminosities);
	SkyboxMID->SetVectorParameterValue(TEXT("WeatherMipLevels"), MipLevels);
	SkyboxMID->SetVectorParameterValue(
		TEXT("WeatherGradientParams"),
		FLinearColor(
			Settings.GradientScale,
			FMath::Max(Settings.GradientPower, 0.001f),
			Settings.GradientIntensity,
			Settings.HorizonOffset));
	SkyboxMID->SetVectorParameterValue(
		TEXT("WeatherGradientColor"),
		FLinearColor(
			Settings.GradientColor.R,
			Settings.GradientColor.G,
			Settings.GradientColor.B,
			Settings.bUseLegacyPostProcessSky ? Settings.GradientColor.A : NightVisibility));
	SkyboxMID->SetVectorParameterValue(TEXT("WeatherTint"), Settings.Tint);
	SkyboxMID->SetVectorParameterValue(
		TEXT("WeatherDepthParams"),
		FLinearColor(
			FMath::Max(Settings.FarPlaneDeviceZEpsilon, 0.0f),
			FMath::Max(Settings.DepthMaskFeather, 0.0f),
			FMath::Max(Settings.LinearDepthFallbackDistance, 0.0f),
			0.0f));
	SkyboxMID->SetVectorParameterValue(
		TEXT("WeatherAtmosphereParams"),
		FLinearColor(
			FMath::Clamp(
				Settings.AtmosphereFadeStrength
				* (Settings.AtmosphereFadeBySunElevation
					? Settings.AtmosphereFadeBySunElevation->GetFloatValue(CurrentSunElevationDegrees)
					: SmoothRange(CurrentSunElevationDegrees, -6.0f, 3.0f)),
				0.0f,
				1.0f),
			FMath::Max(Settings.AtmosphereFadePower, 0.001f),
			FMath::Max(Settings.AtmosphereFadeMinimumLuminance, 0.0f),
			FMath::Max(Settings.AtmosphereFadeMaximumLuminance, Settings.AtmosphereFadeMinimumLuminance + 0.0001f)));
	SkyboxMID->SetVectorParameterValue(
		TEXT("WeatherDayNightParams"),
		FLinearColor(
			Settings.NightSkyFullyVisibleSunElevationDegrees,
			Settings.NightSkyHiddenSunElevationDegrees,
			FMath::Clamp(Settings.DirectionalDaylightFadeStrength, 0.0f, 1.0f),
			FMath::Max(Settings.SkyAtmosphereLuminanceMultiplier, 0.0f)
				* FMath::Clamp(Settings.AtmosphereFadeStrength, 0.0f, 1.0f)
				* (Settings.AtmosphereFadeBySunElevation
					? FMath::Clamp(Settings.AtmosphereFadeBySunElevation->GetFloatValue(CurrentSunElevationDegrees), 0.0f, 1.0f)
					: 1.0f)));
	SkyboxMID->SetVectorParameterValue(
		TEXT("WeatherSunDirection"),
		FLinearColor(
			static_cast<float>(CurrentSunDirection.X),
			static_cast<float>(CurrentSunDirection.Y),
			static_cast<float>(CurrentSunDirection.Z),
			0.0f));
	SkyboxMID->SetScalarParameterValue(TEXT("WeatherTintBlend"), FMath::Clamp(Settings.TintBlend, 0.0f, 1.0f));
	SkyboxMID->SetScalarParameterValue(TEXT("WeatherMaxBrightness"), FMath::Max(Settings.MaximumBrightness, 0.0f));
	SkyboxMID->SetScalarParameterValue(TEXT("WeatherSkyRotationDegrees"), Settings.RotationDegrees);
	SkyboxMID->SetScalarParameterValue(TEXT("WeatherMoonPhase"), CurrentMoonPhase);
}

void AWeatherEnvironmentController::UpdateCelestialTransitionEvents(
	const FWeatherDateTime& DateTime,
	const double SunElevation)
{
	const bool bSunIsAbove = SunElevation >= GetAstronomySettings().SunriseElevationDegrees;
	if (!bHasPreviousSunState)
	{
		bWasSunAboveTransition = bSunIsAbove;
		bHasPreviousSunState = true;
		return;
	}

	if (bSunIsAbove != bWasSunAboveTransition)
	{
		if (bSunIsAbove)
		{
			OnSunrise.Broadcast(DateTime);
		}
		else
		{
			OnSunset.Broadcast(DateTime);
		}
		bWasSunAboveTransition = bSunIsAbove;
	}
}

float AWeatherEnvironmentController::SmoothRange(
	const float Value,
	const float Minimum,
	const float Maximum)
{
	const float Alpha = FMath::Clamp((Value - Minimum) / FMath::Max(Maximum - Minimum, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}

float AWeatherEnvironmentController::CalculateMoonPhase(const FDateTime& DateTime)
{
	// Known new moon near 2000-01-06 18:14 UTC. The clock's local value is sufficient for art direction.
	const FDateTime ReferenceNewMoon(2000, 1, 6, 18, 14, 0);
	const double SynodicMonthDays = 29.53058867;
	double Phase = FMath::Fmod((DateTime - ReferenceNewMoon).GetTotalDays() / SynodicMonthDays, 1.0);
	if (Phase < 0.0)
	{
		Phase += 1.0;
	}
	return static_cast<float>(Phase);
}
