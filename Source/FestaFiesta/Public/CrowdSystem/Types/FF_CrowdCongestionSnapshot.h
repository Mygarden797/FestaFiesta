// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_CrowdCongestionSnapshot.generated.h"

// 밀도 격자가 이미 가려낸 혼잡 셀을 그대로 실어 보낸다. 받는 쪽이 격자를 다시 훑지 않게 하는 것이 목적이다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdCongestedCell
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	FIntPoint Cell = FIntPoint::ZeroValue;

	// 환산은 격자를 가진 쪽에서 한 번만 한다. 구독자마다 환산 코드가 생기면 그중 하나가 원점 갱신을 놓친다.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	FVector CenterWorldUU = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	float DensityPerSqm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	int32 Occupancy = 0;

	// 인원 많고 이 값이 0에 가까우면 정체, 둘 다 높으면 통행량이 많은 목이다. Occupancy만으로는 둘이 같아 보인다.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	int32 Throughput = 0;
};

// 스냅샷 한 장. 계측 결과만 담고, 이 혼잡이 사건인지(지속·히스테리시스·중복 억제)는 받는 쪽이 판단한다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdCongestionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	TArray<FFF_CrowdCongestedCell> Cells;

	// 축제 경과 분. 사건 측 시각축과 같다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	float SimTimeMinutes = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	int32 SnapshotIndex = 0;

	// 인접 셀 묶기 같은 후처리를 하려는 구독자를 위해 같이 보낸다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Congestion")
	float CellSizeMeters = 0.f;
};
