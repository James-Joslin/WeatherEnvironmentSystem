// Copyright James Joslin. All Rights Reserved.

#include "WeatherEnvironmentProfile.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

FWeatherSkyboxSettings::FWeatherSkyboxSettings()
{
	Layers.SetNum(4);
}

UWeatherEnvironmentProfile::UWeatherEnvironmentProfile()
{
	Skybox.Layers.SetNum(4);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMoonMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DefaultMoonMesh.Succeeded())
	{
		MoonVisual.MoonMesh = DefaultMoonMesh.Object;
	}

}
