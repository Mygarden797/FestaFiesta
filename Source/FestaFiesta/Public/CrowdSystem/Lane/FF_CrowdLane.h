// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_CrowdLane.generated.h"

class USplineComponent;
class AFFZSProtectedArea;

// 저작 액터. 진실의 원천 하나(스플라인)에서 산출물 둘이 같은 베이크로 나온다.
//   폴리라인            → 군중이 걷는 경로
//   AFFZSProtectedArea  → 시설 배치 금지 (GeneralRoute)
// 따로 저작하면 반드시 어긋나므로 "보호 구역이 실제 보행 경로를 덮는다"를 코드로 보장한다.
UCLASS()
class FESTAFIESTA_API AFF_CrowdLane : public AActor
{
	GENERATED_BODY()

public:
	AFF_CrowdLane();

	UPROPERTY(VisibleAnywhere, Category = "Crowd Lane")
	TObjectPtr<USplineComponent> LanePath;

	UPROPERTY(EditAnywhere, Category = "Crowd Lane")
	FName LaneId = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Crowd Lane", meta = (ClampMin = "10.0", Units = "cm"))
	float LaneHalfWidthCm = 150.f;

	UPROPERTY(EditAnywhere, Category = "Crowd Lane")
	bool bOneWay = false;

	// 폴리라인 정점 간격. WaypointAcceptanceRadiusUU의 1~2배가 기준이다.
	UPROPERTY(EditAnywhere, Category = "Crowd Lane|Bake", meta = (ClampMin = "50.0", Units = "cm"))
	float SampleSpacingCm = 200.f;

	UPROPERTY(EditAnywhere, Category = "Crowd Lane|Bake", meta = (ClampMin = "0.0", Units = "cm"))
	float SimplifyToleranceCm = 150.f;

	UPROPERTY(EditAnywhere, Category = "Crowd Lane|Bake", meta = (ClampMin = "0.0", Units = "cm"))
	float CornerOverlapCm = 150.f;

	UPROPERTY(EditAnywhere, Category = "Crowd Lane|Bake", meta = (ClampMin = "2.0"))
	float MinSegmentLengthRatio = 2.f;

	UPROPERTY(EditAnywhere, Category = "Crowd Lane|Bake", meta = (ClampMin = "0.0", Units = "cm"))
	float AgentRadiusCm = 34.f;

	// 보호 박스 Z extent의 두 배. 시설 최대 높이 이상이어야 Z 겹침 판정을 통과한다.
	UPROPERTY(EditAnywhere, Category = "Crowd Lane|Bake", meta = (ClampMin = "100.0", Units = "cm"))
	float ProtectedHeightCm = 1200.f;

	void BuildPolylineCm(TArray<FVector>& OutPointsCm) const;

	// 2 x LaneHalfWidth + 2 x (AgentRadius + WallClearance). 기본값 상속을 쓰지 않는 이유는
	// GeneralRouteWidthCm(300)이 이 부등식을 만족하지 않기 때문이다.
	float GetRequiredClearWidthCm() const;

	FString GetSegmentIdPrefix() const;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Crowd Lane|Bake")
	void Bake();

	UFUNCTION(CallInEditor, Category = "Crowd Lane|Bake")
	void ClearBakedAreas();

	bool ValidateLaneData(TArray<FText>& OutErrors) const;

	virtual void PostEditMove(bool bFinished) override;

	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY()
	TArray<TObjectPtr<AFFZSProtectedArea>> BakedAreas;
};
