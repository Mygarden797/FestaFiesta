#include "FF_FFZSOrientedGeometry.h"

#include <cfloat>

namespace
{
    struct FOrientedAxes
    {
        FVector2D X = FVector2D(1.0, 0.0);
        FVector2D Y = FVector2D(0.0, 1.0);
    };

    constexpr double CardinalSnapToleranceDegrees = 1.0e-6;

    double NormalizeYawDegrees(const double YawDegrees)
    {
        double Result = FMath::Fmod(YawDegrees, 360.0);
        if (Result < -180.0)
        {
            Result += 360.0;
        }
        else if (Result >= 180.0)
        {
            Result -= 360.0;
        }
        return Result;
    }

    bool TryGetAxes(const double YawDegrees, FOrientedAxes& OutAxes)
    {
        if (!FMath::IsFinite(YawDegrees))
        {
            return false;
        }

        const double NormalizedYaw = NormalizeYawDegrees(YawDegrees);
        const int32 NearestQuarterTurn = FMath::RoundToInt(NormalizedYaw / 90.0);
        if (FMath::Abs(NormalizedYaw - NearestQuarterTurn * 90.0) <=
            CardinalSnapToleranceDegrees)
        {
            switch ((NearestQuarterTurn % 4 + 4) % 4)
            {
            case 1:
                OutAxes.X = FVector2D(0.0, 1.0);
                OutAxes.Y = FVector2D(-1.0, 0.0);
                break;
            case 2:
                OutAxes.X = FVector2D(-1.0, 0.0);
                OutAxes.Y = FVector2D(0.0, -1.0);
                break;
            case 3:
                OutAxes.X = FVector2D(0.0, -1.0);
                OutAxes.Y = FVector2D(1.0, 0.0);
                break;
            default:
                OutAxes.X = FVector2D(1.0, 0.0);
                OutAxes.Y = FVector2D(0.0, 1.0);
                break;
            }
            return true;
        }

        const double Radians = FMath::DegreesToRadians(NormalizedYaw);
        const double CosYaw = FMath::Cos(Radians);
        const double SinYaw = FMath::Sin(Radians);
        OutAxes.X = FVector2D(CosYaw, SinYaw);
        OutAxes.Y = FVector2D(-SinYaw, CosYaw);
        return !OutAxes.X.ContainsNaN() && !OutAxes.Y.ContainsNaN();
    }

    FVector2D RotateByAxes(const FVector2D& Value, const FOrientedAxes& Axes)
    {
        return Axes.X * Value.X + Axes.Y * Value.Y;
    }

    bool TryConvertBoundsToCellRect(const FBox2D& Bounds, FFFZSCellRect& OutRect)
    {
        OutRect = {};
        if (!Bounds.bIsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN())
        {
            return false;
        }

        const double MinX = FMath::FloorToDouble(Bounds.Min.X);
        const double MinY = FMath::FloorToDouble(Bounds.Min.Y);
        const double MaxX = FMath::CeilToDouble(Bounds.Max.X);
        const double MaxY = FMath::CeilToDouble(Bounds.Max.Y);
        if (!FMath::IsFinite(MinX) || !FMath::IsFinite(MinY) ||
            !FMath::IsFinite(MaxX) || !FMath::IsFinite(MaxY) ||
            MinX < static_cast<double>(MIN_int32) || MinX > static_cast<double>(MAX_int32) ||
            MinY < static_cast<double>(MIN_int32) || MinY > static_cast<double>(MAX_int32) ||
            MaxX < static_cast<double>(MIN_int32) || MaxX > static_cast<double>(MAX_int32) ||
            MaxY < static_cast<double>(MIN_int32) || MaxY > static_cast<double>(MAX_int32))
        {
            return false;
        }

        OutRect = FFFZSCellRect(
            FIntPoint(static_cast<int32>(MinX), static_cast<int32>(MinY)),
            FIntPoint(static_cast<int32>(MaxX), static_cast<int32>(MaxY)));
        return OutRect.IsValid();
    }

    double ProjectionRadius(
        const FFFZSOrientedRect& Rect,
        const FOrientedAxes& RectAxes,
        const FVector2D& ProjectionAxis)
    {
        return Rect.HalfExtentCells.X *
                FMath::Abs(FVector2D::DotProduct(RectAxes.X, ProjectionAxis)) +
            Rect.HalfExtentCells.Y *
                FMath::Abs(FVector2D::DotProduct(RectAxes.Y, ProjectionAxis));
    }
}

namespace FFZSOrientedGeometry
{
    FFFZSOrientedRect MakeFromLocalRect(
        const FFFZSCellRect& LocalRect,
        const FIntPoint OriginCell,
        const float YawDegrees)
    {
        FFFZSOrientedRect Result;
        Result.CenterCell = FVector2D::ZeroVector;
        Result.HalfExtentCells = FVector2D::ZeroVector;
        Result.YawDegrees = 0.0f;

        FOrientedAxes Axes;
        if (!LocalRect.IsValid() || !TryGetAxes(YawDegrees, Axes))
        {
            return Result;
        }

        const double Width = static_cast<double>(LocalRect.Max.X) - LocalRect.Min.X;
        const double Height = static_cast<double>(LocalRect.Max.Y) - LocalRect.Min.Y;
        const FVector2D LocalIndexCenter(
            (static_cast<double>(LocalRect.Min.X) + LocalRect.Max.X - 1.0) * 0.5,
            (static_cast<double>(LocalRect.Min.Y) + LocalRect.Max.Y - 1.0) * 0.5);
        const FVector2D RotationPivot(
            static_cast<double>(OriginCell.X) + 0.5,
            static_cast<double>(OriginCell.Y) + 0.5);

        Result.CenterCell = RotationPivot + RotateByAxes(LocalIndexCenter, Axes);
        Result.HalfExtentCells = FVector2D(Width * 0.5, Height * 0.5);
        Result.YawDegrees = static_cast<float>(NormalizeYawDegrees(YawDegrees));
        if (!Result.IsValid())
        {
            Result.CenterCell = FVector2D::ZeroVector;
            Result.HalfExtentCells = FVector2D::ZeroVector;
            Result.YawDegrees = 0.0f;
        }
        return Result;
    }

    FCornerArray GetCorners(const FFFZSOrientedRect& Rect)
    {
        FCornerArray Result;
        for (FVector2D& Corner : Result)
        {
            Corner = FVector2D::ZeroVector;
        }

        FOrientedAxes Axes;
        if (!Rect.IsValid() || !TryGetAxes(Rect.YawDegrees, Axes))
        {
            return Result;
        }

        const FVector2D ExtentX = Axes.X * Rect.HalfExtentCells.X;
        const FVector2D ExtentY = Axes.Y * Rect.HalfExtentCells.Y;
        Result[0] = Rect.CenterCell - ExtentX - ExtentY;
        Result[1] = Rect.CenterCell + ExtentX - ExtentY;
        Result[2] = Rect.CenterCell + ExtentX + ExtentY;
        Result[3] = Rect.CenterCell - ExtentX + ExtentY;
        return Result;
    }

    FBox2D GetAabb(const FFFZSOrientedRect& Rect)
    {
        FBox2D Result(EForceInit::ForceInit);
        if (!Rect.IsValid())
        {
            return Result;
        }

        for (const FVector2D& Corner : GetCorners(Rect))
        {
            Result += Corner;
        }
        return Result;
    }

    FFFZSCellRect ToConservativeCellRect(const FFFZSOrientedRect& Rect)
    {
        FFFZSCellRect Result;
        return TryConvertBoundsToCellRect(GetAabb(Rect), Result) ? Result : FFFZSCellRect{};
    }

    bool IntersectsPositiveArea(
        const FFFZSOrientedRect& A,
        const FFFZSOrientedRect& B)
    {
        FOrientedAxes AxesA;
        FOrientedAxes AxesB;
        if (!A.IsValid() || !B.IsValid() ||
            !TryGetAxes(A.YawDegrees, AxesA) || !TryGetAxes(B.YawDegrees, AxesB))
        {
            return false;
        }

        const FVector2D CenterDelta = B.CenterCell - A.CenterCell;
        const FVector2D ProjectionAxes[] = {
            AxesA.X,
            AxesA.Y,
            AxesB.X,
            AxesB.Y
        };
        for (const FVector2D& Axis : ProjectionAxes)
        {
            const double CenterDistance =
                FMath::Abs(FVector2D::DotProduct(CenterDelta, Axis));
            const double RadiusSum =
                ProjectionRadius(A, AxesA, Axis) + ProjectionRadius(B, AxesB, Axis);
            if (!FMath::IsFinite(CenterDistance) || !FMath::IsFinite(RadiusSum))
            {
                return false;
            }

            // The scaled machine epsilon absorbs trig/projection round-off at exact contact while
            // remaining far below the 1 cm authority resolution, even near int32 coordinates.
            const double ComparisonScale = FMath::Max3(1.0, CenterDistance, RadiusSum);
            const double ContactTolerance = 64.0 * DBL_EPSILON * ComparisonScale;
            if (CenterDistance >= RadiusSum - ContactTolerance)
            {
                return false;
            }
        }
        return true;
    }
}
