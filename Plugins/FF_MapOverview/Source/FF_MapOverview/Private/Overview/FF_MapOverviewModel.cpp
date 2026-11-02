#include "FF_MapOverviewModel.h"

namespace
{
    // 지도 UV의 (0.5, 0.5)가 중심이다. 지형 머티리얼의 원형 마스크와 같은
    // 반경을 선·시설·클릭 판정에 사용해야 화면과 입력의 경계가 일치한다.
    constexpr double MapRadius = 0.45;
    constexpr double MapRadiusSquared = MapRadius * MapRadius;
    constexpr int32 CircleClipEdgeCount = 64;
}

bool FFFMapOverviewProjection::SetBounds(const FBox& InBounds)
{
    Bounds = InBounds;
    bLocalWindow = false;
    Scale = 0.0;
    if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN()) return false;
    const FVector Size = Bounds.GetSize();
    const double Diagonal = FVector2D(Size.X, Size.Y).Size();
    if (!FMath::IsFinite(Diagonal) || Diagonal < 1.0 || Size.X <= 0.0 || Size.Y <= 0.0) return false;
    Center = Bounds.GetCenter();
    Scale = 0.84 / Diagonal;
    return true;
}

bool FFFMapOverviewProjection::SetLocalBounds(const FVector& InCenter, const double DiameterCm)
{
    Scale = 0.0;
    bLocalWindow = true;
    if (InCenter.ContainsNaN() || !FMath::IsFinite(DiameterCm) || DiameterCm < 1.0)
    {
        Bounds = FBox(ForceInit);
        return false;
    }
    Center = InCenter;
    const FVector Extent(DiameterCm * 0.5, DiameterCm * 0.5, 0.0);
    Bounds = FBox(Center - Extent, Center + Extent);
    Scale = (2.0 * MapRadius) / DiameterCm;
    return true;
}

bool FFFMapOverviewProjection::IsWorldPointVisible(const FVector& WorldPoint) const
{
    if (!IsValid() || WorldPoint.ContainsNaN()) return false;
    const FVector2D Point = WorldToMap(WorldPoint);
    return (Point - FVector2D(0.5, 0.5)).SizeSquared() <= MapRadiusSquared + UE_DOUBLE_SMALL_NUMBER;
}

bool FFFMapOverviewProjection::GetTerrainUVTransform(const FBox& TerrainBounds, FVector2D& OutUVCenter, double& OutUVScale) const
{
    if (!IsValid() || !TerrainBounds.IsValid || TerrainBounds.Min.ContainsNaN() || TerrainBounds.Max.ContainsNaN()) return false;
    const FVector TerrainSize = TerrainBounds.GetSize();
    if (TerrainSize.X < 1.0 || TerrainSize.Y < 1.0) return false;
    const FVector TerrainCenter = TerrainBounds.GetCenter();
    OutUVCenter = FVector2D(0.5 + (Center.Y - TerrainCenter.Y) / TerrainSize.Y,
        0.5 - (Center.X - TerrainCenter.X) / TerrainSize.X);
    OutUVScale = (1.0 / Scale) / TerrainSize.X;
    return FMath::IsFinite(OutUVScale) && !OutUVCenter.ContainsNaN();
}

FVector2D FFFMapOverviewProjection::WorldToMap(const FVector& WorldPoint) const
{
    // 월드 +X는 화면 위쪽, +Y는 오른쪽이다. 역변환도 같은 축 계약을 따른다.
    return FVector2D(0.5 + (WorldPoint.Y - Center.Y) * Scale,
        0.5 - (WorldPoint.X - Center.X) * Scale);
}

bool FFFMapOverviewProjection::MapToWorld(const FVector2D& MapPoint, FVector& OutWorldPoint) const
{
    if (!IsValid() || MapPoint.ContainsNaN() || (MapPoint - FVector2D(0.5, 0.5)).SizeSquared() > MapRadiusSquared) return false;
    const FVector Point(Center.X - (MapPoint.Y - 0.5) / Scale,
        Center.Y + (MapPoint.X - 0.5) / Scale, Center.Z);
    if (Point.X < Bounds.Min.X || Point.X > Bounds.Max.X || Point.Y < Bounds.Min.Y || Point.Y > Bounds.Max.Y) return false;
    OutWorldPoint = Point;
    return true;
}

bool FFFMapOverviewProjection::ClipMapLine(FVector2D& Start, FVector2D& End) const
{
    if (!IsValid() || Start.ContainsNaN() || End.ContainsNaN()) return false;
    const FVector2D Min = WorldToMap(FVector(Bounds.Max.X, Bounds.Min.Y, Center.Z));
    const FVector2D Max = WorldToMap(FVector(Bounds.Min.X, Bounds.Max.Y, Center.Z));
    const FVector2D Delta = End - Start;
    double First = 0.0, Last = 1.0;
    auto Clip = [&](const double Direction, const double Distance)
    {
        if (FMath::Abs(Direction) < UE_DOUBLE_SMALL_NUMBER) return Distance >= 0.0;
        const double T = Distance / Direction;
        if (Direction < 0.0) First = FMath::Max(First, T);
        else Last = FMath::Min(Last, T);
        return First <= Last;
    };
    if (!Clip(-Delta.X, Start.X - Min.X) || !Clip(Delta.X, Max.X - Start.X) ||
        !Clip(-Delta.Y, Start.Y - Min.Y) || !Clip(Delta.Y, Max.Y - Start.Y)) return false;
    End = Start + Delta * Last;
    Start += Delta * First;
    // Local maps show the circular 50 m radius, not the bounding square's corners.
    if (bLocalWindow)
    {
        const FVector2D LocalDelta = End - Start;
        const FVector2D RelativeStart = Start - FVector2D(0.5, 0.5);
        const double A = LocalDelta.SizeSquared();
        if (A <= UE_DOUBLE_SMALL_NUMBER) return RelativeStart.SizeSquared() <= MapRadiusSquared;
        const double B = 2.0 * FVector2D::DotProduct(RelativeStart, LocalDelta);
        const double C = RelativeStart.SizeSquared() - MapRadiusSquared;
        const double Discriminant = B * B - 4.0 * A * C;
        if (Discriminant < 0.0) return false;
        const double Root = FMath::Sqrt(Discriminant);
        const double Low = FMath::Max(0.0, (-B - Root) / (2.0 * A));
        const double High = FMath::Min(1.0, (-B + Root) / (2.0 * A));
        if (Low > High) return false;
        End = Start + LocalDelta * High;
        Start += LocalDelta * Low;
    }
    return true;
}

void FFFMapOverviewProjection::ClipWorldPolygon(const TArray<FVector2D>& Input, TArray<FVector2D>& Output) const
{
    Output = Input;
    if (!IsValid()) { Output.Reset(); return; }
    // 두 배열의 저장 공간을 교환해 재사용한다. 원형 경계 64면을 자를 때마다
    // 이전 배열을 버리고 재할당하지 않으며, 각 단계는 직전 결과만 읽는다.
    TArray<FVector2D> Previous;
    // Convex facility footprints can straddle an edited venue edge. Clip their
    // presentation without changing placements or drawing beyond the map's rim.
    for (int32 Edge = 0; Edge < 4 && !Output.IsEmpty(); ++Edge)
    {
        const bool bX = Edge < 2;
        const bool bMinimum = (Edge % 2) == 0;
        const double Limit = bX ? (bMinimum ? Bounds.Min.X : Bounds.Max.X) : (bMinimum ? Bounds.Min.Y : Bounds.Max.Y);
        Swap(Previous, Output);
        Output.Reset();
        Output.Reserve(Previous.Num() + 1);
        auto Coordinate = [bX](const FVector2D& Point) { return bX ? Point.X : Point.Y; };
        auto Inside = [&](const FVector2D& Point) { return bMinimum ? Coordinate(Point) >= Limit : Coordinate(Point) <= Limit; };
        for (int32 Index = 0; Index < Previous.Num(); ++Index)
        {
            const FVector2D& Start = Previous[Index];
            const FVector2D& End = Previous[(Index + 1) % Previous.Num()];
            const bool bStartInside = Inside(Start), bEndInside = Inside(End);
            if (bStartInside != bEndInside)
            {
                const double Fraction = (Limit - Coordinate(Start)) / (Coordinate(End) - Coordinate(Start));
                Output.Add(FMath::Lerp(Start, End, Fraction));
            }
            if (bEndInside) Output.Add(End);
        }
    }
    if (bLocalWindow)
    {
        // The circular overlay is visual geometry only; a 64-sided clip avoids
        // drawing data overlays beyond the terrain material's circular mask.
        const double Radius = MapRadius / Scale;
        const FVector2D Origin(Center.X, Center.Y);
        for (int32 Edge = 0; Edge < CircleClipEdgeCount && !Output.IsEmpty(); ++Edge)
        {
            const double Angle = 2.0 * UE_DOUBLE_PI * (Edge + 0.5) / CircleClipEdgeCount;
            const FVector2D Normal(FMath::Cos(Angle), FMath::Sin(Angle));
            const double Limit = Radius * FMath::Cos(UE_DOUBLE_PI / CircleClipEdgeCount);
            Swap(Previous, Output);
            Output.Reset();
            Output.Reserve(Previous.Num() + 1);
            auto Distance = [&](const FVector2D& Point) { return Limit - FVector2D::DotProduct(Point - Origin, Normal); };
            for (int32 Index = 0; Index < Previous.Num(); ++Index)
            {
                const FVector2D& Start = Previous[Index];
                const FVector2D& End = Previous[(Index + 1) % Previous.Num()];
                const double StartDistance = Distance(Start), EndDistance = Distance(End);
                if ((StartDistance >= 0.0) != (EndDistance >= 0.0))
                    Output.Add(FMath::Lerp(Start, End, StartDistance / (StartDistance - EndDistance)));
                if (EndDistance >= 0.0) Output.Add(End);
            }
        }
    }
}
