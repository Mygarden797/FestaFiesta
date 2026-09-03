// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "DataTables/FF_ArchetypeRow.h"
#include "Engine/DataTable.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Time/FF_FestivalStageSubsystem.h"

/**
 * 프로그램 카드가 자기 밖의 두 표와 계속 맞물려 있는지 본다.
 *
 * 카드는 혼자서는 유효해 보이는데 **다른 표와 어긋나면 조용히 망가지는** 값을 둘 갖고 있다:
 * 시각(`DT_ProgramSchedule` 의 그날 Live 길이)과 아키타입(`DT_NpcArchetypes` 의 ArchetypeId).
 * 둘 다 어긋나도 크래시가 안 나고 편성도 되고 정원도 채워진다 — 그래서 이 파일이 필요하다.
 *
 * 🔴 깨졌을 때 표를 고쳐 맞추지 말 것. 카드 저작이 틀린 것이 먼저 의심할 쪽이다.
 * `FF_DaySequenceContractTests` 와 같은 성격이고, 고정하는 것은 표의 내용이 아니라
 * **경제가 표에 대해 의존하고 있는 사실**이다.
 */
namespace FFF_ProgramCardContractHelpers
{
	// 프로덕션과 같은 경로. FF_FestivalStageSubsystem.cpp 의 Initialize 가 이 리터럴을 쓴다.
	const TCHAR* ScheduleTablePath =
		TEXT("/Game/FestaFiesta/Data/Time/DT_ProgramSchedule.DT_ProgramSchedule");

	TArray<FFF_ProgramCard> LoadCatalog()
	{
		// 카탈로그는 UFF_FestivalDefinition 생성자가 채운다. 축제를 시작할 필요가 없다.
		const UFF_FestivalDefinition* Definition =
			NewObject<UFF_FestivalDefinition>(GetTransientPackage());
		return Definition != nullptr ? Definition->ProgramCatalog : TArray<FFF_ProgramCard>();
	}

	/**
	 * 표에서 **가장 짧은 날**의 Live 길이(초)를 찾는다.
	 *
	 * 날마다 다르다(실측 Day1 240 / Day2·3 300). 카드는 날짜를 안 가리고 아무 날에나
	 * 편성되므로, 가장 짧은 날에 들어가지 않는 카드는 그날 시작 시각이 영영 안 온다.
	 *
	 * @return 행이 하나도 없으면 0
	 */
	float FindShortestDayLiveSeconds(const UDataTable& Table)
	{
		TArray<FFF_LiveTriggerEvent*> AllRows;
		Table.GetAllRows<FFF_LiveTriggerEvent>(TEXT("FF_ProgramCardContractTests"), AllRows);

		// Day → 그날 마지막 이벤트 시각. GetLiveDurationForCurrentDay 와 같은 정의다.
		TMap<int32, float> LastEventByDay;
		for (const FFF_LiveTriggerEvent* Row : AllRows)
		{
			if (Row == nullptr) continue;

			float& Last = LastEventByDay.FindOrAdd(Row->Day, 0.f);
			Last = FMath::Max(Last, Row->TriggerSeconds);
		}

		float Shortest = 0.f;
		for (const TPair<int32, float>& Pair : LastEventByDay)
		{
			if (Pair.Value <= 0.f) continue;
			Shortest = (Shortest <= 0.f) ? Pair.Value : FMath::Min(Shortest, Pair.Value);
		}

		return Shortest;
	}
}

/**
 * 🔴 모든 카드가 **가장 짧은 날** 안에서 시작하고 끝난다.
 *
 * 체험비는 카드의 `StartMinutes` 가 지나는 순간 걷힌다(`economy_spec.md` §3.9).
 * 시각이 그날 Live 길이를 넘으면 그 순간이 안 와서 시작 정산이 카드를 영영 못 보고,
 * Closing 안전망으로 떨어져 Warning 만 남는다. **그 Warning 을 읽기 전에 여기서 잡는다.**
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramCardContract_CardTimesFitInsideShortestDay, "FestaFiesta.Economy.ProgramCardContract.CardTimesFitInsideShortestDay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramCardContract_CardTimesFitInsideShortestDay::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramCardContractHelpers;

	UDataTable* Table = LoadObject<UDataTable>(nullptr, ScheduleTablePath);
	if (!TestNotNull(TEXT("DT_ProgramSchedule 을 프로덕션 경로에서 로드한다"), Table))
	{
		return false;
	}

	const float ShortestLiveSeconds = FindShortestDayLiveSeconds(*Table);
	if (!TestTrue(TEXT("가장 짧은 날의 Live 길이를 찾는다"), ShortestLiveSeconds > 0.f))
	{
		return false;
	}

	// 1 Stage초 = 1 축제분 (Stage 시간 API). 이 환산이 바뀌면 여기도 같이 바뀐다.
	const float ShortestLiveMinutes = ShortestLiveSeconds * UFF_FestivalStageSubsystem::StageSecondsToFestivalMinutes(1.f);

	const TArray<FFF_ProgramCard> Catalog = LoadCatalog();
	if (!TestTrue(TEXT("카탈로그가 비어 있지 않다"), Catalog.Num() > 0)) return false;

	for (const FFF_ProgramCard& Card : Catalog)
	{
		const FString CardName = Card.ProgramId.ToString();

		TestTrue(*FString::Printf(TEXT("'%s' 시작 시각이 0 이상이다"), *CardName),
			Card.StartMinutes >= 0.f);
		TestTrue(*FString::Printf(TEXT("'%s' 종료가 시작보다 뒤다"), *CardName),
			Card.EndMinutes > Card.StartMinutes);

		// 🔴 시작이 하루를 넘으면 체험비가 안 걷힌다. 종료가 넘으면 손님이 프로그램에 묶인 채
		// 하루가 끝나서 퇴장 금지가 안 풀린다. 둘 다 막는다.
		TestTrue(*FString::Printf(
			TEXT("'%s' 시작(%.0f분)이 가장 짧은 날(%.0f분) 안이다"),
			*CardName, Card.StartMinutes, ShortestLiveMinutes),
			Card.StartMinutes < ShortestLiveMinutes);
		TestTrue(*FString::Printf(
			TEXT("'%s' 종료(%.0f분)가 가장 짧은 날(%.0f분) 안이다"),
			*CardName, Card.EndMinutes, ShortestLiveMinutes),
			Card.EndMinutes <= ShortestLiveMinutes);
	}

	return true;
}

/**
 * 🔴 모든 카드의 `PreferredArchetypeId` 가 아키타입 표에 실제로 있다.
 *
 * 프로그램을 사면 정원만큼의 손님이 **이 아키타입으로 태어난다** — 일반 방문객처럼 표에서
 * 추첨하는 것이 아니라 카드 값이 그대로 찍힌다(`FFF_VisitorDistributionBuilder::Build` 의
 * 예약석 경로). 그래서 오타가 있어도 **아무도 안 막는다**: 스폰도 되고 정원도 채워지고
 * 체험비도 정상적으로 걷힌 뒤, "참가자들이 취향대로 안 움직인다"로 뒤늦게 드러난다.
 *
 * 비워 두는 것은 저작 선택이라 통과시킨다(아키타입 없는 손님이 온다).
 * 막는 것은 **채워져 있는데 표에 없는 값**이다.
 *
 * 대소문자 오타는 여기서 안 걸리고, 걸릴 필요도 없다 — FName 비교가 대소문자를 무시해서
 * `life_stage_20S` 는 런타임에도 정상 동작한다. 잡는 것은 실제로 다른 값뿐이다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramCardContract_CardArchetypesExistInTable, "FestaFiesta.Economy.ProgramCardContract.CardArchetypesExistInTable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramCardContract_CardArchetypesExistInTable::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramCardContractHelpers;

	// 프로덕션과 같은 창구로 표를 찾는다. 경로를 여기 박으면 설정이 바뀌어도 안 깨진다.
	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	if (!TestNotNull(TEXT("Simulation 설정이 있다"), Settings)) return false;

	const UDataTable* ArchetypeTable = Settings->ArchetypeTable.LoadSynchronous();
	if (!TestNotNull(TEXT("설정에 지정된 아키타입 표를 로드한다"), ArchetypeTable))
	{
		return false;
	}

	TArray<FFF_ArchetypeRow*> AllRows;
	ArchetypeTable->GetAllRows<FFF_ArchetypeRow>(TEXT("FF_ProgramCardContractTests"), AllRows);

	TSet<FName> KnownArchetypes;
	for (const FFF_ArchetypeRow* Row : AllRows)
	{
		if (Row != nullptr && !Row->ArchetypeId.IsNone())
		{
			KnownArchetypes.Add(Row->ArchetypeId);
		}
	}

	if (!TestTrue(TEXT("아키타입 표에 행이 있다"), KnownArchetypes.Num() > 0)) return false;

	const TArray<FFF_ProgramCard> Catalog = LoadCatalog();
	if (!TestTrue(TEXT("카탈로그가 비어 있지 않다"), Catalog.Num() > 0)) return false;

	for (const FFF_ProgramCard& Card : Catalog)
	{
		if (Card.PreferredArchetypeId.IsNone())
		{
			// 저작 선택이다. 아키타입 없는 손님이 온다.
			continue;
		}

		TestTrue(*FString::Printf(
			TEXT("'%s' 의 아키타입 '%s' 이 표에 있다"),
			*Card.ProgramId.ToString(), *Card.PreferredArchetypeId.ToString()),
			KnownArchetypes.Contains(Card.PreferredArchetypeId));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
