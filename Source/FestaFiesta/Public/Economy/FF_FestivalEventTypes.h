#pragma once

#include "CoreMinimal.h"
#include "FF_FestivalEventTypes.generated.h"

/**
 * 사건(Event) 카드 — Economy(스태프 판정, 김시리)와 사건 시스템(이정원,
 * UFF_FestivalEventSubsystem) 사이의 **데이터 계약**이다.
 *
 * 사건의 발생·저작·심각도 배분·수명주기는 이 파일이 모른다 — 그건 사건 시스템 몫이다.
 * 이 구조체가 채워져서 넘어오면 FFF_StaffEventRules 가 능력치 합·소요시간·페널티를 계산한다.
 *
 * 근거: data/metadata/SiriKim_Docs/staff_event_design.md §4·§7.
 * 사건 쪽에서는 FFF_IncidentOption 이 이 카드를 들고 오고, FF_StaffDispatchSubsystem 이 판정에 넘긴다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FestivalEventCard
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FGuid EventId;

	// 종류(화재·미아·쓰레기 등). 값 목록·저작은 사건 시스템 쪽 몫이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FName EventType;

	// 발생 구역. ZoneSystem 의 존 식별자와 맞춘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FName ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "1"))
	int32 Severity = 1;

	// 착수 하한. 투입 인원이 이 미만이면 아예 시작도 못 하고 대기한다(제한 시간은 흐른다).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "1"))
	int32 MinStaff = 1;

	// 정원. 기본값은 MinStaff+2 지만 종류별로 다르게 저작할 수 있다.
	// 정원을 넘는 투입분은 FFF_StaffEventRules::CalcAbilitySum 에서 절반만 인정된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "1"))
	int32 MaxStaff = 3;

	// 작업량. **단위는 "사람-분"이다**(초가 아니다) — 저작할 때 감을 잡기 쉽게 하려는 선택이다.
	// 투입 인원의 능력치 합으로 나누고 60을 곱하면 소요 시간(초)이 된다(FFF_StaffEventRules::CalcResolveSeconds).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0.0"))
	float WorkAmount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0.0"))
	float TimeLimitSeconds = 0.f;

	// 시간 초과 시점의 페널티 최대치. 실제 페널티 = 이 값 x (1 − 진행률), 초과 순간에 한 번만 계산한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event", meta = (ClampMin = "0"))
	int32 MaxOverduePenalty = 0;
};
