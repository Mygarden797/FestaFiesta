// FF_FFZSBuildArea.h
// 역할: 여러 지역을 하나의 건설 영역으로 묶어 지역 경계를 넘어 시설을 배치하게 한다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSBuildArea.generated.h"

class AFFZSGameplayZone;
class USceneComponent;
class FFFZSBuildAreaGeometry;
class UFFZSWorldSubsystem;
struct FFFZSOrientedRect;

// World XY centimetres. Presentation consumes the same polygon union as placement authority.
struct FF_ZONESYSTEM_API FFFZSBuildAreaLineSegment
{
    FVector2D Start = FVector2D::ZeroVector;
    FVector2D End = FVector2D::ZeroVector;
};

/**
 * 여러 지역의 경계를 합쳐 건설 가능한 땅을 하나로 관리한다. 지역 사이에서도 시설을 설치할 수 있다.
 * NPC 소속과 혼잡도를 위한 기존 지역 정보는 유지한다. 레벨에 이 액터를 하나 배치하면 활성화되며,
 * 배치하지 않은 레벨에서는 기존 지역별 건설 규칙을 사용한다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FFZS Unified Build Area"))
class FF_ZONESYSTEM_API AFFZSBuildArea : public AActor
{
    GENERATED_BODY()

public:
    AFFZSBuildArea();
    virtual ~AFFZSBuildArea() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build Area")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Build Area", meta = (
        DisplayName = "Source Region IDs", ToolTip = "통합 건설 영역에 포함된 기존 지역 ID입니다. 기존 Gameplay Zone은 NPC와 혼잡 계산을 위해 유지하세요."))
    TArray<FName> SourceRegionIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Build Area", meta = (
        DisplayName = "Boundary Status", ToolTip = "기존 지역 경계를 합친 결과입니다. 지역을 수정한 뒤 Rebuild From World Regions를 누르세요. 플레이 시작 시에는 자동으로 다시 계산됩니다."))
    FText BoundaryStatus;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Build Area", meta = (
        DisplayName = "Buildable Area (m²)", ToolTip = "겹치는 부분은 한 번만 세고, 구멍과 지역 사이의 빈 공간은 제외한 XY 건설 면적입니다."))
    double BoundaryAreaSquareMeters = 0.0;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Build Area", meta = (
        DisplayName = "Rebuild From World Regions", ToolTip = "현재 레벨의 Gameplay Zone 경계를 정확하게 합칩니다. 기존 지역이나 시설을 이동하거나 삭제하지 않습니다. 런타임 경계를 변경한 경우에도 이 함수를 호출하세요."))
    void RebuildFromWorldRegions();

    UFUNCTION(BlueprintPure, Category = "Build Area")
    bool IsBuildAreaReady() const;

    UFUNCTION(BlueprintPure, Category = "Build Area")
    bool ContainsWorldPoint(const FVector& WorldPoint) const;

    // 이 사각형은 지역 내부 좌표가 아니라 월드 전체의 XY 좌표(cm)를 사용한다.
    bool ContainsWorldOrientedRect(const FFFZSOrientedRect& WorldRectCm, double WorldZCm) const;

    // Outer contours and holes, clipped to the requested view. Internal region seams are omitted.
    void GetBoundarySegments(double WorldZCm, const FBox2D& ViewBounds,
        TArray<FFFZSBuildAreaLineSegment>& OutSegments) const;

    // Clips world-space input lines to the reservation rectangle and keeps only unbuildable pieces.
    void GetOutsideSegments(const FFFZSOrientedRect& WorldRectCm, double WorldZCm,
        const TArray<FFFZSBuildAreaLineSegment>& InputSegments,
        TArray<FFFZSBuildAreaLineSegment>& OutSegments) const;

    UFUNCTION(BlueprintPure, Category = "Build Area")
    FBox GetBuildBounds() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    friend class UFFZSWorldSubsystem;
    // 플레이 중 경계 변경은 재진입을 검사하는 월드 서브시스템만 수행한다.
    bool RebuildFromRegions(const TArray<AFFZSGameplayZone*>& Regions);
    TSharedPtr<FFFZSBuildAreaGeometry> Geometry;
};
