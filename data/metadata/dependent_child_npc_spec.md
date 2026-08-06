# 종속 아동 NPC 메커닉 명세 (초안)

- 작성일: 2026-08-04
- 상태: 초안 — 사용자 확인 전, 최종본 아님
- 결정 배경: 2026-08-04 사용자가 생애주기 5종 + 동반유형 오버레이 + 아동 종속 NPC 구조를 확정함
- **2026-08-04 갱신 (이중 계산 문제 발견·수정)**: 이 문서의 3절(아래)이 원래 설명하던 "확률 0.4로 아동 추가" 메커닉은, 일반 인구 샘플러가 `age_10_or_under`(0~19세 통합)를 이미 `life_stage_teen`으로 독립 샘플링하고 있는 것과 겹쳐서 **아동이 이중으로 계산되는 문제**가 있었다. 해결책과 정확한 생성 순서는 `population_group_generation_spec.md` 2절로 옮겼다 — 아래 3절의 숫자(0.4, 1.5)는 더 이상 독립 노브가 아니라 그 문서의 "목표 아동 수"에 맞게 조정되는 파생값으로 재정의됐다. 이 문서는 계속 "종속 아동이 만들어진 뒤 어떻게 행동하는가"(2절, 4절)만 다룬다.

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

**2026-08-04 갱신 — 일반 규칙으로 확장됨**: "자체 Utility 계산 없이 부모 목적지 상속"은 `population_group_generation_spec.md` 1.5절에서 **그룹 전체의 목적지 결정 방식**(그룹 리더 1명만 Utility 계산, 나머지 전원 상속)으로 일반화됐다. 아동은 이제 그 일반 규칙의 특수 케이스일 뿐이다 — 아동을 특별하게 만드는 건 `destination_choice` 상속이 아니라 위 표의 나머지 3개(이동속도·주의집중·대기 인내 노브)뿐이다. 성인 그룹원(리더 아님)도 목적지는 상속받지만 이 3개 노브는 그대로 성인 기본값을 쓴다.

## 3. 발생 조건

- `companion_type = group_3_plus` 인 그룹(개체가 아니라 그룹 — `population_group_generation_spec.md` 1절) 중 일부에 아동을 배치한다.
- **생성 순서가 바뀌었다**: 아래 두 노브를 먼저 정하고 그 결과로 나오는 아동 수를 받아들이는 게 아니라, 목표 아동 수를 먼저 정하고 이 두 노브(또는 대체 배치 로직)가 그 목표에 맞게 조정된다. 아래 값은 그 조정의 **출발점**일 뿐이다.
- **2026-08-04 완료**: 목표 아동 수는 더 이상 추상적인 개념이 아니라 실제 값이 있다 — `Tools/data/build_npc_archetypes.mjs`를 고쳐서 원본 단일세 인구로 0~14세/15~19세를 분리했고, 그 결과가 `data/derived/dependent_child_population_target_v0.csv`다: `resident_neutral_v0` 시나리오는 방문객의 **7.9193%**, `datalab_observed_v0`는 **3.8925%**가 목표 아동 비율이다. 그룹 생성·아동 배치 코드를 짤 때는 이 값을 실제 입력으로 쓴다.

| 노브 | 기본값 | 범위 | 상태 |
|---|---|---|---|
| `child_attach_probability_given_group3plus` | 0.4000 | 0.1~0.7 | assumption — 목표 아동 수에 맞게 재조정되는 파생값 |
| `child_count_mean_given_attached` | 1.5000 | 1.0~3.0 | assumption — 목표 아동 수에 맞게 재조정되는 파생값 |

- 어느 생애주기 아키타입에 아동을 부착할지: `life_stage_30_40s`를 기본 대상으로 한다. Tour_Stat 연령별 활동표에서 "가족/친지/친구 방문" 응답률이 40대에서 13.2%로 전 연령 중 가장 높게 관측된 점(참고용, `not_applicable` 매핑이라 카테고리 점수에는 안 들어감)을 정성적 근거로 삼되, 이는 방문지 선택 이유가 아니라 활동 항목이며 "가족 동반"을 직접 증명하지는 않는다.

## 4. Unreal 구조 제안

- 아동은 별도 `FNpcArchetypeRow`를 만들지 않는다.
- **2026-08-04 갱신**: 목적지 상속 자체는 이제 아동 전용 로직이 아니라 `population_group_generation_spec.md` 1.5절의 **그룹 전체 규칙**(`GroupId`+`bIsGroupLeader`, 리더만 Utility 계산·나머지 상속)이 담당한다. `bIsDependent`/`DependentOfNpcId`는 목적지 상속용이 아니라 **아동 전용 3개 노브(이동속도·주의집중·대기인내)를 누구 기준으로 적용할지**만 남긴다 — 보통 `DependentOfNpcId` = 같은 그룹의 리더(부모) `StableAgentId`.
- NPC 스폰 시 `bIsDependent`, `DependentOfNpcId` 필드만 있는 경량 구조로 생성한다.
- Utility AI 목적지 선택 로직은 `bIsGroupLeader == false`인 개체 전체(아동뿐 아니라 성인 그룹원도)에 대해 스킵하고 리더의 `current_destination`을 그대로 복사한다. 그중 `bIsDependent == true`인 개체는 추가로 위 3개 노브를 적용한다.

## 5. v0에서 하지 않는 것

- 아동의 독립적 선호 벡터를 만들지 않는다.
- 아동 수·부착률에 대한 실측 검증을 하지 않는다 (근거 자료 없음).
- 아동이 부모와 분리되는 이벤트를 만들지 않는다.

## 6. 다음에 검증이 필요한 것

- 실제 축제 방문객 중 가족 동반(자녀 포함) 비율 — 실측 시 이 문서의 노브를 교체한다.
- 아동의 부스 유형별 체류시간 차이 — 별도 관찰조사나 유사 축제 사례가 필요하다.
