// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_VisitHistory.generated.h"

struct FFF_DestinationCandidate;

/**
 * "최근에 어디를 갔었나" — 개체(또는 그룹) 하나의 방문 이력.
 *
 * ### 왜 필요한가
 *
 * [1] `preference_vector` 는 **카테고리 단위**(food_stall, rest_area…)라 시설 인스턴스를 구분하지
 * 못한다. 그래서 "밥 먹었으니 음식 부스 매력이 내려간다"는 나오지만 "이 부스는 가봤으니 옆 부스로"는 나오지 않는다.
 * 시설별 거리는 구분되지만 거리만으로는 "방금 갔던 곳"을 알 수 없다. 방문 이력이 없으면 가까운 같은
 * 카테고리 시설을 반복 선택할 수 있어, 인스턴스 단위 재방문 감쇠가 별도로 필요하다.
 *
 * [2] 경제와 직결된다 — 같은 부스만 가면 매출이 한 곳에 몰리고, 여러 곳을 돌면 분산된다.
 * 플레이어가 부스를 여럿 짓는 동기가 여기서 나온다. 리얼리즘이 아니라 게임 메커니즘이다.
 *
 * ### 왜 영구 페널티가 아니라 감쇠형인가
 *
 * 영구 페널티면 "제일 좋아하는 부스에 두 번은 안 간다"가 되어 오히려 부자연스럽다. 실제로는
 * 마음에 든 곳을 다시 가기도 한다. 시간이 지나면 회복하는 형태로 두었다.
 *
 * ### 고정 용량인 이유
 *
 * 방문객 8만 명 규모에서 개체당 무한 이력을 들면 메모리가 감당이 안 된다. 최근 K개만 남기고
 * 그보다 오래된 방문은 "안 가본 곳"과 같게 취급한다 — 감쇠 때문에 어차피 페널티가 0 에 가깝다.
 * `revisit_memory_minutes` 안에 K개보다 많은 곳을 갈 수 있으면 K를 늘려야 한다.
 */
USTRUCT(BlueprintType)
struct FFF_VisitHistory
{
	GENERATED_BODY()

	// 개체당 보관할 최근 방문 수. 재평가 주기(수 분) × 이 값이 revisit_memory_minutes 를
	// 덮을 만큼이어야 한다 — 그보다 작으면 기억이 용량 때문에 먼저 지워진다.
	// !!! 실측 근거 없음 !!! 재평가 주기가 확정되면 다시 잡는다.
	static constexpr int32 Capacity = 5;

	// 방문한 목적지. 오래된 것부터가 아니라 **덮어쓰는 링**이라 순서에 의미가 없다.
	UPROPERTY(BlueprintReadOnly, Category = "Visit")
	TArray<FName> DestinationIds;

	// 위 배열과 같은 인덱스의 방문 시각(Live 경과분).
	UPROPERTY(BlueprintReadOnly, Category = "Visit")
	TArray<float> VisitedAtLiveMinutes;

	// 다음에 덮어쓸 위치. Capacity 에 도달하면 가장 오래된 칸을 덮는다.
	UPROPERTY(BlueprintReadOnly, Category = "Visit")
	int32 NextWriteIndex = 0;

	/**
	 * 방문을 기록한다. 호출 시점은 "시설 이용 완료"다 — `FFF_NeedRules::Relieve` 와 같은 시점.
	 * 도착만 하고 포기한 곳을 "가봤다"로 세면 줄만 서다 떠난 시설을 다시 안 가게 된다.
	 */
	void RecordVisit(FName DestinationId, float NowLiveMinutes);

	/**
	 * 마지막 방문으로부터 지난 분. 기록에 없으면 음수를 돌려준다(= 안 가본 곳).
	 * "안 가본 곳"과 "방금 간 곳(0분)"이 구분되어야 하므로 0 을 쓰지 않는다.
	 */
	float GetMinutesSinceVisit(FName DestinationId, float NowLiveMinutes) const;

	/**
	 * 후보 배열에 `MinutesSinceLastVisit` 를 채운다.
	 * `FFF_QueueWaitAdapter::ApplyToCandidates` 와 같은 형태의 "후보 채우기" 함수다 —
	 * 선택기가 이력 구조를 몰라도 되게 하려는 것이다.
	 *
	 * @return 실제로 방문 기록이 있던 후보 개수
	 */
	int32 ApplyToCandidates(TArray<FFF_DestinationCandidate>& InOutCandidates, float NowLiveMinutes) const;

	void Reset()
	{
		DestinationIds.Reset();
		VisitedAtLiveMinutes.Reset();
		NextWriteIndex = 0;
	}
};
