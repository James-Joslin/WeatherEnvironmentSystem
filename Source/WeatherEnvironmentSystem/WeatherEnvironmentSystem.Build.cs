// Copyright James Joslin. All Rights Reserved.

using UnrealBuildTool;

public class WeatherEnvironmentSystem : ModuleRules
{
	public WeatherEnvironmentSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"RenderCore"
		});
	}
}
