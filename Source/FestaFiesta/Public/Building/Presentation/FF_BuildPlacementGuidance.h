// FF_BuildPlacementGuidance.h
// 역할: 배치 실패 사유를 안내 문구로 바꾸고 가까운 대체 위치 후보를 만든다.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FF_BuildPlacementGuidance.generated.h"

/** A presentation of the existing authority result, never a separate placement decision. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_BuildPlacementGuidance
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FText Title;
    UPROPERTY(BlueprintReadOnly) FText Advice;
    UPROPERTY(BlueprintReadOnly) FText Obstruction;
    UPROPERTY(BlueprintReadOnly) int32 ProblemIndex = 0;
    UPROPERTY(BlueprintReadOnly) int32 ProblemCount = 0;
    UPROPERTY(BlueprintReadOnly) bool bCanFindNearby = false;
};

namespace FFBuildPlacementAssistance
{
    FESTAFIESTA_API FText ReasonTitle(EFFZSPlacementFailure Reason);
    FESTAFIESTA_API bool CanResolveByMoving(const TArray<EFFZSPlacementFailure>& Reasons);
    /** Distance ordered, unique offsets on the user's lattice; bounded to a 6 m radius. */
    FESTAFIESTA_API TArray<FIntPoint> MakeNearbyOffsets(int32 SnapCm);
}
