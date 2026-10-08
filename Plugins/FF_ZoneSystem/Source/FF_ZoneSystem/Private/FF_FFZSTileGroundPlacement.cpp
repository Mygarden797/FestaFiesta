#include "FF_FFZSTileGroundPlacement.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSPlacementGroundSupport.h"
#include "FF_FFZSWorldSubsystem.h"

bool FFZSTileGroundPlacement::IsAuthoredTerrain(const UPrimitiveComponent* Component)
{
    static const FName TileTerrainTag(TEXT("FFZS_TileGround"));
    return Component && (Component->IsA<ULandscapeHeightfieldCollisionComponent>() ||
        Component->ComponentHasTag(TileTerrainTag) ||
        (Component->GetOwner() && Component->GetOwner()->ActorHasTag(TileTerrainTag)));
}

bool FFZSTileGroundPlacement::FGroundSample::IsContinuousWith(const FGroundSample& Other) const
{
    if (bAuthoredTerrain && Other.bAuthoredTerrain) { return true; }
    constexpr double ContactToleranceCm = 0.1;
    const double AllowedRise = FVector::DistXY(Point, Other.Point) * FMath::Max(Slope, Other.Slope);
    return FMath::Abs(Point.Z - Other.Point.Z) <= AllowedRise + ContactToleranceCm;
}

EFFZSTileSupportResult FFZSTileGroundPlacement::FindHighestSupport(UWorld& World,
    const FTransform& ZoneTransform, const TArray<FFFZSOrientedRect>& Footprint,
    const FFFZSGroundValidationSettings& Ground, const FCollisionQueryParams& Query,
    const FFFZSPlacementGroundSupport& Support, double& OutHeight, FHitResult& OutObstruction)
{
    // A thin horizontal box represents the bottom of the tile, not its visible mesh.
    // The tiny XY inset avoids treating a merely touching neighbor as underlying terrain.
    constexpr double SweepHalfHeightCm = 0.5;
    constexpr double EdgeInsetCm = 0.01;
    constexpr double ContactToleranceCm = 0.1;
    constexpr int32 MaxRegisteredSurfaceSkips = 64;
    FCollisionQueryParams SweepQuery = Query;
    TSet<const AFFZSFacilityActor*> IgnoredSurfaces;
    const UFFZSWorldSubsystem* Subsystem = World.GetSubsystem<UFFZSWorldSubsystem>();
    OutHeight = Support.MaxHeight;
    for (const FFFZSOrientedRect& Rect : Footprint)
    {
        const FVector Center = ZoneTransform.TransformPosition(FVector(Rect.CenterCell, 0.0));
        const FQuat Rotation = ZoneTransform.GetRotation() *
            FQuat(FVector::UpVector, FMath::DegreesToRadians(static_cast<double>(Rect.YawDegrees)));
        const FVector Extent(FMath::Max(EdgeInsetCm, Rect.HalfExtentCells.X - EdgeInsetCm),
            FMath::Max(EdgeInsetCm, Rect.HalfExtentCells.Y - EdgeInsetCm), SweepHalfHeightCm);
        FHitResult Hit;
        for (;;)
        {
            if (!World.SweepSingleByChannel(Hit,
                Center + FVector::UpVector * (Ground.TraceAboveCm + SweepHalfHeightCm),
                Center + FVector::UpVector * (SweepHalfHeightCm - Ground.TraceBelowCm),
                Rotation, ECC_Visibility, FCollisionShape::MakeBox(Extent), SweepQuery))
            {
                return EFFZSTileSupportResult::MissingGround;
            }
            const AFFZSFacilityActor* Surface = Cast<AFFZSFacilityActor>(Hit.GetActor());
            if (!Surface || Surface->GetWorld() != &World || !Subsystem ||
                !Surface->GetFacilitySpec().IsGroundSurface() ||
                Subsystem->FindFacility(Surface->GetFacilityInstanceId()) != Surface)
            {
                break;
            }
            // A source-sized Road visual can reach between every ground ray while its indexed
            // footprint is far away. Match the ground-ray policy, but only for this world's
            // registered paving identity. Ignore state survives subsequent footprint rectangles.
            if (IgnoredSurfaces.Contains(Surface) || IgnoredSurfaces.Num() >= MaxRegisteredSurfaceSkips)
            {
                OutObstruction = Hit;
                return EFFZSTileSupportResult::Obstructed;
            }
            IgnoredSurfaces.Add(Surface);
            SweepQuery.AddIgnoredActor(Surface);
        }
        if (Hit.bStartPenetrating || Hit.Location.ContainsNaN())
        {
            return EFFZSTileSupportResult::MissingGround;
        }
        const double Height = Hit.Location.Z - SweepHalfHeightCm;
        const UPrimitiveComponent* Component = Hit.GetComponent();
        // Landscape has an explicit terrain identity. An arbitrary mesh can contain a wall
        // in the same body as its floor; reject peaks outside the locally sampled envelope
        // instead of silently turning that wall into a foundation. Component-wide bounds
        // are deliberately never used to calculate the tile's elevation.
        // Modular mesh terrain can opt in explicitly. A stable tag makes the distinction
        // reviewable by level authors; it never grants build-area or occupancy permission.
        const bool bAuthoredTerrain = IsAuthoredTerrain(Component);
        if (!bAuthoredTerrain && (!Support.Contains(Component, Hit.Item) ||
            Height > Support.MaxHeight + Support.UnsampledRiseCm + ContactToleranceCm))
        {
            OutObstruction = Hit;
            return EFFZSTileSupportResult::Obstructed;
        }
        OutHeight = FMath::Max(OutHeight, Height);
    }
    return EFFZSTileSupportResult::Valid;
}
