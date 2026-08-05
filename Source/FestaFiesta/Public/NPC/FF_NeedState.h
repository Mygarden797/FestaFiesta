// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_NeedState.generated.h"

/**
 * NPC 욕구 상태. 0.0(해소됨) ~ 1.0(한계)로 정규화한다.
 *
 * Need는 "화장실 Need 미해소율이 34%였다"와 같이 트윈의 진단 근거가 된다.
 *
 * 각 Need가 EFFZSFacilityCategory(FF_ZoneSystem, 7종)에 의해 실제로 해소될 수 있도록 재설계 했다.
 * 
 * | Need | 해소 카테고리 | 비고 |
 * |---|---|---|
 * | Hunger | Booth(식음료 서브셋) | Booth가 식음료·잡화·체험을 다 덮어 서브셋 구분은 미해결(카테고리 매핑 문제) |
 * | Thirst | Booth(식음료 서브셋) | 기온 증가율 공공데이터 주입 지점 |
 * | Restroom | Toilet | 1:1 |
 * | Fatigue | Rest | 1:1 |
 * | Stimulation | Performance, PhotoSpot | 공연 관람·사진 촬영 어느 쪽으로 풀려도 됨 |
 *
 * Info·Safety는 의도적으로 뺐다. Safety는 밀집도 위험이라 이미 Utility의 density_cost 항이
 * 맡고 있다(개인 욕구가 아니라 전역 비용), 여기에 또 넣으면 이중 반영이다.
 * Info는 "몰라서 헤맨다"는 욕구를 뒷받침할 근거가 없어서 제외했다 — 필요해지면 그때 추가한다.
 *
 * P0(8/14 시연)는 Hunger·Restroom만 갱신한다. 나머지는 필드만 두고 Phase 4에서 켠다 —
 * 구조체가 나중에 바뀌면 로그 스키마·저장 포맷이 같이 깨지기 때문에 자리를 먼저 잡아둔다.
 * 
 * Stimulation은 Live 시간이 지날수록 쌓여 메인 공연 시작 전 무대 앞으로 몰리는 유입을 만들어낸다. 
 * AT-016(밀도-유량 곡선) 재현이 이 핫스팟 집중에 달려있어서, 켜는 시점을 Phase 2로 당긴다.
 *
 * !!! 스텁 !!! 증가율·해소량 규칙은 아직 없다.
 */
USTRUCT(BlueprintType)
struct FFF_NeedState
{
	GENERATED_BODY()

	// P0 대상. Booth(식음료 서브셋)로 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Hunger = 0.f;

	// P0 대상. Toilet으로 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Restroom = 0.f;

	// Phase 4(P0 확장 후보 아님). Booth(식음료 서브셋)로 해소. 공공데이터(기온) 주입 후보
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Thirst = 0.f;

	// Phase 4(P0 확장 후보 아님). Rest로 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Fatigue = 0.f;

	// Phase 2 대상. Performance 또는 PhotoSpot으로 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Stimulation = 0.f;

	//그룹 Need 집계 — 멤버별 Need의 성분별 최대값.
	// 한 명이라도 화장실이 급하면 일행 전체가 움직임
	// (평균을 쓰면 4인 가족에서 한 명의 급한 욕구가 1/4로 희석된다)
	static FFF_NeedState MakeAggregated(const FFF_NeedState& A, const FFF_NeedState& B)
	{
		FFF_NeedState Result;
		Result.Hunger      = FMath::Max(A.Hunger,      B.Hunger);
		Result.Restroom    = FMath::Max(A.Restroom,    B.Restroom);
		Result.Thirst      = FMath::Max(A.Thirst,      B.Thirst);
		Result.Fatigue     = FMath::Max(A.Fatigue,     B.Fatigue);
		Result.Stimulation = FMath::Max(A.Stimulation, B.Stimulation);
		return Result;
	}
};
