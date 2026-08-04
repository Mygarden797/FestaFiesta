# 공공데이터 도입 결정 기록 — 2026-08-03

## 이번 단계의 결론

- 한국관광 데이터랩에서 사하구 관광지 13곳의 `성·연령 방문 집중률`은 확보했다.
- 같은 기간·같은 관광지 단위의 `관광지별 방문자 수`는 별도로 확보하지 못했다.
- 따라서 v0에서는 13개 관광지를 동일한 비중으로 취급한 기준분포를 사용한다.
- 이 결과는 사하구 전체 관광객 분포도, 실제 선택 효용계수도 아니다.
- NPC 아키타입 인구 비율로 직접 사용하지 않고 Utility AI의 선호도 사전값 후보로만 사용한다.

## 확정·가정·추후 검증

| 상태 | 내용 |
|---|---|
| 확정 | 원본에는 관광지 13곳과 성·연령 세그먼트 14개의 집중률이 있다. |
| 확정 | 산출표는 `13 × 14 = 182행`이며, 한 행은 `관광지 한 곳 × 성·연령 세그먼트 한 조합`이다. |
| 가정 | 관광지별 방문 규모를 알 수 없어 13개 관광지를 동일가중으로 평균했다. |
| 가정 | Utility AI에서는 `lambda_affinity × centered_log_share_ratio`로 연결하며, `lambda_affinity`는 민감도 실험용 노브다. |
| 추후 검증 | 데이터랩 집중률의 기준 모집단·통신사 추정 방식·표본 편향·이용조건을 확인한다. |
| 추후 검증 | 실제 방문 규모가 확보되면 동일가중 평균을 규모가중 평균 및 축소추정으로 교체한다. |

## 계산 방법

1. 각 성·연령 세그먼트에 대해 관광지 13곳의 집중률을 산술평균한다.
2. 관광지별 관측 집중률을 이 평균과 비교한다.
3. `log_share_ratio = ln(관측 집중률 / 동일가중 기준 집중률)`을 계산한다.
4. 관광지 안에서 14개 로그비의 평균을 빼 `centered_log_share_ratio`를 만든다.
5. 양수는 기준보다 상대적으로 많이 관측, 음수는 상대적으로 적게 관측됐다는 뜻이다.

## 파일과 데이터 계보

```text
data/raw/tourism/20260803201032_사하구_202507-202606_데이터랩_다운로드.zip
  → data/extracted/tourism/saha_datalab_2025_07_2026_06/*.csv
  → data/derived/affinity_prior_equal_site_v0.csv
  → 이후 검토를 거쳐 data/unreal/ Utility AI DataTable CSV
```

- CSV: 계산된 182행의 표형 값. Excel 검토와 향후 Unreal DataTable 변환에 사용한다.
- JSON: 출처, 버전, 수식, 가정, 경고를 기록한다.
- XLSX: Summary·Baseline·Affinity·Source 시트를 함께 둔 사람이 검토하기 쉬운 감사용 문서다.

## 인구 파일 — 해결됨

`0세` 열만 있던 첫 파일은 채택하지 않았고 `data/raw`에도 복사하지 않았다. 사용자가 연령 구간을 `0세~100세 이상`으로 고쳐 재다운로드한 파일을 검증 후 채택했다.

| 항목 | 값 |
|---|---|
| 등록 경로 | `data/raw/population/202607_202607_연령별인구현황_월간.csv` |
| 원본 파일명 | `202607_202607_연령별인구현황_월간 (1).csv` |
| SHA-256 | `00804C2EA3914335507DD79AE884D3168BA80C89023ECA2A6DC48F0C33D93AFE` |
| 크기 | 36,360 bytes |
| 구조 | 310열, 17행 (사하구 합계 1 + 행정동 16) |
| source_id | `population_mois_saha_age_sex_2026_07` |

검증 스크립트 `Tools/data/verify_population.mjs` 의 전 항목 통과: 연령 열 존재, 계·남·여 각 101개 연령 열, 연령 합 = 연령구간인구수, 남 + 여 = 계, 행정동 16개 합 = 사하구 합계 281,665 (차 0).

부수 확인: KOSIS 연앙인구 파일은 파일명과 달리 **전국과 17개 시도만** 수록하고 있어 사하구 행이 없다. `source_catalog.json` 의 해당 항목 용도를 "부산 전체와의 비교용"으로 낮췄다.

## 단계 A~D 수행 결과

### cleaned 인구표

- 산출: `data/cleaned/population/saha_population_age_sex_2026_07.csv` (3,434행)
- 한 행 = 한 지역 × 한 성별 × 한 살 나이
- 성별 `계` 행과 `gu`/`dong` 중복 합산을 막기 위해 `계`는 출력하지 않고 `region_level` 열로 수준을 구분했다.
- `age = 100` 은 `100세 이상` 개방구간이며 `age_is_open_interval` 로 표시했다.
- `segment_key` 를 데이터랩 14세그먼트 정의와 글자 그대로 맞춰 두 표를 조인할 수 있게 했다.

### Synthetic Population 3층 분리

주민등록 분포와 데이터랩 집중률을 **곱하지 않는다**. 두 자료의 모집단이 다르므로 각각 독립 시나리오로 둔다.

```text
ResidentPopulationPrior (confirmed)
  → VisitorPropensityAssumption (실측 없음)
  → FestivalVisitorSyntheticPopulation (시나리오 산출물)
```

- 산출: `data/derived/visitor_population_prior_v0.csv` (28행 = 시나리오 2 × 세그먼트 14)
- `resident_neutral_v0`: 세그먼트별 방문 성향이 같다고 가정 → 방문객 구성 = 주민 구성
- `datalab_observed_v0`: 사하구 관광지 관측 구성을 그대로 사용
- 두 시나리오의 총변동거리 **TVD = 0.1144**. 이 값이 현재 방문객 구성에 대해 정직하게 말할 수 있는 불확실성 폭이다.

| 상태 | 내용 |
|---|---|
| 추후 검증 | 10대 이하 세그먼트의 데이터랩/주민 배수가 약 0.50이다. 데이터랩이 이동통신 기반 추정이면 휴대폰 미소지 아동이 구조적으로 과소집계되므로, 이를 "아이들이 관광지에 안 간다"로 해석하면 안 된다. |
| 금지 | `visitor_share_pct` 를 실측 방문객 비율이라고 부르는 것. 총 방문객 수를 이 표에서 유도하는 것. |

### Utility AI v0 스펙

- 스펙: `data/metadata/utility_ai_spec.md`
- 계수 노브 표: `data/unreal/utility_coefficients_v0.csv` (16개 계수, 전부 `status` 표기)
- `affinity_scale_ref = 1.27` 은 산출물의 관측 최대 절대값 1.2687에서 정한 `derived` 값이다. 나머지 계수는 전부 `assumption`.

## 외부 검토(Codex) 반영 사항

| 지적 | 판단 | 조치 |
|---|---|---|
| 데이터랩 구성비를 효용계수 β라 부르면 안 됨 | 수용 | `ObservedAffinityPrior` 성격으로 문서 전반 표현 통일. `lambda_affinity` 는 별도 노브로 유지 |
| 동일가중 평균을 "사하구 관광객의 41%"라고 말하면 안 됨 | 수용 | "13개 관광지 동일가중 평균 구성"으로만 표기 |
| MSA는 대기시간이 아니라 흐름을 평균해야 함 | 수용 | `utility_ai_spec.md` 6절에 흐름 평균 방식으로 명시. 균형 반복 중에는 샘플링 대신 기대흐름 누적 |
| Balking은 클램프가 아니라 별도 선택지로 | 수용 | `no_choice_utility` 계수 신설, 후보 집합에 `no_choice` 포함 |
| 창발 결과는 타당성 증명이 아님 | 수용 | 스펙 7절에 "쓸 수 있는 말 / 쓸 수 없는 말"로 명문화 |
| Tour_Stat 응답률은 β 비율이 아님 | 수용 | 계수 표 `source_basis` 를 `tour_stat_2018_rank_only` 로 표기. 순위 참고로만 사용 |
| IPF를 "유일한 최대엔트로피 해"라 단정하면 부정확 | 수용 | 현 단계에서 IPF를 쓰지 않으므로 해당 없음. 도입 시 시드 조건 명시 |
| 문화관광축제 43건을 검증 앵커로 확정하면 안 됨 | 수용 | 연인원·기간·집계방식 미확인. `total_visitor_count` 의 `source_basis` 를 참고 범위로만 표기 |

추가로 확인한 사항 하나: 데이터랩 집중률에는 선호뿐 아니라 **접근성**이 섞여 있다. 여기에 `distance_cost` 를 따로 빼면 거리 효과를 두 번 세게 된다. v0에서는 해결하지 않고 `lambda_affinity` 와 `weight_distance` 를 함께 흔드는 민감도 분석으로 영향 범위를 본다.

## 단계 E — 아키타입 구조 확정 (2026-08-04)

사용자가 다음 구조를 확정했다.

- **주축**: 생애주기 5종 (Tour_Stat 2018 연령별 활동·선택이유의 뚜렷한 방향성 신호로 정의)
- **오버레이 1**: 동반유형(1인/2인/3인+) — 별도 아키타입이 아니라 모든 생애주기에 공통 공식으로 적용되는 개체 보정
- **오버레이 2**: 아동 종속 NPC — 0~14세 독립 행동데이터 부재를 근거로 성인 아키타입에 종속
- **성별**: 아키타입 정체성 축에서 제외. 인구 샘플링에만 실측 비율대로 사용 (DataLab 성별 신호가 1개소짜리 대분류 노이즈에 오염되어 있었기 때문)

판단 근거: 4대분류(역사관광/자연관광/문화관광/쇼핑) affinity로 세그먼트를 군집화했으나, 역사관광·쇼핑이 각 1개 관광지뿐이라 노이즈가 군집 결과를 지배했다(예: "10대 이하 남성"과 "30대 여성"이 최근접으로 묶임 — 해석 불가). 대신 Tour_Stat 2018 연령별 활동·선택이유의 방향성 있는 차이(테마파크 15~19세 최고, 유흥/오락 20대 최고, 지역축제참가 50대 최고, 온천/스파 60+ 최고)를 주축으로 삼았다.

### 생성된 산출물

| 파일 | 내용 |
|---|---|
| `data/cleaned/tourism/tour_stat_2018_age_activity.csv` | 연령별 활동 응답률 (raw HTML 재파싱, 손타이핑 없음) |
| `data/cleaned/tourism/tour_stat_2018_age_reason.csv` | 연령별 방문지 선택 이유 응답률 |
| `data/cleaned/tourism/tour_stat_2018_companion_activity.csv` | 가구원수별 활동 응답률 |
| `data/cleaned/tourism/tour_stat_2018_companion_reason.csv` | 가구원수별 선택 이유 응답률 |
| `data/derived/festival_activity_category_mapping_v0.csv` | Tour_Stat 21개 활동 항목 → 축제 부스 카테고리 판단 매핑 (assumption, 아트/기획팀 재검토 필요) |
| `data/derived/npc_archetypes_v0.csv` | 생애주기 5종 × 시나리오 2종 = 10행. 인구비중 + 카테고리별 응답률/편차 |
| `data/derived/companion_type_modifier_v0.csv` | 동반유형 3종의 카테고리별 오버레이 보정값 |
| `data/metadata/dependent_child_npc_spec.md` | 아동 종속 NPC 메커닉 초안 |
| `data/metadata/utility_ai_spec.md` | `preference_vector` 합성식(아키타입 delta + 동반유형 delta 단순합산) 반영 |

### 확정됨 (2026-08-04)

- 아키타입 구조: 생애주기 5종(주축) + 동반유형 오버레이 + 아동 종속 NPC.

### 아직 초안인 것 (확인 필요)

- 5개 아키타입 표시 명칭(가칭): 주니어 체험형 / 청년 유흥형 / 청장년 휴식형 / 중년 액티브형 / 시니어 여유형. `archetype_label_draft`/`ArchetypeLabelDraft`에 반영돼 있으나 **UI 담당자 논의 후 확정** 예정.
- `festival_activity_category_mapping_v0.csv` 의 활동→부스 카테고리 매핑 (실제 부스 유형 확정 전 임시)
- 아동 부착 확률·수 노브 (`dependent_child_npc_spec.md`)
- 동반유형 3종의 표시 명칭(`CompanionLabelDraft`, "1인 동반"/"2인 동반"/"3인 이상 동반") — 서술적 라벨이라 급하지 않음

### 남은 해상도 문제 (데이터로 해결 불가, 노브로 유지)

- `life_stage_teen`은 15~19세 응답을 0~19세 전체의 대리값으로 쓴다. 0~14세 독립 신호 없음.
- `life_stage_60plus`는 Tour_Stat이 "60세 이상"으로 묶여 있어 60대/70대+를 구분하지 못한다.
- `life_stage_30_40s`는 30대·40대 Tour_Stat 값을 인구가중 없이 단순평균했다.
- 동반유형 보정은 생애주기와 독립이라고 가정한 것이며, 실제 상호작용 여부는 검증되지 않았다.

## 2026-08-04 갱신 — 동반유형 재정의 + 아동 이중 계산 문제 수정

사용자 피드백으로 구조적 문제 2개를 발견해 `population_group_generation_spec.md`(신설)로 해결했다. 전체 내용은 그 문서 참고, 여기서는 결정 사실만 기록한다.

| 상태 | 내용 |
|---|---|
| 확정 | 동반유형(`companion_type`)은 NPC 개인 속성이 아니라 그룹 생성 규칙이다. 그룹 크기를 먼저 샘플링하고 그 수만큼 NPC를 생성하는 5단계 흐름으로 재정의했다. |
| 가정 | 연령 분포와 동반유형 분포의 교차자료가 없어 `P(연령, 그룹크기) ≈ P(연령) × P(그룹크기)` 독립가정을 명시적으로 쓴다. 실제 독립이라는 주장이 아니라 초기 생성 기준이다. |
| 가정 | `group_3_plus` 그룹은 최소 1명이 성인 아키타입이어야 한다는 최소 제약만 건다(v0). 그 이상의 가구 구성 모델은 범위 밖. |
| 버그 발견 | `build_npc_archetypes.mjs`가 `life_stage_teen`에 `age_10_or_under`(0~19세 통합) 인구 세그먼트 전체를 매핑하고 있어서, `dependent_child_npc_spec.md`의 "확률 0.4로 아동 추가" 메커닉과 겹치면 아동이 이중으로 계산되는 문제가 있었다. |
| 해결 | "방식 A"(사람 수를 먼저 맞춘다) 채택: 원본 단일세 인구 데이터에서 0~14세 목표 아동 수를 직접 계산하고, `life_stage_teen`은 15~19세만으로 다시 계산한다. 아동 부착 확률·평균 수 노브는 이 목표치에 맞게 조정되는 파생값으로 재정의했다. |
| 미착수 | `build_npc_archetypes.mjs`의 `life_stage_teen popSegments` 수정, 그룹 생성·아동 배치를 실제로 수행하는 스크립트 — 둘 다 아직 결정·문서화만 하고 구현 전이다. |

### 후속 결정 — 그룹 세부 규칙 (같은 날)

| 상태 | 내용 |
|---|---|
| 확정 | 그룹 목적지는 **리더 상속 모델**로 간다 — 그룹 리더 1명만 Utility 계산, 나머지 전원(성인 포함) 상속. "임시 분리 후 재합류"는 v1 이후로 명시적으로 미룸. |
| 확정 | 그룹은 시간축에서도 하나의 단위 — 스폰 일괄 처리, `arrival_time` 그룹 공유, `stay_duration_multiplier`도 `walk_speed_multiplier`처럼 개체ID 대신 GroupId로 시드해서 그룹 전체가 공유. 취향·인내심 관련 4개 특성은 계속 개체별 독립. |
| 가정 | `group_3_plus` 인원을 3~4명으로 제한(70%/30%). |
| 가정 | 그룹 내 연령 구성을 유형별로 나눔 — `pair`: couple_or_friends 80%/parent_and_child 20%, `group_3_plus`: family 50%/friends 50%. 전부 실측 없는 초기값이며 새 age×companion 교차자료가 생기면 교체. |
