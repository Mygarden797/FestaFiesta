// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/DataTable.h"
#include "Time/FF_FestivalStage.h"
#include "Time/FF_FestivalStageSubsystem.h"

/**
 * DT_DaySequence 가 경제의 정산 전제를 계속 만족하는지 본다.
 *
 * 이 표는 경제 소유가 아니다. 여기서 고정하는 것은 표의 내용이 아니라
 * **경제가 표에 대해 의존하고 있는 사실**이다 — 어느 날 어디서 하루 경계를 여는가.
 *
 * 🔴 깨졌을 때 표를 고쳐 맞추지 말 것. 흐름이 바뀌었다는 뜻이고, 바뀐 흐름에서
 * 하루 경계를 어디서 열어야 하는지 표 소유자와 정하는 것이 먼저다. 경계가
 * 조용히 사라지면 원장이 날짜별로 안 갈리고 어제 만족도가 크래시도 로그도 없이 빠진다.
 *
 * 단계 순서를 코드에서 grep 으로 확인할 수 없기 때문에 이 파일이 필요하다.
 * 전이 메커니즘(RequestTransition·OnStageChanged)만 코드에 있고 순서는 전부 이 표에 있다.
 */
namespace FFF_DaySequenceContractHelpers
{
	// 프로덕션과 같은 경로. FF_FestivalStageSubsystem.cpp 의 Initialize 가 이 리터럴을 쓴다.
	// 경로가 바뀌면 여기 로드가 실패해서 TableLoadsAtProductionPath 가 먼저 깨진다.
	const TCHAR* TablePath = TEXT("/Game/FestaFiesta/Data/Time/DT_DaySequence.DT_DaySequence");

	// 축제는 3일이다. HandleLiveEnd 의 `CurrentDay == 3` 과
	// AdvanceToNextDay 의 `CurrentDay >= 3` 두 곳이 이 값을 박고 있다.
	constexpr int32 LastDay = 3;

	// 단계 수 + 1. 체인이 이보다 길면 순환이다.
	constexpr int32 MaxChainLength = 9;

	UDataTable* LoadTable()
	{
		return LoadObject<UDataTable>(nullptr, TablePath);
	}

	int32 MakeKey(int32 Day, EFF_FestivalStage Stage)
	{
		return Day * 256 + static_cast<int32>(Stage);
	}

	FString StageName(EFF_FestivalStage Stage)
	{
		return StaticEnum<EFF_FestivalStage>()->GetNameStringByValue(static_cast<int64>(Stage));
	}

	// (Day, Stage) → 행. 중복 키는 OutDuplicates 로 따로 보고한다 —
	// 프로덕션 조회가 선형 탐색 후 첫 일치를 돌려주므로 중복은 조용히 가려진다.
	void BuildRowMap(
		const UDataTable& Table,
		TMap<int32, FFF_PhaseTimeRange>& OutRows,
		TArray<FString>& OutDuplicates)
	{
		TArray<FFF_PhaseTimeRange*> AllRows;
		Table.GetAllRows<FFF_PhaseTimeRange>(TEXT("FF_DaySequenceContractTests"), AllRows);

		for (const FFF_PhaseTimeRange* Row : AllRows)
		{
			if (Row == nullptr)
			{
				continue;
			}

			const int32 Key = MakeKey(Row->Day, Row->Stage);
			if (OutRows.Contains(Key))
			{
				OutDuplicates.Add(FString::Printf(
					TEXT("Day%d %s"), Row->Day, *StageName(Row->Stage)));
				continue;
			}
			OutRows.Add(Key, *Row);
		}
	}

	// 하루가 시작하는 단계.
	//
	// Day1 은 전이 없이 기본값에서 시작한다 — 그 기본값을 CDO 에서 읽어온다.
	// 여기 Setup 을 박으면 기본값이 바뀌었을 때 테스트가 낡은 전제로 통과한다.
	// Day2·3 은 AdvanceToNextDay 가 RequestTransition(MorningCheck) 을 부르는 것이 유일한 진입이다.
	EFF_FestivalStage GetStartStage(int32 Day)
	{
		if (Day <= 1)
		{
			return GetDefault<UFF_FestivalStageSubsystem>()->CurrentStage;
		}
		return EFF_FestivalStage::MorningCheck;
	}

	/**
	 * 하루의 단계 체인을 실제 진행 규칙대로 따라간다.
	 *
	 * ⚠️ 표만 따라가면 안 된다. Live 에는 행이 없다 — Live 의 길이는 별도 경로
	 * (GetLiveDurationForCurrentDay)에서 오고, 나가는 곳은 HandleLiveEnd 가 코드로 정한다
	 * (Day3 이면 Finale, 아니면 Closing). 그래서 Live 에서만 표를 벗어난다.
	 *
	 * Report 는 하루의 종점이다. Report 의 NextStage 는 쓰이지 않는다 —
	 * 다음 날로 넘기는 것은 AdvanceToNextDay 이고 그쪽이 MorningCheck 을 직접 지정한다.
	 */
	bool WalkDayChain(
		const TMap<int32, FFF_PhaseTimeRange>& Rows,
		int32 Day,
		TArray<EFF_FestivalStage>& OutChain,
		FString& OutFailure)
	{
		EFF_FestivalStage Current = GetStartStage(Day);

		for (int32 Step = 0; Step < MaxChainLength; ++Step)
		{
			if (OutChain.Contains(Current))
			{
				OutFailure = FString::Printf(
					TEXT("Day%d 체인이 %s 로 되돌아온다 (순환)"), Day, *StageName(Current));
				return false;
			}
			OutChain.Add(Current);

			if (Current == EFF_FestivalStage::Report)
			{
				return true;
			}

			if (Current == EFF_FestivalStage::Live)
			{
				Current = (Day == LastDay)
					? EFF_FestivalStage::Finale
					: EFF_FestivalStage::Closing;
				continue;
			}

			const FFF_PhaseTimeRange* Row = Rows.Find(MakeKey(Day, Current));
			if (Row == nullptr)
			{
				OutFailure = FString::Printf(
					TEXT("Day%d %s 행이 표에 없어서 체인이 끊긴다"), Day, *StageName(Current));
				return false;
			}
			Current = Row->NextStage;
		}

		OutFailure = FString::Printf(
			TEXT("Day%d 체인이 %d 단계를 넘겼다"), Day, MaxChainLength);
		return false;
	}

	// 표를 로드하고 행 맵까지 세운다. 실패하면 사유를 남기고 false.
	bool LoadRows(
		FAutomationTestBase& Test,
		TMap<int32, FFF_PhaseTimeRange>& OutRows)
	{
		UDataTable* Table = LoadTable();
		if (!Test.TestNotNull(TEXT("DT_DaySequence 를 프로덕션 경로에서 로드한다"), Table))
		{
			return false;
		}
		if (!Test.TestTrue(TEXT("RowStruct 가 FFF_PhaseTimeRange 다"),
			Table->RowStruct == FFF_PhaseTimeRange::StaticStruct()))
		{
			return false;
		}

		TArray<FString> Duplicates;
		BuildRowMap(*Table, OutRows, Duplicates);

		// 빈 표로 아래 단언들이 조용히 통과하는 것을 막는다.
		return Test.TestTrue(TEXT("행이 하나 이상 있다"), OutRows.Num() > 0);
	}
}

// 표가 프로덕션 경로에 실제로 있고 읽을 수 있다.
//
// 이 테스트가 없으면 경로가 바뀌었을 때 아래 테스트들이 "빈 표"로 통과한다.
// 프로덕션은 로드 실패 시 Warning 만 찍고 넘어가므로 게임은 단계가 안 넘어가는 상태가 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DaySequence_TableLoadsAtProductionPath, "FestaFiesta.Economy.DaySequence.TableLoadsAtProductionPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DaySequence_TableLoadsAtProductionPath::RunTest(const FString& Parameters)
{
	TMap<int32, FFF_PhaseTimeRange> Rows;
	if (!FFF_DaySequenceContractHelpers::LoadRows(*this, Rows))
	{
		return false;
	}

	// 3일 전부에 최소 한 행씩은 있어야 한다.
	for (int32 Day = 1; Day <= FFF_DaySequenceContractHelpers::LastDay; ++Day)
	{
		bool bFound = false;
		for (const TPair<int32, FFF_PhaseTimeRange>& Pair : Rows)
		{
			if (Pair.Value.Day == Day)
			{
				bFound = true;
				break;
			}
		}
		TestTrue(*FString::Printf(TEXT("Day%d 행이 있다"), Day), bFound);
	}

	return true;
}

// (Day, Stage) 가 중복되지 않는다.
//
// 프로덕션 조회는 모든 행을 선형 탐색해 **첫 일치**를 돌려준다. 같은 (Day, Stage) 가
// 둘이면 뒤쪽 행은 표에 보이는데도 무시된다 — 시간을 고쳐도 안 바뀌는 상태가 되고,
// 에디터에서는 두 행이 다 보이므로 원인을 찾기 어렵다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DaySequence_NoDuplicateDayStageRows, "FestaFiesta.Economy.DaySequence.NoDuplicateDayStageRows", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DaySequence_NoDuplicateDayStageRows::RunTest(const FString& Parameters)
{
	UDataTable* Table = FFF_DaySequenceContractHelpers::LoadTable();
	if (!TestNotNull(TEXT("DT_DaySequence 를 로드한다"), Table))
	{
		return false;
	}

	TMap<int32, FFF_PhaseTimeRange> Rows;
	TArray<FString> Duplicates;
	FFF_DaySequenceContractHelpers::BuildRowMap(*Table, Rows, Duplicates);

	TestEqual(
		*FString::Printf(TEXT("중복된 (Day, Stage) 가 없다: %s"), *FString::Join(Duplicates, TEXT(", "))),
		Duplicates.Num(), 0);

	return true;
}

// 3일 전부 체인이 Live 를 지나 Report 로 끝난다.
//
// 체인이 끊기거나 순환하면 그 날은 진행이 불가능하고, 그 뒤의 정산·결산이 전부 안 온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DaySequence_EveryDayChainReachesLiveAndTerminates, "FestaFiesta.Economy.DaySequence.EveryDayChainReachesLiveAndTerminates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DaySequence_EveryDayChainReachesLiveAndTerminates::RunTest(const FString& Parameters)
{
	TMap<int32, FFF_PhaseTimeRange> Rows;
	if (!FFF_DaySequenceContractHelpers::LoadRows(*this, Rows))
	{
		return false;
	}

	for (int32 Day = 1; Day <= FFF_DaySequenceContractHelpers::LastDay; ++Day)
	{
		TArray<EFF_FestivalStage> Chain;
		FString Failure;

		// 인자 평가 순서가 정해져 있지 않으므로 먼저 걷고 나서 단언한다 —
		// TestTrue 안에서 같이 부르면 Failure 가 빈 문자열로 찍힐 수 있다.
		const bool bWalked = FFF_DaySequenceContractHelpers::WalkDayChain(Rows, Day, Chain, Failure);
		if (!TestTrue(
			*FString::Printf(TEXT("Day%d 체인이 Report 까지 이어진다 (%s)"), Day, *Failure),
			bWalked))
		{
			continue;
		}

		TestTrue(*FString::Printf(TEXT("Day%d 체인에 Live 가 있다"), Day),
			Chain.Contains(EFF_FestivalStage::Live));
		TestTrue(*FString::Printf(TEXT("Day%d 체인에 Closing 이 있다"), Day),
			Chain.Contains(EFF_FestivalStage::Closing));
	}

	return true;
}

// 체인에 실제로 들어가는 단계는 진행 가능한 길이를 갖는다.
//
// 🔴 MaxSeconds <= 0 이면 Tick 이 조기 반환해서 **그 단계에서 영구히 멈춘다**
// (FF_FestivalStageSubsystem.cpp 의 `if (Range.MaxSeconds <= 0.f) return;`).
// 크래시도 로그도 없이 시간만 안 흐르므로, 표에 행을 빼먹었을 때 이 테스트가 잡는다.
//
// ★ 2026-09-03 에 전제를 넓혔다. 보는 것은 `재생 가능한 길이`가 아니라 **나갈 수단**이다 —
//   수동 버튼(`bSkippable`)이 있으면 시간 제한이 없어도 멈추지 않는다. 자세한 것은 아래.
//
// ⚠️ 이 테스트는 혼자서는 아무것도 보증하지 못한다 — 체인이 비면 순회할 것이 없어
// 공허하게 통과한다(변이 검사로 확인했다). 체인이 실제로 걸리는지는
// EveryDayChainReachesLiveAndTerminates 가 본다. 그쪽이 빨간불이면 이 테스트의
// 초록불은 의미가 없으므로 먼저 그쪽을 고쳐야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DaySequence_ReachableStagesHavePlayableDuration, "FestaFiesta.Economy.DaySequence.ReachableStagesHavePlayableDuration", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DaySequence_ReachableStagesHavePlayableDuration::RunTest(const FString& Parameters)
{
	TMap<int32, FFF_PhaseTimeRange> Rows;
	if (!FFF_DaySequenceContractHelpers::LoadRows(*this, Rows))
	{
		return false;
	}

	for (int32 Day = 1; Day <= FFF_DaySequenceContractHelpers::LastDay; ++Day)
	{
		TArray<EFF_FestivalStage> Chain;
		FString Failure;
		if (!FFF_DaySequenceContractHelpers::WalkDayChain(Rows, Day, Chain, Failure))
		{
			// 체인 자체가 깨진 것은 위 테스트가 보고한다.
			continue;
		}

		for (const EFF_FestivalStage Stage : Chain)
		{
			// Live 만 예외다. Live 의 길이는 이 표가 아니라 GetLiveDurationForCurrentDay 가 갖는다.
			if (Stage == EFF_FestivalStage::Live)
			{
				continue;
			}

			const FFF_PhaseTimeRange* Row = Rows.Find(
				FFF_DaySequenceContractHelpers::MakeKey(Day, Stage));
			if (Row == nullptr)
			{
				continue;
			}

			// 🔴 **전제를 넓혔다 (2026-09-03 백종태).** 원래는 `MaxSeconds > 0` 만 봤다.
			//
			// 이 테스트가 지키려는 것은 `MaxSeconds` 자체가 아니라 **단계에서 나갈 수 있는가**
			// 다. `MaxSeconds <= 0` 이면 `Tick` 이 곧바로 return 해서 시간이 안 흐르고
			// 자동 전환도 안 되므로, 나갈 다른 수단이 없으면 게임이 조용히 멈춘다.
			//
			// 그런데 마감에 **수동 버튼**이 생기면서 (리포트 패널의 `다음 날로`) 시간 제한이
			// 필요 없어졌다 — 결산을 다 읽었는지는 플레이어가 안다. 그래서 마감은
			// `MaxSeconds = 0` + `bSkippable` 조합이 정상이 됐다.
			//
			// 나갈 수단은 셋이다 —
			//   ① MaxSeconds > 0   시간이 지나면 자동으로
			//   ② bSkippable       `SkipCurrentStage()` 로 수동으로
			//   ③ Report 단계      `AdvanceToNextDay()` 가 여기서만 통한다 (bSkippable 무관)
			const bool bCanLeaveByTime = Row->MaxSeconds > 0.f;
			const bool bCanLeaveByHand =
				Row->bSkippable || Stage == EFF_FestivalStage::Report;

			TestTrue(
				*FString::Printf(
					TEXT("Day%d %s 에서 나갈 수단이 있다 (MaxSeconds>0 이거나 bSkippable) "
						 "— 현재 MaxSeconds=%.1f · bSkippable=%s"),
					Day, *FFF_DaySequenceContractHelpers::StageName(Stage),
					Row->MaxSeconds, Row->bSkippable ? TEXT("켜짐") : TEXT("꺼짐")),
				bCanLeaveByTime || bCanLeaveByHand);
		}
	}

	return true;
}

// 🔴 경제의 정산 지점이 표와 일치한다. 이 파일에서 가장 중요한 단언이다.
//
// 하루 경계(원장 스냅샷·화제성 감쇠·어제 만족도 기록)는 하루에 정확히 한 번 열려야 하고,
// 여는 곳이 날마다 다르다:
//   Day1  MorningCheck 을 지나지 않는다 → StartFestival 이 연다
//   Day2·3 MorningCheck 을 지난다      → StageEconomyBridge 가 연다
//
// Day1 에 MorningCheck 이 생기면 경계가 두 번 열리지 않는지 다시 봐야 하고,
// Day2·3 에서 MorningCheck 이 없어지면 그 날 경계가 아예 안 열린다.
// 둘 다 결산·평가를 조용히 어긋내므로 표가 바뀌는 순간 알아야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DaySequence_SettlementStagesMatchEconomyAssumption, "FestaFiesta.Economy.DaySequence.SettlementStagesMatchEconomyAssumption", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DaySequence_SettlementStagesMatchEconomyAssumption::RunTest(const FString& Parameters)
{
	TMap<int32, FFF_PhaseTimeRange> Rows;
	if (!FFF_DaySequenceContractHelpers::LoadRows(*this, Rows))
	{
		return false;
	}

	for (int32 Day = 1; Day <= FFF_DaySequenceContractHelpers::LastDay; ++Day)
	{
		TArray<EFF_FestivalStage> Chain;
		FString Failure;
		if (!FFF_DaySequenceContractHelpers::WalkDayChain(Rows, Day, Chain, Failure))
		{
			continue;
		}

		const bool bHasMorningCheck = Chain.Contains(EFF_FestivalStage::MorningCheck);

		if (Day == 1)
		{
			TestFalse(
				TEXT("Day1 은 MorningCheck 을 지나지 않는다 — 그래서 StartFestival 이 경계를 연다"),
				bHasMorningCheck);

			// ⚠️ 위 단언은 한쪽만 본다 — 체인이 비어도 통과한다(변이 검사로 확인했다).
			// 아래 줄이 그 구멍을 막는다. 둘을 같이 둬야 "체인을 실제로 걸었고,
			// 그 안에 MorningCheck 이 없다"가 된다. 하나만 지우지 말 것.
			//
			// Day1 의 소비 단계는 Setup 이다. 이것이 Day2·3 의 MorningCheck 과 같은 슬롯이고,
			// 하루 경계가 "돈 쓰는 단계 시작"에 열린다는 규칙이 세 날 모두 성립하는 근거다.
			TestTrue(TEXT("Day1 에는 Setup 이 있다"),
				Chain.Contains(EFF_FestivalStage::Setup));
		}
		else
		{
			TestTrue(
				*FString::Printf(
					TEXT("Day%d 는 MorningCheck 을 지난다 — 브리지가 여기서 경계를 연다"), Day),
				bHasMorningCheck);
		}
	}

	// 브리지가 MorningCheck 만 경계로 보는지는 여기서 다시 보지 않는다 —
	// FF_StageEconomyBridgeTests.cpp 의 OnlyMorningCheckIsDayBoundary 가 갖고 있다.
	// 이 파일은 표 쪽만, 그 파일은 코드 쪽만 본다. 둘이 같이 초록불이어야 정산이 성립한다.

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
