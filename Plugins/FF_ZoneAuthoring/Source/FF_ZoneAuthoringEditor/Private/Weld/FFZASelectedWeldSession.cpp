#include "Weld/FFZASelectedWeldSession.h"

#include "Components/SplineComponent.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSZoneActor.h"
#include "FF_ZoneAuthoringModule.h"
#include "FFZAAuthoredZone.h"
#include "FFZAAutoWeld.h"
#include "FFZASplineAuthoring.h"
#include "FFZAZoneAuthoringSubsystem.h"
#include "FFZAZoneOverlayActor.h"
#include "LevelUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FFZASelectedWeldSession"

namespace
{
    constexpr double PointEpsilonSquared = 1.e-8;

    bool IsFinitePoint(const FVector& Point)
    {
        return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y) && FMath::IsFinite(Point.Z);
    }

    bool IsInvertibleTransform(const FTransform& Transform)
    {
        const FVector Scale = Transform.GetScale3D();
        return !Transform.ContainsNaN() && FMath::Abs(Scale.X) > UE_SMALL_NUMBER &&
            FMath::Abs(Scale.Y) > UE_SMALL_NUMBER && FMath::Abs(Scale.Z) > UE_SMALL_NUMBER;
    }

    void GetHeights(const TArray<FVector>& Points, TArray<double>& OutHeights)
    {
        OutHeights.Reset(Points.Num());
        for (const FVector& Point : Points) { OutHeights.Add(Point.Z); }
    }

    void GetXY(const TArray<FVector>& Points, TArray<FVector2D>& OutPolygon)
    {
        OutPolygon.Reset(Points.Num());
        for (const FVector& Point : Points) { OutPolygon.Emplace(Point.X, Point.Y); }
    }

    FText StaleMessage(const FString& Label)
    {
        return FText::Format(LOCTEXT("Stale", "'{0}'의 경계 또는 Transform이 미리보기 이후 바뀌었습니다. 미리보기를 다시 실행하세요."),
            FText::FromString(Label));
    }
}

bool FFFZASelectedWeldGeometrySnapshot::Matches(const FFFZASelectedWeldGeometrySnapshot& Other) const
{
    return ActorTransform.Equals(Other.ActorTransform, 0.0) && SplineTransform.Equals(Other.SplineTransform, 0.0) &&
        bClosedLoop == Other.bClosedLoop && Points == Other.Points && PointTypes == Other.PointTypes;
}

FFFZASelectedWeldSession::FFFZASelectedWeldSession(const TArray<AFFZSGameplayZone*>& SelectedZones,
    const float InAnalysisCellSizeCm)
{
    if (FMath::IsFinite(InAnalysisCellSizeCm) && InAnalysisCellSizeCm >= 10.0f)
    {
        AnalysisCellSizeCm = InAnalysisCellSizeCm;
    }
    if (GEditor) { World = GEditor->GetEditorWorldContext().World(); }
    TSet<AFFZSGameplayZone*> Seen;
    for (AFFZSGameplayZone* Zone : SelectedZones)
    {
        if (!IsValid(Zone) || Seen.Contains(Zone)) { continue; }
        Seen.Add(Zone);
        FFFZASelectedWeldZone& Row = Zones.AddDefaulted_GetRef();
        Row.Zone = Zone;
        Row.Label = Zone->GetActorNameOrLabel();
        Row.ActorPath = Zone->GetPathName();
        Row.ZoneId = Zone->ZoneId;
    }
    // Paths identify rows within this fixed scope. Geometric solvers decide common boundaries
    // independently of these paths, selection order, and duplicate authored IDs.
    Zones.Sort([](const FFFZASelectedWeldZone& A, const FFFZASelectedWeldZone& B)
    {
        return A.ActorPath < B.ActorPath;
    });
    CaptureGeometry();
    StatusText = LOCTEXT("ReadyForAutomatic", "수집한 Zone들의 공통 경계를 자동으로 계산할 수 있습니다.");
}

UWorld* FFFZASelectedWeldSession::GetWorld() const
{
    return World.Get();
}

bool FFFZASelectedWeldSession::CheckEditorWorld(FText& OutError) const
{
    UWorld* CapturedWorld = World.Get();
    if (!GEditor || !CapturedWorld || CapturedWorld->WorldType != EWorldType::Editor ||
        GEditor->GetEditorWorldContext().World() != CapturedWorld)
    {
        OutError = LOCTEXT("ChangedWorld", "편집 중인 월드가 바뀌었습니다. 현재 선택 목록을 다시 가져오세요.");
        return false;
    }
    if (GEditor->PlayWorld || GEditor->IsSimulateInEditorInProgress())
    {
        OutError = LOCTEXT("DuringPIE", "플레이 또는 시뮬레이션을 종료한 뒤 미리보기를 다시 실행하세요.");
        return false;
    }
    return true;
}

void FFFZASelectedWeldSession::ClearPreview()
{
    // Repeated tolerance slider events should not copy all selected boundaries every frame.
    if (!bHasPreview && ReferenceIndex == INDEX_NONE && Stats.SourcePointKeys.IsEmpty()) { return; }
    bHasPreview = false;
    bAutomaticRepairComplete = false;
    bExpandedRepairRadius = false;
    bGapAreaRecoveryUsed = false;
    RepairRadiusCm = 0.0;
    RepairFailureReason.Reset();
    Stats = FFZALayoutOps::FWeldStats();
    Diagnostics = FFZAWeldDiagnostics::FResult();
    ReferenceIndex = INDEX_NONE;
    for (FFFZASelectedWeldZone& Row : Zones)
    {
        Row.bChanged = false;
        Row.bReference = false;
        Row.bAutoReference = false;
        Row.GroupIndex = INDEX_NONE;
        Row.bDisconnected = false;
        Row.bRejected = false;
        Row.bInvalidInput = !Row.bSupported;
        Row.After = Row.Before;
        Row.SourcePointKeys.Reset();
        Row.MovedPointCount = 0;
        Row.InsertedPointCount = 0;
        Row.RemovedPointCount = 0;
        Row.MaximumMoveCm = 0.0;
        Row.ResidualGapCount = 0;
        Row.ResidualOverlapCount = 0;
        if (Row.bSupported) { Row.Issue.Reset(); }
    }
}

void FFFZASelectedWeldSession::CaptureGeometry()
{
    ClearPreview();
    Snapshots.Reset(Zones.Num());
    Snapshots.SetNum(Zones.Num());
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        FFFZASelectedWeldZone& Row = Zones[Index];
        FSplineSnapshot& Snapshot = Snapshots[Index];
        Row.bSupported = false;
        Row.Before.Reset();
        Row.After.Reset();
        Row.Issue.Reset();
        AFFZSGameplayZone* Zone = Row.Zone.Get();
        if (!Zone || Zone->IsActorBeingDestroyed() || Zone->GetWorld() != World.Get())
        {
            Row.Issue = TEXT("선택한 Zone이 삭제되었거나 다른 월드에 있습니다.");
            Row.bInvalidInput = true;
            continue;
        }
        Row.Label = Zone->GetActorNameOrLabel();
        Row.ZoneId = Zone->ZoneId;
        Snapshot.Level = Zone->GetLevel();
        Snapshot.Geometry.ActorTransform = Zone->GetActorTransform();
        Snapshot.Spline = Zone->BoundarySpline;
        USplineComponent* Spline = Snapshot.Spline.Get();
        if (!Spline)
        {
            Row.Issue = TEXT("Boundary Spline이 없습니다.");
            Row.bInvalidInput = true;
            continue;
        }
        Snapshot.Geometry.SplineTransform = Spline->GetComponentTransform();
        Snapshot.Geometry.bClosedLoop = Spline->IsClosedLoop();
        const int32 PointCount = Spline->GetNumberOfSplinePoints();
        Row.bSupported = PointCount >= 3 && Snapshot.Geometry.bClosedLoop &&
            IsInvertibleTransform(Snapshot.Geometry.ActorTransform) && IsInvertibleTransform(Snapshot.Geometry.SplineTransform);
        Row.Before.Reserve(PointCount);
        Snapshot.Geometry.PointTypes.Reserve(PointCount);
        for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
        {
            const FVector Point = Spline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
            const ESplinePointType::Type PointType = Spline->GetSplinePointType(PointIndex);
            Row.Before.Add(Point);
            Snapshot.Geometry.PointTypes.Add(static_cast<uint8>(PointType));
            Row.bSupported &= PointType == ESplinePointType::Linear && IsFinitePoint(Point);
        }
        Row.After = Row.Before;
        Snapshot.Geometry.Points = Row.Before;
        Row.bInvalidInput = !Row.bSupported;
        if (!Row.bSupported)
        {
            Row.Issue = TEXT("유한한 좌표와 유효한 Transform을 가진 닫힌 Linear 경계(3점 이상)가 필요합니다.");
        }
    }
}

bool FFFZASelectedWeldSession::BuildPreview(const int32 InReferenceIndex, const double ToleranceCm, FText& OutError)
{
    return BuildPreviewInternal(InReferenceIndex, ToleranceCm, OutError, false);
}

bool FFFZASelectedWeldSession::BuildPreview(const double ToleranceCm, FText& OutError)
{
    return BuildPreviewInternal(INDEX_NONE, ToleranceCm, OutError, true);
}

bool FFFZASelectedWeldSession::BuildPreviewInternal(const int32 InReferenceIndex, const double ToleranceCm,
    FText& OutError, const bool bAutomatic)
{
    ClearPreview();
    OutError = FText::GetEmpty();
    if (!CheckEditorWorld(OutError)) { StatusText = OutError; return false; }
    if (Zones.Num() < 2)
    {
        OutError = LOCTEXT("TooFewZones", "작업 목록에 최소 두 개의 Zone이 필요합니다.");
        StatusText = OutError;
        return false;
    }
    if (!FMath::IsFinite(ToleranceCm) || ToleranceCm <= 0.0 || !FMath::IsFinite(ToleranceCm * ToleranceCm))
    {
        OutError = LOCTEXT("InvalidTolerance", "Weld Tolerance Cm은 0보다 큰 유한한 값이어야 합니다.");
        StatusText = OutError;
        return false;
    }
    CaptureGeometry();
    if (!bAutomatic && (!Zones.IsValidIndex(InReferenceIndex) || !Zones[InReferenceIndex].bSupported))
    {
        OutError = LOCTEXT("InvalidReference", "선택 목록 안의 유효한 Zone을 기준으로 지정하세요.");
        StatusText = OutError;
        return false;
    }
    TArray<TArray<FVector2D>> Polygons;
    Polygons.SetNum(Zones.Num());
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        if (Zones[Index].bSupported) { GetXY(Zones[Index].Before, Polygons[Index]); }
    }
    FFZAAutoWeld::FResult AutomaticResult;
    const bool bBuilt = bAutomatic
        ? FFZAAutoWeld::BuildAutomaticResult(Polygons, ToleranceCm, AutomaticResult)
        : FFZAAutoWeld::BuildResult(Polygons, InReferenceIndex, ToleranceCm, AutomaticResult);
    Stats = MoveTemp(AutomaticResult.Stats);
    if (Stats.bInvalidReference)
    {
        Zones[InReferenceIndex].bInvalidInput = true;
        Zones[InReferenceIndex].Issue = TEXT("기준 경계가 자기교차 또는 붕괴된 다각형입니다.");
        OutError = LOCTEXT("InvalidReferenceRing", "기준 Zone의 경계를 유효한 단순 다각형으로 정리한 뒤 다시 실행하세요.");
        StatusText = OutError;
        return false;
    }
    if (!bBuilt)
    {
        OutError = FText::FromString(AutomaticResult.FailureReason);
        StatusText = OutError;
        return false;
    }
    Polygons = MoveTemp(AutomaticResult.Polygons);
    const TArray<bool>& Changed = AutomaticResult.ChangedPolygons;
    Diagnostics = MoveTemp(AutomaticResult.Diagnostics);
    bAutomaticRepairComplete = AutomaticResult.bCanCommit;
    bExpandedRepairRadius = AutomaticResult.bExpandedRadiusUsed;
    bGapAreaRecoveryUsed = AutomaticResult.bAreaFallbackUsed;
    RepairRadiusCm = AutomaticResult.RepairRadiusCm;
    RepairFailureReason = MoveTemp(AutomaticResult.FailureReason);
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        RepairFailureReason.ReplaceInline(*FString::Printf(TEXT("Zone %d:"), Index + 1),
            *FString::Printf(TEXT("'%s':"), *Zones[Index].Label), ESearchCase::CaseSensitive);
    }
    ReferenceIndex = InReferenceIndex;
    for (int32 GroupIndex = 0; GroupIndex < Stats.Groups.Num(); ++GroupIndex)
    {
        const FFZALayoutOps::FWeldGroup& Group = Stats.Groups[GroupIndex];
        for (const int32 Index : Group.PolygonIndices)
        {
            if (Zones.IsValidIndex(Index)) { Zones[Index].GroupIndex = GroupIndex; }
        }
        if (Zones.IsValidIndex(Group.ReferencePolygonIndex))
        {
            FFFZASelectedWeldZone& Root = Zones[Group.ReferencePolygonIndex];
            Root.bReference = true;
            Root.bAutoReference = !Group.bUsesManualReference;
        }
    }
    for (const int32 Index : Stats.InvalidInputPolygonIndices)
    {
        if (Zones.IsValidIndex(Index))
        {
            Zones[Index].bInvalidInput = true;
            if (Zones[Index].Issue.IsEmpty()) { Zones[Index].Issue = TEXT("유효하지 않은 다각형이어서 건너뜁니다."); }
        }
    }
    for (const int32 Index : Stats.DisconnectedPolygonIndices)
    {
        if (Zones.IsValidIndex(Index))
        {
            Zones[Index].bDisconnected = true;
            Zones[Index].Issue = TEXT("묶음 안에서 안전하게 용접할 기준 경계가 남지 않아 건너뜁니다.");
        }
    }
    for (const int32 Index : Stats.RejectedPolygonIndices)
    {
        if (Zones.IsValidIndex(Index))
        {
            Zones[Index].bRejected = true;
            Zones[Index].Issue = TEXT("자기교차, 붕괴 또는 겹침을 만드는 결과여서 거부했습니다.");
        }
    }
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        FFFZASelectedWeldZone& Row = Zones[Index];
        Row.bChanged = Changed.IsValidIndex(Index) && Changed[Index];
        if (!Row.bChanged) { continue; }
        TArray<double> Heights;
        GetHeights(Row.Before, Heights);
        Row.SourcePointKeys = Stats.SourcePointKeys[Index];
        if (!FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(Heights, Polygons[Index], Row.SourcePointKeys, Row.After))
        {
            OutError = LOCTEXT("BadWriteback", "경계 결과의 높이 보간을 검증할 수 없습니다. 미리보기를 다시 실행하세요.");
            ClearPreview();
            StatusText = OutError;
            return false;
        }
        TSet<int32> RetainedOriginalPoints;
        for (int32 PointIndex = 0; PointIndex < Row.After.Num(); ++PointIndex)
        {
            const double Key = Row.SourcePointKeys[PointIndex];
            const int32 Edge = FMath::FloorToInt(Key);
            const FVector OriginalPoint = FMath::Lerp(Row.Before[Edge], Row.Before[(Edge + 1) % Row.Before.Num()], Key - Edge);
            const FVector2D Delta(Row.After[PointIndex].X - OriginalPoint.X, Row.After[PointIndex].Y - OriginalPoint.Y);
            Row.MaximumMoveCm = FMath::Max(Row.MaximumMoveCm, Delta.Size());
            if (Key == static_cast<double>(Edge))
            {
                RetainedOriginalPoints.Add(Edge);
                if (Delta.SizeSquared() > PointEpsilonSquared) { ++Row.MovedPointCount; }
            }
            else { ++Row.InsertedPointCount; }
        }
        Row.RemovedPointCount = Row.Before.Num() - RetainedOriginalPoints.Num();
    }
    // Automatic repair returns its final inspection. Slate and drawing use the cached result;
    // never repeat polygon clipping or pair analysis on a frame attribute.
    TSet<uint64> CountedGapPairs;
    TSet<uint64> CountedOverlapPairs;
    for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Diagnostics.Issues)
    {
        if (Issue.Type == FFZAWeldDiagnostics::EIssueType::Rejected ||
            !Zones.IsValidIndex(Issue.PolygonIndexA) || !Zones.IsValidIndex(Issue.PolygonIndexB)) { continue; }
        const uint32 First = static_cast<uint32>(FMath::Min(Issue.PolygonIndexA, Issue.PolygonIndexB));
        const uint32 Second = static_cast<uint32>(FMath::Max(Issue.PolygonIndexA, Issue.PolygonIndexB));
        const uint64 PairKey = (static_cast<uint64>(First) << 32) | Second;
        TSet<uint64>& CountedPairs = Issue.Type == FFZAWeldDiagnostics::EIssueType::Gap ? CountedGapPairs : CountedOverlapPairs;
        if (CountedPairs.Contains(PairKey)) { continue; }
        CountedPairs.Add(PairKey);
        for (const int32 Index : {Issue.PolygonIndexA, Issue.PolygonIndexB})
        {
            if (Issue.Type == FFZAWeldDiagnostics::EIssueType::Gap) { ++Zones[Index].ResidualGapCount; }
            else { ++Zones[Index].ResidualOverlapCount; }
        }
    }
    bHasPreview = true;
    StatusText = FText::Format(LOCTEXT("PreviewSummary",
        "대상 {0} Zone / 묶음 {1} / 변경 {2} Zone / 이동 {3}점 / 추가 {4}점 / 최대 이동 {5} cm / 잔여 틈 {6}쌍 / 겹침 {7}쌍 / 거부 {8} / 잘못된 경계 {9}"),
        FText::AsNumber(Zones.Num()), FText::AsNumber(Stats.Groups.Num()), FText::AsNumber(Stats.ChangedPolygonCount),
        FText::AsNumber(Stats.MovedVertexCount), FText::AsNumber(Stats.InsertedVertexCount), FText::AsNumber(Stats.MaximumMoveCm),
        FText::AsNumber(Diagnostics.GapPairCount), FText::AsNumber(Diagnostics.OverlapPairCount),
        FText::AsNumber(Stats.RejectedPolygonCount), FText::AsNumber(Stats.InvalidInputPolygonCount));
    StatusText = FText::Format(LOCTEXT("AutomaticPreviewSummary", "{0}\n자동 틈 복구 범위 {1} cm / 제거 {2}점 / {3}"),
        StatusText, FText::AsNumber(RepairRadiusCm), FText::AsNumber(Stats.RemovedVertexCount),
        bAutomaticRepairComplete ? LOCTEXT("RepairPassed", "최종 검사 통과") : LOCTEXT("RepairNotReady", "적용 불가"));
    if (bGapAreaRecoveryUsed)
    {
        StatusText = FText::Format(LOCTEXT("GapAreaRecoverySummary", "{0}\n복구 방식: 틈 영역을 이웃 Zone에 편입"), StatusText);
    }
    if (!bAutomaticRepairComplete)
    {
        StatusText = FText::Format(LOCTEXT("IncompleteRepairPreview", "{0}\n자동 복구 미완료: {1}. 적용 시 일부 결과만 쓰지 않습니다."),
            StatusText, FText::FromString(RepairFailureReason));
    }
    return true;
}

bool FFFZASelectedWeldSession::CheckSnapshotCurrent(FText& OutError) const
{
    if (!CheckEditorWorld(OutError)) { return false; }
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        const FFFZASelectedWeldZone& Row = Zones[Index];
        const FSplineSnapshot& Snapshot = Snapshots[Index];
        AFFZSGameplayZone* Zone = Row.Zone.Get();
        if (!Zone || Zone->IsActorBeingDestroyed() || Zone->GetWorld() != World.Get() ||
            Zone->GetLevel() != Snapshot.Level.Get() || !World->ContainsLevel(Zone->GetLevel()) ||
            Zone->BoundarySpline != Snapshot.Spline.Get())
        {
            OutError = StaleMessage(Row.Label);
            return false;
        }
        FFFZASelectedWeldGeometrySnapshot CurrentGeometry;
        CurrentGeometry.ActorTransform = Zone->GetActorTransform();
        USplineComponent* Spline = Snapshot.Spline.Get();
        if (Spline)
        {
            CurrentGeometry.SplineTransform = Spline->GetComponentTransform();
            CurrentGeometry.bClosedLoop = Spline->IsClosedLoop();
            const int32 PointCount = Spline->GetNumberOfSplinePoints();
            CurrentGeometry.Points.Reserve(PointCount);
            CurrentGeometry.PointTypes.Reserve(PointCount);
            for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
            {
                CurrentGeometry.Points.Add(Spline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World));
                CurrentGeometry.PointTypes.Add(static_cast<uint8>(Spline->GetSplinePointType(PointIndex)));
            }
        }
        if (!Snapshot.Geometry.Matches(CurrentGeometry))
        {
            OutError = StaleMessage(Row.Label);
            return false;
        }
    }
    return true;
}

bool FFFZASelectedWeldSession::Apply(FText& OutMessage)
{
    if (!CanApply())
    {
        OutMessage = bHasPreview && bAutomaticRepairComplete
            ? LOCTEXT("AlreadyRepaired", "선택한 범위에 틈·겹침이 없고 적용할 변경도 없습니다.")
            : LOCTEXT("NoChanges", "Zone 목록과 허용 거리를 확인하고 자동 연결을 계산하세요.");
        return false;
    }
    if (!CheckSnapshotCurrent(OutMessage))
    {
        ClearPreview();
        StatusText = OutMessage;
        return false;
    }
    // An enabled button can explain an unresolved preview, but must never write a partly
    // repaired selection. Completeness is based on full pair counts, not capped display rows.
    if (!bAutomaticRepairComplete || Diagnostics.GapPairCount != 0 || Diagnostics.OverlapPairCount != 0 ||
        Stats.RejectedPolygonCount != 0 || Stats.InvalidInputPolygonCount != 0)
    {
        OutMessage = FText::Format(LOCTEXT("IncompleteRepairApply", "자동 복구를 완료하지 못해 적용하지 않았습니다. {0}\n남은 틈 {1}쌍 / 겹침 {2}쌍. 표시된 구간을 확인하세요."),
            FText::FromString(RepairFailureReason), FText::AsNumber(Diagnostics.GapPairCount),
            FText::AsNumber(Diagnostics.OverlapPairCount));
        StatusText = OutMessage;
        return false;
    }
    // Validate every actor and every height/key before any Modify() call. No world lookup can
    // add another Zone to this scope. The synchronous commit consumes exactly this snapshot.
    TArray<TArray<FVector2D>> Polygons;
    Polygons.SetNum(Zones.Num());
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        const FFFZASelectedWeldZone& Row = Zones[Index];
        if (!Row.bChanged) { continue; }
        AFFZSGameplayZone* Zone = Row.Zone.Get();
        if (!Zone->IsEditable() || FLevelUtils::IsLevelLocked(Zone))
        {
            OutMessage = FText::Format(LOCTEXT("LockedActor", "'{0}'이 편집 불가이거나 레벨이 잠겨 있습니다. 잠금을 해제하고 미리보기를 다시 실행하세요."),
                FText::FromString(Row.Label));
            ClearPreview();
            StatusText = OutMessage;
            return false;
        }
        TArray<double> Heights;
        TArray<FVector> ValidatedPoints;
        GetHeights(Row.Before, Heights);
        GetXY(Row.After, Polygons[Index]);
        if (!FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(Heights, Polygons[Index], Row.SourcePointKeys, ValidatedPoints) ||
            ValidatedPoints != Row.After)
        {
            OutMessage = LOCTEXT("ApplyValidationFailed", "경계 결과 검증에 실패했습니다. 미리보기를 다시 실행하세요.");
            ClearPreview();
            StatusText = OutMessage;
            return false;
        }
    }
    {
        const FScopedTransaction Transaction(LOCTEXT("WeldSelection", "선택한 Zone 틈·겹침 자동 복구"));
        for (int32 Index = 0; Index < Zones.Num(); ++Index)
        {
            const FFFZASelectedWeldZone& Row = Zones[Index];
            if (!Row.bChanged) { continue; }
            AFFZSGameplayZone* Zone = Row.Zone.Get();
            Zone->Modify();
            // The identical pure preflight and unchanged spline guarantee successful writeback.
            const bool bApplied = FFZASplineAuthoring::SetWorldPolygonPreservingHeights(
                *Zone->BoundarySpline, Polygons[Index], Row.SourcePointKeys);
            check(bApplied);
            if (AFFZAAuthoredZone* Authored = Cast<AFFZAAuthoredZone>(Zone)) { Authored->RefreshAuthoringStatus(); }
        }
    }
    RefreshDerivedData();
    OutMessage = FText::Format(LOCTEXT("Applied", "{0}개 묶음의 {1}개 Zone에 적용했습니다. 잔여 틈 {2}쌍 / 겹침 {3}쌍 / 거부 {4}. Ctrl+Z로 되돌릴 수 있습니다."),
        FText::AsNumber(Stats.Groups.Num()), FText::AsNumber(Stats.ChangedPolygonCount),
        FText::AsNumber(Diagnostics.GapPairCount), FText::AsNumber(Diagnostics.OverlapPairCount),
        FText::AsNumber(Stats.RejectedPolygonCount));
    StatusText = OutMessage;
    bHasPreview = false;
    const FString ReferenceLabel = Zones.IsValidIndex(ReferenceIndex) ? Zones[ReferenceIndex].Label : TEXT("automatic shared seams");
    UE_LOG(LogFFZoneAuthoring, Log, TEXT("Boundary Connect: reference %s, captured %d, groups %d, changed %d, moved %d, inserted %d, maximum move %.3f cm, residual gaps %d, overlaps %d, rejected %d."),
        *ReferenceLabel, Zones.Num(), Stats.Groups.Num(), Stats.ChangedPolygonCount, Stats.MovedVertexCount,
        Stats.InsertedVertexCount, Stats.MaximumMoveCm, Diagnostics.GapPairCount, Diagnostics.OverlapPairCount,
        Stats.RejectedPolygonCount);
    return true;
}

void FFFZASelectedWeldSession::RefreshDerivedData() const
{
    UWorld* CapturedWorld = World.Get();
    if (!CapturedWorld || CapturedWorld->WorldType != EWorldType::Editor) { return; }
    for (TActorIterator<AFFZSBuildArea> It(CapturedWorld); It; ++It) { It->RebuildFromWorldRegions(); }
    if (UFFZAZoneAuthoringSubsystem* Subsystem = CapturedWorld->GetSubsystem<UFFZAZoneAuthoringSubsystem>())
    {
        Subsystem->RebuildZoneCellMap(AnalysisCellSizeCm);
    }
    for (TActorIterator<AFFZAZoneOverlayActor> It(CapturedWorld); It; ++It) { It->RebuildOverlay(); }
    if (GEditor) { GEditor->RedrawLevelEditingViewports(); }
}

#undef LOCTEXT_NAMESPACE
