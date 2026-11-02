// FF_FFZSFacilityDefinitionCrossAssetValidation.h
// 역할: 서로 다른 시설 정의 사이의 ID·업그레이드 연결을 검사하여 중복과 끊긴 참조를 찾는다.
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
        TEXT("/Game/FestaFiesta/Building/FF_Data_Building");

    bool IsWithinRequiredDefinitionRoot(FName PackagePath);
    bool HasFiniteBounds(const FBox& Bounds);

    FFFZSFacilityIdentityCollisions FindIdentityCollisions(
        const FFFZSFacilityAssetIdentity& CurrentIdentity,
        TConstArrayView<FFFZSFacilityAssetIdentity> OtherIdentities);
}
