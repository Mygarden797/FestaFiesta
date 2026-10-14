// Whole-venue projection and clipping shared by the minimap and its numerical tests.
#pragma once

#include "CoreMinimal.h"

struct FF_MAPOVERVIEW_API FFFMapOverviewProjection
{
    FBox Bounds = FBox(ForceInit);
    FVector Center = FVector::ZeroVector;
    double Scale = 0.0;
    bool bLocalWindow = false;

    // +X is north (up), +Y is east (right). The diagonal fits inside the circle.
    bool SetBounds(const FBox& InBounds);
    bool SetLocalBounds(const FVector& InCenter, double DiameterCm = 10000.0);
    bool IsValid() const { return Scale > 0.0 && Bounds.IsValid; }
    bool IsWorldPointVisible(const FVector& WorldPoint) const;
    bool GetTerrainUVTransform(const FBox& TerrainBounds, FVector2D& OutUVCenter, double& OutUVScale) const;
    FVector2D WorldToMap(const FVector& WorldPoint) const;
    bool MapToWorld(const FVector2D& MapPoint, FVector& OutWorldPoint) const;
    bool ClipMapLine(FVector2D& Start, FVector2D& End) const;
    void ClipWorldPolygon(const TArray<FVector2D>& Input, TArray<FVector2D>& Output) const;
};

struct FFFMapOverviewVertex
{
    FVector2D Point = FVector2D::ZeroVector;
    FLinearColor Color = FLinearColor::White;
    // Semantic land/road/tile layer. Actual colors belong to the UMG designer.
    uint8 GeometryLayer = 0;
};

struct FFFMapOverviewFacilityMarker
{
    FGuid Id;
    FVector2D Point = FVector2D::ZeroVector;
    FText DisplayName;
    uint8 Kind = 0;
};

struct FFFMapOverviewWorldPolygon
{
    TArray<FVector2D> Points;
    FBox2D Bounds = FBox2D(ForceInit);
    uint8 GeometryLayer = 0;
};

struct FFFMapOverviewLine
{
    FVector2D Start = FVector2D::ZeroVector;
    FVector2D End = FVector2D::ZeroVector;
};

// No Actors/UObjects are retained in the render snapshot. Static vertices are rebuilt
// after authority changes; camera markers update separately at 10 Hz.
struct FFFMapOverviewSnapshot
{
    FFFMapOverviewProjection Projection;
    TArray<FFFMapOverviewVertex> Vertices;
    TArray<uint32> Indices;
    TArray<FFFMapOverviewLine> Boundary;
    TArray<FFFMapOverviewLine> CameraFootprint;
    TArray<FFFMapOverviewFacilityMarker> FacilityMarkers;
    FVector2D CameraFocus = FVector2D(0.5, 0.5);
    bool bHasCamera = false;
    int32 FacilityCount = 0;
};
