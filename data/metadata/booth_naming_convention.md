# Booth FacilityDefinitionId 네이밍 컨벤션

- 작성일: 2026-08-06
- 대상: 시설(Booth) 에셋을 만드는 담당자 — 한광훈 님 또는 부스 콘텐츠를 정하는 기획/아트
- 배경: `FF_ZoneSystemBridge`(김시리)가 preference_vector·Need 해소를 계산하려면 `Booth` 카테고리 하나로 뭉쳐 있는 식음료·잡화·체험·나이트라이프를 구분해야 한다. 별도 DataTable 대신 `FacilityDefinitionId` 네이밍 규칙으로 푼다 — 코드는 이미 구현돼 있고, **부스를 만들 때 이름만 규칙대로 지으면 된다.**

## 규칙

`FacilityDefinitionId`를 `Booth_<토큰>_<이름>` 형태로 짓는다.

## 토큰 표 (실제 부산바다축제 리플렛·타임테이블 대조, 2026-08-06)

| 토큰 | 의미 | 실제 축제 예시 |
|---|---|---|
| `FoodStall` | 식음료 | 다대포차·청춘포차의 개별 트럭 — 피자몬(피자), 마이야미(닭꼬치), 돼지팔팔한마리(닭발), 뷀 백주(타코야끼) 등 |
| `MarketGoods` | 잡화·굿즈 | 다대비치마켓(해변 플리마켓) |
| `CraftExperience` | 체험 | 웰니스 프로그램 — DIY 아로마 테라피, DIY 아로마 감정향수, 아로마 필라테스, 미니볼 바레(사전예약 체험 부스) |
| `ActivityGame` | 게임·액티비티 | **이번 축제 실제 프로그램엔 뚜렷한 예시가 없다.** 분수놀이터(어린이 워터파크) 정도가 가장 가깝지만 확실치 않음 — 게임 부스가 있는 다른 축제를 위해 남겨두는 토큰이며, 이번 축제에서는 안 써도 된다 |
| `Nightlife` | 나이트라이프 | 선셋클럽("야간 EDM 비치클럽", 하우스DJ 세트) · 선셋펍(하우스DJ + 라이브밴드 라운지) |

예: `Booth_FoodStall_피자몬`, `Booth_MarketGoods_다대비치마켓`, `Booth_Nightlife_선셋클럽`

## 주의

- 이 규칙을 안 따르면 크래시는 안 나지만, 그 부스는 "취향 없음"으로 조용히 처리된다 — NPC가 취향으로 그곳을 선호하는 일이 없어진다.
- `Toilet`/`Rest`/`Performance`/`PhotoSpot`/`Info`/`Safety` 카테고리는 이미 자동으로 처리되니 신경 쓸 필요 없다.
- **선셋클럽·선셋펍은 무대(Performance)가 아니라 Booth(Nightlife)로 분류한다** — 관객이 무대를 보는 형태가 아니라 바·라운지에서 어울리는 형태이기 때문이다.
- **선셋펍은 낮에는 웰니스 체험(CraftExperience), 밤에는 DJ·라이브밴드(Nightlife)를 같은 공간에서 겸한다.** 지금 네이밍 컨벤션은 시설 하나에 토큰 하나만 허용해서 이런 시간대별 겸용은 아직 못 담는다 — 이 부스를 실제로 만들 때 어느 쪽을 주 토큰으로 할지, 혹은 시간대별 카테고리 전환이 필요한지 결정이 필요하다(예: 두 개의 별도 FacilityDefinition으로 분리하거나, Schedule Runtime이 시간대별로 다른 토큰을 적용하는 방식).

## 실제 축제 구조에서 확인된 것 (참고)

- **오아시스 존**(지도 범례: 그늘+음수대+무릎나눔)은 Booth가 아니라 `Rest` 카테고리다. `FFF_NeedState`의 Thirst가 Booth(FoodStall)뿐 아니라 Rest로도 해소되게 반영했다(`FF_ZoneSystemBridge::GetNeedReliefFlags`).
- 화장실·종합안내소&미아보호소는 각각 `Toilet`/`Info`로 이미 자동 처리 — Booth 토큰이 필요 없다.
