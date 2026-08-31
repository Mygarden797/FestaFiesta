// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/Panels/FF_LivePanelFacilities.h"

/**
 * 라이브 시설 패널 판단 규칙 고정 테스트 (U6).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 여기서 확인해야 할 것은 전부 **라이브 중 한순간에만 나타나는 상태**다. 재배치가 필요한
 * 시설, 큐가 막힌 시설, 줄이 막 임계를 넘긴 시설을 손으로 만들려면 축제를 켜고 시설을
 * 놓고 손님이 몰릴 때까지 기다려야 하고, 그렇게 해도 원하는 조합이 잘 안 나온다.
 *
 * 셋을 못으로 박는다 —
 *   ① **판정 순서.** 덜 급한 이유가 더 급한 이유를 가리면 안 된다
 *   ② **모름을 정상으로 치지 않는 것**
 *   ③ **정렬이 급한 순인 것**, 그리고 값이 같을 때 순서가 흔들리지 않는 것
 */

namespace FFLiveFacilityTestLocal
{
	FFF_LiveFacilityView MakeView(
		const TCHAR* Name,
		EFF_LiveFacilityState State = EFF_LiveFacilityState::Active,
		EFF_LiveFacilityQueueState QueueState = EFF_LiveFacilityQueueState::Open,
		int32 Waiting = 0,
		bool bNeedsRelocation = false)
	{
		FFF_LiveFacilityView View;
		View.DisplayName = FText::FromString(Name);
		View.State = State;
		View.QueueState = QueueState;
		View.WaitingCount = Waiting;
		View.bNeedsRelocation = bNeedsRelocation;
		return View;
	}

	FFF_LiveFacilityRow MakeRow(const FFF_LiveFacilityView& View, int32 Threshold = 8)
	{
		FFF_LiveFacilityRow Row;
		Row.View = View;
		Row.Alert = FFF_LiveFacilityRules::ResolveAlert(View, Threshold);
		return Row;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveFacilityRules_Alert, "FestaFiesta.UI.LiveFacility.Alert", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveFacilityRules_Alert::RunTest(const FString& Parameters)
{
	using namespace FFLiveFacilityTestLocal;

	// ② 모름은 정상이 아니다. 화면이 `정상` 이라고 말하면 사람은 안 본다.
	TestEqual(TEXT("상태를 모르면 모름"),
		FFF_LiveFacilityRules::ResolveAlert(MakeView(TEXT("A"), EFF_LiveFacilityState::Unknown), 8),
		EFF_LiveFacilityAlert::Unknown);

	// 평범한 시설.
	TestEqual(TEXT("운영 중이고 줄이 짧으면 정상"),
		FFF_LiveFacilityRules::ResolveAlert(MakeView(TEXT("A")), 8),
		EFF_LiveFacilityAlert::Ok);

	// --- ① 순서 ---

	// ★ 재배치가 정원 참보다 먼저다. 뒤집히면 자리가 잘못된 시설이 `정원 참` 으로 보여서
	//   무엇을 고쳐야 하는지 알 수 없게 된다.
	TestEqual(TEXT("재배치가 정원 참보다 먼저"),
		FFF_LiveFacilityRules::ResolveAlert(
			MakeView(TEXT("A"), EFF_LiveFacilityState::Active,
				EFF_LiveFacilityQueueState::Full, 99, /*Relocate*/ true), 8),
		EFF_LiveFacilityAlert::Blocked);

	// ★ 모름이 재배치보다 먼저다. 못 읽은 값으로 "자리를 옮겨라" 라고 말하면 안 된다.
	TestEqual(TEXT("모름이 재배치보다 먼저"),
		FFF_LiveFacilityRules::ResolveAlert(
			MakeView(TEXT("A"), EFF_LiveFacilityState::Unknown,
				EFF_LiveFacilityQueueState::Open, 0, /*Relocate*/ true), 8),
		EFF_LiveFacilityAlert::Unknown);

	// 큐가 막힌 것과 운영 중이 아닌 것은 둘 다 막힘이다.
	TestEqual(TEXT("큐가 막히면 막힘"),
		FFF_LiveFacilityRules::ResolveAlert(
			MakeView(TEXT("A"), EFF_LiveFacilityState::Active, EFF_LiveFacilityQueueState::Blocked), 8),
		EFF_LiveFacilityAlert::Blocked);

	for (const EFF_LiveFacilityState State : {
		EFF_LiveFacilityState::Planned,
		EFF_LiveFacilityState::Validated,
		EFF_LiveFacilityState::Closed,
		EFF_LiveFacilityState::MovePending })
	{
		TestEqual(TEXT("운영 중이 아니면 막힘"),
			FFF_LiveFacilityRules::ResolveAlert(MakeView(TEXT("A"), State), 8),
			EFF_LiveFacilityAlert::Blocked);
	}

	// --- 대기 인원 임계 ---

	TestEqual(TEXT("임계 바로 아래는 정상"),
		FFF_LiveFacilityRules::ResolveAlert(
			MakeView(TEXT("A"), EFF_LiveFacilityState::Active, EFF_LiveFacilityQueueState::Open, 7), 8),
		EFF_LiveFacilityAlert::Ok);

	// 경계는 포함이다. `>` 로 쓰면 여기서 걸린다.
	TestEqual(TEXT("임계와 같으면 주의"),
		FFF_LiveFacilityRules::ResolveAlert(
			MakeView(TEXT("A"), EFF_LiveFacilityState::Active, EFF_LiveFacilityQueueState::Open, 8), 8),
		EFF_LiveFacilityAlert::Watch);

	// 임계를 끌 수 있어야 한다 — 확정 전 수치라 0 으로 두고 보는 경우가 있다.
	TestEqual(TEXT("임계가 0 이면 대기 인원을 보지 않는다"),
		FFF_LiveFacilityRules::ResolveAlert(
			MakeView(TEXT("A"), EFF_LiveFacilityState::Active, EFF_LiveFacilityQueueState::Open, 999), 0),
		EFF_LiveFacilityAlert::Ok);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveFacilityRules_Sort, "FestaFiesta.UI.LiveFacility.Sort", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveFacilityRules_Sort::RunTest(const FString& Parameters)
{
	using namespace FFLiveFacilityTestLocal;

	TArray<FFF_LiveFacilityRow> Rows;
	Rows.Add(MakeRow(MakeView(TEXT("정상"))));
	Rows.Add(MakeRow(MakeView(TEXT("주의"), EFF_LiveFacilityState::Active,
		EFF_LiveFacilityQueueState::Open, 20)));
	Rows.Add(MakeRow(MakeView(TEXT("모름"), EFF_LiveFacilityState::Unknown)));
	Rows.Add(MakeRow(MakeView(TEXT("막힘"), EFF_LiveFacilityState::Closed)));

	FFF_LiveFacilityRules::SortByUrgency(Rows);

	// ③ 급한 순이다. 이름순이면 라이브 중에 목록을 사람이 훑어야 한다.
	TestEqual(TEXT("막힘이 맨 위"), Rows[0].View.DisplayName.ToString(), FString(TEXT("막힘")));
	TestEqual(TEXT("주의가 그다음"), Rows[1].View.DisplayName.ToString(), FString(TEXT("주의")));

	// ★ 모름이 정상보다 위다. 읽지 못한 것은 확인이 필요한 것이지 괜찮다고 확인된 것이 아니다.
	TestEqual(TEXT("모름이 정상보다 위"), Rows[2].View.DisplayName.ToString(), FString(TEXT("모름")));
	TestEqual(TEXT("정상이 맨 아래"), Rows[3].View.DisplayName.ToString(), FString(TEXT("정상")));

	{
		// 같은 급이면 대기 인원이 많은 쪽이 위다.
		TArray<FFF_LiveFacilityRow> Tie;
		Tie.Add(MakeRow(MakeView(TEXT("적음"), EFF_LiveFacilityState::Active,
			EFF_LiveFacilityQueueState::Open, 10)));
		Tie.Add(MakeRow(MakeView(TEXT("많음"), EFF_LiveFacilityState::Active,
			EFF_LiveFacilityQueueState::Open, 30)));

		FFF_LiveFacilityRules::SortByUrgency(Tie);
		TestEqual(TEXT("대기 많은 쪽이 위"), Tie[0].View.DisplayName.ToString(), FString(TEXT("많음")));
	}

	{
		// ★ 값이 완전히 같으면 이름으로 고정한다. 안 그러면 갱신할 때마다 줄 순서가 흔들려
		//   읽는 중에 눈이 따라가지 못한다.
		TArray<FFF_LiveFacilityRow> Same;
		Same.Add(MakeRow(MakeView(TEXT("나나"))));
		Same.Add(MakeRow(MakeView(TEXT("가가"))));

		FFF_LiveFacilityRules::SortByUrgency(Same);
		TestEqual(TEXT("같으면 이름순으로 고정"), Same[0].View.DisplayName.ToString(), FString(TEXT("가가")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveFacilityRules_Reason, "FestaFiesta.UI.LiveFacility.Reason", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveFacilityRules_Reason::RunTest(const FString& Parameters)
{
	using namespace FFLiveFacilityTestLocal;

	// !! 색만으로 구분하지 않는다 !! (AT-011) 급한 줄에는 사유가 반드시 붙는다.
	for (const FFF_LiveFacilityRow& Row : {
		MakeRow(MakeView(TEXT("A"), EFF_LiveFacilityState::Active,
			EFF_LiveFacilityQueueState::Open, 0, /*Relocate*/ true)),
		MakeRow(MakeView(TEXT("B"), EFF_LiveFacilityState::Active, EFF_LiveFacilityQueueState::Blocked)),
		MakeRow(MakeView(TEXT("C"), EFF_LiveFacilityState::Closed)),
		MakeRow(MakeView(TEXT("D"), EFF_LiveFacilityState::Active, EFF_LiveFacilityQueueState::Full)),
		MakeRow(MakeView(TEXT("E"), EFF_LiveFacilityState::Active,
			EFF_LiveFacilityQueueState::Open, 20)),
		MakeRow(MakeView(TEXT("F"), EFF_LiveFacilityState::Unknown)) })
	{
		TestFalse(
			*FString::Printf(TEXT("%s 에 사유가 있다"), *Row.View.DisplayName.ToString()),
			FFF_LiveFacilityRules::DescribeAlert(Row).IsEmpty());
	}

	// 정상인 줄에는 사유를 붙이지 않는다. 전부에 문구가 붙으면 급한 줄이 안 도드라진다.
	TestTrue(TEXT("정상에는 사유가 없다"),
		FFF_LiveFacilityRules::DescribeAlert(MakeRow(MakeView(TEXT("A")))).IsEmpty());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
