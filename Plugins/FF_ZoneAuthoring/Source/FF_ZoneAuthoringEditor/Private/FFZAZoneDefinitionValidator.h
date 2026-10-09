// FFZAZoneDefinitionValidator.h
// 역할: Zone 정의 에셋을 저장/검사할 때 "다른 정의와 Zone ID가 겹치는지"를 확인한다.
// (에셋 하나만 보는 검사는 UFFZAZoneDefinition::IsDataValid가 담당한다.)
#pragma once

#include "EditorValidatorBase.h"
#include "FFZAZoneDefinitionValidator.generated.h"

UCLASS()
class UFFZAZoneDefinitionValidator final : public UEditorValidatorBase
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
