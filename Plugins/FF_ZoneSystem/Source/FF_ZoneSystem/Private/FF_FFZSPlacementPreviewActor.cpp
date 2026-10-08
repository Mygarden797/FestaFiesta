// FF_FFZSPlacementPreviewActor.cpp

//  권위 없는 시설 배치 Ghost, 압축 예약 영역 표시, 비동기 검증 결과 표시를 구현한다.
//흐름: 사용자 입력을 Preview Transform으로 바꾸고 최신 Revision 결과만 시각화한다.
// 불변식: Preview는 점유·비용·Queue를 확정하지 않으며 실제 명령 결과를 대신하지 않는다.
// 실패/성능: stale 비동기 완료는 버리고 Debug 표시는 Shipping 비용을 만들지 않게 제한한다.
 

#include "FF_FFZSPlacementPreviewActor.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/StaticMesh.h"

#include "Components/SceneComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSBuildableGridComponent.h"
#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSBuildRangeComponent.h"
#include "FF_FFZSBuildRangePresentation.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSBuildAreaGeometry.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSOrientedGeometry.h"
#include "FF_FFZSVisualGeometry.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "SceneManagement.h"


// 이 Actor는 배치 명령을 시각화하는 비권위 Preview다.
// RefreshPreview는 Subsystem의 현재 상태로 후보를 검증하고 Actor Transform, 메시, CustomDepth, 디버그
// 셀만 바꾼다. 시설 레지스트리, 점유 셀, WorldRevision은 변경하지 않는다. 실제 변경은 오직
// CommitPreview -> ExecuteCommand 경로에서 다시 검증·정책 적용·커밋될 때 발생한다.

// 따라서 화면이 초록색이어도 커밋 성공을 보장하지 않는다. Preview 뒤에 월드 Revision, 점유, 정책이
// 바뀔 수 있으며 ExpectedWorldRevision은 이 오래된 화면을 커밋하는 경쟁을 명시적으로 거부하기 위한 값이다.

AFFZSPlacementPreviewActor::AFFZSPlacementPreviewActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = 0.1f;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
    PreviewMesh->SetupAttachment(SceneRoot);
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetGenerateOverlapEvents(false);
    PreviewMesh->SetRenderCustomDepth(true);
    // The temporary ghost needs the translucent overlay pass even for Nanite source assets.
    // Only this component uses the source fallback mesh; installed facilities are unchanged.
    PreviewMesh->bDisallowNanite = true;

    RangeLines = CreateDefaultSubobject<UFFZSBuildRangeComponent>(TEXT("RangeLines"));
    RangeLines->SetupAttachment(SceneRoot);
    RangeLines->SetAbsolute(true, true, true);
    RangeLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RangeLines->SetGenerateOverlapEvents(false);
    RangeLines->SetCanEverAffectNavigation(false);
    RangeLines->PrimaryComponentTick.bCanEverTick = false;

    PlacementGridLines = CreateDefaultSubobject<UFFZSBuildableGridComponent>(TEXT("PlacementGridLines"));
    PlacementGridLines->SetupAttachment(SceneRoot);
    PlacementGridLines->SetAbsolute(true, true, true);
    PlacementGridLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlacementGridLines->SetGenerateOverlapEvents(false);
    PlacementGridLines->SetCanEverAffectNavigation(false);
    PlacementGridLines->PrimaryComponentTick.bCanEverTick = false;
}

void AFFZSPlacementPreviewActor::BeginPlay()
{
    Super::BeginPlay();
    SetAutoRefreshEnabled(bAutoRefreshPreview);
}

void AFFZSPlacementPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (RangeLines)
    {
        RangeLines->Flush();
    }
    if (PlacementGridLines)
    {
        PlacementGridLines->Flush();
    }
    if (VisualLoadHandle.IsValid())
    {
        VisualLoadHandle->CancelHandle();
        VisualLoadHandle.Reset();
    }
    Super::EndPlay(EndPlayReason);
}

// 자동 갱신은 표시 품질을 위한 폴링이다. 최소 0.02초 간격으로 제한하여 실수로 매 프레임 무제한
// 검증하지 않도록 하며, 꺼져 있을 때는 명시적 RefreshPreview 호출만 비용을 발생시킨다.
void AFFZSPlacementPreviewActor::SetAutoRefreshEnabled(const bool bEnabled)
{
    bAutoRefreshPreview = bEnabled;
    PrimaryActorTick.TickInterval = FMath::Max(0.02f, AutoRefreshIntervalSeconds);
    SetActorTickEnabled(bAutoRefreshPreview);
}

void AFFZSPlacementPreviewActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshPreview();
}

// ValidatePlacement는 로컬 기하/충돌/내비게이션 검사와 외부 정책의 사전 검증까지 수행하지만,
// 플러그인의 권위 배치 상태를 커밋하지 않는다. 결과 Transform도 이 Preview Actor에만 적용.
FFFZSPlacementValidation AFFZSPlacementPreviewActor::RefreshPreview()
{
    bUsesEffectiveBatchSpec = false;
    if (UFFZSWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr)
    {
        LastValidation = Subsystem->ValidatePlacement(PreviewCommand);
        if (!LastValidation.Reasons.Contains(RequestedFailureReason)) RequestedFailureReason = EFFZSPlacementFailure::None;
        bHasUnvalidatedPreviewTransform = false;
        SetActorTransform(LastValidation.PlacementTransform);
        RequestPreviewMesh();
        PreviewMesh->SetRenderCustomDepth(true);
        PreviewMesh->SetCustomDepthStencilValue(LastValidation.bIsValid ? 1 : 2);
        RefreshGhostFeedback();
        RefreshRangePresentation();
        RefreshPlacementGridPresentation();
    }
    else
    {
        PreviewMesh->SetOverlayMaterial(nullptr);
        for (UStaticMeshComponent* Surface : FailureSurfaces) if (Surface) Surface->SetVisibility(false);
        if (RangeLines)
        {
            RangeLines->Flush();
        }
        if (PlacementGridLines)
        {
            PlacementGridLines->Flush();
        }
    }
    return LastValidation;
}

void AFFZSPlacementPreviewActor::ApplyBatchPreviewResult(const FFFZSCommand& Command,
    const FFFZSPlacementValidation& Validation)
{
    PreviewCommand = Command;
    bUsesEffectiveBatchSpec = true;
    LastValidation = Validation;
    if (!LastValidation.Reasons.Contains(RequestedFailureReason)) RequestedFailureReason = EFFZSPlacementFailure::None;
    bHasUnvalidatedPreviewTransform = false;
    SetActorTransform(LastValidation.PlacementTransform);
    RequestPreviewMesh();
    PreviewMesh->SetRenderCustomDepth(true);
    PreviewMesh->SetCustomDepthStencilValue(LastValidation.bIsValid ? 1 : 2);
    RefreshGhostFeedback();
    RefreshRangePresentation();
}

void AFFZSPlacementPreviewActor::SetUnvalidatedPreviewTransform(const FTransform& Transform)
{
    SetActorTransform(Transform);
    if (bHasUnvalidatedPreviewTransform) return;
    bHasUnvalidatedPreviewTransform = true;
    PreviewMesh->SetRenderCustomDepth(false);
    // Reuse the loaded material without asset requests or per-frame allocations. A neutral
    // ghost must not inherit the previous target's green/red verdict or blocker geometry.
    if (GhostFeedbackMaterial)
    {
        GhostFeedbackMaterial->SetVectorParameterValue(TEXT("StatusColor"), FLinearColor::White);
        GhostFeedbackMaterial->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(
            GetDefault<UFFZSBuildPresentationSettings>()->ValidPlacementOpacity, 0.0f, 1.0f));
        PreviewMesh->SetOverlayMaterial(GhostFeedbackMaterial);
    }
    else
    {
        PreviewMesh->SetOverlayMaterial(nullptr);
    }
    for (UStaticMeshComponent* Surface : FailureSurfaces) if (Surface) Surface->SetVisibility(false);
    if (RangeLines) RangeLines->Flush();
    if (UFFZSBuildableGridComponent* Grid = Cast<UFFZSBuildableGridComponent>(PlacementGridLines)) Grid->HideGrid();
    else if (PlacementGridLines) PlacementGridLines->Flush();
}

// 공통 설정과 마지막 판정으로 고스트 재질을 갱신한다. 원본 시설 재질은 수정하지 않는다.
void AFFZSPlacementPreviewActor::RefreshGhostFeedback()
{
    const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
    const bool bFootprintFailure = FFZSBuildRangePresentation::IsFailureForRegion(
        GetDisplayedFailureReason(), FFZSBuildRangePresentation::ERegion::Footprint);
    // A queue/audience overhang must not paint an otherwise clear building as the obstruction.
    PreviewMesh->SetRenderCustomDepth(LastValidation.bIsValid || bFootprintFailure);
    if (!bFeedbackMaterialLoadAttempted)
    {
        bFeedbackMaterialLoadAttempted = true;
        // 액터별 override를 우선하고, 없으면 프로젝트의 공통 재질을 한 번만 로드한다.
        const TSoftObjectPtr<UMaterialInterface>& MaterialReference = PlacementFeedbackMaterial.IsNull()
            ? Style.DefaultPlacementFeedbackMaterial : PlacementFeedbackMaterial;
        UMaterialInterface* Material = MaterialReference.Get();
        if (!Material && !MaterialReference.IsNull())
            Material = LoadObject<UMaterialInterface>(nullptr, *MaterialReference.ToSoftObjectPath().ToString(),
                nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (Material)
        {
            GhostFeedbackMaterial = UMaterialInstanceDynamic::Create(Material, this);
            FailureSurfaceMaterial = UMaterialInstanceDynamic::Create(Material, this);
        }
    }
    // 색과 투명도는 새 미리보기부터 반영한다. 재질 파라미터 이름은 공용 재질 계약이다.
    if (FailureSurfaceMaterial)
    {
        FailureSurfaceMaterial->SetVectorParameterValue(TEXT("StatusColor"), Style.InvalidPlacementColor);
        FailureSurfaceMaterial->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Style.FailureSurfaceOpacity, 0.0f, 1.0f));
    }
    if (GhostFeedbackMaterial)
    {
        GhostFeedbackMaterial->SetVectorParameterValue(TEXT("StatusColor"), LastValidation.bIsValid
            ? Style.ValidPlacementColor : bFootprintFailure ? Style.InvalidPlacementColor : FLinearColor::White);
        GhostFeedbackMaterial->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(
            bFootprintFailure ? Style.InvalidPlacementOpacity : Style.ValidPlacementOpacity, 0.0f, 1.0f));
        PreviewMesh->SetOverlayMaterial(GhostFeedbackMaterial);
    }
}

// 실패 영역 바닥의 반투명 면을 재사용하며 충돌과 내비게이션에는 참여시키지 않는다.
void AFFZSPlacementPreviewActor::ShowFailureSurface(
    const FFFZSOrientedRect& WorldRegion, const double BaseZ, const int32 SurfaceIndex)
{
    if (!FailureSurfaceMaterial || !WorldRegion.IsValid() || SurfaceIndex < 0 ||
        SurfaceIndex >= FFFZSPlacementValidation::MaxObstructions) return;
    if (!FailureSurfaces.IsValidIndex(SurfaceIndex))
    {
        UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
        if (!Plane) return;
        UStaticMeshComponent* Surface = NewObject<UStaticMeshComponent>(this);
        Surface->SetupAttachment(SceneRoot);
        Surface->SetAbsolute(true, true, true);
        Surface->SetStaticMesh(Plane);
        Surface->SetMaterial(0, FailureSurfaceMaterial);
        Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Surface->SetGenerateOverlapEvents(false);
        Surface->SetCanEverAffectNavigation(false);
        Surface->SetCastShadow(false);
        Surface->SetReceivesDecals(false);
        Surface->RegisterComponent();
        FailureSurfaces.Add(Surface);
    }
    if (!FailureSurfaces.IsValidIndex(SurfaceIndex)) return;
    UStaticMeshComponent* Surface = FailureSurfaces[SurfaceIndex];
    Surface->SetWorldTransform(FTransform(FRotator(0, WorldRegion.YawDegrees, 0),
        FVector(WorldRegion.CenterCell, BaseZ), FVector(WorldRegion.HalfExtentCells.X / 50.0,
            WorldRegion.HalfExtentCells.Y / 50.0, 1.0)));
    Surface->SetVisibility(true);
}

void AFFZSPlacementPreviewActor::RefreshPlacementGridVisual()
{
    RefreshPlacementGridPresentation();
}

EFFZSPlacementFailure AFFZSPlacementPreviewActor::GetDisplayedFailureReason() const
{
    return FFZSBuildRangePresentation::ResolveDisplayedFailureReason(LastValidation, RequestedFailureReason);
}

void AFFZSPlacementPreviewActor::SetDisplayedFailureReason(const EFFZSPlacementFailure Reason)
{
    const EFFZSPlacementFailure PreviousReason = GetDisplayedFailureReason();
    RequestedFailureReason = LastValidation.Reasons.Contains(Reason) ? Reason : EFFZSPlacementFailure::None;
    if (PreviousReason == GetDisplayedFailureReason() || bHasUnvalidatedPreviewTransform) return;
    RefreshGhostFeedback();
    RefreshRangePresentation();
}

void AFFZSPlacementPreviewActor::SetPlacementGridSurface(
    const FVector& WorldPoint,
    const FVector& WorldNormal)
{
    const FVector SafeNormal = WorldNormal.GetSafeNormal();
    bHasPlacementGridSurface = !WorldPoint.ContainsNaN() && !SafeNormal.ContainsNaN() &&
        !SafeNormal.IsNearlyZero();
    if (!bHasPlacementGridSurface)
    {
        PlacementGridSurfacePointWorld = FVector::ZeroVector;
        PlacementGridSurfaceNormalWorld = FVector::UpVector;
        return;
    }
    PlacementGridSurfacePointWorld = WorldPoint;
    PlacementGridSurfaceNormalWorld = SafeNormal;
}

bool AFFZSPlacementPreviewActor::GetPlacementGridSurface(FVector& OutWorldPoint, FVector& OutWorldNormal) const
{
    if (!bHasPlacementGridSurface)
    {
        return false;
    }
    OutWorldPoint = PlacementGridSurfacePointWorld;
    OutWorldNormal = PlacementGridSurfaceNormalWorld;
    return true;
}

// 권위 변경의 유일한 진입점이다. ExecuteCommand가 Game Thread, Phase, 중복 CommandId, Revision,
// 배치 유효성, 외부 정책, 롤백을 다시 처리하므로 LastValidation을 그대로 신뢰해 설치하지 않는다.
FFFZSCommandResult AFFZSPlacementPreviewActor::CommitPreview()
{
    if (UFFZSWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr)
    {
        return Subsystem->ExecuteCommand(PreviewCommand);
    }
    FFFZSCommandResult Result;
    Result.FailureReason = EFFZSPlacementFailure::InternalError;
    Result.Message = FText::FromString(TEXT("FF Zone System World Subsystem을 사용할 수 없습니다."));
    return Result;
}

//비동기 메시 경합 처리
 // 원하는 SoftObjectPath가 같으면 기존 요청/표시를 재사용한다.
 // 경로가 바뀌면 이전 Handle을 취소하고 기존 메시를 비운 뒤 새 요청을 만든다.
 // 취소는 이미 완료 직전인 콜백을 절대적인 순서 장벽으로 가정하지 않는다. 최종 콜백은 아래에서
 // 현재 명령 경로와 RequestedVisualPath를 다시 비교하고 현재 SoftObjectPtr만 읽는다.
 // 따라서 요청 A 뒤에 B를 선택했는데 A가 늦게 완료되어 A의 메시를 덮어쓰는 일을 막는다.
 
void AFFZSPlacementPreviewActor::RequestPreviewMesh()
{
    const FSoftObjectPath NewPath = PreviewCommand.FacilitySpec.VisualMesh.ToSoftObjectPath();
    if (NewPath == RequestedVisualPath)
    {
        PreviewMesh->SetRelativeTransform(FFZSVisualGeometry::ResolveVisualMeshTransform(
            PreviewCommand.FacilitySpec,
            PreviewMesh->GetStaticMesh()));
        return;
    }

    RequestedVisualPath = NewPath;
    if (VisualLoadHandle.IsValid())
    {
        VisualLoadHandle->CancelHandle();
        VisualLoadHandle.Reset();
    }
    PreviewMesh->SetStaticMesh(nullptr);
    PreviewMesh->SetRelativeTransform(FFZSVisualGeometry::ResolveVisualMeshTransform(
        PreviewCommand.FacilitySpec,
        nullptr));
    if (!RequestedVisualPath.IsValid())
    {
        return;
    }
    if (UStaticMesh* LoadedMesh = PreviewCommand.FacilitySpec.VisualMesh.Get())
    {
        PreviewMesh->SetStaticMesh(LoadedMesh);
        PreviewMesh->SetRelativeTransform(FFZSVisualGeometry::ResolveVisualMeshTransform(
            PreviewCommand.FacilitySpec,
            LoadedMesh));
        return;
    }

    VisualLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        RequestedVisualPath,
        FStreamableDelegate::CreateUObject(this, &AFFZSPlacementPreviewActor::ApplyLoadedPreviewMesh));
}

// 어느 요청의 델리게이트가 먼저 깨어났는지와 무관하게 현재 선택 경로가 일치할 때만 표시.
// EndPlay에서는 Handle을 취소하고 CreateUObject 델리게이트는 Actor 수명을 추적.
void AFFZSPlacementPreviewActor::ApplyLoadedPreviewMesh()
{
    VisualLoadHandle.Reset();
    if (PreviewCommand.FacilitySpec.VisualMesh.ToSoftObjectPath() != RequestedVisualPath)
    {
        return;
    }
    if (UStaticMesh* LoadedMesh = PreviewCommand.FacilitySpec.VisualMesh.Get())
    {
        PreviewMesh->SetStaticMesh(LoadedMesh);
        PreviewMesh->SetRelativeTransform(FFZSVisualGeometry::ResolveVisualMeshTransform(
            PreviewCommand.FacilitySpec,
            LoadedMesh));
    }
}

// 판정 결과를 설명하는 비권위 제품 표현이다. DrawDebug 수명과 빌드 설정에 의존하지 않고 한 개의
// LineBatch scene proxy에 canonical rect를 보관해 커서가 멈춰도 범위가 사라지지 않는다.
void AFFZSPlacementPreviewActor::RefreshRangePresentation()
{
    for (UStaticMeshComponent* Surface : FailureSurfaces) if (Surface) Surface->SetVisibility(false);
    if (!RangeLines)
    {
        return;
    }
    RangeLines->Flush();
    RangeLines->SetHiddenInGame(!bDrawDebugCells);
    if (!bDrawDebugCells)
    {
        return;
    }

    const UFFZSWorldSubsystem* Subsystem =
        GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    const AFFZSGameplayZone* Zone = Subsystem ? Subsystem->FindZone(PreviewCommand.ZoneId) : nullptr;
    if (!Zone)
    {
        return;
    }

    const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
    const double BaseZ = LastValidation.PlacementTransform.GetLocation().Z +
        FMath::Clamp(Style.RangeSurfaceLiftCm, 0.5f, 20.0f);
    // Use precisely the same height slice as validation. The ground lift is drawing-only.
    const double AuthorityZ = Zone->GetActorLocation().Z;
    const AFFZSBuildArea* BuildArea = Subsystem->FindBuildArea();
    FFFZSBuildAreaGeometry FallbackGeometry;
    if (!BuildArea)
    {
        FFFZSBuildAreaSourcePolygon Source;
        for (const FVector2D& Point : Zone->GetBoundaryPolygonLocalCm())
            Source.WorldVertices.Add(FVector2D(Zone->GetActorTransform().TransformPosition(FVector(Point, 0))));
        Source.MinZ = AuthorityZ - Zone->VerticalExtentCm * 0.5;
        Source.MaxZ = AuthorityZ + Zone->VerticalExtentCm * 0.5;
        FallbackGeometry.Rebuild({Source});
    }
    TArray<FBatchedLine> Lines;
    FFZSBuildRangePresentation::AppendPlacementRanges(Lines, LastValidation, Zone->GetActorTransform(), BaseZ, Style);

    // Keep the true build boundary visible even when the optional G grid is hidden. Clip its
    // display to the selected reservation and nearby land; never draw internal zone seams.
    FBox2D ViewBounds(EForceInit::ForceInit);
    const auto IncludeGroupBounds = [Zone, &ViewBounds](const TArray<FFFZSOrientedRect>& Regions)
    {
        for (const FFFZSOrientedRect& Region : Regions)
            if (Region.IsValid())
                for (const FVector2D& Corner : FFZSOrientedGeometry::GetCorners(Region))
                    ViewBounds += FVector2D(Zone->GetActorTransform().TransformPosition(FVector(Corner, 0)));
    };
    IncludeGroupBounds(LastValidation.FootprintOrientedRects);
    IncludeGroupBounds(LastValidation.QueueOrientedRects);
    IncludeGroupBounds(LastValidation.AudienceOrientedRects);
    if (!ViewBounds.bIsValid) ViewBounds += FVector2D(LastValidation.PlacementTransform.GetLocation());
    ViewBounds = ViewBounds.ExpandBy(FMath::Clamp(
        GetDefault<UFFZSDeveloperSettings>()->PlacementGridPaddingCm, 300.0f, 2000.0f));
    TArray<FFFZSBuildAreaLineSegment> BoundarySegments;
    if (BuildArea) BuildArea->GetBoundarySegments(AuthorityZ, ViewBounds, BoundarySegments);
    else FallbackGeometry.GetBoundarySegments(AuthorityZ, ViewBounds, BoundarySegments);
    FFZSBuildRangePresentation::AppendPlanarSegments(Lines, BoundarySegments, BaseZ + 0.25,
        FLinearColor(0.035f, 0.045f, 0.055f, 0.9f), 5.0f);
    FFZSBuildRangePresentation::AppendPlanarSegments(Lines, BoundarySegments, BaseZ + 0.5,
        Style.BuildBoundaryColor, 2.5f);
    if (!LastValidation.bIsValid)
    {
        const EFFZSPlacementFailure DisplayedReason = GetDisplayedFailureReason();
        int32 SurfaceIndex = 0;
        TArray<FFFZSOrientedRect, TInlineAllocator<12>> DisplayedRegions;
        const auto DrawFailureRegion = [this, &Lines, &Style, &SurfaceIndex, &DisplayedRegions, BaseZ](const FFFZSOrientedRect& Region)
        {
            if (!Region.IsValid() || SurfaceIndex >= FFFZSPlacementValidation::MaxObstructions) return;
            for (const FFFZSOrientedRect& Existing : DisplayedRegions)
                if (Existing.CenterCell.Equals(Region.CenterCell, 0.01) &&
                    Existing.HalfExtentCells.Equals(Region.HalfExtentCells, 0.01) &&
                    FMath::IsNearlyEqual(Existing.YawDegrees, Region.YawDegrees)) return;
            DisplayedRegions.Add(Region);
            FFZSBuildRangePresentation::FWorldCorners Corners;
            const auto XY = FFZSOrientedGeometry::GetCorners(Region);
            for (int32 Index = 0; Index < 4; ++Index) Corners[Index] = FVector(XY[Index], BaseZ);
            FFZSBuildRangePresentation::AppendFailureHatching(Lines, Corners, Style.FailureColor);
            ShowFailureSurface(Region, BaseZ, SurfaceIndex++);
        };
        for (const FFFZSPlacementObstruction& Obstruction : LastValidation.Obstructions)
        {
            // Boundary failures have clipped diagnostics below; a full red plane would imply
            // the entire reservation is outside, including the portion that can be built on.
            if (Obstruction.Reason != DisplayedReason || Obstruction.Reason == EFFZSPlacementFailure::OutsideZone) continue;
            FFFZSOrientedRect OverlapSurface;
            if (FFZSBuildRangePresentation::AppendObstructionFeedback(Lines, Obstruction, BaseZ + 1.0,
                FLinearColor(1.0f, 0.62f, 0.12f), Style.FailureColor, &OverlapSurface))
            {
                // A rectangular intersection can reuse the inexpensive plane. Other convex
                // intersections retain their clipped hatching without an oversized red fill.
                if (OverlapSurface.IsValid()) ShowFailureSurface(OverlapSurface, BaseZ, SurfaceIndex++);
            }
            else DrawFailureRegion(Obstruction.AffectedWorldRegion);
        }
        TArray<FFFZSBuildAreaLineSegment> HatchSegments;
        TArray<FFFZSBuildAreaLineSegment> OutsideSegments;
        TArray<FFFZSBuildAreaLineSegment> IssueBoundarySegments;
        for (const auto& Issue : LastValidation.BoundaryIssues)
        {
            if (DisplayedReason != EFFZSPlacementFailure::OutsideZone) break;
            FFZSBuildRangePresentation::BuildFailureHatchSegments(Issue.WorldRegion, HatchSegments);
            if (BuildArea) BuildArea->GetOutsideSegments(Issue.WorldRegion, AuthorityZ, HatchSegments, OutsideSegments);
            else FallbackGeometry.GetOutsideSegments(Issue.WorldRegion, AuthorityZ, HatchSegments, OutsideSegments);
            if (OutsideSegments.IsEmpty())
            {
                // A narrow enclosed hole can fall between all regular hatch strokes.
                const FBox2D IssueBounds = FFZSOrientedGeometry::GetAabb(Issue.WorldRegion);
                if (BuildArea) BuildArea->GetBoundarySegments(AuthorityZ, IssueBounds, IssueBoundarySegments);
                else FallbackGeometry.GetBoundarySegments(AuthorityZ, IssueBounds, IssueBoundarySegments);
                FFZSBuildRangePresentation::AppendBoundaryHatchSeeds(Issue.WorldRegion, IssueBoundarySegments, HatchSegments);
                if (BuildArea) BuildArea->GetOutsideSegments(Issue.WorldRegion, AuthorityZ, HatchSegments, OutsideSegments);
                else FallbackGeometry.GetOutsideSegments(Issue.WorldRegion, AuthorityZ, HatchSegments, OutsideSegments);
            }
            FFZSBuildRangePresentation::AppendPlanarSegments(Lines, OutsideSegments, BaseZ + 1.0,
                Style.FailureColor, 2.0f);
        }
        const auto DrawUnlocalizedFailure = [this, Zone, &DrawFailureRegion, DisplayedReason](const TArray<FFFZSOrientedRect>& Regions,
            const FFZSBuildRangePresentation::ERegion RegionType)
        {
            if (!FFZSBuildRangePresentation::IsFailureForRegion(DisplayedReason, RegionType)) return;
            if (LastValidation.Obstructions.ContainsByPredicate([DisplayedReason](const FFFZSPlacementObstruction& D)
                { return D.Reason == DisplayedReason; })) return;
            for (const FFFZSOrientedRect& Rect : Regions)
            {
                FFFZSOrientedRect WorldRect = Rect;
                WorldRect.CenterCell = FVector2D(Zone->GetActorTransform().TransformPosition(FVector(Rect.CenterCell, 0)));
                WorldRect.YawDegrees += Zone->GetActorRotation().Yaw;
                DrawFailureRegion(WorldRect);
            }
        };
        DrawUnlocalizedFailure(LastValidation.FootprintOrientedRects, FFZSBuildRangePresentation::ERegion::Footprint);
        DrawUnlocalizedFailure(LastValidation.AudienceOrientedRects, FFZSBuildRangePresentation::ERegion::Audience);
        DrawUnlocalizedFailure(LastValidation.QueueOrientedRects, FFZSBuildRangePresentation::ERegion::Queue);
    }
    if (!LastValidation.QueueOrientedRects.IsEmpty())
    {
        const FFFZSFacilitySpec DisplaySpec = PreviewCommand.Type == EFFZSCommandType::Install && !bUsesEffectiveBatchSpec
            ? FFZSFacilityUpgradeRules::BuildEffectiveSpec(PreviewCommand.FacilitySpec, 1, 1.0f)
            : PreviewCommand.FacilitySpec;
        FFZSBuildRangePresentation::AppendQueuePath(Lines, DisplaySpec,
            LastValidation.PlacementTransform, Style.QueueColor, Style, true);
        FFZSBuildRangePresentation::AppendQueueSlots(Lines, DisplaySpec,
            LastValidation.PlacementTransform, Style.QueueColor, Style, true);
    }
    RangeLines->DrawLines(Lines);
}

// The same available-land renderer powers placement and the pre-selection overview.
void AFFZSPlacementPreviewActor::RefreshPlacementGridPresentation()
{
    UFFZSBuildableGridComponent* Grid = Cast<UFFZSBuildableGridComponent>(PlacementGridLines);
    if (!Grid)
    {
        return;
    }
    UFFZSWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    const AFFZSGameplayZone* Zone = Subsystem ? Subsystem->FindZone(PreviewCommand.ZoneId) : nullptr;
    const FIntPoint Size = PreviewCommand.FacilitySpec.FootprintSizeCells;
    if (bHasUnvalidatedPreviewTransform || !bDrawPlacementGrid || !Zone || Size.X <= 0 || Size.Y <= 0)
    {
        Grid->HideGrid();
        return;
    }
    const float Yaw = UFFZSGridLibrary::YawCentidegreesToDegrees(
        UFFZSGridLibrary::GetEffectiveYawCentidegrees(PreviewCommand.Placement));
    const FVector2D Center = FVector2D(PreviewCommand.Placement.OriginCell) + FVector2D(0.5, 0.5) +
        FVector2D((Size.X - 1.0) * 0.5, (Size.Y - 1.0) * 0.5).GetRotated(Yaw);
    const double Radians = FMath::DegreesToRadians(static_cast<double>(Yaw));
    const double AbsCos = FMath::Abs(FMath::Cos(Radians));
    const double AbsSin = FMath::Abs(FMath::Sin(Radians));
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const FVector2D HalfExtent(
        FFZSPlacementGridModel::ResolveGridHalfExtentCm(
            FMath::CeilToInt(AbsCos * Size.X + AbsSin * Size.Y), Settings->PlacementGridPaddingCm,
            Settings->PlacementGridMaxRadiusCm, PlacementGridStepCm),
        FFZSPlacementGridModel::ResolveGridHalfExtentCm(
            FMath::CeilToInt(AbsSin * Size.X + AbsCos * Size.Y), Settings->PlacementGridPaddingCm,
            Settings->PlacementGridMaxRadiusCm, PlacementGridStepCm));
    Grid->RefreshGrid(*Subsystem, PreviewCommand, Center, HalfExtent, PlacementGridStepCm,
        bHasPlacementGridSurface ? PlacementGridSurfacePointWorld : LastValidation.PlacementTransform.GetLocation(),
        bHasPlacementGridSurface ? PlacementGridSurfaceNormalWorld : FVector::UpVector);
}
