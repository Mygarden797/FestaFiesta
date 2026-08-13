#pragma once

#include "EditorValidatorBase.h"

#include "FF_FFZSFacilityDefinitionValidator.generated.h"

/** Cross-asset authoring gate for FF Zone System facility definitions. */
UCLASS()
class UFFZSFacilityDefinitionValidator final : public UEditorValidatorBase
{
    GENERATED_BODY()

public:
    virtual bool CanValidateAsset_Implementation(
        const FAssetData& InAssetData,
        UObject* InAsset,
        FDataValidationContext& InContext) const override;

    virtual EDataValidationResult ValidateLoadedAsset_Implementation(
        const FAssetData& InAssetData,
        UObject* InAsset,
        FDataValidationContext& InContext) override;
};
