// FFZALayerPolicy.h
// 역할: FF_ZoneSystem의 공식 확장 지점(IFFZSExternalPolicy)에 구획/구역 규칙을 연결하는 어댑터.
//
// FF_ZoneSystem은 설치/이동 명령을 실행하기 전에 등록된 Policy에게 "허용?"을 묻는다.
// 이 Policy는 Custom 도메인으로 등록되어 다른 팀의 Economy/Safety 등 Policy와 충돌하지 않는다.
// 판정 로직은 UFFZAZoneAuthoringSubsystem::EvaluatePlacement 한 곳에만 있다.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FF_FFZSExternalPolicy.h"
#include "FFZALayerPolicy.generated.h"

class UFFZAZoneAuthoringSubsystem;

UCLASS(Transient)
class UFFZALayerPolicy final : public UObject, public IFFZSExternalPolicy
{
    GENERATED_BODY()

public:
    static const FName PolicyName;

    void SetOwner(UFFZAZoneAuthoringSubsystem* InOwner) { Owner = InOwner; }

    virtual FName GetFFZoneSystemPolicyName_Implementation() const override;
    virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override;
    virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) const override;
    virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
    virtual void CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
    virtual void RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;

private:
    TWeakObjectPtr<UFFZAZoneAuthoringSubsystem> Owner;
};
