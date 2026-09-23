// FF_FFZSFacilityDefinitionCrossAssetValidation.cpp
// 역할: 서로 다른 시설 정의 사이의 ID·업그레이드 연결을 검사하여 중복과 끊긴 참조를 찾는다.
#include "FF_FFZSFacilityDefinitionCrossAssetValidation.h"

#include "Algo/Sort.h"

namespace FFZSFacilityDefinitionCrossAssetValidation
{
    namespace Private
    {
        bool IsFiniteVector(const FVector& Value)
        {
            return FMath::IsFinite(Value.X) &&
                FMath::IsFinite(Value.Y) &&
                FMath::IsFinite(Value.Z);
        }

        void SortPaths(TArray<FSoftObjectPath>& Paths)
        {
            Algo::Sort(
                Paths,
                [](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
                {
                    return Left.ToString() < Right.ToString();
                });
        }
    }

    bool IsWithinRequiredDefinitionRoot(const FName PackagePath)
    {
        const FString PackagePathString = PackagePath.ToString();
        const FString Root(RequiredDefinitionRoot);
        return PackagePathString.Equals(Root, ESearchCase::IgnoreCase) ||
            PackagePathString.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase);
    }

    bool HasFiniteBounds(const FBox& Bounds)
    {
        return Bounds.IsValid &&
            Private::IsFiniteVector(Bounds.Min) &&
            Private::IsFiniteVector(Bounds.Max);
    }

    FFFZSFacilityIdentityCollisions FindIdentityCollisions(
        const FFFZSFacilityAssetIdentity& CurrentIdentity,
        const TConstArrayView<FFFZSFacilityAssetIdentity> OtherIdentities)
    {
        FFFZSFacilityIdentityCollisions Result;
        for (const FFFZSFacilityAssetIdentity& Other : OtherIdentities)
        {
            if (Other.AssetPath == CurrentIdentity.AssetPath)
            {
                continue;
            }

            if (!CurrentIdentity.FacilityDefinitionId.IsNone() &&
                Other.FacilityDefinitionId == CurrentIdentity.FacilityDefinitionId)
            {
                Result.DuplicateDefinitionIdAssets.AddUnique(Other.AssetPath);
            }

            if (CurrentIdentity.PrimaryAssetId.IsValid() &&
                Other.PrimaryAssetId == CurrentIdentity.PrimaryAssetId)
            {
                Result.DuplicatePrimaryAssetIdAssets.AddUnique(Other.AssetPath);
            }
        }

        Private::SortPaths(Result.DuplicateDefinitionIdAssets);
        Private::SortPaths(Result.DuplicatePrimaryAssetIdAssets);
        return Result;
    }
}
