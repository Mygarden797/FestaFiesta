// FFZAZoneDefinitionValidator.cpp
// 역할: 모든 Zone 정의 에셋을 훑어 같은 Zone ID가 두 번 쓰였는지 검사한다.
#include "FFZAZoneDefinitionValidator.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FFZAZoneDefinition.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZAZoneDefinitionValidator)

#define LOCTEXT_NAMESPACE "FFZAZoneDefinitionValidator"

bool UFFZAZoneDefinitionValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& InContext) const
{
    (void)InAssetData;
    (void)InContext;
    return InAsset && InAsset->IsA<UFFZAZoneDefinition>();
}

EDataValidationResult UFFZAZoneDefinitionValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& InContext)
{
    const UFFZAZoneDefinition* Definition = Cast<UFFZAZoneDefinition>(InAsset);
    if (!Definition || Definition->ZoneId.IsNone())
    {
        return EDataValidationResult::NotValidated;
    }

    // AssetRegistrySearchable 태그(ZoneId)로 조회하므로 다른 에셋을 메모리에 올리지 않는다.
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.ClassPaths.Add(UFFZAZoneDefinition::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> Assets;
    AssetRegistry.GetAssets(Filter, Assets);

    const FSoftObjectPath SelfPath(Definition);
    for (const FAssetData& Other : Assets)
    {
        if (Other.GetSoftObjectPath() == SelfPath)
        {
            continue;
        }
        FString OtherId;
        if (Other.GetTagValue(GET_MEMBER_NAME_CHECKED(UFFZAZoneDefinition, ZoneId), OtherId) &&
            FName(*OtherId) == Definition->ZoneId)
        {
            AssetFails(InAsset, FText::Format(
                LOCTEXT("DuplicateZoneId", "Zone ID '{0}'를 다른 정의 에셋({1})도 쓰고 있습니다. ID는 하나의 에셋에만 있어야 합니다."),
                FText::FromName(Definition->ZoneId), FText::FromString(Other.GetObjectPathString())));
            return EDataValidationResult::Invalid;
        }
    }
    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
