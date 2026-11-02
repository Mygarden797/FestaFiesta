// FF_FFZSTestExternalPolicy.cpp

// Policy callback 횟수와 실패·재진입 방어를 관찰하기 위한 테스트 대역 구현이다.
// 각 callback 횟수를 기록하고, PreValidate 중 권위 API 변경을 일부러
// 시도해 WorldSubsystem이 false를 반환하는지 확인한다. 실제 비용·Nav·안전 상태는 수정하지 않으며
// Production Policy 구현 예제가 아니다.


#include "FF_FFZSTestExternalPolicy.h"

#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

FName UFFZSTestExternalPolicy::GetFFZoneSystemPolicyName_Implementation() const
{
    return PolicyName;
}

EFFZSPolicyDomain UFFZSTestExternalPolicy::GetFFZoneSystemPolicyDomain_Implementation() const
{
    return PolicyDomain;
}

// PreValidate는 원래 읽기 전용이다. 선택적 mutation probe는 방어 코드가 거절하는지 확인할 때만 켠다.
FFFZSPolicyDecision UFFZSTestExternalPolicy::PreValidateFFZoneSystemChange_Implementation(
    const FFFZSPolicyContext& Context) const
{
    ++PreValidateCount;
    if (bAttemptAuthorityMutationsOnPreValidate)
    {
        if (UFFZSWorldSubsystem* Subsystem = MutationSubsystem.Get())
        {
            bRegisterZoneAttemptSucceeded = Subsystem->RegisterZone(ZoneToRegister.Get());
            bUnregisterZoneAttemptSucceeded = Subsystem->UnregisterZone(ZoneToUnregister.Get());
            bRegisterProtectedAreaAttemptSucceeded = Subsystem->RegisterProtectedArea(ProtectedAreaToRegister.Get());
            bUnregisterProtectedAreaAttemptSucceeded = Subsystem->UnregisterProtectedArea(ProtectedAreaToUnregister.Get());
            bSetPhaseAttemptSucceeded = Subsystem->SetChangePhase(EFFZSChangePhase::LiveOperation);
            bRegisterPolicyAttemptSucceeded = Subsystem->RegisterExternalPolicy(PolicyToRegister.Get());
            bUnregisterPolicyAttemptSucceeded = Subsystem->UnregisterExternalPolicy(PolicyToUnregister.Get());
        }
    }
    return MakeDecision(bAllowPreValidation, TEXT("Policy가 사전 검증을 거부했습니다."));
}

// 실패를 허용하는 유일한 적용 단계다. count는 실패 Policy도 이후 Rollback되는지 판정하는 증거다.
FFFZSPolicyDecision UFFZSTestExternalPolicy::ApplyTentativeFFZoneSystemChange_Implementation(
    const FFFZSPolicyContext& Context)
{
    ++TentativeApplyCount;
    return MakeDecision(bAllowTentativeApply, TEXT("Policy가 임시 적용을 거부했습니다."));
}

// Commit은 실패하지 않는 최종 확정 신호이며 테스트 대역은 외부 상태 대신 호출 횟수만 기록한다.
void UFFZSTestExternalPolicy::CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
    ++CommitCount;
}

// Rollback counter는 거절을 반환한 Policy도 부분 적용 가능성 때문에 보상 대상에 포함됐는지 확인한다.
void UFFZSTestExternalPolicy::RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
    ++RollbackCount;
}

FFFZSPolicyDecision UFFZSTestExternalPolicy::MakeDecision(
    const bool bAllowed,
    const TCHAR* Message) const
{
    FFFZSPolicyDecision Decision;
    Decision.bAllowed = bAllowed;
    Decision.FailureReason =
        bAllowed ? EFFZSPlacementFailure::None : EFFZSPlacementFailure::ExternalPolicyRejected;
    if (!bAllowed)
    {
        Decision.Message = FText::FromString(Message);
    }
    return Decision;
}
