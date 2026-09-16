# Road 길 6종과 Tile 바닥 6종

모래·아스팔트·목재·돌·헤링본·콘크리트 표면을 두 카테고리로 제공한다.
**Road(길)는 시설 설치를 막고, Tile(타일)는 위에 시설을 설치할 수 있다.**
둘 다 NPC가 통행할 수 있는 바닥이며 서비스 목적지나 대기열을 만들지 않는다.
상부 배치 정책은 카테고리로 결정한다. 이전 `길 위 시설 설치 허용` 값은 저장 호환용으로만 남고 배치 판정에는 사용하지 않는다.

| 구분 | Road | Tile |
| --- | --- | --- |
| 카탈로그 | 길 → 바닥 | 타일 → 바닥 |
| 시설 ID | `Road_Surface_<종류>` | `Tile_Surface_<종류>` |
| PDA 폴더 | `/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Road` | `/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Tile` |
| PDA 이름 | `PDA_FFZS_Road_Surface_<종류>` | `PDA_FFZS_Tile_Surface_<종류>` |
| 기본 비용 키 | `Cost.Road.Surface.<종류>` | `Cost.Tile.Surface.<종류>` |
| 다른 시설과 겹침 | 본체·여유 공간·대기열 모두 차단 | 상부 시설의 배치 허용 |

메시는 `/Game/Asset/FacilitiesAsset_HKH/Roads/SM_Road_<종류>`, 재질은 같은 폴더의
`Materials/M_Road_Palette`를 공유한다. Tile은 Road의 메시와 썸네일을 재사용한다.
원본 메시 규격은 400 × 400 × 8 cm, 원점은 바닥 중심, 진행 방향은 +X다.
실제 설치 크기·높이·드래그 간격은 **각 PDA에 현재 저장된 값**을 사용한다.

## 기존 에셋 분리

C++의 Tile 카테고리를 먼저 빌드하고 작업 중인 에디터를 종료한 뒤 실행한다.
이미 제작한 메시를 다시 임포트할 필요 없이 다음 명령으로 분리와 저장 검증을 할 수 있다.

```powershell
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/split_road_tiles.py -Label road_tile_split -SuccessPattern ROAD_TILE_SPLIT_PASS
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/verify_road_tiles.py -Label road_tile_verify -SuccessPattern ROAD_TILE_VERIFY_PASS
```

분리는 여섯 Road의 기존 경로·ID·크기·높이·메시·드래그 설정을 보존하고 이전 상부 허용 플래그만 끈다.
Tile이 없으면 Road PDA 전체를 복제하고 ID·카테고리·표시 이름·설치 비용 키를 Tile 값으로 바꾼다.
Tile의 최초 가격 행은 원본 Road가 현재 참조하는 가격 행 전체를 복제하므로 제작자가 조정한 가격을 따른다.
기존 Tile PDA와 가격 행은 재실행해도 덮어쓰지 않는다. 판매 반환 금액도 기존 경제 시스템의 설치 비용 기준을 따른다.
모든 대상과 가격 키를 먼저 확인하고, 13개 패키지 저장을 복구 가능한 트랜잭션으로 보호한다.
저장 실패 시 기존 패키지를 복원하며 오류가 발생한 임시 에디터는 저장 없이 종료해야 한다.

새 에디터 검증은 12개 PDA의 전체 Spec, 드래그 설정, 카탈로그 표시, 썸네일과 전체 비용 표를
마이그레이션 직후 보고서와 비교한다. 제작자가 이후 수정한 내용을 검증하려면 분리를 다시 실행해
현재 값을 기록한 뒤 검증한다. 이미 분리한 상태의 재실행은 패키지를 저장하지 않는다.
보고서는 `Saved/Roads_20260910/road_tile_split_*.json`에 기록된다.

이 에셋 분리 스크립트는 저장 게임 파일을 직접 수정하지 않는다. 런타임의 버전별 저장 복원은
스키마 7 이하에서 상부 설치를 허용하던 Road를 Tile로 변환해 기존 상부 시설 배치를 유지한다.
상부 설치를 막던 이전 Road는 Road로 남는다. 스키마 8부터 Road는 상부 설치를 항상 차단한다.

## 배치와 드래그

Road/Tile끼리는 겹칠 수 없고, 이미 설치된 일반 시설 아래에 바닥을 새로 끼워 넣을 수 없다.
Tile을 먼저 설치한 뒤 시설을 올린다. 상부 시설을 지지하는 Tile은 해당 시설을 먼저 옮겨야
이동·철거·크기 변경할 수 있다. 시설끼리의 충돌과 건설 구역 제한은 계속 적용한다.
NPC는 두 바닥의 실제 영역과 높이를 반영하며 바닥 자체를 회피 벽으로 취급하지 않는다.

PDA 최상위 `Placement > Enable Drag Placement`를 켜면 마우스 왼쪽 버튼을 누른 채
같은 시설을 연속 설치할 수 있다. 새 Road는 기본적으로 켜지고 새 Tile은 원본 Road 설정을 복사한다.
재임포트·분리 시 제작자가 편집한 설정을 유지한다. 직사각형 크기와 회전에 따른 설치 간격을 사용하며
각 지점마다 기존 배치·충돌·비용 검사를 적용한다. 상세 조작은 `DRAG_PLACEMENT_GUIDE_KO.md`에 있다.

`enable_drag_placement.py`와 `verify_drag_placement.py`는 과거 여섯 Road의 드래그 기능을
최초 납품할 때 사용한 별도 마이그레이션이다. Road/Tile 분리 후에는 위의 새 검증을 사용한다.
분리 과정에서 Road Spec의 호환 플래그를 바꾸므로 분리 전 드래그 보고서와 전체 Spec 해시는 달라질 수 있다.

## 원본 임포트와 회귀 검사

Blender 제작 스크립트는 `build_roads.py`, 원본과 FBX/텍스처/렌더는 `SourceArt/Roads_20260910`에 있다.
이미 생성된 원본 경로는 자동으로 다시 쓰지 않는다. 최신 임포터는 Road를 시설 설치 불가로 만들고
Tile 분리를 함께 실행하므로 최초 임포트에서도 두 카테고리를 모두 생성한다.

```powershell
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/import_roads.py -Label road_import
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/verify_roads.py -Label road_verify
& Tools/RoadFacilities/Run-RoadTests.ps1
```

메시 검증은 명시 LOD 3개, UV0/UV1, UCX 충돌 1개, 공유 재질과 4개 축/단위 보정 메시를 확인한다.
카탈로그·배치·저장 복원·경제·NPC 관련 회귀는 Unreal 자동화 테스트로 확인한다.
`capture_roads.py`는 공유 표면 메시와 Tile 위 시설의 외형을 확인하는 렌더 도구다.
렌더는 실제 설치 허용 여부를 증명하지 않는다. 실행하지 않은 검사 결과는 성공으로 기록하지 않는다.
