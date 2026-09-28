# 한국 국가 영역 선택 시험

> 2026-09-21 현재: P12-C 8개국 구현·임포트·PIE 검증 완료. 아래 P12-B 설명은 한국 시험 단계의 기록이다. 재현 순서와 자료 계약은 문서 마지막의 **P12-C — 8개국 확장**을 따른다. 최종 자료는 `world_verify_v003/report.json`, `WorldAutomation_v002/Report/index.json`, `world_cleanup.json`이다.

P12-B는 한국부터 지구 위 영역 클릭과 외곽선·옅은 채움을 적용한다. 등록되지 않은 국가는 기존 조형물 선택을 사용한다. 카탈로그의 위치·직접 지정한 보기 지점·카메라 접근량은 변경하지 않는다.

## 구성

- `prepare_korea.py`: geoBoundaries 한국 ADM1 17개를 합쳐 내부 시·도 선을 지우고 국가 Polygon/MultiPolygon과 1024² 마스크를 같은 좌표에서 만든다.
- `collect_surface.py`: 현재 UE에 임포트된 P05 Top LOD0에서 한국 주변 삼각형·UV와 원본 높이 텍스처를 읽는다. 에셋을 저장하지 않는다.
- `prepare_surface.py`: UV 텍셀 중심에서 높이를 선형 보간하고 기존 WPO와 같은 산 높이 가중치를 만든다.
- `import_korea.py`: 기존 SunnyLand의 파생 재질과 영역 DataAsset을 새 `CountryRegions` 폴더에 생성하고 Frontend 액터에 참조만 연결한다. 원본 재질과 카탈로그 해시를 확인한다.
- `region_feedback.py`: 기본 반사광·독립 호버·선택 강조를 정의하는 공용 재질 패치. 최초 생성과 기존 에셋 갱신이 같은 코드를 사용한다.
- `upgrade_feedback.py`: 이미 생성된 영역 부모 재질만 백업 후 갱신한다. 지형·마스크·보기 위치를 변경하지 않는다.
- `import_korea_flag_padding.py`: `generate_frontend_casual_art.js --flag KR`로 만든 태극기만 재임포트한다. 목록·상세 카드의 같은 텍스처 경로를 유지한다.
- `verify_korea.py`: 실제 Frontend PIE에서 공개 API/버튼 델리게이트, 한국/인접 지역 광선, 재질·조형물 복원, 사용자 지정 카메라, 화면 크기를 검사하고 PNG를 남긴다. 물리 마우스 입력 검사는 별도다.

## 재생성 순서

1. `prepare_korea.py`를 실행한다. 입력·옵션은 `--help`를 따른다. 원본 파일과 출처·라이선스는 `THIRD_PARTY_NOTICES.md`에 기록한다.
2. 사용자 에디터가 닫힌 상태에서 `Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/atlas_regions/collect_surface.py -Label p12b_collect_surface`로 실제 메시를 추출한다. NullRHI는 사용하지 않는다.
3. `prepare_surface.py`를 실행한다. Python에 NumPy/Pillow가 필요하다.
4. C++ Editor 타깃을 빌드한다. 최초 생성 시 `run_editor.ps1 -Script Tools/ui/atlas_regions/import_korea.py -Label p12b_import`를 실행한다. 생성된 결과가 이미 있으면 임포터는 중단한다. 임포터를 업데이트 도구로 사용하지 않는다.
5. `Saved/AtlasRegions/verify_korea_request.json`에 `{"output":"고유한_검사폴더","include_ultrawide":false}`를 저장하고 `verify_korea.py`를 같은 실행기로 실행한다. 결과 폴더는 미리 만들지 않는다.

## 조절과 좌표 계약

- `MI_AtlasRegion_Land`: `RegionOutlineColor` 외곽선 색, `RegionFillColor` 기본 채움 색. `RegionFillAmount`는 이전 값을 유지한 전체 채움 배율(0.18이 기준)이다.
- `RegionIdleFillAmount`(.06), `RegionHoverFillAmount`(.32), `RegionSelectedFillAmount`(.52): 각각 기본·호버·선택 채움. `RegionHoverColor`와 `RegionSelectedColor`로 상태별 색을 바꾼다.
- `RegionOutlineWidth`(1.1): 화면 픽셀 기준 경계 두께. 호버/선택은 추가로 넓어진다. `RegionGlowStrength`(.55): 약한 자체 발광.
- `RegionSheenStrength`(.24), `RegionSheenSpeed`(.18), `RegionSheenWidth`(.16): 반사광 강도·초당 반복 횟수·폭. `RegionSheenDirection`은 1이면 지도 서→동, -1이면 동→서다. 구체 회전 시 화면상의 방향은 달라진다. Strength=0이면 반사광이 꺼진다.
- `RegionEnabled`, `RegionHovered`, `RegionSelected`, `RegionMotionEnabled`, `RegionMask`, `RegionBounds`는 런타임에서 지정한다. 호버와 선택을 독립적으로 유지하며, 움직임 감소는 반사광만 끄고 정적인 상태 표시는 남긴다.
- 경계 좌표는 `(경도, 위도)`. Bounds는 `(서, 남, 동, 북)`. 지구 Top의 UV는 `경도=U×360−180`, `위도=90−V×180`이다.
- 마스크는 R=내부, G=경계, B=외부 연한 띠, A=255인 선형 데이터다. 현재 셰이더는 R/G를 사용하며 화면 미분으로 경계가 멀리서 사라지는 현상을 완화한다.
- 클릭은 바다 구체 위경도를 추정하지 않고 실제 Top 삼각형의 보간 UV를 사용한다. MPC의 `LandThickness`와 `MountainHeight` 변경도 월드 단위 WPO로 반영한다.
- 원본 지구 메시·UV를 교체하면 `collect_surface.py`와 `prepare_surface.py`부터 다시 만들어야 한다. 임의의 메시 이동·스케일 변경을 자동 추적하는 시스템은 아니다.

## 범위와 한계

- 한국 본토·제주·울릉 등 원본에 있는 52개 도형을 사용한다. 원본에 없는 독도 도형을 임의로 만들지 않았다.
- 국가 경계와 지구 해안은 서로 다른 원본이다. 베벨된 해안과 아주 작은 섬에서는 표시가 Top 표면에 잘릴 수 있다. 현재 육지 메시를 새로 만들지는 않는다.
- 한국에만 영역 데이터가 있으므로 한국 조형물만 나라 선택 화면에서 숨긴다. 타이틀에서는 원래대로 복원한다. 8개국 전체 확장은 P12-C다.
- 선택 화면에 들어오면 한국은 기본 표시를 유지한다. 선택한 한국 위에 다시 마우스를 올려도 별도 반응이 있다. 국가 위에서는 손 모양 커서와 나라 이름 툴팁을 표시하고, 드래그할 때는 이를 해제한다.
- 자동화 테스트 필터는 `FestaFiesta.UI.AtlasRegions`. 주변 기존 Atlas 테스트도 함께 실행한다. 실제 물리 마우스·게임패드, 패키징·콘솔 검증은 별도로 기록한다.

## P12-C — 8개국 확장

### 상태와 보존 범위

- `confirmed`: 나머지 7개국 BR·DE·JP·US·ES·TH·AU의 경계·마스크 준비 완료. 공식 원본의 655개 도형·77,026개 닫힌 링 정점을 유지했고, 본토·섬·인접국·바다 45개 지점 및 동일 경도의 ±360° 표현 135개 검사를 통과했다.
- `confirmed`: `Saved/AtlasRegions/import_world.json` passed, `world_verify_v003/report.json` 실제3화면비36검사/89실제광선/7메시없는섬표본/PNG25장·104.078초 passed. 최종 Atlas29개 성공, 기존 에셋해시 보존·PIE종료·시작 전 ini 복원. 물리 커서/입력은 별도 확인이다.
- 기존 KR 도형·1024² 마스크·한국 시험 재질은 그대로 둔다. 전 세계판 `M_AtlasRegion_World`·`MI_AtlasRegion_World`를 별도로 만들고, 기존 `DA_AtlasCountryRegions`에 8개국과 전 세계 클릭 표면을 넣는다.
- 지구 메시·텍스처·해안·산맥, `DA_FestivalAtlas`의 Location/Inspection Location/Approach, Frontend의 카메라 위치는 수정 대상이 아니다.
- 8개국의 등록 순서는 **KR, BR, DE, JP, US, ES, TH, AU**다. 나라 선택 화면에서 등록된 국가의 조형물을 숨기고, 타이틀로 돌아가면 기존 표시를 복원하는 범위다.

### 원본과 출력 계약

- 원본: [Natural Earth Admin 0 Countries 1:10m](https://www.naturalearthdata.com/downloads/10m-cultural-vectors/10m-admin-0-countries/) v5.1.1, WGS84 경위도.
- 다운로드: `https://naciscdn.org/naturalearth/10m/cultural/ne_10m_admin_0_countries.zip`.
- 공식 [Public Domain 사용 조건](https://www.naturalearthdata.com/about/terms-of-use/)과 가공 내용은 `THIRD_PARTY_NOTICES.md`에 임포트 전에 등록했다. 원본의 기본 de facto 경계를 그대로 사용한다.
- ZIP SHA-256: `ce1ac7036499a0edd641fbc093cd209a98f96a49d2eca8480aaacad35138a7f6`. 생성기는 다른 해시·버전·좌표계를 조용히 받아들이지 않는다.
- 원본 ZIP과 조건 원문 사본: `SourceArt/UI/Atlas/CountryRegions/World/source/`. 새 작업 환경에서는 위 공식 ZIP을 이 폴더에 확보한다. `SourceArt/`는 로컬 전용이므로 Git 체크아웃만으로 원본이 생기지는 않는다.
- `World/manifest.json`의 `countries`에는 `country_id`, `region_file`, `texture_file`, `validation_file`, `bounds`, `size`, 생성물 해시가 있다. 각 상대 경로는 `World/` 기준이다.
- 각국 `<ID>/<ID>.region.json`은 기존 KR와 같은 `country_id`, `bounds`, `polygons[{outer, holes}]`, `texture`, `metadata` 구조다. 마스크 채널은 기존과 같은 R=내부/G=경계/B=외부 연한 띠/A=255, 선형 데이터다.
- `World/validation.json`은 전체 데이터 검사 요약이며, `<ID>/validation.json`에는 실제 검사 좌표·예상 포함 여부·래스터 값이 있다. `prepare_world.py`는 KR 파일과 전 세계 클릭 표면 `World/surface.json`을 덮어쓰지 않는다.

| 국가 | 마스크 | 비고 |
| --- | --- | --- |
| KR | 1024×1024 | 기존 geoBoundaries 결과 유지 |
| BR·JP·US·AU | 2048×2048 | 큰 영역·떨어진 섬 포함 |
| DE·ES·TH | 1024×1024 | 원본 도서 포함 |

미국은 알래스카·하와이·알류샨을 하나의 좁은 연속 경도 범위로 보관한다. 최종 Bounds는 `[-188.8, 17.7, -65.7, 72.7]`이며, 동경 179°는 이 범위에서 서경 181°와 같은 좌표로 해석한다. 런타임과 셰이더는 모두 `lon += 360 * round((bounds_center - lon) / 360)` 규칙을 사용해야 한다. 단순히 -180~180에 잘라 버리거나 US Bounds를 전 세계 너비로 확대하지 않는다. 도형의 위치·크기·모양 자체는 바뀌지 않는다.

### 전 세계판 재현 순서

아래 명령은 리포 루트에서 실행한다. Python 3.10+와 NumPy/Pillow가 필요하다. UE 스크립트는 사용자 에디터가 닫힌 상태에서 실행하고, 메시 추출·재질 임포트·PIE에는 `-NullRHI`를 사용하지 않는다. `-Label`과 검증 `output`은 실행마다 새로운 이름으로 바꾼다.

1. 경계·마스크를 생성한다. 원본을 확인만 할 때는 `--inspect`를 붙인다.

   ```powershell
   python Tools/ui/atlas_regions/prepare_world.py
   ```

2. 실제 임포트된 P05 Top 8개 메시의 전 세계 LOD0·UV와 높이 텍스처를 추출한다. 다른 대륙을 관통해서 고르지 않도록 선택 국가 바깥의 육지도 포함한다.

   ```powershell
   Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/atlas_regions/collect_world_surface.py -Label p12c_collect_world
   ```

3. 추출한 표면에서 기존 WPO와 같은 높이 가중치를 계산한다. 한국용 기본 입출력을 사용하지 않도록 모든 경로와 `ALL`을 명시한다.

   ```powershell
   python Tools/ui/atlas_regions/prepare_surface.py --surface Saved/AtlasRegions/WorldSurface/surface_raw.json --height Saved/AtlasRegions/WorldSurface/height.png --output SourceArt/UI/Atlas/CountryRegions/World/surface.json --country-id ALL --exporter Tools/ui/atlas_regions/collect_world_surface.py
   ```

4. 프로젝트의 `FestaFiestaEditor Win64 Development` 타깃을 빌드한다. 변경된 국가 영역 C++가 성공적으로 빌드된 이후 임포트한다.

5. 7개국 마스크·8슬롯 전 세계 재질을 생성하고 기존 영역 DataAsset을 확장한다. 원본 보호 해시와 결과 보고서를 확인한다. 이미 생성된 에셋이 있으면 임포터의 중단 사유를 확인하며, 보호 검사를 제거하거나 임의로 덮어쓰지 않는다.

   ```powershell
   Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/atlas_regions/import_world.py -Label p12c_import_world
   ```

6. `Saved/AtlasRegions/verify_world_request.json`에 `{"output":"world_verify_고유한이름"}`을 저장한다. 그 이름의 결과 폴더는 미리 만들지 않는다. 실제 PIE 검사와 캡처를 실행한다.

   ```powershell
   Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/atlas_regions/verify_world.py -Label p12c_verify_world
   ```

7. 실행별 `report.json`과 PNG를 확인한다. 기본 1600×900에서 8개국, 960×720·1680×720에서 대표 국가를 검사한다. 목록 선택·호버/선택 분리·나라 광선 판정·날짜변경선·움직임 감소·타이틀 복원·카메라 보존이 검사 범위이며, 물리 마우스 조작 확인은 별도로 기록한다. 기존 `FestaFiesta.UI.Atlas` 자동화 회귀도 실행한다.

### 도서와 클릭 표면의 한계

- Natural Earth 1:10m에 없는 도서는 추가하지 않는다. 매우 작은 섬은 원본 폴리곤에는 있어도 마스크 텍셀이나 현재 지구 메시보다 작을 수 있다.
- 데이터의 논리적 포함 판정과 실제 보이는 Top 메시를 향한 광선 판정은 별도다. 원본 도형에 포함되어도 해당 지점에 육지 메시가 없다면 지구 위 클릭 대상으로 늘려 잡지 않는다. 왼쪽 국가 목록은 계속 사용할 수 있다.
- 미국의 날짜변경선 양쪽 섬, 일본의 여러 섬, 스페인 카나리아·발레아레스, 호주 태즈메이니아·매쿼리를 원본 그대로 보존했다. 이 범위를 본토 중심 카메라가 모두 한 화면에 보여 준다고 보장하지 않으며, 사용자가 지정한 보기 위치를 우선한다.
