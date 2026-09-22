# 축제 조합 도감

2026-09-21

현재 전체화면 유리 디자인과 프로젝트 폰트 적용은 [FULLSCREEN_GLASS_UI_KO.md](FULLSCREEN_GLASS_UI_KO.md)를 따릅니다. 아래 내용은 앞서 적용한 발견 상태와 카탈로그 이동 기능의 기록입니다.

시설과 조명이 어우러지는 해변 축제의 분위기를 사용합니다. 짙은 바다색 헤더에는 잔잔한 파도와 줄 조명, 진주빛 가장자리를 넣고, 조합은 밝은 아이보리의 축제 입장권처럼 표현합니다. 참고 이미지에서는 수집 진행과 발견 상태를 읽기 쉽게 나누는 구성을 참고했습니다. 실제 시설 썸네일과 게임의 초록·민트·금빛 팔레트를 유지합니다.

## 발견 상태

| 조합 상태 | 해당 시설의 현재 설치 상태 | 썸네일 |
|---|---|---|
| 미발견 | 미설치 | 청록 실루엣과 민트 윤곽 |
| 미발견 | 설치됨 | 그레이스케일 |
| 발견 완료 | 설치 여부와 무관 | 원본 컬러 |

목록과 하단 조합 상세에 같은 규칙을 적용합니다. 시설 이름과 카탈로그 링크는 미발견이어도 사용할 수 있습니다. 실제 이미지를 흐리게 만드는 대신 원본 알파를 유지하는 UI 셰이더로 색상과 실루엣을 변환합니다. 상단 범례에는 규칙을 표시하고 마우스를 올리면 설치 상태에 따른 차이를 설명합니다.

발견 진행 막대는 전체 조합의 발견 비율입니다. 검색이나 발견 필터를 바꿔도 전체 수집 진행도를 유지합니다. 빈 도감에서는 0으로 표시합니다.

보상은 기존과 같이 잠김 → 받기 → 완료로 구분하며, 세 상태의 전체 크기는 72×32 UMG 단위로 동일합니다. 발견 즉시 지급하지 않고 수령 버튼을 눌렀을 때 한 번 지급합니다.

## 카탈로그에서 찾기

도감 상세의 시설을 누르면 해당 카테고리와 페이지로 이동해 시설을 선택합니다. 그 카드에는 민트·금빛·라일락 이리디센스 테두리와 **도감에서 찾은 시설** 배지를 표시합니다. 이동 자체로 배치를 시작하지 않습니다.

강조는 시설 ID에 연결됩니다. 페이지 이동·검색·목록 재사용 때문에 다른 시설에 강조가 남지 않습니다. 다른 시설을 직접 선택하거나, 일반 카탈로그를 다시 열거나, 카탈로그를 떠나면 강조가 해제됩니다.

## 에셋과 반영

- 도감: `WBP_FF_FacilityCodexScreen`, `WBP_FF_FacilityCodexCard`
- 카탈로그: `WBP_FF_PrepBuildTile`
- 원본 시설 이미지는 수정하지 않습니다.
- 재질은 `/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials`에 저장됩니다.
- `M_FF_Codex_DiscoveryThumbnail`: `ThumbnailTexture`, `VisualState`(컬러 0 / 흑백 1 / 실루엣 2), `TexelSize`
- `M_FF_CodexFestivalSurface`: 헤더·종이·카드·선택 테두리의 공통 재질
- `M_FF_CodexCatalogFocus`: 카탈로그 이동 대상의 테두리·배지 재질

최신 Editor 빌드의 비실행 `/Engine/Maps/Entry`에서 `Tools/FacilityCodex/apply_festival_codex.py`를 `ExecutePythonScript`로 실행합니다. 세 재질 생성 스크립트와 `FF.ApplyFacilityCodexFestivalTheme`, `FF.ApplyCodexCatalogFocus`를 순서대로 실행합니다. 레이아웃은 저장된 WBP가 소유하며 런타임에 다시 만들지 않습니다.

반영 전 백업과 재실행 해시·다른 UI 에셋 보존 결과는 `Saved/FacilityCodexFestival_20260921/`에 기록합니다. 이전 보상 UI 마이그레이션을 선행한 현재 에셋을 기준으로 합니다. 기존 버전 메타데이터가 있으면 Designer 편집을 덮어쓰지 않습니다.

## 검증 범위

- `FestaFiesta.FacilityCodex.UI.AuthoredWidgets`: 필터·진행도·실제 시설 이미지·보상과 시설 링크·720p/1080p 배치
- `FestaFiesta.FacilityCodex.UI.ThumbnailDiscoveryStates`: 그레이스케일 RGB, 실루엣 색, 알파, 이미지 재사용
- `FestaFiesta.UI.PrepBuild.RenderedPreview`와 `-FFSynergyNavigationPreview`: 이동한 시설 강조와 카탈로그 화면
- `FestaFiesta.UI.PrepBuild.ReleasedRowPointerLifecycle`: 재사용한 카드의 강조 잔상 방지
- 기존 Codex 모델·보상·Presenter 검사로 발견·수령·화면 이동 동작을 확인합니다.

`Saved/Automation/FacilityCodex`와 `Saved/Automation/BuildableGrid`의 PNG는 실제 저장 위젯을 검사용 데이터로 렌더링한 화면입니다.
