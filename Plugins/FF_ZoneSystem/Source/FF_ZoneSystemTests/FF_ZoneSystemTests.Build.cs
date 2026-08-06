// FF_ZoneSystemTests.Build.cs

// Editor 전용 FF_ZoneSystem 자동화 테스트 모듈의 컴파일 의존성을 선언한다.
// 테스트 모듈은 Runtime 플러그인의 Public API를 소비하지만 게임 패키지의 Public API는 아니다.
// 따라서 의존성을 Private으로 두어 테스트 구현 세부사항이 다른 모듈로 전파되지 않게 한다.
// .uplugin에서 Type=Editor로 제한되므로 Shipping 게임 코드에 포함되어서는 안 된다.


using UnrealBuildTool;

// Editor 테스트 모듈의 빌드 규칙. 이름은 .uplugin의 FF_ZoneSystemTests 모듈과 일치해야 한다.
public class FF_ZoneSystemTests : ModuleRules
{
    // UBT가 Editor Target용 테스트 모듈 규칙을 구성할 때 호출한다. 
    public FF_ZoneSystemTests(ReadOnlyTargetRules Target) : base(Target)
    {
        // 테스트 .cpp도 실제 include를 명시해 우연한 PCH 의존을 방지한다.
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Private 의존성:
         // Core/CoreUObject/Engine: 테스트 데이터, UObject, World 구성
         // UnrealEd: Editor Automation 환경과 Editor 전용 테스트 지원
         //FF_ZoneSystem: 검증 대상 Runtime Public API
        
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "FF_ZoneSystem"
        });
    }
}
