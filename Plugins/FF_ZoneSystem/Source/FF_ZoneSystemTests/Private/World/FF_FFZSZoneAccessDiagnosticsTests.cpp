// FF_FFZSZoneAccessDiagnosticsTests.cpp
// ZoneAccess 진단은 존이 없는 Frontend와 NavMesh가 누락된 게임플레이 맵을 구분해야 한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "FF_FFZSZoneAccessSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "NavigationSystem.h"
#include "Tests/AutomationCommon.h"

namespace
{
    class FScopedZoneAccessDiagnostics : public FOutputDevice
    {
    public:
        FScopedZoneAccessDiagnostics()
        {
            GLog->AddOutputDevice(this);
        }

        virtual ~FScopedZoneAccessDiagnostics() override
        {
            GLog->RemoveOutputDevice(this);
        }

        virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity,
            const FName& Category) override
        {
            if (Category == FName(TEXT("LogFFZSZoneAccess")) && Verbosity <= ELogVerbosity::Warning)
            {
                ++WarningOrErrorCount;
            }
        }

        int32 WarningOrErrorCount = 0;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSZoneAccessNoZonesTest,
    "FF_ZoneSystem.Integration.ZoneAccess.NoZonesDoNotRequireNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSZoneAccessNoZonesTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper TestWorld;
    if (!TestWorld.CreateTestWorld(EWorldType::Game))
    {
        TestWorld.ForwardErrorMessages(this);
        return false;
    }

    UWorld* World = TestWorld.GetTestWorld();
    UFFZSZoneAccessSubsystem* Access = World->GetSubsystem<UFFZSZoneAccessSubsystem>();
    if (!TestNotNull(TEXT("Game 월드에는 ZoneAccess가 있다"), Access))
    {
        return false;
    }
    TestNull(TEXT("빈 테스트 맵에는 NavigationSystem이 없다"),
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World));

    FScopedZoneAccessDiagnostics Diagnostics;
    TestEqual(TEXT("게임플레이 존이 없으면 고칠 앵커도 없다"), Access->RepairZoneAccessPoints(), 0);
    Access->LogReachabilityReport();
    TestEqual(TEXT("Frontend처럼 존이 없는 맵에 NavMesh나 출구 누락 경고를 남기지 않는다"),
        Diagnostics.WarningOrErrorCount, 0);
    TestTrue(TEXT("빈 월드에 복구 결과를 만들지 않는다"), Access->GetRepairs().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSZoneAccessMissingNavigationTest,
    "FF_ZoneSystem.Integration.ZoneAccess.MissingNavigationPreservesAuthoredAnchors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSZoneAccessMissingNavigationTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper TestWorld;
    if (!TestWorld.CreateTestWorld(EWorldType::Game))
    {
        TestWorld.ForwardErrorMessages(this);
        return false;
    }

    UWorld* World = TestWorld.GetTestWorld();
    UFFZSZoneAccessSubsystem* Access = World->GetSubsystem<UFFZSZoneAccessSubsystem>();
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("ZoneAccess가 있다"), Access)
        || !TestNotNull(TEXT("게임플레이 존을 만든다"), Zone))
    {
        return false;
    }

    Zone->ZoneId = TEXT("GZ_ZoneAccessDiagnostics");
    const FTransform AuthoredAnchor(FVector(125.0, 250.0, 0.0));
    Zone->EntryExitAnchors.Add(AuthoredAnchor);

    AddExpectedMessagePlain(TEXT("NavMesh가 없어 접근점을 고칠 수 없다"), ELogVerbosity::Warning);
    AddExpectedMessagePlain(TEXT("NavMesh가 준비되지 않아 도달 리포트를 만들 수 없다"), ELogVerbosity::Warning);

    TestEqual(TEXT("NavMesh가 없는 게임플레이 맵은 복구 성공으로 처리하지 않는다"),
        Access->RepairZoneAccessPoints(), 0);
    Access->LogReachabilityReport();
    TestEqual(TEXT("저작 앵커 수를 보존한다"), Zone->EntryExitAnchors.Num(), 1);
    if (Zone->EntryExitAnchors.Num() == 1)
    {
        TestTrue(TEXT("저작 앵커 위치·회전·스케일을 보존한다"),
            Zone->EntryExitAnchors[0].Equals(AuthoredAnchor));
    }
    TestTrue(TEXT("NavMesh 누락 상태에서 복구 결과를 만들지 않는다"), Access->GetRepairs().IsEmpty());
    return true;
}

#endif
