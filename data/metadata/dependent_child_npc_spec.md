# 종속 아동 NPC 메커닉 명세 (초안)

- 작성일: 2026-08-04
- 상태: 초안 — 사용자 확인 전, 최종본 아님
- 결정 배경: 2026-08-04 사용자가 생애주기 5종 + 동반유형 오버레이 + 아동 종속 NPC 구조를 확정함

## 1. 왜 독립 아키타입으로 만들지 않는가

인구 데이터(`data/cleaned/population/`)의 `age_10_or_under` 세그먼트는 0~19세를 통째로 묶는다. 반면 행동 데이터(Tour_Stat 2018)는 **15~19세부터** 시작한다. 0~14세 아동의 독립적인 여행지 선택·활동 성향을 관측한 자료는 국민여행조사·데이터랩 어느 쪽에도 없다.

`data/derived/npc_archetypes_v0.csv`의 `life_stage_teen` 행은 15~19세 응답만을 대리값으로 쓰고 있으며, 그 메타데이터(`npc_archetypes_v0.json` `prohibited_uses`)에 "0~14세 아동에게 그대로 적용하지 않는다"고 명시했다. 이 명세가 그 공백을 메우는 설계다.

아동을 독립 아키타입으로 만들면 존재하지 않는 데이터를 있는 것처럼 다루게 된다. 대신 **성인 아키타입에 종속되는 개체**로 설계한다.

## 2. 종속 관계

```text
성인 NPC (life_stage_30_40s 아키타입, 주로)
  └─ dependent_child_count: 0~N (동반유형 group_3_plus 일 때만 1 이상)
       └─ 아동 NPC 1인당
            ├─ destination_choice: 부모의 선택을 그대로 상속 (자체 Utility 계산 없음)
            ├─ movement_speed_multiplier: 부모보다 낮음 (assumption, 노브)
            ├─ attention_span_minutes: 짧음 → 재평가 트리거를 부모보다 자주 발생 (assumption, 노브)
            └─ wait_tolerance_multiplier: 부모보다 낮음 (assumption, 노브)
```

아동이 부모와 다른 목적지를 원하는 상황(짜증, 이탈 시도)은 v0 범위 밖이다. v1 이후 확장 후보로 남긴다.

## 3. 발생 조건

- `companion_type = group_3_plus` 인 성인 NPC 개체 중 일부에 대해서만 아동을 부착한다.
- 부착 확률과 아동 수 분포는 실측이 없다. 다음을 임시 노브로 둔다.

| 노브 | 기본값 | 범위 | 상태 |
|---|---|---|---|
| `child_attach_probability_given_group3plus` | 0.4000 | 0.1~0.7 | assumption |
| `child_count_mean_given_attached` | 1.5000 | 1.0~3.0 | assumption |

- 어느 생애주기 아키타입에 아동을 부착할지: `life_stage_30_40s`를 기본 대상으로 한다. Tour_Stat 연령별 활동표에서 "가족/친지/친구 방문" 응답률이 40대에서 13.2%로 전 연령 중 가장 높게 관측된 점(참고용, `not_applicable` 매핑이라 카테고리 점수에는 안 들어감)을 정성적 근거로 삼되, 이는 방문지 선택 이유가 아니라 활동 항목이며 "가족 동반"을 직접 증명하지는 않는다.

## 4. Unreal 구조 제안

- 아동은 별도 `FNpcArchetypeRow`를 만들지 않는다.
- NPC 스폰 시 `bIsDependent`, `DependentOfNpcId` 필드만 있는 경량 구조로 생성한다.
- Utility AI 목적지 선택 로직은 `bIsDependent == true`인 개체에 대해 스킵하고 부모의 `current_destination`을 그대로 복사한다.

## 5. v0에서 하지 않는 것

- 아동의 독립적 선호 벡터를 만들지 않는다.
- 아동 수·부착률에 대한 실측 검증을 하지 않는다 (근거 자료 없음).
- 아동이 부모와 분리되는 이벤트를 만들지 않는다.

## 6. 다음에 검증이 필요한 것

- 실제 축제 방문객 중 가족 동반(자녀 포함) 비율 — 실측 시 이 문서의 노브를 교체한다.
- 아동의 부스 유형별 체류시간 차이 — 별도 관찰조사나 유사 축제 사례가 필요하다.
