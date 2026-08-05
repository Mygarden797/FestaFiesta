/**
 * @file FF_FFZSFacilityDefinition.cpp
 * @brief 시설 Data Asset 검증과 안정적인 Primary Asset ID 생성을 구현한다.
 * 흐름: Spec 검증 오류를 에디터 데이터 검증에 전달하고 유효한 정의 ID를 Primary Asset 이름으로 사용한다.
 * 불변식: Primary Asset Type 문자열은 FFZSFacilityDefinition이며 런타임 사양 의미와 일치해야 한다.
 * 실패/성능: 모든 검증 오류를 모아 제작자에게 보여 주며 에셋이나 메시를 강제 로드하지 않는다.
 */

#include "FF_FFZSFacilityDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// 단일 검증 진실원인 FFFZSFacilitySpec::IsValid에 위임한다.
// 호출자가 이미 모은 오류를 보존하는 Spec 계약을 그대로 따른다.
bool UFFZSFacilityDefinition::IsDefinitionValid(TArray<FText>& OutErrors) const
{
    return Spec.IsValid(OutErrors);
}

// 유효한 안정 DefinitionId가 있으면 프로젝트 전체에서 같은 Primary Asset Type을 사용한다.
// ID가 비어 있으면 부모 규칙을 사용해 에디터가 자산을 표시할 수 있게 하되 Spec 검증은 별도로 실패한다.
FPrimaryAssetId UFFZSFacilityDefinition::GetPrimaryAssetId() const
{
    if (!Spec.FacilityDefinitionId.IsNone())
    {
        return FPrimaryAssetId(TEXT("FFZSFacilityDefinition"), Spec.FacilityDefinitionId);
    }
    return Super::GetPrimaryAssetId();
}

#if WITH_EDITOR
// 에디터 저장·검증 메뉴가 부모 오류와 Spec 오류를 모두 보게 한다.
// 런타임 빌드에는 이 경로가 없으므로 설치 명령도 같은 Spec 검증을 다시 수행한다.
EDataValidationResult UFFZSFacilityDefinition::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult ParentResult = Super::IsDataValid(Context);
    TArray<FText> Errors;
    if (!IsDefinitionValid(Errors))
    {
        for (const FText& Error : Errors)
        {
            // 모든 오류를 전달해 담당자가 한 번의 검증으로 수정 항목 전체를 확인하게 한다.
            Context.AddError(Error);
        }
        return EDataValidationResult::Invalid;
    }

    return ParentResult == EDataValidationResult::Invalid
        ? EDataValidationResult::Invalid
        : EDataValidationResult::Valid;
}
#endif
