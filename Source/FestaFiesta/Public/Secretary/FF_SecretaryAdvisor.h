// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "NPC/Party/FF_NeedState.h"

/* 비서가 무슨 말을 할지 고르는 판단부(2026-10-09). 게임을 모르는 순수 함수만 둔다.

   값을 모으는 일(FF_SecretarySubsystem)과 나눴다. 어떤 상황에 무슨 말을 하는지를 월드 없이 시험할 수 있고,
   문장을 고칠 때 이 파일만 보면 된다.

   ─── 안 하는 것 ─────────────────────────────────────────────────────────────
   - 새 수치를 만들지 않는다. 만족도 항목 · 필요 · 줄 · 혼잡 · 사건은 팀원 시스템이 이미 내는 값이다.
   - "불만 1등"은 만족도 항목(대기 · 혼잡 · 취향 · 필요 충족) 중 가장 낮은 것이다. 손님 한 명 한 명의
     불만 사유를 세는 기록은 게임에 없다(2026-10-09 사용자 확인: 근사치로 충분).
   - 아침 브리핑의 불만 1등 · 모자란 필요 · 매출 1등은 직접 고르지 않는다. 어제 결산 한마디
     (FFF_DailyReportCommentRules::FindComplaint)가 고른 것을 그대로 말해 결산 화면과 어긋나지 않게 한다.
   - 운영 중에는 시설을 지을 수 없다(셋업 · 모닝체크에서만). 운영 중 훈수의 설치 권유는 "내일 아침에"로 말한다.
*/

enum class EFF_SecretaryTipKind : uint8
{
	None,
	NoStaffForIncident,
	IdleIncident,
	MissingToilet,
	UnmetNeed,
	LongQueue,
	Crowded,
	// 상황이 아니라 조작 · 규칙 팁. 할 상황이 없고 한동안 조용할 때만 묶음에서 차례로 하나씩.
	General,
	Count
};

// 운영 중 훈수를 고를 때 보는 값. 비서 서브시스템이 짧은 주기로 채운다.
struct FFF_SecretaryLiveSignals
{
	// 스태프가 안 간 채 오래된 사건 수.
	int32 IdleIncidentCount = 0;

	// 지금 출동할 수 있는 스태프 수.
	int32 DispatchableStaffCount = 0;

	// 필요별로 못 채운 손님 비율(%). bNeedSampled 가 거짓이면 손님 표본이 없어 믿지 않는다.
	bool bNeedSampled = false;
	float NeedUnmetPercent[static_cast<int32>(EFF_NeedType::Count)] = {};

	// 설치된 화장실 수.
	int32 ToiletCount = 0;

	// 줄이 가장 긴 시설과 그 인원.
	int32 LongestQueue = 0;
	FText LongestQueueFacility;

	// 혼잡 칸 수.
	int32 CongestedCellCount = 0;
};

struct FFF_SecretaryThresholds
{
	int32 LongQueueCount = 8;
	int32 CrowdedCellCount = 6;
	int32 UnmetNeedPercent = 35;
};

struct FFF_SecretaryTip
{
	EFF_SecretaryTipKind Kind = EFF_SecretaryTipKind::None;
	FText Text;

	bool IsValid() const { return Kind != EFF_SecretaryTipKind::None && !Text.IsEmpty(); }
};

// 조작 · 규칙 팁 하나. SameTopic 은 같은 얘기를 하는 상황 훈수(없으면 None).
struct FFF_SecretaryGeneralTip
{
	FText Text;
	EFF_SecretaryTipKind SameTopic = EFF_SecretaryTipKind::None;
};

// 브리핑에서 "가장 아쉬웠던 것"으로 고른 항목.
enum class EFF_SecretaryWeakness : uint8
{
	None,
	Wait,
	Density,
	Preference,
	Need
};

// 아침 브리핑을 지을 때 보는 값. 만족도 · 아쉬운 점 · 매출 1등은 어제 결산(FFF_DailyReportView)에서 옮긴다.
struct FFF_SecretaryBriefingInputs
{
	// 오늘 날짜(1부터).
	int32 Day = 1;

	// 어제 만족도(0~100). 모르면 INDEX_NONE.
	int32 YesterdaySatisfaction = INDEX_NONE;

	// 어제 결산이 고른 불만 1위. bJudgedYesterday 가 거짓이면 어제 결산을 못 봐 아쉬운 점 얘기를 하지 않는다.
	// 참인데 Weakness 가 None 이면 모든 항목이 기준 이상이었다.
	bool bJudgedYesterday = false;
	EFF_SecretaryWeakness Weakness = EFF_SecretaryWeakness::None;

	// 어제 가장 오래 기다린 시설.
	FText WorstWaitFacility;

	// 어제 결산이 짚은 가장 모자란 필요(화장실 · 배고픔 · 자극 중).
	bool bHasWorstNeed = false;
	EFF_NeedType WorstNeed = EFF_NeedType::Hunger;

	// 어제 매출 1등 시설(배치된 시설 하나 단위)과 금액.
	FText TopRevenueFacility;
	int32 TopRevenueGold = 0;

	// 설치된 화장실 수. 모르면 INDEX_NONE.
	int32 ToiletCount = INDEX_NONE;

	// 아직 한 번도 안 놓아 본 분류. 소개할 순서대로.
	TArray<EFFZSFacilityCategory> UnbuiltCategories;
};

namespace FFSecretaryAdvisor
{
	/* 지금 할 훈수 하나. 할 말이 없으면 Kind 가 None.

	   급한 순서: 출동할 스태프 없음 > 방치된 사건 > 화장실 0개 > 못 채운 필요 > 긴 줄 > 혼잡.
	   IsResting 이 참인 종류(최근에 한 말)는 건너뛰고 다음 순서를 본다.
	*/
	FESTAFIESTA_API FFF_SecretaryTip ChooseLiveTip(
		const FFF_SecretaryLiveSignals& Signals,
		const FFF_SecretaryThresholds& Thresholds,
		TFunctionRef<bool(EFF_SecretaryTipKind)> IsResting);

	/* 상황이 없을 때 하는 조작 · 규칙 팁(2026-10-09 사용자: 말풍선이 더 자주 나와도 좋다). 앞에서부터 차례로 쓴다.
	   튜토리얼에서 이미 확인한 사실만 쓴다. 운영 중에 쓸 수 있는 것만(짓기는 "내일 아침에").
	*/
	FESTAFIESTA_API const TArray<FFF_SecretaryGeneralTip>& GetGeneralTips();

	/* From 부터 차례로 보아 다음에 할 조작 팁의 위치. 없으면 INDEX_NONE.
	   같은 얘기를 상황 훈수로 이미 했으면(WasSpoken) 그 팁은 건너뛴다. PIE 에서 "처리 안 된 사건" 훈수 바로 다음에
	   "사건 표시를 눌러 보세요" 팁이 나와 같은 말을 두 번 했다.
	*/
	FESTAFIESTA_API int32 FindNextGeneralTip(int32 From, TFunctionRef<bool(EFF_SecretaryTipKind)> WasSpoken);

	// 아침 브리핑 문장들. 대사창에 한 줄씩 띄운다.
	FESTAFIESTA_API TArray<FText> BuildMorningBriefing(const FFF_SecretaryBriefingInputs& Inputs);

	// 첫날 셋업 인사. 어제가 없어 짧다.
	FESTAFIESTA_API TArray<FText> BuildFirstDayGreeting();

	// 아직 안 놓아 본 분류를 소개하는 순서. 길 · 타일은 소개할 거리가 아니라 뺀다.
	FESTAFIESTA_API const TArray<EFFZSFacilityCategory>& GetIntroductionOrder();

	// 한글 낱말의 마지막 글자에 받침이 있는가. "이었어요/였어요" 같은 조사를 고른다. 한글이 아니면 거짓.
	FESTAFIESTA_API bool HasFinalConsonant(const FString& Word);
}
