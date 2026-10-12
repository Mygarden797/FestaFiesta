// FFZALayerPolicy.cpp
// 역할: 정책 콜백을 서브시스템 판정 함수로 전달한다. 외부 상태를 예약하지 않으므로 Commit/Rollback은 비어 있다.
#include "FFZALayerPolicy.h"

#include "FFZAZoneAuthoringSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZALayerPolicy)

const FName UFFZALayerPolicy::PolicyName(TEXT("FFZoneAuthoring.PlotDistrictLayers"));

FName UFFZALayerPolicy::GetFFZoneSystemPolicyName_Implementation() const
{
    return PolicyName;
}

EFFZSPolicyDomain UFFZALayerPolicy::GetFFZoneSystemPolicyDomain_Implementation() const
{
    // Custom 도메인은 여러 개 등록할 수 있고 bRequireProductionPolicyDomains 검사 대상도 아니다.
    return EFFZSPolicyDomain::Custom;
}

FFFZSPolicyDecision UFFZALayerPolicy::PreValidateFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) const
{
    const UFFZAZoneAuthoringSubsystem* Subsystem = Owner.Get();
    return Subsystem ? Subsystem->EvaluatePlacement(Context) : FFFZSPolicyDecision();
}

FFFZSPolicyDecision UFFZALayerPolicy::ApplyTentativeFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
    // 사전 검증과 실제 적용 사이에 구획이 잠길 수 있으므로 같은 판정을 한 번 더 한다(예약할 외부 상태는 없음).
    const UFFZAZoneAuthoringSubsystem* Subsystem = Owner.Get();
    return Subsystem ? Subsystem->EvaluatePlacement(Context) : FFFZSPolicyDecision();
}

void UFFZALayerPolicy::CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
}

void UFFZALayerPolicy::RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
}
