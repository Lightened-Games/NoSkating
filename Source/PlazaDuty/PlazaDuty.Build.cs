// Copyright Plaza Duty. All Rights Reserved.

using UnrealBuildTool;

public class PlazaDuty : ModuleRules
{
	public PlazaDuty(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"Niagara",
			"AIModule",
			"NavigationSystem",
			"EngineCameras",
			"Json"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "AudioExtensions", "DeveloperSettings", "AudioMixer", "AnimationCore" });

		// Editor-only helpers used by the content scripts (volume brushes, nav data).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.Add("PlazaDuty");
	}
}
