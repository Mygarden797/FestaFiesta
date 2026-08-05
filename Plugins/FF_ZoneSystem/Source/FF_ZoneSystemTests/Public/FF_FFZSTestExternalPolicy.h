/**
 * @file FF_FFZSTestExternalPolicy.h
 * @brief 외부 Policy lifecycle과 callback 재진입 차단을 관찰하는 Editor 테스트 대역을 선언한다.
 *
 * 포인터는 테스트 World의 객체를 약하게 참조해 수명을 소유하지 않는다. 공개 counter와 bool은
 * 검증 결과를 읽기 위한 probe이며 Thread-safe telemetry가 아니다. Production 모듈에서 include하거나
 * 실제 Economy/Safety Adapter의 기반 클래스로 사용해서는 안 된다.
 */

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "FF_FFZSTestExternalPolicy.generated.h"

class AFFZSGameplayZone;
class AFFZSProtectedArea;
class UFFZSWorldSubsystem;

/** 임시 적용, Commit 및 Rollback의 전체 Policy 수명주기를 기록하는 테스트 대역이다. */
UCLASS()
class FF_ZONESYSTEMTESTS_API UFFZSTestExternalPolicy final : public UObject, public IFFZSExternalPolicy
{
    GENERATED_BODY()

public:
    /** Registry 정렬과 도메인 중복 검증에 쓰는 테스트 identity다. */
    FName PolicyName = TEXT("Test.Policy");
    EFFZSPolicyDomain PolicyDomain = EFFZSPolicyDomain::Custom;
    bool bAllowPreValidation = true;
    bool bAllowTentativeApply = true;
    /** true일 때 읽기 전용 PreValidate 안에서 아래 권위 API를 고의 호출한다. */
    bool bAttemptAuthorityMutationsOnPreValidate = false;

    // 약한 참조는 probe가 World/Actor 수명을 연장해 테스트 teardown을 방해하지 않게 한다.
    TWeakObjectPtr<UFFZSWorldSubsystem> MutationSubsystem;
    TWeakObjectPtr<AFFZSGameplayZone> ZoneToRegister;
    TWeakObjectPtr<AFFZSGameplayZone> ZoneToUnregister;
    TWeakObjectPtr<AFFZSProtectedArea> ProtectedAreaToRegister;
    TWeakObjectPtr<AFFZSProtectedArea> ProtectedAreaToUnregister;
    TWeakObjectPtr<UObject> PolicyToRegister;
    TWeakObjectPtr<UObject> PolicyToUnregister;

    // 각 결과 flag는 callback guard가 fail-closed했는지 Assert에서 직접 읽는 관찰값이다.
    mutable bool bRegisterZoneAttemptSucceeded = false;
    mutable bool bUnregisterZoneAttemptSucceeded = false;
    mutable bool bRegisterProtectedAreaAttemptSucceeded = false;
    mutable bool bUnregisterProtectedAreaAttemptSucceeded = false;
    mutable bool bSetPhaseAttemptSucceeded = false;
    mutable bool bRegisterPolicyAttemptSucceeded = false;
    mutable bool bUnregisterPolicyAttemptSucceeded = false;

    // Lifecycle counter는 실패 Policy 포함 여부와 exactly-once Commit/Rollback을 검증한다.
    mutable int32 PreValidateCount = 0;
    int32 TentativeApplyCount = 0;
    int32 CommitCount = 0;
    int32 RollbackCount = 0;

    virtual FName GetFFZoneSystemPolicyName_Implementation() const override;
    virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override;
    virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(
        const FFFZSPolicyContext& Context) const override;
    virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(
        const FFFZSPolicyContext& Context) override;
    virtual void CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
    virtual void RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;

private:
    FFFZSPolicyDecision MakeDecision(bool bAllowed, const TCHAR* Message) const;
};
