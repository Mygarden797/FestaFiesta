#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "FF_FacilityUpgradeTestPolicy.generated.h"

/** Fault injection and notification observer for the facility upgrade integration tests. */
UCLASS()
class UFF_FacilityUpgradeTestPolicy final : public UObject, public IFFZSExternalPolicy
{
	GENERATED_BODY()
public:
	TFunction<void()> ObserveTentative;
	TFunction<void()> ObserveGoldChanged;
	int32 GoldNotificationCount = 0;
	int32 TentativeCallCount = 0;

	virtual FName GetFFZoneSystemPolicyName_Implementation() const override
	{
		return TEXT("ZZZ.FacilityUpgradeAutomation");
	}
	virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override
	{
		return EFFZSPolicyDomain::Custom;
	}
	virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(const FFFZSPolicyContext&) const override
	{
		return FFFZSPolicyDecision();
	}
	virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override
	{
		FFFZSPolicyDecision Decision;
		if (Context.Command.Type == EFFZSCommandType::Upgrade)
		{
			++TentativeCallCount;
			if (ObserveTentative) ObserveTentative();
			Decision.bAllowed = false;
			Decision.FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
			Decision.Message = FText::FromString(TEXT("Injected late upgrade rejection"));
		}
		return Decision;
	}
	virtual void CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext&) override {}
	virtual void RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext&) override {}

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue)
	{
		++GoldNotificationCount;
		if (ObserveGoldChanged) ObserveGoldChanged();
	}
};
