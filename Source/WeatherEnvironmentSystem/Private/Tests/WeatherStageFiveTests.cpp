// Copyright James Joslin. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/WeatherLightningPresenter.h"
#include "Components/WeatherPrecipitationPresenter.h"
#include "Misc/AutomationTest.h"
#include "WeatherGrid.h"
#include "WeatherPresentation.h"

namespace WeatherStageFiveTests
{
	FWeatherGrid BuildGrid(const int32 Width, const int32 Height, const double CellSize = 100.0)
	{
		FWeatherGridDefinition Definition;
		Definition.CellSize = CellSize;
		Definition.MaximumCellCount = Width * Height;
		Definition.VerticalQueryMinimum = -50.0;
		Definition.VerticalQueryMaximum = 50.0;
		Definition.bSnapOriginToCellSize = false;

		FWeatherGrid Grid;
		Grid.Rebuild(
			FBox(
				FVector::ZeroVector,
				FVector(Width * CellSize, Height * CellSize, 1.0)),
			Definition);
		return Grid;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherPrecipitationSelectionTest,
	"WeatherEnvironment.Stage5.Precipitation.RangePriorityAndPoolExhaustion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherPrecipitationSelectionTest::RunTest(const FString& Parameters)
{
	FWeatherGrid Grid = WeatherStageFiveTests::BuildGrid(4, 1);
	const float Intensities[] = {0.2f, 0.9f, 0.7f, 0.5f};
	for (int32 CellIndex = 0; CellIndex < Grid.GetMutableCells().Num(); ++CellIndex)
	{
		Grid.GetMutableCells()[CellIndex].bIsRaining = true;
		Grid.GetMutableCells()[CellIndex].RainIntensity = Intensities[CellIndex];
	}

	FWeatherPrecipitationPresentationSettings Settings;
	Settings.PresentationRadiusInCells = 10.0f;
	Settings.IntensityPriorityWeight = 1.0f;
	Settings.ProximityPriorityWeight = 0.0f;
	const TArray<FVector> Views = {FVector(50.0, 50.0, 0.0)};
	TArray<FWeatherPrecipitationCandidate> Candidates;
	FWeatherPresentationMath::SelectPrecipitationCells(
		Grid,
		Views,
		Settings,
		2,
		Candidates);
	TestEqual(TEXT("Pool cap limits selected rain cells"), Candidates.Num(), 2);
	if (Candidates.Num() == 2)
	{
		TestEqual(TEXT("Highest-intensity cell wins first"), Candidates[0].CellCoord.X, 1);
		TestEqual(TEXT("Second-highest intensity receives the remaining slot"), Candidates[1].CellCoord.X, 2);
	}

	Grid.GetMutableCells()[1].bIsRaining = false;
	Grid.GetMutableCells()[0].RainIntensity = 0.01f;
	FWeatherPresentationMath::SelectPrecipitationCells(
		Grid,
		Views,
		Settings,
		4,
		Candidates);
	TestFalse(
		TEXT("Instantaneous intensity cannot bypass the authoritative rain boolean"),
		Candidates.ContainsByPredicate([](const FWeatherPrecipitationCandidate& Candidate)
		{
			return Candidate.CellCoord.X == 1;
		}));
	TestTrue(
		TEXT("A hysteretically retained rain cell remains presentable at low intensity"),
		Candidates.ContainsByPredicate([](const FWeatherPrecipitationCandidate& Candidate)
		{
			return Candidate.CellCoord.X == 0;
		}));

	Settings.PresentationRadiusInCells = 0.25f;
	const TArray<FVector> DistantView = {FVector(-100.0, 50.0, 0.0)};
	FWeatherPresentationMath::SelectPrecipitationCells(
		Grid,
		DistantView,
		Settings,
		4,
		Candidates);
	TestTrue(TEXT("Distant rain cells allocate no Niagara candidates"), Candidates.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherPrecipitationTransitionTest,
	"WeatherEnvironment.Stage5.Precipitation.FadeTransitionsAndMissingAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherPrecipitationTransitionTest::RunTest(const FString& Parameters)
{
	float Alpha = FWeatherPresentationMath::AdvanceTransitionAlpha(0.0f, true, 0.25f, 1.0f, 2.0f);
	TestTrue(TEXT("Fade-in advances without a component respawn"), FMath::IsNearlyEqual(Alpha, 0.25f));
	Alpha = FWeatherPresentationMath::AdvanceTransitionAlpha(Alpha, true, 1.0f, 1.0f, 2.0f);
	TestTrue(TEXT("Fade-in clamps at one"), FMath::IsNearlyEqual(Alpha, 1.0f));
	Alpha = FWeatherPresentationMath::AdvanceTransitionAlpha(Alpha, false, 0.5f, 1.0f, 2.0f);
	TestTrue(TEXT("Fade-out uses its independent duration"), FMath::IsNearlyEqual(Alpha, 0.75f));
	TestTrue(TEXT("A zero-duration transition completes immediately"), FMath::IsNearlyZero(
		FWeatherPresentationMath::AdvanceTransitionAlpha(Alpha, false, 0.0f, 1.0f, 0.0f)));

	UWeatherPrecipitationPresenter* Presenter = NewObject<UWeatherPrecipitationPresenter>();
	FWeatherPrecipitationPresentationSettings MissingAssetSettings;
	MissingAssetSettings.RainSystem.Reset();
	Presenter->Configure(nullptr, MissingAssetSettings, FWeatherNiagaraParameterNames());
	const TArray<FVector> NoViews;
	Presenter->UpdatePresentation(1.0f, NoViews);
	TestEqual(TEXT("A missing rain asset allocates no Niagara components"), Presenter->GetAllocatedComponentCount(), 0);
	TestEqual(TEXT("A missing rain asset leaves no active cells"), Presenter->GetActiveCellCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherLightningDeterminismTest,
	"WeatherEnvironment.Stage5.Lightning.DeterministicTimingAndPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherLightningDeterminismTest::RunTest(const FString& Parameters)
{
	FWeatherLightningPresentationSettings Settings;
	Settings.MinimumStrikeIntervalSeconds = 5.0f;
	Settings.MaximumStrikeIntervalSeconds = 15.0f;
	Settings.MinimumLightningPotential = 0.5f;
	Settings.LowPotentialIntervalMultiplier = 2.0f;
	Settings.CellEdgeInsetFraction = 0.1f;
	const FWeatherCellCoord Coord(3, -2);
	const float FirstA = FWeatherPresentationMath::CalculateLightningStrikeInterval(
		Settings, 1337, Coord, 0, 1.0f);
	const float FirstB = FWeatherPresentationMath::CalculateLightningStrikeInterval(
		Settings, 1337, Coord, 0, 1.0f);
	const float LowPotential = FWeatherPresentationMath::CalculateLightningStrikeInterval(
		Settings, 1337, Coord, 0, 0.5f);
	TestTrue(TEXT("Identical cell timers replay exactly"), FirstA == FirstB);
	TestTrue(TEXT("Randomized interval remains inside the configured range"), FirstA >= 5.0f && FirstA <= 15.0f);
	TestTrue(TEXT("Lower lightning potential lengthens the same deterministic interval"),
		FMath::IsNearlyEqual(LowPotential, FirstA * 2.0f));
	TestTrue(TEXT("The next strike sequence receives a new deterministic sample"),
		FirstA != FWeatherPresentationMath::CalculateLightningStrikeInterval(
			Settings, 1337, Coord, 1, 1.0f));

	const FVector2D OffsetA = FWeatherPresentationMath::CalculateLightningUnitOffset(
		Settings, 1337, Coord, 0);
	const FVector2D OffsetB = FWeatherPresentationMath::CalculateLightningUnitOffset(
		Settings, 1337, Coord, 0);
	TestTrue(TEXT("Strike positions replay exactly"), OffsetA == OffsetB);
	TestTrue(TEXT("Strike positions respect the configured cell-edge inset"),
		OffsetA.X >= 0.1f && OffsetA.X <= 0.9f && OffsetA.Y >= 0.1f && OffsetA.Y <= 0.9f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherLightningFailurePolicyTest,
	"WeatherEnvironment.Stage5.Lightning.TraceFailureAndMissingAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherLightningFailurePolicyTest::RunTest(const FString& Parameters)
{
	const FVector Fallback(1.0, 2.0, 3.0);
	const FVector Hit(4.0, 5.0, 6.0);
	FVector Resolved = FVector::ZeroVector;
	TestTrue(TEXT("An optional failed trace uses the safe fallback"),
		FWeatherPresentationMath::ResolveGroundTraceResult(
			true, false, false, Fallback, Hit, Resolved));
	TestTrue(TEXT("Optional trace failure returns the fallback location"), Resolved == Fallback);
	TestFalse(TEXT("A required failed trace rejects the strike"),
		FWeatherPresentationMath::ResolveGroundTraceResult(
			true, true, false, Fallback, Hit, Resolved));
	TestTrue(TEXT("A successful trace uses its terrain or water impact"),
		FWeatherPresentationMath::ResolveGroundTraceResult(
			true, true, true, Fallback, Hit, Resolved));
	TestTrue(TEXT("Successful trace returns the impact location"), Resolved == Hit);

	UWeatherLightningPresenter* Presenter = NewObject<UWeatherLightningPresenter>();
	FWeatherLightningPresentationSettings MissingAssetSettings;
	MissingAssetSettings.LightningSystem.Reset();
	Presenter->Configure(nullptr, MissingAssetSettings, FWeatherNiagaraParameterNames(), 1337);
	const TArray<FVector> NoViews;
	Presenter->UpdatePresentation(60.0f, NoViews);
	TestEqual(TEXT("A missing lightning asset creates no active effect"), Presenter->GetActiveEffectCount(), 0);
	TestEqual(TEXT("No relevant views schedule no distant cell timer"), Presenter->GetScheduledCellCount(), 0);
	return true;
}

#endif
