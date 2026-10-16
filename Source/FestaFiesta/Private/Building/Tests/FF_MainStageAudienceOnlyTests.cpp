#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSAudienceLibrary.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_MainStageAudienceOnlyTest,
    "FestaFiesta.Building.MainStage.AudienceOnlyNoQueuePersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_MainStageAudienceOnlyTest::RunTest(const FString& Parameters)
{
    const UFFZSFacilityDefinition* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage.PDA_FFZS_Performance_MainStage_SunsetMainStage"));
    if (!TestNotNull(TEXT("The production main-stage definition loads"), Definition)) return false;
    const FFFZSFacilitySpec Base = Definition->Spec;
    TestTrue(TEXT("The production main stage is an audience-only venue"), Base.IsAudienceOnlyVenue());
    TestFalse(TEXT("PostLoad does not opt the main stage back into common queues"), Base.QueueSettings.bEnabled);
    TestFalse(TEXT("The main stage has no FIFO service"), Base.UsesServiceQueue());
    TestEqual(TEXT("The retired waiting-team capacity is zero"), Base.QueueRules.MaxWaitingAgents, 0);
    TestEqual(TEXT("The retired concurrent FIFO capacity is zero"), Base.QueueRules.ServiceCapacity, 0);
    TestTrue(TEXT("The retired FIFO reserves no space"), Base.QueueReservedRects.IsEmpty() && Base.QueueReservedCells.IsEmpty());
    TestTrue(TEXT("The retired FIFO has no spline or service anchors"), Base.QueueSplinePointsCm.IsEmpty() && Base.Anchors.IsEmpty());
    if (!TestTrue(TEXT("The authored audience venue retains its seats"), Base.Audience.bEnabled && Base.Audience.GetCapacity() > 0)) return false;

    // Old serialized data may still contain the former FIFO. Its retirement must
    // preserve the independently authored audience, pricing and paid upgrade data.
    const FFFZSFacilitySpec LegacyQueue = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Performance);
    FFFZSFacilitySpec Legacy = Base;
    Legacy.QueueRules.MaxWaitingAgents = LegacyQueue.QueueRules.MaxWaitingAgents;
    Legacy.QueueRules.ServiceCapacity = LegacyQueue.QueueRules.ServiceCapacity;
    Legacy.QueueSplinePointsCm = LegacyQueue.QueueSplinePointsCm;
    Legacy.QueueReservedRects = LegacyQueue.QueueReservedRects;
    Legacy.Anchors = LegacyQueue.Anchors;
    Legacy.QueueSettings.bEnabled = true;
    Legacy.QueueLayout = EFFZSQueueLayout::UShape;
    Legacy.NormalizeNonServiceGeometry();
    TestTrue(TEXT("Normalization exactly restores the queue-free authored specification"),
        FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&Base, &Legacy, 0));
    FFFZSFacilitySpec OtherPerformance = Base;
    OtherPerformance.FacilityDefinitionId = TEXT("Performance_SubStage_AudienceOnlyRegression");
    TestTrue(TEXT("Other performance facilities retain service queue capability"), OtherPerformance.UsesServiceQueue());

    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Nav(Settings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
    TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("The synthetic zone exists"), Zone)) return false;
    // Keep the real, large production audience inside the test's authored boundary.
    Zone->BoundarySpline->SetLocationAtSplinePoint(1, FVector(40000, 0, 0), ESplineCoordinateSpace::Local, false);
    Zone->BoundarySpline->SetLocationAtSplinePoint(2, FVector(40000, 40000, 0), ESplineCoordinateSpace::Local, false);
    Zone->BoundarySpline->SetLocationAtSplinePoint(3, FVector(0, 40000, 0), ESplineCoordinateSpace::Local, true);
    if (!FFProjectZoneTest::RegisterZone(*this, Zone, Zones)) return false;

    FFFZSFacilitySpec Spec = Base;
    Spec.InstallCostKey = NAME_None;
    Spec.AllowedZoneIds.Reset();
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(10000, 10000);
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, Placement, Zones->GetWorldRevision());
    const FFFZSCommandResult Result = Zones->ExecuteCommand(Install);
    if (!TestTrue(*FString::Printf(TEXT("The queue-free production stage installs: %s"), *Result.Message.ToString()), Result.bSucceeded)) return false;
    AFFZSFacilityActor* Facility = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("The installed main stage exists"), Facility)) return false;
    FFF_QueueLaneGeometry Lane;
    TestFalse(TEXT("A main stage cannot project a Party Queue lane"),
        UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(Facility->GetFacilitySpec(), Facility->GetActorTransform(), Lane));
    FText QueueError;
    FFFZSQueueSettings RequestedSettings;
    RequestedSettings.bEnabled = true;
    TestFalse(TEXT("Facility settings cannot re-enable the main-stage FIFO"),
        Zones->SetFacilityQueueSettings(Install.FacilityInstanceId, RequestedSettings, EFFZSQueueLayout::Left, QueueError));
    TestTrue(TEXT("Whole-park queue reset succeeds with an audience-only venue"), Zones->ResetAllFacilityQueueSettingsToDefaults().bSucceeded);
    TestFalse(TEXT("Whole-park queue reset does not recreate the main-stage FIFO"), Facility->GetFacilitySpec().QueueSettings.bEnabled);

    // Current-schema saves also require this migration: a player may resume a
    // save made before queue retirement without any schema change.
    FFFZSWorldSnapshot SavedWorld = Zones->CaptureWorldSnapshot();
    if (!TestEqual(TEXT("One stage was captured"), SavedWorld.Facilities.Num(), 1)) return false;
    FFFZSFacilityInstanceSnapshot& Saved = SavedWorld.Facilities[0];
    Saved.FacilitySpec.QueueRules.MaxWaitingAgents = LegacyQueue.QueueRules.MaxWaitingAgents;
    Saved.FacilitySpec.QueueRules.ServiceCapacity = LegacyQueue.QueueRules.ServiceCapacity;
    Saved.FacilitySpec.QueueReservedRects = LegacyQueue.QueueReservedRects;
    Saved.FacilitySpec.QueueSplinePointsCm = LegacyQueue.QueueSplinePointsCm;
    Saved.FacilitySpec.Anchors = LegacyQueue.Anchors;
    Saved.FacilitySpec.QueueSettings.bEnabled = true;
    Saved.State = EFFZSFacilityState::Active;
    Saved.Queue.State = EFFZSQueueState::Open;
    Saved.Queue.StateBeforeBlocked = EFFZSQueueState::Open;
    Saved.Queue.CompletedCount = 3;
    Saved.Queue.AbandonedCount = 2;
    Saved.Queue.TotalCompletedWaitSeconds = 45.f;
    Saved.Queue.NextJoinSequence = 2;
    FFFZSQueueEntrySnapshot& Entry = Saved.Queue.Entries.AddDefaulted_GetRef();
    Entry.TicketId = FGuid::NewGuid();
    Entry.AgentId = TEXT("RetiredMainStageQueueParticipant");
    Entry.JoinSequence = 1;
    Entry.AssignedQueueIndex = 0;
    TArray<FText> Errors;
    if (!Zones->RestoreWorldSnapshot(SavedWorld, Errors))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    TestEqual(TEXT("Restore preserves the caller's original FIFO entries"), SavedWorld.Facilities[0].Queue.Entries.Num(), 1);
    TestTrue(TEXT("Restore preserves the caller's original queue settings"), SavedWorld.Facilities[0].FacilitySpec.QueueSettings.bEnabled);
    Facility = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("The restored stage keeps its identity"), Facility)) return false;
    const FFFZSQueueSnapshot RestoredQueue = Facility->GetQueueComponent()->CaptureSnapshot();
    TestTrue(TEXT("Legacy participants are removed from the retired FIFO"), RestoredQueue.Entries.IsEmpty());
    TestEqual(TEXT("The retired FIFO is closed"), RestoredQueue.State, EFFZSQueueState::Closed);
    TestEqual(TEXT("The historical completion total is preserved"), RestoredQueue.CompletedCount, 3);
    TestEqual(TEXT("The historical abandonment total is preserved"), RestoredQueue.AbandonedCount, 2);
    TestEqual(TEXT("The historical completed wait time is preserved"), RestoredQueue.TotalCompletedWaitSeconds, 45.f);
    TestTrue(TEXT("Restored audience geometry and capacity match the authored stage"),
        FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&Base.Audience, &Facility->GetFacilitySpec().Audience, 0));
    FFFZSAudienceSnapshot Audience;
    FText AudienceError;
    TestTrue(TEXT("The restored active stage still exposes its independent audience venue"),
        UFFZSAudienceLibrary::GetFacilityAudienceSnapshot(World, Install.FacilityInstanceId, Audience, AudienceError));
    TestEqual(TEXT("The active audience capacity remains authored"), Audience.Capacity, Base.Audience.GetCapacity());

    UFF_ZoneSystemQueueBridgeSubsystem* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
    if (!TestNotNull(TEXT("The project queue bridge exists"), Bridge)) return false;
    Bridge->RefreshQueueRegistrations();
    TestEqual(TEXT("The project bridge does not register the retired FIFO"), Bridge->FindQueueFacilityId(Install.FacilityInstanceId), INDEX_NONE);
    UFF_FestivalViewModelSubsystem* ViewModel = World->GetGameInstance()->GetSubsystem<UFF_FestivalViewModelSubsystem>();
    if (!TestNotNull(TEXT("The live facility view model exists"), ViewModel)) return false;
    const TArray<FFF_LiveFacilityView> Views = ViewModel->GetLiveFacilities();
    const FFF_LiveFacilityView* View = Views.FindByPredicate([&Install](const FFF_LiveFacilityView& Value)
        { return Value.FacilityInstanceId == Install.FacilityInstanceId; });
    if (!TestNotNull(TEXT("The main stage stays in the live facility list"), View)) return false;
    TestTrue(TEXT("The main stage remains an audience venue in the live UI"), View->bSeatedVenue);
    TestFalse(TEXT("The live UI no longer labels the main stage as a FIFO queue"), View->bUsesQueue);
    return true;
}

#endif
