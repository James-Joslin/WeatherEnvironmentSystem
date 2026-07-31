// Copyright James Joslin. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WeatherGrid.h"

namespace WeatherStageTwoTests
{
	bool BoxesTouchOrOverlap(const FBox& A, const FBox& B)
	{
		return A.Min.X <= B.Max.X && A.Max.X >= B.Min.X
			&& A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y
			&& A.Min.Z <= B.Max.Z && A.Max.Z >= B.Min.Z;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherGridCoordinateTest,
	"WeatherEnvironment.Stage2.Grid.CoordinatesAndBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherGridCoordinateTest::RunTest(const FString& Parameters)
{
	FWeatherGridDefinition Definition;
	Definition.CellSize = 100.0;
	Definition.VerticalQueryMinimum = -50.0;
	Definition.VerticalQueryMaximum = 50.0;
	Definition.MaximumCellCount = 100;

	FWeatherGrid Grid;
	FString Message;
	TestTrue(
		TEXT("Negative, non-square source bounds build"),
		Grid.Rebuild(
			FBox(FVector(-150.0, -50.0, -10.0), FVector(175.0, 175.0, 10.0)),
			Definition,
			&Message));
	TestEqual(TEXT("Snapped grid has four columns"), Grid.GetInfo().Dimensions.X, 4);
	TestEqual(TEXT("Snapped grid has three rows"), Grid.GetInfo().Dimensions.Y, 3);
	TestEqual(TEXT("Origin snaps down on negative X"), Grid.GetInfo().Origin.X, -200.0);
	TestEqual(TEXT("Origin snaps down on negative Y"), Grid.GetInfo().Origin.Y, -100.0);

	FWeatherCellCoord Coord;
	TestTrue(TEXT("Negative world position maps into the grid"), Grid.WorldToCell(FVector(-150.0, -50.0, 0.0), Coord));
	TestEqual(TEXT("Negative position X cell"), Coord.X, 0);
	TestEqual(TEXT("Negative position Y cell"), Coord.Y, 0);

	TestTrue(TEXT("Exact internal boundary is accepted"), Grid.WorldToCell(FVector(-100.0, 0.0, 0.0), Coord));
	TestEqual(TEXT("Exact X boundary selects the cell on its positive side"), Coord.X, 1);
	TestEqual(TEXT("Exact Y boundary selects the cell on its positive side"), Coord.Y, 1);

	TestTrue(
		TEXT("Expanded maximum edge is included"),
		Grid.WorldToCell(Grid.GetInfo().GridBounds.Max, Coord));
	TestEqual(TEXT("Maximum edge clamps to final column"), Coord.X, 3);
	TestEqual(TEXT("Maximum edge clamps to final row"), Coord.Y, 2);
	TestFalse(TEXT("Position beyond X bounds is rejected"), Grid.WorldToCell(FVector(201.0, 0.0, 0.0), Coord));
	TestFalse(TEXT("Position beyond vertical range is rejected"), Grid.WorldToCell(FVector(0.0, 0.0, 51.0), Coord));

	FVector Center;
	TestTrue(TEXT("Cell-to-world succeeds for a valid coordinate"), Grid.CellToWorld(FWeatherCellCoord(3, 2), Center));
	const FBox FinalCellBounds = Grid.GetCellBounds(FWeatherCellCoord(3, 2));
	TestTrue(TEXT("Partial source X edge is covered by the final cell"), FinalCellBounds.Max.X >= 175.0);
	TestTrue(TEXT("Partial source Y edge is covered by the final cell"), FinalCellBounds.Max.Y >= 175.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherGridSafetyAndPreservationTest,
	"WeatherEnvironment.Stage2.Grid.SafetyAndPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherGridSafetyAndPreservationTest::RunTest(const FString& Parameters)
{
	const FBox Bounds(FVector(0.0, 0.0, -100.0), FVector(1000.0, 1000.0, 100.0));
	FWeatherGridDefinition Definition;
	Definition.CellSize = 100.0;
	Definition.MaximumCellCount = 99;
	Definition.VerticalQueryMinimum = -100.0;
	Definition.VerticalQueryMaximum = 100.0;

	FWeatherGrid Grid;
	FString Message;
	TestFalse(TEXT("Allocation above the safety cap is rejected"), Grid.Rebuild(Bounds, Definition, &Message));
	TestTrue(TEXT("Safety-cap rejection explains the cause"), Message.Contains(TEXT("safety cap")));
	TestEqual(TEXT("Rejected rebuild allocates no cells"), Grid.GetCells().Num(), 0);

	Definition.MaximumCellCount = 100;
	TestTrue(TEXT("Grid builds at the exact safety cap"), Grid.Rebuild(Bounds, Definition, &Message));
	TestEqual(TEXT("Exact cap produces expected count"), Grid.GetInfo().CellCount, 100);

	FWeatherCellState* EditedState = Grid.FindMutableCell(FWeatherCellCoord(5, 5));
	TestNotNull(TEXT("Known cell can be edited by simulation code"), EditedState);
	if (EditedState)
	{
		EditedState->Humidity = 0.875f;
	}

	Definition.MaximumCellCount = 110;
	TestTrue(
		TEXT("Compatible expansion rebuild succeeds"),
		Grid.Rebuild(
			FBox(FVector(-100.0, 0.0, -100.0), FVector(1000.0, 1000.0, 100.0)),
			Definition,
			&Message));
	FWeatherSample Preserved = Grid.GetWeatherAtLocation(FVector(550.0, 550.0, 0.0));
	TestTrue(TEXT("Expanded grid still contains the edited world cell"), Preserved.bIsValid);
	TestTrue(TEXT("Overlapping world-cell values survive rebuild"), FMath::IsNearlyEqual(Preserved.State.Humidity, 0.875f));

	Definition.CellSize = 250.0;
	Definition.MaximumCellCount = 100;
	TestTrue(TEXT("Changing cell size deterministically rebuilds"), Grid.Rebuild(Bounds, Definition, &Message));
	TestEqual(TEXT("New cell width gives four columns"), Grid.GetInfo().Dimensions.X, 4);
	TestEqual(TEXT("New cell width gives four rows"), Grid.GetInfo().Dimensions.Y, 4);
	TestEqual(TEXT("New cell size gives sixteen cells"), Grid.GetInfo().CellCount, 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherGridPointAndBoundsQueryTest,
	"WeatherEnvironment.Stage2.Grid.PointAndBoundsQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherGridPointAndBoundsQueryTest::RunTest(const FString& Parameters)
{
	FWeatherGridDefinition Definition;
	Definition.CellSize = 100.0;
	Definition.MaximumCellCount = 100;
	Definition.VerticalQueryMinimum = -100.0;
	Definition.VerticalQueryMaximum = 100.0;
	Definition.bSnapOriginToCellSize = false;

	FWeatherGrid Grid;
	TestTrue(
		TEXT("Known query grid builds"),
		Grid.Rebuild(
			FBox(FVector(-250.0, 75.0, -10.0), FVector(170.0, 325.0, 10.0)),
			Definition));
	TestEqual(TEXT("Non-square partial grid width"), Grid.GetInfo().Dimensions.X, 5);
	TestEqual(TEXT("Non-square partial grid height"), Grid.GetInfo().Dimensions.Y, 3);

	FWeatherCellState* KnownState = Grid.FindMutableCell(FWeatherCellCoord(2, 1));
	TestNotNull(TEXT("Known query cell exists"), KnownState);
	if (KnownState)
	{
		KnownState->CloudCoverage = 0.625f;
		KnownState->WeatherType = EWeatherType::PartlyCloudy;
	}

	const FWeatherSample Sample = Grid.GetWeatherAtLocation(FVector(0.0, 200.0, 0.0));
	TestTrue(TEXT("Point query returns a valid sample"), Sample.bIsValid);
	TestEqual(TEXT("Point query maps to known X coordinate"), Sample.CellCoord.X, 2);
	TestEqual(TEXT("Point query maps to known Y coordinate"), Sample.CellCoord.Y, 1);
	TestTrue(TEXT("Point query returns known cell values"), FMath::IsNearlyEqual(Sample.State.CloudCoverage, 0.625f));

	const FBox QueryBounds(FVector(-160.0, 160.0, -5.0), FVector(55.0, 285.0, 5.0));
	TArray<FWeatherCellCoord> FastResults;
	Grid.GetCellsIntersectingBounds(QueryBounds, FastResults);

	TSet<FWeatherCellCoord> BruteForceResults;
	for (int32 Y = 0; Y < Grid.GetInfo().Dimensions.Y; ++Y)
	{
		for (int32 X = 0; X < Grid.GetInfo().Dimensions.X; ++X)
		{
			const FWeatherCellCoord Coord(X, Y);
			if (WeatherStageTwoTests::BoxesTouchOrOverlap(Grid.GetCellBounds(Coord), QueryBounds))
			{
				BruteForceResults.Add(Coord);
			}
		}
	}

	TestEqual(TEXT("Range query count matches brute force"), FastResults.Num(), BruteForceResults.Num());
	for (const FWeatherCellCoord& Coord : FastResults)
	{
		TestTrue(TEXT("Every fast range result appears in brute force"), BruteForceResults.Contains(Coord));
	}
	return true;
}

#endif
