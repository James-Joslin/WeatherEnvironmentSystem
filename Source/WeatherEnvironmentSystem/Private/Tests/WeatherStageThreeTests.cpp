// Copyright James Joslin. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WeatherGrid.h"
#include "WeatherWind.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherWindDirectionAndDeadZoneTest,
	"WeatherEnvironment.Stage3.Wind.DirectionDeadZoneAndSmoothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherWindDirectionAndDeadZoneTest::RunTest(const FString& Parameters)
{
	FWeatherWindSettings Settings;
	Settings.BaseWindSpeed = 100.0f;
	Settings.MaximumWindSpeed = 1000.0f;
	Settings.DirectorDeadZoneRadius = 10.0f;
	Settings.CurlNoiseStrength = 0.0f;
	Settings.GustStrength = 0.0f;

	const FVector East = FWeatherWindMath::ResolveBaseDirection(
		FVector::ZeroVector,
		FVector(100.0, 0.0, 0.0),
		FVector::ForwardVector,
		FVector::ZeroVector,
		Settings);
	TestTrue(TEXT("Director east of a cell produces east wind"), East.Equals(FVector::ForwardVector, KINDA_SMALL_NUMBER));

	const FVector Diagonal = FWeatherWindMath::ResolveBaseDirection(
		FVector::ZeroVector,
		FVector(100.0, 100.0, 0.0),
		FVector::ForwardVector,
		FVector::ZeroVector,
		Settings);
	TestTrue(
		TEXT("Diagonal director vector is normalized"),
		Diagonal.Equals(FVector(UE_SQRT_2 * 0.5, UE_SQRT_2 * 0.5, 0.0), 0.0001));

	const FVector Retained = FWeatherWindMath::ResolveBaseDirection(
		FVector::ZeroVector,
		FVector(5.0, 0.0, 0.0),
		-FVector::RightVector,
		FVector(0.0, 250.0, 0.0),
		Settings);
	TestTrue(TEXT("Dead zone retains previous valid direction"), Retained.Equals(FVector::RightVector, KINDA_SMALL_NUMBER));

	const FVector ForwardFallback = FWeatherWindMath::ResolveBaseDirection(
		FVector::ZeroVector,
		FVector::ZeroVector,
		-FVector::RightVector,
		FVector::ZeroVector,
		Settings);
	TestTrue(TEXT("Dead zone falls back to actor forward"), ForwardFallback.Equals(-FVector::RightVector, KINDA_SMALL_NUMBER));

	const FVector Smoothed = FWeatherWindMath::SmoothWind(
		FVector::ZeroVector,
		FVector(100.0, 0.0, 0.0),
		1.0f,
		1.0f);
	TestTrue(
		TEXT("Wind uses exponential interpolation"),
		FMath::IsNearlyEqual(Smoothed.X, 100.0 * (1.0 - FMath::Exp(-1.0)), 0.001));

	const FVector OppositeTurn = FWeatherWindMath::SmoothWind(
		FVector(100.0, 0.0, 0.0),
		FVector(-100.0, 0.0, 0.0),
		0.1f,
		2.0f);
	TestTrue(
		TEXT("An opposite turn preserves wind magnitude while rotating"),
		FMath::IsNearlyEqual(OppositeTurn.Size2D(), 100.0, 0.001));
	TestTrue(
		TEXT("An opposite turn does not remain on the old axis then flip"),
		FMath::Abs(OppositeTurn.Y) > 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherWindRouteTest,
	"WeatherEnvironment.Stage3.Wind.RoutePauseLoopAndPingPong",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherWindRouteTest::RunTest(const FString& Parameters)
{
	TArray<FWeatherWindRoutePoint> Points;
	Points.SetNum(2);
	Points[0].Location = FVector::ZeroVector;
	Points[0].TravelSpeed = 100.0f;
	Points[0].ArrivalRadius = 0.0f;
	Points[0].PauseDuration = 0.5f;
	Points[0].Easing = EWeatherWindRouteEasing::Linear;
	Points[1].Location = FVector(100.0, 0.0, 0.0);
	Points[1].TravelSpeed = 100.0f;
	Points[1].ArrivalRadius = 0.0f;
	Points[1].PauseDuration = 0.0f;
	Points[1].Easing = EWeatherWindRouteEasing::Linear;

	FWeatherWindRouteState State;
	TestTrue(TEXT("A non-empty route starts"), FWeatherWindRouteSolver::Start(FVector::ZeroVector, Points, State));
	FVector Location = FWeatherWindRouteSolver::Advance(
		0.25f,
		Points,
		EWeatherWindRouteBehavior::Once,
		State,
		FVector::ZeroVector);
	TestTrue(TEXT("Arrival pause begins at the first point"), State.bIsPaused);
	TestTrue(TEXT("Director remains at a paused point"), Location.IsNearlyZero());
	TestTrue(TEXT("Pause time is consumed exactly"), FMath::IsNearlyEqual(State.PauseRemainingSeconds, 0.25f));

	Location = FWeatherWindRouteSolver::Advance(
		0.75f,
		Points,
		EWeatherWindRouteBehavior::Once,
		State,
		Location);
	TestTrue(TEXT("Movement starts after the remaining pause"), FMath::IsNearlyEqual(Location.X, 50.0f, 0.001f));
	Location = FWeatherWindRouteSolver::Advance(
		0.5f,
		Points,
		EWeatherWindRouteBehavior::Once,
		State,
		Location);
	TestTrue(TEXT("Once route reaches its final point"), Location.Equals(Points[1].Location, 0.001f));
	TestFalse(TEXT("Once route stops at its final point"), State.bIsRunning);

	Points[0].PauseDuration = 0.0f;
	FWeatherWindRouteSolver::Start(FVector::ZeroVector, Points, State);
	Location = FWeatherWindRouteSolver::Advance(
		1.1f,
		Points,
		EWeatherWindRouteBehavior::PingPong,
		State,
		FVector::ZeroVector);
	TestTrue(TEXT("Ping pong reverses after the final point"), State.TravelDirection == -1);
	TestTrue(TEXT("Ping pong spends excess time on the reverse segment"), FMath::IsNearlyEqual(Location.X, 90.0f, 0.001f));

	FWeatherWindRouteSolver::Start(FVector::ZeroVector, Points, State);
	Location = FWeatherWindRouteSolver::Advance(
		2.1f,
		Points,
		EWeatherWindRouteBehavior::Loop,
		State,
		FVector::ZeroVector);
	TestTrue(TEXT("Loop wraps and continues toward point one"), State.TargetPointIndex == 1);
	TestTrue(TEXT("Loop consumes time across the wrap"), FMath::IsNearlyEqual(Location.X, 10.0f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherWindFieldMappingTest,
	"WeatherEnvironment.Stage3.Wind.FieldMappingAndEncoding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherWindFieldMappingTest::RunTest(const FString& Parameters)
{
	FWeatherGridDefinition Definition;
	Definition.CellSize = 100.0;
	Definition.MaximumCellCount = 10;
	Definition.VerticalQueryMinimum = -10.0;
	Definition.VerticalQueryMaximum = 10.0;
	Definition.bSnapOriginToCellSize = false;

	FWeatherGrid Grid;
	TestTrue(
		TEXT("Mapping grid builds"),
		Grid.Rebuild(FBox(FVector(0.0, 0.0, -10.0), FVector(200.0, 100.0, 10.0)), Definition));
	const FWeatherGridInfo& Info = Grid.GetInfo();
	TestTrue(
		TEXT("Minimum corner maps to UV zero"),
		FWeatherWindMath::WorldToFieldUV(Info.GridBounds.Min, Info).Equals(FVector2D::ZeroVector, KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Maximum corner maps to UV one"),
		FWeatherWindMath::WorldToFieldUV(Info.GridBounds.Max, Info).Equals(FVector2D(1.0, 1.0), KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Cell boundary maps proportionally"),
		FWeatherWindMath::WorldToFieldUV(FVector(100.0, 50.0, 0.0), Info).Equals(FVector2D(0.5, 0.5), KINDA_SMALL_NUMBER));

	const FVector SourceWind = FVector(300.0, 400.0, 0.0);
	const FColor Encoded = FWeatherWindMath::EncodeFieldTexel(SourceWind, 0.25f, 1000.0f);
	FVector DecodedWind;
	float DecodedGust = 0.0f;
	FWeatherWindMath::DecodeFieldTexel(Encoded, 1000.0f, DecodedWind, DecodedGust);
	TestTrue(TEXT("Encoded direction survives quantization"), SourceWind.GetSafeNormal().Equals(DecodedWind.GetSafeNormal(), 0.01));
	TestTrue(TEXT("Encoded speed survives quantization"), FMath::IsNearlyEqual(DecodedWind.Size(), 500.0, 5.0));
	TestTrue(TEXT("Encoded gust survives quantization"), FMath::IsNearlyEqual(DecodedGust, 0.25f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherFoliageCompatibilityTest,
	"WeatherEnvironment.Stage3.Wind.FoliageMaterialCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherFoliageCompatibilityTest::RunTest(const FString& Parameters)
{
	FWeatherWindSettings Settings;
	const FWeatherFoliageMaterialState ReferenceState = FWeatherWindMath::BuildFoliageMaterialState(
		FVector::ForwardVector,
		500.0f,
		0.0f,
		Settings.FoliageMaterials);

	TestTrue(TEXT("Reference simple wind intensity matches the legacy MPC"), FMath::IsNearlyEqual(ReferenceState.SimpleWindIntensity, 0.75f));
	TestTrue(TEXT("Reference simple wind speed matches the legacy MPC"), FMath::IsNearlyEqual(ReferenceState.SimpleWindSpeed, 0.4f));
	TestTrue(TEXT("Reference sway intensity matches the legacy MPC"), FMath::IsNearlyEqual(ReferenceState.WindSwayIntensity, 1.0f));
	TestTrue(TEXT("Reference sway frequency matches the legacy MPC"), FMath::IsNearlyEqual(ReferenceState.WindSwayGustFrequency, 0.2f));
	TestTrue(TEXT("Small gust wavelength matches the legacy MPC"), FMath::IsNearlyEqual(ReferenceState.GrassWindSmallSize, 1024.0f));
	TestTrue(TEXT("Large gust wavelength matches the legacy MPC"), FMath::IsNearlyEqual(ReferenceState.GrassWindLargeSize, 2000.0f));
	TestTrue(TEXT("Small gust amplitude matches the legacy MPC"), FMath::IsNearlyEqual(ReferenceState.GrassWindSmallAmplification, -70.0f));
	TestTrue(TEXT("Large gust amplitude matches the legacy MPC"), FMath::IsNearlyEqual(ReferenceState.GrassWindLargeAmplification, -150.0f));
	TestTrue(
		TEXT("Weather direction is converted to the legacy sway-axis convention"),
		ReferenceState.WindSwayDirection.Equals(FLinearColor(0.0f, 0.947775f, 1.0f, 0.0f), KINDA_SMALL_NUMBER));

	const FWeatherFoliageMaterialState StrongState = FWeatherWindMath::BuildFoliageMaterialState(
		FVector::RightVector,
		1000.0f,
		1.0f,
		Settings.FoliageMaterials);
	TestTrue(TEXT("Controller wind speed scales simple-wind displacement"), FMath::IsNearlyEqual(StrongState.SimpleWindIntensity, 1.5f));
	TestTrue(TEXT("Controller wind speed scales sway displacement"), FMath::IsNearlyEqual(StrongState.WindSwayIntensity, 2.0f));
	TestTrue(TEXT("Controller gust increases animation rate"), StrongState.SimpleWindSpeed > ReferenceState.SimpleWindSpeed);
	TestTrue(
		TEXT("Direction conversion remains correct after controller updates"),
		StrongState.WindSwayDirection.Equals(FLinearColor(-0.947775f, 0.0f, 1.0f, 0.0f), KINDA_SMALL_NUMBER));

	const FWeatherFoliageSpatialMaterialState Spatial =
		FWeatherWindMath::BuildFoliageSpatialMaterialState(2000.0f, Settings.FoliageMaterials);
	TestTrue(
		TEXT("Spatial field normalization converts the 500 cm/s reference speed to one"),
		FMath::IsNearlyEqual(Spatial.Mapping.R, 4.0f));
	const FVector2D ReferenceScales = FWeatherWindMath::EvaluateFoliageSpatialScales(
		0.25f,
		0.0f,
		Spatial.Mapping);
	TestTrue(TEXT("Spatial displacement matches the legacy reference scale"), FMath::IsNearlyEqual(ReferenceScales.X, 1.0));
	TestTrue(TEXT("Spatial animation matches the legacy reference scale"), FMath::IsNearlyEqual(ReferenceScales.Y, 1.0));
	const FVector2D GustScales = FWeatherWindMath::EvaluateFoliageSpatialScales(
		0.25f,
		1.0f,
		Spatial.Mapping);
	TestTrue(
		TEXT("Spatial legacy animation rate remains phase-stable as gust changes"),
		FMath::IsNearlyEqual(GustScales.Y, 1.0));
	return true;
}

#endif
