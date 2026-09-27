// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FestaFiestaEditorTarget : TargetRules
{
	public FestaFiestaEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		// 대규모 이동·리팩토링 중에도 변경 파일을 묶어 컴파일해 개별 작업 증가를 막는다.
		// 한 파일 수정 시 같은 묶음은 다시 컴파일되지만, 게임용 Target 설정에는 영향이 없다.
		bUseAdaptiveUnityBuild = false;
		ExtraModuleNames.Add("FestaFiesta");
		// 제작 코드 변경 시 게임 DLL 전체를 다시 링크하지 않도록 별도 모듈로 빌드한다.
		ExtraModuleNames.Add("FestaFiestaEditor");
	}
}
