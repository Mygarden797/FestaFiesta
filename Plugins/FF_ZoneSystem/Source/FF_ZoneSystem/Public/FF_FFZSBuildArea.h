#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSBuildArea.generated.h"

class AFFZSGameplayZone;
class USceneComponent;
class FFFZSBuildAreaGeometry;
class UFFZSWorldSubsystem;

/**
 * One construction authority for the existing gameplay regions. Their shared seams disappear from
 * construction checks; region IDs, local coordinates, NPC membership and density remain unchanged.
 * Place exactly one actor in the level to opt in. Levels without it keep legacy placement rules.
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

    // Unlike region-local rectangles, these rectangle coordinates are world XY centimetres.
    bool ContainsWorldOrientedRect(const FFFZSOrientedRect& WorldRectCm, double WorldZCm) const;

    UFUNCTION(BlueprintPure, Category = "Build Area")
    FBox GetBuildBounds() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    friend class UFFZSWorldSubsystem;
    // Only the guarded subsystem can mutate runtime construction authority.
    bool RebuildFromRegions(const TArray<AFFZSGameplayZone*>& Regions);
    TSharedPtr<FFFZSBuildAreaGeometry> Geometry;
};
