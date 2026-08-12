# data/metadata — 폴더 지도

```
metadata/
├─ handoff/      다른 담당자에게 주는 계약. 그 사람이 읽어야 동작한다
├─ spec/         설계 명세. 커밋되는 CSV·코드의 "왜 이 값인가"
├─ session_log/  로컬 전용 작업 공간 (git 제외)
├─ public_data_decision_log.md   데이터·계수 결정과 번복 이력
├─ source_catalog.json           원본 출처·해시 (원자료가 git 제외라 여기가 유일한 계보)
└─ variable_catalog.json / .csv  국민여행조사 원자료 열 사전
```

담당 범위는 **Utility AI**와 **경제**(김시리) 두 파트다. 두 파트의 성격이 다르므로 아래 "값의 근거"를 먼저 본다.

## `handoff/` — 남이 읽어야 동작하는 문서

| 문서                              | 대상               | 안 읽으면                      |
| ------------------------------- | ---------------- | -------------------------- |
| `crowdai_handoff_note.md`       | 이정원 (crowdAI)    | 그룹핑·입장 타이밍·개체 변주 호출이 안 걸린다 |
| `facility_naming_convention.md` | 한광훈·박건영 (시설 에셋)  | 부스가 "취향 없음"으로 조용히 처리된다     |
| `zone_system_requests.md`       | 한광훈 (ZoneSystem) | 시설 태그·정원·비용 키 질문이 열린 채 남는다 |
| `queue_requests.md`             | 장원재 (Queue)      | 대기시간 단위·Party 분할 전제가 어긋난다  |

`facility_naming_convention.md`는 **런타임 Warning 로그가 이 경로를 출력한다** — 위반한 시설을 만든 사람이 찾아올 문서다.

## `spec/` — 커밋되는 데이터·코드의 근거

**Utility AI**

| 문서                                     | 무엇                              |
| -------------------------------------- | ------------------------------- |
| `utility_ai_spec.md`                   | Utility 식(6항)·계수·필드 출처·검증 방법    |
| `utility_destination_selector_spec.md` | 선택기 호출 순서와 조립 방법                |
| `individual_variation_spec.md`         | 개체 변주 6종·Seed 규칙·삼각분포·그룹 공유     |
| `population_group_generation_spec.md`  | 그룹 생성 규칙 전체와 폐기 이력              |
| `dependent_child_npc_spec.md`          | 아동 슬롯이 왜 아키타입을 안 받는지, 무엇을 상속하는지 |
| `visitor_arrival_curve_spec.md`        | 유입 곡선이 왜 절대 인원이 아니라 비율인지        |
| `info_safety_facility_spec.md`         | Info·Safety 시설의 건설 이득과 Event 연결 |

**경제 · 공용**

| 문서                            | 무엇                                                     |
| ----------------------------- | ------------------------------------------------------ |
| `economy_spec.md`             | **경제의 원본.** Gold 원장 전체·밸런스 수치·3일 수지 검산·구조·미결(E-1~E-20) |
| `reservation_program_spec.md` | 유료 프로그램 예약 규칙과 구조 충돌 (Utility AI·경제 양쪽에 걸린다)           |

**여기 있는 파일을 옮기거나 이름을 바꿀 때는** 코드·스크립트가 경로로 참조하고 있으니 같이 고쳐야 한다. 놓치기 쉬운 곳:

```bash
grep -rn "data/metadata/" --include=*.h --include=*.cpp --include=*.mjs Source/ Tools/
grep -rln "data/metadata/" data/derived/    # 생성 스크립트가 JSON에 심어둔 경로
```

`Tools/data/*.mjs`가 생성물에 spec 경로를 심으므로, 경로를 고친 뒤 해당 스크립트를 재실행해야 한다.

## 값의 근거 — 두 파트가 다르다

**Utility AI 는 공공데이터 기반**이다. 그래서 값마다 4단계 상태 라벨을 붙인다.

`confirmed` 확인된 사실 / `derived` 실측 가중집계 / `assumption`·`draft_assumption` 근거 없는 임시값 / `to_verify` 미확인

**경제는 게임 시스템이고 공공데이터를 쓰지 않는다** (2026-08-12 확정). Gold 는 원(₩)이 아니고 환율도 없다.
금액은 기획서 값 또는 게임 내부 값끼리의 관계로 정하고, **밸런스가 안 맞으면 그냥 숫자를 바꾼다.**
그래서 4단계 라벨을 쓰지 않고 두 종류만 쓴다.

`[기획서]` 기획서에 명시된 수치 (바꿀 때 팀 합의 필요) / `[밸런스]` 내가 정한 값 (플레이테스트로 자유롭게 조정)

> 트윈 성격은 **관객 만족도**(Utility AI)가 진다. 경제 수치에 공공데이터 근거를 붙이면
> 가짜 권위가 생겨 밸런싱이 막힌다 — `economy_spec.md` §0.
