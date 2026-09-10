#include "FF_FFZSWorldSubsystem.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"
#include "NavigationPath.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSExternalPolicy.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSOrientedGeometry.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSQueueAgent.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSQueueGeometry.h"
#include "FF_ZoneSystem.h"
#include "FF_FFZSZoneActor.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSSpatialHash.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "FFZSWorldSubsystem"

namespace
{
    FFFZSOrientedRect TransformRegion(const FFFZSOrientedRect& Rect, const FTransform& Transform)
    {
        FFFZSOrientedRect Result = Rect;
        const FVector Center = Transform.TransformPosition(FVector(Rect.CenterCell, 0.0));
        Result.CenterCell = FVector2D(Center.X, Center.Y);
        Result.YawDegrees = FRotator::NormalizeAxis(Rect.YawDegrees + Transform.Rotator().Yaw);
        return Result;
    }

    FBox2D GetRegionBounds(const TArray<FFFZSOrientedRect>& Regions)
    {
        FBox2D Bounds(ForceInit);
        for (const FFFZSOrientedRect& Region : Regions)
        {
            Bounds += FFZSOrientedGeometry::GetAabb(Region);
        }
        return Bounds;
    }

    // 검증 단계의 권위 Footprint OBB를 재사용해 footprint 기하 계산을 반복하지 않는다.
    FTransform MakePlacementTransform(
        const AFFZSGameplayZone& Zone,
        const FFFZSGridPlacement& Placement,
        const TArray<FFFZSOrientedRect>& FootprintRects)
    {
        FVector Center = Zone.GetActorLocation();
        if (!FootprintRects.IsEmpty())
        {
            const FFFZSOrientedRect& Rect = FootprintRects[0];
            const double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
            Center = Zone.GetActorTransform().TransformPosition(FVector(
                Rect.CenterCell.X * CellSizeCm,
                Rect.CenterCell.Y * CellSizeCm,
                0.0));
        }
        const FQuat Rotation = Zone.GetActorQuat() * FQuat(
            FVector::UpVector,
            FMath::DegreesToRadians(
                UFFZSGridLibrary::YawCentidegreesToDegrees(
                    UFFZSGridLibrary::GetEffectiveYawCentidegrees(Placement))));
        return FTransform(Rotation, Center, FVector::OneVector);
    }

    bool IsWorldChangeCommand(const EFFZSCommandType Type)
    {
        return Type == EFFZSCommandType::Install || Type == EFFZSCommandType::Move ||
            Type == EFFZSCommandType::Remove || Type == EFFZSCommandType::Upgrade;
    }

    bool IsKnownChangePhase(const EFFZSChangePhase Phase)
    {
        return Phase == EFFZSChangePhase::InitialSetup ||
            Phase == EFFZSChangePhase::MorningCheck ||
            Phase == EFFZSChangePhase::LiveOperation ||
            Phase == EFFZSChangePhase::EveningSettlement;
    }

    bool IsWorldChangePhase(const EFFZSChangePhase Phase)
    {
        return Phase == EFFZSChangePhase::InitialSetup || Phase == EFFZSChangePhase::MorningCheck;
    }
    bool IsPointOnSegment2D(const FVector2D& Point, const FVector2D& A, const FVector2D& B, const double ToleranceCm)
    {
        const FVector2D Segment = B - A;
        const double LengthSquared = Segment.SizeSquared();
        const double ToleranceSquared = ToleranceCm * ToleranceCm;
        if (LengthSquared <= UE_SMALL_NUMBER)
        {
            return (Point - A).SizeSquared() <= ToleranceSquared;
        }
        const double T = FMath::Clamp(FVector2D::DotProduct(Point - A, Segment) / LengthSquared, 0.0, 1.0);
        return (Point - (A + Segment * T)).SizeSquared() <= ToleranceSquared;
    }

    bool IsPointInsidePolygon2D(const FVector2D& Point, const TArray<FVector2D>& Polygon, const bool bIncludeBoundary)
    {
        bool bInside = false;
        for (int32 I = 0, J = Polygon.Num() - 1; I < Polygon.Num(); J = I++)
        {
            const FVector2D& A = Polygon[J];
            const FVector2D& B = Polygon[I];
            if (IsPointOnSegment2D(Point, A, B, 0.01))
            {
                return bIncludeBoundary;
            }
            if (((B.Y > Point.Y) != (A.Y > Point.Y)) &&
                Point.X < (A.X - B.X) * (Point.Y - B.Y) / ((A.Y - B.Y) + UE_SMALL_NUMBER) + B.X)
            {
                bInside = !bInside;
            }
        }
        return bInside;
    }

    bool SegmentIntersectsOpenRect(
        const FVector2D& A,
        const FVector2D& B,
        const double MinX,
        const double MinY,
        const double MaxX,
        const double MaxY)
    {
        constexpr double InteriorInsetCm = 0.01;
        const double Left = MinX + InteriorInsetCm;
        const double Bottom = MinY + InteriorInsetCm;
        const double Right = MaxX - InteriorInsetCm;
        const double Top = MaxY - InteriorInsetCm;
        if (Left >= Right || Bottom >= Top)
        {
            return false;
        }

        const FVector2D Delta = B - A;
        double T0 = 0.0;
        double T1 = 1.0;
        const auto Clip = [&T0, &T1](const double P, const double Q)
        {
            if (FMath::Abs(P) <= UE_SMALL_NUMBER)
            {
                return Q >= 0.0;
            }
            const double R = Q / P;
            if (P < 0.0)
            {
                if (R > T1)
                {
                    return false;
                }
                T0 = FMath::Max(T0, R);
            }
            else
            {
                if (R < T0)
                {
                    return false;
                }
                T1 = FMath::Min(T1, R);
            }
            return true;
        };

        return Clip(-Delta.X, A.X - Left) &&
            Clip(Delta.X, Right - A.X) &&
            Clip(-Delta.Y, A.Y - Bottom) &&
            Clip(Delta.Y, Top - A.Y) &&
            T0 <= T1;
    }

    TArray<FVector2D> GetZonePolygonInCellSpace(
        const AFFZSGameplayZone& Zone,
        const FTransform& ZoneWorldTransform,
        const FTransform& CellSpaceWorldTransform)
    {
        TArray<FVector2D> Result;
        const TArray<FVector2D> LocalPolygon = Zone.GetBoundaryPolygonLocalCm();
        Result.Reserve(LocalPolygon.Num());
        for (const FVector2D& Point : LocalPolygon)
        {
            const FVector World = ZoneWorldTransform.TransformPosition(FVector(Point.X, Point.Y, 0.0));
            const FVector CellLocal = CellSpaceWorldTransform.InverseTransformPosition(World);
            Result.Add(FVector2D(CellLocal.X, CellLocal.Y));
        }
        return Result;
    }

    struct FPreparedZoneOverlap
    {
        double HalfVerticalExtentCm = 0.0;
        FTransform WorldTransform;
        FBox2D BoundsInCellSpace = FBox2D(EForceInit::ForceInit);
        TArray<FVector2D> PolygonInCellSpace;
    };

    // 한 검증 호출 동안 후보 Zone의 Transform, 폴리곤, AABB를 대상 셀 공간에 한 번만 준비한다.
    struct FZoneCellOwnershipCache
    {
        FTransform CellSpaceWorldTransform;
        TArray<FVector2D> TargetPolygonInCellSpace;
        TArray<FPreparedZoneOverlap> Candidates;

        FZoneCellOwnershipCache(
            const TMap<FName, TWeakObjectPtr<AFFZSGameplayZone>>& Zones,
            const AFFZSGameplayZone& TargetZone)
            : CellSpaceWorldTransform(TargetZone.GetActorTransform())
            , TargetPolygonInCellSpace(TargetZone.GetBoundaryPolygonLocalCm())
        {
            Candidates.Reserve(Zones.Num());
            for (const TPair<FName, TWeakObjectPtr<AFFZSGameplayZone>>& Pair : Zones)
            {
                const AFFZSGameplayZone* Candidate = Pair.Value.Get();
                if (!Candidate || Candidate == &TargetZone)
                {
                    continue;
                }

                FPreparedZoneOverlap& Prepared = Candidates.AddDefaulted_GetRef();
                Prepared.HalfVerticalExtentCm = Candidate->VerticalExtentCm * 0.5;
                Prepared.WorldTransform = Candidate->GetActorTransform();
                Prepared.PolygonInCellSpace = GetZonePolygonInCellSpace(
                    *Candidate,
                    Prepared.WorldTransform,
                    CellSpaceWorldTransform);
                for (const FVector2D& Point : Prepared.PolygonInCellSpace)
                {
                    Prepared.BoundsInCellSpace += Point;
                }
            }
        }
    };

    FVector2D ToOrientedRectLocalCm(
        const FVector2D& PointInZoneCm,
        const FFFZSOrientedRect& Rect,
        const double CellSizeCm)
    {
        const double Radians = FMath::DegreesToRadians(static_cast<double>(Rect.YawDegrees));
        const FVector2D AxisX(FMath::Cos(Radians), FMath::Sin(Radians));
        const FVector2D AxisY(-AxisX.Y, AxisX.X);
        const FVector2D Delta = PointInZoneCm - Rect.CenterCell * CellSizeCm;
        return FVector2D(
            FVector2D::DotProduct(Delta, AxisX),
            FVector2D::DotProduct(Delta, AxisY));
    }

    bool IsOrientedRectFullyInsideZone(
        const TArray<FVector2D>& Polygon,
        const FFFZSOrientedRect& Rect,
        const double CellSizeCm)
    {
        if (!Rect.IsValid())
        {
            return false;
        }
        const FFZSOrientedGeometry::FCornerArray Corners =
            FFZSOrientedGeometry::GetCorners(Rect);
        for (const FVector2D& CornerCell : Corners)
        {
            if (!IsPointInsidePolygon2D(CornerCell * CellSizeCm, Polygon, true))
            {
                return false;
            }
        }
        if (!IsPointInsidePolygon2D(Rect.CenterCell * CellSizeCm, Polygon, true))
        {
            return false;
        }

        const double HalfX = Rect.HalfExtentCells.X * CellSizeCm;
        const double HalfY = Rect.HalfExtentCells.Y * CellSizeCm;
        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            const FVector2D A = ToOrientedRectLocalCm(Polygon[Index], Rect, CellSizeCm);
            const FVector2D B = ToOrientedRectLocalCm(
                Polygon[(Index + 1) % Polygon.Num()], Rect, CellSizeCm);
            if (SegmentIntersectsOpenRect(A, B, -HalfX, -HalfY, HalfX, HalfY))
            {
                return false;
            }
        }
        return true;
    }

    bool DoesZoneOverlapOrientedRectInterior(
        const FPreparedZoneOverlap& Candidate,
        const FTransform& CellSpaceWorldTransform,
        const FFFZSOrientedRect& Rect,
        const double CellSizeCm)
    {
        if (!Rect.IsValid())
        {
            return false;
        }
        const FBox2D BoundsCell = FFZSOrientedGeometry::GetAabb(Rect);
        const double MinX = BoundsCell.Min.X * CellSizeCm;
        const double MinY = BoundsCell.Min.Y * CellSizeCm;
        const double MaxX = BoundsCell.Max.X * CellSizeCm;
        const double MaxY = BoundsCell.Max.Y * CellSizeCm;
        if (!BoundsCell.bIsValid || !Candidate.BoundsInCellSpace.bIsValid ||
            Candidate.BoundsInCellSpace.Max.X <= MinX || Candidate.BoundsInCellSpace.Min.X >= MaxX ||
            Candidate.BoundsInCellSpace.Max.Y <= MinY || Candidate.BoundsInCellSpace.Min.Y >= MaxY)
        {
            return false;
        }

        const FVector CellCenterWorld = CellSpaceWorldTransform.TransformPosition(
            FVector(Rect.CenterCell.X * CellSizeCm, Rect.CenterCell.Y * CellSizeCm, 0.0));
        if (FMath::Abs(Candidate.WorldTransform.InverseTransformPosition(CellCenterWorld).Z) >
            Candidate.HalfVerticalExtentCm)
        {
            return false;
        }

        const TArray<FVector2D>& Polygon = Candidate.PolygonInCellSpace;
        for (const FVector2D& CornerCell : FFZSOrientedGeometry::GetCorners(Rect))
        {
            if (IsPointInsidePolygon2D(CornerCell * CellSizeCm, Polygon, false))
            {
                return true;
            }
        }
        if (IsPointInsidePolygon2D(Rect.CenterCell * CellSizeCm, Polygon, false))
        {
            return true;
        }

        const double HalfX = Rect.HalfExtentCells.X * CellSizeCm;
        const double HalfY = Rect.HalfExtentCells.Y * CellSizeCm;
        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            const FVector2D A = ToOrientedRectLocalCm(Polygon[Index], Rect, CellSizeCm);
            const FVector2D B = ToOrientedRectLocalCm(
                Polygon[(Index + 1) % Polygon.Num()], Rect, CellSizeCm);
            const bool bVertexInside =
                A.X > -HalfX && A.X < HalfX && A.Y > -HalfY && A.Y < HalfY;
            if (bVertexInside ||
                SegmentIntersectsOpenRect(A, B, -HalfX, -HalfY, HalfX, HalfY))
            {
                return true;
            }
        }
        return false;
    }

    bool IsOrientedRectOwnedByExactlyOneZone(
        const FZoneCellOwnershipCache& Cache,
        const FFFZSOrientedRect& Rect,
        const double CellSizeCm)
    {
        if (!IsOrientedRectFullyInsideZone(Cache.TargetPolygonInCellSpace, Rect, CellSizeCm))
        {
            return false;
        }
        for (const FPreparedZoneOverlap& Candidate : Cache.Candidates)
        {
            if (DoesZoneOverlapOrientedRectInterior(
                Candidate, Cache.CellSpaceWorldTransform, Rect, CellSizeCm))
            {
                return false;
            }
        }
        return true;
    }

    constexpr int32 LegacySnapshotSchemaVersion = 3;
    constexpr int32 CardinalSnapshotSchemaVersion = 4;
    constexpr int32 FreeYawSnapshotSchemaVersion = 5;
    constexpr int32 InstanceUpgradeSnapshotSchemaVersion = 6;
    constexpr int32 CurrentSnapshotSchemaVersion = 7;
    constexpr int64 LegacyMeterCellScale = 100;

    // 구 Cell 좌표를 100배 할 때 오버플로를 fail-closed로 처리한다.
    bool TryScaleLegacyCellCoordinate(const int64 LegacyValue, int32& OutValue)
    {
        if (LegacyValue < MIN_int32 || LegacyValue > MAX_int32)
        {
            return false;
        }
        const int64 Scaled = LegacyValue * LegacyMeterCellScale;
        if (Scaled < MIN_int32 || Scaled > MAX_int32)
        {
            return false;
        }
        OutValue = static_cast<int32>(Scaled);
        return true;
    }

    // 구 Queue Cell을 Y/X 정렬 후 수평 span과 연속 행으로 합쳐 O(N log N)에 압축한다.
    bool TryCompressLegacyQueueCells(
        const TArray<FIntPoint>& LegacyCells,
        TArray<FFFZSCellRect>& OutRects)
    {
        OutRects.Reset();
        TArray<FIntPoint> SortedCells = LegacyCells;

        // 정렬 전에 모든 반열림 Max=(Cell+1)을 검사해 부분 결과를 남기지 않고 실패.
        for (const FIntPoint& Cell : SortedCells)
        {
            int32 ScaledValue = 0;
            if (!TryScaleLegacyCellCoordinate(Cell.X, ScaledValue) ||
                !TryScaleLegacyCellCoordinate(Cell.Y, ScaledValue) ||
                !TryScaleLegacyCellCoordinate(static_cast<int64>(Cell.X) + 1, ScaledValue) ||
                !TryScaleLegacyCellCoordinate(static_cast<int64>(Cell.Y) + 1, ScaledValue))
            {
                return false;
            }
        }

        SortedCells.Sort([](const FIntPoint& A, const FIntPoint& B)
        {
            return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
        });

        TMap<FIntPoint, int32> PreviousRowSpans;
        bool bHasPreviousRow = false;
        int32 PreviousRowY = 0;
        int32 CellIndex = 0;
        while (CellIndex < SortedCells.Num())
        {
            const int32 RowY = SortedCells[CellIndex].Y;
            if (!bHasPreviousRow || static_cast<int64>(PreviousRowY) + 1 != RowY)
            {
                PreviousRowSpans.Reset();
            }

            int32 ScaledMinY = 0;
            int32 ScaledMaxY = 0;
            // 위의 전수 검사 뒤라 실패할 수 없지만, 함수 계약은 손상 입력에 계속 fail-closed한다.
            if (!TryScaleLegacyCellCoordinate(RowY, ScaledMinY) ||
                !TryScaleLegacyCellCoordinate(static_cast<int64>(RowY) + 1, ScaledMaxY))
            {
                OutRects.Reset();
                return false;
            }

            TMap<FIntPoint, int32> CurrentRowSpans;
            while (CellIndex < SortedCells.Num() && SortedCells[CellIndex].Y == RowY)
            {
                const int32 SpanStartX = SortedCells[CellIndex].X;
                int64 SpanEndXExclusive = static_cast<int64>(SpanStartX) + 1;
                ++CellIndex;

                while (CellIndex < SortedCells.Num() && SortedCells[CellIndex].Y == RowY)
                {
                    const int32 NextX = SortedCells[CellIndex].X;
                    if (NextX < SpanEndXExclusive)
                    {
                        ++CellIndex;
                        continue;
                    }
                    if (NextX == SpanEndXExclusive)
                    {
                        ++SpanEndXExclusive;
                        ++CellIndex;
                        continue;
                    }
                    break;
                }

                int32 ScaledMinX = 0;
                int32 ScaledMaxX = 0;
                if (!TryScaleLegacyCellCoordinate(SpanStartX, ScaledMinX) ||
                    !TryScaleLegacyCellCoordinate(SpanEndXExclusive, ScaledMaxX))
                {
                    OutRects.Reset();
                    return false;
                }

                const FIntPoint SpanKey(ScaledMinX, ScaledMaxX);
                int32 RectIndex = INDEX_NONE;
                if (const int32* PreviousRectIndex = PreviousRowSpans.Find(SpanKey))
                {
                    FFFZSCellRect& PreviousRect = OutRects[*PreviousRectIndex];
                    if (PreviousRect.Max.Y == ScaledMinY)
                    {
                        PreviousRect.Max.Y = ScaledMaxY;
                        RectIndex = *PreviousRectIndex;
                    }
                }
                if (RectIndex == INDEX_NONE)
                {
                    RectIndex = OutRects.Emplace(
                        FIntPoint(ScaledMinX, ScaledMinY),
                        FIntPoint(ScaledMaxX, ScaledMaxY));
                }
                CurrentRowSpans.Add(SpanKey, RectIndex);
            }

            PreviousRowSpans = MoveTemp(CurrentRowSpans);
            PreviousRowY = RowY;
            bHasPreviousRow = true;
        }

        OutRects.Sort([](const FFFZSCellRect& A, const FFFZSCellRect& B)
        {
            if (A.Min.Y != B.Min.Y) return A.Min.Y < B.Min.Y;
            if (A.Min.X != B.Min.X) return A.Min.X < B.Min.X;
            if (A.Max.Y != B.Max.Y) return A.Max.Y < B.Max.Y;
            return A.Max.X < B.Max.X;
        });
        return true;
    }

    // Schema 3의 Cell 기반 값을 100배하고 cardinal 회전을 고정소수점 Yaw로 바꿔 Schema 5로 변환한다.
    bool TryMigrateLegacySnapshotV3(
        const FFFZSWorldSnapshot& LegacySnapshot,
        FFFZSWorldSnapshot& OutMigratedSnapshot,
        TArray<FText>& OutErrors)
    {
        if (LegacySnapshot.SchemaVersion != LegacySnapshotSchemaVersion)
        {
            OutErrors.Add(LOCTEXT("LegacySnapshotWrongSchema", "Zone/Facility 스냅샷은 SchemaVersion 3일 때만 100cm에서 1cm로 변환하는 마이그레이션 경로를 사용할 수 있습니다."));
            return false;
        }

        OutMigratedSnapshot = LegacySnapshot;
        OutMigratedSnapshot.SchemaVersion = CurrentSnapshotSchemaVersion;

        for (FFFZSFacilityInstanceSnapshot& Saved : OutMigratedSnapshot.Facilities)
        {
            FFFZSFacilitySpec& Spec = Saved.FacilitySpec;
            // Schema 3 저장본이 담고 있던 사양 형식은 언제나 1(1 Cell = 100cm)이다.
            // 이 값은 과거 저장 데이터의 고정 사실이므로 현재 스키마 상수에서 유도하지 않는다.
            constexpr int32 LegacyFacilitySpecSchemaVersion = 1;
            if (Spec.SchemaVersion != LegacyFacilitySpecSchemaVersion || !Spec.QueueReservedRects.IsEmpty())
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("LegacySnapshotAmbiguousSpec", "Schema 3 Facility {0}에는 수정되지 않은 FacilitySpec SchemaVersion 1이 있어야 하며 1cm QueueReservedRects가 없어야 합니다."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }
            if (Spec.QueueReservedCells.Num() > UFFZSDeveloperSettings::MaxLegacyQueueReservedCells)
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("LegacySnapshotQueueMigrationLimit", "Schema 3 Facility {0}의 개별 Queue Cell이 제한된 마이그레이션 경로의 허용량을 초과했습니다. 해당 저장 데이터는 오프라인에서 마이그레이션하십시오."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }

            FIntPoint MigratedFootprint;
            int32 MigratedClearance = 0;
            if (!TryScaleLegacyCellCoordinate(Spec.FootprintSizeCells.X, MigratedFootprint.X) ||
                !TryScaleLegacyCellCoordinate(Spec.FootprintSizeCells.Y, MigratedFootprint.Y) ||
                !TryScaleLegacyCellCoordinate(Spec.ClearanceCells, MigratedClearance))
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("LegacySnapshotSpecOverflow", "Schema 3 Facility {0}의 Footprint 또는 여유 공간을 변환하는 중 1cm 좌표 범위를 초과했습니다."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }

            TArray<FFFZSCellRect> MigratedQueueRects;
            if (!TryCompressLegacyQueueCells(Spec.QueueReservedCells, MigratedQueueRects))
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("LegacySnapshotQueueOverflow", "Schema 3 Facility {0}에 1cm 좌표 범위를 벗어난 Queue Cell이 있습니다."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }

            const int32 QuarterTurns = UFFZSGridLibrary::NormalizeQuarterTurns(Saved.Placement.QuarterTurns);
            if (Saved.Placement.QuarterTurns != QuarterTurns)
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("LegacySnapshotRotation", "Schema 3 Facility {0}의 QuarterTurns 값이 정규화되어 있지 않습니다."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }

            FIntPoint MigratedOrigin;
            if (!TryScaleLegacyCellCoordinate(Saved.Placement.OriginCell.X, MigratedOrigin.X) ||
                !TryScaleLegacyCellCoordinate(Saved.Placement.OriginCell.Y, MigratedOrigin.Y))
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("LegacySnapshotOriginOverflow", "Schema 3 Facility {0}의 OriginCell을 변환하는 중 1cm 좌표 범위를 초과했습니다."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }

            // q1/q2의 X, q2/q3의 Y를 +99 cm 옮겨 구 100 cm 셀의 회전 후 [Min,Max) 경계를 보존한다.
            const int64 AdjustedX = static_cast<int64>(MigratedOrigin.X) +
                ((QuarterTurns == 1 || QuarterTurns == 2) ? LegacyMeterCellScale - 1 : 0);
            const int64 AdjustedY = static_cast<int64>(MigratedOrigin.Y) +
                ((QuarterTurns == 2 || QuarterTurns == 3) ? LegacyMeterCellScale - 1 : 0);
            if (AdjustedX < MIN_int32 || AdjustedX > MAX_int32 || AdjustedY < MIN_int32 || AdjustedY > MAX_int32)
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("LegacySnapshotAdjustedOriginOverflow", "Schema 3 Facility {0}가 회전 원점 보정 후 1cm 좌표 범위를 초과했습니다."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }

            Spec.SchemaVersion = FFFZSFacilitySpec::CurrentSchemaVersion;
            Spec.FootprintSizeCells = MigratedFootprint;
            Spec.ClearanceCells = MigratedClearance;
            Spec.QueueReservedRects = MoveTemp(MigratedQueueRects);
            Spec.QueueReservedCells.Reset();
            Saved.Placement.OriginCell = FIntPoint(static_cast<int32>(AdjustedX), static_cast<int32>(AdjustedY));
            Saved.Placement.YawCentidegrees = QuarterTurns * 9000;
            Saved.Placement.QuarterTurns = 0;
        }
        return true;
    }

    bool TryMigrateCardinalSnapshotV4(
        const FFFZSWorldSnapshot& CardinalSnapshot,
        FFFZSWorldSnapshot& OutMigratedSnapshot,
        TArray<FText>& OutErrors)
    {
        if (CardinalSnapshot.SchemaVersion != CardinalSnapshotSchemaVersion)
        {
            OutErrors.Add(LOCTEXT(
                "CardinalSnapshotWrongSchema",
                "Zone/Facility 스냅샷은 SchemaVersion 4일 때만 cardinal 회전 마이그레이션을 사용할 수 있습니다."));
            return false;
        }

        OutMigratedSnapshot = CardinalSnapshot;
        OutMigratedSnapshot.SchemaVersion = CurrentSnapshotSchemaVersion;
        for (FFFZSFacilityInstanceSnapshot& Saved : OutMigratedSnapshot.Facilities)
        {
            const int32 QuarterTurns = UFFZSGridLibrary::NormalizeQuarterTurns(
                Saved.Placement.QuarterTurns);
            if (Saved.Placement.QuarterTurns != QuarterTurns)
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("CardinalSnapshotRotation", "Schema 4 Facility {0}의 QuarterTurns 값이 정규화되어 있지 않습니다."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }
            Saved.Placement.YawCentidegrees = QuarterTurns * 9000;
            Saved.Placement.QuarterTurns = 0;
        }
        return true;
    }
}

void UFFZSWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FacilitySpatialIndex = MakeShared<FFFZSSpatialHash2D>();
    CurrentPhase = GetDefault<UFFZSDeveloperSettings>()->InitialPhase;

    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
        {
            RegisterZone(*It);
        }
        for (TActorIterator<AFFZSBuildArea> It(World); It; ++It)
        {
            RegisterBuildArea(*It);
        }
        for (TActorIterator<AFFZSProtectedArea> It(World); It; ++It)
        {
            RegisterProtectedArea(*It);
        }
        for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
        {
            if (It->GetFacilityInstanceId().IsValid())
            {
                RegisterFacility(*It);
            }
        }
    }
}

void UFFZSWorldSubsystem::Deinitialize()
{
    FacilitySpatialIndex.Reset();
    RegionSpatialIndex.Reset();
    SpatialRegionIds.Reset();
    UnifiedBuildArea.Reset();
    ZonesById.Reset();
    FacilitiesById.Reset();
    OccupiedFacilityRegions.Reset();
    ProtectedAreas.Reset();
    ExternalPolicies.Reset();
    AgentQueueOwnership.Reset();
    CommandReceipts.Reset();
    CommandReceiptFacilityReferences.Reset();
    Super::Deinitialize();
}

bool UFFZSWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE ||
        WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview ||
        WorldType == EWorldType::GamePreview;
}

// 게임 스레드 밖이거나 Snapshot 복원, 외부 Policy 콜백, Command 실행 중이면 권위 변경을 막는다.
bool UFFZSWorldSubsystem::IsAuthorityMutationBlocked() const
{
    return !IsInGameThread() || bIsRestoringSnapshot || bIsInvokingExternalPolicy || bIsBroadcastingLightingChange || InFlightCommandId.IsValid();
}

bool UFFZSWorldSubsystem::SetChangePhase(const EFFZSChangePhase NewPhase)
{
    if (IsAuthorityMutationBlocked() || !IsKnownChangePhase(NewPhase))
    {
        return false;
    }
    CurrentPhase = NewPhase;
    return true;
}

bool UFFZSWorldSubsystem::RegisterZone(AFFZSGameplayZone* Zone)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    if (!IsValid(Zone) || Zone->ZoneId.IsNone() || Zone->GetWorld() != GetWorld())
    {
        return false;
    }
    TArray<FText> ZoneErrors;
    if (!Zone->ValidateZoneData(ZoneErrors))
    {
        UE_LOG(LogFFZoneSystem, Error, TEXT("잘못된 Gameplay Zone %s: %s"),
            *Zone->GetName(), ZoneErrors.IsEmpty() ? TEXT("알 수 없는 검증 오류") : *ZoneErrors[0].ToString());
        return false;
    }
    if (const TWeakObjectPtr<AFFZSGameplayZone>* Existing = ZonesById.Find(Zone->ZoneId))
    {
        if (Existing->IsValid() && Existing->Get() != Zone)
        {
            UE_LOG(LogFFZoneSystem, Error, TEXT("중복된 Gameplay Zone ID: %s"), *Zone->ZoneId.ToString());
            return false;
        }
    }
    ZonesById.Add(Zone->ZoneId, Zone);
    RebuildRegionSpatialIndex();
    RebuildBuildArea();
    RebuildFacilitySpatialIndex();
    return true;
}

bool UFFZSWorldSubsystem::UnregisterZone(AFFZSGameplayZone* Zone)
{
    if (IsAuthorityMutationBlocked() || !Zone)
    {
        return false;
    }
    if (const TWeakObjectPtr<AFFZSGameplayZone>* Existing = ZonesById.Find(Zone->ZoneId); Existing && Existing->Get() == Zone)
    {
        ZonesById.Remove(Zone->ZoneId);
        RebuildRegionSpatialIndex();
        RebuildBuildArea();
        RebuildFacilitySpatialIndex();
        return true;
    }
    return false;
}

bool UFFZSWorldSubsystem::RegisterFacility(AFFZSFacilityActor* Facility)
{
    if (!IsValid(Facility) || !Facility->GetFacilityInstanceId().IsValid() || Facility->GetZoneId().IsNone())
    {
        return false;
    }

    TArray<FText> SpecErrors;
    if (!Facility->GetFacilitySpec().IsValid(SpecErrors))
    {
        UE_LOG(LogFFZoneSystem, Error, TEXT("Facility %s 등록 실패: 1cm FacilitySpec이 잘못되었습니다: %s"),
            *Facility->GetFacilityInstanceId().ToString(),
            SpecErrors.IsEmpty() ? TEXT("알 수 없는 검증 오류") : *SpecErrors[0].ToString());
        return false;
    }

    const FGuid FacilityId = Facility->GetFacilityInstanceId();
    if (const TWeakObjectPtr<AFFZSFacilityActor>* Existing = FacilitiesById.Find(FacilityId))
    {
        if (Existing->IsValid() && Existing->Get() != Facility)
        {
            UE_LOG(LogFFZoneSystem, Error, TEXT("중복된 Facility Instance ID: %s"), *FacilityId.ToString());
            return false;
        }
        if (Existing->Get() == Facility)
        {
            Facility->SetNavigationObstacleEnabled(true);
            Facility->SetPlayerCollisionEnabled(true);
            return true;
        }
    }

    TArray<FFFZSOrientedRect> ReservedRects;
    TArray<FFFZSOrientedRect> IgnoredFootprint;
    TArray<FFFZSOrientedRect> IgnoredClearance;
    TArray<FFFZSOrientedRect> IgnoredQueue;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Facility->GetFacilitySpec(), Facility->GetGridPlacement(),
        IgnoredFootprint, IgnoredClearance, IgnoredQueue, ReservedRects);
    if (ReservedRects.IsEmpty() ||
        AreRegionsOccupiedByOther(Facility->GetZoneId(), ReservedRects, FacilityId,
            Facility->GetFacilitySpec().Category == EFFZSFacilityCategory::Road,
            Facility->GetFacilitySpec().bAllowFacilityPlacementOnTop))
    {
        UE_LOG(LogFFZoneSystem, Error, TEXT("Facility %s 등록 실패: Zone %s의 압축 영역이 잘못되었거나 다른 영역과 겹칩니다"),
            *FacilityId.ToString(), *Facility->GetZoneId().ToString());
        return false;
    }

    FacilitiesById.Add(FacilityId, Facility);
    FFFZSFacilityRegionOccupancy Occupancy;
    Occupancy.ZoneId = Facility->GetZoneId();
    Occupancy.bIsRoad = Facility->GetFacilitySpec().Category == EFFZSFacilityCategory::Road;
    Occupancy.bAllowsFacilitiesOnTop = Facility->GetFacilitySpec().bAllowFacilityPlacementOnTop;
    // 등록 검증에서 계산한 예약 영역을 재사용한다.
    Occupancy.Regions = MoveTemp(ReservedRects);
    CacheFacilityOccupancy(FacilityId, MoveTemp(Occupancy));
    if (UFFZSFacilityQueueComponent* Queue = Facility->GetMutableQueueComponent())
    {
        Queue->OnAgentReleased.AddUniqueDynamic(this, &UFFZSWorldSubsystem::HandleAgentReleased);
        for (const FFFZSQueueEntrySnapshot& Entry : Queue->CaptureSnapshot().Entries)
        {
            if (!Entry.AgentId.IsNone())
            {
                AgentQueueOwnership.Add(Entry.AgentId, FacilityId);
            }
        }
    }
    Facility->SetNavigationObstacleEnabled(true);
    Facility->SetPlayerCollisionEnabled(true);
    return true;
}

void UFFZSWorldSubsystem::UnregisterFacility(AFFZSFacilityActor* Facility)
{
    if (!Facility)
    {
        return;
    }
    const FGuid FacilityId = Facility->GetFacilityInstanceId();
    if (const TWeakObjectPtr<AFFZSFacilityActor>* Existing = FacilitiesById.Find(FacilityId); !Existing || Existing->Get() != Facility)
    {
        return;
    }
    Facility->SetNavigationObstacleEnabled(false);
    Facility->SetPlayerCollisionEnabled(false);
    ReleaseFacilityRegions(*Facility);
    FacilitiesById.Remove(FacilityId);
    if (UFFZSFacilityQueueComponent* Queue = Facility->GetMutableQueueComponent())
    {
        Queue->OnAgentReleased.RemoveDynamic(this, &UFFZSWorldSubsystem::HandleAgentReleased);
        for (const FFFZSQueueEntrySnapshot& Entry : Queue->CaptureSnapshot().Entries)
        {
            if (const FGuid* Owner = AgentQueueOwnership.Find(Entry.AgentId); Owner && *Owner == FacilityId)
            {
                AgentQueueOwnership.Remove(Entry.AgentId);
            }
        }
    }
}

bool UFFZSWorldSubsystem::RegisterProtectedArea(AFFZSProtectedArea* ProtectedArea)
{
    if (IsAuthorityMutationBlocked() || !IsValid(ProtectedArea))
    {
        return false;
    }

    TArray<FText> Errors;
    if (!ProtectedArea->ValidateProtectedAreaData(Errors))
    {
        UE_LOG(LogFFZoneSystem, Error, TEXT("잘못된 Protected Area %s: %s"),
            *ProtectedArea->GetName(), Errors.IsEmpty() ? TEXT("알 수 없는 검증 오류") : *Errors[0].ToString());
        return false;
    }
    for (const TWeakObjectPtr<AFFZSProtectedArea>& Existing : ProtectedAreas)
    {
        if (const AFFZSProtectedArea* ExistingArea = Existing.Get();
            ExistingArea && ExistingArea != ProtectedArea && ExistingArea->ProtectedAreaId == ProtectedArea->ProtectedAreaId)
        {
            UE_LOG(LogFFZoneSystem, Error, TEXT("중복된 Protected Area ID: %s"), *ProtectedArea->ProtectedAreaId.ToString());
            return false;
        }
    }
    ProtectedAreas.AddUnique(ProtectedArea);
    return true;
}

bool UFFZSWorldSubsystem::UnregisterProtectedArea(AFFZSProtectedArea* ProtectedArea)
{
    if (IsAuthorityMutationBlocked() || !ProtectedArea)
    {
        return false;
    }
    return ProtectedAreas.Remove(ProtectedArea) > 0;
}

AFFZSGameplayZone* UFFZSWorldSubsystem::FindZone(const FName ZoneId) const
{
    if (const TWeakObjectPtr<AFFZSGameplayZone>* Found = ZonesById.Find(ZoneId))
    {
        return Found->Get();
    }
    return nullptr;
}

bool UFFZSWorldSubsystem::HasRegisteredZones() const
{
    for (const TPair<FName, TWeakObjectPtr<AFFZSGameplayZone>>& Pair : ZonesById)
    {
        if (Pair.Value.IsValid())
        {
            return true;
        }
    }
    return false;
}

AFFZSFacilityActor* UFFZSWorldSubsystem::FindFacility(const FGuid FacilityInstanceId) const
{
    if (const TWeakObjectPtr<AFFZSFacilityActor>* Found = FacilitiesById.Find(FacilityInstanceId))
    {
        return Found->Get();
    }
    return nullptr;
}

int32 UFFZSWorldSubsystem::CountFacilitiesByDefinitionId(
    const FName FacilityDefinitionId) const
{
    if (FacilityDefinitionId.IsNone())
    {
        return 0;
    }

    int32 Count = 0;
    for (const TPair<FGuid, TWeakObjectPtr<AFFZSFacilityActor>>& Pair : FacilitiesById)
    {
        const AFFZSFacilityActor* Facility = Pair.Value.Get();
        if (IsValid(Facility) &&
            Facility->GetFacilityState() != EFFZSFacilityState::Removed &&
            Facility->GetFacilitySpec().FacilityDefinitionId == FacilityDefinitionId)
        {
            ++Count;
        }
    }
    return Count;
}

bool UFFZSWorldSubsystem::RegisterBuildArea(AFFZSBuildArea* BuildArea)
{
    if (IsAuthorityMutationBlocked() || !IsValid(BuildArea) || BuildArea->GetWorld() != GetWorld())
    {
        return false;
    }
    if (UnifiedBuildArea.IsValid() && UnifiedBuildArea.Get() != BuildArea)
    {
        UE_LOG(LogFFZoneSystem, Error, TEXT("Only one unified construction area may be registered per world."));
        return false;
    }
    UnifiedBuildArea = BuildArea;
    RebuildBuildArea();
    return true;
}

bool UFFZSWorldSubsystem::UnregisterBuildArea(AFFZSBuildArea* BuildArea)
{
    if (IsAuthorityMutationBlocked() || UnifiedBuildArea.Get() != BuildArea)
    {
        return false;
    }
    UnifiedBuildArea.Reset();
    return true;
}

AFFZSBuildArea* UFFZSWorldSubsystem::FindBuildArea() const
{
    return UnifiedBuildArea.Get();
}

bool UFFZSWorldSubsystem::RefreshBuildArea()
{
    if (IsAuthorityMutationBlocked() || !FindBuildArea())
    {
        return false;
    }
    RebuildBuildArea();
    RebuildRegionSpatialIndex();
    RebuildFacilitySpatialIndex();
    return FindBuildArea()->IsBuildAreaReady();
}

void UFFZSWorldSubsystem::RebuildBuildArea()
{
    if (AFFZSBuildArea* Area = FindBuildArea())
    {
        TArray<AFFZSGameplayZone*> Regions;
        for (const FName Id : GetRegisteredZoneIds())
        {
            Regions.Add(FindZone(Id));
        }
        Area->RebuildFromRegions(Regions);
    }
}

TArray<FFFZSOrientedRect> UFFZSWorldSubsystem::ToWorldRegions(
    const FName ZoneId, const TArray<FFFZSOrientedRect>& Regions) const
{
    TArray<FFFZSOrientedRect> Result;
    if (const AFFZSGameplayZone* Zone = FindZone(ZoneId))
    {
        Result.Reserve(Regions.Num());
        for (const FFFZSOrientedRect& Region : Regions)
        {
            Result.Add(TransformRegion(Region, Zone->GetActorTransform()));
        }
    }
    return Result;
}

bool UFFZSWorldSubsystem::IsOccupancyRelevant(
    const FName ZoneId, const FFFZSFacilityRegionOccupancy& Occupancy) const
{
    if (Occupancy.ZoneId == ZoneId)
    {
        return true;
    }
    if (!FindBuildArea())
    {
        return false;
    }
    const AFFZSGameplayZone* A = FindZone(ZoneId);
    const AFFZSGameplayZone* B = FindZone(Occupancy.ZoneId);
    // An unregistered region does not remove the physical facilities that still occupy it.
    return !A || !B || FMath::Abs(A->GetActorLocation().Z - B->GetActorLocation().Z) <
        (A->VerticalExtentCm + B->VerticalExtentCm) * 0.5;
}

void UFFZSWorldSubsystem::CacheFacilityOccupancy(
    const FGuid FacilityId, FFFZSFacilityRegionOccupancy&& Occupancy)
{
    if (!FacilitySpatialIndex)
    {
        FacilitySpatialIndex = MakeShared<FFFZSSpatialHash2D>();
    }
    Occupancy.WorldRegions = ToWorldRegions(Occupancy.ZoneId, Occupancy.Regions);
    Occupancy.WorldBounds = GetRegionBounds(Occupancy.WorldRegions);
    FacilitySpatialIndex->InsertOrUpdate(FacilityId, Occupancy.WorldBounds);
    OccupiedFacilityRegions.Add(FacilityId, MoveTemp(Occupancy));
}

void UFFZSWorldSubsystem::RebuildFacilitySpatialIndex()
{
    if (!FacilitySpatialIndex)
    {
        FacilitySpatialIndex = MakeShared<FFFZSSpatialHash2D>();
    }
    FacilitySpatialIndex->Reset();
    for (TPair<FGuid, FFFZSFacilityRegionOccupancy>& Pair : OccupiedFacilityRegions)
    {
        if (FindZone(Pair.Value.ZoneId))
        {
            Pair.Value.WorldRegions = ToWorldRegions(Pair.Value.ZoneId, Pair.Value.Regions);
            Pair.Value.WorldBounds = GetRegionBounds(Pair.Value.WorldRegions);
        }
        FacilitySpatialIndex->InsertOrUpdate(Pair.Key, Pair.Value.WorldBounds);
    }
}

void UFFZSWorldSubsystem::RebuildRegionSpatialIndex()
{
    RegionSpatialIndex = MakeShared<FFFZSSpatialHash2D>(10000.0);
    SpatialRegionIds = GetRegisteredZoneIds();
    for (int32 I = 0; I < SpatialRegionIds.Num(); ++I)
    {
        const AFFZSGameplayZone* Region = FindZone(SpatialRegionIds[I]);
        FBox2D Bounds(ForceInit);
        for (const FVector2D& Point : Region->GetBoundaryPolygonLocalCm())
        {
            const FVector World = Region->GetActorTransform().TransformPosition(FVector(Point, 0.0));
            Bounds += FVector2D(World.X, World.Y);
        }
        // Transient ordinal IDs preserve the existing lexical region tie-break after querying.
        RegionSpatialIndex->InsertOrUpdate(FGuid(0, 0, 0, static_cast<uint32>(I + 1)), Bounds);
    }
}

void UFFZSWorldSubsystem::QueryFacilityCandidates(
    const TArray<FFFZSOrientedRect>& WorldRegions, TArray<FGuid>& Candidates) const
{
    if (FacilitySpatialIndex)
    {
        FacilitySpatialIndex->Query(GetRegionBounds(WorldRegions), Candidates);
    }
    else
    {
        OccupiedFacilityRegions.GenerateKeyArray(Candidates);
        Candidates.Sort();
    }
}

AFFZSFacilityActor* UFFZSWorldSubsystem::FindFacilityAtWorldPoint(
    const FVector& WorldPoint,
    const FName ZoneId) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FFZS_FindFacilityAtPoint);
    const FName TargetZoneId = ZoneId.IsNone() && !FindBuildArea()
        ? ResolveZoneAtWorldLocation(WorldPoint)
        : ZoneId;
    if (TargetZoneId.IsNone() && !FindBuildArea())
    {
        return nullptr;
    }

    AFFZSFacilityActor* BestMatch = nullptr;
    double BestDistanceSquared = TNumericLimits<double>::Max();
    FString BestFacilityIdKey;
    TArray<FGuid> Candidates;
    if (FindBuildArea() && FacilitySpatialIndex)
    {
        const FVector2D Point(WorldPoint.X, WorldPoint.Y);
        // Narrow-phase footprint selection allows 2 cm along each rotated axis.
        FacilitySpatialIndex->Query(FBox2D(Point - FVector2D(3.0), Point + FVector2D(3.0)), Candidates);
    }
    else
    {
        FacilitiesById.GenerateKeyArray(Candidates);
    }
    for (const FGuid Id : Candidates)
    {
        AFFZSFacilityActor* Facility = FindFacility(Id);
        if (!IsValid(Facility) || !Id.IsValid() ||
            Facility->GetFacilityInstanceId() != Id ||
            (!TargetZoneId.IsNone() && Facility->GetZoneId() != TargetZoneId) ||
            !Facility->IsWorldPointInsideFootprint(WorldPoint))
        {
            continue;
        }

        const double DistanceSquared = FVector::DistSquared(
            Facility->GetActorLocation(), WorldPoint);
        const FString FacilityIdKey = Id.ToString(EGuidFormats::Digits);
        if (!BestMatch || DistanceSquared < BestDistanceSquared ||
            (DistanceSquared == BestDistanceSquared && FacilityIdKey < BestFacilityIdKey))
        {
            BestMatch = Facility;
            BestDistanceSquared = DistanceSquared;
            BestFacilityIdKey = FacilityIdKey;
        }
    }
    return BestMatch;
}

bool UFFZSWorldSubsystem::BuildPlacementGridMask(
    const FFFZSCommand& Command,
    const FVector2D& MinLocalCm,
    const FVector2D& MaxLocalCm,
    const int32 GridStepCm,
    const double GroundWorldZ,
    FFFZSBuildableGridMask& OutMask) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FFZS_BuildPlacementGridMask);
    OutMask.Reset();
    if (!IsInGameThread() || !FMath::IsFinite(GroundWorldZ))
    {
        return false;
    }
    const AFFZSGameplayZone* Zone = FindZone(Command.ZoneId);
    if (!Zone)
    {
        return false;
    }
    TArray<FText> Errors;
    const AFFZSBuildArea* BuildArea = FindBuildArea();
    const FVector LocalWorldOrigin = Zone->GetActorTransform().InverseTransformPosition(FVector::ZeroVector);
    if (!Zone->ValidateZoneData(Errors) ||
        !OutMask.Initialize(MinLocalCm, MaxLocalCm, GridStepCm,
            BuildArea ? FVector2D(LocalWorldOrigin.X, LocalWorldOrigin.Y) : FVector2D::ZeroVector))
    {
        return false;
    }

    const AFFZSFacilityActor* Moving = Command.Type == EFFZSCommandType::Move
        ? FindFacility(Command.FacilityInstanceId) : nullptr;
    const FFFZSFacilitySpec& Spec = Moving ? Moving->GetFacilitySpec() : Command.FacilitySpec;
    const bool bHasSelection = !Spec.FacilityDefinitionId.IsNone();
    OutMask.bIncludeRoadSupport = Spec.Category != EFFZSFacilityCategory::Road;
    if (!bHasSelection && !Zone->bAllowAllFacilityCategories && Zone->AllowedFacilityCategories.IsEmpty())
    {
        return true;
    }
    if (bHasSelection && (!UFFZSGridLibrary::HasSafeFacilitySpecCardinality(Spec) ||
        !Spec.IsValid(Errors) || !Zone->IsCategoryAllowed(Spec.Category) ||
        (!Spec.AllowedZoneIds.IsEmpty() && !Spec.AllowedZoneIds.Contains(Command.ZoneId))))
    {
        return true; // Well-formed empty mask for a selection disallowed in this zone.
    }

    // Reuse the authority's concave-polygon/exactly-one-zone test. Preparation happens once,
    // not once per cell, and never materializes the world's physical 1 cm occupancy lattice.
    TUniquePtr<FZoneCellOwnershipCache> Ownership;
    if (!BuildArea)
    {
        Ownership = MakeUnique<FZoneCellOwnershipCache>(ZonesById, *Zone);
    }
    const FTransform ZoneTransform = Zone->GetActorTransform();
    const double Step = OutMask.GetStepCm();
    const FVector2D MaskMin = OutMask.GetMinLocalCm();
    const FVector2D MaskMax = MaskMin + FVector2D(OutMask.GetNumX(), OutMask.GetNumY()) * Step;
    FFFZSOrientedRect ViewRect;
    ViewRect.CenterCell = (MaskMin + MaskMax) * 0.5;
    ViewRect.HalfExtentCells = (MaskMax - MaskMin) * 0.5;
    TArray<const FFFZSOrientedRect*> OccupiedRegions;
    TArray<FFFZSOrientedRect> ExistingStructureFootprints;
    const FFFZSOrientedRect WorldViewRect = TransformRegion(ViewRect, ZoneTransform);
    TArray<FGuid> Candidates;
    QueryFacilityCandidates({WorldViewRect}, Candidates);
    for (const FGuid Id : Candidates)
    {
        const FFFZSFacilityRegionOccupancy* Occupancy = OccupiedFacilityRegions.Find(Id);
        if (!Occupancy || !IsOccupancyRelevant(Command.ZoneId, *Occupancy) ||
            (Moving && Id == Moving->GetFacilityInstanceId()))
        {
            continue;
        }
        if (Spec.Category == EFFZSFacilityCategory::Road && Spec.bAllowFacilityPlacementOnTop && !Occupancy->bIsRoad)
        {
            if (const AFFZSFacilityActor* Existing = FindFacility(Id))
            {
                TArray<FFFZSOrientedRect> Footprint, Clearance, Queue, Reserved;
                UFFZSGridLibrary::BuildPlacementOrientedRegions(Existing->GetFacilitySpec(), Existing->GetGridPlacement(),
                    Footprint, Clearance, Queue, Reserved);
                ExistingStructureFootprints.Append(ToWorldRegions(Existing->GetZoneId(), Footprint));
            }
            continue;
        }
        if (!Occupancy->BlocksLayer(Spec.Category == EFFZSFacilityCategory::Road, Spec.bAllowFacilityPlacementOnTop)) continue;
        for (const FFFZSOrientedRect& Region : Occupancy->WorldRegions)
        {
            if (FFZSOrientedGeometry::IntersectsPositiveArea(WorldViewRect, Region))
            {
                OccupiedRegions.Add(&Region);
            }
        }
    }

    for (const FFFZSOrientedRect& Footprint : ExistingStructureFootprints)
    {
        OccupiedRegions.Add(&Footprint);
    }

    TArray<const AFFZSProtectedArea*> RelevantAreas;
    for (const TWeakObjectPtr<AFFZSProtectedArea>& WeakArea : ProtectedAreas)
    {
        const AFFZSProtectedArea* Area = WeakArea.Get();
        if (!Area)
        {
            continue;
        }
        // Validate even another zone's malformed protection, matching ValidateProtectedAreas.
        if (!Area->ValidateProtectedAreaData(Errors) ||
            (!Area->LinkedZoneId.IsNone() && !FindZone(Area->LinkedZoneId)))
        {
            return true;
        }
        if (BuildArea || Area->LinkedZoneId.IsNone() || Area->LinkedZoneId == Command.ZoneId)
        {
            RelevantAreas.Add(Area);
        }
    }

    const double HeightCm = bHasSelection ? Spec.PlacementObstacleHeightCm : 250.0;
    const double ProtectedHalfHeight = HeightCm * 0.5;
    const double CollisionHalfHeight = FMath::Max(5.0, HeightCm * 0.5 - 2.0);
    const double CollisionCenterZ = GroundWorldZ + CollisionHalfHeight + 3.0;
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const bool bDeferToSurface = Settings->bEnableGroundChecks;
    OutMask.ObstacleHeightCm = HeightCm;
    TBitArray<> WorldBlockedCells(false, OutMask.GetCellCount());
    const auto RasterizeWorldBounds = [&](const FBox& Bounds,
        const UPrimitiveComponent* Component, const int32 InstanceIndex = INDEX_NONE)
    {
        if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN())
        {
            return false;
        }
        if (!bDeferToSurface &&
            (Bounds.Max.Z <= GroundWorldZ + 3.0 || Bounds.Min.Z > CollisionCenterZ + CollisionHalfHeight))
        {
            return true;
        }
        FBox2D LocalBounds(EForceInit::ForceInit);
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            const FVector Local = ZoneTransform.InverseTransformPosition(FVector(
                (Corner & 1) ? Bounds.Max.X : Bounds.Min.X,
                (Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y, GroundWorldZ));
            LocalBounds += FVector2D(Local.X, Local.Y);
        }
        const int32 FirstX = static_cast<int32>(FMath::Clamp(
            FMath::FloorToDouble((LocalBounds.Min.X - MaskMin.X) / Step), 0.0, static_cast<double>(OutMask.GetNumX())));
        const int32 FirstY = static_cast<int32>(FMath::Clamp(
            FMath::FloorToDouble((LocalBounds.Min.Y - MaskMin.Y) / Step), 0.0, static_cast<double>(OutMask.GetNumY())));
        const int32 LastX = static_cast<int32>(FMath::Clamp(
            FMath::CeilToDouble((LocalBounds.Max.X - MaskMin.X) / Step), 0.0, static_cast<double>(OutMask.GetNumX())));
        const int32 LastY = static_cast<int32>(FMath::Clamp(
            FMath::CeilToDouble((LocalBounds.Max.Y - MaskMin.Y) / Step), 0.0, static_cast<double>(OutMask.GetNumY())));
        if (bDeferToSurface)
        {
            FFFZSBuildGridObstacle& Obstacle = OutMask.DeferredWorldObstacles.AddDefaulted_GetRef();
            Obstacle.WorldBounds = Bounds;
            Obstacle.FirstCell = FIntPoint(FirstX, FirstY);
            Obstacle.EndCell = FIntPoint(LastX, LastY);
            Obstacle.Component = const_cast<UPrimitiveComponent*>(Component);
            Obstacle.InstanceIndex = InstanceIndex;
            return true;
        }
        for (int32 Y = FirstY; Y < LastY; ++Y)
        {
            WorldBlockedCells.SetRange(Y * OutMask.GetNumX() + FirstX, LastX - FirstX, true);
        }
        return true;
    };
    if (Settings->bEnableWorldCollisionChecks && GetWorld())
    {
        FCollisionQueryParams Params(SCENE_QUERY_STAT(FFZoneSystemBuildGrid), false);
        Params.AddIgnoredActor(Zone);
        if (Moving)
        {
            Params.AddIgnoredActor(Moving);
        }
        FVector QueryCenter = ZoneTransform.TransformPosition(FVector(ViewRect.CenterCell, 0.0));
        const double QueryMinZ = bDeferToSurface ? ZoneTransform.GetLocation().Z -
            FMath::Max(1.0, static_cast<double>(Settings->GroundTraceBelowCm)) : GroundWorldZ + 3.0;
        const double QueryMaxZ = bDeferToSurface ? ZoneTransform.GetLocation().Z +
            FMath::Max(1.0, static_cast<double>(Settings->GroundTraceAboveCm)) + HeightCm :
            CollisionCenterZ + CollisionHalfHeight;
        QueryCenter.Z = (QueryMinZ + QueryMaxZ) * 0.5;
        TArray<FOverlapResult> Overlaps;
        GetWorld()->OverlapMultiByChannel(Overlaps, QueryCenter, ZoneTransform.GetRotation(),
            ECC_Visibility, FCollisionShape::MakeBox(FVector(
                ViewRect.HalfExtentCells, (QueryMaxZ - QueryMinZ) * 0.5)), Params);
        TSet<const UPrimitiveComponent*> SeenComponents;
        for (const FOverlapResult& Overlap : Overlaps)
        {
            const AActor* Actor = Overlap.GetActor();
            const UPrimitiveComponent* Component = Overlap.GetComponent();
            if (!Actor || !Component || Actor->IsA<AFFZSGameplayZone>() ||
                Actor->IsA<AFFZSProtectedArea>() || Actor->IsA<AFFZSFacilityActor>() ||
                Component->IsA<ULandscapeHeightfieldCollisionComponent>() ||
                Component->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Ignore ||
                SeenComponents.Contains(Component))
            {
                continue;
            }
            SeenComponents.Add(Component);
            // A foliage/ISM component can span an entire map. Use only nearby instances,
            // otherwise one tree would incorrectly turn the whole component's bounds dark.
            if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
            {
                const UStaticMesh* Mesh = Instances->GetStaticMesh();
                if (!Mesh)
                {
                    continue;
                }
                const FBox QueryLocalBounds(
                    FVector(MaskMin, 0.0), FVector(MaskMax, CollisionHalfHeight * 2.0));
                FBox QueryWorldBounds = QueryLocalBounds.TransformBy(ZoneTransform);
                QueryWorldBounds.Min.Z = QueryMinZ;
                QueryWorldBounds.Max.Z = QueryMaxZ;
                for (const int32 InstanceIndex : Instances->GetInstancesOverlappingBox(QueryWorldBounds, true))
                {
                    FTransform InstanceTransform;
                    if (!Instances->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
                    {
                        continue;
                    }
                    FBox InstanceBounds = Mesh->GetBoundingBox().TransformBy(InstanceTransform);
                    if (const UBodySetup* Body = Mesh->GetBodySetup())
                    {
                        const FBox CollisionBounds = Body->AggGeom.CalcAABB(InstanceTransform);
                        if (CollisionBounds.IsValid)
                        {
                            InstanceBounds += CollisionBounds;
                        }
                    }
                    if (!RasterizeWorldBounds(InstanceBounds, Component, InstanceIndex))
                    {
                        return true;
                    }
                }
            }
            else if (!RasterizeWorldBounds(Component->Bounds.GetBox(), Component))
            {
                return true;
            }
        }
    }

    FFFZSOrientedRect CellRect;
    CellRect.HalfExtentCells = FVector2D(Step * 0.5);
    const FVector ProtectedHalfExtent(Step * 0.5, Step * 0.5, ProtectedHalfHeight);
    for (int32 Y = 0; Y < OutMask.GetNumY(); ++Y)
    {
        for (int32 X = 0; X < OutMask.GetNumX(); ++X)
        {
            CellRect.CenterCell = MaskMin + FVector2D(X + 0.5, Y + 0.5) * Step;
            const FFFZSOrientedRect WorldCell = TransformRegion(CellRect, ZoneTransform);
            const bool bInside = BuildArea
                ? BuildArea->ContainsWorldOrientedRect(WorldCell, ZoneTransform.GetLocation().Z)
                : IsOrientedRectOwnedByExactlyOneZone(*Ownership, CellRect,
                    UFFZSDeveloperSettings::DefaultPlacementCellSizeCm);
            if (WorldBlockedCells[Y * OutMask.GetNumX() + X] || !bInside)
            {
                continue;
            }
            bool bBlocked = false;
            for (const FFFZSOrientedRect* Occupied : OccupiedRegions)
            {
                if (FFZSOrientedGeometry::IntersectsPositiveArea(WorldCell, *Occupied))
                {
                    bBlocked = true;
                    break;
                }
            }
            if (bBlocked)
            {
                continue;
            }
            FVector WorldCenter = ZoneTransform.TransformPosition(FVector(CellRect.CenterCell, 0.0));
            // Authority checks protection on the zone plane before its ground-height pass.
            // A camera ray landing on a roof must not reveal protected ground below it.
            WorldCenter.Z += ProtectedHalfHeight;
            for (const AFFZSProtectedArea* Area : RelevantAreas)
            {
                if (Area->IntersectsWorldOrientedBox(WorldCenter, ProtectedHalfExtent, ZoneTransform.GetRotation()))
                {
                    bBlocked = true;
                    break;
                }
            }
            if (bBlocked)
            {
                continue;
            }
            OutMask.SetBuildable(X, Y, !bBlocked);
        }
    }
    return true;
}

// Move는 등록된 Spec으로 검증하며, 이 단계에서는 권위 상태를 변경하지 않는다.
FFFZSPlacementValidation UFFZSWorldSubsystem::ValidatePlacement(const FFFZSCommand& Command) const
{
    const AFFZSFacilityActor* Existing = Command.Type == EFFZSCommandType::Move ? FindFacility(Command.FacilityInstanceId) : nullptr;
    FFFZSCommand EffectiveCommand = Command;
    if (Existing)
    {
        EffectiveCommand.FacilitySpec = Existing->GetFacilitySpec();
    }

    FFFZSPlacementValidation Result = ValidatePlacementLocal(EffectiveCommand, Existing);
    if (Result.bIsValid && Command.Type == EFFZSCommandType::Move && Existing)
    {
        const UFFZSFacilityQueueComponent* Queue = Existing->GetQueueComponent();
        if (Queue && Queue->IsOccupied())
        {
            Result.AddFailure(
                EFFZSPlacementFailure::FacilityOccupied,
                LOCTEXT("MovePreviewOccupied", "Facility를 이동하기 전에 닫고 Queue를 비우십시오."));
        }
        else if (DoesRoadSupportFacility(*Existing))
        {
            Result.AddFailure(EFFZSPlacementFailure::FacilityOccupied,
                LOCTEXT("RoadMoveSupportsFacility", "길 위의 시설을 먼저 옮긴 후 길을 이동할 수 있습니다."));
        }
        else if (!Existing->CanMoveOrRemove())
        {
            Result.AddFailure(
                EFFZSPlacementFailure::InvalidFacilityState,
                LOCTEXT("MovePreviewState", "Active 상태의 Facility는 이동하기 전에 닫아야 합니다."));
        }
    }
    if (Result.bIsValid)
    {
        FFFZSPolicyContext Context;
        Context.Command = EffectiveCommand;
        Context.Validation = Result;
        Context.FacilityActor = const_cast<AFFZSFacilityActor*>(Existing);
        Context.CurrentWorldRevision = WorldRevision;
        FFFZSPolicyDecision Decision;
        if (!RunPolicyPreValidation(Context, Decision))
        {
            Result.AddFailure(
                Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason,
                Decision.Message.IsEmpty() ? LOCTEXT("PolicyRejectedPlacement", "외부 Policy가 이 배치를 거부했습니다.") : Decision.Message);
        }
    }
    return Result;
}

FFFZSPlacementValidation UFFZSWorldSubsystem::ValidateUpgradePlacement(
    const FGuid FacilityInstanceId, const float SizeMultiplier) const
{
    const AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    if (!Facility)
    {
        FFFZSPlacementValidation Result;
        Result.AddFailure(EFFZSPlacementFailure::FacilityNotFound,
            LOCTEXT("UpgradePlacementMissing", "강화할 시설의 현재 배치를 확인할 수 없습니다."));
        return Result;
    }
    const FFFZSFacilityUpgradeTier* NextTier = FFZSFacilityUpgradeRules::FindNextTier(
        Facility->GetBaseFacilitySpec().UpgradeTiers, Facility->GetUpgradeLevel());
    if (!NextTier)
    {
        FFFZSPlacementValidation Result;
        Result.AddFailure(EFFZSPlacementFailure::DataInvalid,
            LOCTEXT("UpgradePlacementNoNextLevel", "현재 레벨보다 큰 강화 단계가 설정되어 있지 않습니다."));
        return Result;
    }
    FFFZSCommand Candidate;
    Candidate.Type = EFFZSCommandType::Move;
    Candidate.FacilityInstanceId = FacilityInstanceId;
    Candidate.FacilitySpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
        Facility->GetBaseFacilitySpec(), NextTier->TargetLevel, SizeMultiplier);
    Candidate.ZoneId = Facility->GetZoneId();
    Candidate.Placement = Facility->GetGridPlacement();
    return ValidatePlacementLocal(Candidate, Facility);
}

FFFZSPlacementValidation UFFZSWorldSubsystem::ValidatePlacementLocal(const FFFZSCommand& Command, const AFFZSFacilityActor* IgnoreFacility, const bool bRevalidateExisting) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FFZS_ValidatePlacement);
    FFFZSPlacementValidation Result;

    if (Command.Type != EFFZSCommandType::Install && Command.Type != EFFZSCommandType::Move)
    {
        Result.AddFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("NotPlacementCommand", "Install 및 Move 명령만 배치 미리보기를 지원합니다."));
        return Result;
    }
    if (!IsWorldChangePhase(CurrentPhase))
    {
        Result.AddFailure(EFFZSPlacementFailure::WrongStage, LOCTEXT("WrongPhase", "Install, Move 및 Remove는 InitialSetup 또는 MorningCheck Phase에서만 허용됩니다."));
    }
    if (!Command.FacilityInstanceId.IsValid())
    {
        Result.AddFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("MissingInstance", "FacilityInstanceId가 필요합니다."));
    }
    if (Command.Type == EFFZSCommandType::Install && FindFacility(Command.FacilityInstanceId))
    {
        Result.AddFailure(EFFZSPlacementFailure::DuplicateFacilityId, LOCTEXT("DuplicateFacility", "FacilityInstanceId가 이미 존재합니다."));
    }
    if (Command.Type == EFFZSCommandType::Move && !IgnoreFacility)
    {
        Result.AddFailure(EFFZSPlacementFailure::FacilityNotFound, LOCTEXT("MoveMissing", "이동할 Facility가 존재하지 않습니다."));
    }
    TArray<FText> SpecErrors;
    if (!Command.FacilitySpec.IsValid(SpecErrors))
    {
        Result.AddFailure(EFFZSPlacementFailure::DataInvalid, SpecErrors.IsEmpty() ? LOCTEXT("InvalidSpec", "Facility 데이터가 잘못되었습니다.") : SpecErrors[0]);
        return Result;
    }

    bool bBuildLimitReached = false;
    if (Command.Type == EFFZSCommandType::Install &&
        Command.FacilitySpec.MaxInstancesPerWorld > 0)
    {
        const int32 InstalledCount = CountFacilitiesByDefinitionId(
            Command.FacilitySpec.FacilityDefinitionId);
        if (InstalledCount >= Command.FacilitySpec.MaxInstancesPerWorld)
        {
            bBuildLimitReached = true;
            Result.AddFailure(
                EFFZSPlacementFailure::BuildLimitReached,
                FText::Format(
                    LOCTEXT(
                        "FacilityBuildLimitReached",
                        "이 시설은 최대 {0}개까지 건축할 수 있으며, 현재 {1}개가 설치되어 있습니다."),
                    FText::AsNumber(Command.FacilitySpec.MaxInstancesPerWorld),
                    FText::AsNumber(InstalledCount)));
        }
    }

    AFFZSGameplayZone* Zone = FindZone(Command.ZoneId);
    if (!Zone)
    {
        Result.AddFailure(EFFZSPlacementFailure::ZoneNotFound, LOCTEXT("MissingZone", "대상 Gameplay Zone이 존재하지 않습니다."));
        return Result;
    }
    TArray<FText> ZoneErrors;
    if (!Zone->ValidateZoneData(ZoneErrors))
    {
        Result.AddFailure(EFFZSPlacementFailure::DataInvalid,
            ZoneErrors.IsEmpty() ? LOCTEXT("InvalidZoneData", "대상 Gameplay Zone 데이터가 잘못되었습니다.") : ZoneErrors[0]);
        return Result;
    }

    if (!Zone->IsCategoryAllowed(Command.FacilitySpec.Category) ||
        (!Command.FacilitySpec.AllowedZoneIds.IsEmpty() && !Command.FacilitySpec.AllowedZoneIds.Contains(Command.ZoneId)))
    {
        Result.AddFailure(EFFZSPlacementFailure::CategoryNotAllowed, LOCTEXT("CategoryDenied", "대상 Gameplay Zone에서는 이 Facility가 허용되지 않습니다."));
    }

    FFFZSGridPlacement Placement = Command.Placement;
    UFFZSGridLibrary::NormalizePlacement(Placement);

    // 판정은 압축 Rect를 사용하고 디버그 셀만 제한적으로 materialize한다.
    // [최적화] 네 영역을 한 번의 기하 계산으로 함께 만든다. 개별 Get*Rects를 네 번 부르면
    // 예산 검사와 Queue 정규화가 호출마다 중복 실행됐다. 이 함수는 Preview Actor가 0.1초마다
    // 호출하는 경로라 중복 계산이 그대로 프레임 비용이 된다. 결과 값은 이전과 동일하다.
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Command.FacilitySpec,
        Placement,
        Result.FootprintOrientedRects,
        Result.ClearanceOrientedRects,
        Result.QueueOrientedRects,
        Result.ReservedOrientedRects);
    const auto MakeConservative = [](const TArray<FFFZSOrientedRect>& Oriented)
    {
        TArray<FFFZSCellRect> Bounds;
        Bounds.Reserve(Oriented.Num());
        for (const FFFZSOrientedRect& Rect : Oriented)
        {
            const FFFZSCellRect Bound = FFZSOrientedGeometry::ToConservativeCellRect(Rect);
            if (!Bound.IsValid())
            {
                return TArray<FFFZSCellRect>{};
            }
            Bounds.Add(Bound);
        }
        return UFFZSGridLibrary::CanonicalizeCellRects(Bounds);
    };
    Result.FootprintRects = MakeConservative(Result.FootprintOrientedRects);
    Result.ClearanceRects = MakeConservative(Result.ClearanceOrientedRects);
    Result.QueueRects = MakeConservative(Result.QueueOrientedRects);
    Result.ReservedRects = MakeConservative(Result.ReservedOrientedRects);
    UFFZSGridLibrary::TryMaterializeCellRects(Result.FootprintRects,
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result.FootprintCells);
    UFFZSGridLibrary::TryMaterializeCellRects(Result.ClearanceRects,
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result.ClearanceCells);
    UFFZSGridLibrary::TryMaterializeCellRects(Result.QueueRects,
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result.QueueCells);
    UFFZSGridLibrary::TryMaterializeCellRects(Result.ReservedRects,
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result.ReservedCells);
    Result.PlacementTransform = MakePlacementTransform(
        *Zone, Placement, Result.FootprintOrientedRects);

    if (Result.FootprintOrientedRects.IsEmpty() || Result.ReservedOrientedRects.IsEmpty())
    {
        Result.AddFailure(EFFZSPlacementFailure::DataInvalid,
            LOCTEXT("EmptyCompressedPlacement", "Facility 데이터에서 유효한 압축 1cm 배치 영역을 만들 수 없습니다."));
        return Result;
    }

    // 한도 실패도 PreviewActor가 정상 위치를 유지하도록 배치 영역과 Transform까지 산출한다.
    // 이후 지면·충돌·내비게이션 검사는 명령 결과를 바꾸지 않으므로 권위 실행에서는 fail-fast한다.
    if (bBuildLimitReached)
    {
        return Result;
    }

    const AFFZSBuildArea* BuildArea = FindBuildArea();
    TUniquePtr<FZoneCellOwnershipCache> ZoneOwnershipCache;
    if (!BuildArea)
    {
        ZoneOwnershipCache = MakeUnique<FZoneCellOwnershipCache>(ZonesById, *Zone);
    }
    for (const FFFZSOrientedRect& Rect : Result.ReservedOrientedRects)
    {
        const bool bInside = BuildArea
            ? BuildArea->ContainsWorldOrientedRect(TransformRegion(Rect, Zone->GetActorTransform()), Zone->GetActorLocation().Z)
            : IsOrientedRectOwnedByExactlyOneZone(*ZoneOwnershipCache, Rect,
                UFFZSDeveloperSettings::DefaultPlacementCellSizeCm);
        if (!bInside)
        {
            Result.AddFailure(EFFZSPlacementFailure::OutsideZone,
                BuildArea ? LOCTEXT("OutsideUnifiedBuildArea", "시설, 여유 공간 및 최대 대기열이 통합 건설 영역의 바깥이나 빈 공간을 침범합니다.")
                    : LOCTEXT("OutsideOrOverlappingZone", "Footprint, 여유 공간 및 최대 Queue 영역은 정확히 하나의 Gameplay Zone 안에 완전히 포함되어야 합니다."));
            break;
        }
    }

    const FGuid IgnoreId = IgnoreFacility ? IgnoreFacility->GetFacilityInstanceId() : FGuid();
    // 점유 원장을 한 번만 순회하며 세 영역의 실패 사유는 각각 보존한다.
    bool bFootprintOccupied = false;
    bool bClearanceOccupied = false;
    bool bQueueOccupied = false;
    bool bRoadDisallowsFacilities = false;
    const TArray<FFFZSOrientedRect> WorldReserved = ToWorldRegions(Command.ZoneId, Result.ReservedOrientedRects);
    const TArray<FFFZSOrientedRect> WorldFootprint = ToWorldRegions(Command.ZoneId, Result.FootprintOrientedRects);
    const TArray<FFFZSOrientedRect> WorldClearance = ToWorldRegions(Command.ZoneId, Result.ClearanceOrientedRects);
    const TArray<FFFZSOrientedRect> WorldQueue = ToWorldRegions(Command.ZoneId, Result.QueueOrientedRects);
    TArray<FGuid> Candidates;
    QueryFacilityCandidates(WorldReserved, Candidates);
    for (const FGuid Id : Candidates)
    {
        const FFFZSFacilityRegionOccupancy* Occupancy = OccupiedFacilityRegions.Find(Id);
        if (Id == IgnoreId || !Occupancy || !IsOccupancyRelevant(Command.ZoneId, *Occupancy))
        {
            continue;
        }
        if (Command.FacilitySpec.Category == EFFZSFacilityCategory::Road &&
            Command.FacilitySpec.bAllowFacilityPlacementOnTop && !Occupancy->bIsRoad)
        {
            if (bRevalidateExisting) continue;
            // Paving may extend through queues, but adding it underneath an existing building
            // would bury that building's fixed foundation. Place the road before the building.
            if (const AFFZSFacilityActor* Existing = FindFacility(Id))
            {
                TArray<FFFZSOrientedRect> ExistingFootprint, ExistingClearance, ExistingQueue, ExistingReserved;
                UFFZSGridLibrary::BuildPlacementOrientedRegions(Existing->GetFacilitySpec(), Existing->GetGridPlacement(),
                    ExistingFootprint, ExistingClearance, ExistingQueue, ExistingReserved);
                bFootprintOccupied |= UFFZSGridLibrary::DoOrientedRectSetsOverlap(WorldFootprint,
                    ToWorldRegions(Existing->GetZoneId(), ExistingFootprint));
            }
            continue;
        }
        if (!Occupancy->BlocksLayer(Command.FacilitySpec.Category == EFFZSFacilityCategory::Road,
            Command.FacilitySpec.bAllowFacilityPlacementOnTop))
        {
            continue;
        }
        bRoadDisallowsFacilities |= Occupancy->bIsRoad && !Occupancy->bAllowsFacilitiesOnTop &&
            Command.FacilitySpec.Category != EFFZSFacilityCategory::Road &&
            UFFZSGridLibrary::DoOrientedRectSetsOverlap(WorldReserved, Occupancy->WorldRegions);
        if (!bFootprintOccupied)
        {
            bFootprintOccupied = UFFZSGridLibrary::DoOrientedRectSetsOverlap(
                WorldFootprint, Occupancy->WorldRegions);
        }
        if (!bClearanceOccupied)
        {
            bClearanceOccupied = UFFZSGridLibrary::DoOrientedRectSetsOverlap(
                WorldClearance, Occupancy->WorldRegions);
        }
        if (!bQueueOccupied)
        {
            bQueueOccupied = UFFZSGridLibrary::DoOrientedRectSetsOverlap(
                WorldQueue, Occupancy->WorldRegions);
        }
        if (bFootprintOccupied && bClearanceOccupied && bQueueOccupied)
        {
            break;
        }
    }

    if (bFootprintOccupied)
    {
        Result.AddFailure(EFFZSPlacementFailure::PlacementCollision,
            Command.FacilitySpec.Category == EFFZSFacilityCategory::Road
                ? LOCTEXT("RoadFootprintOccupied", "이미 설치된 길이나 시설 본체 아래에 길을 설치할 수 없습니다. 시설을 옮긴 뒤 길을 먼저 설치하세요.")
                : bRoadDisallowsFacilities
                    ? LOCTEXT("RoadDisallowsFacilities", "이 길은 데이터 에셋의 '길 위 시설 설치 허용'이 꺼져 있어 시설을 설치할 수 없습니다.")
                    : LOCTEXT("FootprintOccupied", "Facility Footprint가 다른 Facility 예약 영역과 겹칩니다."));
    }
    if (bClearanceOccupied)
    {
        Result.AddFailure(EFFZSPlacementFailure::InsufficientClearance,
            LOCTEXT("ClearanceOccupied", "필요한 여유 공간이 점유되어 있습니다."));
    }
    if (bQueueOccupied)
    {
        Result.AddFailure(EFFZSPlacementFailure::QueueFootprintBlocked,
            LOCTEXT("QueueOccupied", "최대 Queue 영역이 점유되어 있습니다."));
    }
    ValidateProtectedAreas(Command.ZoneId, *Zone, Command.FacilitySpec, Result);
    ValidateGround(*Zone, Command.FacilitySpec, IgnoreFacility, Result);
    if (Command.FacilitySpec.Category != EFFZSFacilityCategory::Road)
    {
        // Ground can contain both uncovered soil and paving, and visual meshes load asynchronously.
        // Use the registered road surface as a minimum base height so a facility never sinks into
        // a tile because traces averaged mixed heights or its presentation has not loaded yet.
        FVector Location = Result.PlacementTransform.GetLocation();
        for (const FGuid Id : Candidates)
        {
            const FFFZSFacilityRegionOccupancy* Occupancy = OccupiedFacilityRegions.Find(Id);
            const AFFZSFacilityActor* Road = FindFacility(Id);
            if (Occupancy && Occupancy->bIsRoad && Road && IsOccupancyRelevant(Command.ZoneId, *Occupancy) &&
                UFFZSGridLibrary::DoOrientedRectSetsOverlap(WorldFootprint, Occupancy->WorldRegions))
            {
                Location.Z = FMath::Max(Location.Z, Road->GetActorLocation().Z +
                    Road->GetFacilitySpec().PlacementObstacleHeightCm);
            }
        }
        Result.PlacementTransform.SetLocation(Location);
    }
    ValidateWorldCollision(*Zone, Command.FacilitySpec, IgnoreFacility, Result);
    ValidateNavigation(Command.FacilitySpec, Result);

    Result.bIsValid = Result.Reasons.IsEmpty();
    if (Result.bIsValid)
    {
        Result.Summary = LOCTEXT("PlacementValid", "배치가 유효합니다.");
    }
    return Result;
}

FTransform UFFZSWorldSubsystem::BuildPlacementTransform(
    const AFFZSGameplayZone& Zone,
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement) const
{
    TArray<FFFZSOrientedRect> Footprint;
    TArray<FFFZSOrientedRect> Clearance;
    TArray<FFFZSOrientedRect> Queue;
    TArray<FFFZSOrientedRect> Reserved;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Spec, Placement, Footprint, Clearance, Queue, Reserved);
    return MakePlacementTransform(Zone, Placement, Footprint);
}
void UFFZSWorldSubsystem::ValidateGround(
    const AFFZSGameplayZone& Zone,
    const FFFZSFacilitySpec& Spec,
    const AFFZSFacilityActor* IgnoreFacility,
    FFFZSPlacementValidation& InOutValidation) const
{
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    UWorld* World = GetWorld();
    if (!Settings->bEnableGroundChecks || !World)
    {
        return;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(FFZoneSystemGround), false);
    Params.AddIgnoredActor(&Zone);
    if (Spec.Category == EFFZSFacilityCategory::Road)
    {
        // Repositioning a road, or laying a neighbor at a shared edge, samples the underlying
        // terrain rather than the existing paving. Same-layer occupancy still rejects stacking.
        for (const TPair<FGuid, TWeakObjectPtr<AFFZSFacilityActor>>& Pair : FacilitiesById)
        {
            if (const AFFZSFacilityActor* Facility = Pair.Value.Get(); Facility &&
                Facility->GetFacilitySpec().Category == EFFZSFacilityCategory::Road)
            {
                Params.AddIgnoredActor(Facility);
            }
        }
    }
    if (IgnoreFacility)
    {
        Params.AddIgnoredActor(IgnoreFacility);
    }

    const double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const double SampleSpacingCm = FMath::Clamp(
        static_cast<double>(Settings->GroundSampleSpacingCm), 1.0, 1000.0);
    const FTransform ZoneTransform = Zone.GetActorTransform();
    const FVector TraceAbove = FVector::UpVector * Settings->GroundTraceAboveCm;
    const FVector TraceBelow = FVector::UpVector * Settings->GroundTraceBelowCm;
    // 높이 통계와 최소 UpDot을 즉시 누적해 임시 배열과 샘플별 acos를 없앤다.
    int32 SuccessfulSampleCount = 0;
    float HeightSum = 0.0f;
    float MinHeight = MAX_flt;
    float MaxHeight = -MAX_flt;
    float MinimumUpDot = 1.0f;

    // 물리 샘플 밀도는 1cm 점유 해상도와 분리한다.
    for (const FFFZSOrientedRect& Rect : InOutValidation.FootprintOrientedRects)
    {
        const double RectWidth = Rect.HalfExtentCells.X * 2.0;
        const double RectHeight = Rect.HalfExtentCells.Y * 2.0;
        const double WidthCm = RectWidth * CellSizeCm;
        const double HeightCm = RectHeight * CellSizeCm;
        const int32 SamplesX = FMath::Max(1, FMath::CeilToInt(WidthCm / SampleSpacingCm));
        const int32 SamplesY = FMath::Max(1, FMath::CeilToInt(HeightCm / SampleSpacingCm));
        if (static_cast<int64>(SamplesX) * SamplesY > UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility())
        {
            InOutValidation.AddFailure(EFFZSPlacementFailure::DataInvalid,
                LOCTEXT("GroundSampleBudget", "지면 샘플링이 설정된 안전 예산을 초과했습니다."));
            return;
        }
        const double StepX = RectWidth / SamplesX;
        const double StepY = RectHeight / SamplesY;
        const double YawRadians = FMath::DegreesToRadians(static_cast<double>(Rect.YawDegrees));
        const FVector2D AxisX(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
        const FVector2D AxisY(-AxisX.Y, AxisX.X);

        for (int32 X = 0; X < SamplesX; ++X)
        {
            for (int32 Y = 0; Y < SamplesY; ++Y)
            {
                const double LocalX = -Rect.HalfExtentCells.X + (X + 0.5) * StepX;
                const double LocalY = -Rect.HalfExtentCells.Y + (Y + 0.5) * StepY;
                const FVector2D GridPoint = Rect.CenterCell + AxisX * LocalX + AxisY * LocalY;
                const FVector Sample = ZoneTransform.TransformPosition(
                    FVector(GridPoint.X * CellSizeCm, GridPoint.Y * CellSizeCm, 0.0));
                const FVector Start = Sample + TraceAbove;
                const FVector End = Sample - TraceBelow;
                FHitResult Hit;
                if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
                {
                    if (Settings->bRequireEveryGroundSample)
                    {
                        InOutValidation.AddFailure(EFFZSPlacementFailure::InvalidGround,
                            LOCTEXT("GroundMissing", "물리 Footprint 샘플 중 하나 이상에서 유효한 지면을 찾지 못했습니다."));
                        return;
                    }
                    continue;
                }
                const float Height = static_cast<float>(Hit.ImpactPoint.Z);
                ++SuccessfulSampleCount;
                HeightSum += Height;
                MinHeight = FMath::Min(MinHeight, Height);
                MaxHeight = FMath::Max(MaxHeight, Height);
                MinimumUpDot = FMath::Min(MinimumUpDot, FMath::Clamp(FVector::DotProduct(
                    Hit.ImpactNormal.GetSafeNormal(), FVector::UpVector), -1.0f, 1.0f));
            }
        }
    }

    if (SuccessfulSampleCount == 0)
    {
        InOutValidation.AddFailure(EFFZSPlacementFailure::InvalidGround,
            LOCTEXT("NoGround", "성공한 Footprint 지면 샘플이 없습니다."));
        return;
    }
    const float MaximumSlope = FMath::RadiansToDegrees(FMath::Acos(MinimumUpDot));
    if (MaximumSlope > Settings->MaxGroundSlopeDegrees)
    {
        InOutValidation.AddFailure(EFFZSPlacementFailure::ExcessSlope,
            LOCTEXT("SlopeHigh", "지면 경사가 설정된 프로토타입 제한을 초과했습니다."));
    }
    if (MaxHeight - MinHeight > Settings->MaxGroundHeightVarianceCm)
    {
        InOutValidation.AddFailure(EFFZSPlacementFailure::HeightVarianceExceeded,
            LOCTEXT("HeightVar", "지면 높이 편차가 설정된 프로토타입 제한을 초과했습니다."));
    }
    FVector Location = InOutValidation.PlacementTransform.GetLocation();
    Location.Z = HeightSum / SuccessfulSampleCount;
    InOutValidation.PlacementTransform.SetLocation(Location);
}
void UFFZSWorldSubsystem::ValidateWorldCollision(
    const AFFZSGameplayZone& Zone,
    const FFFZSFacilitySpec& Spec,
    const AFFZSFacilityActor* IgnoreFacility,
    FFFZSPlacementValidation& InOutValidation) const
{
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    UWorld* World = GetWorld();
    if (!Settings->bEnableWorldCollisionChecks || !World)
    {
        return;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(FFZoneSystemPlacement), false);
    Params.AddIgnoredActor(&Zone);
    if (IgnoreFacility)
    {
        Params.AddIgnoredActor(IgnoreFacility);
    }

    const FTransform ZoneTransform = Zone.GetActorTransform();
    // Rect마다 새로 할당하지 않고 Overlap 결과 버퍼를 재사용한다.
    TArray<FOverlapResult> Overlaps;
    Overlaps.Reserve(16);
    const auto HasBlockingOverlap = [World, &Params, &Overlaps](
        const FVector& Center, const FQuat& Rotation, const FVector& HalfExtent) -> bool
    {
        Overlaps.Reset();
        if (!World->OverlapMultiByChannel(
            Overlaps, Center, Rotation, ECC_Visibility, FCollisionShape::MakeBox(HalfExtent), Params))
        {
            return false;
        }
        for (const FOverlapResult& Overlap : Overlaps)
        {
            const AActor* Actor = Overlap.GetActor();
            const UPrimitiveComponent* Component = Overlap.GetComponent();
            if (!Actor || Actor->IsA<AFFZSGameplayZone>() || Actor->IsA<AFFZSProtectedArea>() ||
                Actor->IsA<AFFZSFacilityActor>())
            {
                continue;
            }
            if (!Component || Component->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Ignore)
            {
                return true;
            }
        }
        return false;
    };

    const double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    // [하드코딩 제거] Overlap Box를 예약 영역보다 아주 조금 작게 만드는 여백 값들에 이름을 붙인다.
    // 경계가 정확히 맞닿기만 한 지면/이웃 구조물을 "충돌"로 보고하지 않기 위한 수축량이며,
    // 예전에는 5.0f/2.0f/3.0f/0.25가 설명 없는 리터럴로 흩어져 있어 의도를 알기 어려웠다.
    constexpr double SurfaceInsetCm = 2.0;        // 각 면을 안쪽으로 당기는 거리
    constexpr double GroundLiftCm = 3.0;          // 바닥면과의 접촉을 피하기 위한 들어올림
    constexpr double MinHalfExtentCm = 0.25;      // 1cm Rect도 유효한 Box가 되도록 하는 최소 반치수
    constexpr double MinHalfHeightCm = 5.0;       // 낮은 장애물 높이에서도 남기는 최소 반높이
    const float HalfHeight = static_cast<float>(
        FMath::Max(MinHalfHeightCm, Spec.PlacementObstacleHeightCm * 0.5 - SurfaceInsetCm));
    const float CenterZ = static_cast<float>(
        InOutValidation.PlacementTransform.GetLocation().Z + HalfHeight + GroundLiftCm);
    const auto ValidateRectGroup = [&] (
        const TArray<FFFZSOrientedRect>& Rects,
        const EFFZSPlacementFailure Failure,
        const FText& Message)
    {
        for (const FFFZSOrientedRect& Rect : Rects)
        {
            const double HalfX = Rect.HalfExtentCells.X * CellSizeCm;
            const double HalfY = Rect.HalfExtentCells.Y * CellSizeCm;
            const FVector HalfExtent(
                FMath::Max(MinHalfExtentCm, HalfX - SurfaceInsetCm),
                FMath::Max(MinHalfExtentCm, HalfY - SurfaceInsetCm),
                HalfHeight);
            FVector Center = ZoneTransform.TransformPosition(FVector(
                Rect.CenterCell.X * CellSizeCm,
                Rect.CenterCell.Y * CellSizeCm,
                0.0));
            Center.Z = CenterZ;
            const FQuat Rotation = ZoneTransform.GetRotation() * FQuat(
                FVector::UpVector,
                FMath::DegreesToRadians(static_cast<double>(Rect.YawDegrees)));
            if (HasBlockingOverlap(Center, Rotation, HalfExtent))
            {
                InOutValidation.AddFailure(Failure, Message);
                return;
            }
        }
    };

    ValidateRectGroup(InOutValidation.FootprintOrientedRects, EFFZSPlacementFailure::PlacementCollision,
        LOCTEXT("WorldFootprintBlocked", "Facility Footprint가 배치를 막는 World Object와 겹칩니다."));
    ValidateRectGroup(InOutValidation.ClearanceOrientedRects, EFFZSPlacementFailure::InsufficientClearance,
        LOCTEXT("WorldClearanceBlocked", "여유 공간이 배치를 막는 World Object와 겹칩니다."));
    ValidateRectGroup(InOutValidation.QueueOrientedRects, EFFZSPlacementFailure::QueueFootprintBlocked,
        LOCTEXT("WorldQueueBlocked", "최대 Queue 영역이 배치를 막는 World Object와 겹칩니다."));
}
void UFFZSWorldSubsystem::ValidateNavigation(const FFFZSFacilitySpec& Spec, FFFZSPlacementValidation& InOutValidation) const
{
    if (Spec.Category == EFFZSFacilityCategory::Lighting || Spec.Category == EFFZSFacilityCategory::Road)
    {
        // 조명에는 고객 Entry/Service/Queue 목적지가 없다. 본체 충돌과 보호 동선은
        // 다른 배치 검증 단계에서 검사하고, 존재하지 않는 서비스 경로만 생략한다.
        return;
    }
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    UWorld* World = GetWorld();
    if (!Settings->bEnableNavigationProjectionChecks || !World)
    {
        return;
    }
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Navigation)
    {
        if (Settings->bFailWhenNavigationSystemIsUnavailable)
        {
            InOutValidation.AddFailure(EFFZSPlacementFailure::NavigationUnavailable, LOCTEXT("NoNavigation", "내비게이션 검증이 필요하지만 사용할 수 있는 Navigation System이 없습니다."));
        }
        return;
    }

    const float ExtentCm = FMath::Max(1.0f,
        FMath::Min(Settings->NavigationProjectionExtentCm, Spec.QueueRules.NavigationProjectionExtentCm));
    const FVector ProjectionExtent(ExtentCm, ExtentCm, ExtentCm);
    TArray<FVector> ProjectedRoutePoints;
    ProjectedRoutePoints.Reserve(
        Spec.Anchors.Num() + FMath::Max(0, Spec.QueueRules.MaxWaitingAgents));
    FVector ProjectedEntry = FVector::ZeroVector;
    bool bHasEntry = false;

    const auto ProjectRequiredPoint = [&](const FVector& LocalPoint, const bool bEntry) -> bool
    {
        FNavLocation Projected;
        if (!Navigation->ProjectPointToNavigation(
            InOutValidation.PlacementTransform.TransformPosition(LocalPoint), Projected, ProjectionExtent))
        {
            return false;
        }
        ProjectedRoutePoints.Add(Projected.Location);
        if (bEntry && !bHasEntry)
        {
            ProjectedEntry = Projected.Location;
            bHasEntry = true;
        }
        return true;
    };

    for (const FFFZSAnchorDefinition& Anchor : Spec.Anchors)
    {
        if (!ProjectRequiredPoint(Anchor.LocalTransform.GetLocation(), Anchor.Type == EFFZSAnchorType::Entry))
        {
            InOutValidation.AddFailure(EFFZSPlacementFailure::RequiredRouteDisconnected,
                LOCTEXT("AnchorOffNav", "Entry, Service 또는 Exit Anchor를 설정된 범위 안의 내비게이션에 투영할 수 없습니다."));
            return;
        }
    }

    const auto AddQueueSlotOffNavFailure = [&]()
    {
        InOutValidation.AddFailure(EFFZSPlacementFailure::RequiredRouteDisconnected,
            LOCTEXT("QueueSlotOffNav", "최대 수용 인원의 Queue 슬롯을 내비게이션에 투영할 수 없습니다."));
    };
    TArray<FVector> LocalQueueSlotPoints;
    if (!FFFZSQueueGeometry::BuildLinearQueueSlotPoints(
            Spec.QueueSplinePointsCm,
            Spec.QueueRules.MaxWaitingAgents,
            Spec.QueueRules.SlotSpacingCm,
            LocalQueueSlotPoints))
    {
        AddQueueSlotOffNavFailure();
        return;
    }
    for (const FVector& LocalSlot : LocalQueueSlotPoints)
    {
        if (!ProjectRequiredPoint(LocalSlot, false))
        {
            AddQueueSlotOffNavFailure();
            return;
        }
    }

    if (!bHasEntry)
    {
        InOutValidation.AddFailure(EFFZSPlacementFailure::RequiredRouteDisconnected, LOCTEXT("NoProjectedEntry", "경로 연결성을 검사할 Entry Anchor가 없습니다."));
        return;
    }
    for (const FVector& RoutePoint : ProjectedRoutePoints)
    {
        // 일부 Navigation 구현은 길이 0인 경로 객체를 만들지 않는다.
        if (FVector::DistSquared(ProjectedEntry, RoutePoint) <= 1.0f)
        {
            continue;
        }
        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, ProjectedEntry, RoutePoint);
        if (!Path || !Path->IsValid() || Path->IsPartial())
        {
            InOutValidation.AddFailure(EFFZSPlacementFailure::RequiredRouteDisconnected,
                LOCTEXT("DisconnectedNavIsland", "Entry, Queue 슬롯, Service 및 Exit는 하나의 완전한 내비게이션 경로로 연결되어야 합니다."));
            return;
        }
    }
}

void UFFZSWorldSubsystem::ValidateProtectedAreas(const FName ZoneId, const AFFZSGameplayZone& Zone, const FFFZSFacilitySpec& Spec, FFFZSPlacementValidation& InOutValidation) const
{
    if (ProtectedAreas.IsEmpty())
    {
        return;
    }

    struct FWorldOrientedBox
    {
        FVector Center = FVector::ZeroVector;
        FVector HalfExtent = FVector::ZeroVector;
        FQuat Rotation = FQuat::Identity;
    };
    // ProtectedArea마다 반복하지 않도록 권위 Reserved OBB의 월드 값을 한 번만 만든다.
    TArray<FWorldOrientedBox> RegionBoxes;
    RegionBoxes.Reserve(InOutValidation.ReservedOrientedRects.Num());
    const FTransform ZoneTransform = Zone.GetActorTransform();
    const double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const double HalfHeight = FMath::Max(0.0, static_cast<double>(Spec.PlacementObstacleHeightCm) * 0.5);
    for (const FFFZSOrientedRect& Rect : InOutValidation.ReservedOrientedRects)
    {
        FWorldOrientedBox& Box = RegionBoxes.AddDefaulted_GetRef();
        Box.Center = ZoneTransform.TransformPosition(FVector(
            Rect.CenterCell.X * CellSizeCm,
            Rect.CenterCell.Y * CellSizeCm,
            0.0));
        Box.Center.Z = InOutValidation.PlacementTransform.GetLocation().Z + HalfHeight;
        Box.HalfExtent = FVector(
            Rect.HalfExtentCells.X * CellSizeCm,
            Rect.HalfExtentCells.Y * CellSizeCm,
            HalfHeight);
        Box.Rotation = ZoneTransform.GetRotation() * FQuat(
            FVector::UpVector,
            FMath::DegreesToRadians(static_cast<double>(Rect.YawDegrees)));
    }

    for (const TWeakObjectPtr<AFFZSProtectedArea>& WeakArea : ProtectedAreas)
    {
        const AFFZSProtectedArea* Area = WeakArea.Get();
        if (!Area)
        {
            continue;
        }

        TArray<FText> AreaErrors;
        if (!Area->ValidateProtectedAreaData(AreaErrors) ||
            (!Area->LinkedZoneId.IsNone() && !FindZone(Area->LinkedZoneId)))
        {
            InOutValidation.AddFailure(EFFZSPlacementFailure::DataInvalid,
                AreaErrors.IsEmpty() ? LOCTEXT("InvalidProtectedArea", "Protected Area가 존재하지 않는 Gameplay Zone을 참조합니다.") : AreaErrors[0]);
            return;
        }
        if (!FindBuildArea() && !Area->LinkedZoneId.IsNone() && Area->LinkedZoneId != ZoneId)
        {
            continue;
        }

        for (const FWorldOrientedBox& RegionBox : RegionBoxes)
        {
            if (Area->IntersectsWorldOrientedBox(
                RegionBox.Center, RegionBox.HalfExtent, RegionBox.Rotation))
            {
                const bool bAccessibility = Area->AreaType == EFFZSProtectedAreaType::AccessibilityRoute || Area->AreaType == EFFZSProtectedAreaType::ViewingArea;
                InOutValidation.AddFailure(
                    bAccessibility ? EFFZSPlacementFailure::AccessibilityRouteBlocked : EFFZSPlacementFailure::ProtectedRouteBlocked,
                    bAccessibility ? LOCTEXT("AccessibilityArea", "배치 영역이 접근성 경로 또는 보호 관람 영역과 겹칩니다.") : LOCTEXT("ProtectedArea", "배치 영역이 보호 경로 또는 건설 금지 영역과 겹칩니다."));
                break;
            }
        }
    }
}

// 정책 콜백의 재진입을 막고, Tentative 실패 시 적용된 정책을 역순으로 Rollback한다.
bool UFFZSWorldSubsystem::RunPolicyPreValidation(const FFFZSPolicyContext& Context, FFFZSPolicyDecision& OutDecision) const
{
    if (bIsInvokingExternalPolicy)
    {
        OutDecision.bAllowed = false;
        OutDecision.FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
        OutDecision.Message = LOCTEXT("ReentrantPolicyPreValidate", "외부 Policy 검증을 재귀적으로 실행할 수 없습니다.");
        return false;
    }
    TGuardValue<bool> PolicyCallbackGuard(bIsInvokingExternalPolicy, true);
    const TArray<TWeakObjectPtr<UObject>> PolicySnapshot = ExternalPolicies;
    for (const TWeakObjectPtr<UObject>& WeakPolicy : PolicySnapshot)
    {
        UObject* Policy = WeakPolicy.Get();
        if (!Policy || !Policy->GetClass()->ImplementsInterface(UFFZSExternalPolicy::StaticClass()))
        {
            continue;
        }
        const FFFZSPolicyDecision Decision = IFFZSExternalPolicy::Execute_PreValidateFFZoneSystemChange(Policy, Context);
        if (!Decision.bAllowed)
        {
            OutDecision = Decision;
            return false;
        }
    }
    OutDecision = FFFZSPolicyDecision();
    return true;
}

bool UFFZSWorldSubsystem::ApplyTentativePolicies(const FFFZSPolicyContext& Context, TArray<TWeakObjectPtr<UObject>>& OutAppliedPolicies, FFFZSPolicyDecision& OutDecision)
{
    OutAppliedPolicies.Reset();
    OutAppliedPolicies.Reserve(ExternalPolicies.Num());
    if (bIsInvokingExternalPolicy)
    {
        OutDecision.bAllowed = false;
        OutDecision.FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
        OutDecision.Message = LOCTEXT("ReentrantPolicyApply", "외부 Policy 적용을 재귀적으로 실행할 수 없습니다.");
        return false;
    }
    TGuardValue<bool> PolicyCallbackGuard(bIsInvokingExternalPolicy, true);
    const TArray<TWeakObjectPtr<UObject>> PolicySnapshot = ExternalPolicies;
    for (const TWeakObjectPtr<UObject>& WeakPolicy : PolicySnapshot)
    {
        UObject* Policy = WeakPolicy.Get();
        if (!Policy || !Policy->GetClass()->ImplementsInterface(UFFZSExternalPolicy::StaticClass()))
        {
            continue;
        }
        OutAppliedPolicies.Add(Policy);
        const FFFZSPolicyDecision Decision = IFFZSExternalPolicy::Execute_ApplyTentativeFFZoneSystemChange(Policy, Context);
        if (!Decision.bAllowed)
        {
            OutDecision = Decision;
            RollbackPolicies(Context, OutAppliedPolicies);
            OutAppliedPolicies.Reset();
            return false;
        }
    }
    OutDecision = FFFZSPolicyDecision();
    return true;
}

void UFFZSWorldSubsystem::CommitPolicies(const FFFZSPolicyContext& Context, const TArray<TWeakObjectPtr<UObject>>& AppliedPolicies)
{
    TGuardValue<bool> PolicyCallbackGuard(bIsInvokingExternalPolicy, true);
    const TArray<TWeakObjectPtr<UObject>> PolicySnapshot = AppliedPolicies;
    for (const TWeakObjectPtr<UObject>& WeakPolicy : PolicySnapshot)
    {
        if (UObject* Policy = WeakPolicy.Get())
        {
            IFFZSExternalPolicy::Execute_CommitFFZoneSystemChange(Policy, Context);
        }
    }
}

void UFFZSWorldSubsystem::RollbackPolicies(const FFFZSPolicyContext& Context, const TArray<TWeakObjectPtr<UObject>>& AppliedPolicies)
{
    TGuardValue<bool> PolicyCallbackGuard(bIsInvokingExternalPolicy, true);
    const TArray<TWeakObjectPtr<UObject>> PolicySnapshot = AppliedPolicies;
    for (int32 Index = PolicySnapshot.Num() - 1; Index >= 0; --Index)
    {
        if (UObject* Policy = PolicySnapshot[Index].Get())
        {
            IFFZSExternalPolicy::Execute_RollbackFFZoneSystemChange(Policy, Context);
        }
    }
}

// CommandId와 Payload fingerprint로 멱등 재생을 보장하고 InFlight 상태로 재진입을 막는다.
// Receipt가 준비된 뒤 이벤트를 발행하며 실패는 WorldRevision을 올리지 않는다.
FFFZSCommandResult UFFZSWorldSubsystem::ExecuteCommand(const FFFZSCommand& Command)
{
    if (!IsInGameThread())
    {
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("GameThreadOnly", "Zone/Facility 명령은 Game Thread에서 실행해야 합니다."));
    }
    if (bIsRestoringSnapshot)
    {
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("CommandDuringRestore", "Zone/Facility 스냅샷 복원이 이미 진행 중입니다."));
    }
    if (bIsInvokingExternalPolicy)
    {
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("CommandDuringPolicyCallback", "외부 Policy Callback에서는 명령을 재귀적으로 시작할 수 없습니다."));
    }
    if (!Command.CommandId.IsValid() || !Command.TransactionId.IsValid())
    {
        return MakeFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("MissingCommandIds", "CommandId와 TransactionId가 필요합니다."));
    }
    if (!UFFZSGridLibrary::HasSafeFacilitySpecCardinality(Command.FacilitySpec))
    {
        return MakeFailure(
            EFFZSPlacementFailure::DataInvalid,
            LOCTEXT("UnsafeFacilitySpecCardinality", "FacilitySpec 배열 크기가 시설당 안전 제한을 초과했습니다."));
    }

    const FString PayloadFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    if (const FFFZSCommandReceipt* ExistingReceipt = CommandReceipts.Find(Command.CommandId))
    {
        if (ExistingReceipt->PayloadFingerprint != PayloadFingerprint)
        {
            return MakeFailure(EFFZSPlacementFailure::DuplicatePayloadMismatch, LOCTEXT("CommandIdReused", "CommandId가 이미 다른 Payload에 사용되었습니다."));
        }
        FFFZSCommandResult Replay = ExistingReceipt->Result;
        Replay.FacilityActor = CommandReceiptFacilityReferences.Contains(Command.CommandId)
            ? FindFacility(ExistingReceipt->FacilityInstanceId) : nullptr;
        Replay.bWasDuplicateReplay = true;
        return Replay;
    }

    if (Command.Type == EFFZSCommandType::Install)
    {
#if WITH_DEV_AUTOMATION_TESTS
        const bool bSyntheticAutomationFixture = GIsAutomationTesting;
#else
        constexpr bool bSyntheticAutomationFixture = false;
#endif
        if (!bSyntheticAutomationFixture &&
            !UFFZSFacilityDefinition::DoesSpecMatchLoadedDefinition(
                Command.FacilitySpec))
        {
            return MakeFailure(
                EFFZSPlacementFailure::DataInvalid,
                LOCTEXT(
                    "DefinitionSpecMismatch",
                    "Install requires the exact loaded UFFZSFacilityDefinition Spec."));
        }
    }

    if (WorldRevision >= MAX_int64 - 1)
    {
        return MakeFailure(
            EFFZSPlacementFailure::InternalError,
            LOCTEXT("WorldRevisionExhausted", "WorldRevision space is exhausted; no further mutation is safe."));
    }

    if (InFlightCommandId.IsValid())
    {
        if (InFlightCommandId == Command.CommandId && InFlightCommandFingerprint != PayloadFingerprint)
        {
            return MakeFailure(EFFZSPlacementFailure::DuplicatePayloadMismatch, LOCTEXT("InFlightPayloadMismatch", "실행 중인 CommandId가 다른 Payload로 다시 요청되었습니다."));
        }
        // 이 일시적 거부는 의도적으로 영구 Command 영수증에 저장하지 않는다.
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("DifferentCommandInFlight", "다른 Zone/Facility 명령이 이미 실행 중입니다."));
    }

    TGuardValue<FGuid> CommandGuard(InFlightCommandId, Command.CommandId);
    TGuardValue<FString> FingerprintGuard(InFlightCommandFingerprint, PayloadFingerprint);
    DeferredWorldChangeEvent.Reset();

    FFFZSCommandResult Result;
    TArray<FText> IntegrationErrors;
    if (GetDefault<UFFZSDeveloperSettings>()->bRequireProductionPolicyDomains &&
        !ValidateIntegrationReadiness(IntegrationErrors, true))
    {
        Result = MakeFailure(EFFZSPlacementFailure::ExternalPolicyRejected, IntegrationErrors[0]);
    }
    else if (Command.ExpectedWorldRevision >= 0 && Command.ExpectedWorldRevision != WorldRevision)
    {
        Result = MakeFailure(EFFZSPlacementFailure::StaleWorldRevision, LOCTEXT("RevisionMismatch", "ExpectedWorldRevision이 권위 Revision과 일치하지 않습니다."));
    }
    else if (IsWorldChangeCommand(Command.Type) && !IsWorldChangePhase(CurrentPhase))
    {
        Result = MakeFailure(EFFZSPlacementFailure::WrongStage, LOCTEXT("WrongCommandPhase", "Install, Move, Remove 및 Upgrade는 InitialSetup 또는 MorningCheck Phase에서만 허용됩니다."));
    }
    else if (Command.Type == EFFZSCommandType::Install || Command.Type == EFFZSCommandType::Move)
    {
        const FFFZSPlacementValidation Validation = ValidatePlacement(Command);
        Result = Validation.bIsValid
            ? (Command.Type == EFFZSCommandType::Install ? ExecuteInstall(Command, Validation) : ExecuteMove(Command, Validation))
            : MakeFailure(Validation.Reasons.IsEmpty() ? EFFZSPlacementFailure::DataInvalid : Validation.Reasons[0], Validation.Summary);
    }
    else if (Command.Type == EFFZSCommandType::Remove)
    {
        Result = ExecuteRemove(Command);
    }
    else if (Command.Type == EFFZSCommandType::Upgrade)
    {
        Result = ExecuteUpgrade(Command);
    }
    else
    {
        Result = ExecuteLifecycleCommand(Command);
    }

    const bool bResultReferencesFacility = IsValid(Result.FacilityActor);
    if (bResultReferencesFacility)
    {
        CommandReceiptFacilityReferences.Add(Command.CommandId);
    }
    else
    {
        CommandReceiptFacilityReferences.Remove(Command.CommandId);
    }

    FFFZSCommandReceipt CompletedReceipt;
    CompletedReceipt.PayloadFingerprint = PayloadFingerprint;
    CompletedReceipt.FacilityInstanceId = Command.FacilityInstanceId;
    CompletedReceipt.Result = Result;
    CompletedReceipt.Result.FacilityActor = nullptr;
    CompletedReceipt.bInFlight = false;
    CommandReceipts.Add(Command.CommandId, MoveTemp(CompletedReceipt));

    const TOptional<FFFZSWorldChangeEvent> EventToBroadcast = DeferredWorldChangeEvent;
    DeferredWorldChangeEvent.Reset();
    if (Result.bSucceeded)
    {
        if (EventToBroadcast.IsSet())
        {
            OnWorldChanged.Broadcast(EventToBroadcast.GetValue());
        }
    }
    else
    {
        OnPlacementRejected.Broadcast(Command, Result);
    }
    return Result;
}

FFFZSCommandResult UFFZSWorldSubsystem::ExecuteInstall(const FFFZSCommand& Command, const FFFZSPlacementValidation& Validation)
{
    FFFZSPolicyContext Context;
    Context.Command = Command;
    Context.Validation = Validation;
    Context.CurrentWorldRevision = WorldRevision;
    TArray<TWeakObjectPtr<UObject>> AppliedPolicies;
    FFFZSPolicyDecision Decision;
    if (!ApplyTentativePolicies(Context, AppliedPolicies, Decision))
    {
        return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason,
            Decision.Message.IsEmpty() ? LOCTEXT("InstallPolicy", "외부 Policy가 Install Transaction을 예약하지 못했습니다.") : Decision.Message);
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFFZSFacilityActor* Facility = GetWorld()->SpawnActor<AFFZSFacilityActor>(AFFZSFacilityActor::StaticClass(), Validation.PlacementTransform, SpawnParameters);
    if (!Facility || !Facility->InitializeFacility(Command.FacilityInstanceId, Command.FacilitySpec, Command.ZoneId, Command.Placement, Validation.PlacementTransform) || !RegisterFacility(Facility))
    {
        if (Facility)
        {
            Facility->Destroy();
        }
        RollbackPolicies(Context, AppliedPolicies);
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("InstallSpawnFailed", "Facility Actor 생성 또는 등록에 실패하여 외부 예약을 Rollback했습니다."));
    }

    Context.FacilityActor = Facility;
    CommitPolicies(Context, AppliedPolicies);
    return FinalizeSuccess(Command, Facility, NAME_None);
}

FFFZSCommandResult UFFZSWorldSubsystem::ExecuteMove(const FFFZSCommand& Command, const FFFZSPlacementValidation& Validation)
{
    AFFZSFacilityActor* Facility = FindFacility(Command.FacilityInstanceId);
    if (!Facility)
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityNotFound, LOCTEXT("MoveNotFound", "이동할 Facility를 찾을 수 없습니다."));
    }
    if (Facility->GetQueueComponent()->IsOccupied())
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityOccupied, LOCTEXT("MoveOccupied", "Facility를 이동하기 전에 닫고 Queue를 비우십시오."));
    }
    if (!Facility->CanMoveOrRemove())
    {
        return MakeFailure(EFFZSPlacementFailure::InvalidFacilityState, LOCTEXT("MoveState", "Active 상태의 Facility는 이동하기 전에 닫아야 합니다."));
    }

    FFFZSCommand EffectiveCommand = Command;
    EffectiveCommand.FacilitySpec = Facility->GetFacilitySpec();
    FFFZSPolicyContext Context;
    Context.Command = EffectiveCommand;
    Context.Validation = Validation;
    Context.FacilityActor = Facility;
    Context.CurrentWorldRevision = WorldRevision;
    TArray<TWeakObjectPtr<UObject>> AppliedPolicies;
    FFFZSPolicyDecision Decision;
    if (!ApplyTentativePolicies(Context, AppliedPolicies, Decision))
    {
        return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason,
            Decision.Message.IsEmpty() ? LOCTEXT("MovePolicy", "외부 Policy가 Move Transaction을 예약하지 못했습니다.") : Decision.Message);
    }

    const FName PreviousZoneId = Facility->GetZoneId();
    const FFFZSGridPlacement PreviousPlacement = Facility->GetGridPlacement();
    const FTransform PreviousTransform = Facility->GetActorTransform();
    ReleaseFacilityRegions(*Facility);
    if (!Facility->ApplyPlacement(Command.ZoneId, Command.Placement, Validation.PlacementTransform))
    {
        Facility->ApplyPlacement(PreviousZoneId, PreviousPlacement, PreviousTransform);
        OccupyFacilityRegions(*Facility);
        RollbackPolicies(Context, AppliedPolicies);
        return MakeFailure(EFFZSPlacementFailure::InvalidFacilityState, LOCTEXT("MoveApplyFailed", "이동에 실패하여 이전 Facility 배치를 복원했습니다."));
    }
    // 검증된 예약 영역을 재사용해 Move 이후 기하 계산을 반복하지 않는다.
    FFFZSFacilityRegionOccupancy NewOccupancy;
    NewOccupancy.ZoneId = Facility->GetZoneId();
    NewOccupancy.bIsRoad = Facility->GetFacilitySpec().Category == EFFZSFacilityCategory::Road;
    NewOccupancy.bAllowsFacilitiesOnTop = Facility->GetFacilitySpec().bAllowFacilityPlacementOnTop;
    NewOccupancy.Regions = Validation.ReservedOrientedRects;
    CacheFacilityOccupancy(Facility->GetFacilityInstanceId(), MoveTemp(NewOccupancy));
    CommitPolicies(Context, AppliedPolicies);
    return FinalizeSuccess(Command, Facility, PreviousZoneId);
}

FFFZSCommandResult UFFZSWorldSubsystem::ExecuteRemove(const FFFZSCommand& Command)
{
    AFFZSFacilityActor* Facility = FindFacility(Command.FacilityInstanceId);
    if (!Facility)
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityNotFound, LOCTEXT("RemoveNotFound", "제거할 Facility를 찾을 수 없습니다."));
    }
    if (DoesRoadSupportFacility(*Facility))
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityOccupied,
            LOCTEXT("RoadRemoveSupportsFacility", "길 위의 시설을 먼저 옮긴 후 길을 철거할 수 있습니다."));
    }
    if (Facility->GetQueueComponent()->IsOccupied())
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityOccupied, LOCTEXT("RemoveOccupied", "Facility를 제거하기 전에 닫고 Queue를 비우십시오."));
    }
    if (!Facility->CanMoveOrRemove())
    {
        return MakeFailure(EFFZSPlacementFailure::InvalidFacilityState, LOCTEXT("RemoveState", "Active 상태의 Facility는 제거하기 전에 닫아야 합니다."));
    }

    FFFZSPolicyContext Context;
    Context.Command = Command;
    Context.FacilityActor = Facility;
    Context.CurrentWorldRevision = WorldRevision;
    FFFZSPolicyDecision Decision;
    if (!RunPolicyPreValidation(Context, Decision))
    {
        return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason, Decision.Message);
    }
    TArray<TWeakObjectPtr<UObject>> AppliedPolicies;
    if (!ApplyTentativePolicies(Context, AppliedPolicies, Decision))
    {
        return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason, Decision.Message);
    }

    const FName PreviousZoneId = Facility->GetZoneId();
    UnregisterFacility(Facility);
    Facility->MarkRemoved();
    CommitPolicies(Context, AppliedPolicies);
    FFFZSCommandResult Result = FinalizeSuccess(Command, nullptr, PreviousZoneId);
    Facility->Destroy();
    return Result;
}

FFFZSCommandResult UFFZSWorldSubsystem::ExecuteUpgrade(const FFFZSCommand& Command)
{
    AFFZSFacilityActor* Facility = FindFacility(Command.FacilityInstanceId);
    if (!Facility)
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityNotFound,
            LOCTEXT("UpgradeNotFound", "강화할 시설을 찾을 수 없습니다."));
    }

    const UFFZSFacilityQueueComponent* Queue = Facility->GetQueueComponent();
    if (Queue && Queue->IsOccupied())
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityOccupied,
            LOCTEXT("UpgradeOccupied", "대기·이용·퇴장 중인 손님이 모두 나간 후 강화할 수 있습니다."));
    }
    const EFFZSFacilityState State = Facility->GetFacilityState();
    if (!Queue || Queue->GetQueueState() != EFFZSQueueState::Closed ||
        Facility->NeedsRelocation() ||
        (State != EFFZSFacilityState::Validated && State != EFFZSFacilityState::Closed &&
         State != EFFZSFacilityState::MovePending))
    {
        return MakeFailure(EFFZSPlacementFailure::InvalidFacilityState,
            LOCTEXT("UpgradeState", "정상 배치된 시설을 닫은 후 강화할 수 있습니다."));
    }
    if (!AFFZSFacilityActor::IsValidUpgradeState(Command.TargetUpgradeLevel, Command.UpgradeServiceDurationMultiplier,
            Command.UpgradeSizeMultiplier, Command.UpgradeRevenueMultiplier))
    {
        return MakeFailure(EFFZSPlacementFailure::DataInvalid,
            LOCTEXT("UpgradePayload", "강화 단계와 배수가 유효하지 않습니다. 시간·크기는 양수, 수익은 0 이상인 유한한 값이어야 합니다."));
    }

    const FFFZSFacilitySpec& Spec = Facility->GetBaseFacilitySpec();
    if (DoesRoadSupportFacility(*Facility))
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityOccupied,
            LOCTEXT("RoadUpgradeSupportsFacility", "길 위의 시설을 먼저 옮긴 후 길의 크기를 변경할 수 있습니다."));
    }
    const FFFZSFacilityUpgradeTier* AuthoredTier = FFZSFacilityUpgradeRules::FindNextTier(
        Spec.UpgradeTiers, Facility->GetUpgradeLevel());
    if (!Spec.bUsesFacilityLevels || !AuthoredTier ||
        Command.TargetUpgradeLevel != AuthoredTier->TargetLevel ||
        Command.UpgradeServiceDurationMultiplier != AuthoredTier->ServiceDurationMultiplier ||
        Command.UpgradeSizeMultiplier != AuthoredTier->SizeMultiplier ||
        Command.UpgradeRevenueMultiplier != AuthoredTier->RevenueMultiplier)
    {
        return MakeFailure(EFFZSPlacementFailure::DataInvalid,
            LOCTEXT("UpgradeNotAuthored", "이 시설 정의에 설정된 다음 강화 단계만 구매할 수 있습니다."));
    }

    const FFFZSPlacementValidation Validation = ValidateUpgradePlacement(
        Command.FacilityInstanceId, Command.UpgradeSizeMultiplier);
    if (!Validation.bIsValid)
    {
        return MakeFailure(Validation.Reasons.IsEmpty() ? EFFZSPlacementFailure::DataInvalid
            : Validation.Reasons[0], Validation.Summary);
    }

    // Policies receive authoritative identity/spec/location instead of caller supplied copies.
    // Project policies validate the configured tier, price and additional queue ownership.
    FFFZSPolicyContext Context;
    Context.Command = Command;
    Context.Command.FacilitySpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
        Spec, Command.TargetUpgradeLevel, Command.UpgradeSizeMultiplier);
    Context.Command.ZoneId = Facility->GetZoneId();
    Context.Command.Placement = Facility->GetGridPlacement();
    Context.FacilityActor = Facility;
    Context.CurrentWorldRevision = WorldRevision;
    Context.Validation = Validation;
    FFFZSPolicyDecision Decision;
    if (!RunPolicyPreValidation(Context, Decision))
    {
        return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None
            ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason, Decision.Message);
    }
    TArray<TWeakObjectPtr<UObject>> AppliedPolicies;
    if (!ApplyTentativePolicies(Context, AppliedPolicies, Decision))
    {
        return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None
            ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason, Decision.Message);
    }

    // Actor applies a completely validated candidate; if it cannot apply, all tentative
    // economy/queue changes are rolled back while the old occupancy still remains published.
    if (!Facility->ApplyUpgradeState(Command.TargetUpgradeLevel,
        Command.UpgradeServiceDurationMultiplier, Command.UpgradeSizeMultiplier, Command.UpgradeRevenueMultiplier))
    {
        RollbackPolicies(Context, AppliedPolicies);
        return MakeFailure(EFFZSPlacementFailure::InvalidFacilityState,
            LOCTEXT("UpgradeApplyFailed", "시설 강화 적용에 실패하여 결제와 대기열을 되돌렸습니다."));
    }
    Facility->SetActorTransform(Validation.PlacementTransform);
    FFFZSFacilityRegionOccupancy NewOccupancy;
    NewOccupancy.ZoneId = Facility->GetZoneId();
    NewOccupancy.bIsRoad = Facility->GetFacilitySpec().Category == EFFZSFacilityCategory::Road;
    NewOccupancy.bAllowsFacilitiesOnTop = Facility->GetFacilitySpec().bAllowFacilityPlacementOnTop;
    NewOccupancy.Regions = Validation.ReservedOrientedRects;
    CacheFacilityOccupancy(Facility->GetFacilityInstanceId(), MoveTemp(NewOccupancy));
    CommitPolicies(Context, AppliedPolicies);
    return FinalizeSuccess(Command, Facility, Facility->GetZoneId());
}

FFFZSCommandResult UFFZSWorldSubsystem::ExecuteLifecycleCommand(const FFFZSCommand& Command)
{
    AFFZSFacilityActor* Facility = FindFacility(Command.FacilityInstanceId);
    if (!Facility)
    {
        return MakeFailure(EFFZSPlacementFailure::FacilityNotFound, LOCTEXT("LifecycleNotFound", "Facility를 찾을 수 없습니다."));
    }

    FFFZSPolicyContext Context;
    Context.Command = Command;
    Context.FacilityActor = Facility;
    Context.CurrentWorldRevision = WorldRevision;
    FFFZSPolicyDecision Decision;
    if (!RunPolicyPreValidation(Context, Decision))
    {
        return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason, Decision.Message);
    }
    TArray<TWeakObjectPtr<UObject>> AppliedPolicies;
    if (!ApplyTentativePolicies(Context, AppliedPolicies, Decision))
    {
        return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason, Decision.Message);
    }

    bool bApplied = true;
    if (Command.Type == EFFZSCommandType::Open)
    {
        bApplied = Facility->OpenFacility();
    }
    else if (Command.Type == EFFZSCommandType::Close)
    {
        Facility->CloseFacility(false);
    }
    else if (Command.Type == EFFZSCommandType::EmergencyClose)
    {
        Facility->CloseFacility(true);
    }
    else
    {
        bApplied = false;
    }
    if (!bApplied)
    {
        RollbackPolicies(Context, AppliedPolicies);
        return MakeFailure(EFFZSPlacementFailure::InvalidFacilityState, LOCTEXT("LifecycleState", "현재 Facility 상태에서는 이 Lifecycle 명령을 실행할 수 없습니다."));
    }
    CommitPolicies(Context, AppliedPolicies);
    return FinalizeSuccess(Command, Facility, Facility->GetZoneId());
}

FFFZSCommandResult UFFZSWorldSubsystem::MakeFailure(const EFFZSPlacementFailure Failure, const FText& Message) const
{
    FFFZSCommandResult Result;
    Result.bSucceeded = false;
    Result.FailureReason = Failure;
    Result.Message = Message.IsEmpty() ? UFFZSGridLibrary::PlacementFailureToText(Failure) : Message;
    Result.WorldRevision = WorldRevision;
    return Result;
}

FFFZSCommandResult UFFZSWorldSubsystem::FinalizeSuccess(const FFFZSCommand& Command, AFFZSFacilityActor* Facility, const FName PreviousZoneId)
{
    ++WorldRevision;
    FFFZSCommandResult Result;
    Result.bSucceeded = true;
    Result.Message = LOCTEXT("CommandSucceeded", "Zone/Facility 명령을 Commit했습니다.");
    Result.WorldRevision = WorldRevision;
    Result.FacilityActor = Facility;

    FFFZSWorldChangeEvent Event;
    Event.EventId = FGuid::NewGuid();
    Event.CommandId = Command.CommandId;
    Event.TransactionId = Command.TransactionId;
    Event.Type = Command.Type;
    Event.FacilityInstanceId = Command.FacilityInstanceId;
    Event.PreviousZoneId = PreviousZoneId;
    Event.NewZoneId = Facility ? Facility->GetZoneId() : NAME_None;
    Event.WorldRevision = WorldRevision;
    DeferredWorldChangeEvent = MoveTemp(Event);
    return Result;
}

// PolicyName과 Custom 이외 Domain은 유일하며 정렬 순서로 실행한다.
bool UFFZSWorldSubsystem::RegisterExternalPolicy(UObject* PolicyProvider)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    TGuardValue<bool> PolicyCallbackGuard(bIsInvokingExternalPolicy, true);
    if (!IsValid(PolicyProvider) || !PolicyProvider->GetClass()->ImplementsInterface(UFFZSExternalPolicy::StaticClass()))
    {
        return false;
    }

    const FName PolicyName = IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyName(PolicyProvider);
    const EFFZSPolicyDomain PolicyDomain = IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyDomain(PolicyProvider);
    if (PolicyName.IsNone() || !StaticEnum<EFFZSPolicyDomain>()->IsValidEnumValue(static_cast<int64>(PolicyDomain)))
    {
        UE_LOG(LogFFZoneSystem, Error, TEXT("외부 Zone/Facility Policy에는 유효한 이름과 Domain이 필요합니다."));
        return false;
    }
    for (const TWeakObjectPtr<UObject>& WeakExisting : ExternalPolicies)
    {
        UObject* Existing = WeakExisting.Get();
        if (!Existing)
        {
            continue;
        }
        const FName ExistingName = IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyName(Existing);
        const EFFZSPolicyDomain ExistingDomain = IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyDomain(Existing);
        if (Existing == PolicyProvider)
        {
            return true;
        }
        if (ExistingName == PolicyName || (PolicyDomain != EFFZSPolicyDomain::Custom && ExistingDomain == PolicyDomain))
        {
            UE_LOG(LogFFZoneSystem, Error, TEXT("중복된 외부 Policy 이름/Domain: %s (%d)"),
                *PolicyName.ToString(), static_cast<int32>(PolicyDomain));
            return false;
        }
    }

    ExternalPolicies.Add(PolicyProvider);
    ExternalPolicies.Sort([](const TWeakObjectPtr<UObject>& A, const TWeakObjectPtr<UObject>& B)
        {
            UObject* PolicyA = A.Get();
            UObject* PolicyB = B.Get();
            if (!PolicyA || !PolicyB)
            {
                return IsValid(PolicyA) && !IsValid(PolicyB);
            }
            const EFFZSPolicyDomain DomainA = IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyDomain(PolicyA);
            const EFFZSPolicyDomain DomainB = IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyDomain(PolicyB);
            if (DomainA != DomainB)
            {
                return static_cast<uint8>(DomainA) < static_cast<uint8>(DomainB);
            }
            return IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyName(PolicyA).LexicalLess(
                IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyName(PolicyB));
        });
    UE_LOG(LogFFZoneSystem, Log, TEXT("Registered External Policy %s in domain %d."),
        *PolicyName.ToString(), static_cast<int32>(PolicyDomain));
    return true;
}

bool UFFZSWorldSubsystem::ValidateIntegrationReadiness(TArray<FText>& OutErrors, const bool bRequireAllProductionDomains) const
{
    OutErrors.Reset();
    if (bIsInvokingExternalPolicy)
    {
        OutErrors.Add(LOCTEXT("ReentrantPolicyReadiness", "외부 Policy 준비 상태를 재귀적으로 평가할 수 없습니다."));
        return false;
    }
    TGuardValue<bool> PolicyCallbackGuard(bIsInvokingExternalPolicy, true);
    TSet<FName> Names;
    TSet<EFFZSPolicyDomain> Domains;
    Names.Reserve(ExternalPolicies.Num());
    Domains.Reserve(ExternalPolicies.Num());
    const TArray<TWeakObjectPtr<UObject>> PolicySnapshot = ExternalPolicies;
    for (const TWeakObjectPtr<UObject>& WeakPolicy : PolicySnapshot)
    {
        UObject* Policy = WeakPolicy.Get();
        if (!Policy || !Policy->GetClass()->ImplementsInterface(UFFZSExternalPolicy::StaticClass()))
        {
            OutErrors.Add(LOCTEXT("InvalidPolicyProvider", "외부 Policy 레지스트리에 잘못된 Provider가 있습니다."));
            continue;
        }
        const FName Name = IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyName(Policy);
        const EFFZSPolicyDomain Domain = IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyDomain(Policy);
        if (Name.IsNone() || Names.Contains(Name) || (Domain != EFFZSPolicyDomain::Custom && Domains.Contains(Domain)))
        {
            OutErrors.Add(LOCTEXT("DuplicatePolicyProvider", "외부 Policy 이름과 Custom이 아닌 Domain은 고유해야 합니다."));
        }
        Names.Add(Name);
        Domains.Add(Domain);
    }

    if (bRequireAllProductionDomains)
    {
        const EFFZSPolicyDomain RequiredDomains[] = {
            EFFZSPolicyDomain::Navigation,
            EFFZSPolicyDomain::Economy,
            EFFZSPolicyDomain::SimulationTime,
            EFFZSPolicyDomain::UserInterface,
            EFFZSPolicyDomain::Safety,
            EFFZSPolicyDomain::Experience
        };
        for (const EFFZSPolicyDomain RequiredDomain : RequiredDomains)
        {
            if (!Domains.Contains(RequiredDomain))
            {
                OutErrors.Add(FText::Format(LOCTEXT("MissingPolicyDomain", "필수 외부 Policy Domain {0}이 등록되어 있지 않습니다."),
                    FText::AsNumber(static_cast<int32>(RequiredDomain))));
            }
        }
    }
    return OutErrors.IsEmpty();
}

bool UFFZSWorldSubsystem::UnregisterExternalPolicy(UObject* PolicyProvider)
{
    if (IsAuthorityMutationBlocked() || !PolicyProvider)
    {
        return false;
    }
    return ExternalPolicies.Remove(PolicyProvider) > 0;
}

bool UFFZSWorldSubsystem::SetFacilityLighting(const FGuid FacilityInstanceId, const FFFZSFacilityLightingState& Settings, FText& OutFailureReason)
{
    OutFailureReason = FText::GetEmpty();
    if (IsAuthorityMutationBlocked())
    {
        OutFailureReason = LOCTEXT("LightingMutationBusy", "시설 변경을 처리 중입니다. 잠시 후 다시 시도해 주세요.");
        return false;
    }
    AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    if (!IsValid(Facility) || Facility->IsActorBeingDestroyed() || Facility->GetFacilityState() == EFFZSFacilityState::Removed || !Facility->HasLightingRig())
    {
        OutFailureReason = LOCTEXT("LightingFacilityMissing", "조명을 설정할 수 있는 설치된 시설이 없습니다.");
        return false;
    }
    if (!Settings.IsValid())
    {
        OutFailureReason = LOCTEXT("LightingStateInvalid", "조명 밝기는 0~500%, 색상은 0~1 범위여야 합니다.");
        return false;
    }
    Facility->ApplyFacilityLightingOverride(Settings, true);
    TGuardValue<bool> NotificationGuard(bIsBroadcastingLightingChange, true);
    OnFacilityLightingChanged.Broadcast(FacilityInstanceId);
    return true;
}

bool UFFZSWorldSubsystem::ResetFacilityLighting(const FGuid FacilityInstanceId, FText& OutFailureReason)
{
    OutFailureReason = FText::GetEmpty();
    if (IsAuthorityMutationBlocked())
    {
        OutFailureReason = LOCTEXT("LightingMutationBusy", "시설 변경을 처리 중입니다. 잠시 후 다시 시도해 주세요.");
        return false;
    }
    AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    if (!IsValid(Facility) || Facility->IsActorBeingDestroyed() || Facility->GetFacilityState() == EFFZSFacilityState::Removed || !Facility->HasLightingRig())
    {
        OutFailureReason = LOCTEXT("LightingFacilityMissing", "조명을 설정할 수 있는 설치된 시설이 없습니다.");
        return false;
    }
    Facility->ApplyFacilityLightingOverride(Facility->GetBaseFacilitySpec().LightingSettings.GetDefaultState(), false);
    TGuardValue<bool> NotificationGuard(bIsBroadcastingLightingChange, true);
    OnFacilityLightingChanged.Broadcast(FacilityInstanceId);
    return true;
}

bool UFFZSWorldSubsystem::SetFacilityQueueBlocked(const FGuid FacilityInstanceId, const bool bBlocked)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    UFFZSFacilityQueueComponent* Queue = Facility ? Facility->GetMutableQueueComponent() : nullptr;
    if (!Facility || Facility->GetFacilityState() != EFFZSFacilityState::Active || !Queue)
    {
        return false;
    }
    Queue->SetBlocked(bBlocked);
    return bBlocked ? Queue->GetQueueState() == EFFZSQueueState::Blocked
                    : Queue->GetQueueState() != EFFZSQueueState::Blocked;
}

EFFZSQueueJoinResult UFFZSWorldSubsystem::JoinFacilityQueue(const FGuid FacilityInstanceId, const FName AgentId, AActor* AgentActor, FGuid& OutTicketId)
{
    OutTicketId.Invalidate();
    if (IsAuthorityMutationBlocked())
    {
        return EFFZSQueueJoinResult::NotOpen;
    }
    if (AgentId.IsNone() ||
        (AgentActor && (!AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()) ||
            IFFZSQueueAgent::Execute_GetFFZoneSystemAgentId(AgentActor) != AgentId)))
    {
        return EFFZSQueueJoinResult::InvalidAgent;
    }
    if (AgentQueueOwnership.Contains(AgentId))
    {
        return EFFZSQueueJoinResult::DuplicateAgent;
    }

    AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    if (!Facility)
    {
        return EFFZSQueueJoinResult::FacilityNotFound;
    }
    if (Facility->GetFacilityState() != EFFZSFacilityState::Active || !Facility->GetQueueComponent())
    {
        return EFFZSQueueJoinResult::NotOpen;
    }

    // Delegate/Interface가 동기적으로 실행되어 이 Subsystem에 재진입할 수 있으므로 JoinQueue 전에 예약한다.
    AgentQueueOwnership.Add(AgentId, FacilityInstanceId);
    const EFFZSQueueJoinResult Result = Facility->GetMutableQueueComponent()->JoinQueue(AgentId, AgentActor, OutTicketId);
    if (Result != EFFZSQueueJoinResult::Joined)
    {
        if (const FGuid* Owner = AgentQueueOwnership.Find(AgentId); Owner && *Owner == FacilityInstanceId)
        {
            AgentQueueOwnership.Remove(AgentId);
        }
    }
    return Result;
}

EFFZSQueueJoinResult UFFZSWorldSubsystem::JoinFacilityQueueByActor(const FGuid FacilityInstanceId, AActor* AgentActor, FGuid& OutTicketId)
{
    if (!AgentActor || !AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()))
    {
        OutTicketId.Invalidate();
        return EFFZSQueueJoinResult::InvalidAgent;
    }
    return JoinFacilityQueue(FacilityInstanceId, IFFZSQueueAgent::Execute_GetFFZoneSystemAgentId(AgentActor), AgentActor, OutTicketId);
}

bool UFFZSWorldSubsystem::LeaveCurrentFacilityQueue(const FName AgentId, const EFFZSQueueReleaseReason Reason)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    const FGuid* FacilityId = AgentQueueOwnership.Find(AgentId);
    if (!FacilityId)
    {
        return false;
    }
    AFFZSFacilityActor* Facility = FindFacility(*FacilityId);
    if (!Facility)
    {
        AgentQueueOwnership.Remove(AgentId);
        return false;
    }
    return Facility->GetMutableQueueComponent()->LeaveQueue(AgentId, Reason);
}

void UFFZSWorldSubsystem::HandleAgentReleased(const FName AgentId, FGuid TicketId, EFFZSQueueReleaseReason Reason)
{
    AgentQueueOwnership.Remove(AgentId);
}

bool UFFZSWorldSubsystem::ReportQueueSlotArrival(const FName AgentId, const FVector ActualWorldLocation)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    if (AFFZSFacilityActor* Facility = FindFacility(FindAgentQueueFacility(AgentId)))
    {
        return Facility->GetMutableQueueComponent()->ReportAgentArrivedAtQueueSlot(AgentId, ActualWorldLocation);
    }
    return false;
}

bool UFFZSWorldSubsystem::ReportServicePointArrival(const FName AgentId, const FVector ActualWorldLocation)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    if (AFFZSFacilityActor* Facility = FindFacility(FindAgentQueueFacility(AgentId)))
    {
        return Facility->GetMutableQueueComponent()->ReportAgentArrivedAtServicePoint(AgentId, ActualWorldLocation);
    }
    return false;
}

bool UFFZSWorldSubsystem::ReportExitPointArrival(const FName AgentId, const FVector ActualWorldLocation)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    if (AFFZSFacilityActor* Facility = FindFacility(FindAgentQueueFacility(AgentId)))
    {
        return Facility->GetMutableQueueComponent()->ReportAgentArrivedAtExit(AgentId, ActualWorldLocation);
    }
    return false;
}

bool UFFZSWorldSubsystem::ReportQueueMovementFailed(const FName AgentId)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    if (AFFZSFacilityActor* Facility = FindFacility(FindAgentQueueFacility(AgentId)))
    {
        return Facility->GetMutableQueueComponent()->ReportAgentMovementFailed(AgentId);
    }
    return false;
}

bool UFFZSWorldSubsystem::RebindQueueAgentActor(const FName AgentId, AActor* AgentActor)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    if (AFFZSFacilityActor* Facility = FindFacility(FindAgentQueueFacility(AgentId)))
    {
        return Facility->GetMutableQueueComponent()->RebindAgentActor(AgentId, AgentActor);
    }
    return false;
}

FGuid UFFZSWorldSubsystem::FindAgentQueueFacility(const FName AgentId) const
{
    if (const FGuid* FacilityId = AgentQueueOwnership.Find(AgentId))
    {
        return *FacilityId;
    }
    return FGuid();
}

// 이전 Zone에는 Hysteresis를 적용하고 나머지는 ZoneId 순서로 결정한다.
FName UFFZSWorldSubsystem::ResolveZoneAtWorldLocation(const FVector& WorldLocation, const FName PreviousZoneId) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FFZS_ResolveRegion);
    const float HysteresisCm = GetDefault<UFFZSDeveloperSettings>()->ZoneMembershipHysteresisCm;
    if (AFFZSGameplayZone* Previous = FindZone(PreviousZoneId); Previous && Previous->ContainsWorldPointWithHysteresis(WorldLocation, HysteresisCm))
    {
        return PreviousZoneId;
    }

    TArray<FName> ZoneIds;
    if (FindBuildArea() && RegionSpatialIndex && GetWorld() && GetWorld()->IsGameWorld())
    {
        const FVector2D Point(WorldLocation.X, WorldLocation.Y);
        TArray<FGuid> Candidates;
        RegionSpatialIndex->Query(FBox2D(Point - FVector2D(0.1), Point + FVector2D(0.1)), Candidates);
        for (const FGuid Id : Candidates)
        {
            const int32 Index = static_cast<int32>(Id.D) - 1;
            if (SpatialRegionIds.IsValidIndex(Index))
            {
                ZoneIds.Add(SpatialRegionIds[Index]);
            }
        }
    }
    else
    {
        ZonesById.GenerateKeyArray(ZoneIds);
        ZoneIds.Sort(FNameLexicalLess());
    }
    for (const FName ZoneId : ZoneIds)
    {
        if (AFFZSGameplayZone* Zone = FindZone(ZoneId); Zone && Zone->ContainsWorldPoint(WorldLocation))
        {
            return ZoneId;
        }
    }
    return NAME_None;
}

// 등록 순서와 무관하게 소비 측이 항상 같은 순서를 보도록 BuildReadModel·ResolveZoneAtWorldLocation과
// 같은 사전순으로 맞춘다. 소멸한 Actor의 키는 내보내지 않는다.
TArray<FName> UFFZSWorldSubsystem::GetRegisteredZoneIds() const
{
    TArray<FName> ZoneIds;
    ZoneIds.Reserve(ZonesById.Num());
    for (const TPair<FName, TWeakObjectPtr<AFFZSGameplayZone>>& Pair : ZonesById)
    {
        if (Pair.Value.IsValid())
        {
            ZoneIds.Add(Pair.Key);
        }
    }
    ZoneIds.Sort(FNameLexicalLess());
    return ZoneIds;
}

// 반환값은 "이 표본이 밀도 상태에 반영됐는가"다. 호출자는 false일 때 DeltaSeconds를 이월해야 한다 —
// UpdatePopulationSample이 유지 시간을 누적으로 판정하므로, 흘려보낸 구간만큼 승격이 늦어진다.
bool UFFZSWorldSubsystem::ReportZonePopulation(const FName ZoneId, const int32 Population, const float DeltaSeconds)
{
    if (IsAuthorityMutationBlocked())
    {
        return false;
    }
    AFFZSGameplayZone* Zone = FindZone(ZoneId);
    if (!Zone)
    {
        return false;
    }
    Zone->UpdatePopulationSample(Population, DeltaSeconds);
    return true;
}

FFFZSReadModel UFFZSWorldSubsystem::BuildReadModel() const
{
    FFFZSReadModel Result;
    Result.WorldRevision = WorldRevision;
    Result.Zones.Reserve(ZonesById.Num());
    Result.Facilities.Reserve(FacilitiesById.Num());
    for (const TPair<FName, TWeakObjectPtr<AFFZSGameplayZone>>& Pair : ZonesById)
    {
        if (const AFFZSGameplayZone* Zone = Pair.Value.Get())
        {
            Result.Zones.Add(Zone->GetReadModel());
        }
    }
    for (const TPair<FGuid, TWeakObjectPtr<AFFZSFacilityActor>>& Pair : FacilitiesById)
    {
        if (const AFFZSFacilityActor* Facility = Pair.Value.Get())
        {
            Result.Facilities.Add(Facility->GetReadModel());
        }
    }
    Result.Zones.Sort([](const FFFZSZoneReadModel& A, const FFFZSZoneReadModel& B) { return A.ZoneId.LexicalLess(B.ZoneId); });
    Result.Facilities.Sort([](const FFFZSFacilityReadModel& A, const FFFZSFacilityReadModel& B) { return A.FacilityInstanceId < B.FacilityInstanceId; });
    return Result;
}

FFFZSWorldSnapshot UFFZSWorldSubsystem::CaptureWorldSnapshot() const
{
    FFFZSWorldSnapshot Snapshot;
    Snapshot.SchemaVersion = CurrentSnapshotSchemaVersion;
    Snapshot.WorldRevision = WorldRevision;
    Snapshot.CurrentPhase = CurrentPhase;
    Snapshot.Facilities.Reserve(FacilitiesById.Num());
    Snapshot.CommandReceipts.Reserve(CommandReceipts.Num());
    for (const TPair<FGuid, TWeakObjectPtr<AFFZSFacilityActor>>& Pair : FacilitiesById)
    {
        if (const AFFZSFacilityActor* Facility = Pair.Value.Get())
        {
            FFFZSFacilityInstanceSnapshot& Saved = Snapshot.Facilities.Add_GetRef(
                Facility->CaptureInstanceSnapshot());
            UFFZSGridLibrary::NormalizePlacement(Saved.Placement);
        }
    }
    Snapshot.Facilities.Sort([](const FFFZSFacilityInstanceSnapshot& A, const FFFZSFacilityInstanceSnapshot& B)
        {
            return A.FacilityInstanceId < B.FacilityInstanceId;
        });

    for (const TPair<FGuid, FFFZSCommandReceipt>& Pair : CommandReceipts)
    {
        const FFFZSCommandReceipt& Receipt = Pair.Value;
        if (Receipt.bInFlight)
        {
            continue;
        }

        FFFZSCommandReceiptSnapshot& Saved = Snapshot.CommandReceipts.AddDefaulted_GetRef();
        Saved.CommandId = Pair.Key;
        Saved.PayloadFingerprint = Receipt.PayloadFingerprint;
        Saved.bSucceeded = Receipt.Result.bSucceeded;
        Saved.FailureReason = Receipt.Result.FailureReason;
        Saved.Message = Receipt.Result.Message;
        Saved.ResultWorldRevision = Receipt.Result.WorldRevision;
        Saved.FacilityInstanceId = Receipt.FacilityInstanceId;
        Saved.bResultReferencesFacility = CommandReceiptFacilityReferences.Contains(Pair.Key);
    }
    Snapshot.CommandReceipts.Sort([](const FFFZSCommandReceiptSnapshot& A, const FFFZSCommandReceiptSnapshot& B)
        {
            return A.CommandId < B.CommandId;
        });
    return Snapshot;
}

bool UFFZSWorldSubsystem::ValidateSnapshotBeforeRestore(const FFFZSWorldSnapshot& Snapshot, TArray<FText>& OutErrors) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FFZS_ValidateSnapshot);
    OutErrors.Reset();
    if (Snapshot.SchemaVersion != CurrentSnapshotSchemaVersion)
    {
        OutErrors.Add(Snapshot.SchemaVersion == 2
            ? LOCTEXT("SnapshotSchemaV2", "Zone/Facility 스냅샷 SchemaVersion 2에는 저장된 CurrentPhase가 없어 안전하게 복원할 수 없습니다. SchemaVersion 7에서 사용하기 전에 오프라인 마이그레이션하십시오.")
            : LOCTEXT("SnapshotSchema", "지원하지 않는 Zone/Facility 스냅샷 SchemaVersion입니다. 이 Runtime은 SchemaVersion 3/4/5/6을 명시적으로 7로 변환한 뒤 검증합니다."));
        return false;
    }
    if (Snapshot.WorldRevision < 0 || Snapshot.WorldRevision == MAX_int64)
    {
        OutErrors.Add(LOCTEXT("SnapshotRevision", "Zone/Facility 스냅샷의 WorldRevision은 음수일 수 없습니다."));
    }
    if (!IsKnownChangePhase(Snapshot.CurrentPhase))
    {
        OutErrors.Add(LOCTEXT("SnapshotPhase", "Zone/Facility 스냅샷의 CurrentPhase가 잘못되었습니다."));
    }

    TSet<FGuid> FacilityIds;
    TMap<FGuid, FFFZSFacilityRegionOccupancy> ValidatedOccupancy;
    FFFZSSpatialHash2D SnapshotSpatialIndex;
    const AFFZSBuildArea* BuildArea = FindBuildArea();
    TSet<FName> Agents;
    const float CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    TMap<const AFFZSGameplayZone*, TUniquePtr<FZoneCellOwnershipCache>> ZoneOwnershipCaches;
    // 모든 시설 스냅샷이 함께 쓰는 큐 검사용 컴포넌트다. 처음 필요할 때만 만든다.
    TStrongObjectPtr<UFFZSFacilityQueueComponent> QueueValidator;
    FacilityIds.Reserve(Snapshot.Facilities.Num());
    ValidatedOccupancy.Reserve(Snapshot.Facilities.Num());
    Agents.Reserve(Snapshot.Facilities.Num());
    ZoneOwnershipCaches.Reserve(FMath::Min(Snapshot.Facilities.Num(), ZonesById.Num()));

    for (const FFFZSFacilityInstanceSnapshot& Saved : Snapshot.Facilities)
    {
        TArray<FText> SpecErrors;
        if (Saved.bHasLightingOverride && !Saved.LightingState.IsValid())
        {
            OutErrors.Add(LOCTEXT("SnapshotLightingStateInvalid", "스냅샷 시설 조명의 밝기 또는 색상이 유효하지 않습니다."));
        }
        if (!Saved.FacilityInstanceId.IsValid() || FacilityIds.Contains(Saved.FacilityInstanceId) || !Saved.FacilitySpec.IsValid(SpecErrors))
        {
            OutErrors.Add(LOCTEXT("SnapshotFacility", "스냅샷에 잘못되었거나 중복된 Facility가 있습니다."));
            continue;
        }
        FacilityIds.Add(Saved.FacilityInstanceId);
        if (!AFFZSFacilityActor::IsValidUpgradeState(Saved.UpgradeLevel, Saved.UpgradeServiceDurationMultiplier,
            Saved.UpgradeSizeMultiplier, Saved.UpgradeRevenueMultiplier))
        {
            OutErrors.Add(LOCTEXT("SnapshotUpgrade", "스냅샷의 시설 강화 단계 또는 배수가 유효하지 않습니다."));
            continue;
        }
        const FFFZSFacilitySpec EffectiveSpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
            Saved.FacilitySpec, Saved.UpgradeLevel, Saved.UpgradeSizeMultiplier);
        if (!EffectiveSpec.IsValid(SpecErrors))
        {
            OutErrors.Append(SpecErrors);
            continue;
        }
        if (!StaticEnum<EFFZSFacilityState>()->IsValidEnumValue(static_cast<int64>(Saved.State)) ||
            Saved.State == EFFZSFacilityState::Removed)
        {
            OutErrors.Add(LOCTEXT("SnapshotFacilityState", "스냅샷의 Facility 상태가 잘못되었거나, 등록된 상태에서 Removed일 수 없습니다."));
        }
        if (Saved.Placement.YawCentidegrees !=
                UFFZSGridLibrary::NormalizeYawCentidegrees(Saved.Placement.YawCentidegrees) ||
            Saved.Placement.QuarterTurns != 0)
        {
            OutErrors.Add(LOCTEXT("SnapshotPlacementRotation", "Schema 7 스냅샷 배치의 Yaw는 0..359.99도로 정규화되고 Legacy QuarterTurns는 0이어야 합니다."));
        }

        const AFFZSGameplayZone* Zone = FindZone(Saved.ZoneId);
        if (!Zone)
        {
            OutErrors.Add(FText::Format(LOCTEXT("SnapshotZone", "스냅샷이 존재하지 않는 Zone {0}을 참조합니다."), FText::FromName(Saved.ZoneId)));
            continue;
        }
        TArray<FText> ZoneErrors;
        if (!Zone->ValidateZoneData(ZoneErrors))
        {
            OutErrors.Append(ZoneErrors);
        }
        if (!Zone->IsCategoryAllowed(Saved.FacilitySpec.Category) ||
            (!Saved.FacilitySpec.AllowedZoneIds.IsEmpty() && !Saved.FacilitySpec.AllowedZoneIds.Contains(Saved.ZoneId)))
        {
            OutErrors.Add(LOCTEXT("SnapshotCategory", "스냅샷 Facility의 Category/AllowedZoneIds가 대상 Gameplay Zone을 허용하지 않습니다."));
        }

        if (Saved.WorldTransform.ContainsNaN() || !Saved.WorldTransform.GetScale3D().Equals(FVector::OneVector, 0.001))
        {
            OutErrors.Add(LOCTEXT("SnapshotTransform", "스냅샷 Facility의 Transform은 유한해야 하며 Scale은 1이어야 합니다."));
        }
        else
        {
            const FTransform Expected = BuildPlacementTransform(*Zone, EffectiveSpec, Saved.Placement);
            const FVector SavedLocation = Saved.WorldTransform.GetLocation();
            const FVector ExpectedLocation = Expected.GetLocation();
            const double HorizontalErrorCm = FVector2D::Distance(
                FVector2D(SavedLocation.X, SavedLocation.Y), FVector2D(ExpectedLocation.X, ExpectedLocation.Y));
            const double RotationErrorRadians = Saved.WorldTransform.GetRotation().AngularDistance(Expected.GetRotation());
            if (HorizontalErrorCm > 0.1 || RotationErrorRadians > FMath::DegreesToRadians(0.01))
            {
                OutErrors.Add(LOCTEXT("SnapshotGridTransform", "스냅샷 Facility의 XY/Yaw가 1cm 배치 Grid와 일치하지 않습니다."));
            }
        }

        TArray<FFFZSOrientedRect> FootprintRects;
        TArray<FFFZSOrientedRect> ClearanceRects;
        TArray<FFFZSOrientedRect> QueueRects;
        TArray<FFFZSOrientedRect> ReservedRects;
        UFFZSGridLibrary::BuildPlacementOrientedRegions(
            EffectiveSpec, Saved.Placement,
            FootprintRects, ClearanceRects, QueueRects, ReservedRects);
        if (ReservedRects.IsEmpty())
        {
            // 빈 배열은 정상 시설 영역이 아니라 좌표 변환/오버플로의 fail-closed 신호다.
            OutErrors.Add(LOCTEXT(
                "SnapshotEmptyCompressedRegion",
                "스냅샷 Facility에서 완전한 압축 1cm 배치 영역을 만들 수 없습니다."));
        }
        TUniquePtr<FZoneCellOwnershipCache>& ZoneOwnershipCache = ZoneOwnershipCaches.FindOrAdd(Zone);
        if (!BuildArea && !ZoneOwnershipCache)
        {
            ZoneOwnershipCache = MakeUnique<FZoneCellOwnershipCache>(ZonesById, *Zone);
        }

        bool bOverlapsFacility = false;
        FFFZSFacilityRegionOccupancy Record;
        Record.ZoneId = Saved.ZoneId;
        Record.bIsRoad = EffectiveSpec.Category == EFFZSFacilityCategory::Road;
        Record.bAllowsFacilitiesOnTop = EffectiveSpec.bAllowFacilityPlacementOnTop;
        Record.Regions = ReservedRects;
        Record.WorldRegions = ToWorldRegions(Saved.ZoneId, ReservedRects);
        Record.WorldBounds = GetRegionBounds(Record.WorldRegions);
        TArray<FGuid> Candidates;
        SnapshotSpatialIndex.Query(Record.WorldBounds, Candidates);
        for (const FGuid Id : Candidates)
        {
            const FFFZSFacilityRegionOccupancy& Existing = ValidatedOccupancy.FindChecked(Id);
            if (Existing.BlocksLayer(Record.bIsRoad, Record.bAllowsFacilitiesOnTop) && IsOccupancyRelevant(Saved.ZoneId, Existing) &&
                UFFZSGridLibrary::DoOrientedRectSetsOverlap(Record.WorldRegions, Existing.WorldRegions))
            {
                bOverlapsFacility = true;
                break;
            }
        }
        if (bOverlapsFacility)
        {
            OutErrors.Add(LOCTEXT(
                "SnapshotOverlap",
                "스냅샷 Facility들이 권위 압축 배치 Grid에서 서로 겹칩니다."));
        }
        SnapshotSpatialIndex.InsertOrUpdate(Saved.FacilityInstanceId, Record.WorldBounds);
        ValidatedOccupancy.Add(Saved.FacilityInstanceId, MoveTemp(Record));

        bool bOutsideOrAmbiguousZone = false;
        for (const FFFZSOrientedRect& Rect : ReservedRects)
        {
            const bool bInside = BuildArea
                ? BuildArea->ContainsWorldOrientedRect(TransformRegion(Rect, Zone->GetActorTransform()), Zone->GetActorLocation().Z)
                : IsOrientedRectOwnedByExactlyOneZone(*ZoneOwnershipCache, Rect, CellSizeCm);
            if (!bInside)
            {
                bOutsideOrAmbiguousZone = true;
                break;
            }
        }
        if (bOutsideOrAmbiguousZone)
        {
            OutErrors.Add(LOCTEXT(
                "SnapshotOutside",
                "스냅샷 Facility 영역은 정확히 하나의 Gameplay Zone 안에 완전히 포함되어야 합니다."));
        }

        FFFZSPlacementValidation ProtectedValidation;
        ProtectedValidation.ReservedOrientedRects = ReservedRects;
        ProtectedValidation.PlacementTransform = Saved.WorldTransform;
        ValidateProtectedAreas(Saved.ZoneId, *Zone, EffectiveSpec, ProtectedValidation);
        if (!ProtectedValidation.Reasons.IsEmpty())
        {
            OutErrors.Add(ProtectedValidation.Summary.IsEmpty()
                ? LOCTEXT("SnapshotProtectedArea", "스냅샷 Facility가 보호 영역과 겹칩니다.")
                : ProtectedValidation.Summary);
        }
        // [최적화] 검사용 컴포넌트를 시설마다 새로 만들지 않고 하나를 재사용한다.
        // Configure와 RestoreSnapshot이 큐 상태를 통째로 덮어쓰므로 이전 시설의 값이 남지 않는다.
        // 예전에는 시설 수만큼 임시 UObject가 생겨 저장 검증마다 GC 부담이 커졌다.
        if (!QueueValidator.IsValid())
        {
            QueueValidator.Reset(NewObject<UFFZSFacilityQueueComponent>());
        }
        QueueValidator->Configure(AFFZSFacilityActor::BuildEffectiveQueueRules(
            EffectiveSpec.QueueRules, Saved.UpgradeServiceDurationMultiplier));
        TArray<FText> QueueErrors;
        if (!QueueValidator->RestoreSnapshot(Saved.Queue, QueueErrors))
        {
            OutErrors.Append(QueueErrors);
        }

        bool bStateCompatible = false;
        const bool bQueueEnumsValid =
            StaticEnum<EFFZSQueueState>()->IsValidEnumValue(static_cast<int64>(Saved.Queue.State)) &&
            StaticEnum<EFFZSQueueState>()->IsValidEnumValue(static_cast<int64>(Saved.Queue.StateBeforeBlocked));
        if (bQueueEnumsValid)
        {
            switch (Saved.State)
            {
            case EFFZSFacilityState::Planned:
            case EFFZSFacilityState::Validated:
            case EFFZSFacilityState::MovePending:
                bStateCompatible = Saved.Queue.State == EFFZSQueueState::Closed;
                break;
            case EFFZSFacilityState::Active:
                if (EffectiveSpec.Category == EFFZSFacilityCategory::Lighting || EffectiveSpec.Category == EFFZSFacilityCategory::Road)
                {
                    bStateCompatible = Saved.Queue.State == EFFZSQueueState::Closed &&
                        Saved.Queue.Entries.IsEmpty();
                    break;
                }
                bStateCompatible = !Saved.Queue.bDrainRequested &&
                    (Saved.Queue.State == EFFZSQueueState::Open ||
                     Saved.Queue.State == EFFZSQueueState::Full ||
                     (Saved.Queue.State == EFFZSQueueState::Blocked &&
                      (Saved.Queue.StateBeforeBlocked == EFFZSQueueState::Open ||
                       Saved.Queue.StateBeforeBlocked == EFFZSQueueState::Full)));
                break;
            case EFFZSFacilityState::Closed:
                bStateCompatible = Saved.Queue.State == EFFZSQueueState::Draining ||
                    Saved.Queue.State == EFFZSQueueState::Closed;
                break;
            default:
                break;
            }
        }
        if (!bStateCompatible)
        {
            OutErrors.Add(LOCTEXT("SnapshotStateCompatibility", "FacilityState와 QueueState의 조합이 Runtime에서 유효하지 않습니다."));
        }

        for (const FFFZSQueueEntrySnapshot& Entry : Saved.Queue.Entries)
        {
            if (Entry.AgentId.IsNone() || Agents.Contains(Entry.AgentId))
            {
                OutErrors.Add(LOCTEXT("SnapshotAgent", "World 스냅샷에 동일한 Agent가 두 번 이상 있습니다."));
            }
            Agents.Add(Entry.AgentId);
        }
    }

    TSet<FGuid> ReceiptIds;
    ReceiptIds.Reserve(Snapshot.CommandReceipts.Num());
    for (const FFFZSCommandReceiptSnapshot& SavedReceipt : Snapshot.CommandReceipts)
    {
        const bool bInvalidOutcome =
            (SavedReceipt.bSucceeded && SavedReceipt.FailureReason != EFFZSPlacementFailure::None) ||
            (!SavedReceipt.bSucceeded && SavedReceipt.FailureReason == EFFZSPlacementFailure::None);
        if (!SavedReceipt.CommandId.IsValid() || ReceiptIds.Contains(SavedReceipt.CommandId) ||
            SavedReceipt.PayloadFingerprint.IsEmpty() || SavedReceipt.ResultWorldRevision < 0 ||
            SavedReceipt.ResultWorldRevision > Snapshot.WorldRevision || bInvalidOutcome ||
            (SavedReceipt.bResultReferencesFacility && !SavedReceipt.FacilityInstanceId.IsValid()))
        {
            OutErrors.Add(LOCTEXT("SnapshotReceipt", "스냅샷에 잘못되었거나 중복된 Command 영수증이 있습니다."));
        }
        ReceiptIds.Add(SavedReceipt.CommandId);
    }
    return OutErrors.IsEmpty();
}

// 새 Actor와 레지스트리를 staging해 실패 시 기존 권위 상태를 유지한다.
bool UFFZSWorldSubsystem::RestoreWorldSnapshot(const FFFZSWorldSnapshot& Snapshot, TArray<FText>& OutErrors)
{
    OutErrors.Reset();
    if (!IsInGameThread())
    {
        OutErrors.Add(LOCTEXT("RestoreGameThreadOnly", "Zone/Facility 스냅샷은 Game Thread에서 복원해야 합니다."));
        return false;
    }
    if (bIsRestoringSnapshot)
    {
        OutErrors.Add(LOCTEXT("RestoreReentrant", "Zone/Facility 스냅샷 복원이 이미 진행 중입니다."));
        return false;
    }
    if (bIsInvokingExternalPolicy)
    {
        OutErrors.Add(LOCTEXT("RestoreDuringPolicyCallback", "외부 Policy Callback에서는 스냅샷 복원을 시작할 수 없습니다."));
        return false;
    }
    if (InFlightCommandId.IsValid())
    {
        OutErrors.Add(LOCTEXT("RestoreDuringCommand", "Zone/Facility 명령이 이미 실행 중이므로 스냅샷 복원이 거부되었습니다."));
        return false;
    }

    // 복원 경로에서 호출되는 모든 Callback이 안전하게 거부되도록 마이그레이션, 검증 및 준비 단계를 보호한다.
    TGuardValue<bool> RestoreGuard(bIsRestoringSnapshot, true);

    FFFZSWorldSnapshot MigratedSnapshot;
    const FFFZSWorldSnapshot* EffectiveSnapshot = &Snapshot;
    if (Snapshot.SchemaVersion == LegacySnapshotSchemaVersion)
    {
        if (!TryMigrateLegacySnapshotV3(Snapshot, MigratedSnapshot, OutErrors))
        {
            return false;
        }
        EffectiveSnapshot = &MigratedSnapshot;
    }
    else if (Snapshot.SchemaVersion == CardinalSnapshotSchemaVersion)
    {
        if (!TryMigrateCardinalSnapshotV4(Snapshot, MigratedSnapshot, OutErrors))
        {
            return false;
        }
        EffectiveSnapshot = &MigratedSnapshot;
    }

    if (Snapshot.SchemaVersion == LegacySnapshotSchemaVersion ||
        Snapshot.SchemaVersion == CardinalSnapshotSchemaVersion ||
        Snapshot.SchemaVersion == FreeYawSnapshotSchemaVersion ||
        Snapshot.SchemaVersion == InstanceUpgradeSnapshotSchemaVersion)
    {
        if (EffectiveSnapshot == &Snapshot)
        {
            MigratedSnapshot = Snapshot;
        }
        MigratedSnapshot.SchemaVersion = CurrentSnapshotSchemaVersion;
        for (FFFZSFacilityInstanceSnapshot& Saved : MigratedSnapshot.Facilities)
        {
            Saved.UpgradeSizeMultiplier = 1.0f;
            Saved.UpgradeRevenueMultiplier = 1.0f;
            Saved.FacilitySpec.bUsesFacilityLevels = false;
            Saved.FacilitySpec.UpgradeTiers.Reset();
            if (Snapshot.SchemaVersion < InstanceUpgradeSnapshotSchemaVersion)
            {
                Saved.UpgradeLevel = 1;
                Saved.UpgradeServiceDurationMultiplier = 1.0f;
            }
            // Schema 6 already charged for this instance's upgrade. Preserve its paid effect
            // and level visibility even though that old save has no authored tier array.
            if (Saved.UpgradeLevel > 1)
            {
                Saved.FacilitySpec.bUsesFacilityLevels = true;
            }
        }
        EffectiveSnapshot = &MigratedSnapshot;
    }

    if (!ValidateSnapshotBeforeRestore(*EffectiveSnapshot, OutErrors))
    {
        return false;
    }
    return RestoreWorldSnapshotUnchecked(*EffectiveSnapshot, OutErrors);
}

// ID 없이 Spawn해 BeginPlay 자동 등록을 피하고 게시 전까지 숨김·충돌 비활성 상태로 둔다.
bool UFFZSWorldSubsystem::RestoreWorldSnapshotUnchecked(const FFFZSWorldSnapshot& Snapshot, TArray<FText>& OutErrors)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        OutErrors.Add(LOCTEXT("RestoreMissingWorld", "유효한 World가 없으면 Zone/Facility 스냅샷을 복원할 수 없습니다."));
        return false;
    }

    TArray<TWeakObjectPtr<AFFZSFacilityActor>> StagedFacilities;
    TMap<FGuid, TWeakObjectPtr<AFFZSFacilityActor>> StagedFacilitiesById;
    TMap<FGuid, FFFZSFacilityRegionOccupancy> StagedOccupiedFacilityRegions;
    FFFZSSpatialHash2D StagedSpatialIndex;
    TMap<FName, FGuid> StagedAgentQueueOwnership;
    TMap<FGuid, FFFZSCommandReceipt> StagedCommandReceipts;
    TSet<FGuid> StagedCommandReceiptFacilityReferences;
    StagedFacilities.Reserve(Snapshot.Facilities.Num());
    StagedFacilitiesById.Reserve(Snapshot.Facilities.Num());
    StagedOccupiedFacilityRegions.Reserve(Snapshot.Facilities.Num());
    StagedAgentQueueOwnership.Reserve(Snapshot.Facilities.Num());
    StagedCommandReceipts.Reserve(Snapshot.CommandReceipts.Num());
    StagedCommandReceiptFacilityReferences.Reserve(Snapshot.CommandReceipts.Num());

    const auto DestroyStagedFacilities = [&StagedFacilities]()
    {
        for (const TWeakObjectPtr<AFFZSFacilityActor>& WeakFacility : StagedFacilities)
        {
            if (AFFZSFacilityActor* Facility = WeakFacility.Get(); IsValid(Facility))
            {
                Facility->Destroy();
            }
        }
    };

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FFFZSFacilityInstanceSnapshot& Saved : Snapshot.Facilities)
    {
        if (!FindZone(Saved.ZoneId))
        {
            OutErrors.Add(LOCTEXT("RestoreMissingZone", "스냅샷 복원을 준비하는 동안 Zone이 사라졌습니다."));
            DestroyStagedFacilities();
            return false;
        }

        FTransform Transform = Saved.WorldTransform;
        Transform.SetScale3D(FVector::OneVector);
        AFFZSFacilityActor* Facility = World->SpawnActor<AFFZSFacilityActor>(
            AFFZSFacilityActor::StaticClass(), Transform, SpawnParameters);
        if (!IsValid(Facility) || Facility->IsActorBeingDestroyed())
        {
            OutErrors.Add(LOCTEXT("RestoreSpawn", "스냅샷 복원 중 Facility 준비에 실패했습니다."));
            DestroyStagedFacilities();
            return false;
        }
        StagedFacilities.Add(Facility);
        Facility->SetActorHiddenInGame(true);
        Facility->SetActorEnableCollision(false);

        if (!Facility->InitializeFacility(
                Saved.FacilityInstanceId,
                Saved.FacilitySpec,
                Saved.ZoneId,
                Saved.Placement,
                Transform,
                Saved.State,
                Saved.UpgradeLevel,
                Saved.UpgradeServiceDurationMultiplier,
                Saved.UpgradeSizeMultiplier,
                Saved.UpgradeRevenueMultiplier))
        {
            OutErrors.Add(LOCTEXT("RestoreInitialize", "스냅샷 복원 중 준비된 Facility 초기화에 실패했습니다."));
            DestroyStagedFacilities();
            return false;
        }
        Facility->SetNeedsRelocation(Saved.bNeedsRelocation);
        Facility->ApplyFacilityLightingOverride(Saved.LightingState, Saved.bHasLightingOverride);

        UFFZSFacilityQueueComponent* Queue = Facility->GetMutableQueueComponent();
        TArray<FText> QueueErrors;
        if (!Queue || !Queue->RestoreSnapshot(Saved.Queue, QueueErrors))
        {
            OutErrors.Append(QueueErrors);
            OutErrors.Add(LOCTEXT("RestoreQueue", "준비된 Facility Queue 복원에 실패했습니다."));
            DestroyStagedFacilities();
            return false;
        }
        // CloseFacility는 이 일시적 의도를 Queue 스냅샷 밖에 기록한다. 복원된 Actor가
        // Drain 완료 후에도 Closed -> MovePending으로 전환되도록 다시 구성한다.
        Facility->bSetMovePendingWhenDrained =
            Saved.State == EFFZSFacilityState::Closed && Saved.Queue.bDrainRequested;
        if (StagedFacilitiesById.Contains(Saved.FacilityInstanceId))
        {
            OutErrors.Add(LOCTEXT("RestoreDuplicateFacility", "스냅샷 복원을 준비하는 동안 중복된 Facility ID가 발견되었습니다."));
            DestroyStagedFacilities();
            return false;
        }
        StagedFacilitiesById.Add(Saved.FacilityInstanceId, Facility);

        TArray<FFFZSOrientedRect> StagedFootprint;
        TArray<FFFZSOrientedRect> StagedClearance;
        TArray<FFFZSOrientedRect> StagedQueue;
        TArray<FFFZSOrientedRect> StagedRects;
        UFFZSGridLibrary::BuildPlacementOrientedRegions(
            Facility->GetFacilitySpec(), Saved.Placement,
            StagedFootprint, StagedClearance, StagedQueue, StagedRects);
        FFFZSFacilityRegionOccupancy StagedOccupancy;
        StagedOccupancy.ZoneId = Saved.ZoneId;
        StagedOccupancy.bIsRoad = Facility->GetFacilitySpec().Category == EFFZSFacilityCategory::Road;
        StagedOccupancy.bAllowsFacilitiesOnTop = Facility->GetFacilitySpec().bAllowFacilityPlacementOnTop;
        StagedOccupancy.Regions = StagedRects;
        StagedOccupancy.WorldRegions = ToWorldRegions(Saved.ZoneId, StagedRects);
        StagedOccupancy.WorldBounds = GetRegionBounds(StagedOccupancy.WorldRegions);
        TArray<FGuid> Candidates;
        StagedSpatialIndex.Query(StagedOccupancy.WorldBounds, Candidates);
        for (const FGuid Id : Candidates)
        {
            const FFFZSFacilityRegionOccupancy& Existing = StagedOccupiedFacilityRegions.FindChecked(Id);
            if (Existing.BlocksLayer(StagedOccupancy.bIsRoad, StagedOccupancy.bAllowsFacilitiesOnTop) && IsOccupancyRelevant(Saved.ZoneId, Existing) &&
                UFFZSGridLibrary::DoOrientedRectSetsOverlap(StagedOccupancy.WorldRegions, Existing.WorldRegions))
            {
                OutErrors.Add(LOCTEXT(
                    "RestoreOccupiedRegion",
                    "스냅샷 복원을 준비하는 동안 서로 겹치는 압축 영역이 발견되었습니다."));
                DestroyStagedFacilities();
                return false;
            }
        }
        StagedSpatialIndex.InsertOrUpdate(Saved.FacilityInstanceId, StagedOccupancy.WorldBounds);
        StagedOccupiedFacilityRegions.Add(Saved.FacilityInstanceId, MoveTemp(StagedOccupancy));

        for (const FFFZSQueueEntrySnapshot& Entry : Saved.Queue.Entries)
        {
            if (Entry.AgentId.IsNone() || StagedAgentQueueOwnership.Contains(Entry.AgentId))
            {
                OutErrors.Add(LOCTEXT("RestoreDuplicateAgent", "동일한 Agent가 두 개 이상의 준비된 Facility Queue에서 발견되었습니다."));
                DestroyStagedFacilities();
                return false;
            }
            StagedAgentQueueOwnership.Add(Entry.AgentId, Saved.FacilityInstanceId);
        }
    }

    for (const FFFZSCommandReceiptSnapshot& SavedReceipt : Snapshot.CommandReceipts)
    {
        FFFZSCommandReceipt Receipt;
        Receipt.PayloadFingerprint = SavedReceipt.PayloadFingerprint;
        Receipt.FacilityInstanceId = SavedReceipt.FacilityInstanceId;
        Receipt.bInFlight = false;
        Receipt.Result.bSucceeded = SavedReceipt.bSucceeded;
        Receipt.Result.bWasDuplicateReplay = false;
        Receipt.Result.FailureReason = SavedReceipt.FailureReason;
        Receipt.Result.Message = SavedReceipt.Message;
        Receipt.Result.WorldRevision = SavedReceipt.ResultWorldRevision;
        Receipt.Result.FacilityActor = nullptr;
        if (SavedReceipt.bResultReferencesFacility)
        {
            StagedCommandReceiptFacilityReferences.Add(SavedReceipt.CommandId);
        }
        StagedCommandReceipts.Add(SavedReceipt.CommandId, MoveTemp(Receipt));
    }

    // Actor identity 검사는 기존 Actor의 EndPlay가 새 등록을 지우는 것을 막는다.
    TArray<TWeakObjectPtr<AFFZSFacilityActor>> ExistingFacilities;
    FacilitiesById.GenerateValueArray(ExistingFacilities);
    for (const TWeakObjectPtr<AFFZSFacilityActor>& WeakFacility : ExistingFacilities)
    {
        if (AFFZSFacilityActor* Facility = WeakFacility.Get())
        {
            if (UFFZSFacilityQueueComponent* Queue = Facility->GetMutableQueueComponent())
            {
                Queue->OnAgentReleased.RemoveDynamic(this, &UFFZSWorldSubsystem::HandleAgentReleased);
            }
            Facility->SetNavigationObstacleEnabled(false);
            Facility->SetPlayerCollisionEnabled(false);
            Facility->SetActorEnableCollision(false);
            Facility->SetActorHiddenInGame(true);
        }
    }

    FacilitiesById = MoveTemp(StagedFacilitiesById);
    OccupiedFacilityRegions = MoveTemp(StagedOccupiedFacilityRegions);
    FacilitySpatialIndex = MakeShared<FFFZSSpatialHash2D>(MoveTemp(StagedSpatialIndex));
    AgentQueueOwnership = MoveTemp(StagedAgentQueueOwnership);
    CommandReceipts = MoveTemp(StagedCommandReceipts);
    CommandReceiptFacilityReferences = MoveTemp(StagedCommandReceiptFacilityReferences);
    WorldRevision = FMath::Max<int64>(0, Snapshot.WorldRevision);
    CurrentPhase = Snapshot.CurrentPhase;

    for (const TWeakObjectPtr<AFFZSFacilityActor>& WeakFacility : StagedFacilities)
    {
        if (AFFZSFacilityActor* Facility = WeakFacility.Get())
        {
            if (UFFZSFacilityQueueComponent* Queue = Facility->GetMutableQueueComponent())
            {
                Queue->OnAgentReleased.AddUniqueDynamic(this, &UFFZSWorldSubsystem::HandleAgentReleased);
            }
            Facility->SetActorHiddenInGame(false);
            Facility->SetActorEnableCollision(true);
            Facility->SetNavigationObstacleEnabled(true);
            Facility->SetPlayerCollisionEnabled(true);
        }
    }
    for (const TWeakObjectPtr<AFFZSFacilityActor>& WeakFacility : ExistingFacilities)
    {
        if (AFFZSFacilityActor* Facility = WeakFacility.Get(); IsValid(Facility))
        {
            Facility->Destroy();
        }
    }
    OnWorldSnapshotRestored.Broadcast();
    return true;
}

bool UFFZSWorldSubsystem::ValidateZoneGraph(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    if (Settings->bRestrictZoneIdsToProjectPresets)
    {
        if (Settings->ProjectZoneIds.IsEmpty())
        {
            OutErrors.Add(LOCTEXT("MissingProjectZoneIds", "제한 기능이 활성화된 동안 Project Zone Ids에는 안정적인 ID가 하나 이상 있어야 합니다."));
        }

        TSet<FName> ConfiguredZoneIds;
        for (const FName ProjectZoneId : Settings->ProjectZoneIds)
        {
            if (ProjectZoneId.IsNone() || ConfiguredZoneIds.Contains(ProjectZoneId))
            {
                OutErrors.Add(LOCTEXT("InvalidProjectZoneId", "Project Zone Ids에 비어 있거나 중복된 ID가 있습니다."));
                continue;
            }
            ConfiguredZoneIds.Add(ProjectZoneId);
            if (!FindZone(ProjectZoneId))
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("MissingProjectZone", "설정된 Gameplay Zone {0}이 없습니다."),
                    FText::FromName(ProjectZoneId)));
            }
        }
    }
    for (const TPair<FName, TWeakObjectPtr<AFFZSGameplayZone>>& Pair : ZonesById)
    {
        const AFFZSGameplayZone* Zone = Pair.Value.Get();
        if (!Zone)
        {
            continue;
        }
        TArray<FText> ZoneErrors;
        Zone->ValidateZoneData(ZoneErrors);
        OutErrors.Append(ZoneErrors);
        for (const FName NeighborId : Zone->NeighborZoneIds)
        {
            const AFFZSGameplayZone* Neighbor = FindZone(NeighborId);
            if (!Neighbor)
            {
                OutErrors.Add(FText::Format(LOCTEXT("MissingNeighbor", "Zone {0}이 존재하지 않는 인접 Zone {1}을 참조합니다."), FText::FromName(Zone->ZoneId), FText::FromName(NeighborId)));
            }
            else if (!Neighbor->NeighborZoneIds.Contains(Zone->ZoneId))
            {
                OutErrors.Add(FText::Format(LOCTEXT("OneWayNeighbor", "Zone 인접 관계는 양방향이어야 합니다: {0} -> {1}."), FText::FromName(Zone->ZoneId), FText::FromName(NeighborId)));
            }
        }
    }

    TSet<FName> ProtectedAreaIds;
    for (const TWeakObjectPtr<AFFZSProtectedArea>& WeakArea : ProtectedAreas)
    {
        const AFFZSProtectedArea* Area = WeakArea.Get();
        if (!Area)
        {
            continue;
        }
        TArray<FText> AreaErrors;
        Area->ValidateProtectedAreaData(AreaErrors);
        OutErrors.Append(AreaErrors);
        if (ProtectedAreaIds.Contains(Area->ProtectedAreaId))
        {
            OutErrors.Add(FText::Format(LOCTEXT("DuplicateProtectedId", "Protected Area ID {0}이 중복되었습니다."), FText::FromName(Area->ProtectedAreaId)));
        }
        ProtectedAreaIds.Add(Area->ProtectedAreaId);
        if (!Area->LinkedZoneId.IsNone() && !FindZone(Area->LinkedZoneId))
        {
            OutErrors.Add(FText::Format(LOCTEXT("MissingProtectedZone", "Protected Area {0}이 존재하지 않는 Zone {1}을 참조합니다."),
                FText::FromName(Area->ProtectedAreaId), FText::FromName(Area->LinkedZoneId)));
        }
    }
    return OutErrors.IsEmpty();
}

int32 UFFZSWorldSubsystem::RevalidateFacilitiesInZone(const FName ZoneId)
{
    if (IsAuthorityMutationBlocked())
    {
        return 0;
    }
    int32 InvalidCount = 0;
    static const FName NeedsRelocationTag(TEXT("ZF.NeedsRelocation"));
    for (const TPair<FGuid, TWeakObjectPtr<AFFZSFacilityActor>>& Pair : FacilitiesById)
    {
        AFFZSFacilityActor* Facility = Pair.Value.Get();
        if (!Facility || Facility->GetZoneId() != ZoneId)
        {
            continue;
        }
        FFFZSCommand Command;
        Command.Type = EFFZSCommandType::Move;
        Command.FacilityInstanceId = Facility->GetFacilityInstanceId();
        Command.FacilitySpec = Facility->GetFacilitySpec();
        Command.ZoneId = ZoneId;
        Command.Placement = Facility->GetGridPlacement();
        const bool bValid = ValidatePlacementLocal(Command, Facility, true).bIsValid;
        if (bValid)
        {
            Facility->Tags.Remove(NeedsRelocationTag);
            Facility->SetNeedsRelocation(false);
        }
        else
        {
            Facility->Tags.AddUnique(NeedsRelocationTag);
            Facility->SetNeedsRelocation(true);
            ++InvalidCount;
        }
    }
    return InvalidCount;
}

void UFFZSWorldSubsystem::OccupyFacilityRegions(const AFFZSFacilityActor& Facility)
{
    FFFZSFacilityRegionOccupancy Record;
    Record.ZoneId = Facility.GetZoneId();
    Record.bIsRoad = Facility.GetFacilitySpec().Category == EFFZSFacilityCategory::Road;
    Record.bAllowsFacilitiesOnTop = Facility.GetFacilitySpec().bAllowFacilityPlacementOnTop;
    TArray<FFFZSOrientedRect> IgnoredFootprint;
    TArray<FFFZSOrientedRect> IgnoredClearance;
    TArray<FFFZSOrientedRect> IgnoredQueue;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Facility.GetFacilitySpec(), Facility.GetGridPlacement(),
        IgnoredFootprint, IgnoredClearance, IgnoredQueue, Record.Regions);
    CacheFacilityOccupancy(Facility.GetFacilityInstanceId(), MoveTemp(Record));
}

void UFFZSWorldSubsystem::ReleaseFacilityRegions(const AFFZSFacilityActor& Facility)
{
    OccupiedFacilityRegions.Remove(Facility.GetFacilityInstanceId());
    if (FacilitySpatialIndex)
    {
        FacilitySpatialIndex->Remove(Facility.GetFacilityInstanceId());
    }
}

bool UFFZSWorldSubsystem::DoesRoadSupportFacility(const AFFZSFacilityActor& Road) const
{
    if (Road.GetFacilitySpec().Category != EFFZSFacilityCategory::Road) return false;
    const FFFZSFacilityRegionOccupancy* RoadOccupancy = OccupiedFacilityRegions.Find(Road.GetFacilityInstanceId());
    if (!RoadOccupancy) return false;
    TArray<FGuid> Candidates;
    QueryFacilityCandidates(RoadOccupancy->WorldRegions, Candidates);
    for (const FGuid Id : Candidates)
    {
        const AFFZSFacilityActor* Facility = FindFacility(Id);
        const FFFZSFacilityRegionOccupancy* Occupancy = OccupiedFacilityRegions.Find(Id);
        if (!Facility || !Occupancy || Occupancy->bIsRoad || !IsOccupancyRelevant(Road.GetZoneId(), *Occupancy)) continue;
        TArray<FFFZSOrientedRect> Footprint, Clearance, Queue, Reserved;
        UFFZSGridLibrary::BuildPlacementOrientedRegions(Facility->GetFacilitySpec(), Facility->GetGridPlacement(),
            Footprint, Clearance, Queue, Reserved);
        if (UFFZSGridLibrary::DoOrientedRectSetsOverlap(RoadOccupancy->WorldRegions,
            ToWorldRegions(Facility->GetZoneId(), Footprint))) return true;
    }
    return false;
}

bool UFFZSWorldSubsystem::AreRegionsOccupiedByOther(
    const FName ZoneId,
    const TArray<FFFZSOrientedRect>& Regions,
    const FGuid IgnoreFacilityId,
    const bool bIsRoad,
    const bool bAllowsFacilitiesOnTop) const
{
    const TArray<FFFZSOrientedRect> WorldRegions = ToWorldRegions(ZoneId, Regions);
    if (WorldRegions.IsEmpty())
    {
        return true;
    }
    TArray<FGuid> Candidates;
    QueryFacilityCandidates(WorldRegions, Candidates);
    for (const FGuid Id : Candidates)
    {
        const FFFZSFacilityRegionOccupancy* Occupancy = OccupiedFacilityRegions.Find(Id);
        if (Id == IgnoreFacilityId || !Occupancy || !IsOccupancyRelevant(ZoneId, *Occupancy) ||
            !Occupancy->BlocksLayer(bIsRoad, bAllowsFacilitiesOnTop))
        {
            continue;
        }
        if (UFFZSGridLibrary::DoOrientedRectSetsOverlap(WorldRegions, Occupancy->WorldRegions))
        {
            return true;
        }
    }
    return false;
}
void UFFZSWorldSubsystem::RefreshWorldActors() const
{
}

#undef LOCTEXT_NAMESPACE
