# 길 시설 6종

참고 이미지 `C:\Users\kpo91\OneDrive\Desktop\Build\Road.png`에서 제작한
모래길, 아스팔트길, 나무 데크길, 석재 타일길, 헤링본 벽돌길, 콘크리트길이다.
각 타일은 400 × 400 × 8 cm이며 원점은 바닥 중심, 진행 방향은 +X다.

## 언리얼 에셋

- 메시: `/Game/Asset/FacilitiesAsset_HKH/Roads/SM_Road_<종류>`
- 공유 재질: `/Game/Asset/FacilitiesAsset_HKH/Roads/Materials/M_Road_Palette`
- 시설 정의: `/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Road/PDA_FFZS_Road_Surface_<종류>`
- 카테고리: `길` → `길 바닥`
- 설치비: 기존 `DT_FacilityCosts`의 `Cost.Road.Surface.<종류>` 행. 최초 추가 시 각 100 Gold.

각 길 PDA의 `Spec > Placement > 길 위 시설 설치 허용`을 켜면
그 길 위에 시설을 배치할 수 있고, 끄면 통행용 바닥으로만 사용한다.
이번에 추가한 여섯 정의는 모두 켜져 있다. 이 값은 설치 시 인스턴스 사양으로
복사되므로 PDA 편집은 이후 설치에 반영되며, 이미 설치한 길과 저장 데이터는
각자의 설정을 유지한다. 이동은 해당 인스턴스의 설정으로 다시 검증된다.

길끼리는 같은 자리에 겹칠 수 없고, 일반 시설끼리의 배치 제한은 유지된다.
길은 방문객 서비스 목적지나 대기열을 만들지 않으며 레벨 강화는 사용하지 않는다.

## 재현 및 확인

먼저 C++에서 Road 카테고리와 배치 설정을 컴파일한다. Blender 제작 스크립트는
`build_roads.py`이며 원본과 FBX/텍스처/렌더는 `SourceArt/Roads_20260910`에 있다.
이미 생성된 원본 경로에는 자동으로 다시 쓰지 않는다.

```powershell
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/import_roads.py -Label road_import
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/verify_roads.py -Label road_verify
& Tools/RoadFacilities/Run-RoadEditor.ps1 -Script Tools/RoadFacilities/capture_roads.py -Label road_capture -SuccessPattern ROAD_CAPTURE_PASS
& Tools/RoadFacilities/Run-RoadTests.ps1
```

임포트는 명시 LOD 3개, UV0/UV1, UCX 충돌 1개, 공유 재질을 연결한다.
4개 보정 메시로 1m=100cm와 +X/+Z 방향 및 FBX의 Y축 변환을 확인한다.
신규 길 패키지와 비용 표는 복원 가능한 저장 트랜잭션으로 기록하며 기존 비용 행은 보존한다.
재실행 시 기존 PDA의 수치와 상부 배치 설정은 보존한다.

보고서와 GPU 렌더는 `Saved/Roads_20260910`에 저장된다. 캡처 스크립트는
Entry 임시 월드에서 6종과 LOD 비교, 석재 타일 위의 기존 아이스크림 시설을 렌더한다.
해당 시설 이미지는 시각 확인용 배치이며 실제 설치/이동/중첩 허용은 Road 자동화 테스트가 검증한다.
프로덕션 레벨을 수정하거나 저장하지 않는다.
