#include "FF_FFZSFacilityEditorHelpers.h"

#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueGeometry.h"
#include "FF_FFZSVisualGeometry.h"

bool FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(
    const FFFZSFacilitySpec& Spec, FFFZSAudienceLayout& OutAudience, TArray<FText>& OutErrors,
    TArray<FText>* OutAdjustments)
{
    OutErrors.Reset();
    if (OutAdjustments) OutAdjustments->Reset();
    const auto Fail = [&](const FText& Error) { OutErrors.Add(Error); return false; };
    const auto Explain = [&](const FText& Message) { if (OutAdjustments) OutAdjustments->Add(Message); };
    if (!Spec.Audience.bEnabled || Spec.Audience.GetCapacity() == 0)
    {
        return Fail(FText::Format(NSLOCTEXT("FFZSAudienceEditor", "RequestedCountLimit",
            "요청: {0}열 × {1}줄 = {2}명. 현재 지원 한도는 8192명, 행·열 각각 1~512입니다. 이 수치를 그대로 쓰려면 시설 정원 한도와 NPC 처리 예산 확장이 필요합니다. 자동으로 인원을 줄이지 않았습니다."),
            FText::AsNumber(Spec.Audience.Columns), FText::AsNumber(Spec.Audience.Rows),
            FText::AsNumber(static_cast<int64>(Spec.Audience.Columns) * Spec.Audience.Rows)));
    }
    if (!UFFZSGridLibrary::HasSafeFacilitySpecCardinality(Spec))
        return Fail(NSLOCTEXT("FFZSAudienceEditor", "OtherCardinality", "일반 Queue·앵커·허용 구역 배열이 시설 안전 한도를 초과했습니다. 관람 인원이 원인이 아니므로 해당 배열을 먼저 수정하세요."));
    FFFZSFacilitySpec Candidate = Spec;
    FFFZSAudienceLayout& A = Candidate.Audience;
    const double Envelope = static_cast<double>(A.AgentRadiusCm) + A.ArrivalToleranceCm;
    double MinScale = 1.;
    if (Spec.bUsesFacilityLevels)
    {
        FText TierError;
        if (!FFZSFacilityUpgradeRules::ValidateTiers(Spec.UpgradeTiers, TierError)) return Fail(TierError);
        for (const FFFZSFacilityUpgradeTier& Tier : Spec.UpgradeTiers)
            MinScale = FMath::Min(MinScale, static_cast<double>(Tier.SizeMultiplier));
    }
    if (!FMath::IsFinite(A.AgentRadiusCm) || A.AgentRadiusCm <= 0.f
        || !FMath::IsFinite(A.ArrivalToleranceCm) || A.ArrivalToleranceCm <= 0.f
        || !FMath::IsFinite(A.ColumnSpacingCm) || !FMath::IsFinite(A.RowSpacingCm)
        || !FMath::IsFinite(A.PerimeterMarginCm) || !FMath::IsFinite(A.CenterAisleWidthCm)
        || A.FirstSlotLocalCm.ContainsNaN() || A.FocusLocalCm.ContainsNaN()
        || Spec.FootprintSizeCells.X <= 0 || Spec.FootprintSizeCells.Y <= 0)
    {
        return Fail(NSLOCTEXT("FFZSAudienceEditor", "Inputs", "NPC 반경·도착 오차·자리 좌표·시설 크기를 먼저 확인하세요."));
    }
    // At the smallest enabled level, leave a little room past the strict seat-envelope edge.
    const double MinimumSpacing = FMath::CeilToDouble((2. * Envelope + 2.) / MinScale);
    if (!FMath::IsFinite(MinimumSpacing) || MinimumSpacing > 1000000.)
        return Fail(NSLOCTEXT("FFZSAudienceEditor", "Scale", "NPC 크기 또는 업그레이드 축소 배율 때문에 안전한 관람 간격을 만들 수 없습니다."));
    A.ColumnSpacingCm = FMath::Max(A.ColumnSpacingCm, static_cast<float>(MinimumSpacing));
    A.RowSpacingCm = FMath::Max(A.RowSpacingCm, static_cast<float>(MinimumSpacing));
    A.PerimeterMarginCm = FMath::Max(A.PerimeterMarginCm, static_cast<float>(MinimumSpacing));
    A.CenterAisleWidthCm = FMath::Max(A.CenterAisleWidthCm, 0.f);
    const auto ExplainNumber = [&](const FText& Name, const double Before, const double After, const FText& Reason)
    {
        if (Before != After) Explain(FText::Format(NSLOCTEXT("FFZSAudienceEditor", "NumberRepair", "{0}: {1} → {2}cm. 이유: {3}"),
            Name, FText::AsNumber(Before), FText::AsNumber(After), Reason));
    };
    const FText SpacingReason = NSLOCTEXT("FFZSAudienceEditor", "SpacingReason", "NPC 반경·도착 오차와 가장 작은 사용 레벨의 배율을 고려해 겹치지 않는 간격을 확보했습니다.");
    ExplainNumber(NSLOCTEXT("FFZSAudienceEditor", "ColumnSpacing", "좌우 자리 간격"), Spec.Audience.ColumnSpacingCm, A.ColumnSpacingCm, SpacingReason);
    ExplainNumber(NSLOCTEXT("FFZSAudienceEditor", "RowSpacing", "앞뒤 자리 간격"), Spec.Audience.RowSpacingCm, A.RowSpacingCm, SpacingReason);
    ExplainNumber(NSLOCTEXT("FFZSAudienceEditor", "Perimeter", "바깥 여백"), Spec.Audience.PerimeterMarginCm, A.PerimeterMarginCm, SpacingReason);
    ExplainNumber(NSLOCTEXT("FFZSAudienceEditor", "Aisle", "추가 중앙 통로"), Spec.Audience.CenterAisleWidthCm, A.CenterAisleWidthCm,
        NSLOCTEXT("FFZSAudienceEditor", "NegativeAisle", "통로 폭에 음수를 사용할 수 없어 0으로 보정했습니다."));
    A.FirstSlotLocalCm.Y = FMath::Max(A.FirstSlotLocalCm.Y,
        FMath::Max(Spec.FootprintSizeCells.Y * .5 + A.PerimeterMarginCm + 2.,
            A.FocusLocalCm.Y + Envelope / MinScale + 2.));
    if (!A.TryCenterAndArrangePortals())
    {
        A.IsValid(OutErrors);
        if (OutErrors.IsEmpty()) return Fail(NSLOCTEXT("FFZSAudienceEditor", "Layout", "관람 배치를 계산할 수 없습니다. 좌표·정원·면적 한도를 확인하세요."));
        return false;
    }
    TArray<FFFZSCellRect> QueueRects = Spec.QueueReservedRects;
    for (const FIntPoint& Cell : Spec.QueueReservedCells)
    {
        if (Cell.X == MAX_int32 || Cell.Y == MAX_int32)
            return Fail(NSLOCTEXT("FFZSAudienceEditor", "LegacyCell", "기존 Queue 좌표 범위를 먼저 수정하세요."));
        QueueRects.Emplace(Cell, FIntPoint(Cell.X + 1, Cell.Y + 1));
    }
    // Moving only toward +Y strictly clears at least one rectangle per pass. No cell rasterization.
    for (int32 Pass = 0; Pass <= QueueRects.Num(); ++Pass)
    {
        FFFZSCellRect Bounds;
        if (!UFFZSGridLibrary::TryGetAudienceReservedRect(Candidate, Bounds))
            return Fail(NSLOCTEXT("FFZSAudienceEditor", "Bounds", "관람 좌표가 지원 범위를 초과했습니다."));
        const FFFZSCellRect* Overlap = QueueRects.FindByPredicate(
            [&](const FFFZSCellRect& Rect) { return Rect.IsValid() && Rect.Intersects(Bounds); });
        if (!Overlap) break;
        A.FirstSlotLocalCm.Y = Overlap->Max.Y - Spec.FootprintSizeCells.Y * .5 + A.PerimeterMarginCm + 2.;
        if (!A.TryCenterAndArrangePortals())
            return Fail(NSLOCTEXT("FFZSAudienceEditor", "QueueClearance", "일반 Queue를 피할 수 있는 관람 공간을 만들 수 없습니다."));
    }
    ExplainNumber(NSLOCTEXT("FFZSAudienceEditor", "FirstX", "첫 자리 X"), Spec.Audience.FirstSlotLocalCm.X, A.FirstSlotLocalCm.X,
        NSLOCTEXT("FFZSAudienceEditor", "CenterReason", "입력한 열 수에 맞춰 관람 자리를 무대 중앙으로 정렬했습니다."));
    ExplainNumber(NSLOCTEXT("FFZSAudienceEditor", "FirstY", "첫 자리 Y"), Spec.Audience.FirstSlotLocalCm.Y, A.FirstSlotLocalCm.Y,
        NSLOCTEXT("FFZSAudienceEditor", "ClearanceReason", "무대·일반 Queue와 겹치지 않고 무대를 앞에 바라보도록 뒤로 옮겼습니다."));
    const auto ExplainPortal = [&](const FText& Name, const FVector& Before, const FVector& After)
    {
        if (!Before.Equals(After)) Explain(FText::Format(NSLOCTEXT("FFZSAudienceEditor", "PortalRepair", "{0}: {1} → {2}cm. 이유: 변경된 자리 배열 뒤쪽의 통로로 맞췄습니다."),
            Name, FText::FromString(Before.ToCompactString()), FText::FromString(After.ToCompactString())));
    };
    ExplainPortal(NSLOCTEXT("FFZSAudienceEditor", "Entry", "관람 입구"), Spec.Audience.EntryLocalCm, A.EntryLocalCm);
    ExplainPortal(NSLOCTEXT("FFZSAudienceEditor", "Exit", "관람 출구"), Spec.Audience.ExitLocalCm, A.ExitLocalCm);

    // Calculate real, outward-quantized reservation sizes rather than estimating count × spacing.
    // A single base allowance must cover the largest enabled upgrade as well.
    int64 RequiredAreaCm2 = 0;
    bool bNeedsSeparateBudget = false;
    const auto InspectLevel = [&](const FFFZSFacilitySpec& Effective, const int32 Level)
    {
        FFFZSCellRect Bounds;
        if (!UFFZSGridLibrary::TryGetAudienceReservedRect(Effective, Bounds))
            return Fail(FText::Format(NSLOCTEXT("FFZSAudienceEditor", "LevelBounds", "Lv.{0}의 관람 좌표가 지원 범위를 벗어납니다. 자리 간격·여백 또는 업그레이드 배율을 줄여야 합니다."), FText::AsNumber(Level)));
        RequiredAreaCm2 = FMath::Max(RequiredAreaCm2, Bounds.GetArea());
        FFFZSFacilitySpec BodyOnly = Effective;
        BodyOnly.Audience.bEnabled = false;
        if (!UFFZSGridLibrary::CanEnumerateFacilityCells(BodyOnly))
            return Fail(FText::Format(NSLOCTEXT("FFZSAudienceEditor", "BodyBudget", "Lv.{0}의 무대 본체·여유 공간·일반 Queue가 공통 면적 한도를 넘었거나 잘못되었습니다. 관람 면적 확대만으로 해결할 수 없으므로 해당 시설 값을 수정하세요."), FText::AsNumber(Level)));
        bNeedsSeparateBudget |= !UFFZSGridLibrary::CanEnumerateFacilityCells(Effective);
        return true;
    };
    if (!InspectLevel(Candidate, 1)) return false;
    if (Candidate.bUsesFacilityLevels)
    {
        FFFZSFacilitySpec GeometryBase = Candidate;
        GeometryBase.bUsesFacilityLevels = false;
        GeometryBase.UpgradeTiers.Reset();
        for (const FFFZSFacilityUpgradeTier& Tier : Candidate.UpgradeTiers)
            if (!InspectLevel(FFZSFacilityUpgradeRules::BuildEffectiveSpec(GeometryBase, Tier.TargetLevel, Tier.SizeMultiplier), Tier.TargetLevel)) return false;
    }
    if (bNeedsSeparateBudget)
    {
        const double RequiredM2 = FMath::CeilToDouble(RequiredAreaCm2 / 10000.);
        if (RequiredM2 > FFFZSAudienceLayout::MaxAreaLimitM2)
            return Fail(FText::Format(NSLOCTEXT("FFZSAudienceEditor", "AreaHardLimit", "요청 인원의 현재 배치에는 최소 {0}m²가 필요하지만 관람 면적 지원 한도는 {1}m²입니다. 행·열은 보관했습니다. 자리 간격·바깥 여백·중앙 통로 또는 확대 레벨 배율을 줄인 뒤 다시 적용하세요."),
                FText::AsNumber(RequiredM2), FText::AsNumber(FFFZSAudienceLayout::MaxAreaLimitM2)));
        A.AreaLimitM2 = static_cast<float>(RequiredM2);
        Explain(FText::Format(NSLOCTEXT("FFZSAudienceEditor", "AreaRepair", "Audience Area Limit: {0} → {1}m². 이유: 기존 한도로는 요청 인원의 관람 영역을 수용할 수 없어 모든 사용 레벨에 필요한 최소 면적으로 늘렸습니다. 0은 기존 공통 한도입니다."),
            FText::AsNumber(Spec.Audience.AreaLimitM2), FText::AsNumber(A.AreaLimitM2)));
    }
    if (!Candidate.IsValid(OutErrors)) return false;
    OutAudience = MoveTemp(A);
    return true;
}

namespace FFZSFacilityEditorHelpersPrivate
{
    int32 CeilToPlacementCell(const double SizeCm)
    {
        // Placement bounds are authoritative. Never round a genuinely oversized mesh
        // down: even a sub-millimetre overhang must reserve the next cell.
        constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
        return FMath::Max(1, static_cast<int32>(FMath::CeilToDouble(SizeCm / CellSizeCm)));
    }

    FBox TransformCardinalBoundsExact(
        const FBox& SourceBounds,
        const FRotator& Rotation,
        const FVector& Offset)
    {
        FBox Result(EForceInit::ForceInit);
        const int32 QuarterTurns = ((FMath::RoundToInt(
            FRotator::NormalizeAxis(Rotation.Yaw) / 90.0) % 4) + 4) % 4;
        for (int32 XIndex = 0; XIndex < 2; ++XIndex)
        {
            for (int32 YIndex = 0; YIndex < 2; ++YIndex)
            {
                for (int32 ZIndex = 0; ZIndex < 2; ++ZIndex)
                {
                    const double X = XIndex == 0 ? SourceBounds.Min.X : SourceBounds.Max.X;
                    const double Y = YIndex == 0 ? SourceBounds.Min.Y : SourceBounds.Max.Y;
                    const double Z = ZIndex == 0 ? SourceBounds.Min.Z : SourceBounds.Max.Z;
                    FVector Rotated;
                    switch (QuarterTurns)
                    {
                    case 1: Rotated = FVector(-Y, X, Z); break;
                    case 2: Rotated = FVector(-X, -Y, Z); break;
                    case 3: Rotated = FVector(Y, -X, Z); break;
                    default: Rotated = FVector(X, Y, Z); break;
                    }
                    Result += Rotated + Offset;
                }
            }
        }
        return Result;
    }

    // 예약 사각형은 회전 전 건물 좌하단을 원점으로 하는 grid 좌표이고, 건물은 중심을 기준으로
    // 커진다. 따라서 중심보다 큰 쪽 경계는 크기 변화량 전체를, 작은 쪽 경계는 0을 이동해야
    // 각 경계에서 건물 변까지의 실제 거리가 유지된다. 정확히 중심선 위에 있던 경계는 새 중심선으로
    // 옮긴다. 모든 계산을 int64로 수행해 큰 저작 값에서도 결과 범위를 먼저 확인한다.
    bool TryShiftReservationCoordinate(
        const int32 Coordinate,
        const int32 OldSizeCells,
        const int32 NewSizeCells,
        int32& OutCoordinate)
    {
        const int64 DoubledCenterOffset =
            2LL * static_cast<int64>(Coordinate) - static_cast<int64>(OldSizeCells);
        int64 Shifted = static_cast<int64>(Coordinate);
        if (DoubledCenterOffset > 0)
        {
            Shifted += static_cast<int64>(NewSizeCells) - static_cast<int64>(OldSizeCells);
        }
        else if (DoubledCenterOffset == 0)
        {
            Shifted = static_cast<int64>(FMath::DivideAndRoundNearest(NewSizeCells, 2));
        }

        if (Shifted < MIN_int32 || Shifted > MAX_int32)
        {
            return false;
        }
        OutCoordinate = static_cast<int32>(Shifted);
        return true;
    }

    // Queue Spline Point와 Anchor는 건물 중심 기준 cm 좌표다. 같은 "변까지의 거리 유지" 규칙이
    // 중심 기준에서는 크기 변화량의 절반이 되며, 중심선 위의 값은 그대로 둔다.
    double ShiftCenteredCoordinateCm(
        const double CoordinateCm,
        const int32 OldSizeCells,
        const int32 NewSizeCells)
    {
        constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
        const double HalfDeltaCm =
            0.5 * (static_cast<double>(NewSizeCells) - static_cast<double>(OldSizeCells)) * CellSizeCm;
        if (CoordinateCm > 0.0)
        {
            return CoordinateCm + HalfDeltaCm;
        }
        if (CoordinateCm < 0.0)
        {
            return CoordinateCm - HalfDeltaCm;
        }
        return CoordinateCm;
    }

    // 한 경로 구간을 감싸는 예약 사각형을 만든다. 좌표는 건물 좌하단 기준 grid로 변환한 뒤
    // 여백만큼 넓히고 바깥쪽으로 올림해, 경계 허용치가 사실상 0인 슬롯 판정에서도 모든 슬롯이
    // 확실히 안쪽에 남게 한다. 구간이 건물 한 변의 바깥에 온전히 있으면 그 변에 딱 붙여
    // 잘라내므로, 건물 옆을 지나는 회랑이 본체와 겹쳐 검증에 걸리지 않는다.
    bool TryBuildSegmentRect(
        const FVector& StartCm,
        const FVector& EndCm,
        const FIntPoint FootprintSizeCells,
        const double MarginCells,
        FFFZSCellRect& OutRect)
    {
        const FVector2D StartGrid =
            FFFZSQueueGeometry::LocalPointCmToReservationGrid(StartCm, FootprintSizeCells);
        const FVector2D EndGrid =
            FFFZSQueueGeometry::LocalPointCmToReservationGrid(EndCm, FootprintSizeCells);
        const FVector2D RawMin(
            FMath::Min(StartGrid.X, EndGrid.X), FMath::Min(StartGrid.Y, EndGrid.Y));
        const FVector2D RawMax(
            FMath::Max(StartGrid.X, EndGrid.X), FMath::Max(StartGrid.Y, EndGrid.Y));

        const double MinX = FMath::FloorToDouble(RawMin.X - MarginCells);
        const double MinY = FMath::FloorToDouble(RawMin.Y - MarginCells);
        const double MaxX = FMath::CeilToDouble(RawMax.X + MarginCells);
        const double MaxY = FMath::CeilToDouble(RawMax.Y + MarginCells);
        if (!FMath::IsFinite(MinX) || !FMath::IsFinite(MinY) ||
            !FMath::IsFinite(MaxX) || !FMath::IsFinite(MaxY) ||
            MinX < MIN_int32 || MinY < MIN_int32 || MaxX > MAX_int32 || MaxY > MAX_int32)
        {
            return false;
        }

        FIntPoint Min(static_cast<int32>(MinX), static_cast<int32>(MinY));
        FIntPoint Max(static_cast<int32>(MaxX), static_cast<int32>(MaxY));
        if (RawMin.X >= FootprintSizeCells.X)
        {
            Min.X = FMath::Max(Min.X, FootprintSizeCells.X);
        }
        if (RawMax.X <= 0.0)
        {
            Max.X = FMath::Min(Max.X, 0);
        }
        if (RawMin.Y >= FootprintSizeCells.Y)
        {
            Min.Y = FMath::Max(Min.Y, FootprintSizeCells.Y);
        }
        if (RawMax.Y <= 0.0)
        {
            Max.Y = FMath::Min(Max.Y, 0);
        }

        OutRect = FFFZSCellRect(Min, Max);
        return OutRect.IsValid();
    }

    bool IsFinitePolyline(const TArray<FVector>& Points)
    {
        return Points.Num() >= 2 && !Points.ContainsByPredicate(
            [](const FVector& Point)
            {
                return Point.ContainsNaN();
            });
    }
}

bool FFFZSFacilityEditorHelpers::TryCalculateFootprintCells(
    const FVector& MeshBoundsSizeCm,
    FIntPoint& OutFootprintSizeCells)
{
    OutFootprintSizeCells = FIntPoint::ZeroValue;

    if (MeshBoundsSizeCm.ContainsNaN() ||
        MeshBoundsSizeCm.X <= 0.0 ||
        MeshBoundsSizeCm.Y <= 0.0 ||
        MeshBoundsSizeCm.X > static_cast<double>(MAX_int32) ||
        MeshBoundsSizeCm.Y > static_cast<double>(MAX_int32))
    {
        return false;
    }

    OutFootprintSizeCells.X = FFZSFacilityEditorHelpersPrivate::CeilToPlacementCell(MeshBoundsSizeCm.X);
    OutFootprintSizeCells.Y = FFZSFacilityEditorHelpersPrivate::CeilToPlacementCell(MeshBoundsSizeCm.Y);
    return true;
}

bool FFFZSFacilityEditorHelpers::TryCalculateFootprintCellsFromSourceBounds(
    const FBox& SourceBounds,
    const FRotator& VisualRotation,
    const FVector& VisualOffsetCm,
    FIntPoint& OutFootprintSizeCells)
{
    OutFootprintSizeCells = FIntPoint::ZeroValue;
    if (!SourceBounds.IsValid ||
        SourceBounds.Min.ContainsNaN() ||
        SourceBounds.Max.ContainsNaN() ||
        VisualOffsetCm.ContainsNaN() ||
        !FMath::IsFinite(VisualRotation.Pitch) ||
        !FMath::IsFinite(VisualRotation.Yaw) ||
        !FMath::IsFinite(VisualRotation.Roll))
    {
        return false;
    }

    const FBox AuthoredBounds = FFZSVisualGeometry::IsCardinalYawRotation(VisualRotation)
        ? FFZSFacilityEditorHelpersPrivate::TransformCardinalBoundsExact(
            SourceBounds, VisualRotation, VisualOffsetCm)
        : FFZSVisualGeometry::TransformBounds(
            SourceBounds,
            FTransform(VisualRotation, VisualOffsetCm, FVector::OneVector));
    if (!AuthoredBounds.IsValid || AuthoredBounds.Min.ContainsNaN() || AuthoredBounds.Max.ContainsNaN())
    {
        return false;
    }

    const FVector CenteredSizeCm(
        2.0 * FMath::Max(FMath::Abs(AuthoredBounds.Min.X), FMath::Abs(AuthoredBounds.Max.X)),
        2.0 * FMath::Max(FMath::Abs(AuthoredBounds.Min.Y), FMath::Abs(AuthoredBounds.Max.Y)),
        AuthoredBounds.GetSize().Z);
    return TryCalculateFootprintCells(CenteredSizeCm, OutFootprintSizeCells);
}

bool FFFZSFacilityEditorHelpers::TryResizeFootprintKeepingLayout(
    const FFFZSFacilitySpec& Spec,
    const FIntPoint NewFootprintSizeCells,
    FFFZSFacilitySpec& OutSpec)
{
    using namespace FFZSFacilityEditorHelpersPrivate;

    OutSpec = FFFZSFacilitySpec{};
    // 구 1cm Cell 목록은 좌표 하나가 곧 한 Cell이라 사각형 규칙을 그대로 적용할 수 없다.
    // 잘못 보정하느니 자동 보정을 거부하고 담당자가 QueueReservedRects로 변환하게 한다.
    if (NewFootprintSizeCells.X <= 0 || NewFootprintSizeCells.Y <= 0 ||
        Spec.FootprintSizeCells.X <= 0 || Spec.FootprintSizeCells.Y <= 0 ||
        !Spec.QueueReservedCells.IsEmpty())
    {
        return false;
    }

    FFFZSFacilitySpec Candidate = Spec;
    Candidate.FootprintSizeCells = NewFootprintSizeCells;

    for (FFFZSCellRect& Rect : Candidate.QueueReservedRects)
    {
        if (!TryShiftReservationCoordinate(
                Rect.Min.X, Spec.FootprintSizeCells.X, NewFootprintSizeCells.X, Rect.Min.X) ||
            !TryShiftReservationCoordinate(
                Rect.Max.X, Spec.FootprintSizeCells.X, NewFootprintSizeCells.X, Rect.Max.X) ||
            !TryShiftReservationCoordinate(
                Rect.Min.Y, Spec.FootprintSizeCells.Y, NewFootprintSizeCells.Y, Rect.Min.Y) ||
            !TryShiftReservationCoordinate(
                Rect.Max.Y, Spec.FootprintSizeCells.Y, NewFootprintSizeCells.Y, Rect.Max.Y))
        {
            return false;
        }
    }

    for (FVector& PointCm : Candidate.QueueSplinePointsCm)
    {
        if (PointCm.ContainsNaN())
        {
            return false;
        }
        PointCm.X = ShiftCenteredCoordinateCm(
            PointCm.X, Spec.FootprintSizeCells.X, NewFootprintSizeCells.X);
        PointCm.Y = ShiftCenteredCoordinateCm(
            PointCm.Y, Spec.FootprintSizeCells.Y, NewFootprintSizeCells.Y);
    }

    for (FFFZSAnchorDefinition& Anchor : Candidate.Anchors)
    {
        FVector LocationCm = Anchor.LocalTransform.GetTranslation();
        if (LocationCm.ContainsNaN())
        {
            return false;
        }
        LocationCm.X = ShiftCenteredCoordinateCm(
            LocationCm.X, Spec.FootprintSizeCells.X, NewFootprintSizeCells.X);
        LocationCm.Y = ShiftCenteredCoordinateCm(
            LocationCm.Y, Spec.FootprintSizeCells.Y, NewFootprintSizeCells.Y);
        Anchor.LocalTransform.SetTranslation(LocationCm);
    }

    if (Candidate.Audience.bEnabled)
    {
        // Audience rows grow toward +Y. Keep the entire grid and its portals together,
        // at the same distance from the stage front; do not stretch the people spacing.
        const FVector Shift(0., (static_cast<double>(NewFootprintSizeCells.Y) - Spec.FootprintSizeCells.Y) * .5, 0.);
        Candidate.Audience.FirstSlotLocalCm += Shift;
        Candidate.Audience.EntryLocalCm += Shift;
        Candidate.Audience.ExitLocalCm += Shift;
        Candidate.Audience.FocusLocalCm.X = ShiftCenteredCoordinateCm(
            Candidate.Audience.FocusLocalCm.X, Spec.FootprintSizeCells.X, NewFootprintSizeCells.X);
        Candidate.Audience.FocusLocalCm.Y = ShiftCenteredCoordinateCm(
            Candidate.Audience.FocusLocalCm.Y, Spec.FootprintSizeCells.Y, NewFootprintSizeCells.Y);
    }
    OutSpec = MoveTemp(Candidate);
    return true;
}

bool FFFZSFacilityEditorHelpers::TryBuildQueueRectsFromPath(
    const FFFZSFacilitySpec& Spec,
    const int32 MarginCm,
    TArray<FFFZSCellRect>& OutRects)
{
    using namespace FFZSFacilityEditorHelpersPrivate;

    OutRects.Reset();
    // 여백이 0이면 직선 구간의 사각형 폭이 0이 되어 무효 예약이 된다.
    // 구 1cm Cell 목록이 남아 있으면 예약 합집합의 원천이 둘이 되므로 여기서 다루지 않는다.
    if (MarginCm <= 0 ||
        Spec.FootprintSizeCells.X <= 0 || Spec.FootprintSizeCells.Y <= 0 ||
        !IsFinitePolyline(Spec.QueueSplinePointsCm) ||
        !Spec.QueueReservedCells.IsEmpty())
    {
        return false;
    }

    double RequiredLengthCm = 0.0;
    TArray<FVector> CoveredPathCm;
    // 최대 정원이 실제로 쓰는 구간까지만 덮는다. 저작 경로가 더 길면 남는 꼬리는 예약하지 않아
    // 확보 면적이 최소가 되고, 더 짧으면 부족한 길이가 그대로 드러나 Spec 검증에서 걸린다.
    if (!TryCalculateRequiredQueueLengthCm(
            Spec.QueueRules.MaxWaitingAgents,
            Spec.QueueRules.SlotSpacingCm,
            RequiredLengthCm) ||
        !TryResizeQueueTail(Spec.QueueSplinePointsCm, RequiredLengthCm, CoveredPathCm) ||
        CoveredPathCm.Num() < 2)
    {
        return false;
    }

    constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const double MarginCells = static_cast<double>(MarginCm) / CellSizeCm;
    TArray<FFFZSCellRect> SegmentRects;
    SegmentRects.Reserve(CoveredPathCm.Num() - 1);
    for (int32 Index = 1; Index < CoveredPathCm.Num(); ++Index)
    {
        FFFZSCellRect SegmentRect;
        if (!TryBuildSegmentRect(
                CoveredPathCm[Index - 1],
                CoveredPathCm[Index],
                Spec.FootprintSizeCells,
                MarginCells,
                SegmentRect))
        {
            return false;
        }
        SegmentRects.Add(SegmentRect);
    }

    // 직선으로 이어진 구간들은 하나로 합쳐지고 꺾인 구간만 남아 회랑 모양이 유지된다.
    TArray<FFFZSCellRect> Canonical = UFFZSGridLibrary::CanonicalizeCellRects(SegmentRects);
    if (Canonical.IsEmpty() || Canonical.Num() > UFFZSDeveloperSettings::MaxQueueReservedRects)
    {
        return false;
    }

    OutRects = MoveTemp(Canonical);
    return true;
}

bool FFFZSFacilityEditorHelpers::TryCalculateRequiredQueueLengthCm(
    const int32 Capacity,
    const double SlotSpacingCm,
    double& OutRequiredLengthCm)
{
    OutRequiredLengthCm = 0.0;
    if (Capacity <= 0 || !FMath::IsFinite(SlotSpacingCm) || SlotSpacingCm <= 0.0)
    {
        return false;
    }

    OutRequiredLengthCm = static_cast<double>(Capacity - 1) * SlotSpacingCm;
    return FMath::IsFinite(OutRequiredLengthCm);
}

bool FFFZSFacilityEditorHelpers::TryCalculateMaxWaitingAgents(
    const TArray<FVector>& QueuePointsCm,
    const double SlotSpacingCm,
    int32& OutMaxWaitingAgents)
{
    OutMaxWaitingAgents = 0;
    if (!FFZSFacilityEditorHelpersPrivate::IsFinitePolyline(QueuePointsCm) ||
        !FMath::IsFinite(SlotSpacingCm) ||
        SlotSpacingCm <= 0.0)
    {
        return false;
    }

    int32 FirstNonFinitePointIndex = INDEX_NONE;
    const double LengthCm = FFFZSQueueGeometry::GetLinearPolylineLengthCm(
        QueuePointsCm,
        &FirstNonFinitePointIndex);
    if (FirstNonFinitePointIndex != INDEX_NONE || !FMath::IsFinite(LengthCm))
    {
        return false;
    }

    const double Capacity = FMath::FloorToDouble(LengthCm / SlotSpacingCm) + 1.0;
    if (!FMath::IsFinite(Capacity) || Capacity < 1.0 || Capacity > static_cast<double>(MAX_int32))
    {
        return false;
    }

    OutMaxWaitingAgents = static_cast<int32>(Capacity);
    return true;
}

bool FFFZSFacilityEditorHelpers::TryResizeQueueTail(
    const TArray<FVector>& QueuePointsCm,
    const double RequiredLengthCm,
    TArray<FVector>& OutQueuePointsCm)
{
    using namespace FFZSFacilityEditorHelpersPrivate;

    OutQueuePointsCm.Reset();
    if (!IsFinitePolyline(QueuePointsCm) ||
        !FMath::IsFinite(RequiredLengthCm) ||
        RequiredLengthCm < 0.0)
    {
        return false;
    }

    const double CurrentLengthCm = FFFZSQueueGeometry::GetLinearPolylineLengthCm(QueuePointsCm);
    if (!FMath::IsFinite(CurrentLengthCm))
    {
        return false;
    }

    if (FMath::Abs(CurrentLengthCm - RequiredLengthCm) <= UE_DOUBLE_KINDA_SMALL_NUMBER)
    {
        OutQueuePointsCm = QueuePointsCm;
        return true;
    }

    if (RequiredLengthCm < CurrentLengthCm)
    {
        OutQueuePointsCm.Reserve(QueuePointsCm.Num());
        OutQueuePointsCm.Add(QueuePointsCm[0]);

        if (RequiredLengthCm <= UE_DOUBLE_KINDA_SMALL_NUMBER)
        {
            OutQueuePointsCm.Add(QueuePointsCm[0]);
            return true;
        }

        double RemainingLengthCm = RequiredLengthCm;
        for (int32 Index = 1; Index < QueuePointsCm.Num(); ++Index)
        {
            const FVector& SegmentStart = QueuePointsCm[Index - 1];
            const FVector& SegmentEnd = QueuePointsCm[Index];
            const double SegmentLengthCm = FVector::Distance(SegmentStart, SegmentEnd);
            if (!FFFZSQueueGeometry::IsUsableLinearSegmentLengthCm(SegmentLengthCm))
            {
                OutQueuePointsCm.Add(SegmentEnd);
                continue;
            }

            if (RemainingLengthCm > SegmentLengthCm)
            {
                OutQueuePointsCm.Add(SegmentEnd);
                RemainingLengthCm -= SegmentLengthCm;
                continue;
            }

            const double SegmentAlpha = FMath::Clamp(RemainingLengthCm / SegmentLengthCm, 0.0, 1.0);
            const FVector NewTail = FMath::Lerp(SegmentStart, SegmentEnd, SegmentAlpha);
            if (NewTail != OutQueuePointsCm.Last())
            {
                OutQueuePointsCm.Add(NewTail);
            }
            else if (OutQueuePointsCm.Num() == 1)
            {
                OutQueuePointsCm.Add(NewTail);
            }
            return OutQueuePointsCm.Num() >= 2;
        }

        OutQueuePointsCm.Reset();
        return false;
    }

    FVector ExtensionDirection = FVector::ZeroVector;
    for (int32 Index = QueuePointsCm.Num() - 1; Index > 0; --Index)
    {
        FVector Segment = QueuePointsCm[Index] - QueuePointsCm[Index - 1];
        const double SegmentLengthCm = Segment.Length();
        if (FFFZSQueueGeometry::IsUsableLinearSegmentLengthCm(SegmentLengthCm))
        {
            ExtensionDirection = Segment / SegmentLengthCm;
            break;
        }
    }

    if (ExtensionDirection.IsNearlyZero())
    {
        return false;
    }

    OutQueuePointsCm = QueuePointsCm;
    OutQueuePointsCm.Add(
        QueuePointsCm.Last() + ExtensionDirection * (RequiredLengthCm - CurrentLengthCm));
    return true;
}
