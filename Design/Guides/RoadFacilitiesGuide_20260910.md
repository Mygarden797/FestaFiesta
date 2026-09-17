# Road와 Tile 표면 시설

모래·아스팔트·목재·돌·헤링본·콘크리트의 여섯 표면을 **Road(길)**와 **Tile(타일)**로 나눈다.
Road는 다른 시설을 설치할 수 없는 통행용 바닥이고, Tile은 시설을 올릴 수 있는 바닥이다.
배치 검증·미리보기·점유 영역 등록·저장 복원에서 카테고리로 동일한 규칙을 적용한다.

## 카탈로그와 데이터

기존 Road 열거형 값과 `Road_Surface_<종류>` ID는 유지하고 Tile 열거형 값을 뒤에 추가한다.
기존 카테고리의 저장값은 바뀌지 않는다. 카탈로그에는 길과 타일을 별도로 표시한다.
새 Tile ID는 `Tile_Surface_<종류>`다. 종류는 `Sand`, `Asphalt`, `Wood`, `Stone`, `Herringbone`, `Concrete`다.

시설 정의는 `/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Road`와 `/Tile`에 두고,
두 종류는 `/Game/Asset/FacilitiesAsset_HKH/Roads`의 메시·재질과 기존 썸네일을 공유한다.
원본 제작 규격은 400 × 400 × 8 cm이나, 실제 배치와 드래그 간격은 각 PDA의 작성된 크기를 따른다.
최초 Tile 복제 시 Road에 저장된 크기·높이·회전·입력 설정을 그대로 복사한다.

비용 키는 각각 `Cost.Road.Surface.<종류>`와 `Cost.Tile.Surface.<종류>`를 사용한다.
새 Tile 비용 행은 Road가 현재 참조하는 행을 복제한다. 기존 Road/Tile 가격을 고정 초기값으로 되돌리지 않는다.
판매 반환액은 기존 경제 시스템의 설치 비용 기준을 그대로 사용한다.

## 시설 설치 규칙

| 대상 바닥 | 일반 시설 본체·여유 공간·대기열 | NPC 통행과 표면 높이 |
| --- | --- | --- |
| Road | 중복 배치 차단 | 반영 |
| Tile | 상부 배치 허용 | 반영 |

`bAllowFacilityPlacementOnTop`은 이전 저장값을 읽기 위한 필드로 남지만 배치 정책에 사용하지 않는다.
Road에 과거 허용 값이 저장되어 있어도 Road 위에는 시설을 설치할 수 없고,
Tile은 해당 과거 값과 무관하게 상부 배치를 지원한다. 새 시설을 만들 때는 목적에 맞는 카테고리를 고른다.

Road/Tile끼리 중복 설치할 수 없으며 일반 시설끼리의 충돌과 건설 구역 제한도 유지된다.
상부 시설이 있는 Tile은 해당 시설을 먼저 옮겨야 이동·철거·크기 변경할 수 있다.
기존 시설 밑으로 바닥을 새로 끼워 넣지 못하며 Tile을 먼저 설치한 뒤 시설을 올린다.
시설 설치 높이는 Tile 윗면을 반영하고 메시 비동기 로딩 중에는 정의의 높이를 사용한다.

Road와 Tile은 서비스·대기열·관람객 목적지 및 내비게이션 회피 벽이 아니다.
NPC는 지형 높이와 가까운 두 바닥의 정확한 영역·윗면을 따라 이동한다.
표면 목록은 배치 리비전과 저장 복원 시 갱신하고 NPC별 조회는 해당 공간 구획을 사용한다.

## 분리와 확인

C++ 빌드 후 작업 중인 에디터를 종료하고 아래 명령을 순서대로 실행한다.

```powershell
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/split_road_tiles.py -Label road_tile_split -SuccessPattern ROAD_TILE_SPLIT_PASS
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/verify_road_tiles.py -Label road_tile_verify -SuccessPattern ROAD_TILE_VERIFY_PASS
& Tools/RoadFacilities/Run-RoadTests.ps1
```

분리 스크립트는 6개 Road의 허용 호환 플래그만 끄고, 없는 Tile을 복제하며, 없는 Tile 비용 행만 추가한다.
기존 Tile의 작성값과 가격을 보존하고, 저장 전 패키지 백업으로 실패 시 전체 변경을 복구한다.
새 에디터 검증은 12개 실제 PDA와 전체 가격 표를 분리 직후 보고서와 비교한다.
한 번 더 분리를 실행하면 패키지 저장 없이 완료되어야 한다. 최신 `import_roads.py`도 동일한 분리를 실행한다.

이 에셋 도구는 저장 게임 파일을 직접 수정하지 않는다. 런타임 저장 복원은 스키마 7 이하의
상부 설치 허용 Road를 Tile로 변환하여 기존 시설 배치를 유지한다. 상부 설치를 막던 Road는
그대로 유지한다. 새 스키마 8부터는 Road의 상부 설치를 항상 차단한다.
버전 변환 및 기존 상부 시설 복원은 저장 호환 자동화 검사로 확인한다.

| 확인 항목 | 증거 |
| --- | --- |
| 기존 Road 크기·ID·메시·입력값 보존 및 Tile 6종 저장 | `road_tile_split_migration.json`, `road_tile_split_verification.json` |
| 기존 비용 보존·Tile 비용 연결 | 분리 보고서와 카탈로그·경제 자동화 |
| Road 차단, Tile 허용, 바닥끼리 및 시설끼리 중복 차단 | `FF_ZoneSystem.Road` 자동화 |
| Tile 지지 높이·상부 시설 보호·저장 복원 일관성 | `FF_ZoneSystem.Road` 자동화 |
| 두 바닥의 NPC 높이·회피 제외·이동·삭제·저장 복원 | NPC 브리지 및 `FestaFiesta.Crowd.Road` 자동화 |
| 별도 카탈로그 분류와 기존 열거형 값 보존 | 이름 규칙·UI·카탈로그 자동화 |
| 실제 메시·재질·LOD·충돌·크기 | `verify_roads.py`와 Blender 보고서 |

보고서·로그·실행 기록은 `Saved/Roads_20260910`에 남긴다. 상세 도구 사용법은
`Tools/RoadFacilities/README.md`를 따른다. 실행하지 않은 검사는 `NOT_RUN`으로 기록한다.
