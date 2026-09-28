// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FestaFiestaTarget : TargetRules
{
	public FestaFiestaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("FestaFiesta");

		// Shipping 배포본은 게임·플러그인 모듈을 LTCG(/GL + /LTCG)로 링크해 모듈 경계를 넘는
		// 인라이닝과 미사용 코드 제거를 얻는다. 런처 엔진의 엔진 라이브러리는 미리 빌드돼 있어
		// 적용 대상이 아니다. bAllowLTCG는 [RequiresUniqueBuildEnvironment]인
		// bDefineForceInlineHintToInline을 암묵적으로 켜므로, UnrealGame과 같은 값(false)으로
		// 고정해야 공유 빌드 환경 검사를 통과한다.
		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			bAllowLTCG = true;
			bDefineForceInlineHintToInline = false;
		}
	}
}
