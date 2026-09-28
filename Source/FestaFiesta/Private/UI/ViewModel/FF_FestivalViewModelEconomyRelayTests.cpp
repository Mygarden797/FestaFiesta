// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalBuzzSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

// ViewModel 이 경제·화제성 소유자 값을 그대로 중계하는지 본다. 규칙 자체는 경제 쪽 테스트가 본다.
namespace FFF_ViewModelEconomyRelayTestHelpers
{
	struct FEconomyRelayHarness
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
			Buzz = GameInstance->GetSubsystem<UFF_FestivalBuzzSubsystem>();
			if (!ViewModel || !Progression || !Buzz)
			{
				return false;
			}

			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			if (!Definition || !CostTable)
			{
				return false;
			}

			// 표가 비어 있으면 정의 검증이 Error 를 찍어 테스트가 실패로 잡힌다. 행 하나만 채운다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;
			Definition->FestivalId = TEXT("ViewModelEconomyRelayAutomationFestival");
			return true;
		}

		void StartFestival()
		{
			Progression->StartFestival(Definition);
		}

		FTestWorldWrapper TestWorld;
		UFF_FestivalViewModelSubsystem* ViewModel = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalBuzzSubsystem* Buzz = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ViewModelRelay_AdmissionFee, "FestaFiesta.UI.ViewModel.EconomyRelay.AdmissionFee", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ViewModelRelay_AdmissionFee::RunTest(const FString& Parameters)
{
	using namespace FFF_ViewModelEconomyRelayTestHelpers;

	FEconomyRelayHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(this)))
	{
		return false;
	}

	Harness.StartFestival();

	int32 MinGold = 0, MaxGold = 0, ReferenceGold = 0;
	Harness.ViewModel->GetAdmissionFeeRange(MinGold, MaxGold, ReferenceGold);
	TestEqual(TEXT("범위 최솟값은 정의 값이다"), MinGold, Harness.Definition->AdmissionFeeMinGold);
	TestEqual(TEXT("범위 최댓값은 정의 값이다"), MaxGold, Harness.Definition->AdmissionFeeMaxGold);
	TestEqual(TEXT("기준값은 정의 값이다"), ReferenceGold, Harness.Definition->AdmissionFeeGold);

	const int32 NewFee = ReferenceGold + 2;
	const FFF_VisitorForecast Preview =
		Harness.ViewModel->PreviewVisitorForecast(EFF_PromotionChoice::None, NewFee);
	TestEqual(TEXT("미리보기는 상태를 안 바꾼다"), Harness.ViewModel->GetAdmissionFeeGold(), ReferenceGold);

	TestEqual(TEXT("범위 안 변경은 성공한다"),
		Harness.ViewModel->RequestSetAdmissionFee(NewFee), EFF_AdmissionFeeResult::Success);
	TestEqual(TEXT("오늘 입장료가 바뀐다"), Harness.ViewModel->GetAdmissionFeeGold(), NewFee);
	TestEqual(TEXT("현재 예보가 미리보기와 같다"),
		Harness.ViewModel->GetVisitorForecast().ExpectedVisitors, Preview.ExpectedVisitors);
	TestEqual(TEXT("예상 방문객 캐시도 따라온다"),
		Harness.ViewModel->GetExpectedVisitors(), Preview.ExpectedVisitors);
	TestTrue(TEXT("비싸지면 방문객이 준다"), Preview.VisitorDeltaFromFee < 0);

	TestEqual(TEXT("범위 밖은 거부한다"),
		Harness.ViewModel->RequestSetAdmissionFee(MaxGold + 1), EFF_AdmissionFeeResult::OutOfRange);
	TestEqual(TEXT("거부되면 입장료가 그대로다"), Harness.ViewModel->GetAdmissionFeeGold(), NewFee);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ViewModelRelay_DailyGoal, "FestaFiesta.UI.ViewModel.EconomyRelay.DailyGoal", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ViewModelRelay_DailyGoal::RunTest(const FString& Parameters)
{
	using namespace FFF_ViewModelEconomyRelayTestHelpers;

	FEconomyRelayHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(this)))
	{
		return false;
	}


	Harness.StartFestival();
	TestEqual(TEXT("시작 직후 입장 인원은 0"), Harness.ViewModel->GetAdmittedVisitorsToday(), 0);

	Harness.Progression->RecordAdmittedVisitorsForDailyGoal(12);
	TestEqual(TEXT("오늘 입장 인원이 중계된다"), Harness.ViewModel->GetAdmittedVisitorsToday(), 12);
	TestEqual(TEXT("누적 입장 인원이 중계된다"), Harness.ViewModel->GetTotalAdmittedVisitors(), 12);
	TestEqual(TEXT("목표 보너스 여부가 소유자와 같다"),
		Harness.ViewModel->IsDailyGoalBonusPaid(), Harness.Progression->IsDailyGoalBonusPaid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ViewModelRelay_Buzz, "FestaFiesta.UI.ViewModel.EconomyRelay.Buzz", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ViewModelRelay_Buzz::RunTest(const FString& Parameters)
{
	using namespace FFF_ViewModelEconomyRelayTestHelpers;

	FEconomyRelayHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(this)))
	{
		return false;
	}

	Harness.StartFestival();
	TestEqual(TEXT("시작하면 중립 50"), Harness.ViewModel->GetBuzz(), 50.f);

	Harness.Buzz->AddBuzz(10.f, TEXT("Test"));
	TestEqual(TEXT("화제성이 중계된다"), Harness.ViewModel->GetBuzz(), 60.f);
	TestEqual(TEXT("오늘 변화는 +10"), Harness.ViewModel->GetBuzzChangeToday(), 10.f);

	Harness.Buzz->BeginNewDay(0.5f);
	TestEqual(TEXT("하루 경계에서 중립 쪽으로 식는다"), Harness.ViewModel->GetBuzz(), 55.f);
	TestEqual(TEXT("하루가 바뀌면 오늘 변화는 0"), Harness.ViewModel->GetBuzzChangeToday(), 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ViewModelRelay_StaffPromotion, "FestaFiesta.UI.ViewModel.EconomyRelay.StaffPromotion", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ViewModelRelay_StaffPromotion::RunTest(const FString& Parameters)
{
	using namespace FFF_ViewModelEconomyRelayTestHelpers;

	FEconomyRelayHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(this)))
	{
		return false;
	}


	Harness.StartFestival();
	const TArray<FFF_StaffMember> Roster = Harness.ViewModel->GetStaffRoster();
	TestEqual(TEXT("명단이 소유자와 같다"), Roster.Num(), Harness.Progression->GetStaffRoster().Num());
	TestFalse(TEXT("없는 사람은 승급 불가"), Harness.ViewModel->CanPromoteStaff(99999));
	if (Roster.Num() == 0)
	{
		return true;
	}

	const int32 StaffId = Roster[0].StaffId;
	const bool bCanPromote = Harness.ViewModel->CanPromoteStaff(StaffId);
	TestEqual(TEXT("판정과 실제 승급 결과가 같다"), Harness.ViewModel->RequestPromoteStaff(StaffId), bCanPromote);
	TestFalse(TEXT("같은 날 두 번은 안 된다"), Harness.ViewModel->CanPromoteStaff(StaffId));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ViewModelRelay_Evaluation, "FestaFiesta.UI.ViewModel.EconomyRelay.Evaluation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ViewModelRelay_Evaluation::RunTest(const FString& Parameters)
{
	using namespace FFF_ViewModelEconomyRelayTestHelpers;

	FEconomyRelayHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(this)))
	{
		return false;
	}

	Harness.StartFestival();
	const FFF_FestivalEvaluation FromViewModel = Harness.ViewModel->GetFestivalEvaluation();
	const FFF_FestivalEvaluation FromOwner = Harness.Progression->GetFestivalEvaluation();
	TestEqual(TEXT("총점이 소유자와 같다"), FromViewModel.TotalScore, FromOwner.TotalScore);
	TestEqual(TEXT("등급이 소유자와 같다"), FromViewModel.Grade, FromOwner.Grade);
	TestEqual(TEXT("날짜별 만족도가 소유자와 같다"),
		Harness.ViewModel->GetDailySatisfactionHistory().Num(), Harness.Progression->GetDailySatisfactionHistory().Num());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ViewModelRelay_DisplayText, "FestaFiesta.UI.ViewModel.EconomyRelay.DisplayText", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ViewModelRelay_DisplayText::RunTest(const FString& Parameters)
{
	for (const EFF_AdmissionFeeResult Result : { EFF_AdmissionFeeResult::Success, EFF_AdmissionFeeResult::FestivalNotStarted,
		EFF_AdmissionFeeResult::OutOfRange, EFF_AdmissionFeeResult::LockedDuringLive })
	{
		TestFalse(TEXT("입장료 결과 문구가 있다"), UFF_EconomyDisplayTextLibrary::GetAdmissionFeeResultText(Result).IsEmpty());
	}
	for (const EFF_PromotionGrade Grade : { EFF_PromotionGrade::None, EFF_PromotionGrade::Basic,
		EFF_PromotionGrade::Focused, EFF_PromotionGrade::Influencer })
	{
		TestFalse(TEXT("홍보 등급 문구가 있다"), UFF_EconomyDisplayTextLibrary::GetPromotionGradeText(Grade).IsEmpty());
	}
	for (const EFF_FestivalEvaluationGrade Grade : { EFF_FestivalEvaluationGrade::D, EFF_FestivalEvaluationGrade::C,
		EFF_FestivalEvaluationGrade::B, EFF_FestivalEvaluationGrade::A, EFF_FestivalEvaluationGrade::S })
	{
		TestFalse(TEXT("평가 등급 문구가 있다"), UFF_EconomyDisplayTextLibrary::GetFestivalEvaluationGradeText(Grade).IsEmpty());
	}
	for (const EFF_FestivalEvaluationAxis Axis : { EFF_FestivalEvaluationAxis::Satisfaction, EFF_FestivalEvaluationAxis::Visitors,
		EFF_FestivalEvaluationAxis::Finance, EFF_FestivalEvaluationAxis::Buzz })
	{
		TestFalse(TEXT("평가 항목 문구가 있다"), UFF_EconomyDisplayTextLibrary::GetFestivalEvaluationAxisText(Axis).IsEmpty());
	}
	return true;
}

#endif
