// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/FF_NeedState.h"
#include "FF_Party.generated.h"

// 동반유형 — 데이터 축. data/derived/companion_type_modifier_v0.csv 의 companion_type 키와 1:1이다.
// 이 값이 preference_vector 오버레이를 고른다(FFF_PreferenceVectorBuilder).
// 그룹 "크기"만 나타낸다. 구성(가족인가 친구인가)은 EFF_PartyComposition 이 따로 들고 있다.
UENUM(BlueprintType)
enum class EFF_CompanionType : uint8
{
	Solo        UMETA(DisplayName = "1인 동반"),   // companion_type = "solo"
	Pair        UMETA(DisplayName = "2인 동반"),   // companion_type = "pair"
	Group3Plus  UMETA(DisplayName = "3인 이상 동반") // companion_type = "group_3_plus"
};

// 그룹 구성 — 연령 조합 규칙 축. data/metadata/population_group_generation_spec.md 1.5절.
UENUM(BlueprintType)
enum class EFF_PartyComposition : uint8
{
	Solo             UMETA(DisplayName = "1인"),
	CoupleOrFriends  UMETA(DisplayName = "커플·친구"),
	ParentAndChild   UMETA(DisplayName = "보호자·아동"),
	Family           UMETA(DisplayName = "가족"),
	Friends          UMETA(DisplayName = "친구 그룹")
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
 * !!! 스텁 !!! 이 구조체를 실제로 만들어내는 FFF_GroupComposer 는 아직 없다.
 * 규칙은 population_group_generation_spec.md 에 전부 있고 코드만 없는 상태다.
 */
USTRUCT(BlueprintType)
struct FFF_Party
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 PartyId = INDEX_NONE;

	// LeaderAgentId 를 항상 포함한다. 크기는 1~4 (group_3_plus 는 3~4명으로 상한 있음)
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	TArray<int32> MemberAgentIds;

	// 목적지를 결정하는 유일한 멤버. 나머지는 이 결과를 상속한다
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 LeaderAgentId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	EFF_CompanionType CompanionType = EFF_CompanionType::Solo;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	EFF_PartyComposition Composition = EFF_PartyComposition::Solo;

	// 이 반경 밖으로 벌어지면 Crowd 가 재집결시킨다. 값은 미정 — 이정원 님 이동 스택 확정 후
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	float CohesionRadiusMeters = 0.f;

	// 그룹 공유 필드 — "같이 오면 같이 떠난다". 멤버 개인 필드가 아니다.
	// 단위는 현실 분. 게임 시계 변환은 Time 도메인 단일 지점에서만 수행한다.
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	float ArrivalTimeMinutes = 0.f;

	// 멤버 Need 의 성분별 max. Utility 의 Need 항이 이 값을 본다
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	FFF_NeedState AggregatedNeed;

	int32 Num() const { return MemberAgentIds.Num(); }

	bool IsLeader(int32 AgentId) const { return AgentId != INDEX_NONE && AgentId == LeaderAgentId; }
};
