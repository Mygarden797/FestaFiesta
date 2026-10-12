// FF_FFZSBuildRangeComponent.h
// 역할: 시설·대기열·여유 범위를 줌과 무관한 픽셀 두께로 그리는 비권위 표시 컴포넌트다.
#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "FF_FFZSBuildRangeComponent.generated.h"

// Uses the existing world endpoints and per-line depth priority. Thickness is in screen pixels,
// unlike the engine debug line batcher's world-centimeter thickness. No placement geometry is changed.
UCLASS(Transient)
class FF_ZONESYSTEM_API UFFZSBuildRangeComponent final : public ULineBatchComponent
{
    GENERATED_BODY()

public:
    UFFZSBuildRangeComponent();
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};
