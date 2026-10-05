#include "Staff/FF_StaffFacilitySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Incident/FF_IncidentDefinition.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/FF_IncidentTags.h"
#include "Staff/FF_StaffDispatchSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace FFStaffFacilityTests
{
    const FName ZoneId(TEXT("GZ_DadaPocha"));
    const FName CostKey(TEXT("Cost.Info.StaffFacilityAutomation"));
    constexpr int32 InstallCost = 100;

    struct FScopedSettings
    {
        FScopedSettings()
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , Ground(Settings->bEnableGroundChecks, false)
            , Collision(Settings->bEnableWorldCollisionChecks, false)
            , Navigation(Settings->bEnableNavigationProjectionChecks, false)
            , Production(Settings->bRequireProductionPolicyDomains, false)
            , RestrictZones(Settings->bRestrictZoneIdsToProjectPresets, false)
            , Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup)
            , CellBudget(Settings->MaxEnumeratedCellsPerFacility, 1'000'000)
        {}

        UFFZSDeveloperSettings* Settings;
        TGuardValue<bool> Ground;
        TGuardValue<bool> Collision;
        TGuardValue<bool> Navigation;
        TGuardValue<bool> Production;
        TGuardValue<bool> RestrictZones;
        TGuardValue<EFFZSChangePhase> Phase;
        TGuardValue<int32> CellBudget;
    };

    struct FHarness
    {
        bool Initialize(FAutomationTestBase& InTest)
        {
            Test = &InTest;
            if (!TestWorld.CreateTestWorld(EWorldType::Game))
            {
                TestWorld.ForwardErrorMessages(Test);
                return false;
            }
            World = TestWorld.GetTestWorld();
            UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
            Zone = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
            Staffing = World ? World->GetSubsystem<UFF_StaffFacilitySubsystem>() : nullptr;
            Progression = GI ? GI->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
            if (!World || !GI || !Zone || !Staffing || !Progression) return false;

            Definition = NewObject<UFF_FestivalDefinition>(GI);
            UDataTable* Costs = NewObject<UDataTable>(GI);
            Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
            FFF_FacilityCostRow Cost;
            Cost.InstallCost = InstallCost;
            Costs->AddRow(CostKey, Cost);
            Definition->FestivalId = TEXT("StaffFacilityAutomationFestival");
            Definition->InitialGold = 10000;
            Definition->InitialStaffCount = 4;
            Definition->MaxHiredStaffCount = 1;
            Definition->StaffHireCostGold = 100;
            Definition->FacilityCostTable = Costs;
            Definition->FacilityRevenueTable.Reset();
            Progression->StartFestival(Definition);

            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AFFZSGameplayZone* GameplayZone = World->SpawnActor<AFFZSGameplayZone>(
                AFFZSGameplayZone::StaticClass(), FTransform::Identity, Spawn);
            return GameplayZone && GameplayZone->ApplyAcceptedZonePreset(ZoneId)
                && Zone->RegisterZone(GameplayZone);
        }

        bool Install(FName DefinitionId, int32 Capacity, FIntPoint Position, FGuid& OutId,
            EFFZSTestFacilityVariant Variant = EFFZSTestFacilityVariant::Info)
        {
            FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(Variant);
            Spec.FacilityDefinitionId = DefinitionId;
            Spec.StaffCapacity = Capacity;
            Spec.InstallCostKey = CostKey;
            FFFZSGridPlacement Placement;
            Placement.OriginCell = Position;
            const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(
                Spec, ZoneId, Placement, Zone->GetWorldRevision());
            if (!Execute(Command)) return false;
            OutId = Command.FacilityInstanceId;
            return Zone->FindFacility(OutId) != nullptr;
        }

        FFFZSCommand CommandFor(EFFZSCommandType Type, FGuid FacilityId) const
        {
            FFFZSCommand Command;
            Command.CommandId = FGuid::NewGuid();
            Command.TransactionId = FGuid::NewGuid();
            Command.Type = Type;
            Command.FacilityInstanceId = FacilityId;
            Command.ExpectedWorldRevision = Zone->GetWorldRevision();
            Command.ZoneId = ZoneId;
            return Command;
        }

        bool Execute(const FFFZSCommand& Command)
        {
            const FFFZSCommandResult Result = Zone->ExecuteCommand(Command);
            if (!Result.bSucceeded)
                Test->AddError(FString::Printf(TEXT("Staff facility fixture command failed: %s"), *Result.Message.ToString()));
            return Result.bSucceeded;
        }

        bool ExitLocation(FGuid FacilityId, FVector& OutLocation) const
        {
            const AFFZSFacilityActor* Facility = Zone->FindFacility(FacilityId);
            FTransform Exit;
            if (!Facility || !Facility->GetAnchorWorldTransform(EFFZSAnchorType::Exit, 0, Exit)) return false;
            OutLocation = Exit.GetLocation();
            return true;
        }

        FTestWorldWrapper TestWorld;
        FAutomationTestBase* Test = nullptr;
        UWorld* World = nullptr;
        UFFZSWorldSubsystem* Zone = nullptr;
        UFF_StaffFacilitySubsystem* Staffing = nullptr;
        UFF_FestivalProgressionSubsystem* Progression = nullptr;
        UFF_FestivalDefinition* Definition = nullptr;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffFacilityCapacityAndAssignmentTest,
    "FestaFiesta.Staff.Facilities.CapacityAndExplicitAssignment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_StaffFacilityCapacityAndAssignmentTest::RunTest(const FString& Parameters)
{
    using namespace FFStaffFacilityTests;
    (void)Parameters;
    FScopedSettings Settings;
    FHarness H;
    if (!TestTrue(TEXT("Game world staffing fixture starts"), H.Initialize(*this))) return false;
    TestEqual(TEXT("The festival retains its base staff and base hiring limit"), H.Progression->GetMaxTotalStaffCount(), 5);
    const TArray<FFF_StaffMember> OriginalRoster = H.Progression->GetStaffRoster();
    const int32 GoldBefore = H.Progression->GetGold();
    FGuid Info;
    if (!H.Install(TEXT("Info_StaffCapacityTest"), 2, FIntPoint(1000, 1000), Info)) return false;

    TestEqual(TEXT("Installing Info increases the maximum by its asset capacity"), H.Progression->GetMaxTotalStaffCount(), 7);
    TestEqual(TEXT("Installing Info adds no hired members"), H.Progression->GetHiredStaffCount(), 0);
    TestEqual(TEXT("Installing Info preserves roster size"), H.Progression->GetStaffRoster().Num(), OriginalRoster.Num());
    TestEqual(TEXT("Installation charges only the authored construction cost"), H.Progression->GetGold(), GoldBefore - InstallCost);
    TestEqual(TEXT("Installing does not silently assign the existing roster"), H.Staffing->GetAssignedStaffCount(Info), 0);

    if (!TestTrue(TEXT("Hiring explicitly consumes the newly available capacity"), H.Progression->TryHireStaff(3))) return false;
    TestEqual(TEXT("Only the explicit hire increases actual staff"), H.Progression->GetTotalStaffCount(), 7);
    TestEqual(TEXT("Only the explicit hire charges the hiring cost"), H.Progression->GetGold(), GoldBefore - InstallCost - 300);
    TestFalse(TEXT("Hiring beyond the current maximum is rejected"), H.Progression->TryHireStaff(1));

    FGuid Booth, ZeroCapacityInfo;
    if (!H.Install(TEXT("Booth_StaffCapacityTest"), 99, FIntPoint(4000, 1000), Booth, EFFZSTestFacilityVariant::FoodTruck)
        || !H.Install(TEXT("Info_ZeroStaffCapacityTest"), 0, FIntPoint(1000, 4000), ZeroCapacityInfo)) return false;
    TestEqual(TEXT("Booths and zero-capacity Info do not add hiring slots"), H.Progression->GetMaxTotalStaffCount(), 7);
    TestFalse(TEXT("A nonexistent staff ID is rejected"), H.Staffing->TryAssignStaff(MAX_int32, Info));
    TestFalse(TEXT("An empty facility ID is rejected"), H.Staffing->TryAssignStaff(1, FGuid()));
    TestFalse(TEXT("An unknown facility ID is rejected"), H.Staffing->TryAssignStaff(1, FGuid::NewGuid()));
    TestFalse(TEXT("A non-Info facility cannot receive staff"), H.Staffing->TryAssignStaff(1, Booth));
    TestFalse(TEXT("Info with no slots cannot receive staff"), H.Staffing->TryAssignStaff(1, ZeroCapacityInfo));
    TestTrue(TEXT("The first existing member can be assigned"), H.Staffing->TryAssignStaff(1, Info));
    TestTrue(TEXT("The second slot can be filled through the UI convenience API"), H.Staffing->TryAssignAvailableStaff(Info));
    TestFalse(TEXT("An over-capacity explicit assignment is rejected"), H.Staffing->TryAssignStaff(3, Info));
    TestFalse(TEXT("A full Info facility disables the add-staff action"), H.Staffing->CanAssignAvailableStaff(Info));
    TestEqual(TEXT("Rejected assignments leave exactly two assigned members"), H.Staffing->GetAssignedStaffCount(Info), 2);
    TestTrue(TEXT("Removing an assignment releases a slot"), H.Staffing->TryUnassignStaff(1));
    TestTrue(TEXT("An empty slot enables assignment again"), H.Staffing->CanAssignAvailableStaff(Info));
    TestTrue(TEXT("A different member can fill the released slot"), H.Staffing->TryAssignStaff(3, Info));
    TestEqual(TEXT("Assignments never alter the hired roster size"), H.Progression->GetTotalStaffCount(), 7);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffFacilityDispatchTest,
    "FestaFiesta.Staff.Facilities.AtomicDispatchFromAssignedExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_StaffFacilityDispatchTest::RunTest(const FString& Parameters)
{
    using namespace FFStaffFacilityTests;
    (void)Parameters;
    FScopedSettings Settings;
    FHarness H;
    if (!TestTrue(TEXT("Dispatch world fixture starts"), H.Initialize(*this))) return false;
    FGuid FarHome, NearHome;
    if (!H.Install(TEXT("Info_FarStaffHome"), 2, FIntPoint(1000, 1000), FarHome)
        || !H.Install(TEXT("Info_NearStaffHome"), 2, FIntPoint(5000, 1000), NearHome)) return false;
    if (!H.Staffing->TryAssignStaff(1, FarHome)
        || !H.Staffing->TryAssignStaff(2, NearHome)
        || !H.Staffing->TryAssignStaff(3, NearHome)) return false;

    FVector Target;
    if (!TestTrue(TEXT("The near facility has an authored Exit anchor"), H.ExitLocation(NearHome, Target))) return false;
    TArray<FFF_StaffDispatchOrigin> Origins;
    TestFalse(TEXT("Unassigned members cannot fill a dispatch shortage"), H.Staffing->TryReserveStaff(4, Target, Origins));
    TestEqual(TEXT("Failed reservation publishes no partial origins"), Origins.Num(), 0);
    TestEqual(TEXT("Failed reservation leaves all staff available"), H.Staffing->GetBusyStaffCount(), 0);
    TestFalse(TEXT("A zero-person reservation is rejected"), H.Staffing->TryReserveStaff(0, Target, Origins));
    if (!TestTrue(TEXT("Two assigned staff can be reserved atomically"), H.Staffing->TryReserveStaff(2, Target, Origins))) return false;
    if (!TestEqual(TEXT("Successful reservation returns the exact requested count"), Origins.Num(), 2)) return false;
    TestEqual(TEXT("Nearest home is selected before a lower-ID distant member"), Origins[0].StaffId, 2);
    TestEqual(TEXT("Equal-distance staff have a stable ID tie break"), Origins[1].StaffId, 3);
    TestTrue(TEXT("Dispatch retains the assigned facility identity"), Origins[0].FacilityInstanceId == NearHome);
    TestTrue(TEXT("Dispatch starts exactly at that facility's Exit"), Origins[0].Location.Equals(Target));
    TestFalse(TEXT("A departing member cannot change home"), H.Staffing->TryAssignStaff(2, FarHome));
    TestFalse(TEXT("A departing member cannot lose its assignment"), H.Staffing->TryUnassignStaff(2));
    TestFalse(TEXT("All-busy homes disable the unassign UI action"), H.Staffing->CanUnassignAvailableStaff(NearHome));
    TestFalse(TEXT("Busy members cannot be double-reserved"), H.Staffing->TryReserveStaff(2, Target, Origins));
    TestEqual(TEXT("A second failed reservation preserves the first reservation"), H.Staffing->GetBusyStaffCount(), 2);

    H.Staffing->ReleaseStaff(TArray<int32>{ 2, 3 });
    H.Staffing->ReleaseStaff(TArray<int32>{ 2, 3, MAX_int32 });
    TestEqual(TEXT("Releasing staff is safe to repeat"), H.Staffing->GetBusyStaffCount(), 0);

    UFF_IncidentSubsystem* Incident = H.World->GetSubsystem<UFF_IncidentSubsystem>();
    UFF_StaffDispatchSubsystem* Dispatch = H.World->GetSubsystem<UFF_StaffDispatchSubsystem>();
    if (!TestNotNull(TEXT("The actual incident owner exists"), Incident)
        || !TestNotNull(TEXT("The actual staff journey owner exists"), Dispatch)) return false;
    UFF_IncidentDefinition* IncidentDefinition = NewObject<UFF_IncidentDefinition>(Incident);
    IncidentDefinition->IncidentTag = FFIncidentTags::Crowd::MemberMissing;
    IncidentDefinition->RequiredStaff = 2;
    Incident->CachedDef.Add(IncidentDefinition->IncidentTag, IncidentDefinition);
    FFF_IncidentHandle Handle;
    Handle.Index = Incident->IncidentSlots.AddDefaulted();
    auto& Slot = Incident->IncidentSlots[Handle.Index];
    Slot.bInUse = true;
    Slot.IncidentTag = IncidentDefinition->IncidentTag;
    Slot.Anchor.Kind = EFF_IncidentAnchorKind::WorldPoint;
    Slot.Anchor.WorldPoint = Target;
    Slot.DedupeKey = TEXT("StaffFacilityNoNavDispatch");
    Incident->ActiveByKey.Add(Slot.DedupeKey, Handle);
    TestFalse(TEXT("A world without navigation cannot start a staff journey"),
        Dispatch->TryDispatch(Handle, INDEX_NONE, 2, Slot.Anchor));
    TestEqual(TEXT("Path failure rolls back every reserved staff member"), H.Staffing->GetBusyStaffCount(), 0);
    TestTrue(TEXT("Path failure keeps the incident available for a later retry"), Incident->IsHandleValid(Handle));
    TestFalse(TEXT("Path failure retains no in-flight dispatch"), Dispatch->IsResponding(Handle));
    int32 ResponderCount = 0;
    for (TActorIterator<AActor> It(H.World); It; ++It)
        if (It->ActorHasTag(TEXT("StaffResponder"))) ++ResponderCount;
    TestEqual(TEXT("Path failure leaves no staff actor behind"), ResponderCount, 0);

    TestTrue(TEXT("Returning staff can be reassigned"), H.Staffing->TryAssignStaff(2, FarHome));
    TestTrue(TEXT("Returning staff can move back into the released home slot"), H.Staffing->TryAssignStaff(2, NearHome));

    FFFZSCommand Move = H.CommandFor(EFFZSCommandType::Move, NearHome);
    Move.Placement.OriginCell = FIntPoint(5000, 4000);
    Move.Placement.YawCentidegrees = 9000;
    if (!TestTrue(TEXT("The home facility can move and rotate"), H.Execute(Move))) return false;
    FVector MovedExit;
    FFF_StaffDispatchOrigin MovedOrigin;
    if (!TestTrue(TEXT("The moved facility still exposes an Exit"), H.ExitLocation(NearHome, MovedExit))
        || !TestTrue(TEXT("Assigned staff retain a valid origin after moving"), H.Staffing->GetStaffDispatchOrigin(2, MovedOrigin))) return false;
    TestFalse(TEXT("Moving changes the world departure point"), MovedExit.Equals(Target));
    TestTrue(TEXT("Departure follows the moved and rotated Exit, without a stale cache"), MovedOrigin.Location.Equals(MovedExit));
    TestTrue(TEXT("Moving preserves staff ownership"), MovedOrigin.FacilityInstanceId == NearHome);
    TestEqual(TEXT("Moving preserves total facility capacity"), H.Staffing->GetTotalFacilityCapacity(), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffFacilityRemovalAndSaveTest,
    "FestaFiesta.Staff.Facilities.RemovalAndSavedAssignments",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_StaffFacilityRemovalAndSaveTest::RunTest(const FString& Parameters)
{
    using namespace FFStaffFacilityTests;
    (void)Parameters;
    FScopedSettings Settings;
    FHarness H;
    if (!TestTrue(TEXT("Removal/save world fixture starts"), H.Initialize(*this))) return false;
    FGuid Home;
    if (!H.Install(TEXT("Info_SavedStaffHome"), 2, FIntPoint(1000, 1000), Home)) return false;
    if (!TestTrue(TEXT("Staff can be hired up to the expanded limit"), H.Progression->TryHireStaff(3))
        || !TestTrue(TEXT("A hired member can be assigned to Info"), H.Staffing->TryAssignStaff(5, Home))) return false;

    TArray<uint8> SavedProgression;
    H.Progression->WriteSaveData(SavedProgression);
    TestTrue(TEXT("Progression produces actual save bytes for the assigned roster"), !SavedProgression.IsEmpty());
    TestTrue(TEXT("Assignment can change after the save"), H.Staffing->TryUnassignStaff(5));
    H.Progression->ReadSaveData(SavedProgression);
    FFF_StaffDispatchOrigin SavedOrigin;
    if (!TestTrue(TEXT("Reading save bytes restores the assigned member's departure origin"), H.Staffing->GetStaffDispatchOrigin(5, SavedOrigin))) return false;
    TestTrue(TEXT("The saved facility GUID survives a byte roundtrip"), SavedOrigin.FacilityInstanceId == Home);
    TestEqual(TEXT("Restore preserves the roster without duplicate hiring"), H.Progression->GetTotalStaffCount(), 7);

    TArray<FFF_StaffDispatchOrigin> Origins;
    if (!TestTrue(TEXT("The saved member can depart"), H.Staffing->TryReserveStaff(1, SavedOrigin.Location, Origins))) return false;
    if (!TestTrue(TEXT("The assigned home can be removed"), H.Execute(H.CommandFor(EFFZSCommandType::Remove, Home)))) return false;
    TestEqual(TEXT("Removal drops the facility capacity immediately"), H.Staffing->GetTotalFacilityCapacity(), 0);
    TestEqual(TEXT("Removal returns the maximum to the festival base limit"), H.Progression->GetMaxTotalStaffCount(), 5);
    TestEqual(TEXT("Already hired staff are not deleted when capacity shrinks"), H.Progression->GetTotalStaffCount(), 7);
    TestEqual(TEXT("An over-cap roster cannot hire until enough capacity returns"), H.Progression->GetRemainingHireCap(), 0);
    TestFalse(TEXT("An over-cap roster rejects new hiring"), H.Progression->TryHireStaff(1));
    TestEqual(TEXT("Removal clears the invalid facility assignment"), H.Staffing->GetAssignedStaffCount(Home), 0);
    FFF_StaffDispatchOrigin RemovedOrigin;
    TestFalse(TEXT("Removed facilities cannot become future dispatch origins"), H.Staffing->GetStaffDispatchOrigin(5, RemovedOrigin));
    TestTrue(TEXT("A member already on a dispatch stays busy until the dispatch completes"), H.Staffing->IsStaffBusy(5));
    H.Staffing->ReleaseStaff(TArray<int32>{ 5 });
    TestEqual(TEXT("Dispatch completion releases staff even when their old home was removed"), H.Staffing->GetBusyStaffCount(), 0);

    H.Progression->ReadSaveData(SavedProgression);
    TestEqual(TEXT("A stale saved assignment to a missing facility is cleared"), H.Staffing->GetAssignedStaffCount(Home), 0);
    TestEqual(TEXT("Repairing a missing saved home still preserves all people"), H.Progression->GetTotalStaffCount(), 7);
    return true;
}

#endif
