// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Economy/FF_FestivalEconomyTypes.h"

/**
 * 경제 enum 의 한글 표시 문자열 검증.
 *
 * 🔴 왜 UMETA(DisplayName) 을 런타임에 안 쓰는가
 *
 * UEnum::GetDisplayNameTextByIndex 의 UMETA 조회는 통째로 `#if WITH_EDITOR` 안에 있다
 * (Engine/Source/Runtime/CoreUObject/Private/UObject/Enum.cpp). 평범한 UENUM 에는
 * EnumDisplayNameFn 도 안 붙으므로, 패키징 빌드에서는 마지막 줄로 떨어진다:
 *
 *     return FText::FromString(GetNameStringByIndex(NameIndex));
 *
 * 즉 "장소 임대료" 가 아니라 **"VenueRent"** 가 나온다 — ReasonId 와 글자 하나 안 틀리게 같다.
 * 그래서 문자열의 원본을 UFF_EconomyDisplayTextLibrary(NSLOCTEXT) 에 두고 UMETA 는 에디터 장식으로 남긴다.
 *
 * **이 파일의 두 번째 테스트가 그 둘이 어긋나지 않게 잡는다.** 한쪽만 고치면 깨진다.
 * 드리프트 검사는 UMETA 를 읽을 수 있는 에디터에서만 의미가 있고, 자동화는 에디터에서 돈다.
 *
 * 서브시스템도 GameInstance 도 만들지 않는다 — 전부 static 순수 함수다.
 */
namespace FFF_EconomyDisplayTextTestHelpers
{
	// enum 마지막 값. 새 값을 뒤에 추가하면 여기도 같이 늘려야 한다.
	constexpr int64 LastSpendReason  = static_cast<int64>(EFF_SpendReason::FinaleFireworks);
	constexpr int64 LastEarnReason   = static_cast<int64>(EFF_EarnReason::MinigameReward);
	constexpr int64 LastFinaleResult = static_cast<int64>(EFF_FinalePurchaseResult::InsufficientGold);
	constexpr int64 LastFinaleGrade  = static_cast<int64>(EFF_FinaleGrade::S);
	constexpr int64 LastProgramResult = static_cast<int64>(EFF_ProgramPurchaseResult::InsufficientGold);
}

// 모든 값에 문자열이 있고, 그 문자열이 enum 이름이 아니다.
// enum 이름이 그대로 나오면 패키징 폴백과 구분이 안 되므로 그것 자체가 실패다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_EconomyDisplayText_EveryValueHasTextThatIsNotTheEnumName, "FestaFiesta.Economy.DisplayText.EveryValueHasTextThatIsNotTheEnumName", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_EconomyDisplayText_EveryValueHasTextThatIsNotTheEnumName::RunTest(const FString& Parameters)
{
	// 지출 9종
	{
		const UEnum* EnumType = StaticEnum<EFF_SpendReason>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastSpendReason; ++Value)
		{
			const FString Text =
				UFF_EconomyDisplayTextLibrary::GetSpendReasonText(static_cast<EFF_SpendReason>(Value)).ToString();
			const FString RawName = EnumType->GetAuthoredNameStringByValue(Value);

			TestFalse(FString::Printf(TEXT("지출 %s 에 문자열이 있다"), *RawName), Text.IsEmpty());
			TestNotEqual(FString::Printf(TEXT("지출 %s 가 enum 이름 그대로가 아니다"), *RawName),
				Text, RawName);
		}
	}

	// 매출 5종
	{
		const UEnum* EnumType = StaticEnum<EFF_EarnReason>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastEarnReason; ++Value)
		{
			const FString Text =
				UFF_EconomyDisplayTextLibrary::GetEarnReasonText(static_cast<EFF_EarnReason>(Value)).ToString();
			const FString RawName = EnumType->GetAuthoredNameStringByValue(Value);

			TestFalse(FString::Printf(TEXT("매출 %s 에 문자열이 있다"), *RawName), Text.IsEmpty());
			TestNotEqual(FString::Printf(TEXT("매출 %s 가 enum 이름 그대로가 아니다"), *RawName),
				Text, RawName);
		}
	}

	// 피날레 구매 결과 6종 — UI 가 실패 이유를 그대로 화면에 쓴다.
	{
		const UEnum* EnumType = StaticEnum<EFF_FinalePurchaseResult>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastFinaleResult; ++Value)
		{
			const FString Text = UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText(
				static_cast<EFF_FinalePurchaseResult>(Value)).ToString();
			const FString RawName = EnumType->GetAuthoredNameStringByValue(Value);

			TestFalse(FString::Printf(TEXT("피날레 결과 %s 에 문자열이 있다"), *RawName), Text.IsEmpty());
			TestNotEqual(FString::Printf(TEXT("피날레 결과 %s 가 enum 이름 그대로가 아니다"), *RawName),
				Text, RawName);
		}
	}

	// 프로그램 편성 결과 6종 — 아침 화면이 실패 이유를 그대로 쓴다.
	{
		const UEnum* EnumType = StaticEnum<EFF_ProgramPurchaseResult>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastProgramResult; ++Value)
		{
			const FString Text = UFF_EconomyDisplayTextLibrary::GetProgramPurchaseResultText(
				static_cast<EFF_ProgramPurchaseResult>(Value)).ToString();
			const FString RawName = EnumType->GetAuthoredNameStringByValue(Value);

			TestFalse(FString::Printf(TEXT("편성 결과 %s 에 문자열이 있다"), *RawName), Text.IsEmpty());
			TestNotEqual(FString::Printf(TEXT("편성 결과 %s 가 enum 이름 그대로가 아니다"), *RawName),
				Text, RawName);
		}
	}

	// 실제 문구를 하나씩 못 박아 둔다 — 화면이 이 문자열을 그대로 쓴다.
	TestEqual(TEXT("Gold 부족 문구"),
		UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText(
			EFF_FinalePurchaseResult::InsufficientGold).ToString(),
		FString(TEXT("Gold 부족")));
	TestEqual(TEXT("이미 구매했다 문구"),
		UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText(
			EFF_FinalePurchaseResult::AlreadyPurchased).ToString(),
		FString(TEXT("이미 구매했다")));
	TestEqual(TEXT("장소 임대료 문구"),
		UFF_EconomyDisplayTextLibrary::GetSpendReasonText(EFF_SpendReason::VenueRent).ToString(),
		FString(TEXT("장소 임대료")));

	return true;
}

// 🔴 드리프트 검사: NSLOCTEXT 원본과 UMETA(DisplayName) 이 같아야 한다.
// 한쪽만 고치면 에디터와 패키징 빌드가 다른 글자를 보여주고, 그건 시연 직전에 발견된다.
// 에디터에서만 UMETA 를 읽을 수 있으므로 이 검사는 에디터 전용이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_EconomyDisplayText_RuntimeTextMatchesEditorMetadata, "FestaFiesta.Economy.DisplayText.RuntimeTextMatchesEditorMetadata", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_EconomyDisplayText_RuntimeTextMatchesEditorMetadata::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	{
		const UEnum* EnumType = StaticEnum<EFF_SpendReason>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastSpendReason; ++Value)
		{
			TestEqual(
				FString::Printf(TEXT("지출 %s 의 UMETA 와 NSLOCTEXT 가 같다"),
					*EnumType->GetAuthoredNameStringByValue(Value)),
				UFF_EconomyDisplayTextLibrary::GetSpendReasonText(
					static_cast<EFF_SpendReason>(Value)).ToString(),
				EnumType->GetDisplayNameTextByValue(Value).ToString());
		}
	}

	{
		const UEnum* EnumType = StaticEnum<EFF_EarnReason>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastEarnReason; ++Value)
		{
			TestEqual(
				FString::Printf(TEXT("매출 %s 의 UMETA 와 NSLOCTEXT 가 같다"),
					*EnumType->GetAuthoredNameStringByValue(Value)),
				UFF_EconomyDisplayTextLibrary::GetEarnReasonText(
					static_cast<EFF_EarnReason>(Value)).ToString(),
				EnumType->GetDisplayNameTextByValue(Value).ToString());
		}
	}

	{
		const UEnum* EnumType = StaticEnum<EFF_FinalePurchaseResult>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastFinaleResult; ++Value)
		{
			TestEqual(
				FString::Printf(TEXT("피날레 결과 %s 의 UMETA 와 NSLOCTEXT 가 같다"),
					*EnumType->GetAuthoredNameStringByValue(Value)),
				UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText(
					static_cast<EFF_FinalePurchaseResult>(Value)).ToString(),
				EnumType->GetDisplayNameTextByValue(Value).ToString());
		}
	}

	{
		const UEnum* EnumType = StaticEnum<EFF_FinaleGrade>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastFinaleGrade; ++Value)
		{
			TestEqual(
				FString::Printf(TEXT("피날레 등급 %s 의 UMETA 와 NSLOCTEXT 가 같다"),
					*EnumType->GetAuthoredNameStringByValue(Value)),
				UFF_EconomyDisplayTextLibrary::GetFinaleGradeText(
					static_cast<EFF_FinaleGrade>(Value)).ToString(),
				EnumType->GetDisplayNameTextByValue(Value).ToString());
		}
	}

	{
		const UEnum* EnumType = StaticEnum<EFF_ProgramPurchaseResult>();
		for (int64 Value = 0; Value <= FFF_EconomyDisplayTextTestHelpers::LastProgramResult; ++Value)
		{
			TestEqual(
				FString::Printf(TEXT("편성 결과 %s 의 UMETA 와 NSLOCTEXT 가 같다"),
					*EnumType->GetAuthoredNameStringByValue(Value)),
				UFF_EconomyDisplayTextLibrary::GetProgramPurchaseResultText(
					static_cast<EFF_ProgramPurchaseResult>(Value)).ToString(),
				EnumType->GetDisplayNameTextByValue(Value).ToString());
		}
	}
#else
	AddInfo(TEXT("에디터가 아니라 UMETA 를 읽을 수 없다. 드리프트 검사를 건너뛴다."));
#endif

	return true;
}

// enum 에 값이 추가됐는데 문자열을 안 넣는 사고를 잡는다.
// switch 에 default 를 두지 않았으므로 1차 방어는 컴파일러(경고=에러)다.
// 이 테스트는 누군가 default 를 넣어 그 방어를 무력화했을 때를 위한 2차 방어다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_EconomyDisplayText_EnumSizesMatchExpectations, "FestaFiesta.Economy.DisplayText.EnumSizesMatchExpectations", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_EconomyDisplayText_EnumSizesMatchExpectations::RunTest(const FString& Parameters)
{
	// UEnum 은 _MAX 를 한 칸 더 갖는다. 저작된 값 개수는 NumEnums() - 1 이다.
	TestEqual(TEXT("지출 원인이 9종이다"),
		StaticEnum<EFF_SpendReason>()->NumEnums() - 1, 9);
	TestEqual(TEXT("매출 원인이 5종이다"),
		StaticEnum<EFF_EarnReason>()->NumEnums() - 1, 5);
	TestEqual(TEXT("피날레 구매 결과가 6종이다"),
		StaticEnum<EFF_FinalePurchaseResult>()->NumEnums() - 1, 6);
	TestEqual(TEXT("피날레 등급이 5종이다"),
		StaticEnum<EFF_FinaleGrade>()->NumEnums() - 1, 5);
	TestEqual(TEXT("프로그램 편성 결과가 7종이다"),
		StaticEnum<EFF_ProgramPurchaseResult>()->NumEnums() - 1, 7);

	// 위 개수가 바뀌면 이 파일의 Last* 상수와 문자열 switch 도 같이 고쳐야 한다.
	TestEqual(TEXT("지출 마지막 값이 8 이다"),
		FFF_EconomyDisplayTextTestHelpers::LastSpendReason, static_cast<int64>(8));
	TestEqual(TEXT("매출 마지막 값이 4 이다"),
		FFF_EconomyDisplayTextTestHelpers::LastEarnReason, static_cast<int64>(4));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
