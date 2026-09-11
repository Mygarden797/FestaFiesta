// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

namespace CrowdLaneBake
{
	// 레인 가장자리와 보호 구역 가장자리 사이에 항상 남겨 두는 여유. 벽 반발이 구동력의 22%로
	// 떨어지는 거리다(WallRepulsionA=2000N, B=0.08m, 70kg 기준).
	constexpr float WallClearanceCm = 30.f;

	struct FLaneSegment
	{
		FVector CenterCm = FVector::ZeroVector;
		float   YawDegrees = 0.f;
		float   HalfLengthCm = 0.f;
		float   HalfWidthCm = 0.f;
	};

	struct FBakeParams
	{
		float LaneHalfWidthCm       = 150.f;
		float SimplifyToleranceCm   = 150.f;
		float MinSegmentLengthRatio = 2.f;
		float CornerOverlapCm       = 150.f;
	};

	FESTAFIESTA_API void SimplifyPolyline(const TArray<FVector>& In, float ToleranceCm,
	                                      TArray<FVector>& Out);

	// 정점 간격이 SpacingCm를 넘지 않도록 선형 보간으로 채운다. 원본 정점은 모두 남는다.
	FESTAFIESTA_API void ResamplePolyline(const TArray<FVector>& In, float SpacingCm,
	                                      TArray<FVector>& Out);

	// 구간마다 OBB 하나. 짧은 구간은 이웃과 병합하되, 병합으로 생기는 중심선 이탈이
	// LaneHalfWidthCm의 절반을 넘으면 병합하지 않고 짧은 채로 낸다.
	FESTAFIESTA_API void EmitSegments(const TArray<FVector>& Simplified, const FBakeParams& Params,
	                                  TArray<FLaneSegment>& Out);

	FESTAFIESTA_API bool ValidateWidthInvariant(float LaneHalfWidthCm, float ProtectedWidthCm,
	                                            float AgentRadiusCm, FString& OutError);
}
