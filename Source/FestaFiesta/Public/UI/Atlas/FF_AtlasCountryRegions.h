#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FF_AtlasCountryRegions.generated.h"

class UMaterialInterface;
class UMaterialParameterCollection;
class UTexture2D;

/** 경도·위도 순서의 닫힌 경계. 마지막 점의 첫 점 중복은 선택 사항이다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasRegionRing
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	TArray<FVector2D> Points;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasRegionPolygon
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	FFF_AtlasRegionRing Outer;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	TArray<FFF_AtlasRegionRing> Holes;

	bool Contains(const FVector2D& LongitudeLatitude) const;
};

/** 국가 하나의 육지 경계. 여러 섬과 내부 구멍을 같은 식별자로 묶는다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasCountryRegion
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	FName CountryId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	TArray<FFF_AtlasRegionPolygon> Polygons;
	/** R=minLon, G=minLat, B=maxLon, A=maxLat. 날짜변경선은 경도를 연속 구간으로 풀어 ±180 밖도 허용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	FLinearColor Bounds = FLinearColor(0, 0, 1, 1);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	TObjectPtr<UTexture2D> Mask;

	bool Contains(const FVector2D& LongitudeLatitude) const;
};

/**
 * 선택 마스크와 클릭 판정을 함께 만드는 원본. 런타임에는 파일/렌더 버퍼를 읽지 않는다.
 * Surface는 표시 중인 Top 메시의 실제 삼각형과 UV이며, 지리 경계 주변 여유 영역도 포함한다.
 * 그래야 반올림된 해안에서 방사 위경도 대신 렌더링과 같은 보간 UV로 선택할 수 있다.
 */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_AtlasCountryRegions : public UDataAsset
{
	GENERATED_BODY()
public:
	/** 다중 영역 재질의 슬롯 계약. 초과 국가는 조형물 방식을 유지한다. */
	static constexpr int32 MaxRenderingRegions = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas")
	TArray<FFF_AtlasCountryRegion> Regions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Rendering")
	TObjectPtr<UMaterialInterface> SelectionMaterial;
	/** 이 머티리얼을 사용하는 슬롯만 교체한다. 비었을 때만 _Top 메시 이름을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Rendering")
	TObjectPtr<UMaterialInterface> TargetMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Rendering")
	TObjectPtr<UMaterialParameterCollection> ShapeParameters;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Surface")
	TArray<FVector> SurfaceVertices;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Surface")
	TArray<FVector2D> SurfaceUVs;
	/** 원본 높이 텍스처를 셰이더와 같은 필터·곡선으로 읽은 산 높이 가중치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Surface")
	TArray<float> SurfaceWeights;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Surface")
	TArray<int32> SurfaceIndices;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Surface", meta=(ClampMin="1"))
	float SourceRadius = 5000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Surface")
	float BaseLandThickness = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Atlas|Surface")
	float BaseMountainHeight = 650.f;

	const FFF_AtlasCountryRegion* FindRegion(FName CountryId) const;
	FName FindCountryAtUV(const FVector2D& UV) const;
	bool HasValidSurface() const;
};

/** 전체 지구 표면을 매번 순회하지 않기 위한 삼각형 BVH. 데이터 에셋에는 저장하지 않는다. */
struct FESTAFIESTA_API FFF_AtlasRegionSurfaceIndex
{
	void Reset();
	void Build(const TArray<FVector>& Vertices, const TArray<int32>& Indices);
	/** 연결 구조는 유지하고 WPO로 이동한 삼각형의 바운드만 다시 계산한다. */
	void Refit(const TArray<FVector>& Vertices, const TArray<int32>& Indices);
	bool IsValid() const { return !Nodes.IsEmpty(); }
	bool Trace(const TArray<FVector>& Vertices, const TArray<FVector2D>& UVs,
		const TArray<int32>& Indices, const FVector& Origin, const FVector& Direction,
		double MaxDistance, FVector2D& OutUV, double& OutDistance, int32* OutTrianglesTested = nullptr) const;

private:
	struct FNode
	{
		FBox Bounds = FBox(ForceInit);
		int32 Left = INDEX_NONE;
		int32 Right = INDEX_NONE;
		int32 First = 0;
		int32 Count = 0;
	};
	TArray<FNode> Nodes;
	TArray<int32> TriangleOffsets;
	int32 BuildNode(const TArray<FBox>& TriangleBounds, int32 First, int32 Count, int32 Depth);
};

/** 월드·렌더러가 없는 자동화에서도 검증하는 표면 광선 계산. 거리 매개변수 단위는 입력 광선을 따른다. */
struct FESTAFIESTA_API FFF_AtlasRegionPicking
{
	static bool TraceSurface(const TArray<FVector>& Vertices, const TArray<FVector2D>& UVs,
		const TArray<int32>& Indices, const FBox& Bounds, const FVector& Origin,
		const FVector& Direction, double MaxDistance, FVector2D& OutUV, double& OutDistance);
	static double WaterOcclusionDistance(const FVector& Origin, const FVector& Direction, double Radius);
	/** 월드 단위 방사 WPO를 루트 좌표로 환산한다. 회전·비균일 스케일에서도 셰이더와 같다. */
	static FVector DisplaceVertex(const FVector& ScaledVertex, const FVector& RootWorldScale, double WorldDisplacement,
		const FVector& LocalActorCenter = FVector::ZeroVector);
};
