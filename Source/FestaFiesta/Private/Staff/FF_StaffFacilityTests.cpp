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
#include "Staff/FF_StaffActor.h"
#include "Staff/FF_StaffSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"

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
	if (!TestTrue(TEXT("기본 스태프 2번을 원본 Command로 Lv2로 승급한다"), H.Progression->TryPromoteStaff(2))) return false;

    FVector Target;
    if (!TestTrue(TEXT("The near facility has an authored Exit anchor"), H.ExitLocation(NearHome, Target))) return false;
    TArray<FFF_StaffDispatchOrigin> Origins;
	TArray<FFF_StaffDispatchOrigin> PreviewOrigins;
	TestFalse(TEXT("미배정 인원은 예상 출동에도 포함하지 않는다"), H.Staffing->PreviewStaffReservation(4, Target, PreviewOrigins));
	if (!TestTrue(TEXT("예약 없이 실제 후보를 미리 읽는다"), H.Staffing->PreviewStaffReservation(2, Target, PreviewOrigins))) return false;
	if (!TestEqual(TEXT("미리보기는 정확히 두 명이다"), PreviewOrigins.Num(), 2)) return false;
	TestEqual(TEXT("미리보기는 예약을 만들지 않는다"), H.Staffing->GetBusyStaffCount(), 0);
	TestEqual(TEXT("가장 가까운 후보의 원본 숙련도가 복사된다"), PreviewOrigins[0].TrainingLevel, 2);
    TestFalse(TEXT("Unassigned members cannot fill a dispatch shortage"), H.Staffing->TryReserveStaff(4, Target, Origins));
    TestEqual(TEXT("Failed reservation publishes no partial origins"), Origins.Num(), 0);
    TestEqual(TEXT("Failed reservation leaves all staff available"), H.Staffing->GetBusyStaffCount(), 0);
    TestFalse(TEXT("A zero-person reservation is rejected"), H.Staffing->TryReserveStaff(0, Target, Origins));
    if (!TestTrue(TEXT("Two assigned staff can be reserved atomically"), H.Staffing->TryReserveStaff(2, Target, Origins))) return false;
    if (!TestEqual(TEXT("Successful reservation returns the exact requested count"), Origins.Num(), 2)) return false;
	TestEqual(TEXT("예상 첫 후보와 실제 예약이 같다"), Origins[0].StaffId, PreviewOrigins[0].StaffId);
	TestEqual(TEXT("예상 두 번째 후보와 실제 예약이 같다"), Origins[1].StaffId, PreviewOrigins[1].StaffId);
    TestEqual(TEXT("Nearest home is selected before a lower-ID distant member"), Origins[0].StaffId, 2);
    TestEqual(TEXT("Equal-distance staff have a stable ID tie break"), Origins[1].StaffId, 3);
    TestTrue(TEXT("Dispatch retains the assigned facility identity"), Origins[0].FacilityInstanceId == NearHome);
    TestTrue(TEXT("Dispatch starts exactly at that facility's Exit"), Origins[0].Location.Equals(Target));
    TestFalse(TEXT("A departing member cannot change home"), H.Staffing->TryAssignStaff(2, FarHome));
    TestFalse(TEXT("A departing member cannot lose its assignment"), H.Staffing->TryUnassignStaff(2));
    TestFalse(TEXT("All-busy homes disable the unassign UI action"), H.Staffing->CanUnassignAvailableStaff(NearHome));
    TestFalse(TEXT("Busy members cannot be double-reserved"), H.Staffing->TryReserveStaff(2, Target, Origins));
	TestFalse(TEXT("예약된 인원은 예상 출동에서도 제외된다"), H.Staffing->PreviewStaffReservation(2, Target, PreviewOrigins));
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
    auto CountResponders = [&H]()
    {
        int32 Count = 0;
        for (TActorIterator<AFF_StaffActor> It(H.World); It; ++It) ++Count;
        return Count;
    };
    UFF_StaffSubsystem* Bodies = H.World->GetSubsystem<UFF_StaffSubsystem>();
    if (!TestNotNull(TEXT("The actual staff body owner exists"), Bodies)) return false;
    EFF_IncidentResolveResult Result = EFF_IncidentResolveResult::Dispatched;
    TestFalse(TEXT("More staff than are assigned cannot be sent"),
        Dispatch->TryDispatch(Handle, INDEX_NONE, 4, Slot.Anchor, 0.f, &Result));
    TestEqual(TEXT("A shortage is reported as a shortage"), Result, EFF_IncidentResolveResult::NotEnoughStaff);
    FFF_IncidentAnchor NoSite;
    NoSite.Kind = EFF_IncidentAnchorKind::Screenspace;
    TestFalse(TEXT("A screen-only anchor has no site"), Dispatch->TryDispatch(Handle, INDEX_NONE, 2, NoSite, 0.f, &Result));
    TestEqual(TEXT("A missing site is reported as a missing site"), Result, EFF_IncidentResolveResult::NoSite);
    TestEqual(TEXT("Rejected dispatches reserve nobody"), H.Staffing->GetBusyStaffCount(), 0);
	float PreviewMinutes = 0.f, PreviewAbility = 0.f;
	if (!TestTrue(TEXT("현장 작업 예상은 실제 후보의 숙련도를 사용한다"),
		Dispatch->PreviewWork(2, Slot.Anchor, 10.f, 1, PreviewMinutes, PreviewAbility))) return false;
	TestEqual(TEXT("Lv2 한 명과 추가 Lv1 한 명의 능력 합은 2.5다"), PreviewAbility, 2.5f);
	TestEqual(TEXT("10 사람-분의 예상은 4 축제 분이다"), PreviewMinutes, 4.f);
	TestEqual(TEXT("시간 미리보기도 예약하지 않는다"), H.Staffing->GetBusyStaffCount(), 0);
	// 둘째 몸체 생성 실패를 만들어 첫째 몸체와 전체 예약이 함께 원복되는지 확인한다.
	FFF_StaffDispatchOrigin OccupiedOrigin;
	if (!H.Staffing->GetStaffDispatchOrigin(3, OccupiedOrigin)
		|| !Bodies->SendStaff(OccupiedOrigin, Slot.Anchor)) return false;
	TestFalse(TEXT("팀 일부 몸체 생성 실패면 출동 전체를 취소한다"),
		Dispatch->TryDispatch(Handle, INDEX_NONE, 2, Slot.Anchor, 10.f, &Result, 1));
	TestEqual(TEXT("실패 후 예약이 남지 않는다"), H.Staffing->GetBusyStaffCount(), 0);
	TestEqual(TEXT("실패 후 부분 출동 몸체가 남지 않는다"), CountResponders(), 0);
    TestTrue(TEXT("A world without navigation still starts a staff journey"),
        Dispatch->TryDispatch(Handle, INDEX_NONE, 2, Slot.Anchor, 10.f, &Result, 1));
	float ActualMinutes = 0.f;
	TestTrue(TEXT("출동한 총 작업 시간을 읽는다"), Dispatch->GetTotalWorkMinutes(Handle, ActualMinutes));
	TestEqual(TEXT("실제 총 작업 시간과 미리보기가 같다"), ActualMinutes, PreviewMinutes);
	TestTrue(TEXT("출동 중인 다른 대원도 기존 원본 규칙으로 승급할 수 있다"), H.Progression->TryPromoteStaff(3));
	Dispatch->GetTotalWorkMinutes(Handle, ActualMinutes);
	TestEqual(TEXT("진행 중 승급은 이미 확정한 작업 시간을 바꾸지 않는다"), ActualMinutes, PreviewMinutes);
    TestEqual(TEXT("An accepted dispatch is reported as dispatched"), Result, EFF_IncidentResolveResult::Dispatched);
    FFF_IncidentView View;
    if (TestTrue(TEXT("The dispatched incident still has a view"), Incident->GetIncidentView(Handle, View)))
    {
        TestEqual(TEXT("The incident view shows staff on the way"), View.ResponseStage, EFF_IncidentResponseStage::Travelling);
    }
    TestFalse(TEXT("The same incident cannot be dispatched twice"),
        Incident->ResolveIncident(Handle, INDEX_NONE, INDEX_NONE, &Result));
    TestEqual(TEXT("A second request is reported as already responding"), Result, EFF_IncidentResolveResult::AlreadyResponding);
    TestEqual(TEXT("Both members stay reserved while travelling without a path"), H.Staffing->GetBusyStaffCount(), 2);
    TestTrue(TEXT("The pathless journey is an active response"), Dispatch->IsResponding(Handle));
    TestEqual(TEXT("Each member gets a body at the facility exit"), CountResponders(), 2);
    const AFF_StaffActor* Body = Bodies->FindStaffActor(2);
    if (TestNotNull(TEXT("A dispatched member can be looked up by its staff ID"), Body))
    {
        TestEqual(TEXT("The body knows which member it is"), Body->GetStaffId(), 2);
        const float FromExit = FVector::Dist2D(Body->GetActorLocation(), Target);
        TestTrue(TEXT("A team member starts beside its facility exit, not on it"), FromExit > 1.f && FromExit <= 200.f);
        TestEqual(TEXT("Spreading keeps the body on the exit's floor height"), Body->GetActorLocation().Z, Target.Z, 1.0);
        if (const AFF_StaffActor* Partner = Bodies->FindStaffActor(3))
        {
            TestTrue(TEXT("Two members leaving the same exit do not overlap"),
                FVector::Dist2D(Body->GetActorLocation(), Partner->GetActorLocation()) >= UFF_StaffSubsystem::SpreadSpacingCm - 1.f);
        }
        TestFalse(TEXT("The body never collides with the crowd"), Body->GetActorEnableCollision());
        TestEqual(TEXT("A freshly sent body is told it is walking out"), Body->GetStaffState(), EFF_StaffState::Moving);
    }
    TestNull(TEXT("An undispatched member has no body"), Bodies->FindStaffActor(1));

    Incident->WithdrawIncident(Slot.DedupeKey);
	TestFalse(TEXT("복귀 중인 두 명은 새 예상 출동에서 제외된다"),
		Dispatch->PreviewWork(2, Slot.Anchor, 10.f, 1, PreviewMinutes, PreviewAbility));
    Bodies->Tick(0.1f);
    Dispatch->Tick(0.1f);
    TestEqual(TEXT("A recalled member first steps back from its spread spot into the exit"), H.Staffing->GetBusyStaffCount(), 2);
    Bodies->Tick(0.5f);
    Dispatch->Tick(0.5f);
    TestEqual(TEXT("A recalled pathless member is home after that short step and releases its reservation"), H.Staffing->GetBusyStaffCount(), 0);
    TestFalse(TEXT("The withdrawn response is cleared"), Dispatch->IsResponding(Handle));
    TestEqual(TEXT("Returning home removes every staff actor"), CountResponders(), 0);
	TestTrue(TEXT("복귀 후 다음 출동의 예상 시간을 다시 읽는다"),
		Dispatch->PreviewWork(2, Slot.Anchor, 10.f, 1, PreviewMinutes, PreviewAbility));
	TestEqual(TEXT("다음 출동부터 두 명의 Lv2를 반영한다"), PreviewAbility, 3.f);
	TestEqual(TEXT("다음 출동 예상은 숙련도 상승만큼 줄어든다"), PreviewMinutes, 10.f / 3.f);
    TestFalse(TEXT("A withdrawn incident cannot be resolved"), Incident->ResolveIncident(Handle, INDEX_NONE, INDEX_NONE, &Result));
    TestEqual(TEXT("A withdrawn incident is reported as gone"), Result, EFF_IncidentResolveResult::InvalidIncident);

    TestTrue(TEXT("Returning staff can be reassigned"), H.Staffing->TryAssignStaff(2, FarHome));
    TestTrue(TEXT("Returning staff can move back into the released home slot"), H.Staffing->TryAssignStaff(2, NearHome));

	// 승급을 반영한 다음 출동이 예상한 축제 분만큼 현장에서 실제로 일해야 닫힌다.
	const FFF_IncidentAnchor WorkAnchor = Slot.Anchor;
	FFF_IncidentHandle WorkHandle;
	WorkHandle.Index = Incident->IncidentSlots.AddDefaulted();
	auto& WorkSlot = Incident->IncidentSlots[WorkHandle.Index];
	WorkSlot.bInUse = true;
	WorkSlot.IncidentTag = IncidentDefinition->IncidentTag;
	WorkSlot.Anchor = WorkAnchor;
	WorkSlot.DedupeKey = TEXT("StaffTrainingWorkClock");
	Incident->ActiveByKey.Add(WorkSlot.DedupeKey, WorkHandle);
	if (!TestTrue(TEXT("숙련도 반영 후 다음 출동을 시작한다"),
		Dispatch->TryDispatch(WorkHandle, INDEX_NONE, 2, WorkAnchor, 9.f, &Result, 1))) return false;
	UFF_FestivalStageSubsystem* Stage = H.World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!TestNotNull(TEXT("축제 작업 시계가 있다"), Stage)) return false;
	Stage->CurrentStage = EFF_FestivalStage::Live;
	Stage->ElapsedLiveSeconds = 100.f;
	Dispatch->Tick(0.1f);
	Stage->ElapsedLiveSeconds = 101.f;
	Dispatch->Tick(0.1f);
	Dispatch->GetTotalWorkMinutes(WorkHandle, ActualMinutes);
	TestEqual(TEXT("Lv2 두 명의 9 사람-분 작업은 3 축제 분이다"), ActualMinutes, 3.f);
	TestFalse(TEXT("전원 도착 전에는 작업 중으로 표시하지 않는다"), Dispatch->GetRemainingWorkMinutes(WorkHandle, ActualMinutes));
	for (const int32 StaffId : TArray<int32>{2, 3}) Bodies->Staff.FindChecked(StaffId).State = EFF_StaffState::Working;
	Dispatch->Tick(0.1f);
	TestTrue(TEXT("전원 도착하면 작업 시계가 시작된다"), Dispatch->GetRemainingWorkMinutes(WorkHandle, ActualMinutes));
	TestEqual(TEXT("이동 중 경과한 시간은 작업량에서 빼지 않는다"), ActualMinutes, 3.f);
	Stage->ElapsedLiveSeconds = 103.f;
	Dispatch->Tick(0.1f);
	TestTrue(TEXT("예상한 3분 중 2분만 지나면 사건이 유지된다"), Incident->IsHandleValid(WorkHandle));
	Dispatch->GetRemainingWorkMinutes(WorkHandle, ActualMinutes);
	TestEqual(TEXT("원본 시계 2 축제 분만큼만 작업한다"), ActualMinutes, 1.f);
	Stage->ElapsedLiveSeconds = 103.99f;
	Dispatch->Tick(0.1f);
	TestTrue(TEXT("예상 완료 시각 직전에는 사건을 끝내지 않는다"), Incident->IsHandleValid(WorkHandle));
	// 초→분 환산의 부동소수점 잔여치까지 다음 프레임에서 소진한다.
	// 실제 게임도 프레임 단위로 완료하므로 정확한 실수 경계 일치를 요구하지 않는다.
	Stage->ElapsedLiveSeconds = 104.01f;
	Dispatch->Tick(0.1f);
	TestFalse(TEXT("예상한 3 축제 분을 채우면 사건을 해결한다"), Incident->IsHandleValid(WorkHandle));
	Bodies->Tick(1.f);
	Dispatch->Tick(0.1f);
	TestEqual(TEXT("완료 후 복귀하면 예약을 반환한다"), H.Staffing->GetBusyStaffCount(), 0);
	Stage->CurrentStage = EFF_FestivalStage::Setup;

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
