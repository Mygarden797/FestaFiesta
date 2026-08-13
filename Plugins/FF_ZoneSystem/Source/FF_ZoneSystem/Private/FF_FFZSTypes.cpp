// FF_FFZSTypes.cpp

// FF_ZoneSystem 값 타입의 유효성 검사, 경계 정규화, 실패 누적 함수를 구현한다.
// 흐름: 각 Spec과 Snapshot 입력을 싼 구조 검사부터 순서대로 검증한다.
// 불변식: 검증 함수는 원본을 변경하지 않고 모든 실패를 설명 가능한 결과에 누적한다.
// 실패/성능: 무효 크기와 열거 예산을 일찍 차단해 뒤 단계의 기하·정책 비용을 피한다.

#include "FF_FFZSTypes.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueGeometry.h"
#include "FF_FFZSVisualGeometry.h"

#define LOCTEXT_NAMESPACE "FFZSTypes"

// 검증 순서
    // 1) 식별자/Enum/크기 같은 값싼 기본 조건
    // 2) Cell 열거 예산과 Queue 수치
    // 3) Spline/압축 예약 영역의 공간 관계
    // 4) Anchor 인덱스와 외부 Zone/높이 계약
    // 오류를 가능한 한 모아 저작자에게 한 번에 보여 주되, 위험한 반복문은 선행 조건이 모두
    // 유효할 때만 실행한다. 따라서 잘못된 대형 입력도 실제 Cell/Slot 열거 전에 실패한다.
 
bool FFFZSFacilitySpec::IsValid(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();

    if (SchemaVersion != CurrentSchemaVersion)
    {
        OutErrors.Add(FText::Format(
            LOCTEXT("InvalidSchema", "FacilitySpec은 SchemaVersion {0}(1 Cell = 1cm)만 지원합니다. 이전 데이터는 명시적으로 다시 작성해야 합니다."),
            FText::AsNumber(CurrentSchemaVersion)));
    }
    if (FacilityDefinitionId.IsNone())
    {
        OutErrors.Add(LOCTEXT("MissingDefinitionId", "FacilityDefinitionId가 필요합니다."));
    }
    if (DisplayName.IsEmpty())
    {
        OutErrors.Add(LOCTEXT("MissingDisplayName", "Facility DisplayName이 필요합니다."));
    }
    if (!StaticEnum<EFFZSFacilityCategory>()->IsValidEnumValue(static_cast<int64>(Category)))
    {
        OutErrors.Add(LOCTEXT("InvalidCategory", "Facility Category가 잘못되었습니다."));
    }
    if (FootprintSizeCells.X <= 0 || FootprintSizeCells.Y <= 0)
    {
        OutErrors.Add(LOCTEXT("InvalidFootprint", "Footprint의 두 크기는 모두 최소 1 Cell이어야 합니다."));
    }
    if (ClearanceCells < 0)
    {
        OutErrors.Add(LOCTEXT("InvalidClearance", "ClearanceCells는 음수일 수 없습니다."));
    }
    // 양수/비음수 조건을 먼저 통과한 경우에만 예산 계산을 호출해 오류 중복과 오버플로를 피한다.
    if (FootprintSizeCells.X > 0 && FootprintSizeCells.Y > 0 && ClearanceCells >= 0 &&
        !UFFZSGridLibrary::CanEnumerateFacilityCells(*this))
    {
        OutErrors.Add(FText::Format(
            LOCTEXT(
                "FacilityCellEnumerationLimit",
                "Footprint, 여유 공간 및 Queue 예약 영역은 최대 {0}개의 1cm Cell만 표현할 수 있습니다."),
            FText::AsNumber(UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility())));
    }

    if (QueueRules.MaxWaitingAgents <= 0 || QueueRules.ServiceCapacity <= 0 ||
        !FMath::IsFinite(QueueRules.SlotSpacingCm) || QueueRules.SlotSpacingCm <= 0.0f ||
        !FMath::IsFinite(QueueRules.ServiceDurationSeconds) || QueueRules.ServiceDurationSeconds <= 0.0f)
    {
        OutErrors.Add(LOCTEXT("InvalidQueueRules", "Queue 수용량, 간격 및 Service 시간은 유한한 양수여야 합니다."));
    }
    // [하드코딩 제거] 안내 문구에 한계값을 직접 적지 않고 실제 상수를 넣어 만든다.
    // 예전에는 상수를 조정해도 담당자에게는 512/32/64라는 옛 숫자가 계속 표시됐다.
    if (QueueRules.MaxWaitingAgents > UFFZSDeveloperSettings::MaxQueueWaitingAgents)
    {
        OutErrors.Add(FText::Format(
            LOCTEXT("QueueCapacitySafetyLimit", "MaxWaitingAgents가 시설당 대기열 안전 제한 {0}을 초과했습니다."),
            FText::AsNumber(UFFZSDeveloperSettings::MaxQueueWaitingAgents)));
    }
    if (QueueRules.ServiceCapacity > UFFZSDeveloperSettings::MaxFacilityServiceCapacity)
    {
        OutErrors.Add(FText::Format(
            LOCTEXT("ServiceCapacitySafetyLimit", "ServiceCapacity가 시설당 서비스 안전 제한 {0}을 초과했습니다."),
            FText::AsNumber(UFFZSDeveloperSettings::MaxFacilityServiceCapacity)));
    }
    if (QueueReservedRects.Num() > UFFZSDeveloperSettings::MaxQueueReservedRects)
    {
        OutErrors.Add(FText::Format(
            LOCTEXT("QueueRectCountSafetyLimit", "QueueReservedRects는 최대 {0}개를 허용합니다."),
            FText::AsNumber(UFFZSDeveloperSettings::MaxQueueReservedRects)));
    }
    if (QueueReservedCells.Num() > UFFZSDeveloperSettings::MaxLegacyQueueReservedCells)
    {
        OutErrors.Add(LOCTEXT(
            "LegacyQueueCellCountSafetyLimit",
            "Legacy QueueReservedCells exceeds the supported migration safety limit. Convert it to QueueReservedRects."));
    }
    if (QueueSplinePointsCm.Num() > UFFZSDeveloperSettings::MaxQueueSplinePoints)
    {
        OutErrors.Add(FText::Format(
            LOCTEXT("QueueSplinePointCountSafetyLimit", "QueueSplinePointsCm은 최대 {0}개를 허용합니다."),
            FText::AsNumber(UFFZSDeveloperSettings::MaxQueueSplinePoints)));
    }
    if (Anchors.Num() > UFFZSDeveloperSettings::MaxFacilityAnchors)
    {
        OutErrors.Add(FText::Format(
            LOCTEXT("AnchorCountSafetyLimit", "Anchors는 최대 {0}개를 허용합니다."),
            FText::AsNumber(UFFZSDeveloperSettings::MaxFacilityAnchors)));
    }
    if (AllowedZoneIds.Num() > UFFZSDeveloperSettings::MaxAllowedZoneIds)
    {
        OutErrors.Add(FText::Format(
            LOCTEXT("AllowedZoneCountSafetyLimit", "AllowedZoneIds는 최대 {0}개를 허용합니다."),
            FText::AsNumber(UFFZSDeveloperSettings::MaxAllowedZoneIds)));
    }
    // Collection limits are denial-of-service guards, not tuning rules. Once one is exceeded,
    // do not copy, canonicalize, hash, or iterate attacker-sized arrays merely to accumulate
    // secondary diagnostics. The precise limit error above is the actionable failure.
    if (!UFFZSGridLibrary::HasSafeFacilitySpecCardinality(*this))
    {
        return false;
    }

    if (!FMath::IsFinite(QueueRules.AbandonAfterSeconds) || QueueRules.AbandonAfterSeconds < 0.0f ||
        !FMath::IsFinite(QueueRules.SlotArrivalToleranceCm) || QueueRules.SlotArrivalToleranceCm < 0.0f ||
        !FMath::IsFinite(QueueRules.NavigationProjectionExtentCm) || QueueRules.NavigationProjectionExtentCm <= 0.0f ||
        !FMath::IsFinite(QueueRules.BlockedTimeoutSeconds) || QueueRules.BlockedTimeoutSeconds < 0.0f ||
        QueueRules.MaxRepathAttempts < 0 ||
        QueueRules.MaxRepathAttempts > UFFZSDeveloperSettings::MaxQueueRepathAttempts)
    {
        OutErrors.Add(LOCTEXT("InvalidQueueMovementRules", "Queue 이탈, 도착, 내비게이션, 차단 시간 제한 또는 재탐색 값이 잘못되었습니다."));
    }

    if (QueueSplinePointsCm.Num() < 2)
    {
        OutErrors.Add(LOCTEXT("MissingQueueSpline", "QueueSplinePointsCm은 최소 2개가 필요합니다."));
    }
    else
    {
        int32 FirstNonFinitePointIndex = INDEX_NONE;
        const double QueueSplineLengthCm = FFFZSQueueGeometry::GetLinearPolylineLengthCm(
            QueueSplinePointsCm,
            &FirstNonFinitePointIndex);
        if (FirstNonFinitePointIndex != INDEX_NONE)
        {
            OutErrors.Add(LOCTEXT("InvalidQueueSplinePoint", "Queue Spline Point에는 유한한 센티미터 값을 사용해야 합니다."));
        }
        if (QueueRules.MaxWaitingAgents > 0 &&
            FMath::IsFinite(QueueRules.SlotSpacingCm) &&
            QueueRules.SlotSpacingCm > 0.0f)
        {
            const double RequiredQueueLengthCm =
                (static_cast<double>(QueueRules.MaxWaitingAgents) - 1.0) *
                static_cast<double>(QueueRules.SlotSpacingCm);
            if (QueueSplineLengthCm + UE_KINDA_SMALL_NUMBER < RequiredQueueLengthCm)
            {
                OutErrors.Add(LOCTEXT("QueueSplineTooShort", "Queue Spline은 SlotSpacingCm 간격으로 최대 수용 인원의 모든 슬롯을 배치할 수 있을 만큼 길어야 합니다."));
            }
        }
    }

    TArray<FFFZSCellRect> LocalQueueRects = QueueReservedRects;
    if (QueueReservedRects.IsEmpty() && QueueReservedCells.IsEmpty())
    {
        OutErrors.Add(LOCTEXT(
            "MissingQueueRects",
            "QueueReservedRects는 최대 Queue 영역을 1cm Cell 단위로 예약해야 합니다."));
    }

    for (const FFFZSCellRect& Rect : QueueReservedRects)
    {
        if (!Rect.IsValid())
        {
            OutErrors.Add(LOCTEXT(
                "InvalidQueueRect",
                "모든 QueueReservedRect는 Min.X < Max.X 및 Min.Y < Max.Y 조건을 만족해야 합니다."));
        }
    }

    // 구 필드는 작은 1 cm 호환 데이터만 허용한다. 새 Data Asset은 반드시 QueueReservedRects를 쓴다.
    TSet<FIntPoint> UniqueLegacyQueueCells;
    for (const FIntPoint& Cell : QueueReservedCells)
    {
        if (UniqueLegacyQueueCells.Contains(Cell))
        {
            OutErrors.Add(FText::Format(
                LOCTEXT("DuplicateLegacyQueueCell", "이전 QueueReservedCells에 중복된 Cell({0}, {1})이 있습니다."),
                Cell.X,
                Cell.Y));
        }
        UniqueLegacyQueueCells.Add(Cell);
        if (Cell.X == MAX_int32 || Cell.Y == MAX_int32)
        {
            OutErrors.Add(LOCTEXT("LegacyQueueCellOverflow", "이전 QueueReservedCells에 표현할 수 없는 최대 좌표가 있습니다."));
            continue;
        }
        LocalQueueRects.Emplace(Cell, FIntPoint(Cell.X + 1, Cell.Y + 1));
    }
    LocalQueueRects = UFFZSGridLibrary::CanonicalizeCellRects(LocalQueueRects);

    const TArray<FFFZSCellRect> LocalFootprintRects = {
        FFFZSCellRect(FIntPoint::ZeroValue, FootprintSizeCells)
    };
    if (FootprintSizeCells.X > 0 && FootprintSizeCells.Y > 0 &&
        UFFZSGridLibrary::DoCellRectSetsOverlap(LocalQueueRects, LocalFootprintRects))
    {
        OutErrors.Add(LOCTEXT(
            "QueueOverlapsFootprint",
            "QueueReservedRects는 Facility Footprint와 겹칠 수 없습니다."));
    }

    // 최대 정원 전체를 SlotSpacingCm 간격으로 샘플링한다. 일부 슬롯만 예약 영역에 들어오는
    // 데이터는 실제 만석 상황에서 보행 공간을 침범하므로 저작 단계에서 거부한다.
    if (QueueSplinePointsCm.Num() >= 2 &&
        !QueueSplinePointsCm.ContainsByPredicate(
            [](const FVector& Point) { return Point.ContainsNaN(); }) &&
        QueueRules.MaxWaitingAgents > 0 &&
        QueueRules.MaxWaitingAgents <= UFFZSDeveloperSettings::MaxQueueWaitingAgents &&
        FMath::IsFinite(QueueRules.SlotSpacingCm) &&
        QueueRules.SlotSpacingCm > 0.0f &&
        FootprintSizeCells.X > 0 &&
        FootprintSizeCells.Y > 0)
    {
        bool bAllQueueSlotsReserved = true;
        TArray<FVector> LocalQueueSlotPoints;
        if (!FFFZSQueueGeometry::BuildLinearQueueSlotPoints(
                QueueSplinePointsCm,
                QueueRules.MaxWaitingAgents,
                QueueRules.SlotSpacingCm,
                LocalQueueSlotPoints))
        {
            bAllQueueSlotsReserved = false;
        }
        else
        {
            for (const FVector& LocalSlotPoint : LocalQueueSlotPoints)
            {
                if (!FFFZSQueueGeometry::IsLocalPointInsideReservation(
                        LocalSlotPoint,
                        FootprintSizeCells,
                        LocalQueueRects))
                {
                    bAllQueueSlotsReserved = false;
                    break;
                }
            }
        }
        if (!bAllQueueSlotsReserved)
        {
            OutErrors.Add(LOCTEXT(
                "QueueSlotOutsideReservation",
                "최대 수용 인원의 모든 Queue 슬롯은 QueueReservedRects 안에 있어야 합니다."));
        }
    }

    int32 EntryCount = 0;
    int32 ServiceCount = 0;
    int32 ExitCount = 0;
    TSet<uint64> AnchorKeys;
    TSet<int32> EntryIndices;
    TSet<int32> ServiceIndices;
    TSet<int32> ExitIndices;
    // Anchor 배열 순서는 자유지만 (Type, Index)는 유일해야 한다. Service는 0부터 Capacity-1까지
    // 연속 인덱스를 요구해 런타임이 "가장 낮은 빈 슬롯"을 안전하게 선택할 수 있게 한다.
    for (const FFFZSAnchorDefinition& Anchor : Anchors)
    {
        if (!StaticEnum<EFFZSAnchorType>()->IsValidEnumValue(static_cast<int64>(Anchor.Type)) ||
            Anchor.Index < 0 || Anchor.LocalTransform.ContainsNaN() ||
            !Anchor.LocalTransform.GetScale3D().Equals(FVector::OneVector, 0.001f))
        {
            OutErrors.Add(LOCTEXT("InvalidAnchor", "Anchor의 Type/Index/Transform이 잘못되었습니다. Local Scale은 1이어야 합니다."));
            continue;
        }

        const uint64 AnchorKey = (static_cast<uint64>(Anchor.Type) << 32) | static_cast<uint32>(Anchor.Index);
        if (AnchorKeys.Contains(AnchorKey))
        {
            OutErrors.Add(LOCTEXT("DuplicateAnchor", "Anchor Type/Index 조합은 고유해야 합니다."));
        }
        AnchorKeys.Add(AnchorKey);

        if (Anchor.Type == EFFZSAnchorType::Entry)
        {
            ++EntryCount;
            EntryIndices.Add(Anchor.Index);
        }
        else if (Anchor.Type == EFFZSAnchorType::Service)
        {
            ++ServiceCount;
            ServiceIndices.Add(Anchor.Index);
        }
        else if (Anchor.Type == EFFZSAnchorType::Exit)
        {
            ++ExitCount;
            ExitIndices.Add(Anchor.Index);
        }
    }

    if (EntryCount < 1 || !EntryIndices.Contains(0))
    {
        OutErrors.Add(LOCTEXT("MissingEntry", "최소한 Entry[0]이 필요합니다."));
    }
    if (ServiceCount < QueueRules.ServiceCapacity)
    {
        OutErrors.Add(LOCTEXT("MissingService", "Service Anchor 수는 ServiceCapacity 이상이어야 합니다."));
    }
    if (QueueRules.ServiceCapacity > 0 &&
        QueueRules.ServiceCapacity <= UFFZSDeveloperSettings::MaxFacilityServiceCapacity)
    {
        for (int32 ServiceIndex = 0; ServiceIndex < QueueRules.ServiceCapacity; ++ServiceIndex)
        {
            if (!ServiceIndices.Contains(ServiceIndex))
            {
                OutErrors.Add(LOCTEXT("MissingIndexedService", "Service Anchor는 0부터 ServiceCapacity-1까지 연속된 Index를 제공해야 합니다."));
                break;
            }
        }
    }
    if (ExitCount < 1 || !ExitIndices.Contains(0))
    {
        OutErrors.Add(LOCTEXT("MissingExit", "최소한 Exit[0]이 필요합니다."));
    }

    TSet<FName> UniqueAllowedZones;
    // 빈 배열은 어느 프로젝트 Zone에도 이식 가능한 뜻이다. 배열을 사용한 경우에는 NAME_None과
    // 중복만 금지하며 Festa 전용 이름 목록을 여기서 강제하지 않는다.
    for (const FName AllowedZoneId : AllowedZoneIds)
    {
        if (AllowedZoneId.IsNone())
        {
            OutErrors.Add(LOCTEXT(
                "EmptyAllowedZone",
                "AllowedZoneIds는 비어 있을 수 있지만, 값이 있는 항목은 NAME_None일 수 없습니다."));
            continue;
        }
        if (UniqueAllowedZones.Contains(AllowedZoneId))
        {
            OutErrors.Add(LOCTEXT(
                "DuplicateAllowedZone",
                "AllowedZoneIds는 비어 있을 수 있지만, 값이 있는 항목은 고유해야 합니다."));
            continue;
        }
        UniqueAllowedZones.Add(AllowedZoneId);
    }
    if (!FMath::IsFinite(PlacementObstacleHeightCm) || PlacementObstacleHeightCm < 10.0f)
    {
        OutErrors.Add(LOCTEXT("InvalidObstacleHeight", "PlacementObstacleHeightCm은 유한한 값이며 최소 10cm여야 합니다."));
    }
    if (!StaticEnum<EFFZSVisualSizingMode>()->IsValidEnumValue(static_cast<int64>(VisualSizingMode)))
    {
        OutErrors.Add(LOCTEXT("InvalidVisualSizingMode", "VisualSizingMode 값이 잘못되었습니다."));
    }
    if (VisualOffsetCm.ContainsNaN() || VisualRotation.ContainsNaN())
    {
        OutErrors.Add(LOCTEXT("InvalidVisualTransform", "VisualOffsetCm과 VisualRotation에는 유한한 값만 사용할 수 있습니다."));
    }
    if (VisualSizingMode == EFFZSVisualSizingMode::MatchPlacementBounds &&
        !FFZSVisualGeometry::IsCardinalYawRotation(VisualRotation))
    {
        OutErrors.Add(LOCTEXT(
            "NonCardinalExactVisualRotation",
            "MatchPlacementBounds는 Pitch/Roll 0도와 90도 단위 Yaw만 지원합니다. 임의 회전은 UniformFit을 사용하십시오."));
    }
    if (VisualSizingMode == EFFZSVisualSizingMode::MatchPlacementBounds &&
        VisualOffsetCm != FVector::ZeroVector)
    {
        OutErrors.Add(LOCTEXT(
            "OffsetExactVisualBounds",
            "MatchPlacementBounds는 권위 배치 경계와 정확히 일치해야 하므로 VisualOffsetCm은 0이어야 합니다."));
    }

    return OutErrors.IsEmpty();
}

// 실패 이유는 중복 없이 누적하지만 Summary는 최초의 사람이 읽을 수 있는 메시지를 보존한다.
// 한 번 실패한 검증은 뒤 단계가 성공해도 다시 true가 되지 않는 단조 상태다.
void FFFZSPlacementValidation::AddFailure(const EFFZSPlacementFailure Reason, const FText& Message)
{
    const bool bWasValid = bIsValid;
    if (Reason != EFFZSPlacementFailure::None)
    {
        Reasons.AddUnique(Reason);
    }
    // ValidatePlacementLocal은 성공 시 사람이 읽는 성공 Summary를 채운다. 이후 외부 정책이
    // 거부하면 그 성공 문구가 실패 이유를 가리지 않도록 첫 실패 메시지로 교체한다.
    if ((bWasValid || Summary.IsEmpty()) && !Message.IsEmpty())
    {
        Summary = Message;
    }
    bIsValid = false;
}

#undef LOCTEXT_NAMESPACE
