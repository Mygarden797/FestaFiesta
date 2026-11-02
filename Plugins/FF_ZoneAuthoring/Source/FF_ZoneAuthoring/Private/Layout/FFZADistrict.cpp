// FFZADistrict.cpp
// 역할: 구역의 시설 종류 허용 판정, 검증, 서브시스템 등록.
#include "FFZADistrict.h"

#include "FFZAZoneAuthoringSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZADistrict)

#define LOCTEXT_NAMESPACE "FFZADistrict"

AFFZADistrict::AFFZADistrict()
{
    AreaColor = FLinearColor(0.95f, 0.70f, 0.10f, 1.0f);
}

bool AFFZADistrict::IsCategoryAllowed(const EFFZSFacilityCategory Category) const
{
    return bAllowAllFacilityCategories || AllowedFacilityCategories.Contains(Category);
}

bool AFFZADistrict::ValidateAreaData(TArray<FText>& OutErrors) const
{
    Super::ValidateAreaData(OutErrors);
    if (!bAllowAllFacilityCategories && AllowedFacilityCategories.IsEmpty())
    {
        OutErrors.Add(LOCTEXT("NoCategories", "허용 시설 종류가 비어 있습니다. 이대로면 이 구역에는 아무것도 지을 수 없습니다."));
    }
    return OutErrors.IsEmpty();
}

void AFFZADistrict::RegisterWithSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem)
{
    Subsystem.RegisterDistrict(this);
}

void AFFZADistrict::UnregisterFromSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem)
{
    Subsystem.UnregisterDistrict(this);
}

#undef LOCTEXT_NAMESPACE
