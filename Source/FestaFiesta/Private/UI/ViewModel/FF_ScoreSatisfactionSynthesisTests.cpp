// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "Tests/AutomationCommon.h"

/**
 * ScoreSatisfaction 합성(`Clamp(SimSatisfaction + FinaleBonus, 0, 100)`)을 검증한다
 * (2026-09-18). 두 입력의 소유자가 다르다(SimSatisfaction=Utility AI, FinaleBonus=경제) —
 * 이 테스트는 둘 중 어느 쪽이 바뀌어도 ViewModel의 ScoreSatisfaction이 다시 계산되는지 본다.
 *
 * "연출 보정" 항은 정의·계수가 없어 이번 구현에 포함하지 않았다 — 별도 결정 필요.
 */
namespace FFF_ScoreSatisfactionSynthesisTestHelpers
{
	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	struct FScoreSatisfactionHarness
	{
		bool Initialize(FAutomationTestBase* Test)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(Test);
				return false;
			}

			UWorld* World = TestWorld.GetTestWorld();
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			if (!GameInstance)
			{
				return false;
			}

			ViewModel = GameInstance->GetSubsystem<UFF_FestivalViewModelSubsystem>();
			Progression = GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>();
			Satisfaction = GameInstance->GetSubsystem<UFF_SatisfactionMetricsSubsystem>();
			if (!ViewModel || !Progression || !Satisfaction)
			{
				return false;
			}

			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			if (!Definition || !CostTable)
			{
				return false;
			}

			// 표가 비어 있으면 IsDefinitionValid가 Error를 찍고 자동화 프레임워크가 실패로
			// 취급한다(`FF_FinalePurchaseTests.cpp`와 같은 이유) — 이 테스트는 가격표 내용
			// 자체를 보지 않으므로 행 하나만 채운다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;

			Definition->FestivalId = TEXT("ScoreSatisfactionAutomationFestival");
			// 이 테스트는 만족도 합성만 본다. 임대료·미납이 걷히면 피날레 구매 잔액에 섞인다.
			Definition->DailyVenueRentGold = 0;

			Progression->StartFestival(Definition);
			return true;
		}

		// 밀도·Need 충족도 두 입력만 채워 SimSatisfaction을 원하는 값으로 정확히 맞춘다.
		// 균등가중(0.5/0.5) 2단 블렌드에서 두 입력이 같은 값이면, 대기·취향 입력이 비어도
		// (그 축·상위 블렌드가 정규화로 결측을 건너뛰므로) 최종값이 그 값 그대로 나온다.
		void CommitSimSatisfaction(float Score0To100)
		{
			Satisfaction->BeginAggregation();
			Satisfaction->AddDensitySample(Score0To100, 1);
			Satisfaction->AddNeedFulfillmentSample(Score0To100);
			Satisfaction->CommitAggregation(FFF_SatisfactionCoefficients());
		}

		FTestWorldWrapper TestWorld;
		UFF_FestivalViewModelSubsystem* ViewModel = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_SatisfactionMetricsSubsystem* Satisfaction = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};
}

// SimSatisfaction만 바뀌어도(피날레 미구매, 보너스 0) ScoreSatisfaction이 그 값을 그대로 따라간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ScoreSatisfaction_TracksSimSatisfactionBeforeFinalePurchase, "FestaFiesta.UI.ViewModel.ScoreSatisfaction.TracksSimSatisfactionBeforeFinalePurchase", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ScoreSatisfaction_TracksSimSatisfactionBeforeFinalePurchase::RunTest(const FString& Parameters)
{
	using namespace FFF_ScoreSatisfactionSynthesisTestHelpers;

	FScoreSatisfactionHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(this)))
	{
		return false;
	}

	// 바인딩 직후, 두 소유자 모두 아직 확정값이 없다 — 중립값 50 + 보너스 0.
	TestEqual(TEXT("시작 시 SimSatisfaction은 중립값 50이다"), Harness.ViewModel->GetSimSatisfaction(), 50);
	TestEqual(TEXT("시작 시 ScoreSatisfaction도 50이다(보너스 0)"), Harness.ViewModel->GetScoreSatisfaction(), 50);
	TestEqual(TEXT("미구매면 FinaleBonus가 0이다"), Harness.ViewModel->GetFinaleSatisfactionBonus(), 0);

	Harness.CommitSimSatisfaction(40.f);
	TestEqual(TEXT("SimSatisfaction이 40으로 갱신된다"), Harness.ViewModel->GetSimSatisfaction(), 40);
	TestEqual(TEXT("피날레 미구매 상태라 ScoreSatisfaction도 40이다"), Harness.ViewModel->GetScoreSatisfaction(), 40);

	return true;
}

// 피날레 구매로 FinaleBonus가 생기면 ScoreSatisfaction이 SimSatisfaction + FinaleBonus로 갈라지고,
// 그 뒤 SimSatisfaction이 다시 바뀌어도(보너스는 그대로) 합이 계속 따라간다. 합이 100을 넘으면 클램프한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ScoreSatisfaction_AddsFinaleBonusAndClampsAtMax, "FestaFiesta.UI.ViewModel.ScoreSatisfaction.AddsFinaleBonusAndClampsAtMax", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ScoreSatisfaction_AddsFinaleBonusAndClampsAtMax::RunTest(const FString& Parameters)
{
	using namespace FFF_ScoreSatisfactionSynthesisTestHelpers;

	FScoreSatisfactionHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(this)))
	{
		return false;
	}

	Harness.CommitSimSatisfaction(40.f);
	TestEqual(TEXT("구매 전 SimSatisfaction은 40이다"), Harness.ViewModel->GetSimSatisfaction(), 40);

	// A 등급 보너스는 +20이다([기획서] §10, `FF_FinalePurchaseTests.cpp`의 BonusA와 동일).
	TestEqual(TEXT("A 등급을 구매한다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::A),
		EFF_FinalePurchaseResult::Success);

	TestEqual(TEXT("ViewModel에 보너스 20이 노출된다"), Harness.ViewModel->GetFinaleSatisfactionBonus(), 20);
	TestEqual(TEXT("ScoreSatisfaction이 40+20=60으로 갈라진다(클램프 없이)"),
		Harness.ViewModel->GetScoreSatisfaction(), 60);

	// SimSatisfaction이 나중에 다시 바뀌어도(보너스는 구매 시점에 고정) 합이 계속 따라간다.
	Harness.CommitSimSatisfaction(95.f);
	TestEqual(TEXT("SimSatisfaction이 95로 갱신된다"), Harness.ViewModel->GetSimSatisfaction(), 95);
	TestEqual(TEXT("95+20=115는 100으로 클램프된다"), Harness.ViewModel->GetScoreSatisfaction(), 100);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
