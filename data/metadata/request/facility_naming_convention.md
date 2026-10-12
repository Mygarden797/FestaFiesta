# 시설 FacilityDefinitionId 네이밍 규칙 — Utility AI 쪽 해석

- 작성일: 2026-08-06 (`booth_naming_convention.md`에서 개명·범위 확장)
- **규칙의 원본은 팀 노션 페이지 "FacilityDefinitionId 네이밍 규칙"이다.** 이 문서는 그 규칙을 코드가 어떻게 읽는지, 안 지키면 무슨 일이 생기는지를 적는다. 규칙 자체가 바뀌면 노션이 먼저고 이 문서가 따라간다.
- 대상: 시설 에셋을 만드는 담당자 + Utility AI 코드를 읽는 사람

## 1. 형태

```text
대분류_소분류_고유이름
```

- **대분류**는 `EFFZSFacilityCategory` enum 이름을 그대로 쓴다 (`Booth` `Toilet` `Info` `Rest` `Performance` `Safety` `PhotoSpot`).
- **소분류**는 아래 표에 있는 토큰만 쓴다.
- **고유이름**은 기능·콘텐츠를 나타내는 영문 PascalCase. `_`가 더 들어가도 코드는 앞의 두 토큰만 본다.

```text
좋은 예   Booth_FoodStall_SeafoodGrill / Performance_Busking_StreetStage
피해야 함  Booth_New_01 / Booth_Test_Booth / Booth_Basic_A
```

> **(2026-08-06 변경)** 이전 버전 이 문서는 고유이름 예시를 한글로 적었다(`Booth_FoodStall_피자몬`). 팀 규칙이 영문 PascalCase로 정해졌으므로 **한글 예시는 폐기**한다. 실제 축제 부스명도 영문으로 옮겨 쓴다(피자몬 → `PizzaMon`).

## 2. 소분류 토큰 표

코드의 `UFF_FacilityNamingLibrary::GetKnownMinorTokens()`
(`Source/FestaFiesta/Private/Building/FF_FacilityNamingLibrary.cpp`)와 **1:1로 유지해야 한다.**
이 공용 표를 건축 카탈로그와 Utility AI Bridge가 함께 사용한다. 노션에서 토큰이 추가되면
공용 표도 같이 고쳐야 하고, 안 고치면 새 토큰이 카탈로그에서 제외되며 "규칙 위반" 경고가 뜬다.

| 대분류 | 소분류 | 의미 |
|---|---|---|
| `Booth` | `FoodStall` | 음식·음료 |
| | `MarketGoods` | 굿즈·상품 판매 |
| | `CraftExperience` | 공예·제작 체험 |
| | `ActivityGame` | 게임·놀이 체험 |
| | `Nightlife` | 야간·EDM·주점 |
| `Toilet` | `Standard` / `Accessible` / `Family` / `Temporary` | 일반 / 장애인 / 가족·유아 / 임시 |
| `Info` | `GeneralInfo` / `Ticketing` / `LostFound` / `Translation` | 종합안내 / 티켓 / 분실물 / 외국어 |
| `Rest` | `Seating` / `Shade` / `Picnic` / `Cooling` | 좌석 / 그늘막 / 피크닉 / 무더위 쉼터 |
| `Performance` | `MainStage` / `SubStage` / `Busking` / `ExperienceStage` | 메인 / 보조 / 버스킹 / 참여형 |
| `Safety` | `FirstAid` / `Security` / `FireSafety` / `Evacuation` | 의무실 / 보안 / 소방 / 대피 |
| `PhotoSpot` | `Landmark` / `Theme` / `Scenic` / `Interactive` | 상징물 / 테마 / 경관 / 상호작용형 |

## 3. Utility AI가 소분류로 실제로 하는 일

**지금 동작을 바꾸는 것은 `Booth` 소분류뿐이다.** 나머지 카테고리는 소분류를 검증만 하고 대분류 단위로 처리한다.

### 3.1 취향(preference_vector) 매핑 — `MapToPreferenceCategory`

| 시설 | preference 카테고리 |
|---|---|
| `Booth_FoodStall_*` | `food_stall` |
| `Booth_MarketGoods_*` | `market_goods` |
| `Booth_CraftExperience_*` | `craft_experience` |
| `Booth_ActivityGame_*` | `activity_game` |
| `Booth_Nightlife_*` | `nightlife_entertainment` |
| `Rest_*` (4종 전부) | `rest_area` |
| `Performance_*` (4종 전부) | `performance_stage` |
| `PhotoSpot_*` (4종 전부) | `ambience_photo_zone` |
| `Toilet_*` / `Info_*` / `Safety_*` | 없음(`NAME_None`) — 취향 매칭 대상이 아니다 |

`NAME_None`이면 Utility 식의 preference 항이 0으로 계산된다. 크래시 없이 "취향 없음"으로 동작한다.

### 3.2 Need 해소 판정 — `GetNeedReliefFlags`

| 시설 | 해소하는 Need |
|---|---|
| `Booth_FoodStall_*` | Hunger, Thirst |
| `Booth_Nightlife_*` | Thirst(음주), Stimulation |
| `Toilet_*` | Restroom |
| `Rest_Cooling_*` | Fatigue, **Thirst** |
| `Rest_Seating_*` / `Rest_Shade_*` / `Rest_Picnic_*` | Fatigue만 |
| `Performance_*` / `PhotoSpot_*` | Stimulation |
| `Booth_MarketGoods_*` / `Booth_CraftExperience_*` / `Booth_ActivityGame_*` | Stimulation (2026-10-09 변경. 전엔 아무 Need 도 해소하지 않았다) |
| `Info_*` / `Safety_*` | 없음 (설계상 Need 대상이 아니다 — `FF_NeedState.h` 상단 주석) |

## 4. 안 지키면 무슨 일이 생기나

크래시는 안 난다. 대신 **조용한 오작동**이 난다.

- `Booth` 소분류가 표에 없거나 오타(`Booth_Foodstall_...`)면 그 부스는 **영구히 "취향 없음"**이 되어 NPC가 취향으로 선호하지 않고, **Need(배고픔·갈증·자극)도 해소하지 못한다.** 부스는 멀쩡히 서 있는데 아무도 제대로 안 온다.
- 다른 카테고리는 지금은 대분류만 보므로 동작이 안 바뀌지만, 소분류별 분기를 나중에 추가할 때 오타가 있으면 조용히 빠진다.

**그래서 시설당 딱 한 번 `Warning` 로그를 찍는다** (`LogFestaFiestaNpc`). 위반 id, 위반 이유(토큰 수 부족 / 대분류 불일치 / 소분류가 표에 없음), 그 카테고리의 아는 소분류 목록이 같이 나온다.

```text
[시설 네이밍 컨벤션 위반] FacilityDefinitionId='Booth_Foodstall_Grill' — 소분류 'Foodstall'가 규칙 표에 없다. ...
```

매 평가마다 찍으면 방문객 8만 명 규모에서 로그가 감당이 안 되므로 1회로 제한했다. **시설을 만든 뒤 출력 로그에서 `시설 네이밍 컨벤션 위반`을 검색해 아무것도 안 나오면 정상이다.**

`FacilityDefinitionId`가 아예 비어 있으면(`NAME_None`) 경고하지 않는다 — 시설 정의가 아직 안 붙은 정상 상태일 수 있어서다.

## 5. 미결 — 결정이 필요한 것

### 5.1 ~~`Rest` 소분류와 Thirst 해소~~ → 2026-08-06 결정 완료

**`Rest_Cooling`(무더위 쉼터)만 Thirst를 해소한다.** 음수대가 있을 가능성이 가장 높은 소분류로 좁혔다. `Seating`/`Shade`/`Picnic`은 Fatigue만 해소한다.

소분류를 못 읽으면(컨벤션 위반) Thirst는 해소하지 않는다 — 안전한 쪽으로 뺀다. 나중에 노션 규칙에 음수대 전용 소분류가 추가되면 그쪽으로 옮긴다.

### 5.2 `Performance_ExperienceStage`(참여형 공연)의 취향 카테고리

현재 `Performance` 4종 전부 `performance_stage`다. 참여형 공연(댄스배틀 등)은 관람이 아니라 참여라서 `activity_game`에 더 가까울 수 있다.

### 5.3 선셋펍 시간대별 겸용 (이전부터 미해결)

선셋펍은 낮에 웰니스 체험(`CraftExperience`), 밤에 DJ·라이브밴드(`Nightlife`)를 같은 공간에서 겸한다. 네이밍 규칙은 시설 하나에 소분류 하나만 허용해서 이걸 못 담는다. 노션 페이지에도 시간대별 전환 규칙은 없다.

- 후보 ①: 별도 `FacilityDefinition` 2개로 분리
- 후보 ②: Schedule Runtime이 시간대별로 다른 소분류를 적용

### 5.4 시설 태그가 Utility 입력과 겹친다

노션 페이지 마지막의 태그 목록(무료/유료, 실내/실외, 가족 추천, 장애인 접근 가능, 대기열 필요, 예약 필요, 운영시간, 혼잡도, 체험형/관람형) 중 여러 개가 **이미 Utility 식이 다루는 값과 겹치거나 모델을 조용히 깨뜨릴 수 있다.**

| 태그 | Utility AI 쪽 처리 | 상태 |
|---|---|---|
| `혼잡도` | **Utility는 이 태그를 무시한다** (2026-08-06 결정). 혼잡은 런타임 값(현재인원/정원)만 쓴다. 이 태그를 `density_norm`에도 넣으면 혼잡을 두 번 세게 되고, 정적 값과 런타임 값이 섞여 "왜 이 부스가 불리한가"를 설명할 수 없어진다. 태그는 UI 표시·기획용 | 결정 |
| `예약 필요` | **예약 NPC만 입장 가능** (2026-08-06 결정). 유료 프로그램 구매 시 정원이 자동으로 채워지고, 예약자는 프로그램 시간 동안 그 장소를 떠나지 않는다. 설계·후속 작업은 `reservation_program_spec.md` | 결정, 미구현 |
| `무료/유료` | 경제 파트 직결 — 매출 계산의 입력. 유료 프로그램은 위 예약 시스템과 같은 대상이다 | 경제 파트 |
| `대기열 필요` | false면 `EstimatedWait`을 0으로 둬야 한다. 큐가 없는 시설(포토스팟 등)에 대기 비용을 물리면 부당하게 불리해진다 | **미결** |
| `운영시간` | 이미 `bIsOpen`으로 받고 있다. 겹치므로 어느 쪽이 권위인지 정해야 한다 | **미결** |
| `가족 추천` / `장애인 접근 가능` | 아동 동반 그룹의 취향 보정에 쓸 수 있다. v0에는 없다 | v1 후보 |
| `체험형/관람형` | 5.2와 같은 문제를 태그로 푸는 방법일 수 있다 | v1 후보 |

## 6. 실제 축제 구조에서 확인된 것 (참고)

- **오아시스 존**(지도 범례: 그늘+음수대+무릎나눔)은 Booth가 아니라 `Rest` 카테고리다. 5.1의 원인이 이것이다.
- 선셋클럽·선셋펍은 **무대(`Performance`)가 아니라 `Booth_Nightlife`**로 분류한다 — 관객이 무대를 보는 형태가 아니라 바·라운지에서 어울리는 형태이기 때문이다.
- 화장실·종합안내소·미아보호소는 각각 `Toilet`/`Info`로 자동 처리된다.
- `ActivityGame`은 이전 버전 이 문서에서 "이번 축제에 뚜렷한 예시가 없다"고 적었으나, 팀 규칙 페이지에 `Booth_ActivityGame_RingToss` 예시가 명시되면서 **해소됐다.** `preference_vector`의 `activity_game` 축이 죽은 값이 될 걱정은 없다.
