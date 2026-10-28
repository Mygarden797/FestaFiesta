#include "FF_FFZSConstructionHistoryTestPolicy.h"
#include "FF_FFZSWorldSubsystem.h"

FName UFFZSConstructionHistoryTestPolicy::GetFFZoneSystemPolicyName_Implementation() const
{
    return TEXT("Test.ConstructionHistory");
}
EFFZSPolicyDomain UFFZSConstructionHistoryTestPolicy::GetFFZoneSystemPolicyDomain_Implementation() const
{
    return EFFZSPolicyDomain::Custom;
}
FFFZSPolicyDecision UFFZSConstructionHistoryTestPolicy::PreValidateFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) const
{
    if (UFFZSWorldSubsystem* System = Zones.Get())
    {
        if (StrokeToEndOnPreValidate.IsValid())
        {
            System->EndConstructionPlacementStroke(StrokeToEndOnPreValidate);
            bCallbackBeginSucceeded |= System->BeginConstructionPlacementStroke(FGuid::NewGuid());
        }
        if (const auto* Batch = System->GetConstructionBatchPolicyContexts()) ObservedBatchSize = Batch->Num();
        FGuid OriginalId;
        bool bUndo = false;
        if (System->GetConstructionReplaySource(Context.Command.CommandId, OriginalId, bUndo))
            (bUndo ? UndoSources : RedoSources).Add(OriginalId);
    }
    return {};
}
FFFZSPolicyDecision UFFZSConstructionHistoryTestPolicy::ApplyTentativeFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
    FFFZSPolicyDecision Decision;
    Decision.bAllowed = ++TentativeCalls != RejectTentativeCall;
    if (!Decision.bAllowed)
    {
        Decision.FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
        Decision.Message = FText::FromString(TEXT("History test rejects a late tentative member."));
    }
    return Decision;
}
void UFFZSConstructionHistoryTestPolicy::CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
    ++CommitCalls;
}
void UFFZSConstructionHistoryTestPolicy::RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
    ++RollbackCalls;
}
