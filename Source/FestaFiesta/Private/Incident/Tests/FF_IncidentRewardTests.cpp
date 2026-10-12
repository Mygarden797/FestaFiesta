#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Incident/Effects/FF_IncidentEffect_Reward.h"
#include "Incident/FF_IncidentDefinition.h"
#include "Incident/FF_IncidentTags.h"
#include "Incident/Tests/FF_IncidentRewardTestObserver.h"
#include "Save/FF_SaveArchive.h"
#include "Tests/AutomationCommon.h"
#include "UI/FF_UIDisplayTextLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRewardReceiptTest,
	"FestaFiesta.Incident.Rewards.ActualReceiptAndSingleCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentRewardReceiptTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* GI = World->GetGameInstance();
	UFF_FestivalProgressionSubsystem* Progression = GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	UFF_IncidentSubsystem* Incident = World->GetSubsystem<UFF_IncidentSubsystem>();
	if (!TestNotNull(TEXT("경제 원본"), Progression) || !TestNotNull(TEXT("사건 원본"), Incident)) return false;

	UFF_FestivalDefinition* Festival = NewObject<UFF_FestivalDefinition>(GI);
	Festival->FestivalId = TEXT("IncidentRewardAutomationFestival");
	UDataTable* Costs = NewObject<UDataTable>(GI);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost;
	Cost.InstallCost = 100;
	Costs->AddRow(TEXT("Cost.Info.IncidentRewardAutomation"), Cost);
	Festival->FacilityCostTable = Costs;
	Festival->InitialGold = 1000;
	Festival->InitialStaffCount = 0;
	Progression->StartFestival(Festival);
	UFF_IncidentDefinition* Definition = NewObject<UFF_IncidentDefinition>(Incident);
	Definition->IncidentTag = FFIncidentTags::Crowd::MemberMissing;
	Definition->Title = FText::FromString(TEXT("사건 보상 검사"));
	UFF_IncidentOption* Option = NewObject<UFF_IncidentOption>(Definition);
	UFF_IncidentEffect_Reward* Reward = NewObject<UFF_IncidentEffect_Reward>(Option);
	Reward->RewardGold = 100;
	Option->OnResolvedEffect.Add(Reward);
	Definition->Options.Add(Option);
	Incident->CachedDef.Add(Definition->IncidentTag, Definition);
	UFF_IncidentRewardTestObserver* Observer = NewObject<UFF_IncidentRewardTestObserver>(GI);
	Observer->Progression = Progression;
	Observer->Incident = Incident;
	Progression->OnGoldChanged.AddDynamic(Observer, &UFF_IncidentRewardTestObserver::HandleGold);
	Incident->OnIncidentClosed.AddDynamic(Observer, &UFF_IncidentRewardTestObserver::HandleClosed);
	Incident->OnIncidentFinished.AddDynamic(Observer, &UFF_IncidentRewardTestObserver::HandleFinished);

	auto Open = [&]()
	{
		FFF_IncidentHandle Handle;
		Handle.Index = Incident->IncidentSlots.AddDefaulted();
		auto& Slot = Incident->IncidentSlots[Handle.Index];
		Slot.bInUse = true;
		Slot.IncidentTag = Definition->IncidentTag;
		Slot.DedupeKey = FName(*FString::Printf(TEXT("RewardTest%d"), Handle.Index));
		Slot.Anchor.Kind = EFF_IncidentAnchorKind::WorldPoint;
		Incident->ActiveByKey.Add(Slot.DedupeKey, Handle);
		return Handle;
	};

	const FFF_IncidentHandle Completed = Open();
	Observer->CurrentHandle = Completed;
	Observer->bRunNestedRevenue = true;
	TestTrue(TEXT("현장 작업 완료"), Incident->CompleteStaffResponse(Completed, 0));
	TestFalse(TEXT("지급 델리게이트 전에 핸들을 무효화"), Observer->bHandleValidDuringPayment);
	TestEqual(TEXT("재진입 수입을 사건 영수증에 합산하지 않음"), Observer->LastInfo.GrantedGold, 100);
	TestEqual(TEXT("실제 잔액은 별도 거래까지 반영"), Progression->GetGold(), 1107);
	TestEqual(TEXT("확정 종료 알림 한 번"), Observer->FinishedCount, 1);
	TestFalse(TEXT("동일 완료 핸들 재요청은 지급 없음"), Incident->CompleteStaffResponse(Completed, 0));
	TestEqual(TEXT("재요청 후 매출 불변"), Progression->GetTotalRevenue(), 107);
	FFF_ProgressionSave Saved;
	TestTrue(TEXT("종료 알림 안에서 저장한 원장 읽기"), UFF_SaveArchive::ReadStruct(Observer->SavedAtFinished, Saved));
	TestEqual(TEXT("저장 잔액"), Saved.Gold, 1107);
	TestEqual(TEXT("저장 누적 수입"), Saved.TotalRevenue, 107);
	const int32 ReasonIndex = static_cast<int32>(EFF_EarnReason::IncidentReward);
	if (TestTrue(TEXT("사건 보상 원장 항목 있음"), Saved.CurrentLedger.EarnByReason.IsValidIndex(ReasonIndex)))
	{
		TestEqual(TEXT("보상 원장과 누적 수입 일치"), Saved.CurrentLedger.EarnByReason[ReasonIndex], 107);
	}
	TestTrue(TEXT("종료 안내는 실제 지급액 표시"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(Observer->LastInfo).ToString().Contains(TEXT("금화 +100")));

	const FFF_IncidentHandle Natural = Open();
	Incident->CloseSlot(Natural.Index, EFF_IncidentState::Expired, 0.f, EFF_IncidentCloseReason::NaturalResolution);
	TestEqual(TEXT("자연 해소 지급 없음"), Observer->LastInfo.GrantedGold, 0);
	const FFF_IncidentHandle TimedOut = Open();
	Incident->LapseIncident(TimedOut, 60.f);
	TestEqual(TEXT("방치 지급 없음"), Observer->LastInfo.GrantedGold, 0);
	TestEqual(TEXT("자연 해소와 방치가 성공 보상을 실행하지 않음"), Progression->GetGold(), 1107);
	TestFalse(TEXT("방치된 핸들 완료 재요청 거부"), Incident->CompleteStaffResponse(TimedOut, 0));

	// Dev의 단계 종료 정리와 기존 보상·종료 알림 처리가 함께 있어도 정리 자체는 지급하지 않는다.
	const FFF_IncidentHandle ActiveAtClose = Open();
	const FFF_IncidentHandle RespondingAtClose = Open();
	Incident->IncidentSlots[RespondingAtClose.Index].State = EFF_IncidentState::Acknowledged;
	Incident->IncidentSlots[RespondingAtClose.Index].ChosenOptionIndex = 0;
	Definition->OnFailedEffect.Add(Reward);
	const int32 ClosedBeforeStage = Observer->ClosedCount;
	const int32 FinishedBeforeStage = Observer->FinishedCount;
	Incident->HandleStageChanged(EFF_FestivalStage::Setup, EFF_FestivalStage::Live);
	TestTrue(TEXT("라이브 진입은 사건을 닫지 않음"), Incident->IsHandleValid(ActiveAtClose));
	Incident->HandleStageChanged(EFF_FestivalStage::Live, EFF_FestivalStage::Closing);
	TestFalse(TEXT("운영 종료는 미처리 사건 정리"), Incident->IsHandleValid(ActiveAtClose));
	TestFalse(TEXT("운영 종료는 출동 중 사건도 정리"), Incident->IsHandleValid(RespondingAtClose));
	TestEqual(TEXT("마커·스태프 정리용 종료 신호는 각각 전달"), Observer->ClosedCount, ClosedBeforeStage + 2);
	TestEqual(TEXT("운영 종료에 완료·방치 안내를 추가하지 않음"), Observer->FinishedCount, FinishedBeforeStage);
	TestEqual(TEXT("운영 종료는 성공·실패 효과 모두 실행하지 않음"), Progression->GetGold(), 1107);
	TestTrue(TEXT("다음 운영을 위해 중복 키 해제"), Incident->ActiveByKey.IsEmpty());
	TestFalse(TEXT("운영이 끝난 출동의 늦은 완료도 보상 없음"), Incident->CompleteStaffResponse(RespondingAtClose, 0));
	Incident->HandleStageChanged(EFF_FestivalStage::Closing, EFF_FestivalStage::Report);
	TestEqual(TEXT("다음 단계에서 같은 사건을 다시 닫지 않음"), Observer->ClosedCount, ClosedBeforeStage + 2);
	Definition->OnFailedEffect.Reset();

	Festival->InitialGold = MAX_int32 - 40;
	Progression->StartFestival(Festival);
	const FFF_IncidentHandle Capped = Open();
	TestTrue(TEXT("잔액 상한 근처 완료"), Incident->CompleteStaffResponse(Capped, 0));
	TestEqual(TEXT("상한 수용액만 영수증에 표시"), Observer->LastInfo.GrantedGold, 40);
	TestEqual(TEXT("상한 수용액만 원장에 기록"), Progression->GetTotalRevenue(), 40);
	TestEqual(TEXT("잔액 상한"), Progression->GetGold(), MAX_int32);
	const FFF_IncidentHandle Full = Open();
	TestTrue(TEXT("잔액 상한이어도 사건 완료"), Incident->CompleteStaffResponse(Full, 0));
	TestEqual(TEXT("수용액 없으면 보상 표시 없음"), Observer->LastInfo.GrantedGold, 0);
	TestFalse(TEXT("0 지급인데 금화 보상 문구를 만들지 않음"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(Observer->LastInfo).ToString().Contains(TEXT("금화 +")));

	Progression->OnGoldChanged.RemoveAll(Observer);
	Incident->OnIncidentClosed.RemoveAll(Observer);
	Incident->OnIncidentFinished.RemoveAll(Observer);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRewardAssetTest,
	"FestaFiesta.Incident.Rewards.ConfiguredAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentRewardAssetTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Paths = {
		TEXT("/Game/FestaFiesta/Data/Incident/FF_ID_NPCCrowded.FF_ID_NPCCrowded"),
		TEXT("/Game/FestaFiesta/Data/Incident/FF_ID_ItemLost.FF_ID_ItemLost")
	};
	for (const FString& Path : Paths)
	{
		const UFF_IncidentDefinition* Definition = LoadObject<UFF_IncidentDefinition>(nullptr, *Path);
		if (!TestNotNull(*Path, Definition)) continue;
		TestTrue(TEXT("편성할 선택지가 있음"), Definition->Options.Num() > 0);
		for (const UFF_IncidentOption* Option : Definition->Options)
		{
			if (!TestNotNull(TEXT("선택지"), Option)) continue;
			int32 RewardCount = 0;
			int32 Gold = 0;
			for (const UFF_IncidentEffect* Effect : Option->OnResolvedEffect)
			{
				if (const UFF_IncidentEffect_Reward* Reward = Cast<UFF_IncidentEffect_Reward>(Effect))
				{
					++RewardCount;
					Gold += Reward->RewardGold;
				}
			}
			TestEqual(TEXT("선택지별 금화 효과 중복 없음"), RewardCount, 1);
			TestEqual(TEXT("해결한 사건당 100금화"), Gold, 100);
		}
		for (const UFF_IncidentEffect* Effect : Definition->OnFailedEffect)
		{
			TestNull(TEXT("방치 효과에는 성공 금화 보상 없음"), Cast<UFF_IncidentEffect_Reward>(Effect));
		}
	}
	return true;
}

#endif
