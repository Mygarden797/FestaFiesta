#pragma once

#include "CoreMinimal.h"

/** Stable profile keys and shared copy for first-use help and the reusable guide. */
namespace FFBuildFeatureGuide
{
    struct FESTAFIESTA_API FTopic
    {
        FName Id;
        FText Title;
        FText Body;
        FName ToolbarButton;
        FName ToolbarIcon;
    };
    FESTAFIESTA_API const TArray<FTopic>& Topics();
    FESTAFIESTA_API const FTopic* Find(FName Id);
}
