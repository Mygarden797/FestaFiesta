// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/Rules/FF_PromotionRules.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

/**
 * 홍보 구매와 예상 방문객.
 *
 * 이 기능의 요점은 "돈이 나가면 손님이 늘어난다" 하나다. 2026-08-31 전까지 차감만 붙어 있고
 * 배수를 아무도 안 써서 **사면 손해만 보는 상태**였다 — 그 회귀를 여기서 막는다.
 */
namespace FFF_PromotionPurchaseTestHelpers
{
	constexpr int32 InitialGold = 10000;
	constexpr int32 BaseVisitors = 500;
	// 홍보 없음은 ×0.85 다(안 하면 손해). 생성자 기본값 기준.
	constexpr int32 NoPromotionVisitors = 425;

	struct FPromotionHarness
	{
		bool Initialize(int32 Gold = InitialGold)
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			if (GameInstance == nullptr) return false;

			Progression = NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			if (Progression == nullptr || Definition == nullptr || CostTable == nullptr) return false;

			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;

			Definition->FestivalId = TEXT("PromotionAutomationFestival");
			Definition->InitialGold = Gold;
			Definition->BaseDailyVisitors = BaseVisitors;

			Progression->StartFestival(Definition);
			return true;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};
}

// 🔴 이 기능의 존재 이유 — 사면 예상 방문객이 실제로 늘어난다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Promotion_PurchaseRaisesExpectedVisitors, "FestaFiesta.Economy.Promotion.PurchaseRaisesExpectedVisitors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Promotion_PurchaseRaisesExpectedVisitors::RunTest(const FString& Parameters)
{
	using namespace FFF_PromotionPurchaseTestHelpers;

	FPromotionHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize())) return false;

	TestEqual(TEXT("홍보 전에는 홍보 없음 배수(×0.85)다"),
		Harness.Progression->GetExpectedVisitors(), NoPromotionVisitors);

	const int32 GoldBefore = Harness.Progression->GetGold();
	const int32 Cost = Harness.Definition->PromotionFocused.CostGold;

	TestEqual(TEXT("집중 홍보가 구매된다"),
		Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Focused),
		EFF_PromotionPurchaseResult::Success);

	TestEqual(TEXT("비용이 빠진다"), Harness.Progression->GetGold(), GoldBefore - Cost);
	TestEqual(TEXT("원장에 홍보비로 남는다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::Promotion), Cost);
	TestEqual(TEXT("등급이 보관된다"),
		Harness.Progression->GetPurchasedPromotionGrade(), EFF_PromotionGrade::Focused);

	// 🔴 여기가 핵심이다. 이 줄이 죽으면 "사면 돈만 나가는" 상태로 되돌아간 것이다.
	const int32 Expected = FMath::FloorToInt32(
		BaseVisitors * Harness.Definition->PromotionFocused.VisitorMultiplier);
	TestEqual(TEXT("예상 방문객이 배수만큼 늘어난다"),
		Harness.Progression->GetExpectedVisitors(), Expected);
	TestTrue(TEXT("실제로 기본값보다 많다"), Expected > BaseVisitors);

	return true;
}

// 등급이 셀수록 손님이 많다. 두 등급의 순서가 뒤집히면 비싼 쪽을 살 이유가 없어진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Promotion_HigherGradeBringsMoreVisitors, "FestaFiesta.Economy.Promotion.HigherGradeBringsMoreVisitors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Promotion_HigherGradeBringsMoreVisitors::RunTest(const FString& Parameters)
{
	using namespace FFF_PromotionPurchaseTestHelpers;

	FPromotionHarness Basic;
	FPromotionHarness Focused;
	if (!TestTrue(TEXT("픽스처 둘이 구성된다"),
		Basic.Initialize() && Focused.Initialize())) return false;

	Basic.Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic);
	Focused.Progression->TryPurchasePromotion(EFF_PromotionGrade::Focused);

	TestTrue(TEXT("지역 SNS 도 홍보 없음보다 많다"),
		Basic.Progression->GetExpectedVisitors() > NoPromotionVisitors);
	TestTrue(TEXT("집중이 기본보다 많다"),
		Focused.Progression->GetExpectedVisitors() > Basic.Progression->GetExpectedVisitors());
	TestTrue(TEXT("집중이 더 비싸다"),
		Focused.Definition->PromotionFocused.CostGold > Focused.Definition->PromotionBasic.CostGold);

	return true;
}

// 1회 구매다. 두 번째는 등급이 무엇이든 거절되고 Gold 가 안 빠진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Promotion_PurchaseIsOncePerFestival, "FestaFiesta.Economy.Promotion.PurchaseIsOncePerFestival", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Promotion_PurchaseIsOncePerFestival::RunTest(const FString& Parameters)
{
	using namespace FFF_PromotionPurchaseTestHelpers;

	FPromotionHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize())) return false;

	Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic);
	const int32 GoldAfterFirst = Harness.Progression->GetGold();
	const int32 VisitorsAfterFirst = Harness.Progression->GetExpectedVisitors();

	TestEqual(TEXT("같은 등급 재구매가 거절된다"),
		Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic),
		EFF_PromotionPurchaseResult::AlreadyPurchased);
	TestEqual(TEXT("상위 등급으로 갈아타는 것도 거절된다"),
		Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Focused),
		EFF_PromotionPurchaseResult::AlreadyPurchased);

	TestEqual(TEXT("잔액이 그대로다"), Harness.Progression->GetGold(), GoldAfterFirst);
	TestEqual(TEXT("예상 방문객도 그대로다"),
		Harness.Progression->GetExpectedVisitors(), VisitorsAfterFirst);

	return true;
}

// 실패는 상태를 하나도 안 바꾼다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Promotion_FailedPurchaseChangesNothing, "FestaFiesta.Economy.Promotion.FailedPurchaseChangesNothing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Promotion_FailedPurchaseChangesNothing::RunTest(const FString& Parameters)
{
	using namespace FFF_PromotionPurchaseTestHelpers;

	// 기본 홍보도 못 살 잔액.
	FPromotionHarness Poor;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Poor.Initialize(10))) return false;

	TestEqual(TEXT("잔액이 모자라면 거절된다"),
		Poor.Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic),
		EFF_PromotionPurchaseResult::InsufficientGold);
	TestEqual(TEXT("잔액이 그대로다"), Poor.Progression->GetGold(), 10);
	TestEqual(TEXT("등급이 안 바뀐다"),
		Poor.Progression->GetPurchasedPromotionGrade(), EFF_PromotionGrade::None);
	TestEqual(TEXT("예상 방문객이 안 늘어난다"),
		Poor.Progression->GetExpectedVisitors(), NoPromotionVisitors);

	// `안 함` 은 구매 행위가 아니다. 통과시키면 그 뒤로 진짜 홍보를 못 산다.
	FPromotionHarness Rich;
	if (!TestTrue(TEXT("두 번째 픽스처가 구성된다"), Rich.Initialize())) return false;

	TestEqual(TEXT("None 은 구매로 취급하지 않는다"),
		Rich.Progression->TryPurchasePromotion(EFF_PromotionGrade::None),
		EFF_PromotionPurchaseResult::InvalidGrade);
	TestEqual(TEXT("그 뒤에도 진짜 홍보를 살 수 있다"),
		Rich.Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic),
		EFF_PromotionPurchaseResult::Success);

	return true;
}

// 축제가 시작되지 않았으면 "모름"이다. 0 이 아니다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Promotion_UnknownBeforeFestivalStarts, "FestaFiesta.Economy.Promotion.UnknownBeforeFestivalStarts", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Promotion_UnknownBeforeFestivalStarts::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UFF_FestivalProgressionSubsystem* Progression =
		NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
	if (!TestNotNull(TEXT("서브시스템이 만들어진다"), Progression)) return false;

	// 🔴 0 은 "아무도 안 온다"이고 INDEX_NONE 은 "아직 알 수 없다"다. 견적서가 둘을 다르게 그린다.
	TestEqual(TEXT("축제 전에는 모름이다"),
		Progression->GetExpectedVisitors(), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("구매도 축제 미시작으로 거절된다"),
		Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic),
		EFF_PromotionPurchaseResult::FestivalNotStarted);

	return true;
}

// 새 축제에는 홍보가 남지 않는다. 2회차가 1회차 배수를 물려받으면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Promotion_RestartClearsPromotion, "FestaFiesta.Economy.Promotion.RestartClearsPromotion", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Promotion_RestartClearsPromotion::RunTest(const FString& Parameters)
{
	using namespace FFF_PromotionPurchaseTestHelpers;

	FPromotionHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize())) return false;

	Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Focused);
	TestTrue(TEXT("1회차에 홍보가 붙어 있다"),
		Harness.Progression->GetExpectedVisitors() > BaseVisitors);

	Harness.Progression->RestartFestival();

	TestEqual(TEXT("등급이 비워진다"),
		Harness.Progression->GetPurchasedPromotionGrade(), EFF_PromotionGrade::None);
	TestEqual(TEXT("예상 방문객이 홍보 없음 값으로 돌아온다"),
		Harness.Progression->GetExpectedVisitors(), NoPromotionVisitors);
	TestEqual(TEXT("다시 살 수 있다"),
		Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic),
		EFF_PromotionPurchaseResult::Success);

	return true;
}

// 이벤트가 실제로 나가는지는 FF_CrowdEconomyBridgeSubsystemTests.cpp 의
// RestartResetsExpectedVisitors 가 본다 — 이 델리게이트들은 전부 Dynamic(Blueprint용)이라
// 여기서는 AddLambda 로 못 걸고, Crowd 브리지가 이미 실제 UFUNCTION 구독자로 존재한다.

// 순수 계산 — 내림하고, 배수 1.0 미만은 1.0 으로 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Promotion_VisitorCalcRoundsDown, "FestaFiesta.Economy.Promotion.VisitorCalcRoundsDown", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Promotion_VisitorCalcRoundsDown::RunTest(const FString& Parameters)
{
	// 100 * 1.08 = 108.0 이지만 부동소수라 107.999.. 가 될 수 있다. 내림이 안전한 쪽이다.
	TestEqual(TEXT("1.08 배"), FFF_PromotionRules::CalcExpectedVisitors(500, 1.08f), 540);
	TestEqual(TEXT("1.18 배"), FFF_PromotionRules::CalcExpectedVisitors(500, 1.18f), 590);
	TestEqual(TEXT("배수 1.0 이면 그대로"), FFF_PromotionRules::CalcExpectedVisitors(500, 1.f), 500);

	// 홍보가 손님을 줄이지는 않는다. 저작 오류를 그대로 반영하지 않는다.
	TestEqual(TEXT("1.0 미만은 1.0 으로 본다"),
		FFF_PromotionRules::CalcExpectedVisitors(500, 0.5f), 500);
	TestEqual(TEXT("기본값이 0 이면 0"), FFF_PromotionRules::CalcExpectedVisitors(0, 1.18f), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
