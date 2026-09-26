// The authored release map must keep spawning visible NPCs after garbage
// collection, PIE restarts, and the next day's arrival schedule.
#include "Misc/AutomationTest.h"

// 역할: 실제 배포 맵에서 메모리 정리, 플레이 재시작, 날짜 변경 후에도 관객이 생성되는지 검증한다.
// 이전 플레이 월드의 정리가 끝난 뒤 다음 검사를 시작해 세션 간 데이터가 섞이지 않게 한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Components/SkeletalMeshComponent.h"
#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UObject/GarbageCollection.h"
#include "UObject/ObjectKey.h"

namespace
{
struct FFFProductionCrowdPIEState
{
    // Weak identities must not keep an ended world alive during GC/teardown.
    TWeakObjectPtr<UWorld> PreviousWorld;
    TSet<FObjectKey> SeenWorlds;
    bool bAbortFurtherSessions = false;
};

bool HasProductionCrowdPIESession()
{
    if (GEditor && GEditor->PlayWorld) return true;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        // A context whose World has already been cleared still means teardown
        // has not finished. Wait for removal of the context itself as well.
        if (Context.WorldType == EWorldType::PIE) return true;
    }
    return false;
}

class FFFStartProductionCrowdPIE final : public IAutomationLatentCommand
{
public:
    FFFStartProductionCrowdPIE(FAutomationTestBase* InTest,
        TSharedRef<FFFProductionCrowdPIEState> InState)
        : Test(InTest), State(InState) {}

    virtual bool Update() override
    {
        if (State->bAbortFurtherSessions) return true;
        if (HasProductionCrowdPIESession())
        {
            Test->AddError(TEXT("A previous PIE session remains before the next crowd session can start."));
            State->bAbortFurtherSessions = true;
            return true;
        }
        FStartPIECommand Start(false);
        return Start.Update();
    }

private:
    FAutomationTestBase* Test;
    TSharedRef<FFFProductionCrowdPIEState> State;
};

class FFFWaitForProductionCrowdPIEEnd final : public IAutomationLatentCommand
{
public:
    FFFWaitForProductionCrowdPIEEnd(FAutomationTestBase* InTest, const int32 InSession,
        TSharedRef<FFFProductionCrowdPIEState> InState)
        : Test(InTest), Session(InSession), State(InState) {}

    virtual bool Update() override
    {
        if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
        if (HasProductionCrowdPIESession())
        {
            if (FPlatformTime::Seconds() - StartedAt < 60.0) return false;
            Test->AddError(FString::Printf(TEXT("PIE session %d teardown timed out: PlayWorld or a PIE world context remains."), Session));
            State->bAbortFurtherSessions = true;
            return true;
        }
        if (!State->bAbortFurtherSessions)
        {
            Test->TestEqual(FString::Printf(TEXT("PIE session %d completed with a distinct world identity"), Session),
                State->SeenWorlds.Num(), Session);
        }
        Test->AddInfo(FString::Printf(TEXT("PIE session %d teardown completed: no PlayWorld and no PIE world contexts."), Session));
        return true;
    }

private:
    FAutomationTestBase* Test;
    int32 Session;
    TSharedRef<FFFProductionCrowdPIEState> State;
    double StartedAt = -1.0;
};

class FFFCheckProductionCrowdSpawning final : public IAutomationLatentCommand
{
public:
    FFFCheckProductionCrowdSpawning(FAutomationTestBase* InTest, const int32 InSession,
        TSharedRef<FFFProductionCrowdPIEState> InState)
        : Test(InTest), Session(InSession), State(InState) {}

    virtual bool Update() override
    {
        if (State->bAbortFurtherSessions) return true;
        // These commands are queued together. Start each timeout only when its
        // own PIE session is reached, not when all three commands are created.
        if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World())
            {
                World = Context.World();
                break;
            }
        }
        UFF_CrowdSimulationSubsystem* Crowd = World
            ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
        UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
        UFF_FestivalStageSubsystem* Stage = Game
            ? Game->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
        if (!World || !World->HasBegunPlay() || !Crowd || !Crowd->IsSimulationInitialized() || !Stage)
        {
            if (FPlatformTime::Seconds() - StartedAt < 60.0) return false;
            Test->AddError(Label(TEXT("Test_Main PIE crowd did not initialize within 60 seconds")));
            State->bAbortFurtherSessions = true;
            return true;
        }

        const FObjectKey WorldIdentity(World);
        if (!Test->TestFalse(Label(TEXT("This is a new PIE world, not the previous session")),
                State->PreviousWorld == World || State->SeenWorlds.Contains(WorldIdentity)))
        {
            State->bAbortFurtherSessions = true;
            return true;
        }
        State->PreviousWorld = World;
        State->SeenWorlds.Add(WorldIdentity);

        // Test only the crowd's production stage subscriber and real authored
        // assets. Required-facility/operating-stage authorization has independent
        // coverage; directly seed stage fields and broadcast here so that gate
        // cannot prevent exercising the spawn lifecycle. Never save this map.
        // Everything below is synchronous, so normal world ticks cannot advance
        // the clock or consume visitors between checkpoints. PIE teardown owns
        // the seeded state, including this test-only frozen festival clock.
        Stage->bFestivalStarted = false;
        Stage->CurrentDay = 1;
        BroadcastStage(*Stage, EFF_FestivalStage::Setup);
        Test->TestTrue(Label(TEXT("The authored arrival curve is configured")), Crowd->IsArrivalCurveActive());
        Test->TestTrue(Label(TEXT("The authored visitor plan is nonempty")), Crowd->GetArrivalPlannedCount() > 0);
        Test->TestEqual(Label(TEXT("Setup has no active visitors")), Crowd->GetActiveAgentCount(), 0);
        CollectGarbage(RF_NoFlags, true);
        Test->TestNotNull(Label(TEXT("The initialized arrival table survives Setup GC without reloading")),
            Crowd->GetLoadedArrivalCurveTable());

        BroadcastStage(*Stage, EFF_FestivalStage::Live);
        Test->TestEqual(Label(TEXT("The Live subscriber starts a fresh arrival cursor")), Crowd->GetArrivalSpawnedCount(), 0);
        const float DayOneDuration = Stage->GetLiveDurationForCurrentDay();
        if (!Test->TestTrue(Label(TEXT("The real Day 1 duration is finite and positive")),
                FMath::IsFinite(DayOneDuration) && DayOneDuration > 0.f))
        {
            return true;
        }
        Stage->ElapsedLiveSeconds = DayOneDuration * 0.25f;
        Crowd->UpdateArrivalSchedule();
        CheckSpawned(*World, *Crowd, TEXT("Day 1 at 25%"));
        const int32 FirstCursor = Crowd->GetArrivalSpawnedCount();

        CollectGarbage(RF_NoFlags, true);
        Test->TestNotNull(Label(TEXT("The arrival table survives GC between arrival buckets")),
            Crowd->GetLoadedArrivalCurveTable());
        Stage->ElapsedLiveSeconds = DayOneDuration * 0.65f;
        Crowd->UpdateArrivalSchedule();
        CheckSpawned(*World, *Crowd, TEXT("Day 1 at 65%"));
        Test->TestTrue(Label(TEXT("A later real arrival bucket advances the cursor")),
            Crowd->GetArrivalSpawnedCount() > FirstCursor);
        CollectGarbage(RF_NoFlags, true);
        CheckSpawned(*World, *Crowd, TEXT("Day 1 after visible-actor GC"));

        BroadcastStage(*Stage, EFF_FestivalStage::Report);
        CheckDespawned(*World, *Crowd, TEXT("Day 1 Report"));
        CollectGarbage(RF_NoFlags, true);
        Stage->CurrentDay = 2;
        BroadcastStage(*Stage, EFF_FestivalStage::Live);
        Test->TestEqual(Label(TEXT("Day 2 restarts the arrival cursor")), Crowd->GetArrivalSpawnedCount(), 0);
        CollectGarbage(RF_NoFlags, true);
        Test->TestNotNull(Label(TEXT("Day 2 retains its arrival table before the first visitor")),
            Crowd->GetLoadedArrivalCurveTable());
        const float DayTwoDuration = Stage->GetLiveDurationForCurrentDay();
        if (!Test->TestTrue(Label(TEXT("The real Day 2 duration is finite and positive")),
                FMath::IsFinite(DayTwoDuration) && DayTwoDuration > 0.f))
        {
            return true;
        }
        Stage->ElapsedLiveSeconds = DayTwoDuration * 0.25f;
        Crowd->UpdateArrivalSchedule();
        CheckSpawned(*World, *Crowd, TEXT("Day 2 at 25% after GC"));
        BroadcastStage(*Stage, EFF_FestivalStage::Report);
        CheckDespawned(*World, *Crowd, TEXT("Day 2 Report"));
        Test->AddInfo(Label(TEXT("Completed two-day spawn/GC/despawn lifecycle using the actual Test_Main PIE world")));
        return true;
    }

private:
    FString Label(const TCHAR* Message) const
    {
        return FString::Printf(TEXT("PIE session %d: %s"), Session, Message);
    }

    static void BroadcastStage(UFF_FestivalStageSubsystem& Stage, const EFF_FestivalStage Next)
    {
        const EFF_FestivalStage Previous = Stage.CurrentStage;
        Stage.CurrentStage = Next;
        Stage.ElapsedLiveSeconds = 0.f;
        Stage.OnStageChanged.Broadcast(Previous, Next);
    }

    void CheckSpawned(UWorld& World, const UFF_CrowdSimulationSubsystem& Crowd, const TCHAR* Checkpoint) const
    {
        const FString Prefix = Label(Checkpoint);
        const int32 Active = Crowd.GetActiveAgentCount();
        const int32 Cursor = Crowd.GetArrivalSpawnedCount();
        Test->TestTrue(Prefix + TEXT(": visitors are active"), Active > 0);
        Test->TestTrue(Prefix + TEXT(": the arrival cursor has advanced"), Cursor > 0);
        Test->TestEqual(Prefix + TEXT(": every active visitor has a presentation actor"), Crowd.GetBoundActorCount(), Active);
        Test->TestEqual(Prefix + TEXT(": no scheduled visitor silently disappears before a simulation tick"), Cursor, Active);
        int32 VisibleActors = 0;
        int32 VisibleSkeletalNpcs = 0;
        bool bFiniteActorPositions = true;
        for (TActorIterator<AFF_BaseCrowd> It(&World); It; ++It)
        {
            if (It->IsHidden()) continue;
            ++VisibleActors;
            bFiniteActorPositions &= !It->GetActorLocation().ContainsNaN();
            const USkeletalMeshComponent* Mesh = It->SkeletalMeshComponent;
            if (IsValid(Mesh) && Mesh->GetSkeletalMeshAsset() && Mesh->IsVisible() && !Mesh->bHiddenInGame &&
                Mesh->GetComponentScale().GetAbsMin() > KINDA_SMALL_NUMBER)
            {
                ++VisibleSkeletalNpcs;
            }
        }
        bool bFiniteAgentPositions = true;
        for (int32 Agent = 0; Agent < Crowd.GetAgentPoolSize(); ++Agent)
        {
            if (Crowd.IsAgentActive(Agent)) bFiniteAgentPositions &= !Crowd.GetAgentPosition(Agent).ContainsNaN();
        }
        Test->TestEqual(Prefix + TEXT(": bound actors are revealed"), VisibleActors, Active);
        Test->TestTrue(Prefix + TEXT(": real skeletal NPC meshes are enabled, beyond proxy cylinders"), VisibleSkeletalNpcs > 0);
        Test->TestTrue(Prefix + TEXT(": agent and actor positions are finite"), bFiniteAgentPositions && bFiniteActorPositions);
        Test->AddInfo(FString::Printf(TEXT("%s: active=%d cursor=%d bound=%d visible=%d skeletal=%d"),
            *Prefix, Active, Cursor, Crowd.GetBoundActorCount(), VisibleActors, VisibleSkeletalNpcs));
    }

    void CheckDespawned(UWorld& World, const UFF_CrowdSimulationSubsystem& Crowd, const TCHAR* Checkpoint) const
    {
        const FString Prefix = Label(Checkpoint);
        Test->TestEqual(Prefix + TEXT(": Report despawns all visitors"), Crowd.GetActiveAgentCount(), 0);
        Test->TestEqual(Prefix + TEXT(": Report releases every actor binding"), Crowd.GetBoundActorCount(), 0);
        int32 VisibleActors = 0;
        for (TActorIterator<AFF_BaseCrowd> It(&World); It; ++It) VisibleActors += It->IsHidden() ? 0 : 1;
        Test->TestEqual(Prefix + TEXT(": all pooled NPC actors are hidden"), VisibleActors, 0);
    }

    FAutomationTestBase* Test;
    int32 Session;
    TSharedRef<FFFProductionCrowdPIEState> State;
    double StartedAt = -1.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFCrowdProductionSpawnTest,
    "FestaFiesta.Crowd.Production.TestMainRepeatedPIESpawnsAfterGC",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFCrowdProductionSpawnTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/FestaFiesta/Maps/Test_Main"));
    if (!TestNotNull(TEXT("The actual release map loads without saving"), World)) return false;
    const TSharedRef<FFFProductionCrowdPIEState> State = MakeShared<FFFProductionCrowdPIEState>();
    for (int32 Session = 1; Session <= 3; ++Session)
    {
        ADD_LATENT_AUTOMATION_COMMAND(FFFStartProductionCrowdPIE(this, State));
        ADD_LATENT_AUTOMATION_COMMAND(FFFCheckProductionCrowdSpawning(this, Session, State));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
        // RequestEndPlayMap only queues teardown. Do not let the next start or
        // the next automation test run until the world and context are gone.
        ADD_LATENT_AUTOMATION_COMMAND(FFFWaitForProductionCrowdPIEEnd(this, Session, State));
    }
    return true;
}
#endif
