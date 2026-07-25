// Copyright James Joslin. All Rights Reserved.

using UnrealBuildTool;

public class WeatherEnvironmentSystemEditor : ModuleRules
{
	public WeatherEnvironmentSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"WeatherEnvironmentSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"CoreUObject",
			"Engine",
			"MaterialEditor",
			"Projects",
			"UnrealEd"
		});
	}
}
