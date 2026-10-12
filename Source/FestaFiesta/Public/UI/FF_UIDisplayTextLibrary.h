// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Frontend/FF_OptionsScreen.h"
#include "UI/LiveHUD/FF_LiveHudMenuBar.h"
#include "UI/LiveHUD/FF_LiveHudTypes.h"
#include "UI/LiveHUD/Incident/FF_IncidentToast.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_UIDisplayTextLibrary.generated.h"

/**
 * UI enum 의 한글 표시 문구를 한 곳에서 낸다.
 *
 * === 🔴 왜 `UMETA(DisplayName)` 을 런타임에 읽으면 안 되나 ===
 * `UEnum::GetDisplayNameText*()` 는 **에디터 메타데이터**를 읽는다. 그 메타데이터는
 * 패키징 빌드에서 떨어져 나가고, 그러면 함수가 조용히 **enum 항목 이름(영문)** 을 돌려준다.
 * 에디터에서는 `점검`·`셋업` 으로 잘 보이다가 패키징하면 `Check`·`Setup` 이 뜬다.
 *
 * 2026-08-28 시연용 패키징에서 실제로 발생했다 — 하단 탭이 전부 영문이었고
 * 일차 표시가 `1일차 · Setup` 이 되었다. 2026-08-19 에 김시리가 같은 함정을 짚었는데
 * 그때 고친 것은 경제 enum 뿐이라 UI enum 세 개가 남아 있었다.
 *
 * === 규칙 ===
 * `UMETA(DisplayName)` 은 **에디터 표시용으로만** 둔다. 화면에 나가는 문구는 전부 여기서 낸다.
 * enum 에 항목을 더하면 여기 `switch` 도 채운다 — 안 채우면 자동 테스트
 * (`FestaFiesta.UI.DisplayText.*`) 가 영문 항목 이름이 새어 나온 것을 잡는다.
 *
 * 같은 성격의 경제 enum 은 `UFF_EconomyDisplayTextLibrary` 가 맡는다.
 */
UCLASS()
class FESTAFIESTA_API UFF_UIDisplayTextLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 축제 단계. 하단 바의 `N일차 · <단계>` 에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetFestivalStageText(EFF_FestivalStage Stage);

	/** 준비 단계 하단 바의 탭. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetPrepTabText(EFF_PrepHudTab Tab);

	/** 라이브 하단 바의 탭. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetLiveTabText(EFF_LiveTab Tab);

	/** 구 라이브 메뉴 바의 탭. 새 라이브 HUD 로 완전히 옮기면 이 함수와 enum 을 함께 지운다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetLiveHudTabText(EFF_LiveHudTab Tab);

	/** 옵션 화면의 분류 6종. 타이틀 `옵션` 의 좌측 목록과 우측 제목이 같은 문구를 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetOptionCategoryText(EFF_OptionCategory Category);

	/** 시설의 설치 진행 상태. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetFacilityStateText(EFF_LiveFacilityState State);

	/** 시설 대기열의 상태. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetFacilityQueueStateText(EFF_LiveFacilityQueueState State);

	/** 관객 욕구 종류. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetNeedTypeText(EFF_NeedType NeedType);

	/**
	 * 그 욕구가 미해소일 때 **무엇을 하면 되는지.**
	 *
	 * 숫자만 두면 `배고픔 62%` 를 보고도 할 일을 모른다. 욕구 이름과 대처를 함께 두는
	 * 것이 이 화면의 값어치다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetNeedRemedyText(EFF_NeedType NeedType);

	/** 사건 [보내기]/[확인]의 결과. 거부면 플레이어가 무엇을 하면 되는지까지 적는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetIncidentResolveResultText(EFF_IncidentResolveResult Result);

	/** 알림 앞머리의 짧은 표시(출동·처리·불가). 색만으로 결과를 가르지 않으려고 둔다 (AT-011). */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetIncidentResolveResultLabel(EFF_IncidentResolveResult Result);

	/** 사건에 보낸 스태프의 진행 단계. 상세 패널의 상태 줄과 마커 툴팁에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetIncidentResponseStageText(EFF_IncidentResponseStage Stage);

	/** 결과가 접수(출동 또는 즉시 해결)인지. 알림 색을 고를 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static bool IsIncidentResolveAccepted(EFF_IncidentResolveResult Result);

	/** 사건이 닫힌 이유의 앞머리(완료·소멸·방치·놓침). 닫힘이 아닌 상태는 빈 문구다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetIncidentClosedLabel(EFF_IncidentState State);

	/** 사건이 닫혔을 때의 한 줄. Title 이 비면 제목 없이 이유만 적는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetIncidentClosedText(EFF_IncidentState State, const FText& Title);

	/** 종료 원인을 구분하는 앞머리. 원인이 없는 이전 알림은 상태 기반 문구를 유지한다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetIncidentFinishedLabel(const FFF_IncidentClosedInfo& Info);

	/** 자연 해소·처리 완료·시간 만료의 안내. 출동 중 자연 해소되면 스태프 복귀도 알린다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static FText GetIncidentFinishedText(const FFF_IncidentClosedInfo& Info);

	/** 닫힘 토스트의 띠 색. 해소는 좋음, 방치는 나쁨, 소멸·놓침은 중립. */
	static EFF_IncidentToastKind GetIncidentClosedKind(EFF_IncidentState State);
};
