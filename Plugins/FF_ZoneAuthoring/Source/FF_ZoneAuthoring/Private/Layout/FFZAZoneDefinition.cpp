// FFZAZoneDefinition.cpp
// 역할: Zone 정의 에셋의 표시 이름 계산과 자체 규칙 검증.
#include "FFZAZoneDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZAZoneDefinition)

#define LOCTEXT_NAMESPACE "FFZAZoneDefinition"

const FName UFFZAZoneDefinition::PrimaryAssetTypeName(TEXT("FFZoneDefinition"));

namespace
{
    // FName은 공백·특수문자를 허용하지만, 저장 키·로그·설정 파일에서 안전하도록 영문/숫자/밑줄만 쓴다.
    bool IsSafeIdentifier(const FString& Value)
    {
        if (Value.IsEmpty())
        {
            return false;
        }
        for (const TCHAR Character : Value)
        {
            const bool bAllowed = (Character >= TEXT('A') && Character <= TEXT('Z')) ||
                (Character >= TEXT('a') && Character <= TEXT('z')) ||
                (Character >= TEXT('0') && Character <= TEXT('9')) ||
                Character == TEXT('_');
            if (!bAllowed)
            {
                return false;
            }
        }
        return true;
    }
}

FText UFFZAZoneDefinition::GetResolvedDisplayName() const
{
    if (!DisplayName.IsEmpty())
    {
        return DisplayName;
    }
    FString Raw = ZoneId.ToString();
    Raw.RemoveFromStart(TEXT("GZ_"));
    return FText::FromString(FName::NameToDisplayString(Raw, false));
}

bool UFFZAZoneDefinition::ValidateDefinition(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();
    if (ZoneId.IsNone())
    {
        OutErrors.Add(LOCTEXT("MissingId", "Zone ID가 비어 있습니다."));
    }
    else if (!IsSafeIdentifier(ZoneId.ToString()))
    {
        OutErrors.Add(FText::Format(
            LOCTEXT("UnsafeId", "Zone ID '{0}'에는 영문, 숫자, 밑줄(_)만 사용할 수 있습니다."),
            FText::FromName(ZoneId)));
    }
    if (!bAllowAllFacilityCategories && AllowedFacilityCategories.IsEmpty())
    {
        OutErrors.Add(LOCTEXT("NoCategories", "'모든 시설 허용'을 껐다면 허용 시설 종류를 하나 이상 넣어야 합니다."));
    }
    return OutErrors.IsEmpty();
}

FPrimaryAssetId UFFZAZoneDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(FPrimaryAssetType(PrimaryAssetTypeName), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UFFZAZoneDefinition::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TArray<FText> Errors;
    if (!ValidateDefinition(Errors))
    {
        for (const FText& Error : Errors)
        {
            Context.AddError(Error);
        }
        Result = EDataValidationResult::Invalid;
    }
    else if (Result == EDataValidationResult::NotValidated)
    {
        Result = EDataValidationResult::Valid;
    }
    return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
