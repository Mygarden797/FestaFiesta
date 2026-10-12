# FF_ZoneAuthoring (Zone 저작 계층)

FF_ZoneSystem **위에 얹는** 디자이너용 Zone 설정 도구입니다. FF_ZoneSystem 소스는 한 줄도 수정하지 않습니다.
자세한 설명(비전공자용 가이드, 언리얼에서 Zone 설정하는 법)은 팀 Notion 문서를 보세요.

## 들어 있는 것

| 단계 | 클래스/파일 | 하는 일 |
|---|---|---|
| 1 | `UFFZAZoneDefinition` (Data Asset) | Zone 종류(ID·이름·색·허용 시설)를 에셋 하나로 정의 |
| 2 | `AFFZAAuthoredZone` (Zone 액터) | 정의를 고르면 값 자동 입력, Scale/회전 자동 정리, 실시간 검사 |
| 3 | `AFFZAPlot` / `AFFZADistrict` | 잠긴 땅(구획) / 시설 종류 정책(구역). FF_ZoneSystem External Policy로 적용 |
| 4 | `AFFZAZoneOverlayActor` | 건축 모드 바닥 색 표시(지형 추종). 건축 모드에 맞춰 자동 켜짐 |
| 5 | `FFFZAZoneCellMap` | 칸 지도: 겹침·이웃 자동 계산, 좌표→Zone O(1) 조회 |
| 6 | `AFFZAZoneLayoutManager` | 에디터 도구 상자(만들기·정리·용접·이웃 채우기·ID 동기화·전체 검사) |

## 처음 한 번

1. 에디터를 닫고 `.uproject` 우클릭 → Generate Visual Studio project files → 빌드.
2. 에디터에서 Tools > Execute Python Script → `Plugins/FF_ZoneAuthoring/Content/Python/ffza_create_overlay_material.py`
   (오버레이 머티리얼 생성).
3. 자동화 테스트: Tools > Session Frontend > Automation → `FF_ZoneAuthoring` 검색 → 실행.

## 원칙

- 판정의 최종 권위는 여전히 FF_ZoneSystem(1 cm 정밀 다각형)입니다. 이 플러그인은 규칙을 "추가"하고 저작을 "돕기"만 합니다.
- 기존 `AFFZSGameplayZone` 액터도 그대로 동작합니다. 새 Zone부터 `FF Authored Zone`을 쓰면 됩니다.
