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

void UWeatherEnvironmentProfile::PostLoad()
{
	Super::PostLoad();
	if (DataVersion < 4)
	{
		// Stage 4 adds only simulation settings. Older assets never serialized this
		// property, so explicitly materialize the complete version-four defaults.
		Simulation = FWeatherSimulationSettings();
		DataVersion = 4;
	}

	if (DataVersion < 5)
	{
		// Version five makes procedural fronts scale with the grid instead of an
		// absolute open-world assumption. Preserve explicitly authored seeds and
		// counts, but make an untouched Stage 4 profile immediately demonstrable.
		Simulation.bUseCellRelativeGeneratedSigma = true;
		if (Simulation.GeneratedSigmaCellRange.X <= 0.0
			|| Simulation.GeneratedSigmaCellRange.Y <= 0.0)
		{
			Simulation.GeneratedSigmaCellRange = FVector2D(0.35, 0.65);
		}
		if (Simulation.InitialSeeds.IsEmpty()
			&& Simulation.InitialGeneratedSeedCount == 0)
		{
			Simulation.InitialGeneratedSeedCount = 4;
		}
		DataVersion = 5;
	}

	if (DataVersion < 6)
	{
		// Version six replaces the demonstration-oriented equal preset cycle with
		// an area-scaled, weighted lifecycle. Preserve any explicit designer seed
		// population; only retire the untouched four-generated-front default.
		Simulation.FrontLifecycle = FWeatherFrontLifecycleSettings();
		if (Simulation.InitialSeeds.IsEmpty()
			&& Simulation.InitialGeneratedSeedCount == 4)
		{
			Simulation.InitialGeneratedSeedCount = 0;
		}
		DataVersion = 6;
	}
}
