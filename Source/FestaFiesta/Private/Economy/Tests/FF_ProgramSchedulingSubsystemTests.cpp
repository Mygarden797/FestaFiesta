// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ProgramSchedulingSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "DataTables/FF_FacilityRevenueTableRow.h"
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
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UObject/StrongObjectPtr.h"

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
			Definition->FacilityRevenueTable.Reset();

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

		int32 ConfirmAllReservedParties()
		{
			int32 Confirmed = 0;
			int32 PartyId = 1;
			for (const FFF_ReservedPartyRequest& Request : Progression->GetReservedPartyRequests())
			{
				if (Reservations->ConfirmReservedParty(Request.ReservationId, PartyId++, Request.GroupSize))
				{
					Confirmed += Request.GroupSize;
				}
			}
			return Confirmed;
		}

		bool FindEffectiveCard(const FGuid& FacilityId, FName ProgramId, FFF_ProgramCard& OutCard) const
		{
			const TArray<FFF_ProgramCard> Catalog = Progression->GetProgramCatalogForFacility(FacilityId);
			const FFF_ProgramCard* Found = Catalog.FindByPredicate([&](const FFF_ProgramCard& Card) { return Card.ProgramId == ProgramId; });
			if (!Found) return false;
			OutCard = *Found;
			return true;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedulingSubsystem_TableFeesRemainLive,
	"FestaFiesta.Economy.ProgramScheduling.TableFeesRemainLive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_ProgramSchedulingSubsystem_TableFeesRemainLive::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_ProgramSchedulingCommandTestHelpers;
	FScopedProgramSchedulingSettings SettingsGuard;
	FProgramSchedulingHarness Harness;
	if (!TestTrue(TEXT("Program table test world initializes"), Harness.Initialize(this))) return false;
	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;
	const FGuid FirstId = Harness.InstallCommand.FacilityInstanceId;
	const FName FirstDefinitionId = Harness.Facility->GetFacilitySpec().FacilityDefinitionId;
	FFFZSFacilitySpec SecondSpec = Harness.Facility->GetFacilitySpec();
	SecondSpec.FacilityDefinitionId = TEXT("Booth_CraftExperience_ProgramFeeSibling");
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(2500, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(SecondSpec, ZoneId, Placement, Harness.Zone->GetWorldRevision());
	if (!TestTrue(TEXT("A sibling facility in the same category installs"), Harness.Zone->ExecuteCommand(Install).bSucceeded)) return false;
	const FGuid SecondId = Install.FacilityInstanceId;
	TStrongObjectPtr<UDataTable> Revenues(NewObject<UDataTable>());
	Revenues->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	FFF_FacilityRevenueTableRow FirstRow;
	FirstRow.ProgramFeesPerPerson.Add(Workshop, 40);
	Revenues->AddRow(FirstDefinitionId, FirstRow);
	FFF_FacilityRevenueTableRow SecondRow;
	SecondRow.ProgramFeesPerPerson.Add(Workshop, 55);
	SecondRow.ProgramFeesPerPerson.Add(Overlap, 0);
	Revenues->AddRow(SecondSpec.FacilityDefinitionId, SecondRow);
	Harness.Definition->FacilityRevenueTable = Revenues.Get();
	FFF_ProgramCard FirstCard, SecondCard, FreeCard;
	if (!TestTrue(TEXT("First facility's UI catalog has its workshop"), Harness.FindEffectiveCard(FirstId, Workshop, FirstCard))
		|| !TestTrue(TEXT("Second facility's UI catalog has the same workshop"), Harness.FindEffectiveCard(SecondId, Workshop, SecondCard))
		|| !TestTrue(TEXT("The free program is retained in the facility's catalog"), Harness.FindEffectiveCard(SecondId, Overlap, FreeCard))) return false;
	TestEqual(TEXT("The first facility quotes its own program admission fee"), FirstCard.FeePerPerson, 40);
	TestEqual(TEXT("The same program has an independent fee at the sibling facility"), SecondCard.FeePerPerson, 55);
	TestEqual(TEXT("Free admission is presented as zero, not the global paid fee"), FreeCard.FeePerPerson, 0);
	const FFF_ProgramCard* DefaultWorkshop = Harness.Definition->ProgramCatalog.FindByPredicate([](const FFF_ProgramCard& Card) { return Card.ProgramId == Workshop; });
	if (!TestNotNull(TEXT("The shared program template still exists"), DefaultWorkshop)) return false;
	TestEqual(TEXT("Facility quotes never mutate the shared program template"), DefaultWorkshop->FeePerPerson, 30);
	TestEqual(TEXT("The effective quote preserves program capacity"), FirstCard.Capacity, DefaultWorkshop->Capacity);
	TestEqual(TEXT("The effective quote preserves setup cost"), FirstCard.CostGold, DefaultWorkshop->CostGold);
	if (!TestEqual(TEXT("The first facility schedules its paid program"), Harness.Scheduling->TryScheduleProgramAtFacility(Workshop, FirstId), EFF_PaidProgramScheduleResult::Success)
		|| !TestEqual(TEXT("The sibling schedules a zero-fee program through the real boundary"), Harness.Scheduling->TryScheduleProgramAtFacility(Overlap, SecondId), EFF_PaidProgramScheduleResult::Success)) return false;
	const TArray<FFF_ScheduledProgram> Scheduled = Harness.Progression->GetScheduledPrograms();
	if (!TestEqual(TEXT("Both programs have reservations"), Scheduled.Num(), 2)) return false;
	TestEqual(TEXT("The paid schedule retains its facility definition for later table lookup"), Scheduled[0].FacilityDefinitionId, FirstDefinitionId);
	TestEqual(TEXT("The free schedule retains its own facility definition"), Scheduled[1].FacilityDefinitionId, SecondSpec.FacilityDefinitionId);
	TestEqual(TEXT("Confirmed program guests fill both reservations"), Harness.ConfirmAllReservedParties(), FirstCard.Capacity + FreeCard.Capacity);
	const int32 GoldBefore = Harness.Progression->GetGold();
	FirstRow.ProgramFeesPerPerson.Add(Workshop, 70);
	Revenues->AddRow(FirstDefinitionId, FirstRow);
	FFF_ProgramCard ChangedQuote;
	if (!TestTrue(TEXT("The facility quote refreshes after a scheduled fee edit"), Harness.FindEffectiveCard(FirstId, Workshop, ChangedQuote))) return false;
	TestEqual(TEXT("The quote shows the newly edited fee immediately"), ChangedQuote.FeePerPerson, 70);
	const FFF_PrepProgramQuote UiQuote = FFF_PrepProgramRules::BuildQuote(&ChangedQuote,
		EFF_ProgramPurchaseResult::Success, true, 1, GoldBefore, 10);
	TestEqual(TEXT("The preparation panel's projection uses the effective facility fee"), UiQuote.MaxRevenue, ChangedQuote.Capacity * 70);
	const FFF_PrepProgramQuote FreeUiQuote = FFF_PrepProgramRules::BuildQuote(&FreeCard,
		EFF_ProgramPurchaseResult::Success, true, 1, GoldBefore, 10);
	TestEqual(TEXT("A free facility program is quoted as zero revenue"), FreeUiQuote.MaxRevenue, 0);
	TestEqual(TEXT("Fee edits do not themselves create revenue"), Harness.Progression->GetGold(), GoldBefore);
	const int32 Expected = FirstCard.Capacity * 70;
	TestEqual(TEXT("At program start the new fee supersedes the value from scheduling time"),
		Harness.Progression->SettleStartedPrograms(FirstCard.StartMinutes), Expected);
	TestEqual(TEXT("Only the accepted program fee is credited to funds"), Harness.Progression->GetGold(), GoldBefore + Expected);
	FFF_FacilityInstanceRevenue FirstLedger;
	TestTrue(TEXT("Program income belongs to its actual installed facility"), Harness.Progression->GetFacilityInstanceRevenue(FirstId, FirstLedger));
	TestEqual(TEXT("The installation ledger agrees with the edited program price"), FirstLedger.TodayGold, Expected);
	TestEqual(TEXT("The program ledger agrees with the installation ledger"), Harness.Progression->GetDayLedger().GetEarnByReason(EFF_EarnReason::ProgramFee), Expected);
	TestEqual(TEXT("A full free program settles without a paid fallback"), Harness.Progression->SettleStartedPrograms(FreeCard.StartMinutes), 0);
	FFF_FacilityInstanceRevenue FreeLedger;
	TestFalse(TEXT("Free attendance does not fabricate facility income"), Harness.Progression->GetFacilityInstanceRevenue(SecondId, FreeLedger));
	SecondRow.ProgramFeesPerPerson.Add(Overlap, 100);
	Revenues->AddRow(SecondSpec.FacilityDefinitionId, SecondRow);
	TestEqual(TEXT("Increasing a fee after free settlement cannot bill the guests again"), Harness.Progression->SettleStartedPrograms(FreeCard.StartMinutes + 1.f), 0);
	TestEqual(TEXT("The closing fallback also cannot charge a settled free program"), Harness.Progression->SettleScheduledPrograms(), 0);
	TestEqual(TEXT("Changing a completed fee leaves historical funds unchanged"), Harness.Progression->GetGold(), GoldBefore + Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedulingSubsystem_TableFeesRespectFundsCapacity,
	"FestaFiesta.Economy.ProgramScheduling.TableFeesRespectFundsCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_ProgramSchedulingSubsystem_TableFeesRespectFundsCapacity::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_ProgramSchedulingCommandTestHelpers;
	FScopedProgramSchedulingSettings SettingsGuard;
	FProgramSchedulingHarness Harness;
	if (!TestTrue(TEXT("Large program fee world initializes"), Harness.Initialize(this))) return false;
	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;
	TStrongObjectPtr<UDataTable> Revenues(NewObject<UDataTable>());
	Revenues->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	FFF_FacilityRevenueTableRow Row;
	Row.ProgramFeesPerPerson.Add(Workshop, MAX_int32);
	Revenues->AddRow(Harness.Facility->GetFacilitySpec().FacilityDefinitionId, Row);
	Harness.Definition->FacilityRevenueTable = Revenues.Get();
	Harness.Definition->InitialGold = MAX_int32 - 100;
	Harness.Progression->StartFestival(Harness.Definition);
	const FGuid FacilityId = Harness.InstallCommand.FacilityInstanceId;
	FFF_ProgramCard Card;
	if (!TestTrue(TEXT("The effective maximum-price program quote is available"), Harness.FindEffectiveCard(FacilityId, Workshop, Card))) return false;
	TestEqual(TEXT("Projected capacity times fee saturates without integer overflow"), Card.CalcMaxRevenue(), MAX_int32);
	if (!TestEqual(TEXT("A high-value program can still be scheduled"), Harness.Scheduling->TryScheduleProgramAtFacility(Workshop, FacilityId), EFF_PaidProgramScheduleResult::Success)) return false;
	TestEqual(TEXT("The full capacity is confirmed"), Harness.ConfirmAllReservedParties(), Card.Capacity);
	const int32 Expected = MAX_int32 - Harness.Progression->GetGold();
	TestEqual(TEXT("Settlement returns the accepted amount, not the overflowing request"), Harness.Progression->SettleStartedPrograms(Card.StartMinutes), Expected);
	TestEqual(TEXT("Program income saturates the balance safely"), Harness.Progression->GetGold(), MAX_int32);
	TestEqual(TEXT("Reported revenue equals the amount actually credited"), Harness.Progression->GetTodayRevenue(), Expected);
	TestEqual(TEXT("Program ledger records only accepted funds"), Harness.Progression->GetDayLedger().GetEarnByReason(EFF_EarnReason::ProgramFee), Expected);
	TestEqual(TEXT("An already settled saturated program cannot pay twice"), Harness.Progression->SettleStartedPrograms(Card.StartMinutes + 1.f), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
