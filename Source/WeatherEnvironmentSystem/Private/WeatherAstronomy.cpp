// Copyright James Joslin. All Rights Reserved.

#include "WeatherAstronomy.h"

namespace WeatherAstronomy
{
	constexpr double DegreesToRadians = UE_PI / 180.0;
	constexpr double RadiansToDegrees = 180.0 / UE_PI;

	double NormalizeDegrees(const double Degrees)
	{
		double Result = FMath::Fmod(Degrees, 360.0);
		if (Result < 0.0)
		{
			Result += 360.0;
		}
		return Result;
	}
}

FWeatherCelestialState FWeatherAstronomy::Calculate(
	const FDateTime& LocalDateTime,
	const FWeatherAstronomySettings& Settings)
{
	using namespace WeatherAstronomy;

	const int32 DaysInYear = FDateTime::IsLeapYear(LocalDateTime.GetYear()) ? 366 : 365;
	const double FractionalHour =
		static_cast<double>(LocalDateTime.GetHour())
		+ static_cast<double>(LocalDateTime.GetMinute()) / 60.0
		+ static_cast<double>(LocalDateTime.GetSecond()) / 3600.0;

	const double FractionalYear = 2.0 * UE_PI / static_cast<double>(DaysInYear)
		* (static_cast<double>(LocalDateTime.GetDayOfYear() - 1) + (FractionalHour - 12.0) / 24.0);

	const double EquationOfTimeMinutes = 229.18
		* (0.000075
			+ 0.001868 * FMath::Cos(FractionalYear)
			- 0.032077 * FMath::Sin(FractionalYear)
			- 0.014615 * FMath::Cos(2.0 * FractionalYear)
			- 0.040849 * FMath::Sin(2.0 * FractionalYear));

	const double SolarDeclination =
		0.006918
		- 0.399912 * FMath::Cos(FractionalYear)
		+ 0.070257 * FMath::Sin(FractionalYear)
		- 0.006758 * FMath::Cos(2.0 * FractionalYear)
		+ 0.000907 * FMath::Sin(2.0 * FractionalYear)
		- 0.002697 * FMath::Cos(3.0 * FractionalYear)
		+ 0.00148 * FMath::Sin(3.0 * FractionalYear);

	const double LocalMinutes = FractionalHour * 60.0;
	const double TimeOffsetMinutes = EquationOfTimeMinutes
		+ 4.0 * Settings.LongitudeDegrees
		- 60.0 * Settings.UTCOffsetHours;

	double TrueSolarMinutes = FMath::Fmod(LocalMinutes + TimeOffsetMinutes, 1440.0);
	if (TrueSolarMinutes < 0.0)
	{
		TrueSolarMinutes += 1440.0;
	}

	const double HourAngle = (TrueSolarMinutes / 4.0 - 180.0) * DegreesToRadians;
	const double Latitude = FMath::Clamp(Settings.LatitudeDegrees, -90.0, 90.0) * DegreesToRadians;

	// Local tangent frame: world +X is north, +Y east, +Z up before NorthYaw is applied.
	const double East = -FMath::Cos(SolarDeclination) * FMath::Sin(HourAngle);
	const double North =
		FMath::Sin(SolarDeclination) * FMath::Cos(Latitude)
		- FMath::Cos(SolarDeclination) * FMath::Cos(HourAngle) * FMath::Sin(Latitude);
	const double Up =
		FMath::Sin(SolarDeclination) * FMath::Sin(Latitude)
		+ FMath::Cos(SolarDeclination) * FMath::Cos(HourAngle) * FMath::Cos(Latitude);

	FVector SunDirection(North, East, Up);
	SunDirection = FQuat(FVector::UpVector, Settings.NorthYawDegrees * DegreesToRadians)
		.RotateVector(SunDirection)
		.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);

	FWeatherCelestialState Result;
	Result.SunDirection = SunDirection;
	Result.MoonDirection = -SunDirection;
	Result.SunElevationDegrees = FMath::Asin(FMath::Clamp(SunDirection.Z, -1.0, 1.0)) * RadiansToDegrees;
	Result.SunAzimuthDegrees = NormalizeDegrees(FMath::Atan2(SunDirection.Y, SunDirection.X) * RadiansToDegrees);
	Result.MoonElevationDegrees = -Result.SunElevationDegrees;
	Result.MoonAzimuthDegrees = NormalizeDegrees(Result.SunAzimuthDegrees + 180.0);
	return Result;
}
