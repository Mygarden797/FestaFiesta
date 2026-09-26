// 역할: 화면 생성기와 에셋 정비 도구를 에디터에서만 빌드한다.
using UnrealBuildTool;

public class FestaFiestaEditor : ModuleRules
{
    public FestaFiestaEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 게임의 공개 API만 사용한다. 게임 모듈은 이 제작 모듈에 의존하지 않는다.
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "FestaFiesta", "FF_ZoneSystem",
            "InputCore", "EnhancedInput", "GameplayTags", "DeveloperSettings",
            "UMG", "Slate", "SlateCore", "CommonUI", "CommonInput",
            "UnrealEd", "UMGEditor", "BlueprintGraph", "AssetTools", "AssetRegistry",
            "Json", "RenderCore", "RHI"
        });
    }
}
