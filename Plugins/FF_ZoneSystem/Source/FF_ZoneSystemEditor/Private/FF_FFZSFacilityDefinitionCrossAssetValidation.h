#pragma once

#include "AssetRegistry/AssetData.h"
#include "Containers/ArrayView.h"
#include "Math/Box.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"

struct FFFZSFacilityAssetIdentity
{
    FSoftObjectPath AssetPath;
    FName FacilityDefinitionId = NAME_None;
    FPrimaryAssetId PrimaryAssetId;
};

struct FFFZSFacilityIdentityCollisions
{
    TArray<FSoftObjectPath> DuplicateDefinitionIdAssets;
    TArray<FSoftObjectPath> DuplicatePrimaryAssetIdAssets;
};

namespace FFZSFacilityDefinitionCrossAssetValidation
{
    inline constexpr TCHAR RequiredDefinitionRoot[] =
        TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions");

    bool IsWithinRequiredDefinitionRoot(FName PackagePath);
    bool HasFiniteBounds(const FBox& Bounds);

    FFFZSFacilityIdentityCollisions FindIdentityCollisions(
        const FFFZSFacilityAssetIdentity& CurrentIdentity,
        TConstArrayView<FFFZSFacilityAssetIdentity> OtherIdentities);
}
