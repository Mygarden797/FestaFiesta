// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ZoneSystemEconomyBridgeSubsystem.generated.h"

class UFF_FestivalProgressionSubsystem;
class UFFZSWorldSubsystem;

/**
 * FF_ZoneSystem의 시설 설치·철거 트랜잭션을 프로젝트 축제 자금과 연결한다.
 *
 * 설치비는 PreValidate/ApplyTentative에서 읽기 전용으로 검증하고, 시설 설치가 성공한 뒤
 * Commit에서만 차감한다. FFZS 명령은 Game Thread에서 동기·비재진입으로 실행되므로 별도
 * Gold hold를 만들지 않는다.
 *
 * 철거는 설치비의 UFF_FestivalDefinition::FacilityDemolitionRefundRate 만큼을 Commit에서
 * 환급한다. 환급은 잔액 검사가 필요 없어 실패할 수 없으므로 예약 단계를 두지 않는다.
 * Move와 시설 수명주기 명령은 여전히 과금·환급하지 않는다.
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

	/** 철거가 확정된 시설의 환급을 지급한다. 잔액 검사가 없어 실패하지 않는다. */
	void CommitDemolitionRefund(const FFFZSPolicyContext& Context);

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	// ApplyTentative는 자금을 바꾸지 않고 Commit에 필요한 불변 값만 CommandId별로 보관한다.
	TMap<FGuid, int32> PendingInstallCosts;
	TMap<FGuid, FName> PendingMatchedCostRows;
	TMap<FGuid, int32> PendingValidatedGold;
	TMap<FGuid, FString> PendingCostTablePaths;
	bool bPolicyRegistered = false;
};
