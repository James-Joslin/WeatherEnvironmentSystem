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
	WeatherSimulationSettings = FWeatherSimulationSettings();
	WeatherSimulationAccumulator = 0.0f;
	WeatherSimulationTimeSeconds = 0.0;
	bWeatherSimulationConfigured = false;
	bInitialSeedsApplied = false;
	bInitialGeneratedSeedsApplied = false;
	bInitialLifecyclePopulationApplied = false;
	WeatherFrontLifecycleAccumulator = 0.0f;
	ConfiguredEnvironmentSeed = WeatherSimulationSettings.EnvironmentSeed;
	NextSeedSerial = 0;
	WeatherRandomStream.Initialize(ConfiguredEnvironmentSeed);
	ActiveSeeds.Reset();
	PreviousWeatherSnapshot.Reset();
	WeatherTypeDurations.Reset();
	LastBroadcastLocalWeather = FWeatherSample();
	LocalWeatherEventElapsedSeconds = 0.0f;
	WindFieldTexture = nullptr;
	WindMaterialParameterCollection = nullptr;
	LastWindFieldPixels.Reset();
	ActiveSeeds.Reset();
	PreviousWeatherSnapshot.Reset();
	WeatherTypeDurations.Reset();
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
	TickWeatherSimulation(DeltaTime);
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
		PreviousWeatherSnapshot = WeatherGrid.GetCells();
		WeatherTypeDurations.Init(0.0f, WeatherGrid.GetInfo().CellCount);
		if (bWeatherSimulationConfigured)
		{
			ApplyInitialSeeds();
			MaintainWeatherFrontPopulation(!bInitialLifecyclePopulationApplied);
			StepWeatherSimulation(0.0f);
			PreviousWeatherSnapshot = WeatherGrid.GetCells();
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
	PreviousWeatherSnapshot.Reset();
	WeatherTypeDurations.Reset();
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
	return BuildInterpolatedWeatherSample(WorldLocation);
}

bool UWeatherStateSubsystem::GetWeatherTypeAtLocation(
	const FVector& WorldLocation,
	EWeatherType& OutWeatherType) const
{
	const FWeatherSample Sample = BuildInterpolatedWeatherSample(WorldLocation);
	if (!Sample.bIsValid)
	{
		OutWeatherType = EWeatherType::Clear;
		return false;
	}

	OutWeatherType = Sample.State.WeatherType;
	return true;
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

void UWeatherStateSubsystem::ConfigureWeatherSimulation(
	const FWeatherSimulationSettings& Settings)
{
	const bool bFirstConfiguration = !bWeatherSimulationConfigured;
	const bool bSeedChanged = !bFirstConfiguration
		&& ConfiguredEnvironmentSeed != Settings.EnvironmentSeed;
	WeatherSimulationSettings = Settings;
	WeatherSimulationSettings.FixedUpdateIntervalSeconds = FMath::Max(
		WeatherSimulationSettings.FixedUpdateIntervalSeconds,
		0.001f);
	WeatherSimulationSettings.MaximumSubstepsPerFrame = FMath::Clamp(
		WeatherSimulationSettings.MaximumSubstepsPerFrame,
		1,
		32);
	WeatherSimulationSettings.MaximumSeedCount = FMath::Max(
		WeatherSimulationSettings.MaximumSeedCount,
		1);
	WeatherSimulationSettings.BaselineWeight = FMath::Max(
		WeatherSimulationSettings.BaselineWeight,
		0.0001f);
	WeatherSimulationSettings.RainEnterThreshold = FMath::Clamp(
		WeatherSimulationSettings.RainEnterThreshold,
		0.0f,
		1.0f);
	WeatherSimulationSettings.RainExitThreshold = FMath::Clamp(
		WeatherSimulationSettings.RainExitThreshold,
		0.0f,
		WeatherSimulationSettings.RainEnterThreshold);
	WeatherSimulationSettings.StormEnterThreshold = FMath::Clamp(
		WeatherSimulationSettings.StormEnterThreshold,
		0.0f,
		1.0f);
	WeatherSimulationSettings.StormExitThreshold = FMath::Clamp(
		WeatherSimulationSettings.StormExitThreshold,
		0.0f,
		WeatherSimulationSettings.StormEnterThreshold);
	WeatherSimulationSettings.MinimumWeatherTypeDurationSeconds = FMath::Max(
		WeatherSimulationSettings.MinimumWeatherTypeDurationSeconds,
		0.0f);
	FWeatherFrontLifecycleSettings& Lifecycle = WeatherSimulationSettings.FrontLifecycle;
	Lifecycle.TargetCellsPerFront = FMath::Max(Lifecycle.TargetCellsPerFront, 1.0f);
	Lifecycle.MinimumFrontCount = FMath::Max(Lifecycle.MinimumFrontCount, 0);
	Lifecycle.MaximumFrontCount = FMath::Max(
		Lifecycle.MaximumFrontCount,
		Lifecycle.MinimumFrontCount);
	Lifecycle.ReplenishmentIntervalSeconds = FMath::Max(
		Lifecycle.ReplenishmentIntervalSeconds,
		0.01f);
	Lifecycle.MaximumSpawnsPerInterval = FMath::Max(
		Lifecycle.MaximumSpawnsPerInterval,
		1);
	Lifecycle.BoundaryInsetCellFraction = FMath::Clamp(
		Lifecycle.BoundaryInsetCellFraction,
		0.0f,
		0.49f);
	Lifecycle.MinimumSpacingCellWidths = FMath::Max(
		Lifecycle.MinimumSpacingCellWidths,
		0.0f);
	Lifecycle.PositionAttempts = FMath::Clamp(Lifecycle.PositionAttempts, 1, 64);
	for (FWeatherFrontArchetype& Archetype : Lifecycle.Archetypes)
	{
		Archetype.SpawnWeight = FMath::Max(Archetype.SpawnWeight, 0.0f);
		Archetype.SigmaCellRange.X = FMath::Max(Archetype.SigmaCellRange.X, 0.01);
		Archetype.SigmaCellRange.Y = FMath::Max(Archetype.SigmaCellRange.Y, 0.01);
		Archetype.StrengthRange.X = FMath::Max(Archetype.StrengthRange.X, 0.0);
		Archetype.StrengthRange.Y = FMath::Max(Archetype.StrengthRange.Y, 0.0);
		Archetype.LifetimeSecondsRange.X = FMath::Max(Archetype.LifetimeSecondsRange.X, 0.0);
		Archetype.LifetimeSecondsRange.Y = FMath::Max(Archetype.LifetimeSecondsRange.Y, 0.0);
		Archetype.MovementMultiplierRange.X = FMath::Max(Archetype.MovementMultiplierRange.X, 0.0);
		Archetype.MovementMultiplierRange.Y = FMath::Max(Archetype.MovementMultiplierRange.Y, 0.0);
	}
	WeatherSimulationAccumulator = 0.0f;
	WeatherFrontLifecycleAccumulator = 0.0f;
	bWeatherSimulationConfigured = true;

	if (bFirstConfiguration || bSeedChanged)
	{
		ConfiguredEnvironmentSeed = WeatherSimulationSettings.EnvironmentSeed;
		WeatherRandomStream.Initialize(ConfiguredEnvironmentSeed);
		NextSeedSerial = 0;
	}

	if (bFirstConfiguration)
	{
		ApplyInitialSeeds();
	}
	MaintainWeatherFrontPopulation(!bInitialLifecyclePopulationApplied);

	if (WeatherGrid.GetInfo().bIsValid)
	{
		StepWeatherSimulation(0.0f);
		PreviousWeatherSnapshot = WeatherGrid.GetCells();
	}
}

FGuid UWeatherStateSubsystem::AddSeed(const FWeatherSeed& Seed)
{
	if (!bWeatherSimulationConfigured
		|| ActiveSeeds.Num() >= WeatherSimulationSettings.MaximumSeedCount)
	{
		return FGuid();
	}

	FWeatherSeed MaterializedSeed = Seed;
	if (MaterializedSeed.Id.IsValid())
	{
		const bool bDuplicate = ActiveSeeds.ContainsByPredicate(
			[&MaterializedSeed](const FWeatherSeed& ExistingSeed)
			{
				return ExistingSeed.Id == MaterializedSeed.Id;
			});
		if (bDuplicate)
		{
			return FGuid();
		}
	}
	else
	{
		do
		{
			MaterializedSeed.Id = FWeatherSimulationMath::MakeDeterministicSeedId(
				ConfiguredEnvironmentSeed,
				NextSeedSerial++);
		}
		while (ActiveSeeds.ContainsByPredicate(
			[&MaterializedSeed](const FWeatherSeed& ExistingSeed)
			{
				return ExistingSeed.Id == MaterializedSeed.Id;
			}));
	}

	MaterializedSeed.Sigma = FMath::Max(MaterializedSeed.Sigma, 1.0);
	MaterializedSeed.Strength = FMath::Max(MaterializedSeed.Strength, 0.0f);
	MaterializedSeed.MovementMultiplier = FMath::Max(MaterializedSeed.MovementMultiplier, 0.0f);
	MaterializedSeed.AgeSeconds = FMath::Max(MaterializedSeed.AgeSeconds, 0.0f);
	ResolveSeedValues(MaterializedSeed);
	if (WeatherGrid.GetInfo().bIsValid
		&& !FWeatherSimulationMath::ApplyBoundaryPolicy(
			MaterializedSeed.Position,
			WeatherGrid.GetInfo(),
			WeatherSimulationSettings.BoundaryPolicy))
	{
		return FGuid();
	}

	ActiveSeeds.Add(MaterializedSeed);
	return MaterializedSeed.Id;
}

bool UWeatherStateSubsystem::RemoveSeed(const FGuid SeedId)
{
	const int32 SeedIndex = ActiveSeeds.IndexOfByPredicate(
		[&SeedId](const FWeatherSeed& Seed)
		{
			return Seed.Id == SeedId;
		});
	if (SeedIndex == INDEX_NONE)
	{
		return false;
	}

	ActiveSeeds.RemoveAt(SeedIndex);
	return true;
}

bool UWeatherStateSubsystem::MoveSeed(
	const FGuid SeedId,
	FVector2D NewPosition)
{
	const int32 SeedIndex = ActiveSeeds.IndexOfByPredicate(
		[&SeedId](const FWeatherSeed& Seed)
		{
			return Seed.Id == SeedId;
		});
	if (SeedIndex == INDEX_NONE)
	{
		return false;
	}

	if (WeatherGrid.GetInfo().bIsValid
		&& !FWeatherSimulationMath::ApplyBoundaryPolicy(
			NewPosition,
			WeatherGrid.GetInfo(),
			WeatherSimulationSettings.BoundaryPolicy))
	{
		ActiveSeeds.RemoveAt(SeedIndex);
		return false;
	}

	ActiveSeeds[SeedIndex].Position = NewPosition;
	return true;
}

void UWeatherStateSubsystem::ClearSeeds()
{
	ActiveSeeds.Reset();
	if (bWeatherSimulationConfigured && WeatherGrid.GetInfo().bIsValid)
	{
		StepWeatherSimulation(0.0f);
		PreviousWeatherSnapshot = WeatherGrid.GetCells();
	}
}

int32 UWeatherStateSubsystem::GenerateSeedSet(
	const int32 SeedCount,
	const bool bReplaceExisting)
{
	if (!bWeatherSimulationConfigured || !WeatherGrid.GetInfo().bIsValid)
	{
		return 0;
	}

	if (bReplaceExisting)
	{
		ActiveSeeds.Reset();
	}

	const int32 AvailableSlots = FMath::Max(
		WeatherSimulationSettings.MaximumSeedCount - ActiveSeeds.Num(),
		0);
	const int32 CountToGenerate = FMath::Clamp(SeedCount, 0, AvailableSlots);
	const FBox& Bounds = WeatherGrid.GetInfo().GridBounds;
	int32 AddedCount = 0;
	for (int32 SeedIndex = 0; SeedIndex < CountToGenerate; ++SeedIndex)
	{
		FWeatherSeed Seed;
		Seed.Position = FWeatherSimulationMath::GenerateStratifiedPosition(
			Bounds,
			SeedIndex,
			CountToGenerate,
			WeatherRandomStream);
		Seed.Sigma = FWeatherSimulationMath::SampleGeneratedSigma(
			WeatherSimulationSettings,
			WeatherGrid.GetInfo(),
			WeatherRandomStream);
		Seed.Strength = WeatherRandomStream.FRandRange(
			FMath::Min(WeatherSimulationSettings.GeneratedStrengthRange.X, WeatherSimulationSettings.GeneratedStrengthRange.Y),
			FMath::Max(WeatherSimulationSettings.GeneratedStrengthRange.X, WeatherSimulationSettings.GeneratedStrengthRange.Y));
		Seed.LifetimeSeconds = WeatherRandomStream.FRandRange(
			FMath::Min(WeatherSimulationSettings.GeneratedLifetimeRange.X, WeatherSimulationSettings.GeneratedLifetimeRange.Y),
			FMath::Max(WeatherSimulationSettings.GeneratedLifetimeRange.X, WeatherSimulationSettings.GeneratedLifetimeRange.Y));
		Seed.MovementMultiplier = WeatherRandomStream.FRandRange(
			FMath::Min(WeatherSimulationSettings.GeneratedMovementMultiplierRange.X, WeatherSimulationSettings.GeneratedMovementMultiplierRange.Y),
			FMath::Max(WeatherSimulationSettings.GeneratedMovementMultiplierRange.X, WeatherSimulationSettings.GeneratedMovementMultiplierRange.Y));

		if (!WeatherSimulationSettings.WeatherTypePresets.IsEmpty())
		{
			const int32 PresetIndex = FWeatherSimulationMath::SelectGeneratedPresetIndex(
				WeatherSimulationSettings,
				SeedIndex);
			Seed.bUseWeatherTypePreset = true;
			Seed.WeatherTypePreset = WeatherSimulationSettings.WeatherTypePresets[PresetIndex].WeatherType;
		}
		else
		{
			Seed.ValueSource = EWeatherSeedValueSource::ProfileSampled;
		}

		if (AddSeed(Seed).IsValid())
		{
			++AddedCount;
		}
	}

	if (AddedCount > 0 || bReplaceExisting)
	{
		StepWeatherSimulation(0.0f);
		PreviousWeatherSnapshot = WeatherGrid.GetCells();
	}
	return AddedCount;
}

int32 UWeatherStateSubsystem::ReplenishWeatherFronts(const bool bFillImmediately)
{
	const int32 AddedCount = MaintainWeatherFrontPopulation(bFillImmediately);
	if (AddedCount > 0 && WeatherGrid.GetInfo().bIsValid)
	{
		StepWeatherSimulation(0.0f);
		PreviousWeatherSnapshot = WeatherGrid.GetCells();
	}
	return AddedCount;
}

int32 UWeatherStateSubsystem::GetTargetWeatherFrontCount() const
{
	return FWeatherSimulationMath::CalculateLifecycleTargetCount(
		WeatherSimulationSettings.FrontLifecycle,
		WeatherGrid.GetInfo(),
		WeatherSimulationSettings.MaximumSeedCount);
}

int32 UWeatherStateSubsystem::MaintainWeatherFrontPopulation(const bool bFillImmediately)
{
	if (!bWeatherSimulationConfigured
		|| !WeatherSimulationSettings.FrontLifecycle.bEnabled
		|| !WeatherGrid.GetInfo().bIsValid)
	{
		return 0;
	}

	const FWeatherFrontLifecycleSettings& Lifecycle =
		WeatherSimulationSettings.FrontLifecycle;
	const int32 TargetCount = GetTargetWeatherFrontCount();
	const int32 ManagedCount = ActiveSeeds.CountByPredicate(
		[](const FWeatherSeed& Seed)
		{
			return Seed.bManagedByLifecycle;
		});
	const int32 PopulationCount = Lifecycle.bCountExternalSeedsTowardTarget
		? ActiveSeeds.Num()
		: ManagedCount;
	const int32 MissingCount = FMath::Max(TargetCount - PopulationCount, 0);
	const int32 AvailableSlots = FMath::Max(
		WeatherSimulationSettings.MaximumSeedCount - ActiveSeeds.Num(),
		0);
	const int32 SpawnLimit = bFillImmediately
		? MissingCount
		: FMath::Min(MissingCount, Lifecycle.MaximumSpawnsPerInterval);
	const int32 SpawnCount = FMath::Min(SpawnLimit, AvailableSlots);
	const bool bAtUpwindBoundary = bInitialLifecyclePopulationApplied
		&& Lifecycle.bSpawnReplacementsAtUpwindBoundary;

	int32 AddedCount = 0;
	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		if (!SpawnLifecycleFront(bAtUpwindBoundary))
		{
			break;
		}
		++AddedCount;
	}

	bInitialLifecyclePopulationApplied = true;
	return AddedCount;
}

bool UWeatherStateSubsystem::SpawnLifecycleFront(const bool bAtUpwindBoundary)
{
	const FWeatherFrontLifecycleSettings& Lifecycle =
		WeatherSimulationSettings.FrontLifecycle;
	TArray<FWeatherFrontArchetype> EligibleArchetypes;
	EligibleArchetypes.Reserve(Lifecycle.Archetypes.Num());
	for (const FWeatherFrontArchetype& Archetype : Lifecycle.Archetypes)
	{
		const bool bHasPreset = WeatherSimulationSettings.WeatherTypePresets.ContainsByPredicate(
			[&Archetype](const FWeatherTypePreset& Preset)
			{
				return Preset.WeatherType == Archetype.WeatherType;
			});
		if (Archetype.bEnabled && Archetype.SpawnWeight > 0.0f && bHasPreset)
		{
			EligibleArchetypes.Add(Archetype);
		}
	}

	const int32 ArchetypeIndex = FWeatherSimulationMath::SelectWeightedArchetypeIndex(
		EligibleArchetypes,
		WeatherRandomStream);
	if (!EligibleArchetypes.IsValidIndex(ArchetypeIndex))
	{
		return false;
	}

	const FWeatherFrontArchetype& Archetype = EligibleArchetypes[ArchetypeIndex];
	const auto SampleRange = [this](const FVector2D& Range)
	{
		return WeatherRandomStream.FRandRange(
			static_cast<float>(FMath::Min(Range.X, Range.Y)),
			static_cast<float>(FMath::Max(Range.X, Range.Y)));
	};

	FWeatherSeed Seed;
	Seed.Position = FindLifecycleSpawnPosition(bAtUpwindBoundary);
	Seed.Sigma = FMath::Max(
		static_cast<double>(SampleRange(Archetype.SigmaCellRange))
			* WeatherGrid.GetInfo().CellSize,
		1.0);
	Seed.Strength = FMath::Max(SampleRange(Archetype.StrengthRange), 0.0f);
	Seed.LifetimeSeconds = FMath::Max(SampleRange(Archetype.LifetimeSecondsRange), 0.0f);
	Seed.MovementMultiplier = FMath::Max(
		SampleRange(Archetype.MovementMultiplierRange),
		0.0f);
	Seed.bUseWeatherTypePreset = true;
	Seed.WeatherTypePreset = Archetype.WeatherType;
	Seed.bManagedByLifecycle = true;
	return AddSeed(Seed).IsValid();
}

FVector2D UWeatherStateSubsystem::FindLifecycleSpawnPosition(
	const bool bAtUpwindBoundary)
{
	const FWeatherGridInfo& Info = WeatherGrid.GetInfo();
	const FWeatherFrontLifecycleSettings& Lifecycle =
		WeatherSimulationSettings.FrontLifecycle;
	const FVector2D Minimum(Info.GridBounds.Min.X, Info.GridBounds.Min.Y);
	const FVector2D Maximum(Info.GridBounds.Max.X, Info.GridBounds.Max.Y);
	const double InteriorInset = FMath::Min(
		Info.CellSize * 0.5,
		FMath::Min(Maximum.X - Minimum.X, Maximum.Y - Minimum.Y) * 0.49);
	const double MinimumSpacing = Info.CellSize * Lifecycle.MinimumSpacingCellWidths;
	const double MinimumSpacingSquared = FMath::Square(MinimumSpacing);
	const FVector2D PrevailingDirection = GetPrevailingWindDirection();

	FVector2D BestCandidate = Info.GridBounds.GetCenter();
	double BestNearestDistanceSquared = -1.0;
	for (int32 Attempt = 0; Attempt < Lifecycle.PositionAttempts; ++Attempt)
	{
		const FVector2D Candidate = bAtUpwindBoundary
			? FWeatherSimulationMath::GenerateUpwindBoundaryPosition(
				Info,
				PrevailingDirection,
				Lifecycle.BoundaryInsetCellFraction,
				WeatherRandomStream)
			: FVector2D(
				WeatherRandomStream.FRandRange(
					Minimum.X + InteriorInset,
					Maximum.X - InteriorInset),
				WeatherRandomStream.FRandRange(
					Minimum.Y + InteriorInset,
					Maximum.Y - InteriorInset));

		double NearestDistanceSquared = TNumericLimits<double>::Max();
		for (const FWeatherSeed& ExistingSeed : ActiveSeeds)
		{
			double DeltaX = FMath::Abs(Candidate.X - ExistingSeed.Position.X);
			double DeltaY = FMath::Abs(Candidate.Y - ExistingSeed.Position.Y);
			if (WeatherSimulationSettings.BoundaryPolicy == EWeatherSeedBoundaryPolicy::Wrap)
			{
				const double Width = Maximum.X - Minimum.X;
				const double Height = Maximum.Y - Minimum.Y;
				DeltaX = FMath::Min(DeltaX, FMath::Max(Width - DeltaX, 0.0));
				DeltaY = FMath::Min(DeltaY, FMath::Max(Height - DeltaY, 0.0));
			}
			NearestDistanceSquared = FMath::Min(
				NearestDistanceSquared,
				DeltaX * DeltaX + DeltaY * DeltaY);
		}

		if (ActiveSeeds.IsEmpty() || NearestDistanceSquared >= MinimumSpacingSquared)
		{
			return Candidate;
		}
		if (NearestDistanceSquared > BestNearestDistanceSquared)
		{
			BestCandidate = Candidate;
			BestNearestDistanceSquared = NearestDistanceSquared;
		}
	}

	// A crowded edge should not permanently starve the target population. Use
	// the best max-min candidate found after the configured number of attempts.
	return BestCandidate;
}

FVector2D UWeatherStateSubsystem::GetPrevailingWindDirection() const
{
	FVector2D AccumulatedWind = FVector2D::ZeroVector;
	for (const FWeatherCellState& Cell : WeatherGrid.GetCells())
	{
		AccumulatedWind += FVector2D(Cell.WindVector.X, Cell.WindVector.Y);
	}

	if (!AccumulatedWind.IsNearlyZero())
	{
		return AccumulatedWind.GetSafeNormal();
	}

	const FVector2D DefaultDirection(
		WindSettings.DefaultDirection.X,
		WindSettings.DefaultDirection.Y);
	return DefaultDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector2D(1.0, 0.0));
}

void UWeatherStateSubsystem::StepSimulation(const int32 StepCount)
{
	if (!bWeatherSimulationConfigured || !WeatherGrid.GetInfo().bIsValid)
	{
		return;
	}

	const int32 ClampedStepCount = FMath::Clamp(StepCount, 1, 10000);
	for (int32 StepIndex = 0; StepIndex < ClampedStepCount; ++StepIndex)
	{
		StepWeatherSimulation(WeatherSimulationSettings.FixedUpdateIntervalSeconds);
	}

	// Explicit stepping is an inspection/control API, so expose its completed
	// result immediately instead of retaining a one-step presentation delay.
	PreviousWeatherSnapshot = WeatherGrid.GetCells();
	WeatherSimulationAccumulator = 0.0f;
}

void UWeatherStateSubsystem::ApplyInitialSeeds()
{
	if (!bInitialSeedsApplied)
	{
		bInitialSeedsApplied = true;
		for (const FWeatherSeed& Seed : WeatherSimulationSettings.InitialSeeds)
		{
			if (ActiveSeeds.Num() >= WeatherSimulationSettings.MaximumSeedCount)
			{
				break;
			}
			AddSeed(Seed);
		}
	}

	if (!bInitialGeneratedSeedsApplied && WeatherGrid.GetInfo().bIsValid)
	{
		bInitialGeneratedSeedsApplied = true;
		if (WeatherSimulationSettings.InitialGeneratedSeedCount > 0)
		{
			GenerateSeedSet(WeatherSimulationSettings.InitialGeneratedSeedCount, false);
		}
	}
}

void UWeatherStateSubsystem::ResolveSeedValues(FWeatherSeed& Seed)
{
	if (Seed.bUseWeatherTypePreset)
	{
		for (const FWeatherTypePreset& Preset : WeatherSimulationSettings.WeatherTypePresets)
		{
			if (Preset.WeatherType == Seed.WeatherTypePreset)
			{
				Seed.Values = Preset.Values;
				Seed.ValueSource = EWeatherSeedValueSource::Fixed;
				return;
			}
		}
	}

	if (Seed.ValueSource == EWeatherSeedValueSource::ProfileSampled)
	{
		Seed.Values = FWeatherSimulationMath::SampleValues(
			WeatherSimulationSettings.GeneratedValueRange,
			WeatherRandomStream);
		Seed.ValueSource = EWeatherSeedValueSource::Fixed;
	}
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
	const FWeatherSample Sample = WeatherGrid.GetWeatherAtLocationBilinear(WorldLocation);
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

void UWeatherStateSubsystem::TickWeatherSimulation(const float DeltaTime)
{
	if (!bWeatherSimulationConfigured
		|| !WeatherSimulationSettings.bEnabled
		|| DeltaTime <= 0.0f)
	{
		return;
	}

	const float Interval = FMath::Max(
		WeatherSimulationSettings.FixedUpdateIntervalSeconds,
		0.001f);
	WeatherSimulationAccumulator += DeltaTime;
	int32 Steps = 0;
	while (WeatherSimulationAccumulator >= Interval
		&& Steps < WeatherSimulationSettings.MaximumSubstepsPerFrame)
	{
		StepWeatherSimulation(Interval);
		WeatherSimulationAccumulator -= Interval;
		++Steps;
	}

	if (Steps == WeatherSimulationSettings.MaximumSubstepsPerFrame
		&& WeatherSimulationAccumulator >= Interval)
	{
		WeatherSimulationAccumulator = FMath::Fmod(WeatherSimulationAccumulator, Interval);
	}
}

void UWeatherStateSubsystem::TickWeatherFrontLifecycle(const float StepSeconds)
{
	if (StepSeconds <= 0.0f
		|| !WeatherSimulationSettings.FrontLifecycle.bEnabled)
	{
		return;
	}

	const float Interval = FMath::Max(
		WeatherSimulationSettings.FrontLifecycle.ReplenishmentIntervalSeconds,
		0.01f);
	WeatherFrontLifecycleAccumulator += StepSeconds;
	if (WeatherFrontLifecycleAccumulator >= Interval)
	{
		MaintainWeatherFrontPopulation(false);
		WeatherFrontLifecycleAccumulator = FMath::Fmod(
			WeatherFrontLifecycleAccumulator,
			Interval);
	}
}

void UWeatherStateSubsystem::StepWeatherSimulation(const float StepSeconds)
{
	if (!WeatherGrid.GetInfo().bIsValid || WeatherGrid.GetCells().IsEmpty())
	{
		return;
	}

	const TArray<FWeatherCellState> PreviousCells = WeatherGrid.GetCells();
	PreviousWeatherSnapshot = PreviousCells;
	if (StepSeconds > 0.0f)
	{
		FWeatherSimulationMath::AdvectSeeds(
			ActiveSeeds,
			WeatherGrid,
			WeatherSimulationSettings,
			StepSeconds);
		WeatherSimulationTimeSeconds += StepSeconds;
		TickWeatherFrontLifecycle(StepSeconds);
	}

	FWeatherSimulationMath::RebuildCellFields(
		WeatherGrid,
		ActiveSeeds,
		WeatherSimulationSettings,
		StepSeconds,
		WeatherTypeDurations);
	BroadcastWeatherChanges(PreviousCells, StepSeconds);
	PublishWindMaterialParameters();
}

void UWeatherStateSubsystem::BroadcastWeatherChanges(
	const TArray<FWeatherCellState>& PreviousCells,
	const float StepSeconds)
{
	const TArray<FWeatherCellState>& CurrentCells = WeatherGrid.GetCells();
	const FWeatherGridInfo& Info = WeatherGrid.GetInfo();
	if (PreviousCells.Num() == CurrentCells.Num())
	{
		for (int32 CellIndex = 0; CellIndex < CurrentCells.Num(); ++CellIndex)
		{
			if (PreviousCells[CellIndex].WeatherType != CurrentCells[CellIndex].WeatherType)
			{
				OnWeatherTypeChanged.Broadcast(
					FWeatherCellCoord(CellIndex % Info.Dimensions.X, CellIndex / Info.Dimensions.X),
					PreviousCells[CellIndex].WeatherType,
					CurrentCells[CellIndex].WeatherType);
			}
		}
	}

	LocalWeatherEventElapsedSeconds += FMath::Max(StepSeconds, 0.0f);
	FVector SampleLocation = Info.GridBounds.GetCenter();
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		SampleLocation = Camera->GetCameraLocation();
		SampleLocation.Z = Info.GridBounds.GetCenter().Z;
	}
	const FWeatherSample CurrentLocalWeather = WeatherGrid.GetWeatherAtLocationBilinear(SampleLocation);
	if (!CurrentLocalWeather.bIsValid)
	{
		return;
	}

	const float Threshold = FMath::Max(WeatherSimulationSettings.LocalWeatherChangeThreshold, 0.0f);
	const FWeatherCellState& Current = CurrentLocalWeather.State;
	const FWeatherCellState& Previous = LastBroadcastLocalWeather.State;
	const bool bCategoricalChanged = !LastBroadcastLocalWeather.bIsValid
		|| Current.WeatherType != Previous.WeatherType
		|| Current.bIsRaining != Previous.bIsRaining
		|| Current.bIsStorm != Previous.bIsStorm;
	const bool bContinuousChanged = !LastBroadcastLocalWeather.bIsValid
		|| FMath::Abs(Current.CloudCoverage - Previous.CloudCoverage) >= Threshold
		|| FMath::Abs(Current.CloudDensity - Previous.CloudDensity) >= Threshold
		|| FMath::Abs(Current.Humidity - Previous.Humidity) >= Threshold
		|| FMath::Abs(Current.Storminess - Previous.Storminess) >= Threshold
		|| FMath::Abs(Current.RainIntensity - Previous.RainIntensity) >= Threshold
		|| FMath::Abs(Current.LightningPotential - Previous.LightningPotential) >= Threshold
		|| FMath::Abs(Current.TemperatureCelsius - Previous.TemperatureCelsius) >= Threshold * 50.0f
		|| FMath::Abs(Current.PressureHpa - Previous.PressureHpa) >= Threshold * 100.0f;
	if ((bCategoricalChanged || bContinuousChanged)
		&& (!LastBroadcastLocalWeather.bIsValid
			|| LocalWeatherEventElapsedSeconds >= WeatherSimulationSettings.LocalWeatherEventDebounceSeconds))
	{
		LastBroadcastLocalWeather = CurrentLocalWeather;
		LocalWeatherEventElapsedSeconds = 0.0f;
		OnLocalWeatherChanged.Broadcast(CurrentLocalWeather);
	}
}

FWeatherSample UWeatherStateSubsystem::BuildInterpolatedWeatherSample(
	const FVector& WorldLocation) const
{
	FWeatherSample Current = WeatherGrid.GetWeatherAtLocationBilinear(WorldLocation);
	if (!Current.bIsValid
		|| !bWeatherSimulationConfigured
		|| PreviousWeatherSnapshot.Num() != WeatherGrid.GetInfo().CellCount)
	{
		return Current;
	}

	const FWeatherSample Previous = WeatherGrid.GetWeatherAtLocationBilinear(
		WorldLocation,
		PreviousWeatherSnapshot);
	if (!Previous.bIsValid)
	{
		return Current;
	}

	const float Alpha = FMath::Clamp(
		WeatherSimulationAccumulator
			/ FMath::Max(WeatherSimulationSettings.FixedUpdateIntervalSeconds, 0.001f),
		0.0f,
		1.0f);
	Current.State.CloudCoverage = FMath::Lerp(Previous.State.CloudCoverage, Current.State.CloudCoverage, Alpha);
	Current.State.CloudDensity = FMath::Lerp(Previous.State.CloudDensity, Current.State.CloudDensity, Alpha);
	Current.State.Humidity = FMath::Lerp(Previous.State.Humidity, Current.State.Humidity, Alpha);
	Current.State.TemperatureCelsius = FMath::Lerp(
		Previous.State.TemperatureCelsius,
		Current.State.TemperatureCelsius,
		Alpha);
	Current.State.PressureHpa = FMath::Lerp(Previous.State.PressureHpa, Current.State.PressureHpa, Alpha);
	Current.State.Storminess = FMath::Lerp(Previous.State.Storminess, Current.State.Storminess, Alpha);
	Current.State.RainIntensity = FMath::Lerp(Previous.State.RainIntensity, Current.State.RainIntensity, Alpha);
	Current.State.LightningPotential = FMath::Lerp(
		Previous.State.LightningPotential,
		Current.State.LightningPotential,
		Alpha);
	return Current;
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
			const FWeatherSample Sample = WeatherGrid.GetWeatherAtLocationBilinear(SampleLocation);
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

		const FWeatherSample Sample = WeatherGrid.GetWeatherAtLocationBilinear(LocalLocation);
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
