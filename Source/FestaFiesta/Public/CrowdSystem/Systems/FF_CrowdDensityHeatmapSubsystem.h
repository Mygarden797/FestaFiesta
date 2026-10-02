// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_CrowdDensityHeatmapSubsystem.generated.h"

struct FCrowdDensityGrid;
class ADecalActor;
class UFF_CrowdSimulationSubsystem;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

// 밀도 격자 한 칸 = 텍스처 한 픽셀, 데칼 하나로 바닥에 투영한다. DrawDebug와 달리 Shipping에서도 그려진다
// 데칼은 상자 안의 모든 표면에 묻으므로 칸별 지면 높이(GroundHeightTex, R32F, cm)를 함께 넘긴다.
// 머티리얼은 픽셀 높이가 지면 + GroundTolerance 보다 높으면 칠하지 않는다 — 아치·가로등·지붕이 자동으로 빠진다.
// 지면을 못 찾은 칸은 NoGroundHeightUU 를 써서 가리지 않는다.
UCLASS()
class FESTAFIESTA_API UFF_CrowdDensityHeatmapSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "Crowd|Heatmap")
	bool SetHeatmapVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Crowd|Heatmap")
	bool IsHeatmapVisible() const { return bWantVisible; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Heatmap")
	bool IsDensityGridReady() const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Heatmap")
	bool HasHeatmapMaterial() const { return BaseMaterial != nullptr; }

	static void BuildPixels(const FCrowdDensityGrid& Grid, float MinDensityToDraw, uint8 Alpha,
		TArray<FColor>& OutPixels);

	static void BuildGroundHeights(const FCrowdDensityGrid& Grid,
		TFunctionRef<bool(const FVector& WorldLocation, double& OutZ)> ResolveGroundZ, TArray<float>& OutHeights);

	static const FName TextureParamName;
	static const FName GridOriginParamName;
	static const FName GridSizeParamName;
	static const FName GroundHeightParamName;
	static const FName GroundToleranceParamName;

	static constexpr float NoGroundHeightUU = 1.0e6f;

	float RefreshIntervalSeconds = 0.25f;
	float MinDensityToDraw = 0.05f;
	uint8 CellAlpha = 160;
	float ProjectionMarginUU = 500.f;
	float GroundToleranceUU = 60.f;

private:
	const UFF_CrowdSimulationSubsystem* FindCrowd() const;
	bool EnsureResources(const UFF_CrowdSimulationSubsystem& Crowd);
	void UploadPixels(const FCrowdDensityGrid& Grid);
	void UploadGroundHeights(const UFF_CrowdSimulationSubsystem& Crowd);
	void ApplyDecalVisibility(bool bVisible);
	void ReleaseResources();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> HeatmapTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GroundHeightTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeatmapMaterial;

	UPROPERTY(Transient)
	TObjectPtr<ADecalActor> HeatmapDecal;

	bool bWantVisible = false;
	bool bGroundHeightsDirty = true;
	float TimeSinceRefresh = 0.f;
	int32 BuiltNumX = 0;
	int32 BuiltNumY = 0;
	float BuiltCellSizeUU = 0.f;
	FVector BuiltOriginUU = FVector::ZeroVector;
	FBox BuiltBoundsCm = FBox(ForceInit);
};
