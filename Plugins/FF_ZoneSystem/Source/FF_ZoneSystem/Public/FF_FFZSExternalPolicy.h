// FF_FFZSExternalPolicy.h

//내비게이션·경제·시간·UI·안전·경험 시스템을 배치 명령 트랜잭션에 연결하는 인터페이스다.
// 흐름: PreValidate → ApplyTentative → 로컬 변경 → Commit 순서이며 실패 시 적용 목록을 역순 Rollback한다.
// 불변식: Commit은 실패하지 않아야 하고 Rollback은 부분 적용 뒤에도 여러 번 호출 가능해야 한다.
// 실패/성능: 정책 콜백 중 권한 변경은 fail-closed로 거부되므로 콜백은 짧고 동기적으로 끝내며 긴 I/O를 하지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSExternalPolicy.generated.h"

UINTERFACE(BlueprintType)
class FF_ZONESYSTEM_API UFFZSExternalPolicy : public UInterface
{
    GENERATED_BODY()
};

// 외부 시스템이 FF_ZoneSystem 명령의 검증과 원자적 적용에 참여하는 계약.
// 등록 목록은 명령 시작 시 스냅샷되며 콜백 중 등록 변경이나 새 권한 명령은 거부된다.
// ApplyTentative가 실패한 정책도 부분 적용했을 수 있으므로 Rollback 대상에 포함된다.
 
class FF_ZONESYSTEM_API IFFZSExternalPolicy
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Policy")
    FName GetFFZoneSystemPolicyName() const;
    // 등록 전체에서 유일하고 저장·로그에서 안정적인 정책 이름을 반환한다. NAME_None은 유효하지 않다. 
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Policy")
    EFFZSPolicyDomain GetFFZoneSystemPolicyDomain() const;
    // 담당 협업 도메인을 반환한다. 기본 6개 도메인은 각각 하나만 등록할 수 있고 Custom은 여러 개를 허용.

   
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Policy")
    FFFZSPolicyDecision PreValidateFFZoneSystemChange(const FFFZSPolicyContext& Context) const;
    // 외부 상태를 바꾸지 않고 명령과 로컬 검증 결과를 읽어 허용 여부를 반환.
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Policy")
    FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange(const FFFZSPolicyContext& Context);
    // 실패 가능한 외부 예약을 임시 적용한다.
    // 실패를 반환한 구현도 부분 적용을 모두 되돌릴 수 있어야 한다.
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Policy")
    void CommitFFZoneSystemChange(const FFFZSPolicyContext& Context);
    // 실패하지 않는 최종 확정 단계. 실패 가능한 작업은 반드시 ApplyTentative에서 끝낸다. 
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Policy")
    void RollbackFFZoneSystemChange(const FFFZSPolicyContext& Context);
    // 적용된 정책의 역순으로 호출된다. 부분 적용 뒤와 중복 호출 모두 안전한 멱등 구현이어야 한다. 
};
