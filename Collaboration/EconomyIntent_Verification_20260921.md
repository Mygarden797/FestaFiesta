# 경제 개편 의도와 구현 대조 — 2026-09-21

## 기준과 범위

Looong이 병합한 PR #217 (`c04d3673`)의 제목은 김시리의 경제 구조 개편이다.
`f396a30c`(임대료·미납 제거), `47630dad`(Staff 영구 고용·프로그램 분리),
`8bec1b9a`(WBP 갱신), `ef8893f4`(UI 계약 갱신)과 현재 `UI_API.md`를 기준으로 대조했다.
이 검증은 해당 개편과 프로그램의 다음 날 시설 재사용을 대상으로 한다.
과거 예약 기획의 기간권·스태프 소모 규칙은 최신 요구사항으로 적용하지 않는다.

## 확인한 규칙과 데이터

| 의도 | 확인 결과 |
| --- | --- |
| 임대료·미납 및 하루 경계 자동 지출 제거 | 실제 일자 전환에서 잔액 유지, 일일 장부만 초기화 |
| 스태프는 1회 고용 후 축제 종료까지 유지 | 고용 인원과 축제 전체 상한이 사흘 동안 유지됨 |
| 프로그램은 Gold만 소비 | Staff 부족 판정·요구 인원 필드 없음, 프로그램 편성으로 Staff가 줄지 않음 |
| 프로그램은 하루 단위 | 다음 날 이전 예약·편성이 제거되고 같은 시설에 재편성 가능 |
| 저장된 부산 축제 정의 | 기본 인원 6명, 1명 고용비 450 Gold, 추가 고용 상한 6명 |
| 저장된 프로그램 정의 | 공방: 비용 200, 정원 12, 기본 참가비 30 / 요가: 비용 250, 정원 16, 기본 참가비 25 |
| 시설별 참가비 | 저장된 `DT_FacilityRevenues`가 연결돼 있으며 시설별 참가비가 기본 카드보다 우선 |

`data/unreal/facility_revenues_v0.csv`는 초기 생성용이다. 저장된 DataTable의 편집값을 CSV로 덮어쓰지 않았다.

## 수정한 불일치

- 점검 패널의 `오늘 견적`이 누적 고용 인원 × 현재 단가를 표시했다.
  `GetTodayStaffHireCostGold()`를 통해 당일 원장의 실제 지출만 표시하도록 변경했다.
  전날 고용한 인원은 유지되지만 오늘 새로 고용하지 않았다면 오늘 고용비는 0이다.
  단가를 변경해도 이미 지불한 과거 금액은 달라지지 않는다.
- 저장된 Live 인력 패널의 고정 라벨 `프로그램 배치`, `오늘 편성`이 이전 규칙을 설명했다.
  사건 대응 문구로 고쳤으며 일당 알바·가용 인원·프로그램 필요 인원 미리보기도 정리했다.
- 새로 생성할 위젯의 빌더 문구와 현재 계약 문서도 같은 기준으로 수정했다.

위젯 4종은 전체 재생성 없이 Text 속성만 수정했다.
`Tools/Diagnostics/migrate_staff_economy_ui.py`는 기본적으로 읽기 전용 검증이며,
`-FFApplyStaffEconomyUI`를 명시한 경우 알려진 옛 문구만 변경한다.
기존 레이아웃·스타일을 대조하고 원본을 `Saved/EconomyIntentAudit/Backups`에 보관한다.

## 검증

- `FestaFiestaEditor Win64 Development`: 성공.
- 경제·준비/라이브 인력·프로그램 관련 자동화: 67개 중 66개 통과, 1개 실패.
- 추가 회귀 테스트 3개 모두 통과:
  - `FestaFiesta.UI.PrepCheck.StaffHireLedgerAcrossDays`
  - `FestaFiesta.Economy.DayBoundary.PermanentHireCapPersistsAcrossDays`
  - `FestaFiesta.Economy.ProgramScheduling.ReusesClosedFacilityOnNextDay`
- 저장된 WBP를 별도 프로세스에서 재로드하여 검증 통과. 재검증 시 저장/수정 없음.
- 대상 밖 UI·데이터 에셋 492개 해시 유지. `git diff --check` 통과.

실패한 기존 테스트는 `FestaFiesta.UI.FacilityInfo.PaidProgramWbpContract`의
`Picker matches the compact inspector dock` 크기 검사다.
현재 Picker 빌더는 405px 폭·하단 여백 126px을 사용하지만 테스트의 비-Clarity 분기는
600px·70px을 기대한다. 이 작업은 해당 Picker 에셋·빌더·크기 테스트를 변경하지 않았다.
실행 결과와 로그는 `Saved/EconomyIntentAudit/`에 있다.

## 별도 미완료 범위

Staff를 실제 사건에 파견하는 시스템 연결과 스태프 승급은 최신 원문에서도
미구현/세부 미정으로 구분한다. 이번 경제 개편 검증을 해당 기능의 완성으로 보지 않는다.
