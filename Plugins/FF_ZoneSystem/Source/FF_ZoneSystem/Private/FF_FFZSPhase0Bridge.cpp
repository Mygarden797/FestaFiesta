// FF_FFZSPhase0Bridge.cpp

// Phase 0 고정 월드 변경 응답과 슬롯 좌표를 구현한다.
 

#include "FF_FFZSPhase0Bridge.h"

#define LOCTEXT_NAMESPACE "FFZSPhase0Bridge"

TArray<FVector> UFFZSPhase0Bridge::GetPhase0SlotCoordinatesCm()
{
    // 상위 P0 문서의 60 m x 40 m 코어 구역 안에서 X축으로 균등 배치한 슬롯 중심이다.
    // Notion 문서에는 정확한 좌표가 없으므로 Phase 0 회의에서 값이 바뀌면 이 한 곳과
    // 계약 테스트의 기대값을 같은 변경 단위에서 갱신.
    return {
        FVector(1000.0, 2000.0, 0.0),
        FVector(3000.0, 2000.0, 0.0),
        FVector(5000.0, 2000.0, 0.0)
    };
}

FFFZSPhase0WorldChangeResponse UFFZSPhase0Bridge::RequestWorldChangePhase0(
    const FFFZSCommand& Request)
{
    // Phase 0의 목적은 하위 모듈을 먼저 출발시키는 것이다. 요청의 ID, Phase, 좌표,
    // Policy를 일부만 검사하면 "항상 성공" 계약이 깨지므로 의도적으로 읽지 않는다.
    (void)Request;

    FFFZSPhase0WorldChangeResponse Response;
    Response.bSucceeded = true;
    Response.bWorldStateChanged = false;
    Response.SlotCoordinatesCm = GetPhase0SlotCoordinatesCm();
    Response.Message = LOCTEXT(
        "AcceptedWithoutMutation",
        "Phase 0 스텁이 요청을 수락했습니다. 실제 월드 상태와 비용은 변경되지 않았습니다.");
    return Response;
}

#undef LOCTEXT_NAMESPACE
