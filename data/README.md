# 공공데이터 작업 공간

축제 시뮬레이션에 쓰는 공공데이터의 출처·가공 계보를 보존하는 폴더다.

## 폴더 구조

```text
data/
├─ raw/         다운로드 원본. 절대 직접 수정하지 않는다. (git 제외)
├─ extracted/   ZIP 해제 작업본. (git 제외)
├─ cleaned/     인코딩·열 이름·코드·결측값을 정리한 표.
├─ derived/     가중치·규칙으로 계산한 게임용 분포. Utility AI 입력 후보.
├─ unreal/      Unreal DataTable로 바로 임포트할 CSV.
└─ metadata/    출처 카탈로그, 버전, 가정, 검증 기록.
   └─ SiriKim_Docs/   김시리 로컬 전용 폴더. (git 제외)
```

`data/raw/`, `data/extracted/`, `data/metadata/SiriKim_Docs/`는 `.gitignore`로 제외한다.

## 상태 라벨

- `confirmed`: 파일 내용이나 공식 보고서로 확인됨.
- `assumption`: 검증되지 않은 임시 해석·가정.
- `to_verify`: 출처·라이선스·모집단·시점 등을 아직 확인해야 함.

## 지켜야 할 규칙

1. `raw/` 아래 파일은 수정하거나 이름을 바꾸지 않는다.
2. 값을 가공할 때마다 `cleaned/` 아래에 새 파일을 만든다.
3. `derived/` 산출물에는 출처 ID와 가공 버전을 기록한다.
4. 모집단·연도·공간 단위가 다른 통계는 결합 키·정규화·가중치·가정·불확실성을 문서화하지 않고는 합산하지 않는다.
5. 가중치가 확인·검증되기 전에는 원자료 행 개수 비율을 모집단 비율로 쓰지 않는다.
6. 원자료(microdata)나 API 키는 커밋하지 않는다.

## 현재 상태 (최종 갱신 2026-08-06)

NPC 아키타입 구조가 확정됐다: 생애주기 5종(주축) + 동반유형 오버레이(선호 delta 계산용 카테고리 3종) + 아동 종속 NPC. `derived/`, `unreal/` 아래 Row와 DataTable CSV가 그 구조를 반영한다.

**표시 명칭 — 여전히 "확정"이 아니다.** 한 차례 확정했다가 UI 담당자 논의 필요라는 피드백으로 되돌린 전례가 있으니, 여기 문장을 "확정"으로 고치기 전에 반드시 팀 합의 결과를 확인할 것.

- **2026-08-06**: UI 담당(백종태)이 `data/ui/*.csv`의 `ui_override_label`을 채웠고, 빌드 스크립트 재실행으로 `data/unreal/*.csv`까지 반영됐다 — 아키타입 `체험파`/`활동파`/`휴식파`/`탐방파`/`여유파`, 동반유형 `혼자`/`둘이서`/`여럿이서`.
- 그래도 `DataStatus`는 `draft_assumption` 그대로다. **UI 담당이 값을 정한 것과 팀이 확정한 것은 다른 사건이다**(`data/ui/README.md` 규칙 4). 이 구분이 위 전례가 남긴 교훈이다.

UI 담당자가 명칭을 편집하려면 `data/ui/archetype_label_overrides.csv`·`data/ui/companion_label_overrides.csv`를 쓴다 (`data/ui/README.md` 참조). `data/derived/`·`data/unreal/`을 직접 고치지 않아도 된다. **다만 편집 후 `node Tools/data/build_unreal_datatables.mjs`를 실행해야 `data/unreal/`에 반영된다** — 8/6에 실제로 이 단계가 빠져서 override가 며칠간 반영되지 않은 채 남아 있었다.（10/14 기준: 지금 이 스크립트를 그대로 돌리면 안 된다. `data/unreal/`의 유틸 계수 16행·SNS 문장 270행이 `data/derived/`에 없어 사라진다 — `data/unreal/README.md` 경고 참조）

**2026-08-04 갱신**: 동반유형(`companion_type`)은 NPC 개인에게 독립적으로 부여하는 속성이 아니라 **그룹 생성 규칙의 결과값**으로 재정의됐다 — 그룹 크기·유형(가족/친구 등)·연령 구성·목적지 상속(리더 모델)·시간 공유 규칙은 `metadata/spec/population_group_generation_spec.md`를 참고한다.

같은 작업에서 종속 아동 생성이 일반 인구 샘플링과 이중 계산되는 버그를 발견했고, **`Tools/data/build_npc_archetypes.mjs`를 고쳐서 실제로 수정했다.** `life_stage_teen`의 `population_share_pct`는 이제 원본 단일세 인구로 분리한 15~19세 몫만 반영하고(예: `resident_neutral_v0` 12.0696% → 4.1503%), 0~14세 몫은 `derived/dependent_child_population_target_v0.csv`(목표 아동 비율)로 분리했다 — 두 값을 더하면 원래 통합값과 정확히 같다(이중 계산 없음, 검증됨).

아직 확정 전인 것: 활동→축제 부스 카테고리 매핑(실제 부스 유형 미확정), 아동 부착 확률·수 노브(위 목표 아동 비율에 맞게 재조정될 파생값), 그룹 생성·아동 배치 실제 구현(스크립트는 목표 비율만 산출하고 실제 그룹/아동 생성 코드는 아직 없음). 전체 경과는 `metadata/public_data_decision_log.md`를 참고한다.

**2026-08-06 갱신**: crowdAI(이정원)에게 넘길 **시간대별 목표 유입 곡선**을 확정했다 — `derived/visitor_arrival_curve_v0.csv`, `unreal/visitor_arrival_curve_v0.csv`(`FFF_ArrivalCurveRow`). 버킷별 **신규 유입 비율(%)**이며 절대 인원·동시 체류 목표가 아니다. 설계 근거와 소비 방법은 `metadata/spec/visitor_arrival_curve_spec.md`.

같은 날 **선행 블로커를 발견하고 해결했다**: `derived/companion_type_modifier_v0.csv`에는 solo/pair/group_3_plus의 **인구비중 열이 없고**, 그 출처인 Tour_Stat 2018 표의 가로분석변수는 `가구원수`(여행 동반 인원이 아님)였다. 2025 국민여행조사 원자료를 가공해 대체했다:

| 산출물                                              | 내용                                          |
| ------------------------------------------------ | ------------------------------------------- |
| `derived/visitor_group_size_distribution_v0.csv` | 그룹 크기 분포 1~6인 (문항 `A6`, **6인 상한** — 초과분 합침) |
| `derived/group_child_composition_v0.csv`         | 그룹 크기별 아동(만 15세 미만) 수 분포 (문항 `A6A`)         |
| `derived/companion_type_response_v0.csv`         | 동반자 유형 응답률 (문항 `A6B`, 참고용)                  |
| `derived/group_composition_rules_v0.csv`         | 실측으로 대체되지 않고 남은 규칙 11개                      |

`tourism_daytrip` 기준 solo 3.07% / pair 35.00% / group_3_plus 61.93%, 평균 그룹 크기 3.15명. 통계편 공표표 14-1-1/2/3과 **최대오차 0.08pp**로 일치(스크립트에 0.1pp 검증 내장). 이 과정에서 `D_TRA{n}_NUM`이 "여행 인원"이 아니라 "**지출비용에 포함되는 인원수**"임을 확인해 `metadata/variable_catalog.json`을 정정했고, 미해결이던 `WT_DOM` 적용 단위도 확정했다. 폐기된 가정 4개와 남은 미해결(아동 비율 추정치 3개)은 `metadata/public_data_decision_log.md`·`metadata/spec/population_group_generation_spec.md` 2.4절 참조.

### 2026-08-06 후반 — Utility 계수 3개 추가

`derived/utility_coefficients_v0.csv`가 20 → **28행**이 됐다. 셋 다 **공공데이터 근거가 없다** — 다른 계수와의 상대 관계로 정한 값이라 위 상태 라벨 중 `assumption`(정확히는 `draft_assumption`)이다.

| 계수                                  | 값    | 무엇                                                                                            |
| ----------------------------------- | ---- | --------------------------------------------------------------------------------------------- |
| `weight_need`                       | 1.8  | Utility 식의 need 항 가중치. `weight_attraction`+`lambda_affinity`보다 크게 잡아 "급한 사람은 취향을 포기한다"를 성립시킨다 |
| `no_choice_utility_decay_per_round` | 0.03 | 연속으로 아무것도 못 고를 때 no_choice 매력을 깎는 양. 없으면 NPC가 영원히 배회한다                                        |
| `no_choice_leave_patience_rounds`   | 5    | 연속 no_choice가 이 횟수에 닿으면 퇴장                                                                    |

**민감도 분석 시 주의**: `weight_need`는 `lambda_affinity`·`weight_attraction`과 **함께** 흔들어야 한다. 혼자 흔들면 세 계수 사이의 부등식이 깨진 구간에서 나온 결과를 계수 민감도로 오해한다. 근거와 한계는 `metadata/public_data_decision_log.md` 2026-08-06 마지막 항목.

이 폴더의 산출물이 실제로 어디서 소비되는지는 아래 문서 지도를 따라간다 — CSV → 빌드 스크립트 → DataTable → 순수 함수 순으로 한 방향으로만 흐른다.

## 국민여행조사 가중치 주의사항

전체 2,419개 원자료 열은 `metadata/variable_catalog.csv`, 핵심 변수·가중치·코드 설명은 `metadata/variable_catalog.json`에 기록한다. `WT_DOM`이 가중치 후보이며, 원자료를 실제로 가공하기 전 적용 단위를 재확인해야 한다.

## 문서 지도

| 문서                                                   | 언제 본다                                     |
| ---------------------------------------------------- | ----------------------------------------- |
| `metadata/spec/utility_ai_spec.md`                   | Utility 식(6항)·계수·필드 출처·검증 방법·구현 백로그       |
| `metadata/spec/utility_destination_selector_spec.md` | 선택기를 실제로 어떤 순서로 조립하는지                     |
| `metadata/spec/individual_variation_spec.md`         | 개체 변주 6종·Seed 규칙·삼각분포·그룹 공유               |
| `metadata/spec/population_group_generation_spec.md`  | 그룹 생성 규칙 전체와 폐기 이력                        |
| `metadata/spec/dependent_child_npc_spec.md`          | 아동 슬롯이 왜 아키타입을 안 받는지, 무엇을 상속하는지           |
| `metadata/spec/visitor_arrival_curve_spec.md`        | 유입 곡선이 왜 절대 인원이 아니라 비율인지                  |
| `metadata/spec/reservation_program_spec.md`          | 유료 프로그램 예약 규칙과 후속 작업. 결정 필요 항목 4개 포함      |
| `metadata/request/facility_naming_convention.md`     | 시설 이름을 짓는 사람이 봐야 하는 규칙 + 코드가 소분류로 무엇을 하는지 |
| `metadata/request/crowdai_integration_guide.md`           | crowdAI와의 계약 전체                           |
| `metadata/public_data_decision_log.md`               | 데이터·계수 결정과 **번복** 이력. "왜 이 값인가"를 확인할 때    |
| `../UI_API.md` (리포 루트)                               | UI가 호출·구독하는 것                             |
