// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ZoneSystemEconomyBridgeSubsystem.generated.h"

class UFF_FestivalProgressionSubsystem;
class UFFZSWorldSubsystem;

struct FFF_ConstructionEconomyReceipt
{
	int32 InstallSpend = 0;
	int32 DemolitionRefund = 0;
};

/**
 * FF_ZoneSystem의 시설 설치·철거 트랜잭션을 프로젝트 축제 자금과 연결한다.
 *
 * 설치비는 PreValidate/ApplyTentative에서 읽기 전용으로 검증하고, 시설 설치가 성공한 뒤
 * Commit에서만 차감한다. 그룹 명령은 모든 시설의 예약 비용 합계를 검사한 뒤 확정한다.
 *
 * 철거는 설치비의 UFF_FestivalDefinition::FacilityDemolitionRefundRate 만큼을 Commit에서
 * 환급한다. 확정 영수증은 당시 실제 지출·환급액을 보관하며 실행 취소·다시 실행에 사용한다.
 * 복구는 다른 지출과 매출을 유지하고 건축 관련 장부 항목만 수정한다.
 * 일반 Move와 시설 수명주기 명령은 과금·환급하지 않는다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ZoneSystemEconomyBridgeSubsystem final
	: public UWorldSubsystem
	, public IFFZSExternalPolicy
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	// IFFZSExternalPolicy
	virtual FName GetFFZoneSystemPolicyName_Implementation() const override;
	virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override;
	virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) const override;
	virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) override;
	virtual void CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
	virtual void RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;

private:
	/** GameInstance가 WorldSubsystem보다 늦게 연결되는 테스트/PIE 수명을 위해 매 호출 시 조회한다. */
	UFF_FestivalProgressionSubsystem* ResolveFestivalProgression() const;

	/** 저장 복원이 설치 명령으로 시설을 되살리는 중이면 true. 그때는 설치비·환급을 건드리지 않는다. */
	bool IsRestoringSave() const;

	/** Install 명령의 비용 원천, 양수 비용, 현재 지불 가능 여부를 한 경로에서 검증한다. */
	FFFZSPolicyDecision ValidateInstallCost(
		const FFFZSPolicyContext& Context,
		int32& OutInstallCost,
		FName& OutMatchedCostRow,
		int32& OutValidatedGold,
		FString& OutCostTablePath) const;

	/** 철거 대상 시설의 설치비에 환급 비율을 적용한다. 비용을 조회할 수 없으면 false이며 철거는 막지 않는다. */
	bool ResolveDemolitionRefund(
		const FFFZSPolicyContext& Context,
		int32& OutRefund,
		FName& OutMatchedCostRow,
		FString& OutFailureReason) const;

	FFFZSPolicyDecision ResolveConstructionReceipt(const FFFZSPolicyContext& Context,
		FFF_ConstructionEconomyReceipt& OutReceipt) const;
	FFFZSPolicyDecision ValidateConstructionReceipt(const FFF_ConstructionEconomyReceipt& Receipt) const;
	FFFZSPolicyDecision ValidateConstructionTotals(int64 InstallSpend, int64 Refund) const;

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	// ApplyTentative는 자금을 바꾸지 않고 Commit에 필요한 불변 값만 CommandId별로 보관한다.
	TMap<FGuid, FFF_ConstructionEconomyReceipt> PendingReceipts;
	TMap<FGuid, FFF_ConstructionEconomyReceipt> CommittedReceipts;
	TOptional<int32> GoldBeforePendingCommits;
	bool bPolicyRegistered = false;
};
