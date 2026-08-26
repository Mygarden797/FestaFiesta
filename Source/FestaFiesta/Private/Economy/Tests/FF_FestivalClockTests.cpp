// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/FF_FestivalClock.h"
#include "Engine/GameInstance.h"
#include "Time/FF_FestivalStageSubsystem.h"

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
namespace FFF_FestivalClockTestHelpers
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalClock_OnlyLiveStageHasATime, "FestaFiesta.Economy.FestivalClock.OnlyLiveStageHasATime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalClock_OnlyLiveStageHasATime::RunTest(const FString& Parameters)
{
	using namespace FFF_FestivalClockTestHelpers;

	float Minutes = -1.f;

	// Stage 가 없으면 시각도 없다. 브리지가 Stage 를 못 찾은 상태가 이것이다.
	TestFalse(TEXT("Stage 가 없으면 실패한다"),
		FFF_FestivalClock::TryGetLiveElapsedMinutes(nullptr, Minutes));
	TestEqual(TEXT("실패하면 결과를 건드리지 않는다"), Minutes, -1.f);

	// 🔴 Setup 에서도 초가 쌓인다. 그것을 Live 시각으로 읽으면 프로그램이 아침에 열린다.
	FClockHarness Harness;
	if (!TestTrue(TEXT("Stage 를 만든다"), Harness.Initialize(EFF_FestivalStage::Setup, 90.f)))
	{
		return false;
	}

	TestFalse(TEXT("Setup 경과는 Live 시각이 아니다"),
		FFF_FestivalClock::TryGetLiveElapsedMinutes(Harness.Stage, Minutes));
	TestEqual(TEXT("여전히 결과를 건드리지 않는다"), Minutes, -1.f);

	// Closing 도 마찬가지다. 마감 안전망이 시작 정산으로 오인되면 안 된다.
	Harness.Stage->CurrentStage = EFF_FestivalStage::Closing;
	TestFalse(TEXT("Closing 경과도 Live 시각이 아니다"),
		FFF_FestivalClock::TryGetLiveElapsedMinutes(Harness.Stage, Minutes));

	// 다음 GC 까지 Tick 이 돌 수 있다. 아무 일도 안 하는 단계로 돌려놓는다.
	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;

	return true;
}

// 1 Stage초 = 1 경제분. 밤 조명 트리거 90초와 선셋 요가 카드 90분이 이 계약 위에 서 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalClock_OneSecondIsOneMinute, "FestaFiesta.Economy.FestivalClock.OneSecondIsOneMinute", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalClock_OneSecondIsOneMinute::RunTest(const FString& Parameters)
{
	using namespace FFF_FestivalClockTestHelpers;

	FClockHarness Harness;
	if (!TestTrue(TEXT("Stage 를 만든다"), Harness.Initialize(EFF_FestivalStage::Live, 90.f)))
	{
		return false;
	}

	float Minutes = 0.f;
	if (!TestTrue(TEXT("Live 면 시각을 준다"),
		FFF_FestivalClock::TryGetLiveElapsedMinutes(Harness.Stage, Minutes))) return false;

	TestEqual(TEXT("90초는 90분이다"), Minutes, 90.f);

	// 개장 직후. 예약 손님이 도착하는 시점이고 아직 아무 프로그램도 안 열렸다.
	Harness.Stage->ElapsedLiveSeconds = 0.f;
	if (TestTrue(TEXT("개장 직후에도 시각을 준다"),
		FFF_FestivalClock::TryGetLiveElapsedMinutes(Harness.Stage, Minutes)))
	{
		TestEqual(TEXT("0초는 0분이다"), Minutes, 0.f);
	}

	Harness.Stage->CurrentStage = EFF_FestivalStage::Setup;

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
