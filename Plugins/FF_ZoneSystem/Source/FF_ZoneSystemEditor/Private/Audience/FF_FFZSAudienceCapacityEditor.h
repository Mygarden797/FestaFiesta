// FF_FFZSAudienceCapacityEditor.h
// 역할: 에디터에서 관람 영역을 기준으로 시설 수용량을 갱신하고 관련 편집 이벤트를 관리한다.
#pragma once

#include "CoreMinimal.h"

class UFFZSFacilityDefinition;

/** Editor property-change bridge. No runtime ticking, saving or cross-asset writes. */
struct FFFZSAudienceCapacityEditor
{
    static void Register();
    static void Unregister();
    static FText GetStatus(const UFFZSFacilityDefinition* Definition);
    static bool CanRetry(const UFFZSFacilityDefinition* Definition);
    static bool Retry(UFFZSFacilityDefinition& Definition);
};
