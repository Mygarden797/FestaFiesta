#pragma once

#include "CoreMinimal.h"
#include "Economy/Rules/FF_StaffPool.h"
#include "Economy/FF_FestivalEventTypes.h"

/**
 * 사건(Event) 판정용 순수 함수.
 *
 * 사건 발생·저작·배치 우선순위는 이 파일이 모른다 — "투입된 인원으로 무엇이 나오는가"만
 * 계산한다. 월드 없이 검증된다(기존 FFF_*Rules 방식 그대로).
 *
 * 근거: data/metadata/SiriKim_Docs/staff_event_design.md §4 구현 메모.
 */
struct FESTAFIESTA_API FFF_StaffEventRules
{
	// 투입 인원의 능력치 합. 정원(하한 자리)까지는 레벨 그대로, 넘는 자리는 레벨의 절반만 센다.
	// 레벨이 높은 사람이 하한 자리를 먼저 차지하도록 내부에서 내림차순 정렬한 뒤 계산한다 —
	// 그래서 호출부가 순서를 신경 쓸 필요가 없다(결정론은 이 함수가 보장한다).
	static float CalcAbilitySum(const TArray<FFF_StaffMember>& Assigned, int32 MinStaff);

	// 소요 시간(초) = (작업량 ÷ 능력치 합) x 60. WorkAmount 는 "사람-분" 단위라 초로 환산한다.
	// 능력치 합이 0 이하면 못 끝낸다(최댓값을 돌려준다).
	static float CalcResolveSeconds(const FFF_FestivalEventCard& Card, float AbilitySum);

	// 지금(CurrentDay) 이 사람을 승급시킬 수 있는가.
	// 이미 Lv3, 고용 당일(CurrentDay <= HiredOnDay), 오늘 이미 승급함(LastPromotedDay==CurrentDay)
	// 중 하나라도 해당하면 false.
	static bool CanPromote(const FFF_StaffMember& Member, int32 CurrentDay);

	// 제한 시간을 넘긴 순간의 페널티. 진행률(0~1)이 높을수록 적게 문다.
	// 초과 순간에 한 번만 불러야 한다 — 여러 번 부르면 그때마다 다시 계산되어 이중으로 물린다.
	static int32 CalcOverduePenalty(const FFF_FestivalEventCard& Card, float Progress01);
};
