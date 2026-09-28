// FF_FFZSVisualGeometry.h
// 역할: 메시의 중심과 크기를 시설 표시 규격에 맞추는 변환을 계산한다. 실제 점유 규칙은 변경하지 않는다.
#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
struct FFFZSFacilitySpec;

/**
 * FacilitySpec의 권위 크기에서 표시 전용 메시 transform을 결정하는 순수 계산 모음이다.
 * 배치 Actor와 Preview가 반드시 같은 함수를 사용해야 논리 footprint와 화면 표시가 갈라지지 않는다.
 */
namespace FFZSVisualGeometry
{
    /** 시설 크기에 맞춰 표시하려면 원본 메시의 가로·세로·높이가 모두 유한한 양수여야 한다. */
    FF_ZONESYSTEM_API bool HasUsableSourceBounds(const FBox& SourceBounds);

    /** 검증 도구용 계산이다. 실패하면 임의의 원본 크기로 대신 표시하지 않고 false를 반환한다. */
    FF_ZONESYSTEM_API bool TryResolveVisualTransformFromBounds(
        const FFFZSFacilitySpec& Spec,
        const FBox& SourceBounds,
        FTransform& OutTransform);

    /** 로드된 메시 bounds를 이용해 시설 로컬 상대 transform을 계산한다. */
    FF_ZONESYSTEM_API FTransform ResolveVisualMeshTransform(
        const FFFZSFacilitySpec& Spec,
        const UStaticMesh* Mesh);

    /** 에디터 도구와 자동화 테스트에서 UObject 로드 없이 사용할 수 있는 순수 계산 경로다. */
    FF_ZONESYSTEM_API FTransform ResolveVisualTransformFromBounds(
        const FFFZSFacilitySpec& Spec,
        const FBox& SourceBounds);

    /** 주어진 transform을 적용한 axis-aligned 시설 로컬 bounds를 반환한다. */
    FF_ZONESYSTEM_API FBox TransformBounds(
        const FBox& SourceBounds,
        const FTransform& Transform);

    /** MatchPlacementBounds가 정확한 결과를 보장할 수 있는 90도 단위 Z 회전인지 검사한다. */
    FF_ZONESYSTEM_API bool IsCardinalYawRotation(const FRotator& Rotation);
}
