// Copyright James Joslin. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WeatherAstronomy.h"
#include "WeatherDateTime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherDateTimeValidationTest,
	"WeatherEnvironment.Stage1.Clock.DateTimeValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherDateTimeValidationTest::RunTest(const FString& Parameters)
{
	FWeatherDateTime LeapDay;
	LeapDay.Year = 2024;
	LeapDay.Month = 2;
	LeapDay.Day = 29;
	LeapDay.Hour = 23;
	LeapDay.Minute = 59;
	LeapDay.Second = 30;

	TestTrue(TEXT("Leap day is valid"), LeapDay.IsValid());
	TestEqual(TEXT("Leap day is day 60"), LeapDay.GetDayOfYear(), 60);
	TestEqual(TEXT("Display string includes hours and minutes"), LeapDay.ToDisplayString(false), FString(TEXT("23:59")));
	TestTrue(
		TEXT("Normalized day fraction is near the end of the day"),
		LeapDay.GetNormalizedDayFraction() > 0.999);

	FWeatherDateTime InvalidDay = LeapDay;
	InvalidDay.Year = 2023;
	TestFalse(TEXT("Non-leap February 29 is invalid"), InvalidDay.IsValid());

	FDateTime Native;
	TestTrue(TEXT("Conversion to FDateTime succeeds"), LeapDay.ToDateTime(Native));
	const FWeatherDateTime RoundTrip = FWeatherDateTime::FromDateTime(Native);
	TestTrue(TEXT("Native date conversion round-trips"), RoundTrip == LeapDay);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherDateTimeRolloverTest,
	"WeatherEnvironment.Stage1.Clock.Rollover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherDateTimeRolloverTest::RunTest(const FString& Parameters)
{
	const FDateTime BeforeRollover(2024, 12, 31, 23, 59, 30);
	const FDateTime AfterRollover = BeforeRollover + FTimespan::FromSeconds(90.0);

	TestEqual(TEXT("Year rolls over"), AfterRollover.GetYear(), 2025);
	TestEqual(TEXT("Month rolls over"), AfterRollover.GetMonth(), 1);
	TestEqual(TEXT("Day rolls over"), AfterRollover.GetDay(), 1);
	TestEqual(TEXT("Hour rolls over"), AfterRollover.GetHour(), 0);
	TestEqual(TEXT("Minute advances across the boundary"), AfterRollover.GetMinute(), 1);

	const FDateTime Reverse = AfterRollover - FTimespan::FromSeconds(90.0);
	TestTrue(TEXT("Signed reverse advance returns to start"), Reverse == BeforeRollover);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherAstronomyDirectionTest,
	"WeatherEnvironment.Stage1.Astronomy.Directions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherAstronomyDirectionTest::RunTest(const FString& Parameters)
{
	FWeatherAstronomySettings Settings;
	Settings.LatitudeDegrees = 0.0;
	Settings.LongitudeDegrees = 0.0;
	Settings.UTCOffsetHours = 0.0;
	Settings.NorthYawDegrees = 0.0;

	const FWeatherCelestialState State =
		FWeatherAstronomy::Calculate(FDateTime(2026, 3, 20, 12, 0, 0), Settings);

	TestTrue(TEXT("Equatorial equinox noon sun is high"), State.SunElevationDegrees > 85.0);
	TestTrue(TEXT("Sun direction is normalized"), State.SunDirection.IsNormalized());
	TestTrue(TEXT("Moon direction is normalized"), State.MoonDirection.IsNormalized());
	TestTrue(
		TEXT("Version-one moon direction is exactly opposite the sun"),
		FMath::IsNearlyEqual(FVector::DotProduct(State.SunDirection, State.MoonDirection), -1.0, 1.0e-6));
	TestTrue(
		TEXT("Moon elevation mirrors sun elevation"),
		FMath::IsNearlyEqual(State.MoonElevationDegrees, -State.SunElevationDegrees, 1.0e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeatherAstronomyNorthYawTest,
	"WeatherEnvironment.Stage1.Astronomy.NorthYaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeatherAstronomyNorthYawTest::RunTest(const FString& Parameters)
{
	FWeatherAstronomySettings BaseSettings;
	BaseSettings.LatitudeDegrees = 35.0;
	BaseSettings.LongitudeDegrees = 0.0;
	BaseSettings.UTCOffsetHours = 0.0;

	FWeatherAstronomySettings RotatedSettings = BaseSettings;
	RotatedSettings.NorthYawDegrees = 90.0;

	const FDateTime Time(2026, 6, 21, 9, 0, 0);
	const FWeatherCelestialState Base = FWeatherAstronomy::Calculate(Time, BaseSettings);
	const FWeatherCelestialState Rotated = FWeatherAstronomy::Calculate(Time, RotatedSettings);

	const FVector Expected = FQuat(FVector::UpVector, UE_PI * 0.5).RotateVector(Base.SunDirection);
	TestTrue(
		TEXT("North yaw rotates the celestial solution around world Z"),
		Expected.Equals(Rotated.SunDirection, 1.0e-5));
	TestTrue(
		TEXT("North yaw does not alter elevation"),
		FMath::IsNearlyEqual(Base.SunElevationDegrees, Rotated.SunElevationDegrees, 1.0e-5));
	return true;
}

#endif
