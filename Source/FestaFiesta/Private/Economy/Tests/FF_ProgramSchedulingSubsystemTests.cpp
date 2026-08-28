// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ProgramSchedulingSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"

namespace FFF_ProgramSchedulingCommandTestHelpers
{
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	const FName CostKey(TEXT("Cost.Booth.CraftExperience.ProgramSchedulingAutomation"));
	const FName Workshop(TEXT("Program_SandArtWorkshop"));
	const FName Yoga(TEXT("Program_SunsetYoga"));
	const FName Overlap(TEXT("Program_OverlapAutomation"));
	const FName OpenFacilityCard(TEXT("Program_OpenFacilityAutomation"));

	class FScopedProgramSchedulingSettings
	{
	public:
		FScopedProgramSchedulingSettings()
			: Settings(GetMutableDefault<UFFZSDeveloperSettings>())
			, bGroundChecks(Settings->bEnableGroundChecks)
			, bWorldCollisionChecks(Settings->bEnableWorldCollisionChecks)
			, bNavigationChecks(Settings->bEnableNavigationProjectionChecks)
			, bProductionPolicies(Settings->bRequireProductionPolicyDomains)
			, bRestrictZoneIds(Settings->bRestrictZoneIdsToProjectPresets)
			, InitialPhase(Settings->InitialPhase)
			, MaxEnumeratedCells(Settings->MaxEnumeratedCellsPerFacility)
		{
			Settings->bEnableGroundChecks = false;
			Settings->bEnableWorldCollisionChecks = false;
			Settings->bEnableNavigationProjectionChecks = false;
			Settings->bRequireProductionPolicyDomains = false;
			Settings->bRestrictZoneIdsToProjectPresets = false;
			Settings->InitialPhase = EFFZSChangePhase::InitialSetup;
			Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
		}

		~FScopedProgramSchedulingSettings()
		{
			Settings->bEnableGroundChecks = bGroundChecks;
			Settings->bEnableWorldCollisionChecks = bWorldCollisionChecks;
			Settings->bEnableNavigationProjectionChecks = bNavigationChecks;
			Settings->bRequireProductionPolicyDomains = bProductionPolicies;
			Settings->bRestrictZoneIdsToProjectPresets = bRestrictZoneIds;
			Settings->InitialPhase = InitialPhase;
			Settings->MaxEnumeratedCellsPerFacility = MaxEnumeratedCells;
		}

	private:
		UFFZSDeveloperSettings* Settings;
		bool bGroundChecks;
		bool bWorldCollisionChecks;
		bool bNavigationChecks;
		bool bProductionPolicies;
		bool bRestrictZoneIds;
		EFFZSChangePhase InitialPhase;
		int32 MaxEnumeratedCells;
	};

	struct FProgramSchedulingHarness
	{
		bool Initialize(FAutomationTestBase* Test)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(Test);
				return false;
			}

			World = TestWorld.GetTestWorld();
			GameInstance = World ? World->GetGameInstance() : nullptr;
			Zone = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
			Scheduling = World ? World->GetSubsystem<UFF_ProgramSchedulingSubsystem>() : nullptr;
			Progression = GameInstance
				? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
				: nullptr;
			Reservations = GameInstance
				? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>()
				: nullptr;
			Stage = GameInstance
				? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
				: nullptr;
			if (!World || !GameInstance || !Zone || !Scheduling || !Progression
				|| !Reservations || !Stage)
			{
				return false;
			}

			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			if (!Definition || !CostTable)
			{
				return false;
			}

			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			FFF_FacilityCostRow CostRow;
			CostRow.InstallCost = 100;
			CostTable->AddRow(CostKey, CostRow);
			Definition->FestivalId = TEXT("ProgramSchedulingCommandAutomationFestival");
			Definition->InitialGold = 10000;
			Definition->InitialStaffCount = 10;
			Definition->DailyVenueRentGold = 0;
			Definition->FacilityCostTable = CostTable;

			const FFF_ProgramCard* WorkshopCard = Definition->ProgramCatalog.FindByPredicate(
				[](const FFF_ProgramCard& Card) { return Card.ProgramId == Workshop; });
			if (WorkshopCard == nullptr)
			{
				return false;
			}

			FFF_ProgramCard OverlapCard = *WorkshopCard;
			OverlapCard.ProgramId = Overlap;
			OverlapCard.DisplayName = FText::FromString(TEXT("겹침 자동화"));
			OverlapCard.StartMinutes = WorkshopCard->StartMinutes + 5.f;
			OverlapCard.EndMinutes = WorkshopCard->EndMinutes + 5.f;
			Definition->ProgramCatalog.Add(OverlapCard);

			FFF_ProgramCard OpenCard = *WorkshopCard;
			OpenCard.ProgramId = OpenFacilityCard;
			OpenCard.DisplayName = FText::FromString(TEXT("시설 제한 없음 자동화"));
			OpenCard.StartMinutes = 150.f;
			OpenCard.EndMinutes = 180.f;
			OpenCard.AllowedFacilityTypes.Reset();
			Definition->ProgramCatalog.Add(OpenCard);

			Progression->StartFestival(Definition);

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AFFZSGameplayZone* GameplayZone = World->SpawnActor<AFFZSGameplayZone>(
				AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!GameplayZone || !GameplayZone->ApplyAcceptedZonePreset(ZoneId)
				|| !Zone->RegisterZone(GameplayZone))
			{
				return false;
			}

			FFFZSFacilitySpec Spec =
				FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
			Spec.FacilityDefinitionId = TEXT("Booth_CraftExperience_ProgramSchedulingAutomation");
			Spec.DisplayName = FText::FromString(TEXT("프로그램 자동화 공방"));
			Spec.InstallCostKey = CostKey;
			FFFZSGridPlacement Placement;
			Placement.OriginCell = FIntPoint(1000, 1000);
			InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
				Spec, ZoneId, Placement, Zone->GetWorldRevision());
			const FFFZSCommandResult InstallResult = Zone->ExecuteCommand(InstallCommand);
			if (!InstallResult.bSucceeded)
			{
				Test->AddError(FString::Printf(
					TEXT("프로그램 테스트 시설 설치 실패: %s"),
					*InstallResult.Message.ToString()));
				return false;
			}

			Facility = Zone->FindFacility(InstallCommand.FacilityInstanceId);
			return Facility != nullptr
				&& Facility->GetFacilityState() == EFFZSFacilityState::Validated;
		}

		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		UGameInstance* GameInstance = nullptr;
		UFFZSWorldSubsystem* Zone = nullptr;
		UFF_ProgramSchedulingSubsystem* Scheduling = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_ProgramReservationSubsystem* Reservations = nullptr;
		UFF_FestivalStageSubsystem* Stage = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
		AFFZSFacilityActor* Facility = nullptr;
		FFFZSCommand InstallCommand;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_ProgramSchedulingSubsystem_AuthoritativeCommand,
	"FestaFiesta.Economy.ProgramScheduling.AuthoritativeCommand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_ProgramSchedulingSubsystem_AuthoritativeCommand::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramSchedulingCommandTestHelpers;
	(void)Parameters;

	FScopedProgramSchedulingSettings SettingsGuard;
	FProgramSchedulingHarness Harness;
	if (!TestTrue(TEXT("실제 Game World 명령 픽스처가 구성된다"), Harness.Initialize(this)))
	{
		return false;
	}

	const FGuid FacilityId = Harness.InstallCommand.FacilityInstanceId;
	const FGuid StaleFacilityId = FGuid::NewGuid();
	TestEqual(TEXT("등록되지 않은 실제 시설 GUID는 최종 경계에서 거절된다"),
		Harness.Scheduling->CanScheduleProgramAtFacility(OpenFacilityCard, StaleFacilityId),
		EFF_PaidProgramScheduleResult::FacilityNotFound);
	TestEqual(TEXT("시설 종류가 다른 카드는 거절된다"),
		Harness.Scheduling->CanScheduleProgramAtFacility(Yoga, FacilityId),
		EFF_PaidProgramScheduleResult::FacilityTypeMismatch);

	TestTrue(TEXT("Validated 시설은 편성 가능한 상태다"),
		UFF_ProgramSchedulingSubsystem::IsFacilityStateSchedulable(EFFZSFacilityState::Validated));
	TestTrue(TEXT("Closed 시설은 편성 가능한 상태다"),
		UFF_ProgramSchedulingSubsystem::IsFacilityStateSchedulable(EFFZSFacilityState::Closed));
	TestFalse(TEXT("이동 중 시설에는 편성하지 않는다"),
		UFF_ProgramSchedulingSubsystem::IsFacilityStateSchedulable(EFFZSFacilityState::MovePending));
	TestFalse(TEXT("운영 중 시설에는 편성하지 않는다"),
		UFF_ProgramSchedulingSubsystem::IsFacilityStateSchedulable(EFFZSFacilityState::Active));
	TestFalse(TEXT("설치 전 시설에는 편성하지 않는다"),
		UFF_ProgramSchedulingSubsystem::IsFacilityStateSchedulable(EFFZSFacilityState::Planned));
	TestFalse(TEXT("철거된 시설에는 편성하지 않는다"),
		UFF_ProgramSchedulingSubsystem::IsFacilityStateSchedulable(EFFZSFacilityState::Removed));

	Harness.Stage->CurrentStage = EFF_FestivalStage::Live;
	TestEqual(TEXT("Live 단계에서는 편성할 수 없다"),
		Harness.Scheduling->CanScheduleProgramAtFacility(Workshop, FacilityId),
		EFF_PaidProgramScheduleResult::NotSchedulingStage);
	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;

	const int32 GoldBeforeProgram = Harness.Progression->GetGold();
	TestEqual(TEXT("Setup의 실제 Validated 호환 시설에는 편성된다"),
		Harness.Scheduling->TryScheduleProgramAtFacility(Workshop, FacilityId),
		EFF_PaidProgramScheduleResult::Success);
	TestTrue(TEXT("시설별 오늘 편성 조회가 참이다"),
		Harness.Scheduling->HasScheduledProgramForFacility(FacilityId));
	TestEqual(TEXT("시설별 오늘 편성 목록에 한 건이 있다"),
		Harness.Scheduling->GetScheduledProgramsForFacility(FacilityId).Num(), 1);

	const int32 GoldAfterProgram = Harness.Progression->GetGold();
	TestTrue(TEXT("성공한 구매에서만 Gold가 줄었다"), GoldAfterProgram < GoldBeforeProgram);
	TestEqual(TEXT("겹치는 같은 시설 프로그램은 거절된다"),
		Harness.Scheduling->TryScheduleProgramAtFacility(Overlap, FacilityId),
		EFF_PaidProgramScheduleResult::FacilityTimeConflict);
	TestEqual(TEXT("충돌 실패 뒤 Gold가 남지 않고 추가 차감되지 않는다"),
		Harness.Progression->GetGold(), GoldAfterProgram);
	TestEqual(TEXT("충돌 실패 뒤 예약은 하나뿐이다"),
		Harness.Reservations->GetReservationIds().Num(), 1);
	TestEqual(TEXT("충돌 실패 뒤 편성도 하나뿐이다"),
		Harness.Progression->GetScheduledPrograms().Num(), 1);
	TestEqual(TEXT("같은 프로그램을 다시 편성하면 중복으로 거절된다"),
		Harness.Scheduling->TryScheduleProgramAtFacility(Workshop, FacilityId),
		EFF_PaidProgramScheduleResult::AlreadyScheduled);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
