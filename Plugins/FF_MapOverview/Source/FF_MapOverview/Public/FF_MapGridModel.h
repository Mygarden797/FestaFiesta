#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSBuildableGrid.h"

/** Pure presentation calculations. Placement remains owned by FF_ZoneSystem. */
namespace FFMapGridModel
{
    inline constexpr int32 MaximumCells = 16384;
    inline constexpr int32 MaximumRenderedLines = 32768;
    inline constexpr double LocalRadiusCm = 4800.0;
    inline constexpr double MinimumSpacingPixels = 11.0;

    inline int32 ResolveZoomStep(const int32 BaseStepCm, const double BaseSpacingPixels,
        const int32 PreviousStepCm = 0)
    {
        const int32 Base = FMath::Clamp(BaseStepCm, 25, 1000);
        if (!FMath::IsFinite(BaseSpacingPixels) || BaseSpacingPixels <= 0.0)
        {
            return FMath::Max(Base, PreviousStepCm);
        }
        // A band around the desired spacing prevents rebuilding during tiny zoom changes.
        if (PreviousStepCm >= Base && PreviousStepCm <= Base * 65536)
        {
            const double PreviousSpacing = BaseSpacingPixels * PreviousStepCm / Base;
            if (PreviousSpacing >= 9.0 && PreviousSpacing <= 18.0) return PreviousStepCm;
        }
        int32 Step = Base;
        while (BaseSpacingPixels * Step / Base < MinimumSpacingPixels && Step < Base * 65536)
        {
            Step *= 2;
        }
        return Step;
    }

    inline FBox2D MakeLocalWindow(const FVector2D& Center, const int32 BaseStepCm)
    {
        const double Snap = FMath::Max(400.0, static_cast<double>(BaseStepCm) * 4.0);
        const FVector2D StableCenter(FMath::GridSnap(Center.X, Snap), FMath::GridSnap(Center.Y, Snap));
        return FBox2D(StableCenter - FVector2D(LocalRadiusCm), StableCenter + FVector2D(LocalRadiusCm));
    }

    inline float EdgeOpacity(const FVector2D& Point, const FVector2D& Center,
        const FVector2D& HalfExtent, const bool bWholeMap)
    {
        if (bWholeMap) return 1.0f;
        const double Distance = FMath::Max(FMath::Abs(Point.X - Center.X) / FMath::Max(1.0, HalfExtent.X),
            FMath::Abs(Point.Y - Center.Y) / FMath::Max(1.0, HalfExtent.Y));
        const double Fade = FMath::Clamp((Distance - 0.80) / 0.20, 0.0, 1.0);
        return static_cast<float>(FMath::Lerp(1.0, 0.55, Fade));
    }

    /**
     * Compact an authority mask without inventing rules: every contributing authority cell
     * must be free. Coordinates become an index lattice; OutTransform maps that lattice to
     * the same real world points for the existing budgeted terrain sampler.
     */
    inline bool CompactAuthorityMask(const FFFZSBuildableGridMask& Source,
        const FTransform& SourceTransform, const int32 CellBudget, const double MinimumWorldStepCm,
        FFFZSBuildableGridMask& OutMask, FTransform& OutTransform, const bool bWorldAligned = false)
    {
        OutMask.Reset();
        if (Source.GetCellCount() <= 0 || CellBudget <= 0 || SourceTransform.ContainsNaN()) return false;
        int32 Factor = 1;
        const int32 Budget = FMath::Clamp(CellBudget, 1, MaximumCells);
        while ((FMath::DivideAndRoundUp(Source.GetNumX(), Factor) *
                FMath::DivideAndRoundUp(Source.GetNumY(), Factor) > Budget ||
                Source.GetStepCm() * Factor < MinimumWorldStepCm) && Factor < (1 << 20))
        {
            Factor *= 2;
        }
        const FVector AlignmentOrigin = bWorldAligned
            ? SourceTransform.InverseTransformPosition(FVector::ZeroVector) : FVector::ZeroVector;
        const auto ResolveOffset = [&](double SourceMin, double Alignment)
        {
            const int32 FirstIndex = FMath::RoundToInt((SourceMin - Alignment) / Source.GetStepCm());
            return ((-FirstIndex % Factor) + Factor) % Factor;
        };
        const FIntPoint Offset(ResolveOffset(Source.GetMinLocalCm().X, AlignmentOrigin.X),
            ResolveOffset(Source.GetMinLocalCm().Y, AlignmentOrigin.Y));
        const int32 NumX = FMath::DivideAndRoundUp(FMath::Max(0, Source.GetNumX() - Offset.X), Factor);
        const int32 NumY = FMath::DivideAndRoundUp(FMath::Max(0, Source.GetNumY() - Offset.Y), Factor);
        if (NumX <= 0 || NumY <= 0) return false;
        constexpr int32 IndexStep = 100;
        if (!OutMask.Initialize(FVector2D::ZeroVector, FVector2D(NumX, NumY) * IndexStep, IndexStep)) return false;
        const double WorldStep = Source.GetStepCm() * Factor;
        const FVector SourceScale = SourceTransform.GetScale3D();
        OutTransform = FTransform(SourceTransform.GetRotation(),
            SourceTransform.TransformPosition(FVector(Source.GetMinLocalCm() +
                FVector2D(Offset) * Source.GetStepCm(), 0.0)),
            FVector(SourceScale.X * WorldStep / IndexStep, SourceScale.Y * WorldStep / IndexStep, SourceScale.Z));
        OutMask.FacilityCategory = Source.FacilityCategory;
        OutMask.ObstacleHeightCm = Source.ObstacleHeightCm;
        OutMask.bIncludeGroundSurfaceSupport = Source.bIncludeGroundSurfaceSupport;
        for (int32 Y = 0; Y < NumY; ++Y)
        {
            for (int32 X = 0; X < NumX; ++X)
            {
                const int32 FirstX = Offset.X + X * Factor;
                const int32 FirstY = Offset.Y + Y * Factor;
                const int32 EndX = FMath::Min(FirstX + Factor, Source.GetNumX());
                const int32 EndY = FMath::Min(FirstY + Factor, Source.GetNumY());
                bool bFree = EndX - FirstX == Factor && EndY - FirstY == Factor;
                for (int32 SourceY = FirstY; bFree && SourceY < EndY; ++SourceY)
                    for (int32 SourceX = FirstX; bFree && SourceX < EndX; ++SourceX)
                        bFree = Source.IsBuildable(SourceX, SourceY);
                OutMask.SetBuildable(X, Y, bFree);
            }
        }
        for (const FFFZSBuildGridObstacle& Obstacle : Source.DeferredWorldObstacles)
        {
            FFFZSBuildGridObstacle& Compact = OutMask.DeferredWorldObstacles.Add_GetRef(Obstacle);
            Compact.FirstCell = FIntPoint(FMath::Max(0, Obstacle.FirstCell.X - Offset.X) / Factor,
                FMath::Max(0, Obstacle.FirstCell.Y - Offset.Y) / Factor);
            Compact.EndCell = FIntPoint(FMath::Min(NumX, FMath::DivideAndRoundUp(
                FMath::Max(0, Obstacle.EndCell.X - Offset.X), Factor)), FMath::Min(NumY,
                FMath::DivideAndRoundUp(FMath::Max(0, Obstacle.EndCell.Y - Offset.Y), Factor)));
        }
        return true;
    }
}
