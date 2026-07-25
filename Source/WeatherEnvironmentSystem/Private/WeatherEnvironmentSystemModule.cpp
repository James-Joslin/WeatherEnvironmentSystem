// Copyright James Joslin. All Rights Reserved.

#include "WeatherEnvironmentSystemModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

void FWeatherEnvironmentSystemModule::StartupModule()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("WeatherEnvironmentSystem"));
	if (ensureMsgf(Plugin.IsValid(), TEXT("WeatherEnvironmentSystem plugin descriptor could not be found.")))
	{
		const FString ShaderDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/WeatherEnvironmentSystem"), ShaderDirectory);
	}
}

void FWeatherEnvironmentSystemModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FWeatherEnvironmentSystemModule, WeatherEnvironmentSystem)
