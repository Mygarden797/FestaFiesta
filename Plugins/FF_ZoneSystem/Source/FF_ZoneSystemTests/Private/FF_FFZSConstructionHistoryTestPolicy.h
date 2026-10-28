#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "FF_FFZSConstructionHistoryTestPolicy.generated.h"

class UFFZSWorldSubsystem;

// History-only probes: reject a late member and end a stroke from a policy callback.
UCLASS()
class UFFZSConstructionHistoryTestPolicy final : public UObject, public IFFZSExternalPolicy
{
    GENERATED_BODY()

public:
    TWeakObjectPtr<UFFZSWorldSubsystem> Zones;
    FGuid StrokeToEndOnPreValidate;
    int32 RejectTentativeCall = INDEX_NONE;
    int32 TentativeCalls = 0;
    int32 CommitCalls = 0;
    int32 RollbackCalls = 0;
    mutable bool bCallbackBeginSucceeded = false;
    mutable int32 ObservedBatchSize = 0;
    mutable TSet<FGuid> UndoSources;
    mutable TSet<FGuid> RedoSources;

    virtual FName GetFFZoneSystemPolicyName_Implementation() const override;
    virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override;
    virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) const override;
    virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
    virtual void CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
    virtual void RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
};
