// FF_FacilityUpgradeTestPolicy.h
// 역할: 강화 실패 시 비용과 시설이 함께 원복되는지 검사하는 테스트 정책이다.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "FF_FacilityUpgradeTestPolicy.generated.h"

/** 강화 통합 테스트의 실패 주입·골드 알림 관찰자. 테스트가 직접 등록할 때만 정책으로 참여한다.
 * 도감·경제 테스트도 같은 알림 관찰자를 공유하므로 모듈 내부 Tests 경로에서 사용한다. */
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
