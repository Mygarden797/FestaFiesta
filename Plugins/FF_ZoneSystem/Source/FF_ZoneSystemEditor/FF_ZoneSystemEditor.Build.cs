// FF_ZoneSystemEditor.Build.cs
// 역할: 에디터 전용 UI·에셋 검사 모듈의 빌드 의존성을 선언한다. 게임 패키지에는 포함하지 않는다.
using UnrealBuildTool;

public class FF_ZoneSystemEditor : ModuleRules
{
    public FF_ZoneSystemEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NavigationSystem",
            "InputCore",
            "Slate",
            "SlateCore",
            "UnrealEd",
            "PropertyEditor",
            "AssetRegistry",
            "DataValidation",
            "FF_ZoneSystem"
        });
    }
}
