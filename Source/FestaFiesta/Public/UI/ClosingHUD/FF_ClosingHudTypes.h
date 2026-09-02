// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Time/FF_FestivalStage.h"
#include "FF_ClosingHudTypes.generated.h"

/**
 * 결산 HUD 의 탭.
 *
 * === 2026-09-02 결정 — 별도 화면 대신 탭 두 개 ===
 * 원래 계획은 `마감`·`리포트`·`피날레` 를 **각각 전체 화면**으로 만드는 것이었다.
 * 실제로 필요한 것은 "하루가 끝났을 때 결과를 본다" 하나뿐이라, 준비·라이브와 같은
 * **바 하나 + 패널** 로 줄인다. 화면을 세 장 만들면 셋 다 반쯤 비게 된다.
 */
UENUM(BlueprintType)
enum class EFF_ClosingTab : uint8
{
	/** 오늘·전체 성적. */
	Report	UMETA(DisplayName = "리포트"),

	/** 피날레 등급과 보너스. 3일차에 실제로 터진다. */
	Finale	UMETA(DisplayName = "피날레")
};

/** 지금 열려 있는 탭. `bHasSelection` 이 false 면 `Tab` 은 쓰레기값이다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ClosingTabSelection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Closing HUD")
	EFF_ClosingTab Tab = EFF_ClosingTab::Report;

	UPROPERTY(BlueprintReadOnly, Category = "Closing HUD")
	bool bHasSelection = false;

	static FFF_ClosingTabSelection None() { return FFF_ClosingTabSelection(); }

	static FFF_ClosingTabSelection Of(EFF_ClosingTab InTab)
	{
		FFF_ClosingTabSelection Result;
		Result.Tab = InTab;
		Result.bHasSelection = true;
		return Result;
	}

	bool IsSelected(EFF_ClosingTab InTab) const { return bHasSelection && Tab == InTab; }

	bool operator==(const FFF_ClosingTabSelection& Other) const
	{
		// 선택이 없으면 `Tab` 은 비교하지 않는다 — 쓰레기값끼리 비교하면 같은 상태가 달라 보인다.
		return bHasSelection == Other.bHasSelection && (!bHasSelection || Tab == Other.Tab);
	}
};

/**
 * 하단 바 CTA 의 상태.
 *
 * !! 비활성만 하지 않는다 !! (AT-011) 흐려짐만으로는 왜 못 누르는지 알 수 없으므로
 * 막힌 이유를 함께 띄운다.
 */
UENUM(BlueprintType)
enum class EFF_ClosingCtaState : uint8
{
	/** CTA 를 아예 안 띄운다. */
	Hidden			UMETA(DisplayName = "숨김"),

	/** 누를 수 있다. */
	Ready			UMETA(DisplayName = "가능"),

	/**
	 * 아직 리포트 단계가 아니다.
	 *
	 * 마감에서 결과를 보는 것과 다음 날로 넘기는 것은 다른 일이다. 넘기기는 리포트
	 * 단계가 소유한다 (`AdvanceToNextDay` 가 그 단계 밖에서는 무시된다).
	 */
	BlockedNotReport	UMETA(DisplayName = "리포트 단계 아님"),

	/** 마지막 날이라 넘길 날이 없다. */
	BlockedFinalDay		UMETA(DisplayName = "마지막 날"),

	/**
	 * 피날레 연출 구간이라 `다음으로` 다.
	 *
	 * 리포트의 `다음 날로` 와 **다른 행동**이다 — 여기서는 단계를 건너뛰는 것이고
	 * (`SkipCurrentStage`), 리포트는 날짜를 넘기는 것이다 (`AdvanceToNextDay`).
	 */
	ReadyFinaleSkip		UMETA(DisplayName = "피날레 넘기기")
};

/**
 * 결산 HUD 의 판단 규칙. **순수 함수만 둔다.**
 */
struct FESTAFIESTA_API FFF_ClosingHudRules
{
	/** 이 단계에서 결산 HUD 를 띄우는가. */
	static bool IsClosingHudStage(EFF_FestivalStage Stage);

	/** 피날레 연출 구간인가. 탭을 숨기고 CTA 만 띄운다. */
	static bool IsFinaleShowStage(EFF_FestivalStage Stage) { return Stage == EFF_FestivalStage::Finale; }

	/**
	 * 이 탭을 지금 볼 수 있는가.
	 *
	 * 🔴 **피날레 탭은 사지 않았으면 숨긴다.** 안 산 사람에게 빈 패널을 보여 주면
	 * "고장났나" 가 되고, 살 수 있는 곳(셋업)은 이미 지나간 뒤다.
	 */
	static bool IsTabVisible(EFF_ClosingTab Tab, EFF_FestivalStage Stage, bool bHasPurchasedFinale);

	/** 단계가 바뀌어 안 보이게 된 탭이 열려 있으면 선택을 푼다. */
	static FFF_ClosingTabSelection ClampSelectionToVisible(
		const FFF_ClosingTabSelection& Current, EFF_FestivalStage Stage, bool bHasPurchasedFinale);

	/** 같은 탭을 다시 누르면 닫힌다. */
	static FFF_ClosingTabSelection ToggleTab(const FFF_ClosingTabSelection& Current, EFF_ClosingTab Clicked);

	/**
	 * CTA 상태를 정한다.
	 *
	 * 🔴 **순서가 뜻을 만든다** — ① HUD 단계가 아님 → ② 리포트 단계 아님 → ③ 마지막 날.
	 * ③ 을 앞에 두면 마감 단계에서 `마지막 날이라 넘길 수 없습니다` 가 떠서, 리포트로
	 * 넘어가면 끝낼 수 있다는 사실을 못 알린다.
	 */
	static EFF_ClosingCtaState ResolveCtaState(
		EFF_FestivalStage Stage, bool bIsReportStage, bool bIsFinalDay, bool bCanSkipStage);
};
