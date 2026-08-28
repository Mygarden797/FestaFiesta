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

namespace
{
    constexpr double GridGeometryToleranceCm = 0.01;
    constexpr int32 GridFadeSegmentCount = 4;
    constexpr int32 MaximumGridLinesPerAxis = 512;

    double Cross2D(const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.Y - A.Y * B.X;
    }

    bool IsPointOnSegment2D(
        const FVector2D& Point,
        const FVector2D& A,
        const FVector2D& B)
    {
        const FVector2D Segment = B - A;
        const double LengthSquared = Segment.SizeSquared();
        if (LengthSquared <= UE_SMALL_NUMBER)
        {
            return FVector2D::Distance(Point, A) <= GridGeometryToleranceCm;
        }
        const double T = FMath::Clamp(
            FVector2D::DotProduct(Point - A, Segment) / LengthSquared,
            0.0,
            1.0);
        return FVector2D::Distance(Point, A + Segment * T) <= GridGeometryToleranceCm;
    }

    bool IsPointInsidePolygonInclusive(
        const FVector2D& Point,
        const TArray<FVector2D>& Polygon)
    {
        if (Polygon.Num() < 3)
        {
            return false;
        }

        bool bInside = false;
        for (int32 I = 0, J = Polygon.Num() - 1; I < Polygon.Num(); J = I++)
        {
            const FVector2D& A = Polygon[J];
            const FVector2D& B = Polygon[I];
            if (IsPointOnSegment2D(Point, A, B))
            {
                return true;
            }
            const bool bCrosses = ((B.Y > Point.Y) != (A.Y > Point.Y)) &&
                (Point.X < (A.X - B.X) * (Point.Y - B.Y) /
                    ((A.Y - B.Y) + UE_SMALL_NUMBER) + B.X);
            if (bCrosses)
            {
                bInside = !bInside;
            }
        }
        return bInside;
    }

    template <typename AllocatorType>
    void AddSegmentPolygonIntersections(
        const FVector2D& Start,
        const FVector2D& End,
        const TArray<FVector2D>& Polygon,
        TArray<double, AllocatorType>& OutParameters)
    {
        const FVector2D Direction = End - Start;
        const double DirectionLengthSquared = Direction.SizeSquared();
        if (DirectionLengthSquared <= UE_SMALL_NUMBER)
        {
            return;
        }

        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            const FVector2D& EdgeStart = Polygon[Index];
            const FVector2D& EdgeEnd = Polygon[(Index + 1) % Polygon.Num()];
            const FVector2D EdgeDirection = EdgeEnd - EdgeStart;
            const FVector2D StartToEdge = EdgeStart - Start;
            const double Denominator = Cross2D(Direction, EdgeDirection);
            if (FMath::Abs(Denominator) > GridGeometryToleranceCm)
            {
                const double T = Cross2D(StartToEdge, EdgeDirection) / Denominator;
                const double U = Cross2D(StartToEdge, Direction) / Denominator;
                if (T >= -GridGeometryToleranceCm && T <= 1.0 + GridGeometryToleranceCm &&
                    U >= -GridGeometryToleranceCm && U <= 1.0 + GridGeometryToleranceCm)
                {
                    OutParameters.Add(FMath::Clamp(T, 0.0, 1.0));
                }
                continue;
            }

            // 격자선과 경계선이 같은 직선 위에 놓인 경우 두 경계 끝점을 모두 분할점으로 넣는다.
            if (FMath::Abs(Cross2D(StartToEdge, Direction)) <= GridGeometryToleranceCm)
            {
                const double T0 = FVector2D::DotProduct(EdgeStart - Start, Direction) /
                    DirectionLengthSquared;
                const double T1 = FVector2D::DotProduct(EdgeEnd - Start, Direction) /
                    DirectionLengthSquared;
                if (T0 >= -GridGeometryToleranceCm && T0 <= 1.0 + GridGeometryToleranceCm)
                {
                    OutParameters.Add(FMath::Clamp(T0, 0.0, 1.0));
                }
                if (T1 >= -GridGeometryToleranceCm && T1 <= 1.0 + GridGeometryToleranceCm)
                {
                    OutParameters.Add(FMath::Clamp(T1, 0.0, 1.0));
                }
            }
        }
    }
}

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

    PlacementGridLines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("PlacementGridLines"));
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
    const FColor BaseColor = LastValidation.bIsValid ? FColor::Green : FColor::Red;
    // [하드코딩 제거] 건축 모드 범위 표시와 같은 상한을 설정 헤더에서 공유한다.
    constexpr int32 MaxDisplayedRectsPerGroup =
        UFFZSDeveloperSettings::MaxDisplayedRangeRectsPerGroup;
    const auto DrawRect = [this, Zone, CellSizeCm](const FFFZSOrientedRect& Rect, const FLinearColor& Color)
    {
        const FVector Center = Zone->GetActorTransform().TransformPosition(FVector(
            Rect.CenterCell.X * CellSizeCm,
            Rect.CenterCell.Y * CellSizeCm,
            5.0));
        const FVector Extent(
            FMath::Max(0.25, Rect.HalfExtentCells.X * CellSizeCm - 0.02),
            FMath::Max(0.25, Rect.HalfExtentCells.Y * CellSizeCm - 0.02),
            5.0);
        const FQuat Rotation = Zone->GetActorQuat() * FQuat(
            FVector::UpVector,
            FMath::DegreesToRadians(static_cast<double>(Rect.YawDegrees)));
        RangeLines->DrawBox(
            Center,
            Extent,
            Rotation,
            Color,
            0.0f,
            SDPG_World,
            2.0f);
    };
    const auto DrawGroup = [&DrawRect, BaseColor](
        const TArray<FFFZSOrientedRect>& Rects,
        const FColor& ValidColor)
    {
        const FLinearColor Color(BaseColor == FColor::Red ? BaseColor : ValidColor);
        if (Rects.Num() <= MaxDisplayedRectsPerGroup)
        {
            for (const FFFZSOrientedRect& Rect : Rects)
            {
                if (Rect.IsValid())
                {
                    DrawRect(Rect, Color);
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
                Bounds.GetCenter(), Bounds.GetExtent(), 0.0f), Color);
        }
    };
    DrawGroup(LastValidation.FootprintOrientedRects, FColor::Green);
    DrawGroup(LastValidation.ClearanceOrientedRects, FColor::Cyan);
    DrawGroup(LastValidation.QueueOrientedRects, FColor::Yellow);
}

// 사용자에게 필요한 순간에만 보이는 비권위 로컬 격자다. Zone 폴리곤을 한 번 읽어 각 선분을
// 정확히 잘라 내고, 모든 선을 한 DrawLines 호출로 제출한다. 따라서 셀별 Actor/Component가 없고
// 오목한 Zone 경계 밖이나 화면 전체를 덮는 반투명 판도 만들지 않는다.
void AFFZSPlacementPreviewActor::RefreshPlacementGridPresentation()
{
    if (!PlacementGridLines)
    {
        return;
    }

    PlacementGridLines->Flush();
    PlacementGridLines->SetHiddenInGame(!bDrawPlacementGrid);
    if (!bDrawPlacementGrid)
    {
        return;
    }

    const UFFZSWorldSubsystem* Subsystem =
        GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    const AFFZSGameplayZone* Zone = Subsystem ? Subsystem->FindZone(PreviewCommand.ZoneId) : nullptr;
    if (!Zone || PreviewCommand.FacilitySpec.FootprintSizeCells.X <= 0 ||
        PreviewCommand.FacilitySpec.FootprintSizeCells.Y <= 0)
    {
        PlacementGridLines->SetHiddenInGame(true);
        return;
    }

    const TArray<FVector2D> BoundaryPolygon = Zone->GetBoundaryPolygonLocalCm();
    if (BoundaryPolygon.Num() < 3)
    {
        PlacementGridLines->SetHiddenInGame(true);
        return;
    }

    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const int32 GridStepCm = FFZSPlacementGridModel::SanitizeGridStepCm(PlacementGridStepCm);
    const float PaddingCm = Settings ? Settings->PlacementGridPaddingCm : 600.0f;
    const float MaximumRadiusCm = Settings ? Settings->PlacementGridMaxRadiusCm : 3600.0f;
    const float SurfaceLiftCm = Settings && FMath::IsFinite(Settings->PlacementGridSurfaceLiftCm)
        ? FMath::Clamp(Settings->PlacementGridSurfaceLiftCm, 0.5f, 20.0f)
        : 3.0f;
    const int32 RequestedMajorIntervalCm = Settings
        ? Settings->PlacementGridMajorIntervalCm
        : 500;
    const int32 MajorIntervalCm = FMath::Max(
        GridStepCm,
        FMath::RoundToInt(static_cast<double>(RequestedMajorIntervalCm) / GridStepCm) * GridStepCm);

    const float YawDegrees = UFFZSGridLibrary::YawCentidegreesToDegrees(
        UFFZSGridLibrary::GetEffectiveYawCentidegrees(PreviewCommand.Placement));
    const FVector2D CenterFromFirstCell(
        (static_cast<double>(PreviewCommand.FacilitySpec.FootprintSizeCells.X) - 1.0) * 0.5,
        (static_cast<double>(PreviewCommand.FacilitySpec.FootprintSizeCells.Y) - 1.0) * 0.5);
    const FVector2D FootprintCenterCell = FVector2D(0.5, 0.5) +
        CenterFromFirstCell.GetRotated(YawDegrees);
    const FVector2D GridCenterLocalCm(
        PreviewCommand.Placement.OriginCell.X + FootprintCenterCell.X,
        PreviewCommand.Placement.OriginCell.Y + FootprintCenterCell.Y);

    const double YawRadians = FMath::DegreesToRadians(static_cast<double>(YawDegrees));
    const double AbsCos = FMath::Abs(FMath::Cos(YawRadians));
    const double AbsSin = FMath::Abs(FMath::Sin(YawRadians));
    const int32 RotatedFootprintWidthCm = FMath::CeilToInt(
        AbsCos * PreviewCommand.FacilitySpec.FootprintSizeCells.X +
        AbsSin * PreviewCommand.FacilitySpec.FootprintSizeCells.Y);
    const int32 RotatedFootprintDepthCm = FMath::CeilToInt(
        AbsSin * PreviewCommand.FacilitySpec.FootprintSizeCells.X +
        AbsCos * PreviewCommand.FacilitySpec.FootprintSizeCells.Y);
    const float HalfExtentX = FFZSPlacementGridModel::ResolveGridHalfExtentCm(
        RotatedFootprintWidthCm, PaddingCm, MaximumRadiusCm, GridStepCm);
    const float HalfExtentY = FFZSPlacementGridModel::ResolveGridHalfExtentCm(
        RotatedFootprintDepthCm, PaddingCm, MaximumRadiusCm, GridStepCm);

    const double MinX = FFZSPlacementGridModel::AlignDownToGrid(
        GridCenterLocalCm.X - HalfExtentX, GridStepCm);
    const double MaxX = FFZSPlacementGridModel::AlignUpToGrid(
        GridCenterLocalCm.X + HalfExtentX, GridStepCm);
    const double MinY = FFZSPlacementGridModel::AlignDownToGrid(
        GridCenterLocalCm.Y - HalfExtentY, GridStepCm);
    const double MaxY = FFZSPlacementGridModel::AlignUpToGrid(
        GridCenterLocalCm.Y + HalfExtentY, GridStepCm);
    const int32 XLineCount = FMath::Clamp(
        FMath::RoundToInt((MaxX - MinX) / GridStepCm) + 1,
        0,
        MaximumGridLinesPerAxis);
    const int32 YLineCount = FMath::Clamp(
        FMath::RoundToInt((MaxY - MinY) / GridStepCm) + 1,
        0,
        MaximumGridLinesPerAxis);

    const FTransform ZoneTransform = Zone->GetActorTransform();
    const FVector PreviewLocalLocation = ZoneTransform.InverseTransformPosition(
        LastValidation.PlacementTransform.GetLocation());
    const double FallbackLocalZ = FMath::IsFinite(PreviewLocalLocation.Z) &&
        FMath::Abs(PreviewLocalLocation.Z) <= Zone->VerticalExtentCm * 0.5
        ? PreviewLocalLocation.Z
        : 0.0;
    const FVector SurfacePointLocal = ZoneTransform.InverseTransformPosition(
        PlacementGridSurfacePointWorld);
    const FVector SurfaceNormalLocal = ZoneTransform.InverseTransformVectorNoScale(
        PlacementGridSurfaceNormalWorld).GetSafeNormal();
    const bool bUseSurfacePlane = bHasPlacementGridSurface &&
        !SurfacePointLocal.ContainsNaN() && !SurfaceNormalLocal.ContainsNaN() &&
        FMath::Abs(SurfaceNormalLocal.Z) >= 0.25;
    const FVector SurfaceLiftDirectionWorld = bUseSurfacePlane
        ? PlacementGridSurfaceNormalWorld.GetSafeNormal()
        : ZoneTransform.GetUnitAxis(EAxis::Z);
    const double MaximumSurfaceLocalZ = FMath::Max(
        100.0,
        static_cast<double>(Zone->VerticalExtentCm) * 0.5);
    const auto MakeWorldGridPoint = [
        &ZoneTransform,
        &SurfacePointLocal,
        &SurfaceNormalLocal,
        &SurfaceLiftDirectionWorld,
        bUseSurfacePlane,
        FallbackLocalZ,
        MaximumSurfaceLocalZ,
        SurfaceLiftCm](const FVector2D& LocalPoint)
    {
        double LocalZ = FallbackLocalZ;
        if (bUseSurfacePlane)
        {
            LocalZ = SurfacePointLocal.Z -
                (SurfaceNormalLocal.X * (LocalPoint.X - SurfacePointLocal.X) +
                    SurfaceNormalLocal.Y * (LocalPoint.Y - SurfacePointLocal.Y)) /
                    SurfaceNormalLocal.Z;
            LocalZ = FMath::Clamp(LocalZ, -MaximumSurfaceLocalZ, MaximumSurfaceLocalZ);
        }
        return ZoneTransform.TransformPosition(FVector(LocalPoint.X, LocalPoint.Y, LocalZ)) +
            SurfaceLiftDirectionWorld * SurfaceLiftCm;
    };

    TArray<FBatchedLine> BatchedLines;
    BatchedLines.Reserve((XLineCount + YLineCount) * GridFadeSegmentCount * 2);
    const auto AppendStyledLine = [
        &BatchedLines,
        &MakeWorldGridPoint](
        const FVector2D& LocalStart,
        const FVector2D& LocalEnd,
        const float Opacity,
        const bool bMajor)
    {
        if (Opacity <= UE_KINDA_SMALL_NUMBER || LocalStart.Equals(LocalEnd, 0.01))
        {
            return;
        }
        const FVector WorldStart = MakeWorldGridPoint(LocalStart);
        const FVector WorldEnd = MakeWorldGridPoint(LocalEnd);
        const float InnerThickness = bMajor ? 2.0f : 1.15f;
        const float ShadowThickness = InnerThickness + 2.25f;
        BatchedLines.Emplace(
            WorldStart,
            WorldEnd,
            FLinearColor(0.005f, 0.02f, 0.025f, 0.72f * Opacity),
            0.0f,
            ShadowThickness,
            SDPG_World);
        BatchedLines.Emplace(
            WorldStart,
            WorldEnd,
            FLinearColor(0.18f, 1.0f, 0.80f, (bMajor ? 0.72f : 0.46f) * Opacity),
            0.0f,
            InnerThickness,
            SDPG_World);
    };

    const auto AppendClippedGridLine = [
        &BoundaryPolygon,
        &AppendStyledLine,
        &GridCenterLocalCm,
        HalfExtentX,
        HalfExtentY](
        const FVector2D& SegmentStart,
        const FVector2D& SegmentEnd,
        const bool bMajor)
    {
        TArray<double, TInlineAllocator<32>> Parameters;
        Parameters.Add(0.0);
        Parameters.Add(1.0);
        AddSegmentPolygonIntersections(
            SegmentStart, SegmentEnd, BoundaryPolygon, Parameters);
        Parameters.Sort();

        TArray<double, TInlineAllocator<32>> UniqueParameters;
        UniqueParameters.Reserve(Parameters.Num());
        for (const double Parameter : Parameters)
        {
            if (UniqueParameters.IsEmpty() ||
                !FMath::IsNearlyEqual(UniqueParameters.Last(), Parameter, 0.00001))
            {
                UniqueParameters.Add(Parameter);
            }
        }

        const FVector2D Direction = SegmentEnd - SegmentStart;
        for (int32 Index = 0; Index + 1 < UniqueParameters.Num(); ++Index)
        {
            const double T0 = UniqueParameters[Index];
            const double T1 = UniqueParameters[Index + 1];
            if (T1 - T0 <= 0.00001)
            {
                continue;
            }
            const double MidpointT = (T0 + T1) * 0.5;
            if (!IsPointInsidePolygonInclusive(
                SegmentStart + Direction * MidpointT,
                BoundaryPolygon))
            {
                continue;
            }

            // 폴리곤 교차 계산은 전체 격자선당 한 번만 한다. 그 뒤 화면 페이드를 위한 네 구간과
            // 안쪽 구간의 겹치는 부분만 잘라, 25cm 모드에서도 edge 순회를 반복하지 않는다.
            for (int32 FadeIndex = 0; FadeIndex < GridFadeSegmentCount; ++FadeIndex)
            {
                const double FadeT0 = static_cast<double>(FadeIndex) / GridFadeSegmentCount;
                const double FadeT1 = static_cast<double>(FadeIndex + 1) / GridFadeSegmentCount;
                const double ClippedT0 = FMath::Max(T0, FadeT0);
                const double ClippedT1 = FMath::Min(T1, FadeT1);
                if (ClippedT1 - ClippedT0 <= 0.00001)
                {
                    continue;
                }
                const FVector2D LocalStart = SegmentStart + Direction * ClippedT0;
                const FVector2D LocalEnd = SegmentStart + Direction * ClippedT1;
                const FVector2D LocalMidpoint = (LocalStart + LocalEnd) * 0.5;
                const float Opacity = FFZSPlacementGridModel::ResolveEdgeOpacity(
                    LocalMidpoint.X, GridCenterLocalCm.X, HalfExtentX) *
                    FFZSPlacementGridModel::ResolveEdgeOpacity(
                        LocalMidpoint.Y, GridCenterLocalCm.Y, HalfExtentY);
                AppendStyledLine(LocalStart, LocalEnd, Opacity, bMajor);
            }
        }
    };

    for (int32 LineIndex = 0; LineIndex < XLineCount; ++LineIndex)
    {
        const double X = MinX + LineIndex * GridStepCm;
        const bool bMajor = FFZSPlacementGridModel::IsMajorGridCoordinate(
            X, MajorIntervalCm);
        AppendClippedGridLine(FVector2D(X, MinY), FVector2D(X, MaxY), bMajor);
    }
    for (int32 LineIndex = 0; LineIndex < YLineCount; ++LineIndex)
    {
        const double Y = MinY + LineIndex * GridStepCm;
        const bool bMajor = FFZSPlacementGridModel::IsMajorGridCoordinate(
            Y, MajorIntervalCm);
        AppendClippedGridLine(FVector2D(MinX, Y), FVector2D(MaxX, Y), bMajor);
    }

    if (!BatchedLines.IsEmpty())
    {
        PlacementGridLines->SetHiddenInGame(false);
        PlacementGridLines->DrawLines(BatchedLines);
    }
    else
    {
        PlacementGridLines->SetHiddenInGame(true);
    }
}
