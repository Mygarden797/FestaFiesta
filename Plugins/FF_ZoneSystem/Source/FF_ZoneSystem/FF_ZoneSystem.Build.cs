// FF_ZoneSystem.Build.cs
 // FF_ZoneSystem 런타임 모듈을 컴파일할 때 필요한 Unreal 모듈 의존성을 선언한다.
 // 이 파일은 게임 규칙을 실행하지 않는다. Unreal Build Tool(UBT)이 Target을 준비할 때 읽어
 // include 경로와 링크 대상을 결정한다. Public 헤더가 아래 타입을 노출하므로 의존성도 Public으로
 // 둔다. 새 의존성을 추가할 때는 실제 Public API에서 필요한지 먼저 확인해 결합도와 빌드 시간을
 // 불필요하게 늘리지 않는다.


using UnrealBuildTool;

// Runtime 모듈의 빌드 규칙. 클래스 이름은 .uplugin의 Runtime 모듈 이름과 정확히 같아야 한다.
public class FF_ZoneSystem : ModuleRules
{
    // UBT가 Target별 규칙을 만들 때 호출한다.
    // Target에는 Editor/Game, Development/Shipping, 플랫폼 정보가 들어오지만 이 모듈은
    // 모든 Target에서 동일한 최소 의존성 집합을 사용한다.
    public FF_ZoneSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        // 각 .cpp가 자신이 사용하는 헤더를 명시하도록 해 숨은 include 의존성을 줄인다.
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Public 헤더에서 사용하는 모듈:
         // Core: FString, FGuid, TArray 등 기본 타입
         // CoreUObject: UObject, 리플렉션, 직렬화
         // Engine: Actor, Component, WorldSubsystem, DataAsset
         // DeveloperSettings: Project Settings에 노출되는 UDeveloperSettings
         // NavigationSystem: 배치 검증용 Nav projection 계약
       
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "NavigationSystem"
        });
        // Terrain collisions are sampled as ground, never rasterized as giant obstacle bounds.
        PrivateDependencyModuleNames.Add("Landscape");
    }
}
