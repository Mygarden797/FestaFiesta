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
		// 패키지 시작 진단에서 실제로 선택된 그래픽 API를 기록한다.
		PrivateDependencyModuleNames.Add("RHI");
		// 타이틀 인트로 영상(MediaPlayer·FileMediaSource·MediaTexture). 재생은 기본 활성 WmfMedia 가 맡는다.
		PrivateDependencyModuleNames.Add("MediaAssets");
		
		// 제작 도구는 FestaFiestaEditor에 있다. 여기에는 기존 회귀 테스트와
		// 런타임 타입의 에디터 전용 검증 함수가 직접 사용하는 의존성만 둔다.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				// 입력 테스트는 가벼운 FViewport를, 콘텐츠 테스트는 에셋 목록을 사용한다.
				"AssetTools", "UnrealEd", "UMGEditor", "RenderCore", "AssetRegistry",
				// PIE 중앙 배치 진단의 Slate 좌표를 JSON으로 기록한다.
				"Json"
			});
		}
	}
}
