// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_GroupSizeDistributionRow.generated.h"

/**
 * 방문객 그룹 크기 분포의 한 칸. data/unreal/visitor_group_size_distribution_v0.csv 에서 Import as DataTable.
 *
 * 2025 국민여행조사 원자료의 문항 A6("함께 여행한 일행은 귀하를 포함하여 총 몇 명이었습니까?")을
 * WT_DOM 으로 가중 집계한 값이고, 통계편 공표표 14-1-1/2/3 과 최대오차 0.08pp 이내로 일치한다.
 *
 * 읽을 때 주의 (data/derived/visitor_group_size_distribution_v0.json 참조):
 *  - ScopeId 는 tourism_daytrip(기본) / tourism_all / tourism_overnight 셋 중 하나다.
 *    한 번에 한 ScopeId 의 행만 골라 쓴다 — 섞으면 합계가 300% 가 된다.
 *  - 고른 ScopeId 안에서 SharePct 합계가 100 이다.
 *  - GroupSize 는 1~6. bIsCapBucket 이 true 인 행(6인)은 "6명 이상 전부"를 합친 칸이다.
 *  - 전국 국내 관광여행자 기준이며 부산바다축제 방문객 실측이 아니다.
 */
USTRUCT(BlueprintType)
struct FFF_GroupSizeDistributionRow : public FTableRowBase
{
	GENERATED_BODY()

	// tourism_daytrip(기본) / tourism_all / tourism_overnight
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	FName ScopeId;

	// 그룹 인원수 1~6 (본인 포함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	int32 GroupSize = 0;

	// solo(1명) / pair(2명) / group_3_plus(3명 이상) — FUnitData::CompanionSizeTag 에 그대로 넣는 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	FName CompanionSizeTag;

	// 같은 ScopeId 안에서 합계 100
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	float SharePct = 0.f;

	// true 면 이 행은 "이 인원수 이상 전부"를 합친 상한 칸이다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	bool bIsCapBucket = false;

	// confirmed / assumption / to_verify / derived
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Status;
};
