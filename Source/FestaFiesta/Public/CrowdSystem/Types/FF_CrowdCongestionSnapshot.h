// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_CrowdCongestionSnapshot.generated.h"

/**
 * 혼잡 셀 스냅샷 — Crowd가 밖으로 흘려보내는 "지금 붐비는 자리" 목록.
 *
 * ### 왜 따로 내보내는가
 *
 * 밀도 격자의 Update는 이미 매 스텝 전 셀을 훑으면서 LOS D 초과 셀을 가려낸다.
 * 그 판정 결과를 버리면 받는 쪽이 같은 배열을 한 번 더 전수 조사해야 한다 —
 * 같은 일을 두 번 하는 것이고, 셀 수가 늘면 그 비용이 그대로 두 배가 된다.
 * 그래서 격자가 이미 아는 것을 그대로 실어 보낸다.
 *
 * ### 월드 좌표를 여기서 채우는 이유
 *
 * 구독자가 셀 좌표를 월드로 되돌리려면 격자 원점과 셀 크기를 알아야 하고,
 * 그러면 구독자마다 환산 코드가 하나씩 생긴다. 그중 하나가 원점 갱신을 놓치면
 * 마커만 엉뚱한 자리에 찍히고 수치는 멀쩡해서 한참 뒤에야 발견된다.
 * 환산은 격자를 가진 쪽에서 한 번만 한다.
 *
 * ### 여기 없는 것
 *
 * 이 구조체는 계측 결과뿐이다. 이 혼잡이 **사건인지**는 담지 않는다 —
 * 지속 시간·히스테리시스·중복 억제는 전부 받는 쪽의 판단이다.
 * Crowd는 GameplayTag도 Incident도 모르는 채로 남는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdCongestedCell
{
	GENERATED_BODY()

	// 격자 셀 좌표. Density·CellCounts와 같은 인덱스 공간(Y * NumX + X)에서 풀어낸 값이다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	FIntPoint Cell = FIntPoint::ZeroValue;

	// 셀 중심의 월드 좌표(UU). 마커·앵커가 쓰는 값이며 구독자가 다시 환산하지 않는다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	FVector CenterWorldUU = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	float DensityPerSqm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	int32 Occupancy = 0;

	/**
	 * 직전 집계 구간 동안 이 셀에 새로 들어온 인원.
	 *
	 * Occupancy만으로는 "부스 앞에 모여 선 것"과 "통로가 막힌 것"이 같은 값으로 보인다.
	 * 인원이 많은데 이 값이 0에 가까우면 아무도 빠져나가지 못하는 상태이고,
	 * 둘 다 높으면 그냥 통행량이 많은 목이다. 사건 등급을 가를 때 이 축이 필요하다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	int32 Throughput = 0;
};

/**
 * 스냅샷 한 장. UFF_CrowdSimulationSubsystem::OnCongestionSampled로 나간다.
 *
 * Cells가 비어 있어도 발행된다. 빈 목록이 "방금 다 풀렸다"를 알리는 유일한 신호이므로,
 * 붐비는 셀이 있을 때만 쏘면 구독자는 사건을 닫을 시점을 영영 알 수 없다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdCongestionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	TArray<FFF_CrowdCongestedCell> Cells;

	// 축제 경과 분. 사건 측 시각(FFF_IncidentRules)과 같은 축이다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	float SimTimeMinutes = 0.f;

	// 몇 번째 스냅샷인가. 구독자가 발행을 건너뛴 적이 있는지 확인할 때 쓴다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	int32 SnapshotIndex = 0;

	// 셀 한 변(m). 인접 셀 묶기 같은 후처리를 하려는 구독자를 위해 같이 보낸다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	float CellSizeMeters = 0.f;
};
