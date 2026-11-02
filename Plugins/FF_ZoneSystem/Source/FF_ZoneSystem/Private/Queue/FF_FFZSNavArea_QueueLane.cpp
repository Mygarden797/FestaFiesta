// FF_FFZSNavArea_QueueLane.cpp
// 역할: 대기열 통로의 내비게이션 이동 비용을 정의해 NPC가 대기 공간을 무작정 가로지르지 않게 한다.

#include "FF_FFZSNavArea_QueueLane.h"

UFFZSNavArea_QueueLane::UFFZSNavArea_QueueLane()
{
    DefaultCost = DefaultQueueLaneCost;

    // 진입 자체에 붙는 고정 비용은 쓰지 않는다. 레인을 가로질러 '짧게' 지나가는 것과
    // 복도처럼 '길게' 타고 가는 것을 구분하려면 거리 배수만 있어야 한다.
    // 고정 비용을 넣으면 1m 횡단과 10m 종단이 같은 벌점을 받는다.
    FixedAreaEnteringCost = 0.0f;

    // 에디터 Navigation 뷰(P키)에서 레인이 어디까지 잡혔는지 눈으로 확인하기 위한 색이다.
    DrawColor = FColor(255, 150, 40);
}
