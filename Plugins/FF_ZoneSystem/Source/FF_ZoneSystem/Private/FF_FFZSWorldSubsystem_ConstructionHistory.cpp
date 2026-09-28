// Atomic construction edits and bounded, session-local undo/redo.
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueComponent.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "FFZSConstructionHistory"

namespace
{
    bool IsPreparation(EFFZSChangePhase Phase)
    {
        return Phase == EFFZSChangePhase::InitialSetup || Phase == EFFZSChangePhase::MorningCheck;
    }
    bool IsConstruction(EFFZSCommandType Type)
    {
        return Type == EFFZSCommandType::Install || Type == EFFZSCommandType::Move || Type == EFFZSCommandType::Remove;
    }
}

bool UFFZSWorldSubsystem::CanUndoConstruction() const
{
    return !IsAuthorityMutationBlocked() && IsPreparation(CurrentPhase) && !ConstructionUndoStack.IsEmpty();
}
bool UFFZSWorldSubsystem::CanRedoConstruction() const
{
    return !IsAuthorityMutationBlocked() && IsPreparation(CurrentPhase) && !ConstructionRedoStack.IsEmpty();
}
void UFFZSWorldSubsystem::ClearConstructionHistory()
{
    if (bIsReplayingConstruction || bIsConstructionBatch) return;
    ConstructionUndoStack.Reset();
    ConstructionRedoStack.Reset();
}
void UFFZSWorldSubsystem::PushConstructionHistory(FFFZSConstructionHistoryEntry&& Entry)
{
    if (bIsReplayingConstruction || Entry.Commands.IsEmpty()) return;
    ConstructionRedoStack.Reset();
    ConstructionUndoStack.Add(MoveTemp(Entry));
    if (ConstructionUndoStack.Num() > 64) ConstructionUndoStack.RemoveAt(0);
}
void UFFZSWorldSubsystem::RecordConstructionEdit(const FFFZSCommand& Command, const FFFZSFacilityInstanceSnapshot& Before)
{
    if (bIsReplayingConstruction || bIsConstructionBatch || !IsPreparation(CurrentPhase)) return;
    FFFZSConstructionHistoryEntry Entry;
    Entry.Commands.Add(Command);
    if (Before.FacilityInstanceId.IsValid()) Entry.Before.Add(Before.FacilityInstanceId, Before);
    if (const AFFZSFacilityActor* Facility = FindFacility(Command.FacilityInstanceId))
        Entry.After.Add(Command.FacilityInstanceId, Facility->CaptureInstanceSnapshot());
    PushConstructionHistory(MoveTemp(Entry));
}
bool UFFZSWorldSubsystem::GetConstructionReplaySource(FGuid CommandId, FGuid& OutOriginalCommandId, bool& bOutUndo) const
{
    OutOriginalCommandId.Invalidate();
    bOutUndo = false;
    if (!bIsReplayingConstruction) return false;
    const FGuid* Source = ConstructionReplaySources.Find(CommandId);
    if (!Source) return false;
    OutOriginalCommandId = *Source;
    bOutUndo = bConstructionReplayUndo;
    return true;
}
bool UFFZSWorldSubsystem::UndoConstruction(FText& OutFailureReason)
{
    return ReplayConstructionHistory(true, OutFailureReason);
}
bool UFFZSWorldSubsystem::RedoConstruction(FText& OutFailureReason)
{
    return ReplayConstructionHistory(false, OutFailureReason);
}
bool UFFZSWorldSubsystem::ReplayConstructionHistory(bool bUndo, FText& OutFailureReason)
{
    OutFailureReason = FText::GetEmpty();
    if (IsAuthorityMutationBlocked() || !IsPreparation(CurrentPhase) || bIsReplayingConstruction)
    {
        OutFailureReason = LOCTEXT("HistoryPhase", "준비 시간에 다른 작업이 끝난 뒤 실행 취소·다시 실행할 수 있습니다.");
        return false;
    }
    TArray<FFFZSConstructionHistoryEntry>& Source = bUndo ? ConstructionUndoStack : ConstructionRedoStack;
    TArray<FFFZSConstructionHistoryEntry>& Destination = bUndo ? ConstructionRedoStack : ConstructionUndoStack;
    if (Source.IsEmpty())
    {
        OutFailureReason = LOCTEXT("HistoryEmpty", "되돌릴 건축 작업이 없습니다.");
        return false;
    }
    // Copy protects the entry from callbacks. Never capture or restore unrelated facilities.
    FFFZSConstructionHistoryEntry Entry = Source.Last();
    const auto& Expected = bUndo ? Entry.After : Entry.Before;
    const auto& Desired = bUndo ? Entry.Before : Entry.After;
    for (const FFFZSCommand& Original : Entry.Commands)
    {
        const AFFZSFacilityActor* Current = FindFacility(Original.FacilityInstanceId);
        const FFFZSFacilityInstanceSnapshot* ExpectedState = Expected.Find(Original.FacilityInstanceId);
        if (!!Current != !!ExpectedState)
        {
            OutFailureReason = LOCTEXT("HistoryChanged", "시설이 다른 작업으로 변경되어 이 기록을 되돌릴 수 없습니다.");
            return false;
        }
        if (Current)
        {
            FFFZSFacilityInstanceSnapshot CurrentState = Current->CaptureInstanceSnapshot();
            FFFZSFacilityInstanceSnapshot ExpectedCanonical = *ExpectedState;
            const auto NormalizeClosedQueue = [](FFFZSQueueSnapshot& Queue)
            {
                // Same canonicalization as RestoreSnapshot: a closed queue cannot admit
                // anyone regardless of whether an explicit drain was previously requested.
                Queue.bDrainRequested |= Queue.State == EFFZSQueueState::Closed || Queue.State == EFFZSQueueState::Draining;
            };
            NormalizeClosedQueue(CurrentState.Queue);
            NormalizeClosedQueue(ExpectedCanonical.Queue);
            if (!FFFZSFacilityInstanceSnapshot::StaticStruct()->CompareScriptStruct(&CurrentState, &ExpectedCanonical, 0))
            {
                OutFailureReason = LOCTEXT("HistoryChanged", "시설이 다른 작업으로 변경되어 이 기록을 되돌릴 수 없습니다.");
                return false;
            }
        }
    }
    TGuardValue<bool> ReplayGuard(bIsReplayingConstruction, true);
    TGuardValue<bool> DirectionGuard(bConstructionReplayUndo, bUndo);
    TArray<FFFZSCommand> Commands;
    const FGuid TransactionId = FGuid::NewGuid();
    for (const FFFZSCommand& Original : Entry.Commands)
    {
        FFFZSCommand Command;
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = TransactionId;
        Command.FacilityInstanceId = Original.FacilityInstanceId;
        Command.ExpectedWorldRevision = WorldRevision;
        const FFFZSFacilityInstanceSnapshot* Target = Desired.Find(Command.FacilityInstanceId);
        Command.Type = Target ? (Expected.Contains(Command.FacilityInstanceId) ? EFFZSCommandType::Move : EFFZSCommandType::Install)
            : EFFZSCommandType::Remove;
        if (Target)
        {
            Command.FacilitySpec = Target->FacilitySpec;
            Command.ZoneId = Target->ZoneId;
            Command.Placement = Target->Placement;
        }
        ConstructionReplaySources.Add(Command.CommandId, Original.CommandId);
        Commands.Add(MoveTemp(Command));
    }
    const FFFZSCommandResult Result = ProcessConstructionBatch(Commands, &Desired, false);
    ConstructionReplaySources.Reset();
    if (!Result.bSucceeded)
    {
        OutFailureReason = Result.Message;
        return false;
    }
    // Queue restoration normalizes closed/draining flags and placement validation may
    // resample ground height. The next replay must compare against the state actually
    // published, rather than mistaking this canonicalization for an unrelated edit.
    TMap<FGuid, FFFZSFacilityInstanceSnapshot>& RestoredSide = bUndo ? Entry.Before : Entry.After;
    RestoredSide.Reset();
    for (const FFFZSCommand& Original : Entry.Commands)
        if (const AFFZSFacilityActor* Restored = FindFacility(Original.FacilityInstanceId))
            RestoredSide.Add(Original.FacilityInstanceId, Restored->CaptureInstanceSnapshot());
    Source.Pop();
    Destination.Add(MoveTemp(Entry));
    return true;
}

bool UFFZSWorldSubsystem::ValidateConstructionBatch(const TArray<FFFZSCommand>& Commands, FText& OutFailureReason)
{
    const FFFZSCommandResult Result = ProcessConstructionBatch(Commands, nullptr, true);
    OutFailureReason = Result.bSucceeded ? FText::GetEmpty() : Result.Message;
    return Result.bSucceeded;
}
FFFZSCommandResult UFFZSWorldSubsystem::ExecuteConstructionBatch(const TArray<FFFZSCommand>& Commands)
{
    return ProcessConstructionBatch(Commands, nullptr, false);
}

FFFZSCommandResult UFFZSWorldSubsystem::ProcessConstructionBatch(const TArray<FFFZSCommand>& Commands,
    const TMap<FGuid, FFFZSFacilityInstanceSnapshot>* DesiredSnapshots, bool bPreviewOnly)
{
    if (IsAuthorityMutationBlocked() || bIsConstructionBatch)
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("BatchBusy", "다른 건축 작업이 진행 중입니다."));
    if (!IsPreparation(CurrentPhase))
        return MakeFailure(EFFZSPlacementFailure::WrongStage, LOCTEXT("BatchPhase", "준비 시간에만 건축을 편집할 수 있습니다."));
    if (Commands.IsEmpty() || Commands.Num() > 128)
        return MakeFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("BatchSize", "한 번에 1~128개 시설을 편집할 수 있습니다."));
    if (WorldRevision > MAX_int64 - Commands.Num() - 1)
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("BatchRevision", "건축 기록 번호가 가득 찼습니다."));

    TSet<FGuid> Ids, CommandIds;
    int32 ReplayedCount = 0;
    FFFZSCommandResult ReplayResult;
    for (const FFFZSCommand& Command : Commands)
    {
        if (!IsConstruction(Command.Type) || (Command.bChangeQueueLayout && Command.Type != EFFZSCommandType::Move)
            || !Command.CommandId.IsValid() || !Command.TransactionId.IsValid()
            || !Command.FacilityInstanceId.IsValid() || Ids.Contains(Command.FacilityInstanceId)
            || CommandIds.Contains(Command.CommandId) || Command.TransactionId != Commands[0].TransactionId
            || !UFFZSGridLibrary::HasSafeFacilitySpecCardinality(Command.FacilitySpec))
            return MakeFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("BatchInvalid", "그룹 건축 명령의 ID·종류·시설 데이터가 올바르지 않습니다."));
        Ids.Add(Command.FacilityInstanceId);
        CommandIds.Add(Command.CommandId);
        if (const FFFZSCommandReceipt* Receipt = CommandReceipts.Find(Command.CommandId))
        {
            if (Receipt->PayloadFingerprint != UFFZSGridLibrary::BuildCommandFingerprint(Command) || !Receipt->Result.bSucceeded)
                return MakeFailure(EFFZSPlacementFailure::DuplicatePayloadMismatch, LOCTEXT("BatchReused", "이미 사용한 건축 명령 ID입니다."));
            ReplayResult = Receipt->Result;
            ReplayResult.FacilityActor = FindFacility(Command.FacilityInstanceId);
            ReplayResult.bWasDuplicateReplay = true;
            ++ReplayedCount;
        }
    }
    if (ReplayedCount == Commands.Num()) return ReplayResult;
    if (ReplayedCount != 0)
        return MakeFailure(EFFZSPlacementFailure::DuplicatePayloadMismatch, LOCTEXT("BatchPartialReplay", "그룹 작업 일부만 다시 요청할 수 없습니다."));

    TArray<FText> IntegrationErrors;
    if (GetDefault<UFFZSDeveloperSettings>()->bRequireProductionPolicyDomains && !ValidateIntegrationReadiness(IntegrationErrors, true))
        return MakeFailure(EFFZSPlacementFailure::ExternalPolicyRejected, IntegrationErrors[0]);
    TGuardValue<bool> BatchGuard(bIsConstructionBatch, true);
    TGuardValue<FGuid> CommandGuard(InFlightCommandId, Commands[0].CommandId);
    TGuardValue<TSet<FGuid>> IgnoreGuard(ConstructionIgnoredFacilities, Ids);
    FFFZSConstructionHistoryEntry History;
    History.Commands = Commands;
    TArray<FFFZSPolicyContext> Contexts;
    TMap<FGuid, FFFZSFacilityInstanceSnapshot> Targets;
    TArray<FFFZSFacilityRegionOccupancy> CandidateOccupancies;
    TMap<FName, int32> AdditionalCounts;

    for (const FFFZSCommand& Command : Commands)
    {
        if (Command.ExpectedWorldRevision >= 0 && Command.ExpectedWorldRevision != WorldRevision)
            return MakeFailure(EFFZSPlacementFailure::StaleWorldRevision, LOCTEXT("BatchStale", "배치가 변경되었습니다. 다시 시도하십시오."));
        AFFZSFacilityActor* Existing = FindFacility(Command.FacilityInstanceId);
        if ((Command.Type == EFFZSCommandType::Install) == (Existing != nullptr))
            return MakeFailure(EFFZSPlacementFailure::FacilityNotFound, LOCTEXT("BatchMissing", "그룹에 포함된 시설이 없거나 ID가 겹칩니다."));
        if (Existing)
        {
            if (!Existing->CanMoveOrRemove() || !Existing->GetQueueComponent() || Existing->GetQueueComponent()->IsOccupied())
                return MakeFailure(EFFZSPlacementFailure::FacilityOccupied, LOCTEXT("BatchOccupied", "모든 시설을 닫고 손님이 나간 뒤 편집하십시오."));
            if (DoesGroundSurfaceSupportFacility(*Existing))
                return MakeFailure(EFFZSPlacementFailure::FacilityOccupied, LOCTEXT("BatchSupports", "타일 위 시설도 함께 선택하거나 먼저 옮겨야 합니다."));
            History.Before.Add(Command.FacilityInstanceId, Existing->CaptureInstanceSnapshot());
        }
        FFFZSPolicyContext Context;
        Context.Command = Command;
        Context.FacilityActor = Existing;
        Context.CurrentWorldRevision = WorldRevision;
        if (Command.Type != EFFZSCommandType::Remove)
        {
            FFFZSFacilityInstanceSnapshot Target;
            if (DesiredSnapshots)
            {
                const auto* Saved = DesiredSnapshots->Find(Command.FacilityInstanceId);
                if (!Saved) return MakeFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("BatchTarget", "복구할 시설 정보가 없습니다."));
                Target = *Saved;
            }
            else if (Existing) Target = Existing->CaptureInstanceSnapshot();
            else
            {
                Target.FacilityInstanceId = Command.FacilityInstanceId;
                Target.FacilitySpec = Command.FacilitySpec;
                Target.State = EFFZSFacilityState::Validated;
                Target.Queue.State = EFFZSQueueState::Closed;
            }
            if (!DesiredSnapshots && Command.bChangeQueueLayout) Target.FacilitySpec.QueueLayout = Command.QueueLayout;
            Target.ZoneId = Command.ZoneId;
            Target.Placement = Command.Placement;
            UFFZSGridLibrary::NormalizePlacement(Target.Placement);
            if (!DesiredSnapshots) Target.State = EFFZSFacilityState::Validated;
#if WITH_DEV_AUTOMATION_TESTS
            const bool bFixture = GIsAutomationTesting;
#else
            constexpr bool bFixture = false;
#endif
            if (Command.Type == EFFZSCommandType::Install && !DesiredSnapshots && !bFixture
                && !UFFZSFacilityDefinition::DoesSpecMatchLoadedDefinition(Target.FacilitySpec))
                return MakeFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("BatchDefinition", "현재 시설 정의와 설치 데이터가 다릅니다."));
            FFFZSCommand Candidate = Command;
            Candidate.FacilitySpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Target.FacilitySpec, Target.UpgradeLevel, Target.UpgradeSizeMultiplier);
            if (Command.Type == EFFZSCommandType::Install && Candidate.FacilitySpec.MaxInstancesPerWorld > 0)
            {
                const int32 Count = CountFacilitiesByDefinitionId(Candidate.FacilitySpec.FacilityDefinitionId)
                    + ++AdditionalCounts.FindOrAdd(Candidate.FacilitySpec.FacilityDefinitionId);
                if (Count > Candidate.FacilitySpec.MaxInstancesPerWorld)
                    return MakeFailure(EFFZSPlacementFailure::BuildLimitReached, LOCTEXT("BatchLimit", "그룹 전체를 설치하면 시설 건축 한도를 초과합니다."));
            }
            Context.Validation = ValidatePlacementLocal(Candidate, Existing);
            if (!Context.Validation.bIsValid)
                return MakeFailure(Context.Validation.Reasons.IsEmpty() ? EFFZSPlacementFailure::DataInvalid : Context.Validation.Reasons[0], Context.Validation.Summary);
            Target.WorldTransform = Context.Validation.PlacementTransform;
            FFFZSFacilityRegionOccupancy Occupancy;
            Occupancy.ZoneId = Command.ZoneId;
            Occupancy.bIsGroundSurface = Candidate.FacilitySpec.IsGroundSurface();
            Occupancy.bAllowsFacilitiesOnTop = Candidate.FacilitySpec.AllowsFacilityPlacementOnTop();
            Occupancy.WorldRegions = ToWorldRegions(Command.ZoneId, Context.Validation.ReservedOrientedRects);
            for (const auto& Other : CandidateOccupancies)
                if (IsOccupancyRelevant(Command.ZoneId, Other) && Other.BlocksLayer(Occupancy.bIsGroundSurface, Occupancy.bAllowsFacilitiesOnTop)
                    && UFFZSGridLibrary::DoOrientedRectSetsOverlap(Other.WorldRegions, Occupancy.WorldRegions))
                    return MakeFailure(EFFZSPlacementFailure::PlacementCollision, LOCTEXT("BatchOverlap", "이동 후 그룹 내부 시설이나 대기 공간이 서로 겹칩니다."));
            CandidateOccupancies.Add(MoveTemp(Occupancy));
            Targets.Add(Command.FacilityInstanceId, MoveTemp(Target));
            if (Command.Type == EFFZSCommandType::Move) Context.Command.FacilitySpec = Candidate.FacilitySpec;
        }
        Contexts.Add(MoveTemp(Context));
    }
    // A group may contain a paving tile and facilities above it. Use the final tile position,
    // never its old location, to lift upper facilities before policy and navigation checks.
    for (FFFZSPolicyContext& Context : Contexts)
    {
        FFFZSFacilityInstanceSnapshot* Target = Targets.Find(Context.Command.FacilityInstanceId);
        if (Target)
        {
            const FFFZSFacilitySpec Spec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
                Target->FacilitySpec, Target->UpgradeLevel, Target->UpgradeSizeMultiplier);
            if (!Spec.IsGroundSurface())
            {
                FVector Position = Context.Validation.PlacementTransform.GetLocation();
                const TArray<FFFZSOrientedRect> WorldFootprint = ToWorldRegions(Target->ZoneId, Context.Validation.FootprintOrientedRects);
                for (const FFFZSPolicyContext& SurfaceContext : Contexts)
                {
                    const auto* Surface = Targets.Find(SurfaceContext.Command.FacilityInstanceId);
                    if (!Surface) continue;
                    const FFFZSFacilitySpec SurfaceSpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
                        Surface->FacilitySpec, Surface->UpgradeLevel, Surface->UpgradeSizeMultiplier);
                    FFFZSFacilityRegionOccupancy SurfaceOccupancy;
                    SurfaceOccupancy.ZoneId = Surface->ZoneId;
                    if (SurfaceSpec.IsGroundSurface() && SurfaceSpec.AllowsFacilityPlacementOnTop()
                        && IsOccupancyRelevant(Target->ZoneId, SurfaceOccupancy)
                        && UFFZSGridLibrary::DoOrientedRectSetsOverlap(WorldFootprint,
                            ToWorldRegions(Surface->ZoneId, SurfaceContext.Validation.ReservedOrientedRects)))
                        Position.Z = FMath::Max(Position.Z, Surface->WorldTransform.GetLocation().Z + SurfaceSpec.PlacementObstacleHeightCm);
                }
                if (!Position.Equals(Context.Validation.PlacementTransform.GetLocation()))
                {
                    Context.Validation.PlacementTransform.SetLocation(Position);
                    Target->WorldTransform = Context.Validation.PlacementTransform;
                    if (const AFFZSGameplayZone* Zone = FindZone(Target->ZoneId))
                        ValidateWorldCollision(*Zone, Spec, Context.FacilityActor, Context.Validation);
                    ValidateNavigation(Spec, Context.Validation);
                    if (!Context.Validation.bIsValid)
                        return MakeFailure(Context.Validation.Reasons[0], Context.Validation.Summary);
                }
            }
        }
    }
    TGuardValue<const TArray<FFFZSPolicyContext>*> ContextGuard(ConstructionPolicyContexts, &Contexts);
    for (const FFFZSPolicyContext& Context : Contexts)
    {
        FFFZSPolicyDecision Decision;
        if (!RunPolicyPreValidation(Context, Decision))
            return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason, Decision.Message);
    }
    if (bPreviewOnly)
    {
        FFFZSCommandResult Result;
        Result.bSucceeded = true;
        Result.WorldRevision = WorldRevision;
        return Result;
    }

    // Prepare replacement actors without changing the authoritative registry. Moves only replace selected actors.
    TMap<FGuid, AFFZSFacilityActor*> Staged;
    auto DestroyStaged = [&]() { for (auto& Pair : Staged) if (IsValid(Pair.Value)) Pair.Value->Destroy(); };
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const auto& Pair : Targets)
    {
        const auto& Saved = Pair.Value;
        AFFZSFacilityActor* Facility = GetWorld()->SpawnActor<AFFZSFacilityActor>(AFFZSFacilityActor::StaticClass(), Saved.WorldTransform, SpawnParameters);
        if (!Facility)
        {
            DestroyStaged();
            return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("BatchSpawn", "그룹 시설을 준비하지 못했습니다. 아무것도 변경하지 않았습니다."));
        }
        Staged.Add(Pair.Key, Facility);
        Facility->SetActorHiddenInGame(true);
        Facility->SetActorEnableCollision(false);
        TArray<FText> Errors;
        if (!Facility->InitializeFacility(Saved.FacilityInstanceId, Saved.FacilitySpec, Saved.ZoneId, Saved.Placement, Saved.WorldTransform,
                Saved.State, Saved.UpgradeLevel, Saved.UpgradeServiceDurationMultiplier, Saved.UpgradeSizeMultiplier, Saved.UpgradeRevenueMultiplier)
            || !Facility->GetMutableQueueComponent()->RestoreSnapshot(Saved.Queue, Errors))
        {
            DestroyStaged();
            return MakeFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("BatchInitialize", "그룹 시설 정보를 복원하지 못했습니다. 아무것도 변경하지 않았습니다."));
        }
        Facility->SetNeedsRelocation(Saved.bNeedsRelocation);
        Facility->ApplyFacilityLightingOverride(Saved.LightingState, Saved.bHasLightingOverride);
    }

    TArray<TArray<TWeakObjectPtr<UObject>>> Policies;
    Policies.SetNum(Contexts.Num());
    for (int32 Index = 0; Index < Contexts.Num(); ++Index)
    {
        FFFZSPolicyDecision Decision;
        if (!ApplyTentativePolicies(Contexts[Index], Policies[Index], Decision))
        {
            // ApplyTentativePolicies already rolls back this failing member.
            for (int32 Previous = Index - 1; Previous >= 0; --Previous) RollbackPolicies(Contexts[Previous], Policies[Previous]);
            DestroyStaged();
            return MakeFailure(Decision.FailureReason == EFFZSPlacementFailure::None ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason, Decision.Message);
        }
    }
    TMap<FGuid, AFFZSFacilityActor*> Originals;
    for (const FFFZSCommand& Command : Commands)
        if (AFFZSFacilityActor* Existing = FindFacility(Command.FacilityInstanceId))
        {
            Originals.Add(Command.FacilityInstanceId, Existing);
            UnregisterFacility(Existing);
        }
    bool bRegistered = true;
    for (auto& Pair : Staged) if (!RegisterFacility(Pair.Value)) { bRegistered = false; break; }
    if (!bRegistered)
    {
        for (auto& Pair : Staged) UnregisterFacility(Pair.Value);
        for (auto& Pair : Originals) ensure(RegisterFacility(Pair.Value));
        for (int32 Index = Contexts.Num() - 1; Index >= 0; --Index) RollbackPolicies(Contexts[Index], Policies[Index]);
        DestroyStaged();
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("BatchRegister", "그룹 등록에 실패하여 원래 배치를 복구했습니다."));
    }
    for (auto& Pair : Staged)
    {
        Pair.Value->SetActorEnableCollision(true);
        Pair.Value->SetActorHiddenInGame(false);
    }
    TArray<FFFZSWorldChangeEvent> Events;
    FFFZSCommandResult Result;
    for (int32 Index = 0; Index < Commands.Num(); ++Index)
    {
        const auto& Command = Commands[Index];
        auto& Context = Contexts[Index];
        AFFZSFacilityActor* Current = FindFacility(Command.FacilityInstanceId);
        if (Current) Context.FacilityActor = Current;
        else if (AFFZSFacilityActor* Removed = Originals.FindRef(Command.FacilityInstanceId)) Removed->MarkRemoved();
        CommitPolicies(Context, Policies[Index]);
        const auto* Before = History.Before.Find(Command.FacilityInstanceId);
        Result = FinalizeSuccess(Command, Current, Before ? Before->ZoneId : NAME_None);
        if (DeferredWorldChangeEvent.IsSet()) Events.Add(DeferredWorldChangeEvent.GetValue());
        DeferredWorldChangeEvent.Reset();
        FFFZSCommandReceipt Receipt;
        Receipt.PayloadFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
        Receipt.FacilityInstanceId = Command.FacilityInstanceId;
        Receipt.Result = Result;
        Receipt.Result.FacilityActor = nullptr;
        CommandReceipts.Add(Command.CommandId, MoveTemp(Receipt));
        if (Current)
        {
            CommandReceiptFacilityReferences.Add(Command.CommandId);
            History.After.Add(Command.FacilityInstanceId, Current->CaptureInstanceSnapshot());
        }
    }
    for (auto& Pair : Originals) Pair.Value->Destroy();
    PushConstructionHistory(MoveTemp(History));
    // Every observer sees the entire final group and all completed policy commits.
    for (const auto& Event : Events) OnWorldChanged.Broadcast(Event);
    return Result;
}

#undef LOCTEXT_NAMESPACE
