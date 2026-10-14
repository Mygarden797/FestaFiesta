#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityAnchorComponent.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "Staff/FF_StaffSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"

namespace FFStaffManagementTests
{
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
		TGuardValue<bool> Ground, Collision, Navigation, Production, RestrictZones;
		TGuardValue<EFFZSChangePhase> Phase;
		TGuardValue<int32> CellBudget;
	};

	struct FHarness
	{
		bool Initialize(FAutomationTestBase& Test)
		{
			TestOwner = &Test;
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(&Test);
				return false;
			}
			UWorld* World = TestWorld.GetTestWorld();
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			if (!GI) return false;
			ViewModel = GI->GetSubsystem<UFF_FestivalViewModelSubsystem>();
			Progression = GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
			Stages = GI->GetSubsystem<UFF_FestivalStageSubsystem>();
			Zone = World->GetSubsystem<UFFZSWorldSubsystem>();
			Staffing = World->GetSubsystem<UFF_StaffFacilitySubsystem>();
			Bodies = World->GetSubsystem<UFF_StaffSubsystem>();
			if (!ViewModel || !Progression || !Stages || !Zone || !Staffing || !Bodies) return false;

			UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(GI);
			UDataTable* Costs = NewObject<UDataTable>(GI);
			Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
			const FName CostKey(TEXT("Cost.Info.StaffManagementAutomation"));
			Costs->AddRow(CostKey, FFF_FacilityCostRow());
			Definition->FestivalId = TEXT("StaffManagementAutomationFestival");
			Definition->InitialGold = 10000;
			Definition->InitialStaffCount = 2;
			Definition->FacilityCostTable = Costs;
			Definition->FacilityRevenueTable.Reset();
			Progression->StartFestival(Definition);

			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AFFZSGameplayZone* GameplayZone = World->SpawnActor<AFFZSGameplayZone>(AFFZSGameplayZone::StaticClass(), FTransform::Identity, Spawn);
			if (!GameplayZone || !GameplayZone->ApplyAcceptedZonePreset(ZoneId) || !Zone->RegisterZone(GameplayZone)) return false;
			FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Info);
			Spec.FacilityDefinitionId = TEXT("Info_StaffManagementAutomation");
			Spec.StaffCapacity = 2;
			Spec.InstallCostKey = CostKey;
			FFFZSGridPlacement Placement;
			Placement.OriginCell = FIntPoint(1000, 1000);
			const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, ZoneId, Placement, Zone->GetWorldRevision());
			if (!Execute(Install)) return false;
			const FGuid HomeId = Install.FacilityInstanceId;
			Home = Zone->FindFacility(HomeId);
			if (!Home) return false;
			return Staffing->TryAssignStaff(1, HomeId) && Staffing->TryAssignStaff(2, HomeId);
		}

		bool Execute(const FFFZSCommand& Command)
		{
			const FFFZSCommandResult Result = Zone->ExecuteCommand(Command);
			if (!Result.bSucceeded) TestOwner->AddError(Result.Message.ToString());
			return Result.bSucceeded;
		}

		bool ChangeHome(EFFZSCommandType Type)
		{
			FFFZSCommand Command;
			Command.CommandId = FGuid::NewGuid();
			Command.TransactionId = FGuid::NewGuid();
			Command.Type = Type;
			Command.FacilityInstanceId = Home->GetFacilityInstanceId();
			Command.ExpectedWorldRevision = Zone->GetWorldRevision();
			Command.ZoneId = ZoneId;
			return Execute(Command);
		}

		bool RestoreRelocation(bool bNeedsRelocation)
		{
			const FGuid HomeId = Home->GetFacilityInstanceId();
			FFFZSWorldSnapshot Snapshot = Zone->CaptureWorldSnapshot();
			FFFZSFacilityInstanceSnapshot* Saved = Snapshot.Facilities.FindByPredicate(
				[HomeId](const FFFZSFacilityInstanceSnapshot& Value) { return Value.FacilityInstanceId == HomeId; });
			if (!Saved) return false;
			Saved->bNeedsRelocation = bNeedsRelocation;
			TArray<FText> Errors;
			if (!Zone->RestoreWorldSnapshot(Snapshot, Errors))
			{
				for (const FText& Error : Errors) TestOwner->AddError(Error.ToString());
				return false;
			}
			Home = Zone->FindFacility(HomeId);
			return Home != nullptr;
		}

		// 소유자 모듈의 기존 시설 테스트와 같은 공개 Command/복원 경계만 사용한다.
		FScopedSettings Settings;
		FTestWorldWrapper TestWorld;
		FAutomationTestBase* TestOwner = nullptr;
		const FName ZoneId = TEXT("GZ_DadaPocha");
		UFF_FestivalViewModelSubsystem* ViewModel = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalStageSubsystem* Stages = nullptr;
		UFFZSWorldSubsystem* Zone = nullptr;
		UFF_StaffFacilitySubsystem* Staffing = nullptr;
		UFF_StaffSubsystem* Bodies = nullptr;
		AFFZSFacilityActor* Home = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffManagementOpeningLifecycleTest,
	"FestaFiesta.UI.StaffManagement.OpeningLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_StaffManagementOpeningLifecycleTest::RunTest(const FString& Parameters)
{
	FFStaffManagementTests::FHarness H;
	if (!TestTrue(TEXT("실제 명단·시설·ViewModel 픽스처 생성"), H.Initialize(*this))) return false;
	H.Stages->CurrentStage = EFF_FestivalStage::Live;
	if (!TestTrue(TEXT("안내소 개장"), H.ChangeHome(EFFZSCommandType::Open))) return false;
	const FFF_StaffManagementSnapshot Live = H.ViewModel->GetStaffManagementSnapshot();
	TestTrue(TEXT("라이브 스냅샷이 유효하다"), Live.bAvailable);
	TestEqual(TEXT("개장한 시설의 두 명은 원본대로 출동 가능"), Live.Dispatchable, 2);

	// 정상 폐장만 재현한다. Stage의 다음 Tick 자동 개폐가 UI 검사를 대신하지 않게 직접 호출한다.
	if (!TestTrue(TEXT("안내소 정상 폐장"), H.ChangeHome(EFFZSCommandType::Close))) return false;
	TestEqual(TEXT("정상 폐장도 MovePending이다"), H.Home->GetFacilityState(), EFFZSFacilityState::MovePending);
	H.Stages->CurrentStage = EFF_FestivalStage::MorningCheck;
	const FFF_StaffManagementSnapshot Morning = H.ViewModel->GetStaffManagementSnapshot();
	TestTrue(TEXT("개장 대기 집계 때문에 스냅샷 불변식이 깨지지 않는다"), Morning.bAvailable);
	TestEqual(TEXT("배정 인원 유지"), Morning.Assigned, 2);
	TestEqual(TEXT("정상 마감한 두 명은 개장 대기"), Morning.AwaitingOpening, 2);
	TestEqual(TEXT("정상 아침 점검에 출동 불가 경고를 만들지 않는다"), Morning.Unavailable, 0);
	TestEqual(TEXT("개장 대기를 출동 가능 수로 부풀리지 않는다"), Morning.Dispatchable, 0);
	if (!TestEqual(TEXT("두 직원 표시 행 유지"), Morning.Members.Num(), 2)
		|| !TestEqual(TEXT("시설 행 유지"), Morning.Facilities.Num(), 1)) return false;
	TestEqual(TEXT("직원 상태"), Morning.Members[0].Duty, EFF_StaffDuty::AwaitingOpening);
	TestEqual(TEXT("직원 문구"), Morning.Members[0].DutyLabel.ToString(), FString(TEXT("개장 대기")));
	TestEqual(TEXT("시설별 개장 대기도 보존"), Morning.Facilities[0].AwaitingOpening, 2);
	TestEqual(TEXT("시설별 실제 출동 대기는 0"), Morning.Facilities[0].Ready, 0);
	FFF_StaffDispatchOrigin BlockedOrigin;
	TestFalse(TEXT("UI를 읽어도 원본 출동 불가 판정은 그대로"), H.Staffing->GetStaffDispatchOrigin(1, BlockedOrigin));

	H.Stages->CurrentStage = EFF_FestivalStage::Setup;
	TestEqual(TEXT("셋업에서도 같은 정상 마감 상태는 개장 대기"), H.ViewModel->GetStaffManagementSnapshot().AwaitingOpening, 2);
	H.Stages->CurrentStage = EFF_FestivalStage::Live;
	const FFF_StaffManagementSnapshot StillClosed = H.ViewModel->GetStaffManagementSnapshot();
	TestEqual(TEXT("Live에 아직 닫힌 시설은 경고를 유지"), StillClosed.Unavailable, 2);
	TestEqual(TEXT("Live에서는 개장 대기로 숨기지 않는다"), StillClosed.AwaitingOpening, 0);
	if (!TestTrue(TEXT("다음 개장"), H.ChangeHome(EFFZSCommandType::Open))) return false;
	TestEqual(TEXT("실제 개장 후에만 출동 가능 인원이 돌아온다"), H.ViewModel->GetStaffManagementSnapshot().Dispatchable, 2);

	TArray<FFF_StaffDispatchOrigin> Origins;
	if (!TestTrue(TEXT("원본 예약 한 명"), H.Staffing->TryReserveStaff(1, FVector::ZeroVector, Origins))
		|| !TestEqual(TEXT("예약 출발점 한 개"), Origins.Num(), 1)) return false;
	TestEqual(TEXT("몸이 없어도 예약 인원은 출동 중으로 유지"), H.ViewModel->GetStaffManagementSnapshot().Busy, 1);
	FFF_IncidentAnchor Target;
	Target.Kind = EFF_IncidentAnchorKind::WorldPoint;
	Target.WorldPoint = Origins[0].Location + FVector(1000.f, 0.f, 0.f);
	if (!TestTrue(TEXT("실제 출동 몸 생성"), H.Bodies->SendStaff(Origins[0], Target))) return false;
	H.Bodies->RecallStaff(Origins[0].StaffId);
	if (!TestTrue(TEXT("복귀 중 정상 폐장"), H.ChangeHome(EFFZSCommandType::Close))) return false;
	H.Stages->CurrentStage = EFF_FestivalStage::MorningCheck;
	const FFF_StaffManagementSnapshot Returning = H.ViewModel->GetStaffManagementSnapshot();
	TestTrue(TEXT("복귀 인원과 개장 대기가 섞여도 집계 일치"), Returning.bAvailable);
	TestEqual(TEXT("복귀 인원을 개장 대기로 바꾸지 않는다"), Returning.Returning, 1);
	TestEqual(TEXT("예약되지 않은 나머지만 개장 대기"), Returning.AwaitingOpening, 1);
	TestEqual(TEXT("복귀는 Busy와 중복 집계하지 않는다"), Returning.Busy, 0);
	H.Bodies->DespawnStaff(Origins[0].StaffId);
	H.Staffing->ReleaseStaff(TArray<int32>{ Origins[0].StaffId });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffManagementOpeningFailureTest,
	"FestaFiesta.UI.StaffManagement.OpeningKeepsRealFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_StaffManagementOpeningFailureTest::RunTest(const FString& Parameters)
{
	FFStaffManagementTests::FHarness H;
	if (!TestTrue(TEXT("실제 명단·시설·ViewModel 픽스처 생성"), H.Initialize(*this))) return false;
	if (!TestTrue(TEXT("안내소 개장 후 정상 폐장"), H.ChangeHome(EFFZSCommandType::Open) && H.ChangeHome(EFFZSCommandType::Close))) return false;
	H.Stages->CurrentStage = EFF_FestivalStage::MorningCheck;
	if (!TestTrue(TEXT("재배치 필요 상태를 공개 저장 경계로 복원"), H.RestoreRelocation(true))) return false;
	TestEqual(TEXT("재배치가 필요한 시설은 정상 대기로 숨기지 않는다"), H.ViewModel->GetStaffManagementSnapshot().Unavailable, 2);
	if (!TestTrue(TEXT("정상 위치 상태를 공개 저장 경계로 복원"), H.RestoreRelocation(false))) return false;

	// 사양은 유효하지만 런타임에서 원본 출발 앵커를 읽지 못하는 고장 경로를 만든다.
	TArray<UFFZSFacilityAnchorComponent*> Anchors;
	H.Home->GetComponents(Anchors);
	for (UFFZSFacilityAnchorComponent* Anchor : Anchors)
	{
		if (Anchor->AnchorType == EFFZSAnchorType::Exit) Anchor->AnchorIndex = 10;
	}
	TestEqual(TEXT("출구가 없더라도 유효한 입구 대체 경로는 허용"), H.ViewModel->GetStaffManagementSnapshot().AwaitingOpening, 2);
	for (UFFZSFacilityAnchorComponent* Anchor : Anchors)
	{
		if (Anchor->AnchorType == EFFZSAnchorType::Entry) Anchor->AnchorIndex = 10;
	}
	const FFF_StaffManagementSnapshot MissingAnchors = H.ViewModel->GetStaffManagementSnapshot();
	TestTrue(TEXT("출입구 누락도 읽을 수 있는 스냅샷"), MissingAnchors.bAvailable);
	TestEqual(TEXT("출입구 둘 다 누락되면 경고 유지"), MissingAnchors.Unavailable, 2);
	TestEqual(TEXT("출입구 누락은 개장 대기 0"), MissingAnchors.AwaitingOpening, 0);
	if (!TestEqual(TEXT("문제 시설도 화면에서 유지"), MissingAnchors.Facilities.Num(), 1)) return false;
	TestEqual(TEXT("시설별 경고 인원 유지"), MissingAnchors.Facilities[0].Unavailable, 2);

	if (!TestTrue(TEXT("문제 시설을 공개 명령으로 철거"), H.ChangeHome(EFFZSCommandType::Remove))) return false;
	const FFF_StaffManagementSnapshot Removed = H.ViewModel->GetStaffManagementSnapshot();
	TestTrue(TEXT("시설 제거 후에도 원본 소속 정리를 중계"), Removed.bAvailable);
	TestEqual(TEXT("없는 시설은 개장 대기라고 표시하지 않는다"), Removed.AwaitingOpening, 0);
	TestEqual(TEXT("원본이 해제한 배정은 미배정으로 보인다"), Removed.Unassigned, 2);
	return true;
}

#endif
