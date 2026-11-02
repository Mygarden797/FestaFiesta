#include "FFZAAutoWeld.h"
#include "FFZAOverlapRepair.h"
#include "FFZAPolygonMath.h"
#include "FFZASeamConsensus.h"
#include "FFZAGapAreaRepair.h"
#include "FFZABoundaryNormalize.h"

namespace FFZAAutoWeld
{
namespace
{
    constexpr double PointEpsilon = 1.e-4;
    constexpr double PointEpsilonSquared = PointEpsilon * PointEpsilon;
    // Newly generated editor boundaries must not contain microscopic edges that disappear
    // under ordinary polygon checks or the Zone system's coarser authoring grid.
    constexpr double AutomaticTopologyToleranceCm = 0.01;
    // A Boolean cut removes already occupied area. It does not bridge unrelated remote Zones.
    // Only seam-extension passes use the finite repair radius.
    constexpr double UnboundedTrim = TNumericLimits<double>::Max();

    bool SamePoint(const FVector2D& A, const FVector2D& B)
    {
        return FVector2D::DistSquared(A, B) <= PointEpsilonSquared;
    }

    double TranslatedSignedArea(const TArray<FVector2D>& Ring)
    {
        double TwiceArea = 0.0;
        for (int32 Point = 1; Point + 1 < Ring.Num(); ++Point)
        {
            const FVector2D A = Ring[Point] - Ring[0];
            const FVector2D B = Ring[Point + 1] - Ring[0];
            TwiceArea += A.X * B.Y - A.Y * B.X;
        }
        return TwiceArea * 0.5;
    }

    bool RingShapeChanged(const TArray<FVector2D>& Authored, const TArray<FVector2D>& Repaired)
    {
        // Exact shared-node insertion is not recovery of a rejected deformation.
        for (int32 Point = 0; Point < Repaired.Num(); ++Point)
        {
            if (!FFZAPolygonMath::IsPointOnBoundary(Authored, Repaired[Point], PointEpsilon) ||
                !FFZAPolygonMath::IsPointOnBoundary(Authored,
                    (Repaired[Point] + Repaired[(Point + 1) % Repaired.Num()]) * 0.5, PointEpsilon)) { return true; }
        }
        for (int32 Point = 0; Point < Authored.Num(); ++Point)
        {
            if (!FFZAPolygonMath::IsPointOnBoundary(Repaired, Authored[Point], PointEpsilon) ||
                !FFZAPolygonMath::IsPointOnBoundary(Repaired,
                    (Authored[Point] + Authored[(Point + 1) % Authored.Num()]) * 0.5, PointEpsilon)) { return true; }
        }
        return false;
    }

    bool ValidateFinalRings(const TArray<TArray<FVector2D>>& Authored, const TArray<TArray<FVector2D>>& Repaired,
        const double TopologyToleranceCm = PointEpsilon, FString* OutError = nullptr)
    {
        const auto Reject = [OutError](const FString& Reason)
        {
            if (OutError) { *OutError = Reason; }
            return false;
        };
        if (Authored.Num() != Repaired.Num()) { return Reject(TEXT("결과 경계의 Zone 수가 원본과 일치하지 않습니다.")); }
        TArray<FVector2D> Local;
        for (int32 Polygon = 0; Polygon < Repaired.Num(); ++Polygon)
        {
            const TArray<FVector2D>& Ring = Repaired[Polygon];
            if (Ring.Num() < 3) { return Reject(FString::Printf(TEXT("Zone %d: 결과 경계의 점이 3개 미만입니다."), Polygon + 1)); }
            Local.Reset(Ring.Num());
            for (const FVector2D& Point : Ring)
            {
                if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y))
                {
                    return Reject(FString::Printf(TEXT("Zone %d: 결과 경계에 유한하지 않은 좌표가 있습니다."), Polygon + 1));
                }
                Local.Add(Point - Ring[0]);
            }
            const double Area = TranslatedSignedArea(Local);
            if (!FMath::IsFinite(Area) || FMath::Abs(Area) <= PointEpsilonSquared ||
                Area * TranslatedSignedArea(Authored[Polygon]) <= 0.0)
            {
                return Reject(FString::Printf(TEXT("Zone %d: 결과 경계가 붕괴하거나 원본 방향이 뒤집혔습니다."), Polygon + 1));
            }
            if (!FFZAPolygonMath::IsSimplePolygon(Local, TopologyToleranceCm))
            {
                for (int32 Point = 0; Point < Ring.Num(); ++Point)
                {
                    const double Length = (Ring[Point] - Ring[(Point + 1) % Ring.Num()]).Size();
                    if (Length <= TopologyToleranceCm)
                    {
                        return Reject(FString::Printf(TEXT("Zone %d: %d번 경계 변이 %.9fcm로 너무 짧습니다."),
                            Polygon + 1, Point + 1, Length));
                    }
                }
                return Reject(FString::Printf(TEXT("Zone %d: 결과 경계가 자기교차하거나 비인접 변이 너무 가깝습니다."), Polygon + 1));
            }
        }
        return true;
    }

    bool RecountResult(const TArray<TArray<FVector2D>>& AuthoredPolygons, FResult& Result, FString* OutError = nullptr)
    {
        const auto Reject = [OutError](const FString& Reason)
        {
            if (OutError) { *OutError = Reason; }
            return false;
        };
        Result.ChangedPolygons.Init(false, AuthoredPolygons.Num());
        Result.Stats.MovedVertexCount = 0;
        Result.Stats.InsertedVertexCount = 0;
        Result.Stats.RemovedVertexCount = 0;
        Result.Stats.ChangedPolygonCount = 0;
        Result.Stats.MaximumMoveCm = 0.0;
        if (Result.Polygons.Num() != AuthoredPolygons.Num() ||
            Result.Stats.SourcePointKeys.Num() != AuthoredPolygons.Num()) { return Reject(TEXT("원본 높이 키의 Zone 수가 경계와 일치하지 않습니다.")); }
        for (int32 Polygon = 0; Polygon < AuthoredPolygons.Num(); ++Polygon)
        {
            const TArray<FVector2D>& Original = AuthoredPolygons[Polygon];
            const TArray<FVector2D>& Repaired = Result.Polygons[Polygon];
            const TArray<double>& Keys = Result.Stats.SourcePointKeys[Polygon];
            if (Keys.Num() != Repaired.Num())
            {
                return Reject(FString::Printf(TEXT("Zone %d: 경계점 %d개와 원본 높이 키 %d개의 수가 다릅니다."),
                    Polygon + 1, Repaired.Num(), Keys.Num()));
            }
            bool bChanged = Original.Num() != Repaired.Num();
            TSet<int32> MovedOriginalPoints;
            TSet<int32> RetainedOriginalPoints;
            for (int32 Point = 0; Point < Repaired.Num(); ++Point)
            {
                const double Key = Keys[Point];
                if (!FMath::IsFinite(Key) || Key < 0.0 || Key >= Original.Num() ||
                    !FMath::IsFinite(Repaired[Point].X) || !FMath::IsFinite(Repaired[Point].Y))
                {
                    return Reject(FString::Printf(TEXT("Zone %d: %d번 경계점의 원본 높이 키 %.17g가 유효하지 않습니다."),
                        Polygon + 1, Point + 1, Key));
                }
                const int32 Edge = FMath::FloorToInt(Key);
                const double Alpha = Key - static_cast<double>(Edge);
                const FVector2D Source = FMath::Lerp(Original[Edge], Original[(Edge + 1) % Original.Num()], Alpha);
                const double MoveSquared = FVector2D::DistSquared(Source, Repaired[Point]);
                Result.Stats.MaximumMoveCm = FMath::Max(Result.Stats.MaximumMoveCm, FMath::Sqrt(MoveSquared));
                if (Alpha == 0.0)
                {
                    RetainedOriginalPoints.Add(Edge);
                    if (MoveSquared > PointEpsilonSquared) { MovedOriginalPoints.Add(Edge); }
                }
                else { ++Result.Stats.InsertedVertexCount; }
                bChanged |= !Original.IsValidIndex(Point) || !SamePoint(Original[Point], Repaired[Point]);
            }
            Result.Stats.MovedVertexCount += MovedOriginalPoints.Num();
            Result.Stats.RemovedVertexCount += Original.Num() - RetainedOriginalPoints.Num();
            Result.ChangedPolygons[Polygon] = bChanged;
            Result.Stats.ChangedPolygonCount += bChanged ? 1 : 0;
        }
        return true;
    }

    bool ReferenceShapeUnchanged(const TArray<FVector2D>& Original, const TArray<FVector2D>& Repaired,
        const TArray<double>& Keys)
    {
        if (Keys.Num() != Repaired.Num()) { return false; }
        for (int32 Point = 0; Point < Repaired.Num(); ++Point)
        {
            const double Key = Keys[Point];
            if (!FMath::IsFinite(Key) || Key < 0.0 || Key >= Original.Num()) { return false; }
            const int32 Edge = FMath::FloorToInt(Key);
            if (!SamePoint(Repaired[Point], FMath::Lerp(Original[Edge], Original[(Edge + 1) % Original.Num()], Key - Edge)))
            {
                return false;
            }
        }
        for (const FVector2D& Point : Original)
        {
            if (!Repaired.ContainsByPredicate([&](const FVector2D& Candidate) { return SamePoint(Point, Candidate); })) { return false; }
        }
        return true;
    }

    void Inspect(FResult& Result)
    {
        FFZAWeldDiagnostics::Analyze(Result.Polygons, Result.Stats.RejectedPolygonIndices,
            Result.RepairRadiusCm, Result.Diagnostics);
    }

    double OriginalKeyForPoint(const TArray<FVector2D>& Authored, const FVector2D& Point)
    {
        double BestDistance = TNumericLimits<double>::Max();
        double BestKey = 0.0;
        for (int32 Edge = 0; Edge < Authored.Num(); ++Edge)
        {
            const FVector2D& A = Authored[Edge];
            const FVector2D& B = Authored[(Edge + 1) % Authored.Num()];
            const FVector2D Direction = B - A;
            const double LengthSquared = Direction.SizeSquared();
            if (LengthSquared <= PointEpsilonSquared) { continue; }
            const double Alpha = FMath::Clamp(FVector2D::DotProduct(Point - A, Direction) / LengthSquared, 0.0, 1.0);
            const double Distance = FVector2D::DistSquared(Point, A + Direction * Alpha);
            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                BestKey = Alpha == 1.0 ? static_cast<double>((Edge + 1) % Authored.Num()) : Edge + Alpha;
            }
        }
        return BestKey;
    }

    bool ParameterOnOriginalEdge(const double Key, const int32 Edge, const int32 Count, double& OutParameter)
    {
        if (Key >= Edge && Key <= Edge + 1.0) { OutParameter = Key - Edge; return true; }
        if (Edge == Count - 1 && Key == 0.0) { OutParameter = 1.0; return true; }
        return false;
    }

    bool ComposeOriginalKeys(const TArray<TArray<FVector2D>>& Authored,
        const TArray<TArray<FVector2D>>& BeforePass, const TArray<TArray<double>>& BeforeKeys,
        TArray<TArray<double>>& InOutPassKeys)
    {
        if (BeforePass.Num() != Authored.Num() || BeforeKeys.Num() != Authored.Num() ||
            InOutPassKeys.Num() != Authored.Num()) { return false; }
        for (int32 Polygon = 0; Polygon < Authored.Num(); ++Polygon)
        {
            const TArray<FVector2D>& Ring = BeforePass[Polygon];
            const TArray<double>& Keys = BeforeKeys[Polygon];
            if (Keys.Num() != Ring.Num()) { return false; }
            for (double& Key : InOutPassKeys[Polygon])
            {
                if (!FMath::IsFinite(Key) || Key < 0.0 || Key >= Ring.Num()) { return false; }
                const int32 Edge = FMath::FloorToInt(Key);
                const double Alpha = Key - Edge;
                if (Alpha == 0.0) { Key = Keys[Edge]; continue; }
                const int32 Next = (Edge + 1) % Ring.Num();
                const double A = Keys[Edge], B = Keys[Next];
                const int32 OriginalCount = Authored[Polygon].Num();
                const int32 FirstEdge = FMath::FloorToInt(A);
                bool bOnOriginalEdge = false;
                // Either orientation, including 0 -> the closing edge, keeps exact source
                // interpolation. Projecting an already moved edge would change its height key.
                for (const int32 Candidate : { FirstEdge, (FirstEdge + OriginalCount - 1) % OriginalCount })
                {
                    double StartParameter = 0.0, EndParameter = 0.0;
                    if (ParameterOnOriginalEdge(A, Candidate, OriginalCount, StartParameter) &&
                        ParameterOnOriginalEdge(B, Candidate, OriginalCount, EndParameter))
                    {
                        Key = Candidate + FMath::Lerp(StartParameter, EndParameter, Alpha);
                        if (Key >= OriginalCount) { Key = 0.0; }
                        bOnOriginalEdge = true;
                        break;
                    }
                }
                if (!bOnOriginalEdge)
                {
                    // A Boolean seam joining unrelated original edges has no single authored
                    // edge parameter. Its source position supplies a stable local height key.
                    Key = OriginalKeyForPoint(Authored[Polygon], FMath::Lerp(Ring[Edge], Ring[Next], Alpha));
                }
            }
        }
        return true;
    }

    bool InterpolateMovementOrigins(const TArray<TArray<FVector2D>>& Before,
        const TArray<TArray<FVector2D>>& Origins, const TArray<TArray<double>>& PassKeys,
        TArray<TArray<FVector2D>>& OutOrigins)
    {
        if (Before.Num() != Origins.Num() || Before.Num() != PassKeys.Num()) { return false; }
        OutOrigins.SetNum(Before.Num());
        for (int32 Polygon = 0; Polygon < Before.Num(); ++Polygon)
        {
            if (Before[Polygon].Num() != Origins[Polygon].Num()) { return false; }
            OutOrigins[Polygon].Reset(PassKeys[Polygon].Num());
            for (const double Key : PassKeys[Polygon])
            {
                if (!FMath::IsFinite(Key) || Key < 0.0 || Key >= Before[Polygon].Num()) { return false; }
                const int32 Edge = FMath::FloorToInt(Key);
                OutOrigins[Polygon].Add(FMath::Lerp(Origins[Polygon][Edge],
                    Origins[Polygon][(Edge + 1) % Before[Polygon].Num()], Key - Edge));
            }
        }
        return true;
    }

    bool InheritTrimMovementOrigins(const TArray<TArray<FVector2D>>& Before,
        const TArray<TArray<FVector2D>>& Origins, const TArray<TArray<FVector2D>>& Trimmed,
        TArray<TArray<FVector2D>>& OutOrigins)
    {
        if (Before.Num() != Origins.Num() || Before.Num() != Trimmed.Num()) { return false; }
        OutOrigins.SetNum(Before.Num());
        for (int32 Polygon = 0; Polygon < Before.Num(); ++Polygon)
        {
            if (Before[Polygon].Num() != Origins[Polygon].Num()) { return false; }
            OutOrigins[Polygon].Reset(Trimmed[Polygon].Num());
            for (const FVector2D& Point : Trimmed[Polygon])
            {
                FVector2D Origin = Point;
                for (int32 Edge = 0; Edge < Before[Polygon].Num(); ++Edge)
                {
                    const int32 Next = (Edge + 1) % Before[Polygon].Num();
                    const FVector2D& A = Before[Polygon][Edge];
                    const FVector2D Direction = Before[Polygon][Next] - A;
                    const double LengthSquared = Direction.SizeSquared();
                    if (LengthSquared <= PointEpsilonSquared) { continue; }
                    const double Alpha = FMath::Clamp(FVector2D::DotProduct(Point - A, Direction) / LengthSquared, 0.0, 1.0);
                    if (SamePoint(Point, A + Direction * Alpha))
                    {
                        Origin = FMath::Lerp(Origins[Polygon][Edge], Origins[Polygon][Next], Alpha);
                        break;
                    }
                }
                // Retained edges keep their accumulated seam budget. A new interior Boolean
                // corner starts at the cut itself: removing occupied area is not gap extension.
                OutOrigins[Polygon].Add(Origin);
            }
        }
        return true;
    }

    bool MovementOriginsWithinRadius(const TArray<TArray<FVector2D>>& Rings,
        const TArray<TArray<FVector2D>>& Origins, const double Radius)
    {
        if (Rings.Num() != Origins.Num()) { return false; }
        const double BoundSquared = FMath::Square(Radius + PointEpsilon);
        for (int32 Polygon = 0; Polygon < Rings.Num(); ++Polygon)
        {
            if (Rings[Polygon].Num() != Origins[Polygon].Num()) { return false; }
            for (int32 Point = 0; Point < Rings[Polygon].Num(); ++Point)
            {
                const double DistanceSquared = FVector2D::DistSquared(Rings[Polygon][Point], Origins[Polygon][Point]);
                if (!FMath::IsFinite(DistanceSquared) || DistanceSquared > BoundSquared) { return false; }
            }
        }
        return true;
    }

    // Resolve existing overlap before snapping as a bounded alternative. A narrow sliver can
    // make the initial snap self-intersect even though subtracting the overlap first is safe.
    bool TryCleanupFirst(const TArray<TArray<FVector2D>>& Authored, const int32 Reference,
        const FResult& Initial, FResult& OutAlternative)
    {
        OutAlternative = FResult();
        OutAlternative.Polygons = Authored;
        OutAlternative.RepairRadiusCm = Initial.RepairRadiusCm;
        OutAlternative.bExpandedRadiusUsed = true;
        OutAlternative.Stats.Groups = Initial.Stats.Groups;
        OutAlternative.Stats.SourcePointKeys.SetNum(Authored.Num());
        for (int32 Polygon = 0; Polygon < Authored.Num(); ++Polygon)
        {
            for (int32 Point = 0; Point < Authored[Polygon].Num(); ++Point)
            {
                OutAlternative.Stats.SourcePointKeys[Polygon].Add(static_cast<double>(Point));
            }
        }
        FString Error;
        if (!FFZAOverlapRepair::Repair(OutAlternative.Polygons, Reference, Initial.Stats.Groups,
            OutAlternative.Stats.SourcePointKeys, Authored, UnboundedTrim, Error)) { return false; }
        const TArray<TArray<FVector2D>> Cleaned = OutAlternative.Polygons;
        const TArray<TArray<double>> CleanedKeys = OutAlternative.Stats.SourcePointKeys;
        FFZALayoutOps::WeldSelectedPolygonGroups(OutAlternative.Polygons, Reference, Initial.RepairRadiusCm,
            OutAlternative.ChangedPolygons, &OutAlternative.Stats);
        // Measure this snap relative to the already-trimmed geometry. Large original-to-cut
        // distances belong to Boolean removal, not to extension across a distant gap.
        if (!FMath::IsFinite(OutAlternative.Stats.MaximumMoveCm) ||
            OutAlternative.Stats.MaximumMoveCm > Initial.RepairRadiusCm + PointEpsilon) { return false; }
        if (!ComposeOriginalKeys(Authored, Cleaned, CleanedKeys, OutAlternative.Stats.SourcePointKeys)) { return false; }
        if (!FFZAOverlapRepair::Repair(OutAlternative.Polygons, Reference, OutAlternative.Stats.Groups,
            OutAlternative.Stats.SourcePointKeys, Authored, UnboundedTrim, Error)) { return false; }
        Inspect(OutAlternative);
        return OutAlternative.Diagnostics.GapPairCount == 0 && OutAlternative.Diagnostics.OverlapPairCount == 0;
    }
}

bool BuildResult(const TArray<TArray<FVector2D>>& AuthoredPolygons, const int32 PreferredReferencePolygonIndex,
    const double ToleranceCm, FResult& OutResult)
{
    OutResult = FResult();
    OutResult.Polygons = AuthoredPolygons;
    const double RepairRadius = ToleranceCm * 2.0;
    if (!FMath::IsFinite(ToleranceCm) || ToleranceCm <= 0.0 ||
        !FMath::IsFinite(RepairRadius) || !FMath::IsFinite(RepairRadius * RepairRadius))
    {
        OutResult.ChangedPolygons.Init(false, AuthoredPolygons.Num());
        OutResult.FailureReason = TEXT("용접 허용 거리는 유한한 양수여야 합니다.");
        return false;
    }
    OutResult.RepairRadiusCm = RepairRadius;
    FFZALayoutOps::WeldSelectedPolygonGroups(OutResult.Polygons, PreferredReferencePolygonIndex,
        ToleranceCm, OutResult.ChangedPolygons, &OutResult.Stats);
    if (OutResult.Stats.bInvalidReference)
    {
        OutResult.FailureReason = TEXT("선택한 기준 Zone의 경계가 유효하지 않습니다.");
        Inspect(OutResult);
        return false;
    }
    TArray<int32> RejectedTargets = OutResult.Stats.RejectedPolygonIndices;
    Inspect(OutResult);
    if (OutResult.Diagnostics.GapPairCount > 0 || OutResult.Diagnostics.OverlapPairCount > 0)
    {
        // Start again from authored input. This prevents accumulated movement and guarantees that
        // the expanded component's manual/largest-area anchor owns the seam from the first pass.
        OutResult.Polygons = AuthoredPolygons;
        OutResult.bExpandedRadiusUsed = true;
        FFZALayoutOps::WeldSelectedPolygonGroups(OutResult.Polygons, PreferredReferencePolygonIndex,
            RepairRadius, OutResult.ChangedPolygons, &OutResult.Stats);
        Inspect(OutResult);
    }
    FString OverlapFailure;
    for (const int32 Rejected : OutResult.Stats.RejectedPolygonIndices) { RejectedTargets.AddUnique(Rejected); }
    bool bWeldMovementBounded = FMath::IsFinite(OutResult.Stats.MaximumMoveCm) &&
        OutResult.Stats.MaximumMoveCm <= RepairRadius + PointEpsilon;
    bool bOverlapRepairSucceeded = OutResult.Diagnostics.OverlapPairCount == 0 ||
        FFZAOverlapRepair::Repair(OutResult.Polygons, PreferredReferencePolygonIndex, OutResult.Stats.Groups,
            OutResult.Stats.SourcePointKeys, AuthoredPolygons, UnboundedTrim, OverlapFailure);
    Inspect(OutResult);
    if (OutResult.Stats.InvalidInputPolygonCount == 0 &&
        (OutResult.Diagnostics.GapPairCount > 0 || !bOverlapRepairSucceeded))
    {
        FResult Alternative;
        if (TryCleanupFirst(AuthoredPolygons, PreferredReferencePolygonIndex, OutResult, Alternative))
        {
            for (const int32 Polygon : Alternative.Stats.RejectedPolygonIndices) { RejectedTargets.AddUnique(Polygon); }
            OutResult = MoveTemp(Alternative);
            bOverlapRepairSucceeded = true;
            bWeldMovementBounded = true;
            OverlapFailure.Reset();
        }
    }
    const bool bProvenanceValid = RecountResult(AuthoredPolygons, OutResult);
    const bool bFinalRingsValid = ValidateFinalRings(AuthoredPolygons, OutResult.Polygons);
    const bool bReferenceFixed = bProvenanceValid && ReferenceShapeUnchanged(AuthoredPolygons[PreferredReferencePolygonIndex],
        OutResult.Polygons[PreferredReferencePolygonIndex], OutResult.Stats.SourcePointKeys[PreferredReferencePolygonIndex]);

    // Retire a rejected proposal only when a later stage actually changed that target's shape.
    // Zero reported gaps alone must not turn an unchanged, rejected target into a successful
    // plan. An actual shape repair is required before retiring a rejected proposal.
    OutResult.Stats.RejectedPolygonIndices.Reset();
    for (const int32 Polygon : RejectedTargets)
    {
        if (!AuthoredPolygons.IsValidIndex(Polygon) || !OutResult.Polygons.IsValidIndex(Polygon) ||
            !RingShapeChanged(AuthoredPolygons[Polygon], OutResult.Polygons[Polygon]))
        {
            OutResult.Stats.RejectedPolygonIndices.Add(Polygon);
        }
    }
    OutResult.Stats.RejectedPolygonCount = OutResult.Stats.RejectedPolygonIndices.Num();
    FFZAWeldDiagnostics::FResult FinalDiagnostics;
    FFZAWeldDiagnostics::Analyze(OutResult.Polygons, OutResult.Stats.RejectedPolygonIndices, RepairRadius, FinalDiagnostics);
    const bool bResidualsCleared = FinalDiagnostics.GapPairCount == 0 && FinalDiagnostics.OverlapPairCount == 0;
    OutResult.bCanCommit = bOverlapRepairSucceeded && bResidualsCleared && bProvenanceValid &&
        bFinalRingsValid && bReferenceFixed && bWeldMovementBounded && OutResult.Stats.InvalidInputPolygonCount == 0 &&
        OutResult.Stats.RejectedPolygonCount == 0;
    if (OutResult.bCanCommit)
    {
        OutResult.Stats.DisconnectedPolygonIndices.Reset();
        OutResult.Diagnostics = MoveTemp(FinalDiagnostics);
    }
    else
    {
        if (!bOverlapRepairSucceeded) { OutResult.FailureReason = MoveTemp(OverlapFailure); }
        else if (OutResult.Stats.InvalidInputPolygonCount > 0)
        {
            OutResult.FailureReason = TEXT("유효하지 않은 선택 경계가 있어 전체 자동 적용을 완료할 수 없습니다.");
        }
        else if (!bProvenanceValid || !bReferenceFixed || !bFinalRingsValid)
        {
            OutResult.FailureReason = TEXT("원본 높이 보간 또는 기준 경계 보존 검증에 실패했습니다.");
        }
        else if (!bWeldMovementBounded)
        {
            OutResult.FailureReason = TEXT("자동 복구 범위를 넘는 경계 이동이 필요합니다.");
        }
        else if (OutResult.Stats.RejectedPolygonCount > 0)
        {
            OutResult.FailureReason = FString::Printf(TEXT("거부된 Zone %d개의 경계가 아직 복구되지 않아 적용하지 않았습니다."),
                OutResult.Stats.RejectedPolygonCount);
        }
        else
        {
            OutResult.FailureReason = FString::Printf(TEXT("자동 복구 후에도 틈 %d쌍, 겹침 %d쌍이 남아 적용하지 않았습니다."),
                FinalDiagnostics.GapPairCount, FinalDiagnostics.OverlapPairCount);
        }
        Inspect(OutResult);
    }
    return true;
}

bool BuildAutomaticResult(const TArray<TArray<FVector2D>>& AuthoredPolygons,
    const double ToleranceCm, FResult& OutResult)
{
    OutResult = FResult();
    OutResult.Polygons = AuthoredPolygons;
    const double Radius = ToleranceCm * 2.0;
    OutResult.RepairRadiusCm = Radius;
    OutResult.bExpandedRadiusUsed = true;
    OutResult.ChangedPolygons.Init(false, AuthoredPolygons.Num());
    OutResult.Stats.SourcePointKeys.SetNum(AuthoredPolygons.Num());
    if (AuthoredPolygons.Num() < 2 || !FMath::IsFinite(ToleranceCm) || ToleranceCm <= 0.0 ||
        !FMath::IsFinite(Radius) || !FMath::IsFinite(Radius * Radius))
    {
        OutResult.FailureReason = TEXT("최소 두 개의 Zone과 유한한 양수인 허용 거리가 필요합니다.");
        return false;
    }

    // Identity keys seed the fixed snapshot. Consensus validates the complete input once,
    // checking its point budget before the more expensive per-ring topology validation.
    for (int32 Polygon = 0; Polygon < AuthoredPolygons.Num(); ++Polygon)
    {
        const TArray<FVector2D>& Ring = AuthoredPolygons[Polygon];
        for (int32 Point = 0; Point < Ring.Num(); ++Point)
        {
            OutResult.Stats.SourcePointKeys[Polygon].Add(static_cast<double>(Point));
        }
    }

    // Boolean trimming still needs deterministic ownership for deep intersections, but no
    // Zone is held fixed during seam consensus. Pick an owner by shape, never Zone ID/path.
    const auto OverlapOwner = [](const TArray<TArray<FVector2D>>& Rings)
    {
        int32 Best = 0;
        const auto PointBefore = [](const FVector2D& A, const FVector2D& B)
        {
            return A.X != B.X ? A.X < B.X : A.Y < B.Y;
        };
        const auto Canonical = [&PointBefore](const TArray<FVector2D>& Ring, int32& Start, int32& Direction)
        {
            Start = 0;
            for (int32 Point = 1; Point < Ring.Num(); ++Point)
            {
                if (PointBefore(Ring[Point], Ring[Start])) { Start = Point; }
            }
            Direction = PointBefore(Ring[(Start + 1) % Ring.Num()],
                Ring[(Start + Ring.Num() - 1) % Ring.Num()]) ? 1 : -1;
        };
        for (int32 Candidate = 1; Candidate < Rings.Num(); ++Candidate)
        {
            const double A = FMath::Abs(TranslatedSignedArea(Rings[Candidate]));
            const double B = FMath::Abs(TranslatedSignedArea(Rings[Best]));
            if (A > B) { Best = Candidate; continue; }
            if (A < B) { continue; }
            if (Rings[Candidate].Num() != Rings[Best].Num())
            {
                if (Rings[Candidate].Num() < Rings[Best].Num()) { Best = Candidate; }
                continue;
            }
            int32 StartA, DirectionA, StartB, DirectionB;
            Canonical(Rings[Candidate], StartA, DirectionA);
            Canonical(Rings[Best], StartB, DirectionB);
            const int32 Count = Rings[Candidate].Num();
            for (int32 Point = 0; Point < Count; ++Point)
            {
                const FVector2D& P = Rings[Candidate][(StartA + DirectionA * Point + Count) % Count];
                const FVector2D& Q = Rings[Best][(StartB + DirectionB * Point + Count) % Count];
                if (P == Q) { continue; }
                if (PointBefore(P, Q)) { Best = Candidate; }
                break;
            }
        }
        return Best;
    };

    bool bPlanValid = true;
    bool bSeamMovementBounded = true;
    TArray<TArray<FVector2D>> MovementOrigins = AuthoredPolygons;
    TArray<int32> RejectedTargets;
    constexpr int32 MaximumStages = 4;
    for (int32 Stage = 0; Stage < MaximumStages; ++Stage)
    {
        const TArray<TArray<FVector2D>> BeforeStage = OutResult.Polygons;
        const TArray<TArray<double>> BeforeKeys = OutResult.Stats.SourcePointKeys;
        FFZASeamConsensus::FResult Seams;
        const bool bSeamsBuilt = FFZASeamConsensus::Build(BeforeStage, Radius, Seams, &MovementOrigins);
        if (!bSeamsBuilt || Seams.bBlocked)
        {
            OutResult.FailureReason = Seams.FailureReason;
            if (Stage == 0 && !bSeamsBuilt && !Seams.RejectedPolygonIndices.IsEmpty())
            {
                OutResult.Stats.InvalidInputPolygonIndices = MoveTemp(Seams.RejectedPolygonIndices);
                OutResult.Stats.InvalidInputPolygonCount = OutResult.Stats.InvalidInputPolygonIndices.Num();
                Inspect(OutResult);
                return true;
            }
            // A hard processing budget is an incomplete computation, even if an empty display
            // counter looks clean. Preserve the explicit failure without another expensive scan.
            if (Seams.bBlocked)
            {
                RecountResult(AuthoredPolygons, OutResult);
                return true;
            }
            bPlanValid = false;
            break;
        }
        if (Stage == 0)
        {
            // A geometrically complete input is already welded. Exact shared-edge noding
            // can otherwise add redundant points on every reapply of an area-filled result.
            // Consensus has validated all source rings and processing budgets above.
            Inspect(OutResult);
            if (OutResult.Diagnostics.GapPairCount == 0 && OutResult.Diagnostics.OverlapPairCount == 0)
            {
                OutResult.Stats.Groups = MoveTemp(Seams.Groups);
                OutResult.bCanCommit = RecountResult(AuthoredPolygons, OutResult) &&
                    ValidateFinalRings(AuthoredPolygons, OutResult.Polygons);
                return true;
            }
        }
        for (const int32 Rejected : Seams.RejectedPolygonIndices) { RejectedTargets.AddUnique(Rejected); }
        TArray<TArray<FVector2D>> SeamOrigins;
        if (!InterpolateMovementOrigins(BeforeStage, MovementOrigins, Seams.SourcePointKeys, SeamOrigins))
        {
            OutResult.FailureReason = TEXT("공통 경계의 누적 이동 기준을 검증하지 못했습니다.");
            bPlanValid = false;
            break;
        }
        MovementOrigins = MoveTemp(SeamOrigins);
        OutResult.Polygons = MoveTemp(Seams.Polygons);
        OutResult.Stats.Groups = MoveTemp(Seams.Groups);
        OutResult.Stats.SourcePointKeys = MoveTemp(Seams.SourcePointKeys);
        if (Stage > 0 && !ComposeOriginalKeys(AuthoredPolygons, BeforeStage, BeforeKeys, OutResult.Stats.SourcePointKeys))
        {
            OutResult.FailureReason = TEXT("공통 경계의 원본 높이 보간 키를 합성하지 못했습니다.");
            bPlanValid = false;
            break;
        }
        FString StageValidationError;
        if (!RecountResult(AuthoredPolygons, OutResult, &StageValidationError) ||
            !ValidateFinalRings(AuthoredPolygons, OutResult.Polygons, AutomaticTopologyToleranceCm, &StageValidationError))
        {
            OutResult.FailureReason = MoveTemp(StageValidationError);
            bPlanValid = false;
            break;
        }
        if (!MovementOriginsWithinRadius(OutResult.Polygons, MovementOrigins, Radius))
        {
            bSeamMovementBounded = false;
            OutResult.FailureReason = TEXT("공통 경계를 만들기 위해 자동 연결 이동 한도를 넘어야 합니다.");
            break;
        }
        Inspect(OutResult);
        if (OutResult.Diagnostics.OverlapPairCount > 0)
        {
            FString Error;
            const TArray<TArray<FVector2D>> BeforeTrim = OutResult.Polygons;
            if (!FFZAOverlapRepair::Repair(OutResult.Polygons, OverlapOwner(OutResult.Polygons),
                OutResult.Stats.Groups, OutResult.Stats.SourcePointKeys, AuthoredPolygons, UnboundedTrim, Error))
            {
                OutResult.FailureReason = MoveTemp(Error);
                bPlanValid = false;
                break;
            }
            TArray<TArray<FVector2D>> TrimOrigins;
            if (!InheritTrimMovementOrigins(BeforeTrim, MovementOrigins, OutResult.Polygons, TrimOrigins))
            {
                OutResult.FailureReason = TEXT("겹침 정리 후 공통 경계의 누적 이동 기준을 검증하지 못했습니다.");
                bPlanValid = false;
                break;
            }
            MovementOrigins = MoveTemp(TrimOrigins);
            Inspect(OutResult);
        }
        if (OutResult.Diagnostics.GapPairCount == 0 && OutResult.Diagnostics.OverlapPairCount == 0) { break; }
        if (OutResult.Polygons == BeforeStage)
        {
            OutResult.FailureReason = Seams.FailureReason.IsEmpty()
                ? TEXT("현재 인접 경계에서 더 이상 안전하게 연결할 수 있는 구간이 없습니다.") : Seams.FailureReason;
            break;
        }
    }
    const bool bKeysValid = RecountResult(AuthoredPolygons, OutResult);
    const bool bRingsValid = ValidateFinalRings(AuthoredPolygons, OutResult.Polygons, AutomaticTopologyToleranceCm);
    bSeamMovementBounded &= MovementOriginsWithinRadius(OutResult.Polygons, MovementOrigins, Radius);
    OutResult.Stats.RejectedPolygonIndices.Reset();
    for (const int32 Polygon : RejectedTargets)
    {
        if (!AuthoredPolygons.IsValidIndex(Polygon) || !OutResult.Polygons.IsValidIndex(Polygon) ||
            !RingShapeChanged(AuthoredPolygons[Polygon], OutResult.Polygons[Polygon]))
        {
            OutResult.Stats.RejectedPolygonIndices.Add(Polygon);
        }
    }
    OutResult.Stats.RejectedPolygonCount = OutResult.Stats.RejectedPolygonIndices.Num();
    Inspect(OutResult);
    OutResult.bCanCommit = bPlanValid && bKeysValid && bRingsValid && bSeamMovementBounded &&
        OutResult.Stats.RejectedPolygonCount == 0 && OutResult.Diagnostics.GapPairCount == 0 &&
        OutResult.Diagnostics.OverlapPairCount == 0;
    if (!OutResult.bCanCommit)
    {
        // Restart from the snapshot, not from a partly moved proposal. Filling only exposed
        // empty strips avoids crossing or collapsing original corners in narrow/concave Zones.
        FResult AreaAlternative;
        AreaAlternative.Polygons = AuthoredPolygons;
        AreaAlternative.RepairRadiusCm = Radius;
        AreaAlternative.bExpandedRadiusUsed = true;
        AreaAlternative.bAreaFallbackUsed = true;
        AreaAlternative.Stats.Groups = OutResult.Stats.Groups;
        AreaAlternative.Stats.SourcePointKeys.SetNum(AuthoredPolygons.Num());
        for (int32 Polygon = 0; Polygon < AuthoredPolygons.Num(); ++Polygon)
        {
            for (int32 Point = 0; Point < AuthoredPolygons[Polygon].Num(); ++Point)
            {
                AreaAlternative.Stats.SourcePointKeys[Polygon].Add(static_cast<double>(Point));
            }
        }
        Inspect(AreaAlternative);
        FString AreaError;
        const bool bTrimmed = AreaAlternative.Diagnostics.OverlapPairCount == 0 ||
            FFZAOverlapRepair::Repair(AreaAlternative.Polygons, OverlapOwner(AreaAlternative.Polygons),
                AreaAlternative.Stats.Groups, AreaAlternative.Stats.SourcePointKeys, AuthoredPolygons,
                UnboundedTrim, AreaError);
        if (bTrimmed)
        {
            const TArray<TArray<FVector2D>> BeforeArea = AreaAlternative.Polygons;
            const TArray<TArray<double>> BeforeAreaKeys = AreaAlternative.Stats.SourcePointKeys;
            FFZAGapAreaRepair::FResult Areas;
            const bool bAreasBuilt = FFZAGapAreaRepair::Build(BeforeArea, Radius, Areas);
            if (bAreasBuilt && !Areas.bBlocked)
            {
                AreaAlternative.Polygons = MoveTemp(Areas.Polygons);
                AreaAlternative.Stats.SourcePointKeys = MoveTemp(Areas.SourcePointKeys);
                const bool bAreaKeysComposed = ComposeOriginalKeys(AuthoredPolygons, BeforeArea,
                    BeforeAreaKeys, AreaAlternative.Stats.SourcePointKeys);
                FString AreaValidationError;
                const bool bAreaNormalized = bAreaKeysComposed && FFZABoundaryNormalize::Normalize(
                    AreaAlternative.Polygons, AreaAlternative.Stats.SourcePointKeys, AuthoredPolygons,
                    AutomaticTopologyToleranceCm, AreaValidationError);
                const bool bAreaMovementBounded = bAreaNormalized && FFZAGapAreaRepair::ValidateRepairBand(
                    BeforeArea, AreaAlternative.Polygons, Radius, AreaValidationError);
                const bool bAreaKeysValid = bAreaMovementBounded && RecountResult(AuthoredPolygons, AreaAlternative, &AreaValidationError);
                const bool bAreaRingsValid = ValidateFinalRings(AuthoredPolygons, AreaAlternative.Polygons,
                    AutomaticTopologyToleranceCm, bAreaKeysValid ? &AreaValidationError : nullptr);
                Inspect(AreaAlternative);
                // Coordinated cleanup can close the last numerical gap of an otherwise
                // incomplete area pass. Only the full independent final inspection decides
                // completion; hard area processing budgets were already rejected above.
                AreaAlternative.bCanCommit = bAreaKeysValid && bAreaRingsValid &&
                    AreaAlternative.Diagnostics.GapPairCount == 0 && AreaAlternative.Diagnostics.OverlapPairCount == 0;
                if (AreaAlternative.bCanCommit)
                {
                    OutResult = MoveTemp(AreaAlternative);
                }
                else
                {
                    // Show the valid alternate geometry and its own residuals, rather than a
                    // stale corner-movement failure when area recovery also needs attention.
                    AreaAlternative.FailureReason = !AreaValidationError.IsEmpty() ? MoveTemp(AreaValidationError) :
                        (Areas.FailureReason.IsEmpty() ? TEXT("틈 영역 복구 후 원본 높이 키를 합성하지 못했습니다.") : MoveTemp(Areas.FailureReason));
                    if (bAreaKeysValid && bAreaRingsValid) { OutResult = MoveTemp(AreaAlternative); }
                    else { OutResult.FailureReason = MoveTemp(AreaAlternative.FailureReason); }
                }
            }
            else if (!Areas.FailureReason.IsEmpty())
            {
                OutResult.FailureReason = MoveTemp(Areas.FailureReason);
            }
        }
        else if (!AreaError.IsEmpty())
        {
            OutResult.FailureReason = MoveTemp(AreaError);
        }
    }
    if (OutResult.bCanCommit) { OutResult.FailureReason.Reset(); }
    else if (OutResult.FailureReason.IsEmpty())
    {
        OutResult.FailureReason = FString::Printf(TEXT("자동 연결 후 틈 %d쌍, 겹침 %d쌍 또는 경계 검증 문제가 남았습니다."),
            OutResult.Diagnostics.GapPairCount, OutResult.Diagnostics.OverlapPairCount);
    }
    return true;
}
}
