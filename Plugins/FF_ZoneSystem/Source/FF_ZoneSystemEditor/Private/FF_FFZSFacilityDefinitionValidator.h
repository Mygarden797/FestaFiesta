// FF_FFZSFacilityDefinitionValidator.h
// 역할: 에디터의 데이터 검증 기능에 시설 정의 검사를 연결하여 저장 전 오류를 알려 준다.
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
