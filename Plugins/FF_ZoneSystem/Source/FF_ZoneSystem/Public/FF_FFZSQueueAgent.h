// FF_FFZSQueueAgent.h

// 현재 차단 구간에서 요청한 재경로 횟수다. MaxRepathAttempts를 넘지 않는다. 
// 구체 Character·Controller·StateTree에 의존하지 않는 대기열 에이전트 콜백 계약이다.
// 흐름: 참가 → 슬롯 이동 → 서비스 배정 → 도착 보고 → 서비스 → 출구 이동 → 출구 도착 보고 → 해제 순서다.
// 불변식: AgentId는 저장·재바인딩 동안 안정적이어야 하며 서비스 배정은 서비스 시작과 다르다.
// 실패/성능: 콜백은 동기적으로 호출될 수 있으므로 재진입 권한 변경과 긴 경로 계산은 피한다.

#pragma once

// 리플렉션 연결 코드는 UHT가 생성한다. 이동 콜백은 게임 측 AI가 구현하므로 빈 .cpp는 필요 없다.

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSQueueAgent.generated.h"

UINTERFACE(BlueprintType)
class FF_ZONESYSTEM_API UFFZSQueueAgent : public UInterface
{
    GENERATED_BODY()
};

//최소 NPC 계약. 플러그인은 구체 Character, Controller, StateTree에 의존하지 않는다.
// 구현체는 콜백에서 이동을 요청하고, 실제 도착 시 UFFZSWorldSubsystem의 Report 함수를 호출해야 한다.
 
class FF_ZONESYSTEM_API IFFZSQueueAgent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Queue Agent")
    FName GetFFZoneSystemAgentId() const;
    // 저장·재바인딩·전역 중복 검사에 쓰는 비어 있지 않은 안정 ID를 반환한다. 액터 이름에 의존하지 않는다. 
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Queue Agent")
    void OnFFZoneSystemQueueJoined(FGuid FacilityInstanceId, FGuid TicketId, int32 QueueIndex);
    // 참가가 확정됐음을 알리고 현재 QueueIndex 슬롯으로 이동하도록 요청한다. 
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Queue Agent")
    void OnFFZoneSystemQueueIndexChanged(FGuid FacilityInstanceId, FGuid TicketId, int32 QueueIndex);
    // FIFO 앞당김 등으로 슬롯 번호가 바뀌었음을 알린다. 새 슬롯 도착을 별도로 보고해야 한다. 
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Queue Agent")
    void OnFFZoneSystemServiceAssigned(FGuid FacilityInstanceId, FGuid TicketId, int32 ServiceIndex);
    // 지정 Service 앵커로 이동을 요청한다. 아직 서비스가 시작된 것은 아니다. 
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Queue Agent")
    void OnFFZoneSystemServiceStarted(FGuid FacilityInstanceId, int32 ServiceIndex);
    // ReportServicePointArrival이 지정 앵커 도착을 확인한 정확한 시점에 서비스 시작을 알린다. 
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Queue Agent")
    void OnFFZoneSystemExitRequested(FGuid FacilityInstanceId, int32 ExitIndex, EFFZSQueueReleaseReason Reason);
    // 서비스 종료나 취소 뒤 Exit 앵커로 이동을 요청한다. 이 동안에도 전역 대기열 소유권은 유지된다. 
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FF Zone System|Queue Agent")
    void OnFFZoneSystemQueueReleased(FGuid FacilityInstanceId, EFFZSQueueReleaseReason Reason);
    // Exit 도착 확인 후 최종 소유권이 해제됐음을 알린다. 이후에만 다른 시설 대기열에 참가할 수 있다. 
};
