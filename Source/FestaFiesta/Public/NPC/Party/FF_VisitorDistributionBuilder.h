// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_VisitorDistributionBuilder.generated.h"

class UDataTable;

/**
 * 태그가 붙은 방문객 개체 하나. 그룹으로 묶이지 않은 상태다.
 *
 * 역할 분담(crowdai_handoff_note.md §1): Utility AI는 "몇 명이 어떤 태그를 달고 있는가"의
 * 분포만 만들고, **누구를 누구랑 묶을지는 crowdAI가 정한다.** 그래서 이 구조체에는
 * GroupID 도, "몇 번째 그룹 소속"인지도 들어 있지 않다.
 *
 * 대신 태그만 보고 묶을 수 있도록 그룹화 키를 담는다:
 *   묶는 키 = (TargetGroupSize, TargetGroupChildCount, ArchetypeId)
 * 같은 키의 성인 TargetGroupSize-TargetGroupChildCount 명 + 같은 (크기, 아동수)의 아동
 * TargetGroupChildCount 명을 모으면 그룹 하나가 된다. 개체 수가 정확히 나누어떨어지도록
 * 빌더가 보장하며(FFF_VisitorDistribution::GroupPlan) 자동화 테스트가 검증한다.
 */
USTRUCT(BlueprintType)
struct FFF_TaggedVisitor
{
	GENERATED_BODY()

	/**
	 * 생애주기 아키타입 5종 중 하나. **아동 슬롯은 NAME_None 이다.**
	 *
	 * 아동에게 아키타입을 주지 않는 이유: 0~14세의 독립적 여행지 선택·활동 성향을 관측한
	 * 자료가 없다(dependent_child_npc_spec.md 1절). 아동은 리더의 목적지를 상속하므로
	 * 자체 preference_vector 가 필요 없다. 이 값이 NAME_None 인 것을 "빠진 값"으로 보고
	 * DataTable 에 아동용 Row 를 추가하지 말 것 — 그렇게 하면 없는 데이터를 있는 것처럼
	 * 다루게 된다. 아동 여부 판정은 bIsDependentChildSlot 으로 한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Visitor")
	FName ArchetypeId;

	// solo / pair / group_3_plus — preference_vector 오버레이(FFF_CompanionModifierRow)를 고르는 키.
	// 크기 3~6 이 전부 group_3_plus 로 뭉치므로, 그룹을 묶을 때는 TargetGroupSize 를 봐야 한다.
	UPROPERTY(BlueprintReadOnly, Category = "Visitor")
	FName CompanionSizeTag;

	// 이 개체가 속할 예정인 그룹의 인원수 1~6 (본인 포함)
	UPROPERTY(BlueprintReadOnly, Category = "Visitor")
	int32 TargetGroupSize = 1;

	// 그 그룹에 포함될 아동(만 15세 미만) 수. 그룹원 전원이 같은 값을 들고 있다
	UPROPERTY(BlueprintReadOnly, Category = "Visitor")
	int32 TargetGroupChildCount = 0;

	// true 면 종속 아동 슬롯 — 자체 목적지 계산을 하지 않고 리더 목적지를 상속하며, 리더가 될 수 없다
	UPROPERTY(BlueprintReadOnly, Category = "Visitor")
	bool bIsDependentChildSlot = false;
};

/** 같은 (크기, 아동수, 아키타입) 조합의 그룹이 몇 개 계획됐는지. crowdAI가 그룹을 묶을 때의 레시피. */
USTRUCT(BlueprintType)
struct FFF_VisitorGroupPlanBucket
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GroupPlan")
	int32 GroupSize = 1;

	UPROPERTY(BlueprintReadOnly, Category = "GroupPlan")
	int32 ChildCount = 0;

	// 이 조합의 성인 아키타입. 아동은 아키타입이 없어 이 키로 구분하지 않는다
	UPROPERTY(BlueprintReadOnly, Category = "GroupPlan")
	FName AdultArchetypeId;

	UPROPERTY(BlueprintReadOnly, Category = "GroupPlan")
	int32 GroupCount = 0;
};

/** FFF_VisitorDistributionBuilder::Build 의 결과. */
USTRUCT(BlueprintType)
struct FFF_VisitorDistribution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Distribution")
	TArray<FFF_TaggedVisitor> Visitors;

	// 개체 목록과 같은 내용을 그룹 단위로 요약한 것. crowdAI 검산용이며 "누가 누구랑"은 담지 않는다
	UPROPERTY(BlueprintReadOnly, Category = "Distribution")
	TArray<FFF_VisitorGroupPlanBucket> GroupPlan;

	UPROPERTY(BlueprintReadOnly, Category = "Distribution")
	int32 RequestedVisitorCount = 0;

	// 그룹을 통째로만 만들기 때문에 Requested 보다 (그룹 크기 상한 - 1) 만큼 적을 수 있다
	UPROPERTY(BlueprintReadOnly, Category = "Distribution")
	int32 ActualVisitorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Distribution")
	int32 PlannedGroupCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Distribution")
	int32 DependentChildCount = 0;

	// 실제로 나온 아동 비율(%). 목표치를 맞추려고 사후 보정하지 않는다 
	// 그룹 구성이 아동 수를 결정하고, 이 값은 그 결과로 나오는 파생값이다
	UPROPERTY(BlueprintReadOnly, Category = "Distribution")
	float DependentChildSharePct = 0.f;
};

/**
 * 개별 방문객 태그 분포 생성기 — 결정 로직(김시리) 쪽 산출물.
 *
 * 설계·근거: data/metadata/spec/population_group_generation_spec.md,
 *            data/metadata/handoff/crowdai_handoff_note.md §1·§3
 *
 * 입력 DataTable 3종:
 *   FFF_GroupSizeDistributionRow   그룹 크기 분포 (문항 A6 실측)
 *   FFF_GroupChildCompositionRow   그룹 크기별 아동 수 분포 (문항 A6A 실측)
 *   FFF_ArchetypeRow               생애주기 아키타입 5종의 인구비중
 *
 * 아동 수는 **그룹 구성이 결정한다**(2026-08-06 결정, spec 2.4절) — 목표 아동 비율에 맞추는
 * 사후 보정을 하지 않는다. 그래서 결과 아동 비율은 입력 분포에서 나오는 파생값이며,
 * dependent_child_population_target_v0.csv 는 검산용으로만 쓴다.
 *
 * 순수 함수다: 같은 (Seed, 요청 인원, ScopeId, ArchetypeScenarioId, 입력 테이블) 조합은 항상 같은 결과를 낸다.
 * 현재 시각·포인터·스폰 순서를 입력으로 쓰지 않는다.
 */
class FFF_VisitorDistributionBuilder
{
public:
	/**
	 * @param GroupSizeTable      FFF_GroupSizeDistributionRow 테이블
	 * @param ChildCompositionTable FFF_GroupChildCompositionRow 테이블
	 * @param ArchetypeTable      FFF_ArchetypeRow 테이블
	 * @param ScopeId             그룹 분포 scope. 기본 tourism_daytrip (섞어 쓰면 안 된다)
	 * @param ArchetypeScenarioId 아키타입 인구비중 시나리오. resident_neutral_v0 또는 datalab_observed_v0
	 *                            (FFF_ArrivalCurveRow::ScenarioId 와 이름이 겹치는 걸 피하려 8/6에 이 이름으로 정함 —
	 *                            그쪽은 유입 곡선 모양이고 이건 인구 구성 가정으로, 서로 무관하다)
	 * @param RequestedVisitorCount 만들고 싶은 개체 수. 그룹을 통째로만 만들어서 이보다 조금 적을 수 있다
	 * @param GlobalSimulationSeed 재현용 시뮬레이션 Seed
	 */
	static FFF_VisitorDistribution Build(
		const UDataTable* GroupSizeTable,
		const UDataTable* ChildCompositionTable,
		const UDataTable* ArchetypeTable,
		FName ScopeId,
		FName ArchetypeScenarioId,
		int32 RequestedVisitorCount,
		int32 GlobalSimulationSeed);

	// solo / pair / group_3_plus. 크기 3 이상은 전부 group_3_plus 다
	static FName CompanionSizeTagForGroupSize(int32 GroupSize);
};
