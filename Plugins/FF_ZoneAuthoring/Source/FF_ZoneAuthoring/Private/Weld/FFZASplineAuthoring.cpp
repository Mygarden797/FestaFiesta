// FFZASplineAuthoring.cpp
// 역할: 경계 Spline 액터의 Transform 정규화(모양 보존)와 Spline 형식 보정 구현.
#include "FFZASplineAuthoring.h"

#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"

namespace FFZASplineAuthoring
{
namespace
{
    constexpr double ScaleTolerance = 0.001;
    constexpr double AngleToleranceDegrees = 0.01;
    constexpr double OriginToleranceCm = 0.001;

    double SnapYaw(const double Yaw)
    {
        return FRotator::NormalizeAxis(FMath::GridSnap(FRotator::NormalizeAxis(Yaw), 90.0));
    }
}

bool IsActorTransformNormalized(const AActor& Actor, const FNormalizeOptions& Options)
{
    const FTransform Transform = Actor.GetActorTransform();
    if (Options.bResetScale && !Transform.GetScale3D().Equals(FVector::OneVector, ScaleTolerance))
    {
        return false;
    }
    const FRotator Rotation = Transform.Rotator();
    if (Options.bClearPitchRoll &&
        (!FMath::IsNearlyZero(Rotation.Pitch, AngleToleranceDegrees) || !FMath::IsNearlyZero(Rotation.Roll, AngleToleranceDegrees)))
    {
        return false;
    }
    if (Options.bSnapYawTo90 &&
        !FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Rotation.Yaw, SnapYaw(Rotation.Yaw)), AngleToleranceDegrees))
    {
        return false;
    }
    if (Options.bSnapOriginToWholeCm)
    {
        const FVector Location = Transform.GetLocation();
        if (!FMath::IsNearlyEqual(Location.X, FMath::RoundToDouble(Location.X), OriginToleranceCm) ||
            !FMath::IsNearlyEqual(Location.Y, FMath::RoundToDouble(Location.Y), OriginToleranceCm))
        {
            return false;
        }
    }
    return true;
}

bool NormalizeActorTransformPreservingSpline(AActor& Actor, USplineComponent& Spline, const FNormalizeOptions& Options)
{
    if (IsActorTransformNormalized(Actor, Options))
    {
        return false;
    }

    // 1) 지금 화면에 보이는 경계 점의 월드 위치를 기억한다.
    const int32 PointCount = Spline.GetNumberOfSplinePoints();
    TArray<FVector> WorldPoints;
    WorldPoints.Reserve(PointCount);
    for (int32 Index = 0; Index < PointCount; ++Index)
    {
        WorldPoints.Add(Spline.GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World));
    }

    // 2) 액터 Transform을 규칙에 맞는 값으로 바꾼다.
    const FTransform Current = Actor.GetActorTransform();
    const FRotator CurrentRotation = Current.Rotator();
    FRotator NewRotation = CurrentRotation;
    if (Options.bClearPitchRoll)
    {
        NewRotation.Pitch = 0.0;
        NewRotation.Roll = 0.0;
    }
    if (Options.bSnapYawTo90)
    {
        NewRotation.Yaw = SnapYaw(CurrentRotation.Yaw);
    }
    FVector NewLocation = Current.GetLocation();
    if (Options.bSnapOriginToWholeCm)
    {
        NewLocation.X = FMath::RoundToDouble(NewLocation.X);
        NewLocation.Y = FMath::RoundToDouble(NewLocation.Y);
    }
    const FVector NewScale = Options.bResetScale ? FVector::OneVector : Current.GetScale3D();

    Actor.Modify();
    Spline.Modify();
    if (USceneComponent* Root = Actor.GetRootComponent())
    {
        Root->Modify();
    }
    Actor.SetActorTransform(FTransform(NewRotation, NewLocation, NewScale), false, nullptr, ETeleportType::TeleportPhysics);

    // 3) 기억해 둔 월드 위치로 점을 되돌려 놓는다 → 보이는 모양은 그대로, Scale/회전만 정리됨.
    for (int32 Index = 0; Index < PointCount; ++Index)
    {
        Spline.SetLocationAtSplinePoint(Index, WorldPoints[Index], ESplineCoordinateSpace::World, false);
    }
    Spline.UpdateSpline();
    return true;
}

bool EnsureClosedLinear(USplineComponent& Spline)
{
    bool bChanged = false;
    const int32 PointCount = Spline.GetNumberOfSplinePoints();
    for (int32 Index = 0; Index < PointCount; ++Index)
    {
        if (Spline.GetSplinePointType(Index) != ESplinePointType::Linear)
        {
            if (!bChanged)
            {
                Spline.Modify();
            }
            Spline.SetSplinePointType(Index, ESplinePointType::Linear, false);
            bChanged = true;
        }
    }
    if (!Spline.IsClosedLoop())
    {
        if (!bChanged)
        {
            Spline.Modify();
        }
        Spline.SetClosedLoop(true, false);
        bChanged = true;
    }
    if (bChanged)
    {
        Spline.UpdateSpline();
    }
    return bChanged;
}

void GetWorldPolygon(const USplineComponent& Spline, TArray<FVector2D>& OutPolygon)
{
    const int32 PointCount = Spline.GetNumberOfSplinePoints();
    OutPolygon.Reset(PointCount);
    for (int32 Index = 0; Index < PointCount; ++Index)
    {
        const FVector World = Spline.GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
        OutPolygon.Add(FVector2D(World.X, World.Y));
    }
}

void SetWorldPolygon(USplineComponent& Spline, const TArray<FVector2D>& WorldPolygon, const double WorldZ)
{
    Spline.Modify();
    const int32 PointCount = Spline.GetNumberOfSplinePoints();
    if (PointCount == WorldPolygon.Num())
    {
        for (int32 Index = 0; Index < PointCount; ++Index)
        {
            const FVector Current = Spline.GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
            Spline.SetLocationAtSplinePoint(Index, FVector(WorldPolygon[Index].X, WorldPolygon[Index].Y, Current.Z),
                ESplineCoordinateSpace::World, false);
        }
    }
    else
    {
        Spline.ClearSplinePoints(false);
        for (int32 Index = 0; Index < WorldPolygon.Num(); ++Index)
        {
            Spline.AddSplinePoint(FVector(WorldPolygon[Index].X, WorldPolygon[Index].Y, WorldZ), ESplineCoordinateSpace::World, false);
            Spline.SetSplinePointType(Index, ESplinePointType::Linear, false);
        }
        Spline.SetClosedLoop(true, false);
    }
    Spline.UpdateSpline();
}

bool BuildWorldPolygonPreservingHeights(const TArray<double>& OriginalWorldHeights,
    const TArray<FVector2D>& WorldPolygon, const TArray<double>& SourcePointKeys, TArray<FVector>& OutWorldPoints)
{
    const int32 OriginalCount = OriginalWorldHeights.Num();
    if (OriginalCount < 3 || WorldPolygon.Num() < 3 ||
        WorldPolygon.Num() != SourcePointKeys.Num())
    {
        return false;
    }

    for (const double Height : OriginalWorldHeights)
    {
        if (!FMath::IsFinite(Height))
        {
            return false;
        }
    }

    // Compute and validate every point before Modify(), so a rejected proposal has no side effects.
    TArray<FVector> WorldPoints;
    WorldPoints.Reserve(WorldPolygon.Num());
    for (int32 Index = 0; Index < WorldPolygon.Num(); ++Index)
    {
        const double Key = SourcePointKeys[Index];
        if (WorldPolygon[Index].ContainsNaN() || !FMath::IsFinite(Key) || Key < 0.0 || Key >= OriginalCount)
        {
            return false;
        }
        const int32 Edge = FMath::FloorToInt(Key);
        const double Height = FMath::Lerp(OriginalWorldHeights[Edge], OriginalWorldHeights[(Edge + 1) % OriginalCount], Key - Edge);
        if (!FMath::IsFinite(Height))
        {
            return false;
        }
        WorldPoints.Emplace(WorldPolygon[Index].X, WorldPolygon[Index].Y, Height);
    }
    OutWorldPoints = MoveTemp(WorldPoints);
    return true;
}

bool SetWorldPolygonPreservingHeights(USplineComponent& Spline, const TArray<FVector2D>& WorldPolygon,
    const TArray<double>& SourcePointKeys)
{
    const int32 OriginalCount = Spline.GetNumberOfSplinePoints();
    if (OriginalCount < 3 || !Spline.IsClosedLoop())
    {
        return false;
    }
    TArray<double> OriginalHeights;
    OriginalHeights.Reserve(OriginalCount);
    for (int32 Index = 0; Index < OriginalCount; ++Index)
    {
        const FVector Point = Spline.GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
        if (Spline.GetSplinePointType(Index) != ESplinePointType::Linear || Point.ContainsNaN())
        {
            return false;
        }
        OriginalHeights.Add(Point.Z);
    }
    TArray<FVector> WorldPoints;
    if (!BuildWorldPolygonPreservingHeights(OriginalHeights, WorldPolygon, SourcePointKeys, WorldPoints))
    {
        return false;
    }

    Spline.Modify();
    if (OriginalCount == WorldPoints.Num())
    {
        for (int32 Index = 0; Index < WorldPoints.Num(); ++Index)
        {
            Spline.SetLocationAtSplinePoint(Index, WorldPoints[Index], ESplineCoordinateSpace::World, false);
        }
    }
    else
    {
        Spline.SetSplinePoints(WorldPoints, ESplineCoordinateSpace::World, false);
        for (int32 Index = 0; Index < WorldPoints.Num(); ++Index)
        {
            Spline.SetSplinePointType(Index, ESplinePointType::Linear, false);
        }
    }
    Spline.SetClosedLoop(true, false);
    Spline.UpdateSpline();
    return true;
}

void SetLocalRectangle(USplineComponent& Spline, const FVector2D& SizeCm)
{
    const double SizeX = FMath::Max(1.0, FMath::RoundToDouble(SizeCm.X));
    const double SizeY = FMath::Max(1.0, FMath::RoundToDouble(SizeCm.Y));
    const FVector Points[] = {
        FVector(0.0, 0.0, 0.0),
        FVector(SizeX, 0.0, 0.0),
        FVector(SizeX, SizeY, 0.0),
        FVector(0.0, SizeY, 0.0)
    };
    // 생성자에서도 호출되므로 여기서는 Modify()를 부르지 않는다(Undo가 필요한 호출자가 먼저 Modify).
    Spline.ClearSplinePoints(false);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Points); ++Index)
    {
        Spline.AddSplinePoint(Points[Index], ESplineCoordinateSpace::Local, false);
        Spline.SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    Spline.SetClosedLoop(true, false);
    Spline.UpdateSpline();
}

void SetEditorColor(USplineComponent& Spline, const FLinearColor& Color)
{
#if WITH_EDITOR
    Spline.SetUnselectedSplineSegmentColor(Color);
    Spline.SetSelectedSplineSegmentColor(FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.45f));
#else
    (void)Spline;
    (void)Color;
#endif
}
}
