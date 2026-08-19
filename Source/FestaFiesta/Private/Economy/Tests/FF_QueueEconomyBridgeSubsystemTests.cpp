// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_QueueEconomyBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Tests/AutomationCommon.h"

/**
 * Queue → 경제 브리지의 배선을 검증한다 (S10).
 *
 * 여기는 **누가 무엇에 연결돼 있는가**만 본다. 수수료 금액은 순수 함수 쪽
 * (FF_BoothRevenueRulesTests.cpp)이 갖고, 원장에 실제로 얼마가 꽂히는지는
 * 스텝 4에서 매출 경로가 채워진 뒤에 붙인다.
 */
namespace FFF_QueueEconomyBridgeTestHelpers
{
	// 구독을 이름으로 확인한다. HandlePartyExited 는 private 이라 메서드 포인터를 못 쓰므로
	// (IsAlreadyBound 는 &Class::Method 를 요구한다) FMulticastScriptDelegate::Contains 를 쓴다.
	// UFUNCTION 이라 동적 델리게이트가 이름으로 들고 있어서 이쪽으로 조회된다.
	const FName HandlerName(TEXT("HandlePartyExited"));
}

// --- 결제 판정 -------------------------------------------------------------

/**
 * 🔴 EFF_QueueExit 는 장원재 님 enum 이다. 값이 늘어나면 여기서 죽는다.
 *
 * 새 퇴장 유형이 추가되면 IsPaidExit 가 조용히 false 를 돌려주고, 그 시설은
 * 매출 0 이 된다. 그 0 은 "아무도 안 썼다" 와 부스별 표에서 구분되지 않는다.
 * 개수를 고정해 두면 그 시점에 빨간불이 떠서 "이건 결제인가" 를 묻게 된다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_PaidExitIsServedOnly, "FestaFiesta.Economy.QueueBridge.PaidExitIsServedOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_PaidExitIsServedOnly::RunTest(const FString& Parameters)
{
	const UEnum* ExitEnum = StaticEnum<EFF_QueueExit>();
	if (!TestNotNull(TEXT("퇴장 유형 열거형을 찾는다"), ExitEnum))
	{
		return false;
	}

	// _MAX 를 뺀 실제 값의 수.
	const int32 ExitCount = ExitEnum->NumEnums() - 1;
	TestEqual(TEXT("퇴장 유형이 3개다 — 늘어나면 결제 판정을 다시 봐야 한다"), ExitCount, 3);

	for (int32 Index = 0; Index < ExitCount; ++Index)
	{
		const EFF_QueueExit ExitType =
			static_cast<EFF_QueueExit>(ExitEnum->GetValueByIndex(Index));
		const bool bIsPaid = UFF_QueueEconomyBridgeSubsystem::IsPaidExit(ExitType);

		if (ExitType == EFF_QueueExit::Served)
		{
			TestTrue(TEXT("서비스 완료는 결제다"), bIsPaid);
		}
		else
		{
			TestFalse(
				*FString::Printf(TEXT("%s 는 결제가 아니다"),
					*ExitEnum->GetNameStringByIndex(Index)),
				bIsPaid);
		}
	}

	return true;
}

// --- 배선 -----------------------------------------------------------------

// 브리지가 월드에 생기고 Queue 의 퇴장 신호를 실제로 구독한다.
// 구독이 빠지면 매출이 0 이 되는데 게임은 정상으로 보이므로, 이 배선 자체를 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_SubscribesToPartyExited, "FestaFiesta.Economy.QueueBridge.SubscribesToPartyExited", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_SubscribesToPartyExited::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;

	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	if (!TestNotNull(TEXT("테스트 월드를 만든다"), World))
	{
		return false;
	}

	UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>();
	UFF_QueueEconomyBridgeSubsystem* Bridge = World->GetSubsystem<UFF_QueueEconomyBridgeSubsystem>();

	if (!TestNotNull(TEXT("Queue 서브시스템이 있다"), Queue)
		|| !TestNotNull(TEXT("브리지가 월드에 생긴다"), Bridge))
	{
		return false;
	}

	TestTrue(TEXT("브리지가 퇴장 신호를 구독한다"),
		Queue->OnPartyExited.Contains(Bridge, HandlerName));

	return true;
}

// 월드가 사라지면 구독도 같이 사라진다.
// 브리지를 World 스코프에 둔 이유가 이것이다 — GameInstance 에 뒀다면 월드가 바뀔 때마다
// 죽은 Queue 를 가리키는 구독이 쌓인다. Deinitialize 를 직접 부르지 않고 월드를 무너뜨려
// 실제 수명주기를 그대로 태운다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_UnsubscribesWithWorld, "FestaFiesta.Economy.QueueBridge.UnsubscribesWithWorld", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_UnsubscribesWithWorld::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;

	UFF_QueueSubsystem* Queue = nullptr;
	UFF_QueueEconomyBridgeSubsystem* Bridge = nullptr;

	{
		FTestWorldWrapper TestWorld;
		if (!TestWorld.CreateTestWorld(EWorldType::Game))
		{
			TestWorld.ForwardErrorMessages(this);
			return false;
		}

		UWorld* World = TestWorld.GetTestWorld();
		if (!TestNotNull(TEXT("테스트 월드를 만든다"), World))
		{
			return false;
		}

		Queue = World->GetSubsystem<UFF_QueueSubsystem>();
		Bridge = World->GetSubsystem<UFF_QueueEconomyBridgeSubsystem>();
		if (!TestNotNull(TEXT("Queue 서브시스템이 있다"), Queue)
			|| !TestNotNull(TEXT("브리지가 월드에 생긴다"), Bridge))
		{
			return false;
		}

		TestTrue(TEXT("월드가 사는 동안은 구독돼 있다"),
			Queue->OnPartyExited.Contains(Bridge, HandlerName));
	}

	// 여기서 TestWorld 소멸자가 월드를 정리하고 두 서브시스템의 Deinitialize 가 불린다.
	// Queue 가 이미 파괴됐을 수 있으므로 델리게이트를 다시 만지지 않는다 —
	// 이 테스트가 지키는 것은 "해제 없이 월드가 무너지면 터진다" 가 아니라
	// 그 경로를 실제로 한 번 태워서 크래시·ensure 가 없다는 사실이다.
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
