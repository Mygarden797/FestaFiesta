// FF_BuildPlacementAssistance.cpp
// 역할: 설치 실패 원인을 안내하고 가까운 유효 위치를 나누어 탐색한다.

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildingSettings.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "Building/Presentation/FF_BuildFeedback.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "FFBuildPlacementAssistance"

FFF_BuildPlacementGuidance UFF_BuildModeSubsystem::GetPlacementGuidance(const int32 ProblemIndex) const
{
    FFF_BuildPlacementGuidance Result;
    if (!FFBuildModeModel::IsPreviewActive(State)) return Result;
    if (!bHasPlacementTarget)
    {
        Result.Title = LOCTEXT("ChooseGround", "건축할 바닥을 가리켜 주세요");
        Result.Advice = LOCTEXT("GroundAdvice", "흰색 건축 경계 안으로 시설을 옮겨 주세요. G로 건축 구역을 확인할 수 있어요.");
        return Result;
    }
    if (LastValidation.bIsValid)
    {
        Result.Title = LOCTEXT("Ready", "배치 가능");
        Result.Advice = bDirectPlacementAdjustment
            ? LOCTEXT("DirectReady", "위치·회전 유지 · 화살표로 이동 · 원으로 회전\nEnter로 확정 · T로 커서 배치")
            : LOCTEXT("CursorReady", "클릭 / Enter로 확정 · T로 직접 조정");
        return Result;
    }
    Result.ProblemCount = FMath::Max(1, LastValidation.Reasons.Num());
    Result.ProblemIndex = FMath::Clamp(ProblemIndex, 0, Result.ProblemCount - 1);
    const EFFZSPlacementFailure Reason = LastValidation.Reasons.IsValidIndex(Result.ProblemIndex)
        ? LastValidation.Reasons[Result.ProblemIndex] : EFFZSPlacementFailure::InternalError;
    Result.Title = FFBuildPlacementAssistance::ReasonTitle(Reason);
    Result.Advice = UFF_BuildPlacementFailureFeedback::MakeValidationReason(Reason, LastValidation, State == EFF_BuildModeState::Moving);
    if (const FFFZSPlacementObstruction* GroundIssue = LastValidation.Obstructions.FindByPredicate(
        [Reason](const FFFZSPlacementObstruction& Issue)
        { return Issue.Reason == Reason && Issue.Kind == EFFZSPlacementObstructionKind::GroundSupport; }))
    {
        if (!GroundIssue->DisplayName.IsEmpty()) Result.Title = GroundIssue->DisplayName;
    }
    if (Reason == EFFZSPlacementFailure::OutsideZone)
    {
        const FText BoundaryFailure = LastValidation.GetBoundaryFailureText();
        if (!BoundaryFailure.IsEmpty())
        {
            Result.Title = BoundaryFailure;
            Result.Advice = UFF_BuildPlacementFailureFeedback::MakeBoundaryAdvice();
        }
    }
    Result.bCanFindNearby = FFBuildPlacementAssistance::CanResolveByMoving(LastValidation.Reasons);
    if (Result.bCanFindNearby)
        Result.Advice = FText::Format(LOCTEXT("NearbyShortcutAdvice", "{0}\nN으로 가까운 빈자리로 이동할 수 있어요."), Result.Advice);
    // These are diagnostics from the original validation, not another collision query.
    for (const auto& Blocker : LastValidation.Obstructions)
    {
        if (Blocker.Reason == Reason && Blocker.Kind != EFFZSPlacementObstructionKind::GroundSupport &&
            !Blocker.DisplayName.IsEmpty())
        {
            Result.Obstruction = FText::Format(LOCTEXT("Blocker", "겹치는 대상: {0}"), Blocker.DisplayName);
            break;
        }
    }
    return Result;
}

void UFF_BuildModeSubsystem::CancelNearbyPlacementSearch()
{
    bFindingNearbyPlacement = false;
    NearbyOffsets.Reset();
    NearbyCandidateIndex = 0;
}

void UFF_BuildModeSubsystem::ToggleDirectPlacementAdjustment()
{
    if (bInputSuspendedForPauseMenu || !FFBuildModeModel::IsPreviewActive(State) || !bHasPlacementTarget) return;
    NotifyBuildToolsInteraction();
    CancelNearbyPlacementSearch();
    bDirectPlacementAdjustment = !bDirectPlacementAdjustment;
    PlacementAssistanceMessage = FText::GetEmpty();
    if (!bDirectPlacementAdjustment) ResetPlacementAssistance();
    RefreshPlacementGizmo();
    OnBuildToolsChanged.Broadcast();
}

bool UFF_BuildModeSubsystem::FindNearbyPlacement()
{
    if (bInputSuspendedForPauseMenu || bFindingNearbyPlacement || !PlacementPreview || !GetZoneSystem() ||
        !GetPlacementGuidance().bCanFindNearby) return false;
    NotifyBuildToolsInteraction();
    // Pin before searching, so the result and the later confirmation refer to the same position.
    // Discard a prior pointer gesture before taking the search snapshot; a delayed cancel
    // must not overwrite the independently validated result when handles become visible again.
    ResetPlacementAssistance();
    bDirectPlacementAdjustment = true;
    RebuildPreviewCommand();
    NearbySearchCommand = PlacementPreview->PreviewCommand;
    NearbyOffsets = FFBuildPlacementAssistance::MakeNearbyOffsets(GetPlacementPositionSnapCm());
    NearbyCandidateIndex = 0;
    bFindingNearbyPlacement = !NearbyOffsets.IsEmpty();
    PlacementAssistanceMessage = LOCTEXT("Searching", "현재 방향을 유지하며 가까운 자리를 찾고 있어요…");
    RefreshPlacementGizmo();
    OnBuildToolsChanged.Broadcast();
    return bFindingNearbyPlacement;
}

void UFF_BuildModeSubsystem::TickPlacementAssistance()
{
    RefreshPlacementGizmo();
    if (!bFindingNearbyPlacement || bInputSuspendedForPauseMenu) return;
    UFFZSWorldSubsystem* Zones = GetZoneSystem();
    if (!Zones || !bHasPlacementTarget || !FFBuildModeModel::IsPreviewActive(State) ||
        NearbySearchCommand.ExpectedWorldRevision != Zones->GetWorldRevision())
    {
        CancelNearbyPlacementSearch();
        PlacementAssistanceMessage = LOCTEXT("SearchChanged", "주변 상태가 바뀌었어요. 다시 빈자리를 찾아 주세요.");
        OnBuildToolsChanged.Broadcast();
        return;
    }
    // Full authority validation is bounded per frame. Never infer validity from a green grid cell.
    const UFF_BuildingSettings* Settings = GetDefault<UFF_BuildingSettings>();
    const double Deadline = FPlatformTime::Seconds() + Settings->GetWorkBudgetSeconds();
    const int32 MaxCandidates = Settings->GetNearbyCandidatesPerFrame();
    for (int32 Budget = 0; Budget < MaxCandidates && NearbyCandidateIndex < NearbyOffsets.Num(); ++Budget)
    {
        const FIntPoint Offset = NearbyOffsets[NearbyCandidateIndex++];
        const int64 X = static_cast<int64>(NearbySearchCommand.Placement.OriginCell.X) + Offset.X;
        const int64 Y = static_cast<int64>(NearbySearchCommand.Placement.OriginCell.Y) + Offset.Y;
        if (X < MIN_int32 || X > MAX_int32 || Y < MIN_int32 || Y > MAX_int32) continue;
        FFFZSCommand Candidate = NearbySearchCommand;
        Candidate.Placement.OriginCell = FIntPoint(static_cast<int32>(X), static_cast<int32>(Y));
        const FFFZSPlacementValidation Validation = Zones->ValidatePlacement(Candidate);
        if (Validation.bIsValid)
        {
            CurrentPlacement = Candidate.Placement;
            CancelNearbyPlacementSearch();
            RefreshCurrentPlacementValidation();
            PlacementAssistanceMessage = LastValidation.bIsValid
                ? LOCTEXT("Found", "가까운 빈자리로 이동했어요. Enter 또는 배치 확정을 눌러 완료하세요.")
                : LOCTEXT("ChangedBeforeApply", "조건이 바뀌었어요. 표시된 원인을 확인해 주세요.");
            FFBuildFeedback::Pulse(GetLocalPlayerController());
            OnBuildToolsChanged.Broadcast();
            return;
        }
        if (FPlatformTime::Seconds() >= Deadline) break;
    }
    if (NearbyCandidateIndex >= NearbyOffsets.Num())
    {
        CancelNearbyPlacementSearch();
        PlacementAssistanceMessage = LOCTEXT("NotFound", "주변 6m의 후보 위치에서 자리를 찾지 못했어요. 회전하거나 T로 위치를 다시 선택해 주세요.");
        OnBuildToolsChanged.Broadcast();
    }
}
#undef LOCTEXT_NAMESPACE
