// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ProgramFacilityProtectionSubsystem.generated.h"

class UFFZSWorldSubsystem;

/**
 * 편성된 시설의 Move/Remove를 Zone 명령 최종 경계에서 막는 읽기 전용 정책.
 * BuildMode 외의 명령 발행자도 같은 불변식을 우회할 수 없게 한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ProgramFacilityProtectionSubsystem final
	: public UWorldSubsystem
	, public IFFZSExternalPolicy
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	virtual FName GetFFZoneSystemPolicyName_Implementation() const override;
	virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override;
	virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) const override;
	virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) override;
	virtual void CommitFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) override;
	virtual void RollbackFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	bool bPolicyRegistered = false;
};
