// Copyright James Joslin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeatherEnvironmentProfile.h"

struct WEATHERENVIRONMENTSYSTEM_API FWeatherCelestialState
{
	FVector SunDirection = FVector::UpVector;
	FVector MoonDirection = FVector::DownVector;
	double SunElevationDegrees = 90.0;
	double SunAzimuthDegrees = 0.0;
	double MoonElevationDegrees = -90.0;
	double MoonAzimuthDegrees = 180.0;
};

class WEATHERENVIRONMENTSYSTEM_API FWeatherAstronomy
{
public:
	static FWeatherCelestialState Calculate(
		const FDateTime& LocalDateTime,
		const FWeatherAstronomySettings& Settings);
};
