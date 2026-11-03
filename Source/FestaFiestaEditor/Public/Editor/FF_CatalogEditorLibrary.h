#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_CatalogEditorLibrary.generated.h"

class UWidgetBlueprint;

/** Editor helpers for scripted construction catalog migrations. */
UCLASS()
class FESTAFIESTAEDITOR_API UFF_CatalogEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Register missing source-widget GUIDs without compiling or saving the asset. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Catalog Editor")
	static int32 RegisterWidgetGuids(UWidgetBlueprint* Blueprint);
};
