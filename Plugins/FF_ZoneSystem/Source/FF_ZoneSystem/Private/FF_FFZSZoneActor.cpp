// FF_FFZSZoneActor.cpp

// Gameplay Zone 경계, 이웃, 소속 히스테리시스, 수용량과 밀도 상태를 구현.
// 흐름: 월드 위치를 polygon에 투영하고 측정값을 설정된 hold 시간에 따라 상태로 바꾼다.
// 불변식: 히스테리시스 거리는 설정값이며 상승은 hold를 거치고 안전한 하향은 즉시 반영.
// 실패/성능: 경계 판정 비용은 polygon 꼭짓점 수에 비례하므로 불필요한 고빈도 호출을 피한다.

#include "FF_FFZSZoneActor.h"

#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"

#define LOCTEXT_NAMESPACE "FFZoneSystemZone"

// 이 파일의 책임
    // Gameplay Zone 한 개의 로컬 경계 폴리곤, 허용 시설 범주, 이웃 관계, 수용량/밀도 상태를 제공.
    //존의 등록 키와 시설 점유 셀에 대한 월드 권위는 UFFZSWorldSubsystem이 가진다. Actor는 경계와
    //계산 규칙을 제공하며, 등록 뒤 ZoneId를 임의로 바꾸어 레지스트리 키를 무효화해서는 안 된다.

// 기하 불변식
    // BoundarySpline은 닫힌 선형 단순 폴리곤이어야 하며 중복 점, 자기 교차, 면적 0을 허용하지 않는다.
    // Actor Scale은 1, Pitch/Roll은 0, Yaw는 90도 단위이고 원점은 배치 그리드에 맞아야 한다.
    // 이 계약 덕분에 로컬 cm 단위 폴리곤, 그리드 셀, 월드 Transform이 같은 공간 정의를 공유.

namespace
{
    // 경계 위 점을 내부로 포함하기 위한 공통 판정이다. 길이 0인 변도 방어하지만,
    // 그런 변 자체는 아래 데이터 검증에서 잘못된 폴리곤으로 거부.
    bool IsPointOnSegment(const FVector2D Point, const FVector2D A, const FVector2D B, const float Tolerance)
    {
        const FVector2D Segment = B - A;
        const float SegmentLengthSquared = Segment.SizeSquared();
        if (SegmentLengthSquared <= UE_SMALL_NUMBER)
        {
            return FVector2D::Distance(Point, A) <= Tolerance;
        }
        const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, Segment) / SegmentLengthSquared, 0.0f, 1.0f);
        return FVector2D::Distance(Point, A + Segment * T) <= Tolerance;
    }

    bool IsInsidePolygon(const FVector2D Point, const TArray<FVector2D>& Polygon, const float EdgeToleranceCm)
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
            if (IsPointOnSegment(Point, A, B, EdgeToleranceCm))
            {
                return true;
            }
            const bool bCrosses = ((B.Y > Point.Y) != (A.Y > Point.Y)) &&
                (Point.X < (A.X - B.X) * (Point.Y - B.Y) / ((A.Y - B.Y) + UE_SMALL_NUMBER) + B.X);
            if (bCrosses)
            {
                bInside = !bInside;
            }
        }
        return bInside;
    }
    double Cross2D(const FVector2D& A, const FVector2D& B, const FVector2D& C)
    {
        return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
    }

    bool SegmentsIntersect2D(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
    {
        constexpr double Tolerance = 0.01;
        const double AB_C = Cross2D(A, B, C);
        const double AB_D = Cross2D(A, B, D);
        const double CD_A = Cross2D(C, D, A);
        const double CD_B = Cross2D(C, D, B);
        if (((AB_C > Tolerance && AB_D < -Tolerance) || (AB_C < -Tolerance && AB_D > Tolerance)) &&
            ((CD_A > Tolerance && CD_B < -Tolerance) || (CD_A < -Tolerance && CD_B > Tolerance)))
        {
            return true;
        }
        return (FMath::Abs(AB_C) <= Tolerance && IsPointOnSegment(C, A, B, Tolerance)) ||
            (FMath::Abs(AB_D) <= Tolerance && IsPointOnSegment(D, A, B, Tolerance)) ||
            (FMath::Abs(CD_A) <= Tolerance && IsPointOnSegment(A, C, D, Tolerance)) ||
            (FMath::Abs(CD_B) <= Tolerance && IsPointOnSegment(B, C, D, Tolerance));
    }

    // 면적뿐 아니라 인접하지 않은 모든 변 쌍의 교차도 검사한다. 따라서 검증 비용은 꼭짓점 수에
    // 대해 단순 선형이 아니지만, 에디터/등록 시 잘못된 경계가 런타임 판정으로 들어오는 것을 막는다.
    bool IsSimpleNonDegeneratePolygon(const TArray<FVector2D>& Polygon)
    {
        if (Polygon.Num() < 3)
        {
            return false;
        }

        double TwiceArea = 0.0;
        for (int32 I = 0; I < Polygon.Num(); ++I)
        {
            const FVector2D& A = Polygon[I];
            const FVector2D& B = Polygon[(I + 1) % Polygon.Num()];
            if (A.ContainsNaN() || B.ContainsNaN() || FVector2D::Distance(A, B) <= 0.01)
            {
                return false;
            }
            TwiceArea += A.X * B.Y - B.X * A.Y;

            for (int32 J = I + 1; J < Polygon.Num(); ++J)
            {
                const bool bAdjacent = J == I || J == (I + 1) % Polygon.Num() ||
                    I == (J + 1) % Polygon.Num();
                if (!bAdjacent && SegmentsIntersect2D(A, B, Polygon[J], Polygon[(J + 1) % Polygon.Num()]))
                {
                    return false;
                }
            }
        }
        return FMath::Abs(TwiceArea) > 0.01;
    }


    FText DisplayNameForZone(const FName ZoneId)
    {
        const FString Raw = ZoneId.ToString().Replace(TEXT("GZ_"), TEXT(""));
        return FText::FromString(FName::NameToDisplayString(Raw, false));
    }
}

AFFZSGameplayZone::AFFZSGameplayZone()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BoundarySpline"));
    BoundarySpline->SetupAttachment(SceneRoot);
    BoundarySpline->ClearSplinePoints(false);

    const FVector DefaultPoints[] = {
        FVector(0.0f, 0.0f, 0.0f),
        FVector(10000.0f, 0.0f, 0.0f),
        FVector(10000.0f, 10000.0f, 0.0f),
        FVector(0.0f, 10000.0f, 0.0f)
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(DefaultPoints); ++Index)
    {
        BoundarySpline->AddSplinePoint(DefaultPoints[Index], ESplineCoordinateSpace::Local, false);
        BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    BoundarySpline->SetClosedLoop(true, true);
}

// BeginPlay/EndPlay는 Actor와 Subsystem의 약한 참조 레지스트리를 동기화.
// 등록 시 ZoneId 유일성과 아래 ValidateZoneData의 전체 공간 계약을 다시 확인.
void AFFZSGameplayZone::BeginPlay()
{
    Super::BeginPlay();
    if (UFFZSWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
    {
        Subsystem->RegisterZone(this);
    }
}

void AFFZSGameplayZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>())
        {
            Subsystem->UnregisterZone(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

// Spline이 원본이며 호출할 때마다 현재 점을 읽어 새 배열을 만든다. 영구 폴리곤 캐시가 아니므로
// 런타임 편집 결과는 즉시 반영되지만, 반복 호출에는 배열 할당과 Spline 점 순회 비용이 있다.
TArray<FVector2D> AFFZSGameplayZone::GetBoundaryPolygonLocalCm() const
{
    TArray<FVector2D> Result;
    if (!BoundarySpline)
    {
        return Result;
    }
    Result.Reserve(BoundarySpline->GetNumberOfSplinePoints());
    for (int32 Index = 0; Index < BoundarySpline->GetNumberOfSplinePoints(); ++Index)
    {
        const FVector Point = BoundarySpline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local);
        Result.Add(FVector2D(Point.X, Point.Y));
    }
    return Result;
}

// 월드 점을 먼저 존 로컬 공간으로 역변환한다. 수직 범위와 XY 폴리곤을 모두 통과해야 내부이며,
// 폴리곤 변에서 0.1cm 이내인 점은 경계 흔들림을 줄이기 위해 내부로 포함.
bool AFFZSGameplayZone::ContainsWorldPoint(const FVector& WorldPoint) const
{
    const FVector LocalPoint = GetActorTransform().InverseTransformPosition(WorldPoint);
    if (FMath::Abs(LocalPoint.Z) > VerticalExtentCm * 0.5f)
    {
        return false;
    }
    return IsInsidePolygon(FVector2D(LocalPoint.X, LocalPoint.Y), GetBoundaryPolygonLocalCm(), 0.1f);
}

//이 함수는 모든 존을 넓히는 소속 판정이 아니라 "직전 존 유지"용이다.
// Subsystem은 먼저 PreviousZoneId에만 이 함수를 적용하고, 실패했을 때 다른 존을 정확한 경계로 찾는다.
// 따라서 경계를 조금 벗어난 Agent가 프레임마다 두 존 사이를 튀는 현상을 줄이되, 새 존의 선점 범위를
// 임의로 확장하지 않는다. Hysteresis는 XY 변까지의 거리만 완화하며 수직 범위는 확장하지 않는다.

bool AFFZSGameplayZone::ContainsWorldPointWithHysteresis(const FVector& WorldPoint, const float HysteresisCm) const
{
    if (ContainsWorldPoint(WorldPoint))
    {
        return true;
    }

    const FVector LocalPoint3D = GetActorTransform().InverseTransformPosition(WorldPoint);
    if (FMath::Abs(LocalPoint3D.Z) > VerticalExtentCm * 0.5f)
    {
        return false;
    }

    const FVector2D LocalPoint(LocalPoint3D.X, LocalPoint3D.Y);
    const TArray<FVector2D> Polygon = GetBoundaryPolygonLocalCm();
    for (int32 I = 0; I < Polygon.Num(); ++I)
    {
        if (IsPointOnSegment(LocalPoint, Polygon[I], Polygon[(I + 1) % Polygon.Num()], HysteresisCm))
        {
            return true;
        }
    }
    return false;
}

FVector AFFZSGameplayZone::GridCellCornerToWorld1Cm(const FIntPoint Cell) const
{
    constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    return GetActorTransform().TransformPosition(
        FVector(Cell.X * CellSizeCm, Cell.Y * CellSizeCm, 0.0));
}

FIntPoint AFFZSGameplayZone::WorldToGridCell1Cm(const FVector& WorldLocation) const
{
    constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
    return FIntPoint(
        FMath::FloorToInt(Local.X / CellSizeCm),
        FMath::FloorToInt(Local.Y / CellSizeCm));
}

FVector AFFZSGameplayZone::GridCellCornerToWorld(const FIntPoint Cell, const float CellSizeCm) const
{
    // 파라미터는 ABI/Blueprint 호환용으로만 남긴다. 서로 다른 셀 크기를 런타임에 섞지 않는다.
    static_cast<void>(CellSizeCm);
    return GridCellCornerToWorld1Cm(Cell);
}
bool AFFZSGameplayZone::IsCategoryAllowed(const EFFZSFacilityCategory Category) const
{
    return bAllowAllFacilityCategories || AllowedFacilityCategories.Contains(Category);
}

// 신발끈 공식을 로컬 cm 좌표에 적용한 뒤 m²로 변환한다. Actor Scale=1 불변식이 있으므로 별도
// 스케일 보정은 하지 않으며, 유효하지 않은 점 수는 안전하게 면적 0으로 처리.
float AFFZSGameplayZone::GetBoundaryAreaSquareMeters() const
{
    const TArray<FVector2D> Polygon = GetBoundaryPolygonLocalCm();
    if (Polygon.Num() < 3)
    {
        return 0.0f;
    }
    double TwiceAreaSquareCm = 0.0;
    for (int32 I = 0; I < Polygon.Num(); ++I)
    {
        const FVector2D& A = Polygon[I];
        const FVector2D& B = Polygon[(I + 1) % Polygon.Num()];
        TwiceAreaSquareCm += static_cast<double>(A.X) * B.Y - static_cast<double>(B.X) * A.Y;
    }
    return static_cast<float>(FMath::Abs(TwiceAreaSquareCm) * 0.5 / 10000.0);
}

// 양의 Override가 최우선이고, 없으면 사용 가능 공공 면적(미지정 시 경계 면적)을 1인당 면적으로 나눈다.
// 1인당 면적은 0.01m² 이상으로 방어하고 결과는 내림하므로 소프트 수용량을 과대 계산하지 않는다.
int32 AFFZSGameplayZone::GetSoftCapacity() const
{
    if (SoftCapacityOverride > 0)
    {
        return SoftCapacityOverride;
    }
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const float AreaSquareMeters = UsablePublicAreaSquareMeters > 0.0f ? UsablePublicAreaSquareMeters : GetBoundaryAreaSquareMeters();
    return FMath::Max(0, FMath::FloorToInt(AreaSquareMeters / FMath::Max(0.01f, Settings->SquareMetersPerPersonForSoftCapacity)));
}

// 밀도 상승 규칙은 의도적으로 단계별이다.
// Caution은 현재 비율이 기준을 넘으면 즉시 반영한다.
// Warning/Critical은 각 기준 이상인 시간이 연속 HoldSeconds를 채워야 올라간다.
// 기준 아래로 내려가면 해당 누적 시간이 0으로 초기화되고 현재 비율에 따라 즉시 하향 재평가한다.
// 즉, 상승 지연은 있지만 별도의 하향 임계값을 둔 대칭형 히스테리시스는 아니다.
// 음수 Population/DeltaSeconds는 0으로 보정하고, Capacity가 0이면 비율도 0으로 둔다.

void AFFZSGameplayZone::UpdatePopulationSample(const int32 Population, const float DeltaSeconds)
{
    ReportedPopulation = FMath::Max(0, Population);
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const int32 Capacity = GetSoftCapacity();
    const float Ratio = Capacity > 0 ? static_cast<float>(ReportedPopulation) / Capacity : 0.0f;

    WarningHeldSeconds = Ratio >= Settings->DensityWarningRatio ? WarningHeldSeconds + FMath::Max(0.0f, DeltaSeconds) : 0.0f;
    CriticalHeldSeconds = Ratio >= Settings->DensityCriticalRatio ? CriticalHeldSeconds + FMath::Max(0.0f, DeltaSeconds) : 0.0f;

    if (Ratio >= Settings->DensityCriticalRatio && CriticalHeldSeconds >= Settings->DensityCriticalHoldSeconds)
    {
        DensityState = EFFZSDensityState::Critical;
    }
    else if (Ratio >= Settings->DensityWarningRatio && WarningHeldSeconds >= Settings->DensityWarningHoldSeconds)
    {
        DensityState = EFFZSDensityState::Warning;
    }
    else if (Ratio >= Settings->DensityCautionRatio)
    {
        DensityState = EFFZSDensityState::Caution;
    }
    else
    {
        DensityState = EFFZSDensityState::Normal;
    }
}

FFFZSZoneReadModel AFFZSGameplayZone::GetReadModel() const
{
    FFFZSZoneReadModel Result;
    Result.ZoneId = ZoneId;
    Result.NeighborZoneIds = NeighborZoneIds;
    Result.EntryExitAnchors = EntryExitAnchors;
    Result.AreaSquareMeters = GetBoundaryAreaSquareMeters();
    Result.SoftCapacity = GetSoftCapacity();
    Result.ReportedPopulation = ReportedPopulation;
    Result.DensityRatio = Result.SoftCapacity > 0 ? static_cast<float>(ReportedPopulation) / Result.SoftCapacity : 0.0f;
    Result.DensityState = DensityState;
    return Result;
}

// ZoneId는 Subsystem 맵의 키다. 이미 이 Actor가 현재 키로 등록되어 있으면 Preset 변경을 거부하여
// Actor의 필드와 권위 레지스트리 키가 서로 달라지는 상태를 예방.
bool AFFZSGameplayZone::ApplyAcceptedZonePreset(const FName AcceptedZoneId)
{
    if (!UFFZSGridLibrary::GetAcceptedGameplayZoneIds().Contains(AcceptedZoneId))
    {
        return false;
    }
    if (const UWorld* World = GetWorld())
    {
        if (const UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>())
        {
            // 등록 후 ZoneId를 변경하면 권위 레지스트리 키가 오래된 값으로 남는다.
            if (Subsystem->FindZone(ZoneId) == this)
            {
                return false;
            }
        }
    }
    ZoneId = AcceptedZoneId;
    DisplayName = DisplayNameForZone(AcceptedZoneId);
    SourceReferenceId = FName(*AcceptedZoneId.ToString().Replace(TEXT("GZ_"), TEXT("ZONE_")));
    SchemaVersion = 1;
    ContentVersion = 1;
    return true;
}

// 이 검증은 경계 기하, 프로젝트 ID 정책, Transform, 시설 범주, 이웃 ID의 로컬 계약을 확인한다.
// 이웃 존의 실제 존재와 양방향 연결 같은 월드 간 계약은 Subsystem::ValidateZoneGraph가 담당.
bool AFFZSGameplayZone::ValidateZoneData(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();
    if (ZoneId.IsNone())
    {
        OutErrors.Add(LOCTEXT("MissingZoneId", "ZoneId가 필요합니다."));
    }
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    if (!ZoneId.IsNone() && Settings->bRestrictZoneIdsToProjectPresets &&
        !Settings->ProjectZoneIds.Contains(ZoneId))
    {
        OutErrors.Add(LOCTEXT("UnknownZoneId", "ZoneId가 Project Settings > FF Zone System > Project Zone Ids에 없습니다."));
    }
    if (!BoundarySpline || BoundarySpline->GetNumberOfSplinePoints() < 3 || !BoundarySpline->IsClosedLoop())
    {
        OutErrors.Add(LOCTEXT("InvalidBoundary", "BoundarySpline은 Point가 3개 이상인 닫힌 다각형이어야 합니다."));
    }
    if (BoundarySpline)
    {
        bool bAllLinear = true;
        for (int32 Index = 0; Index < BoundarySpline->GetNumberOfSplinePoints(); ++Index)
        {
            bAllLinear &= BoundarySpline->GetSplinePointType(Index) == ESplinePointType::Linear;
        }
        if (!bAllLinear)
        {
            OutErrors.Add(LOCTEXT("CurvedBoundary", "Gameplay Zone의 모든 BoundarySpline Point는 Linear 보간을 사용해야 합니다."));
        }
        if (!IsSimpleNonDegeneratePolygon(GetBoundaryPolygonLocalCm()))
        {
            OutErrors.Add(LOCTEXT("NonSimpleBoundary", "Gameplay Zone 경계는 자기 교차와 중복 Edge Point가 없는 단순 다각형이어야 합니다."));
        }
    }
    if (!GetActorScale3D().Equals(FVector::OneVector, 0.001f))
    {
        OutErrors.Add(LOCTEXT("ScaledZone", "Gameplay Zone Actor의 Scale은 1이어야 합니다. Actor의 Scale 대신 Spline Point를 편집하십시오."));
    }
    const FVector ActorLocation = GetActorLocation();
    constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const bool bAlignedX = FMath::IsNearlyEqual(ActorLocation.X, FMath::GridSnap(ActorLocation.X, CellSizeCm), 0.1);
    const bool bAlignedY = FMath::IsNearlyEqual(ActorLocation.Y, FMath::GridSnap(ActorLocation.Y, CellSizeCm), 0.1);
    if (!bAlignedX || !bAlignedY)
    {
        OutErrors.Add(LOCTEXT("UnalignedZoneOrigin", "Gameplay Zone의 X/Y 원점은 고정 1cm 배치 Grid에 맞아야 합니다."));
    }

    const FRotator ActorRotation = GetActorRotation();
    const double NormalizedYaw = FRotator::NormalizeAxis(ActorRotation.Yaw);
    const double CardinalYaw = FMath::GridSnap(NormalizedYaw, 90.0);
    if (!FMath::IsNearlyZero(ActorRotation.Pitch, 0.01) || !FMath::IsNearlyZero(ActorRotation.Roll, 0.01) ||
        !FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(NormalizedYaw, CardinalYaw), 0.01))
    {
        OutErrors.Add(LOCTEXT("NonCardinalZone", "Gameplay Zone 회전은 Pitch와 Roll이 0인 상태에서 Yaw 0/90/180/270도만 사용해야 합니다."));
    }

    if (!bAllowAllFacilityCategories && AllowedFacilityCategories.IsEmpty())
    {
        OutErrors.Add(LOCTEXT("NoCategories", "Zone에 허용된 Facility 카테고리가 없습니다."));
    }
    if (NeighborZoneIds.Contains(ZoneId))
    {
        OutErrors.Add(LOCTEXT("SelfNeighbor", "Zone은 자기 자신을 인접 Zone으로 지정할 수 없습니다."));
    }
    TSet<FName> UniqueNeighbors;
    for (const FName NeighborId : NeighborZoneIds)
    {
        if (NeighborId.IsNone() || UniqueNeighbors.Contains(NeighborId))
        {
            OutErrors.Add(LOCTEXT("DuplicateNeighbor", "NeighborZoneIds에는 비어 있거나 중복된 ID가 있을 수 없습니다."));
        }
        UniqueNeighbors.Add(NeighborId);
    }
    return OutErrors.IsEmpty();
}

#undef LOCTEXT_NAMESPACE
