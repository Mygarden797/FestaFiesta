/**
 * @file FF_FFZSWorldSubsystem.cpp
 * @brief 시설·Zone·보호구역 Registry, 점유, 명령, Policy, Queue, Snapshot의 월드 권위를 구현한다.
 *
 * 흐름: 로컬 검증 뒤 외부 Policy를 준비하고 승인된 상태만 단일 publish 경계에서 반영한다.
 * 불변식: 명령 실패는 부분 점유를 남기지 않으며 재진입·stale Revision·중복 Command를 방어한다.
 * 실패/성능: 공간 후보 캐시는 할당과 Transform을 재사용하지만 기하 판정 반복비용 자체를 제거하지 않는다.
 */

#include "FF_FFZSWorldSubsystem.h"

#include "Algo/Accumulate.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Templates/UnrealTemplate.h"
#include "NavigationPath.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSExternalPolicy.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSQueueAgent.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_ZoneSystem.h"
#include "FF_FFZSZoneActor.h"

#define LOCTEXT_NAMESPACE "FFZSWorldSubsystem"
/*
 * UFFZSWorldSubsystem은 한 UWorld의 Zone/Facility 시스템을 조정하는 권위 계층이다.
 *
 * 권위 상태
 * - ZonesById / FacilitiesById: 안정 ID에서 현재 Actor로 가는 레지스트리
 * - OccupiedFacilityRegions: 시설 GUID별 ZoneId와 압축 예약 영역을 보존하는 배치 점유 원장
 * - AgentQueueOwnership: Agent 하나가 동시에 Queue 하나에만 속한다는 소유 원장
 * - WorldRevision / CommandReceipts: 낙관적 동시성 및 CommandId 멱등 재실행 기록
 * Actor의 표현 컴포넌트나 Preview 디버그 도형은 이 원장을 대신하지 않는다.
 *
 * 명령의 권위 흐름
 * 요청 -> Game Thread/재진입/ID/Revision/Phase 검사 -> 로컬 배치 및 정책 사전 검증
 * -> 외부 정책 임시 적용 -> Actor/점유 원장 변경 -> 정책 Commit -> Revision 증가 및 이벤트 발행이다.
 * 중간 실패 시 임시 정책과 변경 가능한 로컬 상태를 복구하고, 완료된 성공/실패 결과를 Receipt로 저장해 재생한다.
 *
 * 동시성 및 재진입 불변식
 * - 권위 변경은 Game Thread에서만 수행한다.
 * - Snapshot 복원, 외부 정책 콜백, 다른 Command 실행 중에는 새 권위 변경을 시작하지 않는다.
 * - 외부 정책과 Queue 델리게이트는 동기 재진입할 수 있으므로 Guard와 선점 기록을 콜백보다 먼저 둔다.
 *
 * 파일의 함수군은 순서대로 기하 도우미/캐시, 월드 수명과 레지스트리, 배치 검증, 외부 정책
 * 트랜잭션, 명령 실행, 통합 정책과 Queue, 조회 모델, 스냅샷, 그래프/점유 유지보수로 나뉜다.
 */

namespace
{
    // [기하 도우미] Zone 로컬 압축 사각형의 네 모서리를 월드로 변환해 Z 높이를 포함한 AABB를 만든다.
    // 사각형 한 개가 수십만 1 cm 셀을 대신하므로 보호 구역 broad-phase 호출 수도 영역 수에 비례한다.
    FBox MakeCellRectWorldBox(
        const AFFZSGameplayZone& Zone,
        const FFFZSCellRect& Rect,
        const double CellSizeCm,
        const double BaseZ,
        const double HeightCm)
    {
        FBox Result(EForceInit::ForceInit);
        const FVector LocalCorners[] = {
            FVector(Rect.Min.X * CellSizeCm, Rect.Min.Y * CellSizeCm, 0.0),
            FVector(Rect.Max.X * CellSizeCm, Rect.Min.Y * CellSizeCm, 0.0),
            FVector(Rect.Max.X * CellSizeCm, Rect.Max.Y * CellSizeCm, 0.0),
            FVector(Rect.Min.X * CellSizeCm, Rect.Max.Y * CellSizeCm, 0.0)
        };
        for (const FVector& LocalCorner : LocalCorners)
        {
            FVector WorldCorner = Zone.GetActorTransform().TransformPosition(LocalCorner);
            WorldCorner.Z = BaseZ;
            Result += WorldCorner;
            WorldCorner.Z = BaseZ + HeightCm;
            Result += WorldCorner;
        }
        return Result;
    }

    bool IsWorldChangeCommand(const EFFZSCommandType Type)
    {
        return Type == EFFZSCommandType::Install || Type == EFFZSCommandType::Move || Type == EFFZSCommandType::Remove;
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
        if (LengthSquared <= UE_SMALL_NUMBER)
        {
            return FVector2D::Distance(Point, A) <= ToleranceCm;
        }
        const double T = FMath::Clamp(FVector2D::DotProduct(Point - A, Segment) / LengthSquared, 0.0, 1.0);
        return FVector2D::Distance(Point, A + Segment * T) <= ToleranceCm;
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
        bool bIsTargetZone = false;
        double VerticalExtentCm = 0.0;
        FTransform WorldTransform;
        TArray<FVector2D> PolygonInCellSpace;
    };

    /*
     * [존 소유권 기하 캐시]
     * 한 번의 배치 검증에서는 TargetZone Transform을 셀 공간 기준으로 고정하고, 각 후보 존의 Transform과
     * 폴리곤을 그 셀 공간으로 한 번만 변환한다. 그래서 셀마다 GetBoundaryPolygonLocalCm 배열을 다시
     * 만들거나 모든 꼭짓점을 월드/로컬 변환하지 않는다. Snapshot 검증은 대상 Zone별로 이 캐시를 하나씩
     * 보관하여 같은 Zone의 여러 시설에도 재사용한다.
     *
     * 단, 이것은 공간 인덱스가 아니다. 각 압축 사각형은 여전히 후보 존을 순회하고, 각 후보 판정은 폴리곤의
     * 샘플/꼭짓점/변을 검사한다. 실제 비용은 셀 수, 후보 존 수, 각 존의 변 수에 함께 좌우되므로
     * O(존 수 + 셀 수)라고 단순화해서는 안 된다. 캐시는 할당과 Transform 반복을 줄이는 역할이다.
     */
    /** 한 번의 배치/스냅샷 검증 호출에서 검사하는 모든 압축 사각형을 위해 한 번만 준비되는 불변 Geometry다. */
    struct FZoneCellOwnershipCache
    {
        FTransform CellSpaceWorldTransform;
        TArray<FVector2D> TargetPolygonInCellSpace;
        TArray<FPreparedZoneOverlap> Candidates;

        FZoneCellOwnershipCache(
            const TMap<FName, TWeakObjectPtr<AFFZSGameplayZone>>& Zones,
            const AFFZSGameplayZone& TargetZone)
            : CellSpaceWorldTransform(TargetZone.GetActorTransform())
            , TargetPolygonInCellSpace(GetZonePolygonInCellSpace(
                TargetZone,
                CellSpaceWorldTransform,
                CellSpaceWorldTransform))
        {
            Candidates.Reserve(Zones.Num());
            for (const TPair<FName, TWeakObjectPtr<AFFZSGameplayZone>>& Pair : Zones)
            {
                const AFFZSGameplayZone* Candidate = Pair.Value.Get();
                if (!Candidate)
                {
                    continue;
                }

                FPreparedZoneOverlap& Prepared = Candidates.AddDefaulted_GetRef();
                Prepared.bIsTargetZone = Candidate == &TargetZone;
                Prepared.VerticalExtentCm = Candidate->VerticalExtentCm;
                Prepared.WorldTransform = Candidate->GetActorTransform();
                if (!Prepared.bIsTargetZone)
                {
                    Prepared.PolygonInCellSpace = GetZonePolygonInCellSpace(
                        *Candidate,
                        Prepared.WorldTransform,
                        CellSpaceWorldTransform);
                }
            }
        }
    };

    /*
     * 대상 존은 셀 네 모서리와 중심이 경계를 포함한 내부인지 확인하고, 존 경계 변이 셀의 열린 내부를
     * 통과하지 않는지도 검사한다. 다른 존은 반대로 셀의 열린 내부와 실제로 겹칠 때만 중복 소유로 센다.
     * 따라서 두 존이 경계선에서 맞닿기만 한 것은 중복으로 보지 않지만, 면적이 겹치면 실패한다.
     * 이 판정은 ValidateZoneData가 보장하는 단순 폴리곤 및 Transform 불변식을 전제로 한다.
     */
    /** Target Zone이 압축 사각형 전체를 포함하는지 꼭짓점·중심·경계 교차로 정확히 검사한다. */
    bool IsRectFullyInsideZone(
        const TArray<FVector2D>& Polygon,
        const FFFZSCellRect& Rect,
        const double CellSizeCm)
    {
        if (!Rect.IsValid())
        {
            return false;
        }
        const double MinX = Rect.Min.X * CellSizeCm;
        const double MinY = Rect.Min.Y * CellSizeCm;
        const double MaxX = Rect.Max.X * CellSizeCm;
        const double MaxY = Rect.Max.Y * CellSizeCm;
        const FVector2D Samples[] = {
            FVector2D(MinX, MinY), FVector2D(MaxX, MinY),
            FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY),
            FVector2D((MinX + MaxX) * 0.5, (MinY + MaxY) * 0.5)
        };
        for (const FVector2D& Sample : Samples)
        {
            if (!IsPointInsidePolygon2D(Sample, Polygon, true))
            {
                return false;
            }
        }
        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            if (SegmentIntersectsOpenRect(
                Polygon[Index], Polygon[(Index + 1) % Polygon.Num()], MinX, MinY, MaxX, MaxY))
            {
                return false;
            }
        }
        return true;
    }

    /** 다른 Zone이 압축 사각형의 열린 내부와 실제 면적으로 겹치는지 검사한다. 경계 접촉은 허용한다. */
    bool DoesZoneOverlapRectInterior(
        const FPreparedZoneOverlap& Candidate,
        const FTransform& CellSpaceWorldTransform,
        const FFFZSCellRect& Rect,
        const double CellSizeCm)
    {
        if (!Rect.IsValid())
        {
            return false;
        }
        const double MinX = Rect.Min.X * CellSizeCm;
        const double MinY = Rect.Min.Y * CellSizeCm;
        const double MaxX = Rect.Max.X * CellSizeCm;
        const double MaxY = Rect.Max.Y * CellSizeCm;
        const FVector CellCenterWorld = CellSpaceWorldTransform.TransformPosition(
            FVector((MinX + MaxX) * 0.5, (MinY + MaxY) * 0.5, 0.0));
        if (FMath::Abs(Candidate.WorldTransform.InverseTransformPosition(CellCenterWorld).Z) >
            Candidate.VerticalExtentCm * 0.5)
        {
            return false;
        }

        const TArray<FVector2D>& Polygon = Candidate.PolygonInCellSpace;
        const FVector2D Samples[] = {
            FVector2D(MinX, MinY), FVector2D(MaxX, MinY),
            FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY),
            FVector2D((MinX + MaxX) * 0.5, (MinY + MaxY) * 0.5)
        };
        for (const FVector2D& Sample : Samples)
        {
            if (IsPointInsidePolygon2D(Sample, Polygon, false))
            {
                return true;
            }
        }
        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            const FVector2D& Vertex = Polygon[Index];
            if ((Vertex.X > MinX && Vertex.X < MaxX && Vertex.Y > MinY && Vertex.Y < MaxY) ||
                SegmentIntersectsOpenRect(
                    Vertex, Polygon[(Index + 1) % Polygon.Num()], MinX, MinY, MaxX, MaxY))
            {
                return true;
            }
        }
        return false;
    }

    /** Target 하나만 사각형 전체를 소유하고 다른 Zone과 면적 중첩이 없을 때 true다. */
    bool IsRectOwnedByExactlyOneZone(
        const FZoneCellOwnershipCache& Cache,
        const FFFZSCellRect& Rect,
        const double CellSizeCm)
    {
        if (!IsRectFullyInsideZone(Cache.TargetPolygonInCellSpace, Rect, CellSizeCm))
        {
            return false;
        }

        int32 OverlapCount = 0;
        for (const FPreparedZoneOverlap& Candidate : Cache.Candidates)
        {
            if (Candidate.bIsTargetZone ||
                DoesZoneOverlapRectInterior(Candidate, Cache.CellSpaceWorldTransform, Rect, CellSizeCm))
            {
                ++OverlapCount;
            }
        }
        return OverlapCount == 1;
    }

    constexpr int32 LegacySnapshotSchemaVersion = 3;
    constexpr int32 CurrentSnapshotSchemaVersion = 4;
    constexpr int64 LegacyMeterCellScale = 100;

    /** int32 범위의 구 Cell 값을 100배 할 때 손상 입력의 오버플로를 명시적으로 거부한다. */
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

    /**
     * 구 100 cm 개별 Queue Cell을 정확한 1 cm 사각형 합집합으로 압축한다.
     *
     * Cell을 Y/X로 한 번 정렬한 뒤 같은 행의 연속 X를 수평 span으로 합치고, 바로 이전 행의
     * 동일 span만 수직으로 연장한다. 따라서 10,000개 입력도 O(N log N)이며, 일반 Rect 합집합의
     * 반복 교차 검사로 O(N^2)까지 커지는 저장 복원 지연을 피한다. 중복 Cell은 한 번만 반영한다.
     */
    bool TryCompressLegacyQueueCells(
        const TArray<FIntPoint>& LegacyCells,
        TArray<FFFZSCellRect>& OutRects)
    {
        OutRects.Reset();
        TArray<FIntPoint> SortedCells = LegacyCells;

        // 정렬 전에 모든 반열림 Max=(Cell+1)을 검사해 부분 결과를 남기지 않고 실패한다.
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

                // 같은 행의 중복은 건너뛰고, 맞닿는 Cell은 하나의 수평 span으로 합친다.
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

    /**
     * Schema 3(1 Cell=100 cm)을 Schema 4(1 Cell=1 cm)로 메모리에서만 변환한다.
     * cm로 저장하던 spline/anchor/높이/queue 시간은 그대로 두고 Cell 기반 값만 100배 한다.
     * 회전 Origin의 99 cm 보정은 구 RotateCell이 나타낸 100x100 cm 면적 경계를 정확히 보존한다.
     */
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
            if (Spec.SchemaVersion != 1 || !Spec.QueueReservedRects.IsEmpty())
            {
                OutErrors.Add(FText::Format(
                    LOCTEXT("LegacySnapshotAmbiguousSpec", "Schema 3 Facility {0}에는 수정되지 않은 FacilitySpec SchemaVersion 1이 있어야 하며 1cm QueueReservedRects가 없어야 합니다."),
                    FText::FromString(Saved.FacilityInstanceId.ToString())));
                return false;
            }
            if (Spec.QueueReservedCells.Num() > UFFZSDeveloperSettings::MaxMaterializedDebugCells)
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

            Spec.SchemaVersion = 2;
            Spec.FootprintSizeCells = MigratedFootprint;
            Spec.ClearanceCells = MigratedClearance;
            Spec.QueueReservedRects = MoveTemp(MigratedQueueRects);
            Spec.QueueReservedCells.Reset();
            Saved.Placement.OriginCell = FIntPoint(static_cast<int32>(AdjustedX), static_cast<int32>(AdjustedY));
        }
        return true;
    }
}

/* [월드 수명/레지스트리]
 * Subsystem 생성 시 이미 월드에 놓인 Actor를 수집하고, 이후 Actor의 BeginPlay/EndPlay가 등록 상태를
 * 유지한다. 중복 ID, 잘못된 데이터, 점유 충돌은 등록 단계에서 거부하여 권위 맵의 불변식을 지킨다.
 */
void UFFZSWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    CurrentPhase = GetDefault<UFFZSDeveloperSettings>()->InitialPhase;

    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
        {
            RegisterZone(*It);
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

/*
 * 권위 변경 공통 차단기다. Game Thread 밖, 전체 Snapshot 교체 중, 외부 정책 콜백 중, Command 실행
 * 중이면 실패 폐쇄한다. TGuardValue로 설정된 플래그/ID는 조기 반환에서도 자동 복구되어 잠금 상태가
 * 남지 않는다. 내부 명령 단계에서 필요한 전용 등록 함수는 호출 순서와 InFlightCommandId가 보호한다.
 */
bool UFFZSWorldSubsystem::IsAuthorityMutationBlocked() const
{
    return !IsInGameThread() || bIsRestoringSnapshot || bIsInvokingExternalPolicy || InFlightCommandId.IsValid();
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
    if (!IsValid(Zone) || Zone->ZoneId.IsNone())
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
            return true;
        }
    }

    const TArray<FFFZSCellRect> ReservedRects = UFFZSGridLibrary::GetReservedRects(
        Facility->GetFacilitySpec(), Facility->GetGridPlacement());
    if (ReservedRects.IsEmpty() ||
        AreRegionsOccupiedByOther(Facility->GetZoneId(), ReservedRects, FacilityId))
    {
        UE_LOG(LogFFZoneSystem, Error, TEXT("Facility %s 등록 실패: Zone %s의 압축 영역이 잘못되었거나 다른 영역과 겹칩니다"),
            *FacilityId.ToString(), *Facility->GetZoneId().ToString());
        return false;
    }

    FacilitiesById.Add(FacilityId, Facility);
    OccupyFacilityRegions(*Facility);
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

AFFZSFacilityActor* UFFZSWorldSubsystem::FindFacility(const FGuid FacilityInstanceId) const
{
    if (const TWeakObjectPtr<AFFZSFacilityActor>* Found = FacilitiesById.Find(FacilityInstanceId))
    {
        return Found->Get();
    }
    return nullptr;
}

/* [배치 검증 함수군]
 * ValidatePlacementLocal은 Spec/Zone/셀 소유권/점유/보호 구역/지면/월드 충돌/내비게이션을 검사하고,
 * 공개 ValidatePlacement는 그 결과가 유효할 때 외부 정책의 읽기 성격 사전 검증까지 덧붙인다.
 * Move 요청의 Spec은 호출자 입력이 아니라 이미 등록된 시설 Spec으로 덮어써 시설 정의 변조를 막는다.
 * 이 단계는 PlacementTransform과 실패 이유를 계산할 뿐 권위 레지스트리나 점유 셀을 변경하지 않는다.
 */
FFFZSPlacementValidation UFFZSWorldSubsystem::ValidatePlacement(const FFFZSCommand& Command) const
{
    const AFFZSFacilityActor* Existing = Command.Type == EFFZSCommandType::Move ? FindFacility(Command.FacilityInstanceId) : nullptr;
    FFFZSCommand EffectiveCommand = Command;
    if (Existing)
    {
        EffectiveCommand.FacilitySpec = Existing->GetFacilitySpec();
    }

    FFFZSPlacementValidation Result = ValidatePlacementLocal(EffectiveCommand, Existing);
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

FFFZSPlacementValidation UFFZSWorldSubsystem::ValidatePlacementLocal(const FFFZSCommand& Command, const AFFZSFacilityActor* IgnoreFacility) const
{
    FFFZSPlacementValidation Result;
    Result.bIsValid = true;

    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();

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
    Placement.QuarterTurns = UFFZSGridLibrary::NormalizeQuarterTurns(Placement.QuarterTurns);

    // 권위 결과는 압축 사각형이다. 구 셀 배열은 작은 디버그 대상만 materialize하며 실패해도 판정에는 영향이 없다.
    Result.FootprintRects = UFFZSGridLibrary::GetFootprintRects(Command.FacilitySpec, Placement);
    Result.ClearanceRects = UFFZSGridLibrary::GetClearanceRects(Command.FacilitySpec, Placement);
    Result.QueueRects = UFFZSGridLibrary::GetQueueRects(Command.FacilitySpec, Placement);
    Result.ReservedRects = UFFZSGridLibrary::GetReservedRects(Command.FacilitySpec, Placement);
    UFFZSGridLibrary::TryMaterializeCellRects(Result.FootprintRects,
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result.FootprintCells);
    UFFZSGridLibrary::TryMaterializeCellRects(Result.ClearanceRects,
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result.ClearanceCells);
    UFFZSGridLibrary::TryMaterializeCellRects(Result.QueueRects,
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result.QueueCells);
    UFFZSGridLibrary::TryMaterializeCellRects(Result.ReservedRects,
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result.ReservedCells);
    Result.PlacementTransform = BuildPlacementTransform(*Zone, Command.FacilitySpec, Placement);

    if (Result.FootprintRects.IsEmpty() || Result.ReservedRects.IsEmpty())
    {
        Result.AddFailure(EFFZSPlacementFailure::DataInvalid,
            LOCTEXT("EmptyCompressedPlacement", "Facility 데이터에서 유효한 압축 1cm 배치 영역을 만들 수 없습니다."));
        return Result;
    }

    const FZoneCellOwnershipCache ZoneOwnershipCache(ZonesById, *Zone);
    for (const FFFZSCellRect& Rect : Result.ReservedRects)
    {
        if (!IsRectOwnedByExactlyOneZone(
            ZoneOwnershipCache, Rect, UFFZSDeveloperSettings::DefaultPlacementCellSizeCm))
        {
            Result.AddFailure(EFFZSPlacementFailure::OutsideZone,
                LOCTEXT("OutsideOrOverlappingZone", "Footprint, 여유 공간 및 최대 Queue 영역은 정확히 하나의 Gameplay Zone 안에 완전히 포함되어야 합니다."));
            break;
        }
    }

    const FGuid IgnoreId = IgnoreFacility ? IgnoreFacility->GetFacilityInstanceId() : FGuid();
    if (AreRegionsOccupiedByOther(Command.ZoneId, Result.FootprintRects, IgnoreId))
    {
        Result.AddFailure(EFFZSPlacementFailure::PlacementCollision,
            LOCTEXT("FootprintOccupied", "Facility Footprint가 다른 Facility 예약 영역과 겹칩니다."));
    }
    if (AreRegionsOccupiedByOther(Command.ZoneId, Result.ClearanceRects, IgnoreId))
    {
        Result.AddFailure(EFFZSPlacementFailure::InsufficientClearance,
            LOCTEXT("ClearanceOccupied", "필요한 여유 공간이 점유되어 있습니다."));
    }
    if (AreRegionsOccupiedByOther(Command.ZoneId, Result.QueueRects, IgnoreId))
    {
        Result.AddFailure(EFFZSPlacementFailure::QueueFootprintBlocked,
            LOCTEXT("QueueOccupied", "최대 Queue 영역이 점유되어 있습니다."));
    }
    ValidateProtectedAreas(Command.ZoneId, *Zone, Command.FacilitySpec, Result);
    ValidateGround(*Zone, Command.FacilitySpec, IgnoreFacility, Result);
    ValidateWorldCollision(*Zone, Command.FacilitySpec, IgnoreFacility, Result);
    ValidateNavigation(Command.FacilitySpec, Result);

    Result.bIsValid = Result.Reasons.IsEmpty();
    if (Result.bIsValid)
    {
        Result.Summary = LOCTEXT("PlacementValid", "배치가 유효합니다.");
    }
    return Result;
}

// 회전된 footprint 압축 사각형의 중점을 닫힌식으로 계산한다. 셀을 펼쳐 평균내지 않으므로
// 600x300 같은 1 cm 사양도 O(1)이며, Zone 회전과 사분면 회전을 합성하고 Scale은 항상 1이다.
FTransform UFFZSWorldSubsystem::BuildPlacementTransform(
    const AFFZSGameplayZone& Zone,
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement) const
{
    const TArray<FFFZSCellRect> FootprintRects = UFFZSGridLibrary::GetFootprintRects(Spec, Placement);
    FVector Center = Zone.GetActorLocation();
    if (!FootprintRects.IsEmpty())
    {
        const FFFZSCellRect& Rect = FootprintRects[0];
        const double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
        Center = Zone.GetActorTransform().TransformPosition(FVector(
            (static_cast<double>(Rect.Min.X) + Rect.Max.X) * 0.5 * CellSizeCm,
            (static_cast<double>(Rect.Min.Y) + Rect.Max.Y) * 0.5 * CellSizeCm,
            0.0));
    }
    const FQuat Rotation = Zone.GetActorQuat() * FQuat(
        FVector::UpVector,
        FMath::DegreesToRadians(90.0 * UFFZSGridLibrary::NormalizeQuarterTurns(Placement.QuarterTurns)));
    return FTransform(Rotation, Center, FVector::OneVector);
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
    if (IgnoreFacility)
    {
        Params.AddIgnoredActor(IgnoreFacility);
    }

    const double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const double SampleSpacingCm = FMath::Clamp(
        static_cast<double>(Settings->GroundSampleSpacingCm), 1.0, 1000.0);
    TArray<float> Heights;
    float MaximumSlope = 0.0f;

    // 1 cm 점유 해상도와 물리 검증 밀도를 분리한다. 각 축을 균등 분할해 간격은 기본 100 cm 이하이고
    // 작은 시설도 중심 한 점을 검사하므로 표본 수는 실제 면적/표본간격²에 비례한다.
    for (const FFFZSCellRect& Rect : InOutValidation.FootprintRects)
    {
        const double WidthCm = Rect.GetWidth() * CellSizeCm;
        const double HeightCm = Rect.GetHeight() * CellSizeCm;
        const int32 SamplesX = FMath::Max(1, FMath::CeilToInt(WidthCm / SampleSpacingCm));
        const int32 SamplesY = FMath::Max(1, FMath::CeilToInt(HeightCm / SampleSpacingCm));
        if (static_cast<int64>(SamplesX) * SamplesY > UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility())
        {
            InOutValidation.AddFailure(EFFZSPlacementFailure::DataInvalid,
                LOCTEXT("GroundSampleBudget", "지면 샘플링이 설정된 안전 예산을 초과했습니다."));
            return;
        }

        for (int32 X = 0; X < SamplesX; ++X)
        {
            for (int32 Y = 0; Y < SamplesY; ++Y)
            {
                const double GridX = Rect.Min.X + (X + 0.5) * Rect.GetWidth() / SamplesX;
                const double GridY = Rect.Min.Y + (Y + 0.5) * Rect.GetHeight() / SamplesY;
                const FVector Sample = Zone.GetActorTransform().TransformPosition(
                    FVector(GridX * CellSizeCm, GridY * CellSizeCm, 0.0));
                const FVector Start = Sample + FVector::UpVector * Settings->GroundTraceAboveCm;
                const FVector End = Sample - FVector::UpVector * Settings->GroundTraceBelowCm;
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
                Heights.Add(Hit.ImpactPoint.Z);
                MaximumSlope = FMath::Max(MaximumSlope, FMath::RadiansToDegrees(FMath::Acos(
                    FMath::Clamp(FVector::DotProduct(
                        Hit.ImpactNormal.GetSafeNormal(), FVector::UpVector), -1.0f, 1.0f))));
            }
        }
    }

    if (Heights.IsEmpty())
    {
        InOutValidation.AddFailure(EFFZSPlacementFailure::InvalidGround,
            LOCTEXT("NoGround", "성공한 Footprint 지면 샘플이 없습니다."));
        return;
    }
    if (MaximumSlope > Settings->MaxGroundSlopeDegrees)
    {
        InOutValidation.AddFailure(EFFZSPlacementFailure::ExcessSlope,
            LOCTEXT("SlopeHigh", "지면 경사가 설정된 프로토타입 제한을 초과했습니다."));
    }
    const float MinHeight = FMath::Min(Heights);
    const float MaxHeight = FMath::Max(Heights);
    if (MaxHeight - MinHeight > Settings->MaxGroundHeightVarianceCm)
    {
        InOutValidation.AddFailure(EFFZSPlacementFailure::HeightVarianceExceeded,
            LOCTEXT("HeightVar", "지면 높이 편차가 설정된 프로토타입 제한을 초과했습니다."));
    }
    FVector Location = InOutValidation.PlacementTransform.GetLocation();
    Location.Z = Algo::Accumulate(Heights, 0.0f) / Heights.Num();
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

    const auto HasBlockingOverlap = [World, &Params](
        const FVector& Center, const FQuat& Rotation, const FVector& HalfExtent) -> bool
    {
        TArray<FOverlapResult> Overlaps;
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
    const float HalfHeight = FMath::Max(5.0f, Spec.PlacementObstacleHeightCm * 0.5f - 2.0f);
    const float CenterZ = InOutValidation.PlacementTransform.GetLocation().Z + HalfHeight + 3.0f;
    const auto ValidateRectGroup = [&] (
        const TArray<FFFZSCellRect>& Rects,
        const EFFZSPlacementFailure Failure,
        const FText& Message)
    {
        for (const FFFZSCellRect& Rect : Rects)
        {
            const double HalfX = Rect.GetWidth() * CellSizeCm * 0.5;
            const double HalfY = Rect.GetHeight() * CellSizeCm * 0.5;
            // 기존 `0.5 - 2`는 1 cm 단일 셀에서 음수였다. 최소 0.25 cm를 보장한다.
            const FVector HalfExtent(
                FMath::Max(0.25, HalfX - 2.0),
                FMath::Max(0.25, HalfY - 2.0),
                HalfHeight);
            FVector Center = Zone.GetActorTransform().TransformPosition(FVector(
                (static_cast<double>(Rect.Min.X) + Rect.Max.X) * 0.5 * CellSizeCm,
                (static_cast<double>(Rect.Min.Y) + Rect.Max.Y) * 0.5 * CellSizeCm,
                0.0));
            Center.Z = CenterZ;
            if (HasBlockingOverlap(Center, Zone.GetActorQuat(), HalfExtent))
            {
                InOutValidation.AddFailure(Failure, Message);
                return;
            }
        }
    };

    ValidateRectGroup(InOutValidation.FootprintRects, EFFZSPlacementFailure::PlacementCollision,
        LOCTEXT("WorldFootprintBlocked", "Facility Footprint가 배치를 막는 World Object와 겹칩니다."));
    ValidateRectGroup(InOutValidation.ClearanceRects, EFFZSPlacementFailure::InsufficientClearance,
        LOCTEXT("WorldClearanceBlocked", "여유 공간이 배치를 막는 World Object와 겹칩니다."));
    ValidateRectGroup(InOutValidation.QueueRects, EFFZSPlacementFailure::QueueFootprintBlocked,
        LOCTEXT("WorldQueueBlocked", "최대 Queue 영역이 배치를 막는 World Object와 겹칩니다."));
}
void UFFZSWorldSubsystem::ValidateNavigation(const FFFZSFacilitySpec& Spec, FFFZSPlacementValidation& InOutValidation) const
{
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

    TArray<double> SegmentLengths;
    double TotalSplineLength = 0.0;
    for (int32 Index = 1; Index < Spec.QueueSplinePointsCm.Num(); ++Index)
    {
        const double Length = FVector::Distance(Spec.QueueSplinePointsCm[Index - 1], Spec.QueueSplinePointsCm[Index]);
        SegmentLengths.Add(Length);
        TotalSplineLength += Length;
    }
    for (int32 SlotIndex = 0; SlotIndex < Spec.QueueRules.MaxWaitingAgents; ++SlotIndex)
    {
        double RemainingDistance = FMath::Clamp(
            static_cast<double>(SlotIndex) * Spec.QueueRules.SlotSpacingCm, 0.0, TotalSplineLength);
        FVector LocalSlot = Spec.QueueSplinePointsCm[0];
        for (int32 SegmentIndex = 0; SegmentIndex < SegmentLengths.Num(); ++SegmentIndex)
        {
            const double SegmentLength = SegmentLengths[SegmentIndex];
            if (RemainingDistance <= SegmentLength || SegmentIndex == SegmentLengths.Num() - 1)
            {
                const double Alpha = SegmentLength > UE_SMALL_NUMBER ? FMath::Clamp(RemainingDistance / SegmentLength, 0.0, 1.0) : 0.0;
                LocalSlot = FMath::Lerp(Spec.QueueSplinePointsCm[SegmentIndex], Spec.QueueSplinePointsCm[SegmentIndex + 1], Alpha);
                break;
            }
            RemainingDistance -= SegmentLength;
        }
        if (!ProjectRequiredPoint(LocalSlot, false))
        {
            InOutValidation.AddFailure(EFFZSPlacementFailure::RequiredRouteDisconnected,
                LOCTEXT("QueueSlotOffNav", "최대 수용 인원의 Queue 슬롯을 내비게이션에 투영할 수 없습니다."));
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
        // 경로 지점은 투영된 Entry 자체일 수 있다. 일부 내비게이션 구현은
        // 길이가 0인 요청에 대해 Path Object를 만들지 않는다.
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

// ReservedRects 각각을 월드 AABB로 만든 뒤 ProtectedArea의 Yaw OBB와 교차 검사한다. LinkedZoneId가
// 지정된 보호 구역은 해당 Zone에만 적용되며, 유효하지 않은 보호 데이터는 안전하게 배치 실패로 처리한다.
void UFFZSWorldSubsystem::ValidateProtectedAreas(const FName ZoneId, const AFFZSGameplayZone& Zone, const FFFZSFacilitySpec& Spec, FFFZSPlacementValidation& InOutValidation) const
{
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
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
        if (!Area->LinkedZoneId.IsNone() && Area->LinkedZoneId != ZoneId)
        {
            continue;
        }

        for (const FFFZSCellRect& Rect : InOutValidation.ReservedRects)
        {
            const FBox RegionBox = MakeCellRectWorldBox(
                Zone,
                Rect,
                UFFZSDeveloperSettings::DefaultPlacementCellSizeCm,
                InOutValidation.PlacementTransform.GetLocation().Z,
                Spec.PlacementObstacleHeightCm);
            if (Area->IntersectsWorldBox(RegionBox))
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

/* [외부 정책 트랜잭션]
 * 정책 목록을 복사한 Snapshot으로 순회하여 콜백 도중 원본 배열 변화의 영향을 받지 않게 한다.
 * PreValidate는 예약 없는 허용 판정, ApplyTentative는 외부 시스템의 임시 예약, Commit은 확정이다.
 * ApplyTentative 중 하나라도 거부하면 이미 호출한 정책(거부한 정책 포함)을 역순 Rollback한다.
 * 역순 복구는 앞선 정책의 예약이 뒤 정책의 예약에 의존할 수 있는 스택 규칙을 보존한다.
 * bIsInvokingExternalPolicy Guard는 정책이 다시 Command/등록/복원을 시작하는 재진입을 실패 폐쇄한다.
 */
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

/* [권위 명령 진입점]
 * CommandId+PayloadFingerprint는 같은 요청의 안전한 멱등 재생을 제공한다. 같은 ID/같은 Payload면 저장된
 * 결과를 돌려주고, Payload가 다르면 거부한다. InFlight Guard는 정책 및 이벤트의 동기 콜백까지 유지되어
 * 중첩 명령이 반쯤 변경된 상태를 관찰하거나 덮어쓰지 못하게 한다.
 *
 * 성공 이벤트는 명령 결과와 Receipt가 준비된 뒤에만 발행하고, 실패는 WorldRevision을 올리지 않는다.
 * DeferredWorldChangeEvent는 Actor/점유/정책 확정 전 외부 구독자에게 중간 상태가 노출되는 것을 막는다.
 */
FFFZSCommandResult UFFZSWorldSubsystem::ExecuteCommand(const FFFZSCommand& Command)
{
    if (!IsInGameThread())
    {
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("GameThreadOnly", "Zone/Facility 명령은 Game Thread에서 실행해야 합니다."));
    }
    if (bIsRestoringSnapshot)
    {
        // 복원 과정에서 Actor를 생성하고 Blueprint/Native Callback을 호출할 수 있다. 해당 Callback은
        // 준비가 덜 된 권위 World를 관찰하거나 변경해서는 안 된다.
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
        Result = MakeFailure(EFFZSPlacementFailure::WrongStage, LOCTEXT("WrongCommandPhase", "Install, Move 및 Remove는 InitialSetup 또는 MorningCheck Phase에서만 허용됩니다."));
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

/* [명령별 적용과 복구]
 * - Install: 정책 임시 예약 뒤 Actor 생성/초기화/등록 중 실패하면 Actor를 파기하고 정책을 Rollback한다.
 * - Move: 이전 Zone/Placement/Transform과 점유 셀을 보존한 뒤 적용하며, 실패하면 모두 이전 값으로 복원한다.
 * - Remove: Queue가 비고 상태가 허용됨을 확인한 뒤 등록 해제/제거한다.
 * - Open/Close: Actor 상태 전환이 거부되면 임시 정책을 Rollback한다.
 * 정책 Commit은 로컬 변경이 성공한 뒤 호출하며, FinalizeSuccess만 Revision과 이벤트를 만든다.
 */
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
    OccupyFacilityRegions(*Facility);
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

/* [통합 정책 레지스트리]
 * 정책 이름은 전체에서, Custom이 아닌 Domain은 Domain별로 유일하다. 정렬된 순서는 실행/롤백 순서를
 * 결정적으로 만들고, Production 모드에서는 필수 Domain이 모두 준비되어야 명령이 진행된다.
 */
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

/* [Queue 권위 연결]
 * AgentQueueOwnership은 한 Agent의 중복 입장을 막는 월드 단위 인덱스다. JoinQueue의 인터페이스/델리게이트가
 * 동기 재진입할 수 있으므로 Component 호출 전에 소유권을 선점하고, Join 실패 시 같은 소유자일 때만
 * 되돌린다. Queue가 Agent를 해제하면 HandleAgentReleased가 인덱스를 함께 정리한다.
 */
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

/* [존 소속/밀도]
 * 직전 존에는 경계 Hysteresis를 적용해 흔들림을 줄이고, 벗어났다면 ZoneId 정렬 순서로 정확한 포함 존을
 * 찾는다. 정렬은 중첩 데이터가 들어왔을 때 결과를 결정적으로 만들지만, 중첩 경계 자체의 데이터 오류를
 * 정당화하지 않는다. Population 보고는 Zone Actor의 상승 Hold 규칙을 통해 DensityState를 갱신한다.
 */
FName UFFZSWorldSubsystem::ResolveZoneAtWorldLocation(const FVector& WorldLocation, const FName PreviousZoneId) const
{
    const float HysteresisCm = GetDefault<UFFZSDeveloperSettings>()->ZoneMembershipHysteresisCm;
    if (AFFZSGameplayZone* Previous = FindZone(PreviousZoneId); Previous && Previous->ContainsWorldPointWithHysteresis(WorldLocation, HysteresisCm))
    {
        return PreviousZoneId;
    }

    TArray<FName> ZoneIds;
    ZonesById.GenerateKeyArray(ZoneIds);
    ZoneIds.Sort(FNameLexicalLess());
    for (const FName ZoneId : ZoneIds)
    {
        if (AFFZSGameplayZone* Zone = FindZone(ZoneId); Zone && Zone->ContainsWorldPoint(WorldLocation))
        {
            return ZoneId;
        }
    }
    return NAME_None;
}

void UFFZSWorldSubsystem::ReportZonePopulation(const FName ZoneId, const int32 Population, const float DeltaSeconds)
{
    if (IsAuthorityMutationBlocked())
    {
        return;
    }
    if (AFFZSGameplayZone* Zone = FindZone(ZoneId))
    {
        Zone->UpdatePopulationSample(Population, DeltaSeconds);
    }
}

/* [조회 모델]
 * UI/외부 시스템에는 가변 Actor/Component 전체 대신 필요한 값만 복사한 ReadModel을 제공한다.
 * ZoneId와 FacilityInstanceId로 정렬하므로 TMap 순회 순서와 무관하게 결과가 결정적이다.
 */
FFFZSReadModel UFFZSWorldSubsystem::BuildReadModel() const
{
    FFFZSReadModel Result;
    Result.WorldRevision = WorldRevision;
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

/* [월드 스냅샷 저장]
 * 시설별 Actor/Queue 상태와 완료된 Command Receipt, WorldRevision, Phase를 하나의 스키마로 모은다.
 * 진행 중 Receipt는 저장하지 않고 ID로 정렬한다. 개별 Facility의 CaptureInstanceSnapshot은 구성요소
 * 데이터를 복사할 뿐이며, 이 함수가 여러 시설과 월드 메타데이터를 묶는 저장 단위다.
 */
FFFZSWorldSnapshot UFFZSWorldSubsystem::CaptureWorldSnapshot() const
{
    FFFZSWorldSnapshot Snapshot;
    Snapshot.SchemaVersion = CurrentSnapshotSchemaVersion;
    Snapshot.WorldRevision = WorldRevision;
    Snapshot.CurrentPhase = CurrentPhase;
    for (const TPair<FGuid, TWeakObjectPtr<AFFZSFacilityActor>>& Pair : FacilitiesById)
    {
        if (const AFFZSFacilityActor* Facility = Pair.Value.Get())
        {
            Snapshot.Facilities.Add(Facility->CaptureInstanceSnapshot());
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

/* [복원 전 전체 검증]
 * 실제 Actor를 교체하기 전에 스키마/Phase/ID/Spec/Zone/Transform/셀 중복/보호 구역/Queue 상태 조합/
 * Agent 중복/Receipt를 모두 검사한다. QueueValidator::RestoreSnapshot은 임시 Component 하나의 내부
 * 스냅샷을 검증·복원하는 기능일 뿐, 월드 레지스트리나 다른 Component까지 원자적으로 바꾸지 않는다.
 */
bool UFFZSWorldSubsystem::ValidateSnapshotBeforeRestore(const FFFZSWorldSnapshot& Snapshot, TArray<FText>& OutErrors) const
{
    OutErrors.Reset();
    if (Snapshot.SchemaVersion != CurrentSnapshotSchemaVersion)
    {
        OutErrors.Add(Snapshot.SchemaVersion == 2
            ? LOCTEXT("SnapshotSchemaV2", "Zone/Facility 스냅샷 SchemaVersion 2에는 저장된 CurrentPhase가 없어 안전하게 복원할 수 없습니다. SchemaVersion 4에서 사용하기 전에 마이그레이션 도구로 다시 저장하십시오.")
            : LOCTEXT("SnapshotSchema", "지원하지 않는 Zone/Facility 스냅샷 SchemaVersion입니다. 이 Runtime은 명시적인 SchemaVersion 3 마이그레이션 후 SchemaVersion 4를 검증합니다."));
        return false;
    }
    if (Snapshot.WorldRevision < 0)
    {
        OutErrors.Add(LOCTEXT("SnapshotRevision", "Zone/Facility 스냅샷의 WorldRevision은 음수일 수 없습니다."));
    }
    if (!IsKnownChangePhase(Snapshot.CurrentPhase))
    {
        OutErrors.Add(LOCTEXT("SnapshotPhase", "Zone/Facility 스냅샷의 CurrentPhase가 잘못되었습니다."));
    }

    TSet<FGuid> FacilityIds;
    TMap<FGuid, FFFZSFacilityRegionOccupancy> FacilityRegions;
    TSet<FName> Agents;
    const float CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    TMap<const AFFZSGameplayZone*, TUniquePtr<FZoneCellOwnershipCache>> ZoneOwnershipCaches;

    for (const FFFZSFacilityInstanceSnapshot& Saved : Snapshot.Facilities)
    {
        TArray<FText> SpecErrors;
        if (!Saved.FacilityInstanceId.IsValid() || FacilityIds.Contains(Saved.FacilityInstanceId) || !Saved.FacilitySpec.IsValid(SpecErrors))
        {
            OutErrors.Add(LOCTEXT("SnapshotFacility", "스냅샷에 잘못되었거나 중복된 Facility가 있습니다."));
            continue;
        }
        FacilityIds.Add(Saved.FacilityInstanceId);
        if (!StaticEnum<EFFZSFacilityState>()->IsValidEnumValue(static_cast<int64>(Saved.State)) ||
            Saved.State == EFFZSFacilityState::Removed)
        {
            OutErrors.Add(LOCTEXT("SnapshotFacilityState", "스냅샷의 Facility 상태가 잘못되었거나, 등록된 상태에서 Removed일 수 없습니다."));
        }
        if (Saved.Placement.QuarterTurns != UFFZSGridLibrary::NormalizeQuarterTurns(Saved.Placement.QuarterTurns))
        {
            OutErrors.Add(LOCTEXT("SnapshotPlacementRotation", "스냅샷 배치의 QuarterTurns는 0..3 범위로 정규화되어 있어야 합니다."));
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
            const FTransform Expected = BuildPlacementTransform(*Zone, Saved.FacilitySpec, Saved.Placement);
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

        const TArray<FFFZSCellRect> ReservedRects =
            UFFZSGridLibrary::GetReservedRects(Saved.FacilitySpec, Saved.Placement);
        if (ReservedRects.IsEmpty())
        {
            // 빈 배열은 정상 시설 영역이 아니라 좌표 변환/오버플로의 fail-closed 신호다.
            OutErrors.Add(LOCTEXT(
                "SnapshotEmptyCompressedRegion",
                "스냅샷 Facility에서 완전한 압축 1cm 배치 영역을 만들 수 없습니다."));
        }
        TUniquePtr<FZoneCellOwnershipCache>& ZoneOwnershipCache = ZoneOwnershipCaches.FindOrAdd(Zone);
        if (!ZoneOwnershipCache)
        {
            ZoneOwnershipCache = MakeUnique<FZoneCellOwnershipCache>(ZonesById, *Zone);
        }

        bool bOverlapsFacility = false;
        for (const TPair<FGuid, FFFZSFacilityRegionOccupancy>& Existing : FacilityRegions)
        {
            if (Existing.Value.ZoneId == Saved.ZoneId &&
                UFFZSGridLibrary::DoCellRectSetsOverlap(ReservedRects, Existing.Value.Regions))
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
        FFFZSFacilityRegionOccupancy& RegionRecord = FacilityRegions.Add(Saved.FacilityInstanceId);
        RegionRecord.ZoneId = Saved.ZoneId;
        RegionRecord.Regions = ReservedRects;

        bool bOutsideOrAmbiguousZone = false;
        for (const FFFZSCellRect& Rect : ReservedRects)
        {
            bOutsideOrAmbiguousZone |= !IsRectOwnedByExactlyOneZone(
                *ZoneOwnershipCache, Rect, CellSizeCm);
        }
        if (bOutsideOrAmbiguousZone)
        {
            OutErrors.Add(LOCTEXT(
                "SnapshotOutside",
                "스냅샷 Facility 영역은 정확히 하나의 Gameplay Zone 안에 완전히 포함되어야 합니다."));
        }

        FFFZSPlacementValidation ProtectedValidation;
        ProtectedValidation.ReservedRects = ReservedRects;
        ProtectedValidation.PlacementTransform = Saved.WorldTransform;
        ValidateProtectedAreas(Saved.ZoneId, *Zone, Saved.FacilitySpec, ProtectedValidation);
        if (!ProtectedValidation.Reasons.IsEmpty())
        {
            OutErrors.Add(ProtectedValidation.Summary.IsEmpty()
                ? LOCTEXT("SnapshotProtectedArea", "스냅샷 Facility가 보호 영역과 겹칩니다.")
                : ProtectedValidation.Summary);
        }
        UFFZSFacilityQueueComponent* QueueValidator = NewObject<UFFZSFacilityQueueComponent>();
        QueueValidator->Configure(Saved.FacilitySpec.QueueRules);
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

/*
 * [월드 스냅샷 복원의 원자성 경계]
 * Restore Guard는 검증 시작부터 게시 완료까지 Command, 정책 등록, Queue 변경 같은 권위 재진입을 막는다.
 * 먼저 숨겨진 새 Actor와 별도 Staged 맵들을 만들고 각 Queue Component를 복원한다. 이 중 하나라도 실패하면
 * 스테이징 Actor만 파기하고 기존 FacilitiesById/OccupiedFacilityRegions/AgentQueueOwnership/Receipt는 건드리지 않는다.
 *
 * 모든 실패 가능 단계가 끝난 뒤에만 Staged 레지스트리를 MoveTemp로 함께 게시하고 이전 Actor를 폐기한다.
 * 이것이 보장하는 원자성은 "Subsystem의 권위 월드 상태가 이전 세트 또는 새 세트로만 보인다"는 논리적
 * 원자성이다. 각 UObject 생성, Queue Component 복원, Actor Destroy가 한 CPU 명령으로 실행된다는 뜻은
 * 아니다. 단일 QueueComponent::RestoreSnapshot 성공만으로도 월드 복원이 완료된 것은 아니다.
 */
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
        // ExecuteCommand는 모든 Policy 및 Event Callback이 끝날 때까지 이 ID를 보호한다.
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

    if (!ValidateSnapshotBeforeRestore(*EffectiveSnapshot, OutErrors))
    {
        return false;
    }
    return RestoreWorldSnapshotUnchecked(*EffectiveSnapshot, OutErrors);
}

// 호출자는 Restore Guard와 전체 사전 검증을 이미 보유해야 한다. 스테이징 Actor는 ID 없이 Spawn되어
// BeginPlay 자동 등록을 피하고, 초기화 후에도 숨김/충돌 비활성 상태로 새 레지스트리가 게시될 때까지 격리한다.
bool UFFZSWorldSubsystem::RestoreWorldSnapshotUnchecked(const FFFZSWorldSnapshot& Snapshot, TArray<FText>& OutErrors)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        OutErrors.Add(LOCTEXT("RestoreMissingWorld", "유효한 World가 없으면 Zone/Facility 스냅샷을 복원할 수 없습니다."));
        return false;
    }

    // 전체 대체 상태를 먼저 준비한다. 모든 Actor, Queue, Cell, Agent 및 영수증이
    // 준비될 때까지 현재 권위 레지스트리와 Actor는 변경하지 않는다.
    TArray<TWeakObjectPtr<AFFZSFacilityActor>> StagedFacilities;
    TMap<FGuid, TWeakObjectPtr<AFFZSFacilityActor>> StagedFacilitiesById;
    TMap<FGuid, FFFZSFacilityRegionOccupancy> StagedOccupiedFacilityRegions;
    TMap<FName, FGuid> StagedAgentQueueOwnership;
    TMap<FGuid, FFFZSCommandReceipt> StagedCommandReceipts;
    TSet<FGuid> StagedCommandReceiptFacilityReferences;

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
                Saved.State))
        {
            OutErrors.Add(LOCTEXT("RestoreInitialize", "스냅샷 복원 중 준비된 Facility 초기화에 실패했습니다."));
            DestroyStagedFacilities();
            return false;
        }
        Facility->SetNeedsRelocation(Saved.bNeedsRelocation);

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

        const TArray<FFFZSCellRect> StagedRects =
            UFFZSGridLibrary::GetReservedRects(Saved.FacilitySpec, Saved.Placement);
        for (const TPair<FGuid, FFFZSFacilityRegionOccupancy>& Existing : StagedOccupiedFacilityRegions)
        {
            if (Existing.Value.ZoneId == Saved.ZoneId &&
                UFFZSGridLibrary::DoCellRectSetsOverlap(StagedRects, Existing.Value.Regions))
            {
                OutErrors.Add(LOCTEXT(
                    "RestoreOccupiedRegion",
                    "스냅샷 복원을 준비하는 동안 서로 겹치는 압축 영역이 발견되었습니다."));
                DestroyStagedFacilities();
                return false;
            }
        }
        FFFZSFacilityRegionOccupancy& StagedOccupancy =
            StagedOccupiedFacilityRegions.Add(Saved.FacilityInstanceId);
        StagedOccupancy.ZoneId = Saved.ZoneId;
        StagedOccupancy.Regions = StagedRects;

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

    // 이 지점 이후에는 실패할 수 있는 작업이 없다. 준비된 레지스트리를 한 번에 게시한 뒤
    // 기존 Actor를 정리한다. ID와 Actor 식별자를 함께 검사하므로 기존 Actor의 EndPlay가
    // 새 Actor를 등록 해제할 수 없다.
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
            Facility->SetActorEnableCollision(false);
            Facility->SetActorHiddenInGame(true);
        }
    }

    FacilitiesById = MoveTemp(StagedFacilitiesById);
    OccupiedFacilityRegions = MoveTemp(StagedOccupiedFacilityRegions);
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
        }
    }
    for (const TWeakObjectPtr<AFFZSFacilityActor>& WeakFacility : ExistingFacilities)
    {
        if (AFFZSFacilityActor* Facility = WeakFacility.Get(); IsValid(Facility))
        {
            Facility->Destroy();
        }
    }
    return true;
}
/* [월드 유지보수 검증]
 * Actor 단위 검증을 넘어 프로젝트 필수 Zone 존재, 이웃의 실제 존재/양방향성, ProtectedArea ID와
 * LinkedZone을 검사한다. RevalidateFacilitiesInZone은 변경된 경계에 기존 시설이 맞는지 다시 계산하고
 * 자동 이동시키지 않은 채 NeedsRelocation으로 표시해 담당자의 통제권을 보존한다.
 */
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
        const bool bValid = ValidatePlacementLocal(Command, Facility).bIsValid;
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

/* [점유 원장 도우미]
 * GUID마다 ZoneId와 정규화된 ReservedRects를 한 번 저장한다. 겹침은 사각형 broad/narrow 비교로 정확히
 * 판정하며, 1 cm 셀 수와 무관하게 시설 수×작은 사각형 수에 비례한다. Release는 자기 GUID 키만 지워
 * 오래된 Actor의 EndPlay가 다른 시설 점유를 제거하지 못하게 한다.
 */
void UFFZSWorldSubsystem::OccupyFacilityRegions(const AFFZSFacilityActor& Facility)
{
    FFFZSFacilityRegionOccupancy Record;
    Record.ZoneId = Facility.GetZoneId();
    Record.Regions = UFFZSGridLibrary::GetReservedRects(
        Facility.GetFacilitySpec(), Facility.GetGridPlacement());
    OccupiedFacilityRegions.Add(Facility.GetFacilityInstanceId(), MoveTemp(Record));
}

void UFFZSWorldSubsystem::ReleaseFacilityRegions(const AFFZSFacilityActor& Facility)
{
    OccupiedFacilityRegions.Remove(Facility.GetFacilityInstanceId());
}

bool UFFZSWorldSubsystem::AreRegionsOccupiedByOther(
    const FName ZoneId,
    const TArray<FFFZSCellRect>& Regions,
    const FGuid IgnoreFacilityId) const
{
    for (const TPair<FGuid, FFFZSFacilityRegionOccupancy>& Pair : OccupiedFacilityRegions)
    {
        if ((IgnoreFacilityId.IsValid() && Pair.Key == IgnoreFacilityId) ||
            Pair.Value.ZoneId != ZoneId)
        {
            continue;
        }
        if (UFFZSGridLibrary::DoCellRectSetsOverlap(Regions, Pair.Value.Regions))
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
