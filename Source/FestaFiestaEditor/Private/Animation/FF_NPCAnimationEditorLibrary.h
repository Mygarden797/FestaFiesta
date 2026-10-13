#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_NPCAnimationEditorLibrary.generated.h"

class UBlendSpace;

UCLASS()
class UFF_NPCAnimationEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|NPC Animation")
	static bool RebuildBlendSpaceRuntimeData(UBlendSpace* BlendSpace);
};
