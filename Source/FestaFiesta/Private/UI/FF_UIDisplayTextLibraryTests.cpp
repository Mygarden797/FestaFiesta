// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/FF_UIDisplayTextLibrary.h"

/**
 * UI enum 표시 문구 고정 테스트.
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * `UEnum::GetDisplayNameText*()` 는 에디터 메타데이터를 읽는다. **에디터에서는 한글이 잘 보이고
 * 패키징 빌드에서만 영문 항목 이름이 뜬다.** 그래서 손으로는 시연 직전 패키징에서야 발견된다 —
 * 2026-08-28 에 실제로 그렇게 발견됐다. 셋을 못으로 박는다 —
 *   ① 모든 enum 항목에 문구가 있는 것 (항목을 더하고 `switch` 를 안 채우면 빈 문자열이 된다)
 *   ② 그 문구가 **enum 항목 이름과 다른 것** (즉 영문 이름이 그대로 새어 나오지 않은 것)
 *
 * === 🔴 이 테스트가 못 잡는 것 ===
 * 누군가 다시 `GetDisplayNameText*()` 로 되돌려도 **이 테스트는 통과한다.** 테스트는
 * 에디터에서 도는데 거기서는 메타데이터가 살아 있어 한글이 나오기 때문이다. 즉 원래 버그는
 * 자동화로 못 잡는다 — 잡히는 것은 "항목을 더하고 `switch` 를 안 채운 경우" 뿐이다.
 *
 * 되돌림을 막는 것은 테스트가 아니라 규칙이다: 화면에 나가는 문구는 전부
 * `UFF_UIDisplayTextLibrary` 를 거치고, `GetDisplayNameText*()` 는 UI 코드에서 쓰지 않는다.
 */

namespace FFUIDisplayTextTestLocal
{
	/**
	 * enum 전 항목을 돌며 문구가 비어 있지 않고 **항목 이름(영문)과 다른지** 본다.
	 *
	 * `SNS` 처럼 한글 문구가 영문과 같아야 정상인 항목이 있어 예외를 받는다.
	 */
	template <typename TEnum, typename TGetter>
	void VerifyAll(
		FAutomationTestBase& Test,
		const TCHAR* EnumLabel,
		TGetter&& Getter,
		const TSet<FString>& AllowSameAsName)
	{
		const UEnum* EnumType = StaticEnum<TEnum>();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s enum 을 찾았다"), EnumLabel), EnumType))
		{
			return;
		}

		// 마지막 항목은 UHT 가 붙이는 `_MAX` 라 건너뛴다.
		const int32 Count = EnumType->NumEnums() - 1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FString Name = EnumType->GetNameStringByIndex(Index);
			const FText Text = Getter(static_cast<TEnum>(EnumType->GetValueByIndex(Index)));

			Test.TestFalse(
				*FString::Printf(TEXT("%s::%s 에 문구가 있다"), EnumLabel, *Name),
				Text.IsEmpty());

			if (AllowSameAsName.Contains(Name))
			{
				continue;
			}

			// 🔴 여기가 핵심이다. 문구가 항목 이름과 같으면 `UMETA` 리플렉션으로 되돌아간 것이고,
			//    그러면 패키징 빌드에서 영문이 뜬다.
			Test.TestNotEqual(
				*FString::Printf(TEXT("%s::%s 문구가 영문 항목 이름이 아니다"), EnumLabel, *Name),
				Text.ToString(), Name);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UIDisplayText_AllEnums, "FestaFiesta.UI.DisplayText.AllEnums", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UIDisplayText_AllEnums::RunTest(const FString& Parameters)
{
	using namespace FFUIDisplayTextTestLocal;

	VerifyAll<EFF_FestivalStage>(*this, TEXT("EFF_FestivalStage"),
		&UFF_UIDisplayTextLibrary::GetFestivalStageText, {});

	VerifyAll<EFF_PrepHudTab>(*this, TEXT("EFF_PrepHudTab"),
		&UFF_UIDisplayTextLibrary::GetPrepTabText, {});

	// `SNS` 는 한글 표기가 없어 항목 이름과 같은 것이 정상이다.
	VerifyAll<EFF_LiveTab>(*this, TEXT("EFF_LiveTab"),
		&UFF_UIDisplayTextLibrary::GetLiveTabText, { TEXT("SNS") });

	VerifyAll<EFF_LiveHudTab>(*this, TEXT("EFF_LiveHudTab"),
		&UFF_UIDisplayTextLibrary::GetLiveHudTabText, { TEXT("SNS") });

	VerifyAll<EFF_OptionCategory>(*this, TEXT("EFF_OptionCategory"),
		&UFF_UIDisplayTextLibrary::GetOptionCategoryText, {});

	VerifyAll<EFF_IncidentResolveResult>(*this, TEXT("EFF_IncidentResolveResult"),
		&UFF_UIDisplayTextLibrary::GetIncidentResolveResultText, {});

	VerifyAll<EFF_IncidentResolveResult>(*this, TEXT("EFF_IncidentResolveResult(Label)"),
		&UFF_UIDisplayTextLibrary::GetIncidentResolveResultLabel, {});

	VerifyAll<EFF_IncidentResponseStage>(*this, TEXT("EFF_IncidentResponseStage"),
		&UFF_UIDisplayTextLibrary::GetIncidentResponseStageText, {});

	// 몇 개는 값까지 못으로 박는다. 위 검사는 "영문이 아니다" 까지만 보므로
	// 문구가 통째로 바뀌는 것은 못 잡는다.
	TestEqual(TEXT("셋업 문구"),
		UFF_UIDisplayTextLibrary::GetFestivalStageText(EFF_FestivalStage::Setup).ToString(),
		FString(TEXT("셋업")));

	TestEqual(TEXT("점검 탭 문구"),
		UFF_UIDisplayTextLibrary::GetPrepTabText(EFF_PrepHudTab::Check).ToString(),
		FString(TEXT("점검")));

	TestEqual(TEXT("운영 탭 문구"),
		UFF_UIDisplayTextLibrary::GetLiveTabText(EFF_LiveTab::Operations).ToString(),
		FString(TEXT("운영")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
