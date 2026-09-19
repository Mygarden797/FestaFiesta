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
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSBuildableGridComponent.h"
#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSBuildRangePresentation.h"
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

    RangeLines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("RangeLines"));
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
    if (UFFZSWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr)
    {
        LastValidation = Subsystem->ValidatePlacement(PreviewCommand);
        SetActorTransform(LastValidation.PlacementTransform);
        RequestPreviewMesh();
        PreviewMesh->SetCustomDepthStencilValue(LastValidation.bIsValid ? 1 : 2);
        RefreshRangePresentation();
        RefreshPlacementGridPresentation();
    }
    else
    {
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

void AFFZSPlacementPreviewActor::RefreshPlacementGridVisual()
{
    RefreshPlacementGridPresentation();
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

    const double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
    TArray<FBatchedLine> Lines;
    // [하드코딩 제거] 건축 모드 범위 표시와 같은 상한을 설정 헤더에서 공유한다.
    constexpr int32 MaxDisplayedRectsPerGroup =
        UFFZSDeveloperSettings::MaxDisplayedRangeRectsPerGroup;
    const auto DrawRect = [this, Zone, CellSizeCm, &Style, &Lines](const FFFZSOrientedRect& Rect,
        const FLinearColor& Color, const bool bDashed, const bool bAccentCorners)
    {
        const auto RegionCorners = FFZSOrientedGeometry::GetCorners(Rect);
        FFZSBuildRangePresentation::FWorldCorners Corners;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Corners[Index] = Zone->GetActorTransform().TransformPosition(FVector(RegionCorners[Index] * CellSizeCm, 0.0));
            Corners[Index].Z = LastValidation.PlacementTransform.GetLocation().Z +
                FMath::Clamp(Style.RangeSurfaceLiftCm, 0.5f, 20.0f);
        }
        FFZSBuildRangePresentation::AppendOutline(Lines, Corners, Color, bDashed, bAccentCorners, Style);
    };
    const auto DrawGroup = [this, &DrawRect, &Style](
        const TArray<FFFZSOrientedRect>& Rects,
        const FLinearColor& ValidColor, const FFZSBuildRangePresentation::ERegion Region)
    {
        const FLinearColor Color = FFZSBuildRangePresentation::HasRegionFailure(LastValidation, Region)
            ? Style.FailureColor : ValidColor;
        // Highly fragmented content retains exact solid outlines without multiplying dash work.
        const bool bDashed = Region == FFZSBuildRangePresentation::ERegion::Clearance && Rects.Num() <= 64;
        const bool bAccentCorners = Region == FFZSBuildRangePresentation::ERegion::Footprint;
        if (Rects.Num() <= MaxDisplayedRectsPerGroup)
        {
            for (const FFFZSOrientedRect& Rect : Rects)
            {
                if (Rect.IsValid())
                {
                    DrawRect(Rect, Color, bDashed, bAccentCorners);
                }
            }
            return;
        }

        FBox2D Bounds(EForceInit::ForceInit);
        for (const FFFZSOrientedRect& Rect : Rects)
        {
            if (!Rect.IsValid())
            {
                continue;
            }
            for (const FVector2D& Corner : FFZSOrientedGeometry::GetCorners(Rect))
            {
                Bounds += Corner;
            }
        }
        if (Bounds.bIsValid)
        {
            DrawRect(FFFZSOrientedRect(
                Bounds.GetCenter(), Bounds.GetExtent(), 0.0f), Color, true, false);
        }
    };
    DrawGroup(LastValidation.FootprintOrientedRects, Style.FootprintColor, FFZSBuildRangePresentation::ERegion::Footprint);
    DrawGroup(LastValidation.ClearanceOrientedRects, Style.ClearanceColor, FFZSBuildRangePresentation::ERegion::Clearance);
    DrawGroup(LastValidation.QueueOrientedRects, Style.QueueColor, FFZSBuildRangePresentation::ERegion::Queue);
    if (!LastValidation.QueueOrientedRects.IsEmpty())
    {
        FFZSBuildRangePresentation::AppendQueueSlots(Lines, PreviewCommand.FacilitySpec,
            LastValidation.PlacementTransform, Style.QueueColor, Style);
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
    if (!bDrawPlacementGrid || !Zone || Size.X <= 0 || Size.Y <= 0)
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
