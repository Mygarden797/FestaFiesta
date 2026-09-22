// FF_FFZSBuildArea.cpp
// 역할: 여러 지역을 하나의 건설 영역으로 묶어 지역 경계를 넘어 시설을 배치하게 한다.
#include "FF_FFZSBuildArea.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSBuildAreaGeometry.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

AFFZSBuildArea::AFFZSBuildArea()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    Geometry = MakeShared<FFFZSBuildAreaGeometry>();
    BoundaryStatus = NSLOCTEXT("FFZSBuildArea", "NotBuilt", "영역을 아직 계산하지 않았습니다.");
}

AFFZSBuildArea::~AFFZSBuildArea() = default;

void AFFZSBuildArea::BeginPlay()
{
    Super::BeginPlay();
    if (UWorld* World = GetWorld())
    {
        if (UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>())
        {
            Subsystem->RegisterBuildArea(this);
        }
    }
}

void AFFZSBuildArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>())
        {
            Subsystem->UnregisterBuildArea(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void AFFZSBuildArea::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (GetWorld() && !GetWorld()->IsGameWorld())
    {
        RebuildFromWorldRegions();
    }
}

void AFFZSBuildArea::RebuildFromWorldRegions()
{
    TArray<AFFZSGameplayZone*> Regions;
    if (UWorld* World = GetWorld())
    {
        if (World->IsGameWorld())
        {
            // Policy callbacks and in-flight commands cannot alter an already validated boundary.
            // Use the registry so a rejected, duplicate or foreign region cannot gain authority.
            if (UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>())
            {
                if (Subsystem->FindBuildArea() == this)
                {
                    Subsystem->RefreshBuildArea();
                }
            }
            return;
        }
        for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                Regions.Add(*It);
            }
        }
    }
    RebuildFromRegions(Regions);
}

bool AFFZSBuildArea::RebuildFromRegions(const TArray<AFFZSGameplayZone*>& Regions)
{
    SourceRegionIds.Reset();
    BoundaryAreaSquareMeters = 0.0;
    // Always drop the old cache first. Invalid edits must not silently keep an older buildable area.
    Geometry = MakeShared<FFFZSBuildAreaGeometry>();
    TArray<FFFZSBuildAreaSourcePolygon> Sources;
    Sources.Reserve(Regions.Num());
    for (const AFFZSGameplayZone* Region : Regions)
    {
        TArray<FText> Errors;
        if (!IsValid(Region) || !Region->ValidateZoneData(Errors) || SourceRegionIds.Contains(Region->ZoneId))
        {
            BoundaryStatus = NSLOCTEXT("FFZSBuildArea", "InvalidRegion",
                "지역 설정이 잘못되었거나 ID가 중복됩니다. Gameplay Zone의 검증 오류를 먼저 수정하세요.");
            SourceRegionIds.Reset();
            return false;
        }
        FFFZSBuildAreaSourcePolygon Source;
        const FTransform Transform = Region->GetActorTransform();
        for (const FVector2D& Point : Region->GetBoundaryPolygonLocalCm())
        {
            const FVector WorldPoint = Transform.TransformPosition(FVector(Point.X, Point.Y, 0.0));
            Source.WorldVertices.Emplace(WorldPoint.X, WorldPoint.Y);
        }
        Source.MinZ = Transform.GetLocation().Z - Region->VerticalExtentCm * 0.5;
        Source.MaxZ = Transform.GetLocation().Z + Region->VerticalExtentCm * 0.5;
        Sources.Add(MoveTemp(Source));
        SourceRegionIds.Add(Region->ZoneId);
    }
    if (!Geometry->Rebuild(Sources))
    {
        BoundaryStatus = NSLOCTEXT("FFZSBuildArea", "Failed",
            "유효한 지역이 없거나 경계 통합에 실패했습니다. 건설은 허용되지 않습니다.");
        return false;
    }
    SourceRegionIds.Sort(FNameLexicalLess());
    BoundaryAreaSquareMeters = Geometry->GetAreaSquareMeters();
    BoundaryStatus = FText::Format(NSLOCTEXT("FFZSBuildArea", "Ready",
        "준비 완료: {0}개 지역을 하나의 건설 영역으로 통합했습니다."), FText::AsNumber(SourceRegionIds.Num()));
    return true;
}

bool AFFZSBuildArea::IsBuildAreaReady() const
{
    return Geometry && Geometry->IsReady();
}

bool AFFZSBuildArea::ContainsWorldPoint(const FVector& WorldPoint) const
{
    return Geometry && Geometry->ContainsWorldPoint(WorldPoint);
}

bool AFFZSBuildArea::ContainsWorldOrientedRect(
    const FFFZSOrientedRect& WorldRectCm, const double WorldZCm) const
{
    return Geometry && Geometry->ContainsWorldOrientedRect(WorldRectCm, WorldZCm);
}

FBox AFFZSBuildArea::GetBuildBounds() const
{
    return Geometry ? Geometry->GetBounds() : FBox(ForceInit);
}
