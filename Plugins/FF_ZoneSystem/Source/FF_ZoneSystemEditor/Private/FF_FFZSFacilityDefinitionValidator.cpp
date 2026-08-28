#include "FF_FFZSFacilityDefinitionValidator.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityDefinitionCrossAssetValidation.h"
#include "FF_FFZSVisualGeometry.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FF_FFZSFacilityDefinitionValidator)

#define LOCTEXT_NAMESPACE "FFZSFacilityDefinitionValidator"

namespace FFZSFacilityDefinitionValidatorPrivate
{
    FString JoinAssetPaths(const TArray<FSoftObjectPath>& Paths)
    {
        TArray<FString> PathStrings;
        PathStrings.Reserve(Paths.Num());
        for (const FSoftObjectPath& Path : Paths)
        {
            PathStrings.Add(Path.ToString());
        }
        return FString::Join(PathStrings, TEXT(", "));
    }

    FSoftObjectPath GetStableAssetPath(const FAssetData& AssetData, const UObject& Asset)
    {
        const FSoftObjectPath RegistryPath = AssetData.GetSoftObjectPath();
        return RegistryPath.IsValid() ? RegistryPath : FSoftObjectPath(&Asset);
    }
}

bool UFFZSFacilityDefinitionValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& InContext) const
{
    (void)InAssetData;
    (void)InContext;
    return InAsset && InAsset->IsA<UFFZSFacilityDefinition>();
}

EDataValidationResult UFFZSFacilityDefinitionValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& InContext)
{
    (void)InContext;

    const UFFZSFacilityDefinition* Definition = Cast<UFFZSFacilityDefinition>(InAsset);
    if (!Definition)
    {
        return EDataValidationResult::NotValidated;
    }

    bool bIsValid = true;
    const auto Fail = [this, InAsset, &bIsValid](const FText& Message)
    {
        AssetFails(InAsset, Message);
        bIsValid = false;
    };

    const bool bInRequiredRoot =
        FFZSFacilityDefinitionCrossAssetValidation::IsWithinRequiredDefinitionRoot(
            InAssetData.PackagePath);
    if (!bInRequiredRoot)
    {
        Fail(FText::Format(
            LOCTEXT(
                "OutsideRequiredRoot",
                "Facility Definition must be stored under {0}; current package path is {1}"),
            FText::FromString(
                FFZSFacilityDefinitionCrossAssetValidation::RequiredDefinitionRoot),
            FText::FromName(InAssetData.PackagePath)));
    }

    if (bInRequiredRoot && !Definition->Spec.VisualMesh.IsNull() &&
        Definition->Spec.VisualSizingMode != EFFZSVisualSizingMode::MatchPlacementBounds)
    {
        Fail(LOCTEXT(
            "ProductionVisualSizingMustBeAuthoritative",
            "Production Facility Definitions with a VisualMesh must use MatchPlacementBounds so FootprintSizeCells and PlacementObstacleHeightCm remain the only authored size source"));
    }

    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    if (AssetRegistry.IsLoadingAssets())
    {
        Fail(LOCTEXT(
            "AssetRegistryStillLoading",
            "Facility Definition uniqueness cannot be proven while the Asset Registry is still loading"));
    }
    else
    {
        FARFilter DefinitionFilter;
        DefinitionFilter.ClassPaths.Add(
            UFFZSFacilityDefinition::StaticClass()->GetClassPathName());
        DefinitionFilter.bRecursiveClasses = true;

        TArray<FAssetData> DefinitionAssets;
        if (!AssetRegistry.GetAssets(DefinitionFilter, DefinitionAssets))
        {
            Fail(LOCTEXT(
                "DefinitionEnumerationFailed",
                "Asset Registry failed to enumerate Facility Definition assets; uniqueness cannot be proven"));
        }
        else
        {
            DefinitionAssets.Sort(
                [](const FAssetData& Left, const FAssetData& Right)
                {
                    return Left.GetSoftObjectPath().ToString() <
                        Right.GetSoftObjectPath().ToString();
                });

            TArray<FFFZSFacilityAssetIdentity> OtherIdentities;
            OtherIdentities.Reserve(DefinitionAssets.Num());
            const FSoftObjectPath CurrentPath =
                FFZSFacilityDefinitionValidatorPrivate::GetStableAssetPath(
                    InAssetData,
                    *Definition);

            for (const FAssetData& OtherAssetData : DefinitionAssets)
            {
                if (OtherAssetData.GetSoftObjectPath() == CurrentPath)
                {
                    continue;
                }

                // Facility Definitions contain only value data and soft references. Loading
                // one here does not load its mesh and is required to inspect the authoritative,
                // potentially unsaved FacilityDefinitionId rather than infer it from a tag.
                const UFFZSFacilityDefinition* OtherDefinition =
                    Cast<UFFZSFacilityDefinition>(OtherAssetData.GetAsset());
                if (!OtherDefinition)
                {
                    Fail(FText::Format(
                        LOCTEXT(
                            "DefinitionLoadFailed",
                            "Could not read Facility Definition {0}; uniqueness cannot be proven"),
                        FText::FromString(
                            OtherAssetData.GetSoftObjectPath().ToString())));
                    continue;
                }

                FFFZSFacilityAssetIdentity& Identity = OtherIdentities.AddDefaulted_GetRef();
                Identity.AssetPath = OtherAssetData.GetSoftObjectPath();
                Identity.FacilityDefinitionId =
                    OtherDefinition->Spec.FacilityDefinitionId;
                Identity.PrimaryAssetId = OtherDefinition->GetPrimaryAssetId();
            }

            FFFZSFacilityAssetIdentity CurrentIdentity;
            CurrentIdentity.AssetPath = CurrentPath;
            CurrentIdentity.FacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
            CurrentIdentity.PrimaryAssetId = Definition->GetPrimaryAssetId();

            const FFFZSFacilityIdentityCollisions Collisions =
                FFZSFacilityDefinitionCrossAssetValidation::FindIdentityCollisions(
                    CurrentIdentity,
                    OtherIdentities);

            if (!Collisions.DuplicateDefinitionIdAssets.IsEmpty())
            {
                Fail(FText::Format(
                    LOCTEXT(
                        "DuplicateDefinitionId",
                        "FacilityDefinitionId '{0}' is also used by: {1}"),
                    FText::FromName(CurrentIdentity.FacilityDefinitionId),
                    FText::FromString(
                        FFZSFacilityDefinitionValidatorPrivate::JoinAssetPaths(
                            Collisions.DuplicateDefinitionIdAssets))));
            }

            if (!Collisions.DuplicatePrimaryAssetIdAssets.IsEmpty())
            {
                Fail(FText::Format(
                    LOCTEXT(
                        "DuplicatePrimaryAssetId",
                        "PrimaryAssetId '{0}' collides with: {1}"),
                    FText::FromString(CurrentIdentity.PrimaryAssetId.ToString()),
                    FText::FromString(
                        FFZSFacilityDefinitionValidatorPrivate::JoinAssetPaths(
                            Collisions.DuplicatePrimaryAssetIdAssets))));
            }
        }
    }

    if (!Definition->Spec.VisualMesh.IsNull())
    {
        const FSoftObjectPath VisualMeshPath =
            Definition->Spec.VisualMesh.ToSoftObjectPath();
        if (!VisualMeshPath.IsValid() || !VisualMeshPath.IsAsset())
        {
            Fail(FText::Format(
                LOCTEXT(
                    "InvalidVisualMeshPath",
                    "VisualMesh soft path is not a valid top-level asset path: {0}"),
                FText::FromString(VisualMeshPath.ToString())));
        }
        else
        {
            const FAssetData VisualMeshAsset =
                AssetRegistry.GetAssetByObjectPath(VisualMeshPath);
            if (!VisualMeshAsset.IsValid())
            {
                Fail(FText::Format(
                    LOCTEXT(
                        "MissingVisualMesh",
                        "VisualMesh soft path does not resolve in the Asset Registry: {0}"),
                    FText::FromString(VisualMeshPath.ToString())));
            }
            else if (VisualMeshAsset.AssetClassPath !=
                UStaticMesh::StaticClass()->GetClassPathName())
            {
                Fail(FText::Format(
                    LOCTEXT(
                        "VisualMeshWrongClass",
                        "VisualMesh soft path resolves to {0}, not a StaticMesh: {1}"),
                    FText::FromString(VisualMeshAsset.AssetClassPath.ToString()),
                    FText::FromString(VisualMeshPath.ToString())));
            }

            const UStaticMesh* LoadedMesh = Definition->Spec.VisualMesh.Get();
            if (!LoadedMesh &&
                Definition->Spec.VisualSizingMode != EFFZSVisualSizingMode::SourceMeshSize)
            {
                LoadedMesh = Definition->Spec.VisualMesh.LoadSynchronous();
                if (!LoadedMesh)
                {
                    Fail(FText::Format(
                        LOCTEXT(
                            "VisualSizingMeshLoadFailed",
                            "VisualMesh must load to validate non-source sizing: {0}"),
                        FText::FromString(VisualMeshPath.ToString())));
                }
            }

            if (LoadedMesh)
            {
                if (!FFZSFacilityDefinitionCrossAssetValidation::HasFiniteBounds(
                        LoadedMesh->GetBoundingBox()))
                {
                    Fail(FText::Format(
                        LOCTEXT(
                            "NonFiniteVisualMeshBounds",
                            "Loaded VisualMesh has invalid or non-finite bounds: {0}"),
                        FText::FromString(VisualMeshPath.ToString())));
                }
                else if (Definition->Spec.VisualSizingMode !=
                        EFFZSVisualSizingMode::SourceMeshSize &&
                    !FFZSVisualGeometry::HasUsableSourceBounds(
                        LoadedMesh->GetBoundingBox()))
                {
                    Fail(FText::Format(
                        LOCTEXT(
                            "UnusableVisualMeshBounds",
                            "UniformFit/MatchPlacementBounds requires positive X/Y/Z mesh extents: {0}"),
                        FText::FromString(VisualMeshPath.ToString())));
                }
            }
        }
    }

    if (bIsValid)
    {
        AssetPasses(InAsset);
    }
    return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE
