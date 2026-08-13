// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ZoneSystemEconomyBridgeSubsystem.generated.h"

class UFF_FestivalProgressionSubsystem;
class UFFZSWorldSubsystem;

/**
 * FF_ZoneSystem의 시설 설치 트랜잭션을 프로젝트 축제 자금과 연결한다.
 *
 * 설치비는 PreValidate/ApplyTentative에서 읽기 전용으로 검증하고, 시설 설치가 성공한 뒤
 * Commit에서만 차감한다. FFZS 명령은 Game Thread에서 동기·비재진입으로 실행되므로 별도
 * Gold hold를 만들지 않으며, Move/Remove와 시설 수명주기 명령은 과금하지 않는다.
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

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	// ApplyTentative는 자금을 바꾸지 않고 Commit에 필요한 불변 값만 CommandId별로 보관한다.
	TMap<FGuid, int32> PendingInstallCosts;
	TMap<FGuid, FName> PendingMatchedCostRows;
	TMap<FGuid, int32> PendingValidatedGold;
	TMap<FGuid, FString> PendingCostTablePaths;
	bool bPolicyRegistered = false;
};
