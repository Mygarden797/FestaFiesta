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
