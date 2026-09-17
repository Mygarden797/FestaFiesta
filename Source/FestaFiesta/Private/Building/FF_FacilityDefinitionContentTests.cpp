// FF_FacilityDefinitionContentTests.cpp

// 프로젝트의 FacilityDefinition 콘텐츠가 배치·대기열·시각·Crowd 연동 계약을 만족하는지 검증한다.
// 흐름: 정의 에셋 열거 -> Spec/메시/예약 영역 검사 -> Queue Bridge와 Crowd 프로필 호환성 확인.
// 불변식: 검사 중 콘텐츠를 수정하거나 동기 마이그레이션하지 않고 저작된 권위 값을 그대로 읽는다.
// 실패/성능: 실패를 에셋 경로와 함께 누적해 한 번의 자동화 실행에서 수정 대상을 모두 보여 준다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSQueueGeometry.h"
#include "FF_FFZSVisualGeometry.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

namespace
{
    constexpr const TCHAR* DefinitionRoot =
        TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/");

    constexpr double MovementTargetDistanceToleranceCm = 0.01;

    bool IsLocalDiscInsideQueueReservation(
        const FVector& LocalCenterCm,
        const double RadiusCm,
        const FFFZSFacilitySpec& Spec)
    {
        if (LocalCenterCm.ContainsNaN() ||
            !FMath::IsFinite(RadiusCm) ||
            RadiusCm < 0.0)
        {
            return false;
        }

        constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
        const FVector2D GridCenter =
            FFFZSQueueGeometry::LocalPointCmToReservationGrid(
                LocalCenterCm,
                Spec.FootprintSizeCells);
        const double RadiusCells = FMath::Max(0.0,
            (RadiusCm - MovementTargetDistanceToleranceCm) / CellSizeCm);
        if (RadiusCells == 0.0)
        {
            return FFFZSQueueGeometry::IsLocalPointInsideReservation(
                LocalCenterCm, Spec.FootprintSizeCells, Spec.QueueReservedRects);
        }
        const FVector2D BodyMin = GridCenter - FVector2D(RadiusCells, RadiusCells);
        const FVector2D BodyMax = GridCenter + FVector2D(RadiusCells, RadiusCells);

        // Queue rectangles form a union. A body crossing an internal rectangle
        // seam is protected when that union covers its complete bounding box.
        // Sweep every X band and require uninterrupted Y coverage; actual gaps
        // still fail instead of being hidden by one large outer bounding box.
        TArray<double> XEdges{ BodyMin.X, BodyMax.X };
        for (const FFFZSCellRect& Rect : Spec.QueueReservedRects)
        {
            if (!Rect.IsValid()) continue;
            XEdges.Add(FMath::Clamp(static_cast<double>(Rect.Min.X), BodyMin.X, BodyMax.X));
            XEdges.Add(FMath::Clamp(static_cast<double>(Rect.Max.X), BodyMin.X, BodyMax.X));
        }
        XEdges.Sort();
        for (int32 Index = 1; Index < XEdges.Num(); ++Index)
        {
            if (XEdges[Index] <= XEdges[Index - 1]) continue;
            const double SampleX = (XEdges[Index] + XEdges[Index - 1]) * 0.5;
            TArray<FVector2D> YIntervals;
            for (const FFFZSCellRect& Rect : Spec.QueueReservedRects)
            {
                if (Rect.IsValid() && Rect.Min.X <= SampleX && Rect.Max.X >= SampleX)
                {
                    YIntervals.Emplace(Rect.Min.Y, Rect.Max.Y);
                }
            }
            YIntervals.Sort([](const FVector2D& Left, const FVector2D& Right)
            {
                return Left.X < Right.X;
            });
            double CoveredY = BodyMin.Y;
            for (const FVector2D& Interval : YIntervals)
            {
                if (Interval.X > CoveredY) break;
                CoveredY = FMath::Max(CoveredY, Interval.Y);
            }
            if (CoveredY < BodyMax.Y) return false;
        }
        return true;
    }

    bool IsWithinAuthoritativeBounds(const FBox& Bounds, const FFFZSFacilitySpec& Spec)
    {
        constexpr double ToleranceCm = 0.5;
        constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
        const double HalfX = Spec.FootprintSizeCells.X * CellSizeCm * 0.5;
        const double HalfY = Spec.FootprintSizeCells.Y * CellSizeCm * 0.5;
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
    FFFFacilityReservationUnionTest,
    "FestaFiesta.Content.FacilityDefinitions.ReservationUnionProtectsBodies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityReservationUnionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FFFZSFacilitySpec Spec;
    Spec.FootprintSizeCells = FIntPoint(1000, 500);
    Spec.QueueReservedRects = {
        FFFZSCellRect(FIntPoint(650, 500), FIntPoint(750, 750)),
        FFFZSCellRect(FIntPoint(551, 750), FIntPoint(750, 963))
    };
    const FVector SeamCenter(200.0, 500.0, 0.0);
    TestTrue(TEXT("Adjacent queue rectangles jointly protect the whole Crowd body"),
        IsLocalDiscInsideQueueReservation(SeamCenter, 34.0, Spec));
    Spec.QueueReservedRects[1].Min.Y = 751;
    TestFalse(TEXT("A one-centimeter gap through the body remains unprotected"),
        IsLocalDiscInsideQueueReservation(SeamCenter, 34.0, Spec));
    TestFalse(TEXT("An entry beyond the reservation is not protected by its outer bounds"),
        IsLocalDiscInsideQueueReservation(FVector(400.0, 300.0, 0.0), 34.0, Spec));
    return true;
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

        // Roads, tiles, and lighting deliberately have no visitor service or queue.
        // Keep their placement/visual checks, and verify the absence contract
        // instead of requiring fabricated Entry anchors and queue lanes.
        const bool bHasVisitorService =
            !Definition->Spec.IsGroundSurface() &&
            Definition->Spec.Category != EFFZSFacilityCategory::Lighting;
        if (!bHasVisitorService)
        {
            TestEqual(*FString::Printf(TEXT("Non-service facility has no service capacity: %s"), *AssetId.ToString()),
                Definition->Spec.QueueRules.ServiceCapacity, 0);
            TestEqual(*FString::Printf(TEXT("Non-service facility has no waiting slots: %s"), *AssetId.ToString()),
                Definition->Spec.QueueRules.MaxWaitingAgents, 0);
            TestTrue(*FString::Printf(TEXT("Non-service facility has no visitor anchors: %s"), *AssetId.ToString()),
                Definition->Spec.Anchors.IsEmpty());
        }

        // FFZS 공용 검증은 의미상 목표점 간격만 보장한다. FestaFiesta 제품 콘텐츠는 실제 Crowd
        // 신체 크기에도 맞아야 하므로 권위 Definition 순회에서 Capsule 지름을 추가로 확인한다.
        // Crowd 반경이 바뀌어 기존 대기 목표가 조용히 겹치는 일을 막기 위해 이 검증을 함께 둔다.
        const double MinimumCrowdTargetSeparationCm =
            static_cast<double>(FUnitData().Radius) * 2.0;
        TestTrue(
            *FString::Printf(TEXT("Queue SlotSpacing fits the default Crowd diameter: %s"), *AssetId.ToString()),
            static_cast<double>(Definition->Spec.QueueRules.SlotSpacingCm) +
                    MovementTargetDistanceToleranceCm >=
                MinimumCrowdTargetSeparationCm);

        TArray<FVector> QueueSlotPoints;
        TestTrue(
            *FString::Printf(TEXT("Queue slots can be sampled for movement-target QA: %s"), *AssetId.ToString()),
            FFFZSQueueGeometry::BuildLinearQueueSlotPoints(
                Definition->Spec.QueueSplinePointsCm,
                Definition->Spec.QueueRules.MaxWaitingAgents,
                Definition->Spec.QueueRules.SlotSpacingCm,
                QueueSlotPoints));

        for (int32 FirstSlotIndex = 0; FirstSlotIndex < QueueSlotPoints.Num(); ++FirstSlotIndex)
        {
            TestTrue(
                *FString::Printf(
                    TEXT("Queue slot %d Crowd body is protected by QueueReservedRects: %s"),
                    FirstSlotIndex,
                    *AssetId.ToString()),
                IsLocalDiscInsideQueueReservation(
                    QueueSlotPoints[FirstSlotIndex],
                    FUnitData().Radius,
                    Definition->Spec));

            for (int32 SecondSlotIndex = FirstSlotIndex + 1;
                 SecondSlotIndex < QueueSlotPoints.Num();
                 ++SecondSlotIndex)
            {
                const double DistanceCm = FVector::Dist2D(
                    QueueSlotPoints[FirstSlotIndex],
                    QueueSlotPoints[SecondSlotIndex]);
                TestTrue(
                    *FString::Printf(
                        TEXT("Queue slots %d/%d fit Crowd bodies: %s"),
                        FirstSlotIndex,
                        SecondSlotIndex,
                        *AssetId.ToString()),
                    DistanceCm + MovementTargetDistanceToleranceCm >=
                        MinimumCrowdTargetSeparationCm);
            }
        }

        const FFFZSAnchorDefinition* EntryZero = nullptr;
        for (int32 FirstAnchorIndex = 0;
             FirstAnchorIndex < Definition->Spec.Anchors.Num();
             ++FirstAnchorIndex)
        {
            const FFFZSAnchorDefinition& FirstAnchor =
                Definition->Spec.Anchors[FirstAnchorIndex];
            if (FirstAnchor.Type == EFFZSAnchorType::Entry && FirstAnchor.Index == 0)
            {
                EntryZero = &FirstAnchor;
            }

            for (int32 SecondAnchorIndex = FirstAnchorIndex + 1;
                 SecondAnchorIndex < Definition->Spec.Anchors.Num();
                 ++SecondAnchorIndex)
            {
                const FFFZSAnchorDefinition& SecondAnchor =
                    Definition->Spec.Anchors[SecondAnchorIndex];
                const double DistanceCm = FVector::Dist2D(
                    FirstAnchor.LocalTransform.GetLocation(),
                    SecondAnchor.LocalTransform.GetLocation());
                TestTrue(
                    *FString::Printf(
                        TEXT("Facility anchors %d/%d fit Crowd bodies: %s"),
                        FirstAnchorIndex,
                        SecondAnchorIndex,
                        *AssetId.ToString()),
                    DistanceCm + MovementTargetDistanceToleranceCm >=
                        MinimumCrowdTargetSeparationCm);
            }

            for (int32 SlotIndex = 0; SlotIndex < QueueSlotPoints.Num(); ++SlotIndex)
            {
                const double DistanceCm = FVector::Dist2D(
                    FirstAnchor.LocalTransform.GetLocation(),
                    QueueSlotPoints[SlotIndex]);
                TestTrue(
                    *FString::Printf(
                        TEXT("Anchor %d and Queue slot %d fit Crowd bodies: %s"),
                        FirstAnchorIndex,
                        SlotIndex,
                        *AssetId.ToString()),
                    DistanceCm + MovementTargetDistanceToleranceCm >=
                        MinimumCrowdTargetSeparationCm);
            }
        }

        if (bHasVisitorService)
        {
            TestNotNull(
                *FString::Printf(TEXT("Entry[0] exists for Approach QA: %s"), *AssetId.ToString()),
                EntryZero);
        }
        if (EntryZero)
        {
            TestTrue(
                *FString::Printf(
                    TEXT("Entry[0] Crowd body is protected by QueueReservedRects: %s"),
                    *AssetId.ToString()),
                IsLocalDiscInsideQueueReservation(
                    EntryZero->LocalTransform.GetLocation(),
                    FUnitData().Radius,
                    Definition->Spec));
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
        TestEqual(
            *FString::Printf(TEXT("Party Queue projection matches the facility service contract: %s"), *AssetId.ToString()),
            UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
                Definition->Spec,
                FTransform::Identity,
                Lane),
            bHasVisitorService);
        TestEqual(
            *FString::Printf(TEXT("Party Queue 슬롯 수가 MaxWaitingAgents와 같다: %s"), *AssetId.ToString()),
            Lane.ExplicitSlotTransforms.Num(),
            Definition->Spec.QueueRules.MaxWaitingAgents);

        // Check authored upgrade geometry against the project's real Crowd radius as well.
        if (Definition->Spec.bUsesFacilityLevels)
        {
            for (const FFFZSFacilityUpgradeTier& Tier : Definition->Spec.UpgradeTiers)
            {
                const FString Label = FString::Printf(TEXT("%s Lv.%d"), *AssetId.ToString(), Tier.TargetLevel);
                const FFFZSFacilitySpec Upgraded = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
                    Definition->Spec, Tier.TargetLevel, Tier.SizeMultiplier);
                FFF_QueueLaneGeometry UpgradedLane;
                TestEqual(*FString::Printf(TEXT("Upgraded Party Queue matches the service contract: %s"), *Label),
                    UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
                        Upgraded, FTransform::Identity, UpgradedLane),
                    bHasVisitorService);
                for (const FTransform& Slot : UpgradedLane.ExplicitSlotTransforms)
                {
                    TestTrue(*FString::Printf(TEXT("Upgraded waiting body stays reserved: %s"), *Label),
                        IsLocalDiscInsideQueueReservation(Slot.GetLocation(), FUnitData().Radius, Upgraded));
                }
                for (const FFFZSAnchorDefinition& Anchor : Upgraded.Anchors)
                {
                    if (Anchor.Type == EFFZSAnchorType::Entry && Anchor.Index == 0)
                    {
                        TestTrue(*FString::Printf(TEXT("Upgraded entry body stays reserved: %s"), *Label),
                            IsLocalDiscInsideQueueReservation(
                                Anchor.LocalTransform.GetLocation(), FUnitData().Radius, Upgraded));
                    }
                }
                UStaticMesh* LevelMesh = Upgraded.VisualMesh.LoadSynchronous();
                if (TestNotNull(*FString::Printf(TEXT("Authored or base level mesh loads: %s"), *Label), LevelMesh))
                {
                    const FTransform LevelTransform = FFZSVisualGeometry::ResolveVisualMeshTransform(Upgraded, LevelMesh);
                    TestTrue(*FString::Printf(TEXT("Upgraded visual fits its actual building bounds: %s"), *Label),
                        IsWithinAuthoritativeBounds(FFZSVisualGeometry::TransformBounds(
                            LevelMesh->GetBoundingBox(), LevelTransform), Upgraded));
                }
            }
        }
    }

    return true;
}

#endif
