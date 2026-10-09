// FF_ZoneAuthoringEditor.Build.cs
// 역할: Zone 저작 계층의 에디터 전용 기능(디테일 패널 잠금, 에셋 검증, 자동화 테스트) 의존성을 선언한다.
// 게임 패키지에는 포함되지 않는다.
using UnrealBuildTool;

public class FF_ZoneAuthoringEditor : ModuleRules
{
    public FF_ZoneAuthoringEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "UnrealEd",
            "PropertyEditor",
            "AssetRegistry",
            "DataValidation",
            "FF_ZoneSystem",
            "FF_ZoneAuthoring"
        });
    }
}
