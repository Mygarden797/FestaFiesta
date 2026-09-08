#include "FF_FFZSBuildableGridActor.h"

#include "Components/SceneComponent.h"
#include "FF_FFZSBuildableGridComponent.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

AFFZSBuildableGridActor::AFFZSBuildableGridActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GridRoot")));
    SetActorEnableCollision(false);
}

void AFFZSBuildableGridActor::HideGrid()
{
    for (const auto& Pair : ZoneGrids)
    {
        Pair.Value->HideGrid();
    }
    SetActorHiddenInGame(true);
}

void AFFZSBuildableGridActor::UpdateGrid(UFFZSWorldSubsystem& WorldSystem,
    const FVector& SurfacePointWorld, const FVector& SurfaceNormalWorld, const int32 GridStepCm,
    const FFFZSCommand* PlacementContext)
{
    const TArray<FName> ZoneIds = WorldSystem.GetRegisteredZoneIds();
    for (auto It = ZoneGrids.CreateIterator(); It; ++It)
    {
        if (!ZoneIds.Contains(It.Key()))
        {
            It.Value()->DestroyComponent();
            It.RemoveCurrent();
        }
    }
    const float ConfigRadius = GetDefault<UFFZSDeveloperSettings>()->PlacementGridMaxRadiusCm;
    const double Radius = FMath::IsFinite(ConfigRadius) ? FMath::Clamp(ConfigRadius, 100.f, 5000.f) : 3600.0;
    TMap<FName, FVector2D> VisibleCenters;
    for (const FName ZoneId : ZoneIds)
    {
        const AFFZSGameplayZone* Zone = WorldSystem.FindZone(ZoneId);
        if (!Zone)
        {
            continue;
        }
        const FVector Local = Zone->GetActorTransform().InverseTransformPosition(SurfacePointWorld);
        const FVector2D Center(Local.X, Local.Y);
        const FBox2D Window(Center - FVector2D(Radius), Center + FVector2D(Radius));
        const FBox2D ZoneBounds(Zone->GetBoundaryPolygonLocalCm());
        if (!ZoneBounds.bIsValid || !ZoneBounds.Intersect(Window) || FMath::Abs(Local.Z) > Zone->VerticalExtentCm * 0.5)
        {
            if (const auto Existing = ZoneGrids.Find(ZoneId))
            {
                (*Existing)->HideGrid();
            }
            continue;
        }
        VisibleCenters.Add(ZoneId, Center);
    }
    // Share the trace budget across nearby zones; adjacent lots must not each spend a full
    // 512 traces in the same overview update. Surface lifetime adapts to this smaller slice.
    const int32 SamplesPerZone = FMath::Max(1, 512 / FMath::Max(1, VisibleCenters.Num()));
    for (const TPair<FName, FVector2D>& Visible : VisibleCenters)
    {
        const FName ZoneId = Visible.Key;
        TObjectPtr<UFFZSBuildableGridComponent>& Grid = ZoneGrids.FindOrAdd(ZoneId);
        if (!Grid)
        {
            Grid = NewObject<UFFZSBuildableGridComponent>(this, NAME_None, RF_Transient);
            Grid->SetupAttachment(GetRootComponent());
            AddInstanceComponent(Grid);
            Grid->RegisterComponent();
        }
        FFFZSCommand Overview = PlacementContext ? *PlacementContext : FFFZSCommand();
        Overview.ZoneId = ZoneId;
        Grid->RefreshGrid(WorldSystem, Overview, Visible.Value, FVector2D(Radius), GridStepCm,
            SurfacePointWorld, SurfaceNormalWorld, SamplesPerZone);
    }
    SetActorHiddenInGame(false);
}
