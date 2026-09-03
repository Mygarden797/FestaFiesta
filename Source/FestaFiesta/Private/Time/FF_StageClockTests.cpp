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

#endif // WITH_DEV_AUTOMATION_TESTS
