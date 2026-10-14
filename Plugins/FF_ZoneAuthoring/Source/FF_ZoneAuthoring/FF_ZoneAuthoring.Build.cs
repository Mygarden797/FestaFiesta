// FF_ZoneAuthoring.Build.cs
// 역할: Zone 저작 계층(런타임) 모듈의 의존성을 선언한다.
// 이 모듈은 FF_ZoneSystem의 공개 API만 사용하며 FF_ZoneSystem 소스를 수정하지 않는다.
using UnrealBuildTool;

public class FF_ZoneAuthoring : ModuleRules
{
    public FF_ZoneAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Public 헤더가 노출하는 타입:
        //  - FF_ZoneSystem: AFFZSGameplayZone(부모 클래스), EFFZSFacilityCategory, Policy 인터페이스
        //  - ProceduralMeshComponent: 바닥 오버레이 메시
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "FF_ZoneSystem",
            "ProceduralMeshComponent"
        });

        // AssetRegistry: Project Zone Ids 동기화 시 Zone Definition 에셋 목록 조회(에디터 버튼에서만 호출)
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "GeometryCore",
            "GeometryAlgorithms"
        });

        // 에디터 빌드에서만: Undo(FScopedTransaction), 액터 선택(GEditor)
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd"
            });
        }
    }
}
