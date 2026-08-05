// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FestaFiesta : ModuleRules
{
	public FestaFiesta(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"NavigationSystem",
			"GameplayAbilities", "GameplayTags", "GameplayTasks",
			"AIModule",
			"UMG","Slate","SlateCore",
			"Niagara","NiagaraCore",
			"FF_ZoneSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"FF_ZoneSystem"
		});

		// The WBP builder is compiled only into the Editor target. Runtime/Game
		// packaging remains free of UnrealEd and UMGEditor dependencies.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AssetTools", "UnrealEd", "UMGEditor"
			});
		}
	}
}
