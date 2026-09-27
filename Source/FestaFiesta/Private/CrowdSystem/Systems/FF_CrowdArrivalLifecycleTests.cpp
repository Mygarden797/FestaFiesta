// Runtime-loaded arrival and appearance tables must survive GC for the whole festival.
#include "Misc/AutomationTest.h"

// 역할: 관객 도착 시간표와 외형 데이터가 메모리 정리 후에도 유지되는지 검증한다.
// 원본 에셋의 임시 사본으로 검사해 에디터가 원본을 보관하는 영향은 배제한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Tests/AutomationCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

namespace FFCrowdArrivalLifecycleTest
{
// Use copies of the actual project data so an editor tab or another test cannot
// accidentally keep the test's tables alive. Settings retain only soft paths;
// the initialized simulation must provide the runtime ownership itself.
struct FScopedTableSettings
{
    UFF_SimulationSettings* Settings = GetMutableDefault<UFF_SimulationSettings>();
    TSoftObjectPtr<UDataTable> OriginalArrival = Settings->ArrivalCurveTable;
    TSoftObjectPtr<UDataTable> OriginalAppearance = Settings->CrowdAppearanceTable;
    TWeakObjectPtr<UDataTable> Arrival;
    TWeakObjectPtr<UDataTable> Appearance;

    ~FScopedTableSettings()
    {
        Settings->ArrivalCurveTable = OriginalArrival;
        Settings->CrowdAppearanceTable = OriginalAppearance;
    }

    bool Install(FAutomationTestBase& Test)
    {
        UDataTable* ArrivalSource = OriginalArrival.LoadSynchronous();
        UDataTable* AppearanceSource = OriginalAppearance.LoadSynchronous();
        if (!Test.TestNotNull(TEXT("Project arrival table resolves"), ArrivalSource)
            || !Test.TestNotNull(TEXT("Project appearance table resolves"), AppearanceSource))
        {
            return false;
        }

        Arrival = DuplicateObject<UDataTable>(ArrivalSource, GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), TEXT("FFArrivalGcTest")));
        Appearance = DuplicateObject<UDataTable>(AppearanceSource, GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), TEXT("FFAppearanceGcTest")));
        if (!Test.TestTrue(TEXT("Independent transient table copies exist"),
            Arrival.IsValid() && Appearance.IsValid()))
        {
            return false;
        }

        for (UDataTable* Table : { Arrival.Get(), Appearance.Get() })
        {
            Table->ClearFlags(RF_Standalone | RF_Public);
            Table->SetFlags(RF_Transient);
            Test.TestFalse(TEXT("Transient table is not rooted by the fixture"), Table->IsRooted());
        }
        Settings->ArrivalCurveTable = Arrival.Get();
        Settings->CrowdAppearanceTable = Appearance.Get();
        return true;
    }
};

int32 CountVisibleActors(const UFF_CrowdSimulationSubsystem& Crowd)
{
    int32 Count = 0;
    for (const AFF_BaseCrowd* Actor : Crowd.CrowdPool)
    {
        if (IsValid(Actor) && !Actor->IsHidden())
        {
            ++Count;
        }
    }
    return Count;
}

void ChangeStage(UFF_FestivalStageSubsystem& Stage, EFF_FestivalStage Next)
{
    const EFF_FestivalStage Previous = Stage.CurrentStage;
    Stage.CurrentStage = Next;
    Stage.OnStageChanged.Broadcast(Previous, Next);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdArrivalTablesSurviveFestivalGarbageCollection,
    "FestaFiesta.Crowd.Arrival.RuntimeTablesSurviveFestivalGarbageCollection",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_CrowdArrivalTablesSurviveFestivalGarbageCollection::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace FFCrowdArrivalLifecycleTest;
    constexpr int32 RequestedVisitors = 60;
    constexpr int32 PoolSize = 64;
    constexpr int32 MaximumPartySize = 6;

    // Declare settings before the world: world shutdown completes before the
    // original global settings are restored. No assets or config are saved.
    FScopedTableSettings TableSettings;
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    UFF_FestivalStageSubsystem* Stage = World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
    if (!TestNotNull(TEXT("Festival clock exists"), Stage))
    {
        return false;
    }
    Stage->CurrentStage = EFF_FestivalStage::Setup;
    Stage->ElapsedLiveSeconds = 0.f;
    if (!Fixture.BeginPlayInTestWorld())
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    // Install only after world setup, so an unrelated initialization collection
    // cannot collect the copies before the simulation has a chance to own them.
    if (!TableSettings.Install(*this))
    {
        return false;
    }
    UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
    if (!TestNotNull(TEXT("Crowd simulation exists"), Crowd))
    {
        return false;
    }
    Crowd->InitializeSimulationWithBounds(FVector::ZeroVector, FBox::BuildAABB(FVector::ZeroVector, FVector(10000.f, 10000.f, 10000.f)), PoolSize, PoolSize, AFF_BaseCrowd::StaticClass());
    Crowd->BeginArrivalSchedule(RequestedVisitors, FVector::ZeroVector, 500.f);
    // The production economy bridge normally supplies the festival's expected visitor count
    // when Live begins. Keep this isolated GC test within its 64-slot fixture on both days.
    Crowd->SetExpectedVisitorCountOverride(RequestedVisitors);
    TestTrue(TEXT("Project arrival curve was loaded during initialization"), Crowd->IsArrivalCurveActive());
    TestTrue(TEXT("The real visitor distribution contains whole parties"), Crowd->GetArrivalPlannedCount() > 0);
    TestEqual(TEXT("Setup starts with no active visitors"), Crowd->GetActiveAgentCount(), 0);

    const TWeakObjectPtr<UWorld> WeakWorld(World);
    const TWeakObjectPtr<UFF_CrowdSimulationSubsystem> WeakCrowd(Crowd);
    auto CollectAndCheck = [&](const TCHAR* Phase)
    {
        // This unowned object must die, proving this is an effective collection
        // rather than a test that passes because nothing was actually collected.
        const TWeakObjectPtr<UDataTable> Control(NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient));
        CollectGarbage(RF_NoFlags, true);
        TestFalse(*FString::Printf(TEXT("%s: unowned control is collected"), Phase), Control.IsValid());
        const bool bWorldAlive = TestTrue(*FString::Printf(TEXT("%s: fixture and simulation survive"), Phase),
            WeakWorld.IsValid() && WeakCrowd.IsValid());
        TestTrue(*FString::Printf(TEXT("%s: simulation retains its loaded arrival table"), Phase), TableSettings.Arrival.IsValid());
        TestTrue(*FString::Printf(TEXT("%s: simulation retains its loaded appearance table"), Phase), TableSettings.Appearance.IsValid());
        return bWorldAlive;
    };

    auto AdvanceArrival = [&](float Progress, const TCHAR* Context)
    {
        const float Duration = Stage->GetLiveDurationForCurrentDay();
        if (!TestTrue(TEXT("Live duration is positive"), Duration > 0.f))
        {
            return 0;
        }
        Stage->ElapsedLiveSeconds = Duration * Progress;
        Crowd->UpdateArrivalSchedule();
        const int32 Active = Crowd->GetActiveAgentCount();
        TestEqual(*FString::Printf(TEXT("%s: arrival ledger matches active visitors"), Context), Crowd->GetArrivalSpawnedCount(), Active);
        TestEqual(*FString::Printf(TEXT("%s: every active visitor has a bound actor"), Context), Crowd->GetBoundActorCount(), Active);
        TestEqual(*FString::Printf(TEXT("%s: every active visitor is revealed"), Context), CountVisibleActors(*Crowd), Active);
        AddInfo(FString::Printf(TEXT("%s: progress=%.2f planned=%d active=%d bound=%d visible=%d arrivalAlive=%d appearanceAlive=%d"),
            Context, Progress, Crowd->GetArrivalPlannedCount(), Active, Crowd->GetBoundActorCount(), CountVisibleActors(*Crowd),
            TableSettings.Arrival.IsValid(), TableSettings.Appearance.IsValid()));
        return Active;
    };

    if (!CollectAndCheck(TEXT("Setup before opening")))
    {
        return false;
    }
    ChangeStage(*Stage, EFF_FestivalStage::Live);
    const int32 EarlyCount = AdvanceArrival(0.25f, TEXT("First day early arrival"));
    TestTrue(TEXT("Visitors appear after Setup GC when Live advances"), EarlyCount > 0);
    TestTrue(TEXT("The arrival curve does not spawn the whole day immediately"), EarlyCount < RequestedVisitors - MaximumPartySize);

    if (!CollectAndCheck(TEXT("During Live")))
    {
        return false;
    }
    const int32 LaterCount = AdvanceArrival(0.65f, TEXT("First day later arrival"));
    TestTrue(TEXT("Arrivals continue after another GC during Live"), LaterCount > EarlyCount);

    ChangeStage(*Stage, EFF_FestivalStage::Report);
    TestEqual(TEXT("Report clears the previous day's agents"), Crowd->GetActiveAgentCount(), 0);
    TestEqual(TEXT("Report releases the visible actors"), CountVisibleActors(*Crowd), 0);
    ++Stage->CurrentDay;
    Stage->ElapsedLiveSeconds = 0.f;
    ChangeStage(*Stage, EFF_FestivalStage::Setup);
    if (!CollectAndCheck(TEXT("Setup for the next day")))
    {
        return false;
    }
    ChangeStage(*Stage, EFF_FestivalStage::Live);
    const int32 NextDayCount = AdvanceArrival(1.f, TEXT("Next day complete arrival"));
    TestEqual(TEXT("Next day reaches the exact planned whole-party population"), NextDayCount, Crowd->GetArrivalPlannedCount());
    TestTrue(TEXT("Next day keeps the requested whole-party population after GC"),
        NextDayCount > RequestedVisitors - MaximumPartySize && NextDayCount <= RequestedVisitors);
    return true;
}

#endif
