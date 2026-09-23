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
			"CommonUI","CommonInput",
			"DeveloperSettings",
			"Niagara","NiagaraCore",
			"FF_ZoneSystem"
		});

		// OpenLevel의 블로킹 구간에서도 로딩 화면을 유지한다. 런타임 cpp에서만 사용한다.
		PrivateDependencyModuleNames.Add("MoviePlayer");
		// The opt-in packaged startup probe records the RHI that actually initialized.
		PrivateDependencyModuleNames.Add("RHI");
		
		// The WBP builder is compiled only into the Editor target. Runtime/Game
		// packaging remains free of UnrealEd and UMGEditor dependencies.
		if (Target.bBuildEditor)
		{
			// BlueprintGraph: 생성기가 복사본에 남은 레거시 K2 노드(OpenLevel)를 찾아 지운다.
			// UnrealEd가 public으로 끌고 오지만, 직접 include하므로 명시적으로 적는다.
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				// Input integration tests host a lightweight FViewport (FRenderResource).
				"AssetTools", "UnrealEd", "UMGEditor", "BlueprintGraph", "RenderCore", "Json", "AssetRegistry"
			});
		}
	}
}
