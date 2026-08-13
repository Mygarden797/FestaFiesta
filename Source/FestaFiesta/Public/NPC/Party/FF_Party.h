// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_NeedState.h"
#include "FF_Party.generated.h"

// 동반유형 — 데이터 축. data/derived/companion_type_modifier_v0.csv 의 companion_type 키와 1:1이다.
// 이 값이 preference_vector 오버레이를 고른다(FFF_PreferenceVectorBuilder).
// 그룹 "크기"만 나타낸다.
// 그룹 크기 비율의 출처는 (2026-08-06) data/unreal/visitor_group_size_distribution_v0.csv 다

UENUM(BlueprintType)
enum class EFF_CompanionType : uint8
{
	Solo        UMETA(DisplayName = "1인 동반"),   // companion_type = "solo"
	Pair        UMETA(DisplayName = "2인 동반"),   // companion_type = "pair"
	Group3Plus  UMETA(DisplayName = "3인 이상 동반") // companion_type = "group_3_plus"
};


/**
 * 함께 다니는 방문객 묶음 (결정 단위는 Party, 이동 단위는 Agent)
 *
 * 1인 방문객도 멤버 1명짜리 Party로 처리해 코드 경로를 하나로 통일한다.
 *
 * v0는 리더 상속 모델이다 — 리더만 FFF_UtilityDestinationSelector::Choose 를 호출하고
 * 나머지 멤버는 리더의 목적지를 그대로 상속한다.
 * (아동의 "부모 목적지 상속"은 이 규칙의 특수 케이스일 뿐 별도 메커니즘이 아니다. 임시 분리 후 재합류는 v1 이후.)
 *
 * ID 타입: 기존 FFF_UtilityDestinationSelector 가 StableAgentId 를 int32 로 받고 있어
 * 같은 관례를 따른다. 강타입 래퍼는 필요해지면 나중에 도입해도 호출부 의미가 바뀌지 않는다.
 *
 *
 * ### (역할 명시) 
 * - Utility AI(김시리): 개별 방문객 분포(아키타입·동반유형 크기 태그·아동 슬롯 여부)와
 *   그룹 내 연령 구성 규칙(비율·데이터)만 만든다.
 * - crowdAI(이정원): 그 태그를 가진 개체들을 실제로 그룹핑(`FCrowdGroupTypes::GroupID` 부여,
 *   리더 지정)하고 입장 타이밍을 정한다
 *
 * 즉 이 구조체는 "crowdAI가 그룹을 형성한 뒤 그 결과를 읽어오는 뷰"다. 
 * 그룹 형성 결과를 Utility AI가 실제로 어떻게 조회/전달받을지는 아직 미정
 * — `data/metadata/request/crowdai_integration_guide.md` §11 열린 질문 참고.
 */
USTRUCT(BlueprintType)
struct FFF_Party
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 PartyId = INDEX_NONE;

	// LeaderAgentId 를 항상 포함한다. 크기는 1~6 (6명 이상은 6명으로 합쳐서 취급)
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	TArray<int32> MemberAgentIds;

	// 목적지를 결정하는 유일한 멤버. 나머지는 이 결과를 상속한다
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 LeaderAgentId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	EFF_CompanionType CompanionType = EFF_CompanionType::Solo;

	// 이 반경 밖으로 벌어지면 Crowd 가 재집결시킨다. 값은 미정 — 정원 님 이동 스택 확정 후
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	float CohesionRadiusMeters = 0.f;

	// 멤버 Need 의 성분별 max. Utility 의 Need 항이 이 값을 본다
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	FFF_NeedState AggregatedNeed;

	int32 Num() const { return MemberAgentIds.Num(); }

	bool IsLeader(int32 AgentId) const { return AgentId != INDEX_NONE && AgentId == LeaderAgentId; }
};
