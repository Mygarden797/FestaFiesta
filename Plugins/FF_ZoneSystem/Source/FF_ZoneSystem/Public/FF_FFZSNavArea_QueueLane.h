// FF_FFZSNavArea_QueueLane.h

// 대기열 레인을 덮는 NavMesh 영역 정의다. 통행을 막지 않고 '비싸게' 만든다.
//
// ─── 왜 NavArea_Null이 아닌가 ──────────────────────────────────────────────────
// 통행을 끊는 가장 단순한 방법은 NavArea_Null이지만, 그러면 그 자리의 NavMesh가 사라진다.
// UFFZSWorldSubsystem::ValidateNavigation은 최대 대기 팀 수에 해당하는 모든 Queue 슬롯을 NavMesh에
// 투영하고 Entry에서 각 지점까지 완전한 경로가 있는지 확인한다. 레인을 도려내면 그 시설은
// 영원히 배치 검증을 통과하지 못한다 — 대기열이 갈 수 없는 곳이 되어 버리기 때문이다.
//
// 비용만 올리면 폴리곤이 그대로 남는다. 투영도 성공하고 경로 연결성도 유지되므로 검증은
// 통과하고, A*는 같은 거리라면 비용이 낮은 우회로를 먼저 고른다.
//
// ─── 왜 대기자 본인은 방해받지 않는가 ─────────────────────────────────────────
// 비용은 경로 '계획'에만 작용한다. 슬롯이 코앞인 대기자는 경로 쿼리 자체를 건너뛰고 직선으로
// 조향하므로 이 영역을 지나든 말든 영향이 없다. 멀리서 걸어오는 통행 NPC만 우회한다.
//
// ─── 비용 값을 고르는 기준 ────────────────────────────────────────────────────
// DefaultCost는 이동 거리에 곱하는 배수다. 8이면 레인 1m를 지나는 것이 일반 지면 8m와 같다.
// 즉 "8m 미만의 우회로가 있으면 돌아간다". 레인을 복도처럼 종단하는 경우(가장 나쁜 경우)는
// 10m 레인이 80m로 계산되어 사실상 항상 우회한다.
// 너무 크게 잡으면 레인이 유일한 통로인 좁은 구역에서 경로가 기괴하게 돌아간다.
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"

#include "FF_FFZSNavArea_QueueLane.generated.h"

UCLASS()
class FF_ZONESYSTEM_API UFFZSNavArea_QueueLane : public UNavArea
{
    GENERATED_BODY()

public:
    UFFZSNavArea_QueueLane();

    // 생성자 기본값. Config로 덮어쓰지 않았을 때 쓰이며, 튜닝 근거는 위 주석에 있다.
    static constexpr float DefaultQueueLaneCost = 12.0f;
};
