#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSVisualGeometry.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

namespace
{
    constexpr const TCHAR* DefinitionRoot =
        TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/");

    bool IsWithinAuthoritativeBounds(const FBox& Bounds, const FFFZSFacilitySpec& Spec)
    {
        constexpr double ToleranceCm = 0.5;
        const double HalfX = Spec.FootprintSizeCells.X * 0.5;
        const double HalfY = Spec.FootprintSizeCells.Y * 0.5;
        return Bounds.IsValid &&
            Bounds.Min.X >= -HalfX - ToleranceCm &&
            Bounds.Max.X <= HalfX + ToleranceCm &&
            Bounds.Min.Y >= -HalfY - ToleranceCm &&
            Bounds.Max.Y <= HalfY + ToleranceCm &&
            Bounds.Min.Z >= -ToleranceCm &&
            Bounds.Max.Z <= Spec.PlacementObstacleHeightCm + ToleranceCm;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFFacilityDefinitionContentProjectionTest,
    "FestaFiesta.Content.FacilityDefinitions.AllDefinitionsAreProjectable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityDefinitionContentProjectionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UAssetManager& AssetManager = UAssetManager::Get();
    TArray<FPrimaryAssetId> AssetIds;
    AssetManager.GetPrimaryAssetIdList(
        UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(),
        AssetIds);
    AssetIds.Sort([](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
    {
        return Left.ToString() < Right.ToString();
    });

    TestTrue(TEXT("최소 한 개의 시설 Definition이 Asset Manager에 등록된다"), !AssetIds.IsEmpty());
    TSet<FName> DefinitionIds;
    for (const FPrimaryAssetId& AssetId : AssetIds)
    {
        const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
        UFFZSFacilityDefinition* Definition =
            Cast<UFFZSFacilityDefinition>(AssetPath.TryLoad());
        if (!TestNotNull(
                *FString::Printf(TEXT("Definition을 로드한다: %s"), *AssetId.ToString()),
                Definition))
        {
            continue;
        }

        const FString PackageName = Definition->GetPackage()->GetName();
        TestTrue(
            *FString::Printf(TEXT("Definition은 지정 저작 루트에 있다: %s"), *PackageName),
            PackageName.StartsWith(DefinitionRoot));
        TestTrue(
            *FString::Printf(TEXT("PrimaryAssetId가 Definition ID에서 파생된다: %s"), *AssetId.ToString()),
            Definition->GetPrimaryAssetId() == AssetId);

        const FName DefinitionId = Definition->Spec.FacilityDefinitionId;
        TestFalse(
            *FString::Printf(TEXT("FacilityDefinitionId가 중복되지 않는다: %s"), *DefinitionId.ToString()),
            DefinitionIds.Contains(DefinitionId));
        DefinitionIds.Add(DefinitionId);

        TArray<FText> Errors;
        if (!Definition->IsDefinitionValid(Errors))
        {
            for (const FText& Error : Errors)
            {
                AddError(FString::Printf(
                    TEXT("%s: %s"),
                    *AssetId.ToString(),
                    *Error.ToString()));
            }
        }

        TestTrue(
            *FString::Printf(TEXT("Loaded Definition is its canonical install source: %s"), *AssetId.ToString()),
            UFFZSFacilityDefinition::DoesSpecMatchLoadedDefinition(Definition->Spec));
        FFFZSFacilitySpec MutatedSpec = Definition->Spec;
        ++MutatedSpec.ClearanceCells;
        TestFalse(
            *FString::Printf(TEXT("A mutated Spec cannot bypass its Definition: %s"), *AssetId.ToString()),
            UFFZSFacilityDefinition::DoesSpecMatchLoadedDefinition(MutatedSpec));

        UStaticMesh* Mesh = Definition->Spec.VisualMesh.LoadSynchronous();
        if (!TestNotNull(
                *FString::Printf(TEXT("VisualMesh를 로드한다: %s"), *AssetId.ToString()),
                Mesh))
        {
            continue;
        }
        TestEqual(
            *FString::Printf(TEXT("Production visual size is Definition-authoritative: %s"), *AssetId.ToString()),
            Definition->Spec.VisualSizingMode,
            EFFZSVisualSizingMode::MatchPlacementBounds);
        FTransform VisualTransform;
        TestTrue(
            *FString::Printf(TEXT("Visual sizing resolves without fallback: %s"), *AssetId.ToString()),
            FFZSVisualGeometry::TryResolveVisualTransformFromBounds(
                Definition->Spec, Mesh->GetBoundingBox(), VisualTransform));
        const FBox VisualBounds =
            FFZSVisualGeometry::TransformBounds(Mesh->GetBoundingBox(), VisualTransform);
        TestTrue(
            *FString::Printf(TEXT("VisualMesh가 권위 placement bounds 안에 있다: %s"), *AssetId.ToString()),
            IsWithinAuthoritativeBounds(VisualBounds, Definition->Spec));

        FFF_QueueLaneGeometry Lane;
        TestTrue(
            *FString::Printf(TEXT("Party Queue가 frozen Spec에서 투영된다: %s"), *AssetId.ToString()),
            UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
                Definition->Spec,
                FTransform::Identity,
                Lane));
        TestEqual(
            *FString::Printf(TEXT("Party Queue 슬롯 수가 MaxWaitingAgents와 같다: %s"), *AssetId.ToString()),
            Lane.ExplicitSlotTransforms.Num(),
            Definition->Spec.QueueRules.MaxWaitingAgents);
    }

    return true;
}

#endif
