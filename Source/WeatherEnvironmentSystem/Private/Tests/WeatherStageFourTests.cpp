// Copyright James Joslin. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WeatherGrid.h"
#include "WeatherSimulation.h"

namespace WeatherStageFourTests
{
	FWeatherGrid BuildGrid(const int32 Width, const int32 Height, const double CellSize = 100.0)
	{
		FWeatherGridDefinition Definition;
		Definition.CellSize = CellSize;
		Definition.MaximumCellCount = Width * Height;
		Definition.VerticalQueryMinimum = -10.0;
		Definition.VerticalQueryMaximum = 10.0;
		Definition.bSnapOriginToCellSize = false;

		FWeatherGrid Grid;
		Grid.Rebuild(
			FBox(
				FVector::ZeroVector,
				FVector(Width * CellSize, Height * CellSize, 1.0)),
			Definition);
		return Grid;
	}

	FWeatherSeed MakeRainSeed(const FVector2D Position, const double Sigma, const float Rain)
	{
		FWeatherSeed Seed;
		Seed.Position = Position;
		Seed.Sigma = Sigma;
		Seed.Strength = 1.0f;
		Seed.Values.RainIntensity = Rain;
		return Seed;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherGaussianPropagationTest,
	"WeatherEnvironment.Stage4.Simulation.GaussianPropagationAndBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherGaussianPropagationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Gaussian is one at its center"), FMath::IsNearlyEqual(
		FWeatherSimulationMath::GaussianWeight(0.0, 100.0), 1.0, 0.000001));
	TestTrue(TEXT("Gaussian has the expected one-sigma falloff"), FMath::IsNearlyEqual(
		FWeatherSimulationMath::GaussianWeight(100.0, 100.0), FMath::Exp(-0.5), 0.000001));

	FWeatherGrid Grid = WeatherStageFourTests::BuildGrid(9, 1);
	FWeatherSimulationSettings Settings;
	Settings.BoundaryPolicy = EWeatherSeedBoundaryPolicy::Clamp;
	Settings.BaselineWeight = 0.1f;
	Settings.BaselineValues.RainIntensity = 0.0f;
	Settings.MinimumWeatherTypeDurationSeconds = 0.0f;

	TArray<FWeatherSeed> Seeds;
	Seeds.Add(WeatherStageFourTests::MakeRainSeed(FVector2D(350.0, 50.0), 100.0, 1.0f));
	TArray<float> Durations;
	FWeatherSimulationMath::RebuildCellFields(Grid, Seeds, Settings, 1.0f, Durations);

	const FWeatherCellState* Center = Grid.FindCell(FWeatherCellCoord(3, 0));
	const FWeatherCellState* OneSigma = Grid.FindCell(FWeatherCellCoord(4, 0));
	const FWeatherCellState* Uncovered = Grid.FindCell(FWeatherCellCoord(8, 0));
	TestNotNull(TEXT("Center cell exists"), Center);
	TestNotNull(TEXT("One-sigma cell exists"), OneSigma);
	TestNotNull(TEXT("Uncovered cell exists"), Uncovered);
	if (Center && OneSigma && Uncovered)
	{
		TestTrue(TEXT("Center uses normalized seed and baseline weights"), FMath::IsNearlyEqual(
			Center->RainIntensity, 1.0f / 1.1f, 0.0001f));
		const float OneSigmaWeight = FMath::Exp(-0.5f);
		TestTrue(TEXT("One-sigma field value uses normalized Gaussian weight"), FMath::IsNearlyEqual(
			OneSigma->RainIntensity, OneSigmaWeight / (OneSigmaWeight + 0.1f), 0.0001f));
		TestTrue(TEXT("Cells outside three sigma retain the profile baseline"), FMath::IsNearlyZero(
			Uncovered->RainIntensity, 0.0001f));
	}

	FWeatherGrid OverlapGrid = WeatherStageFourTests::BuildGrid(1, 1);
	Settings.BaselineWeight = 0.0001f;
	Seeds.Reset();
	Seeds.Add(WeatherStageFourTests::MakeRainSeed(FVector2D(50.0, 50.0), 100.0, 0.0f));
	Seeds.Add(WeatherStageFourTests::MakeRainSeed(FVector2D(50.0, 50.0), 100.0, 1.0f));
	FWeatherSimulationMath::RebuildCellFields(OverlapGrid, Seeds, Settings, 1.0f, Durations);
	const FWeatherCellState* Overlap = OverlapGrid.FindCell(FWeatherCellCoord(0, 0));
	TestNotNull(TEXT("Overlap cell exists"), Overlap);
	if (Overlap)
	{
		TestTrue(TEXT("Equal overlapping fronts normalize to their weighted mean"), FMath::IsNearlyEqual(
			Overlap->RainIntensity, 1.0f / 2.0001f, 0.0001f));
	}

	FWeatherGrid InterpolationGrid = WeatherStageFourTests::BuildGrid(2, 1);
	InterpolationGrid.GetMutableCells()[0].CloudCoverage = 0.0f;
	InterpolationGrid.GetMutableCells()[0].WindVector = FVector(100.0, 0.0, 0.0);
	InterpolationGrid.GetMutableCells()[1].CloudCoverage = 1.0f;
	InterpolationGrid.GetMutableCells()[1].WindVector = FVector(-100.0, 0.0, 0.0);
	const FWeatherSample Interpolated = InterpolationGrid.GetWeatherAtLocationBilinear(
		FVector(100.0, 50.0, 0.0));
	TestTrue(TEXT("Continuous point queries bilinearly blend adjacent cell centres"),
		Interpolated.bIsValid && FMath::IsNearlyEqual(Interpolated.State.CloudCoverage, 0.5f));
	TestTrue(TEXT("Opposing interpolated wind remains finite at cancellation"),
		Interpolated.bIsValid && !Interpolated.State.WindVector.ContainsNaN());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherSeedAdvectionBoundaryTest,
	"WeatherEnvironment.Stage4.Simulation.AdvectionAndBoundaryPolicies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherSeedAdvectionBoundaryTest::RunTest(const FString& Parameters)
{
	FWeatherGrid Grid = WeatherStageFourTests::BuildGrid(2, 1);
	for (FWeatherCellState& Cell : Grid.GetMutableCells())
	{
		Cell.WindVector = FVector(100.0, 0.0, 0.0);
	}

	FWeatherSimulationSettings Settings;
	FWeatherSeed Seed;
	Seed.Position = FVector2D(195.0, 50.0);
	Seed.MovementMultiplier = 1.0f;

	TArray<FWeatherSeed> Seeds;
	Seeds.Add(Seed);
	Settings.BoundaryPolicy = EWeatherSeedBoundaryPolicy::Wrap;
	FWeatherSimulationMath::AdvectSeeds(Seeds, Grid, Settings, 0.1f);
	TestEqual(TEXT("Wrap retains the seed"), Seeds.Num(), 1);
	if (!Seeds.IsEmpty())
	{
		TestTrue(TEXT("Wrap continues the seed at the opposite grid edge"), FMath::IsNearlyEqual(
			Seeds[0].Position.X, 5.0, 0.001));
	}

	Seeds.Reset();
	Seeds.Add(Seed);
	Settings.BoundaryPolicy = EWeatherSeedBoundaryPolicy::Clamp;
	FWeatherSimulationMath::AdvectSeeds(Seeds, Grid, Settings, 0.1f);
	TestEqual(TEXT("Clamp retains the seed"), Seeds.Num(), 1);
	if (!Seeds.IsEmpty())
	{
		TestTrue(TEXT("Clamp pins the seed to the maximum boundary"), FMath::IsNearlyEqual(
			Seeds[0].Position.X, 200.0, 0.001));
	}

	Seeds.Reset();
	Seeds.Add(Seed);
	Settings.BoundaryPolicy = EWeatherSeedBoundaryPolicy::Expire;
	FWeatherSimulationMath::AdvectSeeds(Seeds, Grid, Settings, 0.1f);
	TestTrue(TEXT("Expire removes a seed after it leaves the grid"), Seeds.IsEmpty());

	Seed.Position = FVector2D(50.0, 50.0);
	Seed.LifetimeSeconds = 0.1f;
	Seeds.Add(Seed);
	FWeatherSimulationMath::AdvectSeeds(Seeds, Grid, Settings, 0.1f);
	TestTrue(TEXT("Finite lifetime expiry is deterministic at the exact boundary"), Seeds.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherClassificationStabilityTest,
	"WeatherEnvironment.Stage4.Classification.PriorityBoundariesAndHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherClassificationStabilityTest::RunTest(const FString& Parameters)
{
	UWeatherTypeLookupDataAsset* Lookup = NewObject<UWeatherTypeLookupDataAsset>();
	FWeatherTypeClassificationRule RainRule;
	RainRule.WeatherType = EWeatherType::Rain;
	RainRule.Priority = 10;
	RainRule.RainIntensity.bEnabled = true;
	RainRule.RainIntensity.Minimum = 0.55f;
	RainRule.RainIntensity.Maximum = 1.0f;
	RainRule.Hysteresis = 0.1f;
	Lookup->Rules.Add(RainRule);

	FWeatherTypeClassificationRule StormRule;
	StormRule.WeatherType = EWeatherType::Storm;
	StormRule.Priority = 20;
	StormRule.Storminess.bEnabled = true;
	StormRule.Storminess.Minimum = 0.65f;
	StormRule.Storminess.Maximum = 1.0f;
	Lookup->Rules.Add(StormRule);

	FWeatherCellState State;
	State.RainIntensity = 0.8f;
	State.Storminess = 0.8f;
	TestTrue(TEXT("Highest-priority matching rule wins"),
		Lookup->ClassifyWeather(State, EWeatherType::Clear) == EWeatherType::Storm);

	State.Storminess = 0.0f;
	State.RainIntensity = 0.55f;
	TestTrue(TEXT("Classification range boundaries are inclusive"),
		Lookup->ClassifyWeather(State, EWeatherType::Clear) == EWeatherType::Rain);
	State.RainIntensity = 0.5f;
	TestTrue(TEXT("Current classification retains its expanded hysteresis range"),
		Lookup->ClassifyWeather(State, EWeatherType::Rain) == EWeatherType::Rain);
	State.RainIntensity = 0.44f;
	TestTrue(TEXT("Classification exits after crossing the hysteresis band"),
		Lookup->ClassifyWeather(State, EWeatherType::Rain) == EWeatherType::Clear);

	UWeatherTypeLookupDataAsset* OrderedLookup = NewObject<UWeatherTypeLookupDataAsset>();
	FWeatherTypeClassificationRule FirstRule;
	FirstRule.WeatherType = EWeatherType::Overcast;
	FirstRule.Priority = 5;
	FWeatherTypeClassificationRule SecondRule;
	SecondRule.WeatherType = EWeatherType::Rain;
	SecondRule.Priority = 5;
	OrderedLookup->Rules = {FirstRule, SecondRule};
	TestTrue(TEXT("Equal-priority ties retain the lookup asset order"),
		OrderedLookup->ClassifyWeather(State, EWeatherType::Clear) == EWeatherType::Overcast);

	FWeatherGrid Grid = WeatherStageFourTests::BuildGrid(1, 1);
	FWeatherSimulationSettings Settings;
	Settings.BaselineValues.Storminess = 0.9f;
	Settings.BaselineValues.RainIntensity = 0.6f;
	Settings.MinimumWeatherTypeDurationSeconds = 1.0f;
	TArray<float> Durations;
	TArray<FWeatherSeed> NoSeeds;
	FWeatherSimulationMath::RebuildCellFields(Grid, NoSeeds, Settings, 0.5f, Durations);
	TestTrue(TEXT("Minimum duration prevents the first type transition"),
		Grid.GetCells()[0].WeatherType == EWeatherType::Clear);
	FWeatherSimulationMath::RebuildCellFields(Grid, NoSeeds, Settings, 0.5f, Durations);
	TestTrue(TEXT("Minimum duration retains the type until the complete interval elapses"),
		Grid.GetCells()[0].WeatherType == EWeatherType::Clear);
	FWeatherSimulationMath::RebuildCellFields(Grid, NoSeeds, Settings, 0.5f, Durations);
	TestTrue(TEXT("Type changes after its minimum state duration"),
		Grid.GetCells()[0].WeatherType == EWeatherType::Storm);

	Settings.MinimumWeatherTypeDurationSeconds = 0.0f;
	Settings.BaselineValues.Storminess = 0.0f;
	Settings.BaselineValues.RainIntensity = 0.6f;
	FWeatherSimulationMath::RebuildCellFields(Grid, NoSeeds, Settings, 0.1f, Durations);
	TestTrue(TEXT("Rain enters at the configured enter threshold"), Grid.GetCells()[0].bIsRaining);
	Settings.BaselineValues.RainIntensity = 0.5f;
	FWeatherSimulationMath::RebuildCellFields(Grid, NoSeeds, Settings, 0.1f, Durations);
	TestTrue(TEXT("Rain remains active inside the hysteresis band"), Grid.GetCells()[0].bIsRaining);
	Settings.BaselineValues.RainIntensity = 0.4f;
	FWeatherSimulationMath::RebuildCellFields(Grid, NoSeeds, Settings, 0.1f, Durations);
	TestFalse(TEXT("Rain exits below the configured exit threshold"), Grid.GetCells()[0].bIsRaining);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherDeterminismAndBoundedWorkTest,
	"WeatherEnvironment.Stage4.Simulation.DeterminismAndBoundedWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherDeterminismAndBoundedWorkTest::RunTest(const FString& Parameters)
{
	FWeatherSimulationSettings DefaultSettings;
	TestEqual(TEXT("The production lifecycle replaces the legacy fixed initial count"),
		DefaultSettings.InitialGeneratedSeedCount, 0);
	TestTrue(TEXT("The production front lifecycle is enabled by default"),
		DefaultSettings.FrontLifecycle.bEnabled);
	FWeatherGridInfo DefaultGridInfo;
	DefaultGridInfo.bIsValid = true;
	DefaultGridInfo.CellSize = 1000.0;
	DefaultGridInfo.CellCount = 12;
	DefaultGridInfo.GridBounds = FBox(FVector::ZeroVector, FVector(4000.0, 3000.0, 1.0));
	TestEqual(TEXT("A twelve-cell test grid targets four active fronts"),
		FWeatherSimulationMath::CalculateLifecycleTargetCount(
			DefaultSettings.FrontLifecycle,
			DefaultGridInfo,
			DefaultSettings.MaximumSeedCount),
		4);
	FRandomStream SigmaStream(1337);
	const double DefaultSigma = FWeatherSimulationMath::SampleGeneratedSigma(
		DefaultSettings,
		DefaultGridInfo,
		SigmaStream);
	TestTrue(TEXT("Default generated sigma scales to the current cell size"),
		DefaultSigma >= 350.0 && DefaultSigma <= 650.0);

	const EWeatherType ExpectedDefaultTypes[] = {
		EWeatherType::Overcast,
		EWeatherType::Rain,
		EWeatherType::Storm,
		EWeatherType::PartlyCloudy
	};
	for (int32 SeedIndex = 0; SeedIndex < UE_ARRAY_COUNT(ExpectedDefaultTypes); ++SeedIndex)
	{
		const int32 PresetIndex = FWeatherSimulationMath::SelectGeneratedPresetIndex(
			DefaultSettings,
			SeedIndex);
		TestTrue(TEXT("Default generated seeds select visibly distinct weather presets"),
			DefaultSettings.WeatherTypePresets.IsValidIndex(PresetIndex)
			&& DefaultSettings.WeatherTypePresets[PresetIndex].WeatherType == ExpectedDefaultTypes[SeedIndex]);
	}

	const FBox DefaultBounds(FVector::ZeroVector, FVector(400.0, 300.0, 1.0));
	FRandomStream PositionStream(1337);
	TArray<FVector2D> DefaultPositions;
	for (int32 SeedIndex = 0; SeedIndex < 4; ++SeedIndex)
	{
		DefaultPositions.Add(FWeatherSimulationMath::GenerateStratifiedPosition(
			DefaultBounds,
			SeedIndex,
			4,
			PositionStream));
	}
	TestTrue(TEXT("Four default seeds occupy separate grid quadrants"),
		DefaultPositions[0].X < 200.0 && DefaultPositions[0].Y < 150.0
		&& DefaultPositions[1].X >= 200.0 && DefaultPositions[1].Y < 150.0
		&& DefaultPositions[2].X < 200.0 && DefaultPositions[2].Y >= 150.0
		&& DefaultPositions[3].X >= 200.0 && DefaultPositions[3].Y >= 150.0);

	const FGuid StableId = FWeatherSimulationMath::MakeDeterministicSeedId(1337, 4);
	TestTrue(TEXT("Identical environment seed and command serial produce the same stable ID"),
		StableId == FWeatherSimulationMath::MakeDeterministicSeedId(1337, 4));
	TestTrue(TEXT("A different seed command serial produces a different stable ID"),
		StableId != FWeatherSimulationMath::MakeDeterministicSeedId(1337, 5));

	FWeatherSimulationSettings SampleSettings;
	FRandomStream StreamA(1337);
	FRandomStream StreamB(1337);
	const FWeatherSeedValues SampleA = FWeatherSimulationMath::SampleValues(
		SampleSettings.GeneratedValueRange,
		StreamA);
	const FWeatherSeedValues SampleB = FWeatherSimulationMath::SampleValues(
		SampleSettings.GeneratedValueRange,
		StreamB);
	TestTrue(TEXT("Profile-sampled weather values replay deterministically"),
		SampleA.CloudCoverage == SampleB.CloudCoverage
		&& SampleA.PressureHpa == SampleB.PressureHpa
		&& SampleA.LightningPotential == SampleB.LightningPotential);

	FWeatherGrid GridA = WeatherStageFourTests::BuildGrid(8, 2);
	FWeatherGrid GridB = WeatherStageFourTests::BuildGrid(8, 2);
	for (int32 CellIndex = 0; CellIndex < GridA.GetMutableCells().Num(); ++CellIndex)
	{
		const FVector Wind(50.0 + CellIndex, 25.0, 0.0);
		GridA.GetMutableCells()[CellIndex].WindVector = Wind;
		GridB.GetMutableCells()[CellIndex].WindVector = Wind;
	}

	FWeatherSimulationSettings Settings;
	Settings.BoundaryPolicy = EWeatherSeedBoundaryPolicy::Wrap;
	Settings.MinimumWeatherTypeDurationSeconds = 0.0f;
	TArray<FWeatherSeed> SeedsA;
	SeedsA.Add(WeatherStageFourTests::MakeRainSeed(FVector2D(175.0, 75.0), 80.0, 0.8f));
	TArray<FWeatherSeed> SeedsB = SeedsA;
	TArray<float> DurationsA;
	TArray<float> DurationsB;
	for (int32 Step = 0; Step < 20; ++Step)
	{
		FWeatherSimulationMath::AdvectSeeds(SeedsA, GridA, Settings, 1.0f / 30.0f);
		FWeatherSimulationMath::AdvectSeeds(SeedsB, GridB, Settings, 1.0f / 30.0f);
		FWeatherSimulationMath::RebuildCellFields(GridA, SeedsA, Settings, 1.0f / 30.0f, DurationsA);
		FWeatherSimulationMath::RebuildCellFields(GridB, SeedsB, Settings, 1.0f / 30.0f, DurationsB);
	}
	TestEqual(TEXT("Identical fixed steps retain the same seed count"), SeedsA.Num(), SeedsB.Num());
	if (!SeedsA.IsEmpty() && !SeedsB.IsEmpty())
	{
		TestTrue(TEXT("Identical fixed steps retain bit-stable seed positions"),
			SeedsA[0].Position == SeedsB[0].Position);
	}
	for (int32 CellIndex = 0; CellIndex < GridA.GetCells().Num(); ++CellIndex)
	{
		TestTrue(TEXT("Identical replay produces the same continuous field"), FMath::IsNearlyEqual(
			GridA.GetCells()[CellIndex].RainIntensity,
			GridB.GetCells()[CellIndex].RainIntensity,
			0.000001f));
		TestTrue(TEXT("Identical replay produces the same classification"),
			GridA.GetCells()[CellIndex].WeatherType == GridB.GetCells()[CellIndex].WeatherType);
	}

	FWeatherGrid StressGrid = WeatherStageFourTests::BuildGrid(64, 64);
	TArray<FWeatherSeed> StressSeeds;
	for (int32 SeedIndex = 0; SeedIndex < 32; ++SeedIndex)
	{
		StressSeeds.Add(WeatherStageFourTests::MakeRainSeed(
			FVector2D((SeedIndex % 8) * 800.0 + 50.0, (SeedIndex / 8) * 1500.0 + 50.0),
			50.0,
			0.75f));
	}
	TArray<float> StressDurations;
	int64 EvaluatedCellCount = 0;
	FWeatherSimulationMath::RebuildCellFields(
		StressGrid,
		StressSeeds,
		Settings,
		1.0f / 30.0f,
		StressDurations,
		&EvaluatedCellCount);
	TestTrue(TEXT("Neighborhood-limited propagation avoids seed-by-entire-grid work"),
		EvaluatedCellCount < static_cast<int64>(StressSeeds.Num()) * StressGrid.GetInfo().CellCount / 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherFrontLifecyclePolicyTest,
	"WeatherEnvironment.Stage4.Lifecycle.TargetWeightingAndUpwindSpawning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherFrontLifecyclePolicyTest::RunTest(const FString& Parameters)
{
	FWeatherSimulationSettings Settings;
	FWeatherGridInfo GridInfo;
	GridInfo.bIsValid = true;
	GridInfo.CellSize = 100.0;
	GridInfo.GridBounds = FBox(FVector::ZeroVector, FVector(400.0, 300.0, 1.0));

	GridInfo.CellCount = 100;
	TestEqual(TEXT("Target population scales with grid area"),
		FWeatherSimulationMath::CalculateLifecycleTargetCount(
			Settings.FrontLifecycle,
			GridInfo,
			Settings.MaximumSeedCount),
		7);
	GridInfo.CellCount = 4096;
	TestEqual(TEXT("Target population respects the lifecycle maximum"),
		FWeatherSimulationMath::CalculateLifecycleTargetCount(
			Settings.FrontLifecycle,
			GridInfo,
			Settings.MaximumSeedCount),
		Settings.FrontLifecycle.MaximumFrontCount);
	TestEqual(TEXT("Target population also respects the global allocation cap"),
		FWeatherSimulationMath::CalculateLifecycleTargetCount(
			Settings.FrontLifecycle,
			GridInfo,
			10),
		10);

	FRandomStream SelectionA(4815);
	FRandomStream SelectionB(4815);
	TMap<EWeatherType, int32> SelectionCounts;
	bool bSelectionReplayMatched = true;
	for (int32 SelectionIndex = 0; SelectionIndex < 10000; ++SelectionIndex)
	{
		const int32 IndexA = FWeatherSimulationMath::SelectWeightedArchetypeIndex(
			Settings.FrontLifecycle.Archetypes,
			SelectionA);
		const int32 IndexB = FWeatherSimulationMath::SelectWeightedArchetypeIndex(
			Settings.FrontLifecycle.Archetypes,
			SelectionB);
		bSelectionReplayMatched &= IndexA == IndexB;
		if (Settings.FrontLifecycle.Archetypes.IsValidIndex(IndexA))
		{
			++SelectionCounts.FindOrAdd(
				Settings.FrontLifecycle.Archetypes[IndexA].WeatherType);
		}
	}
	TestTrue(TEXT("Weighted lifecycle selection replays deterministically"),
		bSelectionReplayMatched);
	TestTrue(TEXT("Partly cloudy fronts are more common than storm fronts"),
		SelectionCounts.FindRef(EWeatherType::PartlyCloudy)
			> SelectionCounts.FindRef(EWeatherType::Storm) * 5);
	TestTrue(TEXT("Storm fronts remain possible with the production weighting"),
		SelectionCounts.FindRef(EWeatherType::Storm) > 0);

	TArray<FWeatherFrontArchetype> DisabledArchetypes = Settings.FrontLifecycle.Archetypes;
	for (FWeatherFrontArchetype& Archetype : DisabledArchetypes)
	{
		Archetype.bEnabled = false;
	}
	TestEqual(TEXT("No eligible archetype produces no selection"),
		FWeatherSimulationMath::SelectWeightedArchetypeIndex(
			DisabledArchetypes,
			SelectionA),
		INDEX_NONE);

	GridInfo.CellCount = 12;
	FRandomStream PositionStream(77);
	const FVector2D FromWest = FWeatherSimulationMath::GenerateUpwindBoundaryPosition(
		GridInfo,
		FVector2D(1.0, 0.0),
		0.15f,
		PositionStream);
	const FVector2D FromEast = FWeatherSimulationMath::GenerateUpwindBoundaryPosition(
		GridInfo,
		FVector2D(-1.0, 0.0),
		0.15f,
		PositionStream);
	const FVector2D FromSouth = FWeatherSimulationMath::GenerateUpwindBoundaryPosition(
		GridInfo,
		FVector2D(0.0, 1.0),
		0.15f,
		PositionStream);
	const FVector2D FromNorth = FWeatherSimulationMath::GenerateUpwindBoundaryPosition(
		GridInfo,
		FVector2D(0.0, -1.0),
		0.15f,
		PositionStream);
	TestTrue(TEXT("Positive-X wind spawns on the west edge"),
		FMath::IsNearlyEqual(FromWest.X, 15.0, 0.001));
	TestTrue(TEXT("Negative-X wind spawns on the east edge"),
		FMath::IsNearlyEqual(FromEast.X, 385.0, 0.001));
	TestTrue(TEXT("Positive-Y wind spawns on the south edge"),
		FMath::IsNearlyEqual(FromSouth.Y, 15.0, 0.001));
	TestTrue(TEXT("Negative-Y wind spawns on the north edge"),
		FMath::IsNearlyEqual(FromNorth.Y, 285.0, 0.001));
	return true;
}

#endif
