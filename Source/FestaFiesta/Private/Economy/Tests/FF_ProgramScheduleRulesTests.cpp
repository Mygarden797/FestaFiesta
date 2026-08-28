// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Festival/FF_FestivalDefinition.h"

/**
 * 프로그램 편성 판정 검증 (S8).
 *
 * 순수 함수라 서브시스템도 GameInstance 도 만들지 않는다.
 * 차감·상태 변경이 실제로 일어나는지는 UFF_FestivalProgressionSubsystem 쪽 테스트가 갖는다.
 */
namespace FFF_ProgramScheduleTestHelpers
{
	const FName Workshop(TEXT("Program_SandArtWorkshop"));
	const FName Yoga(TEXT("Program_SunsetYoga"));

	// v0 카탈로그. 기획서 §9.1 의 정원·체험비를 그대로 쓰고 비용만 1일권 기준으로 내렸다
	// (정원을 다 채웠을 때 매출의 55~62%). 정원이 보장되므로 편성하면 그만큼 남는다.
	constexpr int32 WorkshopCost = 200;
	constexpr int32 WorkshopCapacity = 12;
	constexpr int32 WorkshopFee = 30;

	constexpr int32 YogaCost = 250;
	constexpr int32 YogaCapacity = 16;
	constexpr int32 YogaFee = 25;

	TArray<FFF_ProgramCard> MakeCatalog()
	{
		// 시각은 Live 경과분이다. 둘을 다른 시간대에 두는 것이 저작 의도라 여기서도 그렇게 만든다.
		FFF_ProgramCard WorkshopCard;
		WorkshopCard.ProgramId = Workshop;
		WorkshopCard.DisplayName = FText::FromString(TEXT("바다향기 공방"));
		WorkshopCard.CostGold = WorkshopCost;
		WorkshopCard.RequiredStaff = 1;
		WorkshopCard.Capacity = WorkshopCapacity;
		WorkshopCard.FeePerPerson = WorkshopFee;
		WorkshopCard.StartMinutes = 30.f;
		WorkshopCard.EndMinutes = 60.f;

		FFF_ProgramCard YogaCard;
		YogaCard.ProgramId = Yoga;
		YogaCard.DisplayName = FText::FromString(TEXT("선셋 요가"));
		YogaCard.CostGold = YogaCost;
		YogaCard.RequiredStaff = 2;
		YogaCard.Capacity = YogaCapacity;
		YogaCard.FeePerPerson = YogaFee;
		YogaCard.StartMinutes = 90.f;
		YogaCard.EndMinutes = 120.f;

		return { WorkshopCard, YogaCard };
	}
}

// 편성할 수 있으면 Success 이고 카드가 채워진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_AffordableProgramSucceeds, "FestaFiesta.Economy.ProgramSchedule.AffordableProgramSucceeds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_AffordableProgramSucceeds::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleTestHelpers;

	const TArray<FFF_ProgramCard> Catalog = MakeCatalog();
	FFF_ProgramCard Card;

	TestEqual(TEXT("잔액·인원이 충분하면 편성된다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Workshop, {}, /*Gold=*/10000, /*Staff=*/6, Card),
		EFF_ProgramPurchaseResult::Success);

	// UI 가 비용을 다시 조회하지 않아도 되게 카드를 돌려준다.
	TestEqual(TEXT("카드가 채워진다"), Card.ProgramId, Workshop);
	TestEqual(TEXT("비용이 맞다"), Card.CostGold, WorkshopCost);
	TestEqual(TEXT("정원을 다 채우면 매출이 360 이다"), Card.CalcMaxRevenue(), 360);

	return true;
}

// 실패 이유 4종이 각각 구분돼서 나온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_EachFailureIsDistinct, "FestaFiesta.Economy.ProgramSchedule.EachFailureIsDistinct", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_EachFailureIsDistinct::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleTestHelpers;

	const TArray<FFF_ProgramCard> Catalog = MakeCatalog();
	FFF_ProgramCard Card;

	TestEqual(TEXT("카탈로그에 없는 id"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, TEXT("Program_DoesNotExist"), {}, 10000, 6, Card),
		EFF_ProgramPurchaseResult::UnknownProgram);

	TestEqual(TEXT("오늘 이미 편성했다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Workshop, { Workshop }, 10000, 6, Card),
		EFF_ProgramPurchaseResult::AlreadyScheduled);

	TestEqual(TEXT("인원이 모자라다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Yoga, {}, 10000, /*Staff=*/1, Card),
		EFF_ProgramPurchaseResult::NotEnoughStaff);

	TestEqual(TEXT("잔액이 모자라다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Workshop, {}, /*Gold=*/WorkshopCost - 1, 6, Card),
		EFF_ProgramPurchaseResult::InsufficientGold);

	// 딱 맞는 잔액·인원은 통과한다. 경계에서 한 칸 어긋나는 사고를 막는다.
	TestEqual(TEXT("잔액이 정확히 비용만큼이면 편성된다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Workshop, {}, /*Gold=*/WorkshopCost, 6, Card),
		EFF_ProgramPurchaseResult::Success);
	TestEqual(TEXT("인원이 정확히 요구만큼이면 편성된다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Yoga, {}, 10000, /*Staff=*/2, Card),
		EFF_ProgramPurchaseResult::Success);

	return true;
}

// 🔴 판정 순서 — 없는 카드에 "Gold 부족"이 나오면 거짓 안내가 된다.
//
// 이 테스트가 없으면 조건을 재배열해도 위 테스트들이 그대로 통과한다.
// 각 단계마다 "뒤쪽 조건도 같이 틀린" 입력을 넣어 앞쪽 이유가 이기는지 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_EarlierReasonWinsOverLater, "FestaFiesta.Economy.ProgramSchedule.EarlierReasonWinsOverLater", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_EarlierReasonWinsOverLater::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleTestHelpers;

	const TArray<FFF_ProgramCard> Catalog = MakeCatalog();
	FFF_ProgramCard Card;

	// 없는 카드 + 잔액 0 + 인원 0 → 없는 카드가 이긴다.
	TestEqual(TEXT("없는 카드가 잔액·인원보다 먼저다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, TEXT("Program_DoesNotExist"), {}, 0, 0, Card),
		EFF_ProgramPurchaseResult::UnknownProgram);

	// 이미 편성 + 잔액 0 + 인원 0 → 중복이 이긴다.
	TestEqual(TEXT("중복이 잔액·인원보다 먼저다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Workshop, { Workshop }, 0, 0, Card),
		EFF_ProgramPurchaseResult::AlreadyScheduled);

	// 인원 0 + 잔액 0 → 인원이 이긴다.
	// 알바 고용으로 푸는 문제와 매출로 푸는 문제가 달라 플레이어의 다음 행동이 갈린다.
	TestEqual(TEXT("인원이 잔액보다 먼저다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Workshop, {}, 0, 0, Card),
		EFF_ProgramPurchaseResult::NotEnoughStaff);

	return true;
}

// 저작이 깨진 카드는 카탈로그에 있어도 못 산다.
// 0 원짜리 "유료" 프로그램이나 정원 0 이 조용히 통과하면 그날 매출이 이유 없이 0 이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_MalformedCardIsUnbuyable, "FestaFiesta.Economy.ProgramSchedule.MalformedCardIsUnbuyable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_MalformedCardIsUnbuyable::RunTest(const FString& Parameters)
{
	FFF_ProgramCard Broken;
	Broken.ProgramId = TEXT("Program_Broken");
	Broken.CostGold = 200;
	Broken.RequiredStaff = 1;
	Broken.Capacity = 12;
	Broken.FeePerPerson = 30;
	Broken.StartMinutes = 30.f;
	Broken.EndMinutes = 60.f;

	auto EvaluateWith = [](const FFF_ProgramCard& Card)
	{
		FFF_ProgramCard Out;
		return FFF_ProgramScheduleRules::EvaluatePurchase(
			{ Card }, Card.ProgramId, {}, 10000, 6, Out);
	};

	TestEqual(TEXT("온전한 카드는 통과한다"),
		EvaluateWith(Broken), EFF_ProgramPurchaseResult::Success);

	FFF_ProgramCard FreeCard = Broken;
	FreeCard.CostGold = 0;
	TestEqual(TEXT("비용 0 은 저작 오류다"),
		EvaluateWith(FreeCard), EFF_ProgramPurchaseResult::UnknownProgram);

	FFF_ProgramCard NoSeats = Broken;
	NoSeats.Capacity = 0;
	TestEqual(TEXT("정원 0 은 저작 오류다"),
		EvaluateWith(NoSeats), EFF_ProgramPurchaseResult::UnknownProgram);

	FFF_ProgramCard NoFee = Broken;
	NoFee.FeePerPerson = 0;
	TestEqual(TEXT("체험비 0 은 저작 오류다"),
		EvaluateWith(NoFee), EFF_ProgramPurchaseResult::UnknownProgram);

	FFF_ProgramCard NoStaff = Broken;
	NoStaff.RequiredStaff = 0;
	TestEqual(TEXT("필요 인원 0 은 저작 오류다"),
		EvaluateWith(NoStaff), EFF_ProgramPurchaseResult::UnknownProgram);

	// 🔴 시간이 없거나 뒤집힌 카드도 막는다. 이대로 편성되면 RegisterProgram 이
	// 무효한 슬롯을 만들고, 잠금·퇴장 금지·도착 판정이 전부 조용히 꺼진다
	// (FFF_ProgramReservation::IsValid 가 EndMinutes > StartMinutes 를 요구한다).
	FFF_ProgramCard NoDuration = Broken;
	NoDuration.StartMinutes = 0.f;
	NoDuration.EndMinutes = 0.f;
	TestEqual(TEXT("길이가 0 인 프로그램은 저작 오류다"),
		EvaluateWith(NoDuration), EFF_ProgramPurchaseResult::UnknownProgram);

	FFF_ProgramCard Reversed = Broken;
	Reversed.StartMinutes = 60.f;
	Reversed.EndMinutes = 30.f;
	TestEqual(TEXT("종료가 시작보다 이르면 저작 오류다"),
		EvaluateWith(Reversed), EFF_ProgramPurchaseResult::UnknownProgram);

	return true;
}

// 편성된 카드들이 인원을 합해서 묶는다. 이 값이 틀리면 인원 판정 전체가 같이 틀어진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_StaffAssignedAccumulates, "FestaFiesta.Economy.ProgramSchedule.StaffAssignedAccumulates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_StaffAssignedAccumulates::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleTestHelpers;

	const TArray<FFF_ProgramCard> Catalog = MakeCatalog();

	TestEqual(TEXT("편성이 없으면 0 이다"),
		FFF_ProgramScheduleRules::CalcStaffAssignedToPrograms(Catalog, {}), 0);
	TestEqual(TEXT("공방 하나면 1 이다"),
		FFF_ProgramScheduleRules::CalcStaffAssignedToPrograms(Catalog, { Workshop }), 1);
	TestEqual(TEXT("공방 + 요가면 3 이다"),
		FFF_ProgramScheduleRules::CalcStaffAssignedToPrograms(Catalog, { Workshop, Yoga }), 3);

	// 카탈로그에서 사라진 카드가 편성 목록에 남아 있어도 인원을 먹지 않는다.
	// (저작이 바뀐 저장 슬롯을 불러오는 경로에서 실제로 생길 수 있다)
	TestEqual(TEXT("모르는 카드는 인원을 안 먹는다"),
		FFF_ProgramScheduleRules::CalcStaffAssignedToPrograms(
			Catalog, { Workshop, TEXT("Program_Removed") }), 1);

	// 🔴 인원 축이 실제로 편성을 막는지 — 기본 6명으로 요가(2) 를 세 번 열면 네 번째가 막힌다.
	FFF_ProgramCard Card;
	const int32 BaseStaff = 6;
	const TArray<FName> ThreeYogas = { Yoga, Yoga, Yoga };
	const int32 Used = FFF_ProgramScheduleRules::CalcStaffAssignedToPrograms(Catalog, ThreeYogas);
	TestEqual(TEXT("요가 3 개면 인원 6 을 다 쓴다"), Used, 6);
	TestEqual(TEXT("남은 인원이 0 이면 공방도 못 연다"),
		FFF_ProgramScheduleRules::EvaluatePurchase(
			Catalog, Workshop, ThreeYogas, 10000, BaseStaff - Used, Card),
		EFF_ProgramPurchaseResult::NotEnoughStaff);

	return true;
}

// 매출은 참가 인원 × 체험비이고 정원에서 잘린다.
// 🔴 정원이 보장되므로 정상 상황에서는 항상 정원만큼 들어온다. 그래도 **인원 기준으로**
// 계산해 두는 이유는, 매출을 정원으로 고정해 버리면 스폰이 깨져도 원장이 멀쩡해 보이기 때문이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_RevenueFollowsAttendeesNotCapacity, "FestaFiesta.Economy.ProgramSchedule.RevenueFollowsAttendeesNotCapacity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_RevenueFollowsAttendeesNotCapacity::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleTestHelpers;

	const FFF_ProgramCard Card = MakeCatalog()[0];

	TestEqual(TEXT("정원을 다 채우면 360"),
		FFF_ProgramScheduleRules::CalcProgramRevenue(Card, WorkshopCapacity), 360);
	TestEqual(TEXT("절반만 오면 180"),
		FFF_ProgramScheduleRules::CalcProgramRevenue(Card, 6), 180);

	// 인원 0 이면 0 이다. 스폰이 아예 안 붙은 상태가 이 값으로 드러난다.
	TestEqual(TEXT("인원이 0 이면 0"),
		FFF_ProgramScheduleRules::CalcProgramRevenue(Card, 0), 0);

	// 정원 기준 순익 — 360 - 200 = 160 이 확정값이다.
	TestTrue(TEXT("정원을 채우면 비용을 넘는다"),
		FFF_ProgramScheduleRules::CalcProgramRevenue(Card, WorkshopCapacity) > WorkshopCost);

	// 정원을 넘겨 받아도 산 것보다 더 받지 않는다.
	TestEqual(TEXT("정원 초과는 정원에서 잘린다"),
		FFF_ProgramScheduleRules::CalcProgramRevenue(Card, WorkshopCapacity + 5), 360);
	TestEqual(TEXT("음수는 0 으로 본다"),
		FFF_ProgramScheduleRules::CalcProgramRevenue(Card, -3), 0);

	return true;
}

// 🔴 주문서 인원 합은 항상 정원과 같다. **"산 정원만큼 온다"의 근거가 이 등식이다.**
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_ReservedRequestsSumToCapacity, "FestaFiesta.Economy.ProgramScheduleRules.ReservedRequestsSumToCapacity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_ReservedRequestsSumToCapacity::RunTest(const FString& Parameters)
{
	const FGuid ReservationId = FGuid::NewGuid();

	// 6 의 배수인 것과 아닌 것, 6 미만, 딱 6 을 같이 본다 — 나머지 처리가 갈리는 지점이다.
	for (const int32 Capacity : { 1, 5, 6, 7, 12, 16, 25 })
	{
		FFF_ProgramCard Card;
		Card.Capacity = Capacity;
		Card.PreferredArchetypeId = TEXT("life_stage_20s");

		const TArray<FFF_ReservedPartyRequest> Requests =
			FFF_ProgramScheduleRules::BuildReservedPartyRequests(Card, ReservationId);

		int32 Sum = 0;
		for (const FFF_ReservedPartyRequest& Request : Requests)
		{
			Sum += Request.GroupSize;

			// 상한을 넘으면 빌더가 주문서를 통째로 거절한다 — 그러면 정원이 안 찬다.
			TestTrue(*FString::Printf(TEXT("정원 %d: 그룹이 1~6 명이다"), Capacity),
				Request.GroupSize >= 1 && Request.GroupSize <= 6);
			TestEqual(*FString::Printf(TEXT("정원 %d: 예약 핸들이 붙는다"), Capacity),
				Request.ReservationId, ReservationId);
			TestEqual(*FString::Printf(TEXT("정원 %d: 아키타입이 붙는다"), Capacity),
				Request.AdultArchetypeId, FName(TEXT("life_stage_20s")));
			// 아동을 섞으면 리더 성인 규칙 때문에 크기 하한이 2 가 되어 정원 맞추기가 깨진다.
			TestEqual(*FString::Printf(TEXT("정원 %d: 아동은 없다"), Capacity),
				Request.ChildCount, 0);
		}
		TestEqual(*FString::Printf(TEXT("정원 %d 만큼 주문된다"), Capacity), Sum, Capacity);
	}

	return true;
}

// 그룹 수가 최소가 된다 — 정원 12 는 6+6 이지 1 이 열두 개가 아니다.
// 잘게 쪼개면 파티가 흩어져 "일행이 같이 온다"가 무너진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_ReservedRequestsUseFewestGroups, "FestaFiesta.Economy.ProgramScheduleRules.ReservedRequestsUseFewestGroups", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_ReservedRequestsUseFewestGroups::RunTest(const FString& Parameters)
{
	FFF_ProgramCard Card;
	Card.Capacity = 16;

	const TArray<FFF_ReservedPartyRequest> Requests =
		FFF_ProgramScheduleRules::BuildReservedPartyRequests(Card, FGuid::NewGuid());

	if (TestEqual(TEXT("16명이 세 그룹으로 나뉜다"), Requests.Num(), 3))
	{
		TestEqual(TEXT("첫 그룹이 6명"), Requests[0].GroupSize, 6);
		TestEqual(TEXT("둘째 그룹이 6명"), Requests[1].GroupSize, 6);
		TestEqual(TEXT("나머지가 마지막 그룹"), Requests[2].GroupSize, 4);
	}
	return true;
}

// 저작이 비었거나 예약 핸들이 무효하면 빈 목록이다.
// 무효 핸들로 주문을 만들면 스폰된 손님이 아무 예약에도 안 붙어 체험비가 0 이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_ReservedRequestsRejectInvalidInput, "FestaFiesta.Economy.ProgramScheduleRules.ReservedRequestsRejectInvalidInput", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_ReservedRequestsRejectInvalidInput::RunTest(const FString& Parameters)
{
	FFF_ProgramCard ZeroCapacity;
	ZeroCapacity.Capacity = 0;
	TestEqual(TEXT("정원 0 이면 아무도 주문하지 않는다"),
		FFF_ProgramScheduleRules::BuildReservedPartyRequests(ZeroCapacity, FGuid::NewGuid()).Num(), 0);

	FFF_ProgramCard Card;
	Card.Capacity = 12;
	TestEqual(TEXT("무효한 예약 핸들이면 주문하지 않는다"),
		FFF_ProgramScheduleRules::BuildReservedPartyRequests(Card, FGuid()).Num(), 0);

	return true;
}

// 저작된 시설 종류에서만 열린다. 화장실에 요가를 여는 경로를 막는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_FacilityTypeIsEnforced, "FestaFiesta.Economy.ProgramScheduleRules.FacilityTypeIsEnforced", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_FacilityTypeIsEnforced::RunTest(const FString& Parameters)
{
	FFF_ProgramCard Card;
	Card.AllowedFacilityTypes = { TEXT("Booth_CraftExperience") };

	TestTrue(TEXT("공예 체험 부스에서는 열린다"),
		FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, TEXT("Booth"), TEXT("CraftExperience")));
	TestFalse(TEXT("화장실에서는 열리지 않는다"),
		FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, TEXT("Toilet"), TEXT("Standard")));

	// 대분류만 같고 소분류가 다른 경우가 실제로 가장 헷갈린다 — 둘 다 Booth 다.
	TestFalse(TEXT("같은 대분류라도 소분류가 다르면 안 된다"),
		FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, TEXT("Booth"), TEXT("FoodStall")));

	// 종류를 알 수 없으면 열어주지 않는다. 이름 규칙을 어긴 시설이 여기로 온다.
	TestFalse(TEXT("토큰이 비면 거절한다"),
		FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, NAME_None, NAME_None));
	return true;
}

// 여러 시설 종류를 허용할 수 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_FacilityTypeAllowsMultiple, "FestaFiesta.Economy.ProgramScheduleRules.FacilityTypeAllowsMultiple", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_FacilityTypeAllowsMultiple::RunTest(const FString& Parameters)
{
	FFF_ProgramCard Card;
	Card.AllowedFacilityTypes = { TEXT("Booth_CraftExperience"), TEXT("Booth_ActivityGame") };

	TestTrue(TEXT("첫 번째 종류에서 열린다"),
		FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, TEXT("Booth"), TEXT("CraftExperience")));
	TestTrue(TEXT("두 번째 종류에서도 열린다"),
		FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, TEXT("Booth"), TEXT("ActivityGame")));
	TestFalse(TEXT("목록에 없는 종류는 안 된다"),
		FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, TEXT("Booth"), TEXT("Nightlife")));
	return true;
}

// 허용 목록이 비면 제약이 없는 것으로 본다.
// 여기서 거절하면 목록을 아직 안 채운 카드가 전부 편성 불가가 되어 저작 중에 기능이 막힌다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_EmptyFacilityListMeansNoConstraint, "FestaFiesta.Economy.ProgramScheduleRules.EmptyFacilityListMeansNoConstraint", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_EmptyFacilityListMeansNoConstraint::RunTest(const FString& Parameters)
{
	const FFF_ProgramCard Card;   // AllowedFacilityTypes 가 비어 있다

	TestTrue(TEXT("아무 시설에서나 열린다"),
		FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, TEXT("Toilet"), TEXT("Standard")));
	return true;
}

// 기본 카탈로그의 두 카드가 실제로 시설 종류를 저작해 두었는지 본다.
// 비워두면 위 규칙상 아무 시설에서나 열리므로, 저작 누락이 조용히 통과한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramSchedule_CatalogAuthorsFacilityTypes, "FestaFiesta.Economy.ProgramScheduleRules.CatalogAuthorsFacilityTypes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramSchedule_CatalogAuthorsFacilityTypes::RunTest(const FString& Parameters)
{
	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>();
	if (!TestNotNull(TEXT("정의를 만든다"), Definition)) return false;

	if (!TestTrue(TEXT("카탈로그가 비어 있지 않다"), Definition->ProgramCatalog.Num() > 0))
	{
		return false;
	}

	for (const FFF_ProgramCard& Card : Definition->ProgramCatalog)
	{
		TestTrue(*FString::Printf(TEXT("'%s' 에 허용 시설이 저작돼 있다"), *Card.ProgramId.ToString()),
			Card.AllowedFacilityTypes.Num() > 0);
		TestFalse(*FString::Printf(TEXT("'%s' 에 선호 아키타입이 저작돼 있다"), *Card.ProgramId.ToString()),
			Card.PreferredArchetypeId.IsNone());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
