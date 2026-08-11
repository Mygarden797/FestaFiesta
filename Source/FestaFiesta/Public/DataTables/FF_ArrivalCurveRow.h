// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_ArrivalCurveRow.generated.h"

/**
 * 시간대별 목표 유입 곡선의 한 시간 버킷. data/unreal/visitor_arrival_curve_v0.csv 에서 Import as DataTable.
 *
 * 이 표가 정하는 것은 "언제 얼마나 들어와야 하는가"의 목표 분포뿐이다. 실제로 어떤 그룹을
 * 언제 Pool에서 꺼내 입장시킬지는 Crowd 쪽이 자유롭게 정한다.
 *
 * 읽을 때 반드시 지킬 것 (data/metadata/spec/visitor_arrival_curve_spec.md 참조):
 *  - ArrivalSharePct 는 신규 유입 비율(%)이고 동시 체류 인원이 아니다. 시나리오별 합계 = 100.
 *  - 절대 인원이 아니다. 목표 인원 = ArrivalSharePct / 100 * (소비자가 정한 총 유입 인원).
 *  - ProgressStart/End 는 Live 단계의 정규화 진행도 [0,1) 다. 절대 시각이 아니다
 *    (MinutesStartAtDefault·ClockStartAtDefault 는 기본 240분·17:00 기준 표시용).
 *  - BucketCount 를 코드에 박지 말고 이 행에서 읽는다 — 버킷 해상도가 바뀔 수 있다.
 *  - 그룹은 통째로 입장하므로 버킷별 오차는 정상이다. 맞춰야 하는 건 CumulativeSharePct 쪽이다.
 */
USTRUCT(BlueprintType)
struct FFF_ArrivalCurveRow : public FTableRowBase
{
	GENERATED_BODY()

	// flat_v0 / single_peak_v0 / sunset_peak_v0 — 곡선 모양은 전부 draft_assumption 이다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve")
	FName ScenarioId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve")
	int32 BucketIndex = 0;

	// 이 시나리오의 전체 버킷 수. 24를 상수로 박지 말고 이 값을 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve")
	int32 BucketCount = 0;

	// Live 단계 정규화 진행도 [0,1). 현재 버킷 = floor(진행도 * BucketCount)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve")
	float ProgressStart = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve")
	float ProgressEnd = 0.f;

	// 이 버킷에 새로 입장해야 할 인원의 비율(%). 동시 체류가 아니라 신규 유입이다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve")
	float ArrivalSharePct = 0.f;

	// 이 버킷 끝까지의 누적 유입 비율(%). 그룹 통째 입장 때문에 실제로 맞춰야 하는 건 이쪽이다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curve")
	float CumulativeSharePct = 0.f;

	// 기본값(live_stage_duration_minutes=240) 기준 표시용. 계산에는 ProgressStart 를 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	float MinutesStartAtDefault = 0.f;

	// 기본 시간표(17:00 시작) 기준 표시용 시각 문자열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FString ClockStartAtDefault;

	// confirmed / assumption / to_verify / derived / draft_assumption
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Status;
};
