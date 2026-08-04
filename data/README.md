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
   └─ session_log/   AI 세션 간 작업 인수인계 로그. 팀 공유 문서가 아니라 로컬 전용. (git 제외)
```

`data/raw/`, `data/extracted/`, `data/metadata/session_log/`는 `.gitignore`로 제외한다. 원본은 용량이 크거나 재배포 조건이 불확실하고, 세션 로그는 팀 전체가 볼 문서가 아니라 작업 연속성을 위한 개인 기록이기 때문이다. 다운로드 원본의 출처·해시는 `data/metadata/source_catalog.json`에 기록해 계보를 끊지 않는다.

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

## 현재 상태 (2026-08-04)

NPC 아키타입 구조가 확정됐다: 생애주기 5종(주축) + 동반유형 오버레이(선호 delta 계산용 카테고리 3종) + 아동 종속 NPC. 아키타입 5종의 표시 명칭도 확정됐다. `derived/`, `unreal/` 아래 Row와 DataTable CSV가 그 구조를 반영한다.

**2026-08-04 갱신**: 동반유형(`companion_type`)은 NPC 개인에게 독립적으로 부여하는 속성이 아니라 **그룹 생성 규칙의 결과값**으로 재정의됐다 — 그룹 크기·유형(가족/친구 등)·연령 구성·목적지 상속(리더 모델)·시간 공유 규칙은 `metadata/population_group_generation_spec.md`를 참고한다.

같은 작업에서 종속 아동 생성이 일반 인구 샘플링과 이중 계산되는 버그를 발견했고, **`Tools/data/build_npc_archetypes.mjs`를 고쳐서 실제로 수정했다.** `life_stage_teen`의 `population_share_pct`는 이제 원본 단일세 인구로 분리한 15~19세 몫만 반영하고(예: `resident_neutral_v0` 12.0696% → 4.1503%), 0~14세 몫은 `derived/dependent_child_population_target_v0.csv`(목표 아동 비율)로 분리했다 — 두 값을 더하면 원래 통합값과 정확히 같다(이중 계산 없음, 검증됨).

아직 확정 전인 것: 활동→축제 부스 카테고리 매핑(실제 부스 유형 미확정), 아동 부착 확률·수 노브(위 목표 아동 비율에 맞게 재조정될 파생값), 그룹 생성·아동 배치 실제 구현(스크립트는 목표 비율만 산출하고 실제 그룹/아동 생성 코드는 아직 없음). 전체 경과는 `metadata/public_data_decision_log.md`를 참고한다.

## 국민여행조사 가중치 주의사항

전체 2,419개 원자료 열은 `metadata/variable_catalog.csv`, 핵심 변수·가중치·코드 설명은 `metadata/variable_catalog.json`에 기록한다. `WT_DOM`이 가중치 후보이며, 원자료를 실제로 가공하기 전 적용 단위를 재확인해야 한다.
