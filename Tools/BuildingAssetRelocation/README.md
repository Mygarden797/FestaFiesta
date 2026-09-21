# 건축 에셋 폴더 정리

건축 UI의 루트는 `/Game/FestaFiesta/Building/FF_WBP_Building`, 시설 정의의 루트는 `/Game/FestaFiesta/Building/FF_Data_Building`이다.

| UI 하위 폴더 | 내용 |
| --- | --- |
| `Catalog` | 건축 카탈로그 패널, 타일, 카테고리 의존성 및 쿠킹 라벨 |
| `Catalog/Legacy` | 기존 카탈로그 호환 위젯 |
| `Catalog/Images` | 카탈로그 이미지와 PNG 원본 |
| `Controls` | 건축 도구, 조합 표시, 카테고리 선택 및 재질 |
| `FacilityCodex` | 전체 화면 조합 도감, 카드, 보상 아이콘 및 유리·이리디센스 재질 |
| `Facility` | 시설 정보, 조명, 유료 프로그램, 조합 효과 및 재질 |
| `FacilityRevenue` | 시설 수익 순위와 월드 배지 |
| `Shared` | 준비 HUD 바, 탭 버튼, 선택 버튼 |
| `Guide`, `Tooltips`, `Feedback`, `Debug`, `Images` | 건축 도움말, 툴팁, 배치 피드백, 디버그, 알림 이미지 |

시설 데이터는 Booth, Info, Lighting, Performance, PhotoSpot, Rest, Road, Safety, Tile, Toilet 분류와 기존 에셋 이름을 유지한다. 시설 ID와 Primary Asset ID는 변경하지 않는다. 공유 폰트는 `/Game/Asset/Font`에서 계속 사용한다.

## 재현과 검증

`prepare_relocation.py`는 이동 전 목록과 백업을 `Saved/BuildingAssetRelocation_20260921`에 기록한다. 이 도구의 이전 경로는 일회성 이동 기록이며, 런타임 경로로 사용하지 않는다.

1. `verify_assets.py`로 이동 전 클래스·시설 ID·원본 이미지 경로를 기록한다.
2. 에디터 명령 `FF.MoveBuildingAssets`는 manifest의 에셋만 Unreal AssetTools로 이동하고 참조 패키지를 저장한다.
3. `update_references.py text`로 코드·설정·생성 도구의 경로를 수정한다. 경로를 조립하는 준비 HUD는 Build와 공용 버튼 경로를 별도로 처리한다.
4. `update_references.py sources`로 해시를 확인한 PNG 원본만 새 위치로 이동한다.
   `repair_import_sources.py`로 이미지 픽셀을 재수입하지 않고 원본 연결과 경로 메타데이터를 갱신한다.
5. 새 코드 빌드 후 `FF.FixupBuildingAssetRedirectors`로 참조를 다시 저장하고 참조가 없는 이동 리다이렉터만 정리한다.
6. `verify_assets.py`와 시설·카탈로그·도감 자동화 테스트로 결과를 검증한다.

`DefaultGame.ini`는 새 UI 루트를 AlwaysCook 대상으로 포함하고 시설 Primary Asset 검색 경로를 새 데이터 루트로 지정한다. 경로를 포함한 예전 Saved의 시설 저작 도구 캐시는 새 경로에서 재생성해야 한다.

## 2026-09-21 검증 결과

- UI 129개, 시설 정의 114개, 시설 썸네일 12개, PNG 원본 49개 이동 완료.
- 255개 에셋의 클래스·시설 설정·ID·이미지 원본 대조 통과. 이전 경로의 에셋·리다이렉터 0개.
- 에디터 및 게임 Development 빌드 성공. 자동화 47개 통과(기존 경고 포함 2개).
- Windows 쿠킹 1,942개 패키지 성공(오류 0개). 이동한 에셋 255개 모두 새 경로로 포함.
- 기존 의존 에셋이 긴 Windows 출력 경로 제한에 걸려, 쿠킹 검증은 `%TEMP%/FFBC_20260921`의 별도 출력에서 수행했다. 릴리스·스테이징 결과는 변경하지 않았다.
- 기존 시설 메시 저작 manifest 내보내기는 메시의 소유 루트 검사로 완료되지 않았다. 이동 전부터 사용하던 `/Game/Asset/FacilitiesAsset_HKH` 메시 경로를 해당 도구가 허용하지 않는 문제이며, 메시나 소유권 플래그는 변경하지 않았다. 위 게임·UI·쿠킹 검증은 모두 통과했다.

상세 결과: `Saved/BuildingAssetRelocation_20260921/validation_summary.json` 및 `Report/index.json`.
