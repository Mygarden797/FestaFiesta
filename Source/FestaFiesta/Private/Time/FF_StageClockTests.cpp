// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UObject/UnrealType.h"

/**
 * 시간 조회 창구가 Live 가 아닌 단계를 걸러내는지 본다.
 *
 * 🔴 이것이 이 창구가 존재하는 이유다. `ElapsedLiveSeconds` 는 이름과 달리
 * **단계가 바뀔 때마다 0 으로 리셋되고 모든 단계에서 누적된다.** raw 로 읽으면
 * Setup 에서 30초를 서성인 것이 "Live 30분"으로 읽혀 프로그램이 미리 열린다.
 *
 * Stage 는 표를 Initialize 에서 읽으므로 여기서는 표 없이 만든다 — Tick 이 돌아도
 * 단계 시간 데이터가 없어 아무 일도 하지 않는다.
 */
namespace FFF_StageTimeTestHelpers
{
	// Stage 는 ClassWithin 이 UGameInstance 다. 트랜지언트 패키지에 바로 만들면 ensure 가 난다.
	struct FClockHarness
	{
		bool Initialize(EFF_FestivalStage InStage, float InElapsedSeconds)
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			if (GameInstance == nullptr) return false;

			Stage = NewObject<UFF_FestivalStageSubsystem>(GameInstance);
			if (Stage == nullptr) return false;

			Stage->CurrentStage = InStage;
			Stage->ElapsedLiveSeconds = InElapsedSeconds;
			return true;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalStageSubsystem* Stage = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageTime_MainProgramQueueBreak,
	"FestaFiesta.Time.StageClock.MainProgramQueueBreak",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageTime_MainProgramQueueBreak::RunTest(const FString& Parameters)
{
	FFF_StageTimeTestHelpers::FClockHarness Harness;
	if (!TestTrue(TEXT("Create stage"), Harness.Initialize(EFF_FestivalStage::Live, 0.f))) return false;
	UDataTable* Table = NewObject<UDataTable>(Harness.GameInstance);
	Table->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	auto AddEvent = [Table](const FName Name, const int32 Day, const float Seconds, const bool bStart)
	{
		FFF_LiveTriggerEvent Row;
		Row.Day = Day;
		Row.TriggerSeconds = Seconds;
		Row.bIsMainProgramLockStart = bStart;
		Table->AddRow(Name, Row);
	};
	AddEvent(TEXT("SecondEnd"), 1, 270.f, false);
	AddEvent(TEXT("FirstStart"), 1, 180.f, true);
	AddEvent(TEXT("FirstEnd"), 1, 210.f, false);
	AddEvent(TEXT("SecondStart"), 1, 240.f, true);
	AddEvent(TEXT("Day2Start"), 2, 120.f, true);
	AddEvent(TEXT("Day2End"), 2, 150.f, false);
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("Schedule property"), Property)) return false;
	Property->SetObjectPropertyValue_InContainer(Harness.Stage, Table);

	for (const float Scale : { 1.f, 2.f, 4.f })
	{
		Harness.Stage->TimeScale = Scale;
		Harness.Stage->ElapsedLiveSeconds = 176.99f;
		TestFalse(TEXT("No interruption before 19:57 at any speed"), Harness.Stage->IsMainProgramQueueBreakActive());
		Harness.Stage->ElapsedLiveSeconds = 177.f;
		TestEqual(TEXT("Boundary is 19:57, not 180 real seconds early"), Harness.Stage->GetCurrentLiveClockString(), FString(TEXT("19:57")));
		TestTrue(TEXT("Three festival minutes before 20:00"), Harness.Stage->IsMainProgramQueueBreakActive());
	}
	float Start = -1.f;
	float End = -1.f;
	Harness.Stage->ElapsedLiveSeconds = 180.2f;
	TestTrue(TEXT("A frame crossing show start still enters the break"),
		Harness.Stage->TryGetMainProgramQueueBreakWindow(Start, End));
	TestEqual(TEXT("Actual start"), Start, 180.f);
	TestEqual(TEXT("Actual end"), End, 210.f);
	Harness.Stage->ElapsedLiveSeconds = 210.f;
	TestFalse(TEXT("Reopens exactly at show end"), Harness.Stage->IsMainProgramQueueBreakActive());
	Harness.Stage->ElapsedLiveSeconds = 237.f;
	TestTrue(TEXT("Next show has its own three-minute boundary"), Harness.Stage->IsMainProgramQueueBreakActive());
	Harness.Stage->CurrentDay = 2;
	Harness.Stage->ElapsedLiveSeconds = 117.f;
	TestTrue(TEXT("New day uses its own schedule"), Harness.Stage->IsMainProgramQueueBreakActive());
	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;
	TestFalse(TEXT("Setup never interrupts queues"), Harness.Stage->IsMainProgramQueueBreakActive());
	Harness.Stage->CurrentStage = EFF_FestivalStage::Live;
	Harness.Stage->CurrentDay = 3;
	TestFalse(TEXT("No scheduled show means no break"), Harness.Stage->IsMainProgramQueueBreakActive());
	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageTime_OnlyLiveStageHasATime, "FestaFiesta.Time.StageClock.OnlyLiveStageHasATime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageTime_OnlyLiveStageHasATime::RunTest(const FString& Parameters)
{
	using namespace FFF_StageTimeTestHelpers;

	float Minutes = -1.f;

	// 🔴 Setup 에서도 초가 쌓인다. 그것을 Live 시각으로 읽으면 프로그램이 아침에 열린다.
	FClockHarness Harness;
	if (!TestTrue(TEXT("Stage 를 만든다"), Harness.Initialize(EFF_FestivalStage::Setup, 90.f)))
	{
		return false;
	}

	TestFalse(TEXT("Setup 경과는 Live 시각이 아니다"),
		Harness.Stage->TryGetLiveElapsedMinutes(Minutes));
	TestEqual(TEXT("여전히 결과를 건드리지 않는다"), Minutes, -1.f);

	TestFalse(TEXT("일정이 없으면 길이 조회 실패"), Harness.Stage->TryGetLiveDurationMinutes(Minutes));
	TestEqual(TEXT("길이 조회 실패도 출력 유지"), Minutes, -1.f);

	// Closing 도 마찬가지다. 마감 안전망이 시작 정산으로 오인되면 안 된다.
	Harness.Stage->CurrentStage = EFF_FestivalStage::Closing;
	TestFalse(TEXT("Closing 경과도 Live 시각이 아니다"),
		Harness.Stage->TryGetLiveElapsedMinutes(Minutes));

	// 다음 GC 까지 Tick 이 돌 수 있다. 아무 일도 안 하는 단계로 돌려놓는다.
	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;

	return true;
}

// 1 Stage초 = 1 축제분. 밤 조명 트리거 90초와 선셋 요가 카드 90분이 이 계약 위에 서 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageTime_OneSecondIsOneMinute, "FestaFiesta.Time.StageClock.OneSecondIsOneMinute", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageTime_OneSecondIsOneMinute::RunTest(const FString& Parameters)
{
	using namespace FFF_StageTimeTestHelpers;

	FClockHarness Harness;
	if (!TestTrue(TEXT("Stage 를 만든다"), Harness.Initialize(EFF_FestivalStage::Live, 90.f)))
	{
		return false;
	}

	float Minutes = 0.f;
	if (!TestTrue(TEXT("Live 면 시각을 준다"),
		Harness.Stage->TryGetLiveElapsedMinutes(Minutes))) return false;

	TestEqual(TEXT("90초는 90분이다"), Minutes, 90.f);
	TestEqual(TEXT("이동 시간도 동일한 환산을 쓴다"), UFF_FestivalStageSubsystem::StageSecondsToFestivalMinutes(2.5f), 2.5f);

	// 개장 직후. 예약 손님이 도착하는 시점이고 아직 아무 프로그램도 안 열렸다.
	Harness.Stage->ElapsedLiveSeconds = 0.f;
	if (TestTrue(TEXT("개장 직후에도 시각을 준다"),
		Harness.Stage->TryGetLiveElapsedMinutes(Minutes)))
	{
		TestEqual(TEXT("0초는 0분이다"), Minutes, 0.f);
	}

	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageTime_DurationUsesCurrentDay, "FestaFiesta.Time.StageClock.DurationUsesCurrentDay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageTime_DurationUsesCurrentDay::RunTest(const FString& Parameters)
{
	FFF_StageTimeTestHelpers::FClockHarness Harness;
	if (!TestTrue(TEXT("Stage 생성"), Harness.Initialize(EFF_FestivalStage::Setup, 0.f))) return false;
	UDataTable* Table = NewObject<UDataTable>(Harness.GameInstance);
	Table->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	FFF_LiveTriggerEvent Row;
	Row.Day = 1;
	Row.TriggerSeconds = 240.f;
	Table->AddRow(TEXT("Day1End"), Row);
	Row.Day = 2;
	Row.TriggerSeconds = 300.f;
	Table->AddRow(TEXT("Day2End"), Row);
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("일정 테이블 속성"), Property)) return false;
	Property->SetObjectPropertyValue_InContainer(Harness.Stage, Table);
	float Minutes = -1.f;
	Harness.Stage->CurrentDay = 1;
	TestTrue(TEXT("Setup에서도 당일 길이 조회"), Harness.Stage->TryGetLiveDurationMinutes(Minutes));
	TestEqual(TEXT("Day1 길이"), Minutes, 240.f);
	Harness.Stage->CurrentDay = 2;
	TestTrue(TEXT("Day2 길이 조회"), Harness.Stage->TryGetLiveDurationMinutes(Minutes));
	TestEqual(TEXT("Day2 길이"), Minutes, 300.f);
	Harness.Stage->CurrentDay = 3;
	TestFalse(TEXT("일정이 없는 날 거부"), Harness.Stage->TryGetLiveDurationMinutes(Minutes));
	TestEqual(TEXT("실패 시 출력 유지"), Minutes, 300.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageTime_MultipleMainPrograms,
	"FestaFiesta.Time.StageClock.MultipleMainPrograms",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageTime_MultipleMainPrograms::RunTest(const FString& Parameters)
{
	FFF_StageTimeTestHelpers::FClockHarness Harness;
	if (!TestTrue(TEXT("Stage 생성"), Harness.Initialize(EFF_FestivalStage::Live, 0.f))) return false;
	UDataTable* Table = NewObject<UDataTable>(Harness.GameInstance);
	Table->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	FFF_LiveTriggerEvent Row;
	Row.Day = 1;
	Row.TriggerSeconds = 100.f;
	Row.EventLabel = TEXT("첫 공연");
	Row.bIsMainProgramLockStart = true;
	Table->AddRow(TEXT("Main1"), Row);
	Row.TriggerSeconds = 120.f;
	Row.EventLabel = TEXT("첫 공연 종료");
	Row.bIsMainProgramLockStart = false;
	Table->AddRow(TEXT("Main1End"), Row);
	Row.TriggerSeconds = 200.f;
	Row.EventLabel = TEXT("두 번째 공연");
	Row.bIsMainProgramLockStart = true;
	Table->AddRow(TEXT("Main2"), Row);
	Row.TriggerSeconds = 220.f;
	Row.EventLabel = TEXT("두 번째 공연 종료");
	Row.bIsMainProgramLockStart = false;
	Table->AddRow(TEXT("Main2End"), Row);
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("일정 테이블 속성"), Property)) return false;
	Property->SetObjectPropertyValue_InContainer(Harness.Stage, Table);
	Harness.Stage->ElapsedLiveSeconds = 95.f;
	TestTrue(TEXT("첫 공연 락"), Harness.Stage->IsMainProgramLocked());
	Harness.Stage->ElapsedLiveSeconds = 150.f;
	TestFalse(TEXT("공연 사이에는 락 해제"), Harness.Stage->IsMainProgramLocked());
	Harness.Stage->ElapsedLiveSeconds = 195.f;
	TestTrue(TEXT("두 번째 공연도 락"), Harness.Stage->IsMainProgramLocked());
	Harness.Stage->ElapsedLiveSeconds = 221.f;
	TestFalse(TEXT("두 번째 공연 종료 후 락 해제"), Harness.Stage->IsMainProgramLocked());

	// 잠금 진입과 공연 시각은 리드타임만큼 떨어져 있다. 집결 구간을 아는 유일한 창구다.
	float StartSeconds = -1.f;
	Harness.Stage->ElapsedLiveSeconds = 95.f;
	TestTrue(TEXT("집결 구간에서 공연 시각을 알 수 있다"),
		Harness.Stage->TryGetActiveMainProgramStartSeconds(StartSeconds));
	TestEqual(TEXT("공연 시각은 잠금 진입 시각이 아니라 TriggerSeconds 다"), StartSeconds, 100.f);
	Harness.Stage->ElapsedLiveSeconds = 150.f;
	StartSeconds = -1.f;
	TestFalse(TEXT("공연 사이에는 활성 공연이 없다"),
		Harness.Stage->TryGetActiveMainProgramStartSeconds(StartSeconds));
	TestEqual(TEXT("실패하면 출력값을 건드리지 않는다"), StartSeconds, -1.f);
	Harness.Stage->ElapsedLiveSeconds = 205.f;
	TestTrue(TEXT("두 번째 공연도 시각을 돌려준다"),
		Harness.Stage->TryGetActiveMainProgramStartSeconds(StartSeconds));
	TestEqual(TEXT("두 번째 공연 시각"), StartSeconds, 200.f);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageTime_FinishFestival,
	"FestaFiesta.Time.StageClock.FinishFestival",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageTime_FinishFestival::RunTest(const FString& Parameters)
{
	FFF_StageTimeTestHelpers::FClockHarness Harness;
	if (!TestTrue(TEXT("Create stage"), Harness.Initialize(EFF_FestivalStage::Closing, 0.f))) return false;

	// 마지막 날이 아니면 끝나지 않는다.
	Harness.Stage->CurrentDay = 2;
	TestFalse(TEXT("2일차 마감에서는 축제가 끝나지 않는다"), Harness.Stage->FinishFestival());
	TestFalse(TEXT("끝난 것으로 기록되지 않는다"), Harness.Stage->HasFestivalEnded());

	// 마지막 날이라도 Live 중에는 끝낼 수 없다.
	Harness.Stage->CurrentDay = 3;
	Harness.Stage->CurrentStage = EFF_FestivalStage::Live;
	TestFalse(TEXT("Live 중에는 끝낼 수 없다"), Harness.Stage->FinishFestival());

	// Report 행이 없어도 마감에서 끝낼 수 있다. 두 번째 호출은 아무 일도 하지 않는다.
	Harness.Stage->CurrentStage = EFF_FestivalStage::Closing;
	TestTrue(TEXT("3일차 마감에서 축제를 끝낸다"), Harness.Stage->FinishFestival());
	TestTrue(TEXT("종료가 기록된다"), Harness.Stage->HasFestivalEnded());
	TestFalse(TEXT("두 번 끝내지 않는다"), Harness.Stage->FinishFestival());

	// 새 축제를 시작하면 다시 끝낼 수 있다.
	Harness.Stage->StartNewFestival();
	TestFalse(TEXT("새 축제는 끝나지 않은 상태로 시작한다"), Harness.Stage->HasFestivalEnded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageTime_ShowWindowIsIndependentOfQueueBreak,
	"FestaFiesta.Time.StageClock.ShowWindowIsIndependentOfQueueBreak",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageTime_ShowWindowIsIndependentOfQueueBreak::RunTest(const FString& Parameters)
{
	FFF_StageTimeTestHelpers::FClockHarness Harness;
	if (!TestTrue(TEXT("Create stage"), Harness.Initialize(EFF_FestivalStage::Live, 0.f))) return false;
	UDataTable* Table = NewObject<UDataTable>(Harness.GameInstance);
	Table->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	FFF_LiveTriggerEvent Row;
	Row.Day = 1;
	Row.TriggerSeconds = 180.f;
	Row.bIsMainProgramLockStart = true;
	Table->AddRow(TEXT("Start"), Row);
	Row.TriggerSeconds = 210.f;
	Row.bIsMainProgramLockStart = false;
	Table->AddRow(TEXT("End"), Row);
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("Schedule property"), Property)) return false;
	Property->SetObjectPropertyValue_InContainer(Harness.Stage, Table);
	Harness.Stage->CurrentDay = 1;

	// 집결 창(180초 전)은 열려 있지만 대기열 중단(3축제분 전)은 아직이다. 둘을 한 값으로 묶으면
	// 전원이 공연 3초 전에 출발해 공연 후반에야 도착한다.
	float Start = 0.f;
	float End = 0.f;
	Harness.Stage->ElapsedLiveSeconds = 10.f;
	TestTrue(TEXT("공연 170초 전에 집결 창이 열려 있다"), Harness.Stage->TryGetMainProgramShowWindow(180.f, Start, End));
	TestEqual(TEXT("공연 시작 시각"), Start, 180.f);
	TestEqual(TEXT("공연 종료 시각"), End, 210.f);
	TestFalse(TEXT("대기열 중단은 아직 아니다"), Harness.Stage->IsMainProgramQueueBreakActive());
	Harness.Stage->ElapsedLiveSeconds = 176.99f;
	TestFalse(TEXT("공연 3초 전 직전까지 대기열은 열려 있다"), Harness.Stage->IsMainProgramQueueBreakActive());
	Harness.Stage->ElapsedLiveSeconds = 177.f;
	TestTrue(TEXT("공연 3초 전부터 대기열이 닫힌다"), Harness.Stage->IsMainProgramQueueBreakActive());
	Harness.Stage->ElapsedLiveSeconds = 210.f;
	TestFalse(TEXT("공연이 끝나면 창이 닫힌다"), Harness.Stage->TryGetMainProgramShowWindow(180.f, Start, End));
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
