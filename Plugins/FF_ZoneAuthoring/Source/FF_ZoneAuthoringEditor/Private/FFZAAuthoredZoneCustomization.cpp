// FFZAAuthoredZoneCustomization.cpp
// 역할: 정의 에셋이 있는 Zone의 식별/규칙 필드를 잠그고 분류 순서를 정리한다.
#include "FFZAAuthoredZoneCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "FFZAAuthoredZone.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "FFZAAuthoredZoneCustomization"

TSharedRef<IDetailCustomization> FFFZAAuthoredZoneCustomization::MakeInstance()
{
    return MakeShared<FFFZAAuthoredZoneCustomization>();
}

void FFFZAAuthoredZoneCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);

    // 선택한 Zone 중 하나라도 정의 에셋이 없으면 편집을 허용한다(수동 모드).
    const TAttribute<bool> CanEditIdentity = TAttribute<bool>::CreateLambda([Objects]()
    {
        for (const TWeakObjectPtr<UObject>& Object : Objects)
        {
            const AFFZAAuthoredZone* Zone = Cast<AFFZAAuthoredZone>(Object.Get());
            if (Zone && !Zone->ZoneDefinition)
            {
                return true;
            }
        }
        return false;
    });

    IDetailCategoryBuilder& DefinitionCategory = DetailBuilder.EditCategory(
        TEXT("Zone Definition"), LOCTEXT("DefinitionCategory", "Zone Definition"), ECategoryPriority::Important);
    DetailBuilder.EditCategory(TEXT("Zone Authoring"), LOCTEXT("AuthoringCategory", "Zone Authoring"), ECategoryPriority::Important);

    const FName LockedProperties[] = {
        GET_MEMBER_NAME_CHECKED(AFFZSGameplayZone, ZoneId),
        GET_MEMBER_NAME_CHECKED(AFFZSGameplayZone, DisplayName),
        GET_MEMBER_NAME_CHECKED(AFFZSGameplayZone, bAllowAllFacilityCategories),
        GET_MEMBER_NAME_CHECKED(AFFZSGameplayZone, AllowedFacilityCategories)
    };
    for (const FName PropertyName : LockedProperties)
    {
        const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(PropertyName, AFFZSGameplayZone::StaticClass());
        if (Handle->IsValidHandle())
        {
            // 정의 분류 아래로 옮겨 "정의에서 온 값"임을 한눈에 보이게 하고, 정의가 있으면 잠근다.
            DefinitionCategory.AddProperty(Handle).IsEnabled(CanEditIdentity);
        }
    }
}

#undef LOCTEXT_NAMESPACE
