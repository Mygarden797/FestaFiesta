# 건설·구역 코드 관리 안내

관리 범위는 게임 모듈의 `Building`, `Zone`, `MapOverview` 연동 코드와 `FF_MapOverview`, `FF_ZoneAuthoring`, `FF_ZoneSystem` 플러그인이다. Unreal 5.7의 기존 모듈 구분과 `FF_` 타입·파일 접두사를 따른다.

## 시설 화면과 모델 위치

아래 경로는 `Source/FestaFiesta/Public`과 `Private`에 같은 기준으로 적용한다. Widget Blueprint 제작 코드는 `Source/FestaFiestaEditor/Private/Building`의 대응 폴더에서 관리한다.

| 이전 위치 | 현재 위치 | 담당 기능 |
| --- | --- | --- |
| `Building/Codex` | `Building/FacilityCollection` | 시설 조합 도감의 규칙·발견·보상·저장 |
| `UI/FacilityCodex` | `Building/FacilityCollection/UI` | 도감 화면·카드·미리보기·표시 제어 |
| `UI/Facility` | `Building/FacilityInfo` | 시설 정보·조명·프로그램 선택·시너지 표시 |
| `UI/FacilityRevenue` | `Building/FacilityRevenue` | 수익 순위·월드 배지 |

`FactioityRevenue`라는 폴더는 없으며 실제 관리 대상은 `FacilityRevenue`다. 콘텐츠 에셋은 `Content/FestaFiesta/Building/FF_WBP_Building`에서 관리한다. 파일명에서는 `Codex`만 제거했으며 중복·끝 구분자를 정리했다. 소스·도구 파일 63개는 include와 생성 헤더를 갱신했고, 에셋 34개는 Unreal AssetTools로 이름을 바꿔 참조 패키지를 저장했다. `FF_FacilityCodex*` 반영 타입명과 자동화 등록 이름은 Blueprint·설정·저장 호환성을 위해 유지한다.

## 구현·테스트·제작 도구의 경계

- 공개 API와 반영 타입은 `Public`, 구현과 내부 정책은 `Private`에 둔다. 구역 플러그인의 기존 공개 include 경로는 유지한다.
- 자동화 테스트와 테스트 전용 관찰자·정책은 검증 대상 기능 아래 `Tests`에 둔다. 여러 기능을 연결하는 테스트만 공통 `Tests`에 둔다.
- `Building/Testing`은 사람이 사용하는 개발 패널이다. 패널 자체는 자동화 파일이 아니며, 검증 코드는 `Building/Testing/Tests`에 둔다.
- DataTable 생성·에셋 저장·Widget Blueprint 제작은 에디터 모듈이 담당한다. 시너지 표 생성기와 CSV·참조·추가 작업 검증은 `FestaFiestaEditor/Private/Building/Synergy`에 있다.
- `FF_ZoneSystemTests`는 프로젝트 시설을 사용하는 에디터 통합 테스트 모듈로 유지한다. 런타임 플러그인에서 게임 모듈을 의존하도록 합치지 않는다.
- 콘솔 명령으로 등록된 제작기는 텍스트 호출 검색만으로 사용 여부를 판단할 수 없다. 제거할 때는 명령 등록과 Blueprint·에셋 참조도 확인한다.

세부 구조는 [Building 안내](../Source/FestaFiesta/Private/Building/README.md), [Zone 연동 안내](../Source/FestaFiesta/Public/Zone/README_KO.md), 각 플러그인의 README에서 확인한다. 파일 이동 시 `Collaboration/Refactor_Ownership.json`의 현재 경로와 이동 목적지를 함께 갱신해 코드 찾아보기와 작성 이력 추적을 유지한다.

## 정리하면서 개선한 부분

- 시설 도감 모델이 평가기와 같은 규칙 원본을 중복 보관하던 배열을 제거했다. 화면 항목 생성 후 규칙의 소유권을 평가기로 넘긴다.
- 시너지 공간 색인을 다시 만들 때 시설 수만큼 배열·맵 용량을 미리 확보한다.
- 미니맵 다각형 클리핑은 두 작업 배열을 재사용하여 각 변마다 발생하던 임시 배열 할당을 줄인다. 원의 반지름과 분할 수는 공통 상수로 관리한다.
- 시너지 에셋 제작기를 에디터 모듈로 옮긴 뒤 게임 모듈에서 사용하지 않는 `AssetTools` 직접 의존성을 제거했다.
- 중복 include와 사용하지 않는 지도 브리지 include를 제거했다. 공개 API·시설 ID·에셋 식별자는 변경하지 않았다.

## 검증

프로젝트 경로의 `!` 문자가 `Build.bat`의 지연 확장에 영향을 줄 수 있으므로 에디터 빌드는 팀의 직접 UBT 실행 도구를 사용한다.

```powershell
& ./Tools/Build/Build-Editor.ps1 -EngineRoot 'C:/Unreal/UE_5.7'
& 'C:/Unreal/UE_5.7/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -m unittest discover -s Tools/Build -p 'test_*.py'
```

관련 자동화 필터는 `FestaFiesta.Building`, `FestaFiesta.FacilityCodex`, `FestaFiesta.UI.Facility`, `FestaFiesta.UI.PrepBuild`, `FestaFiesta.ZoneIntegration`, `FF_MapOverview`, `FF_ZoneAuthoring`, `FF_ZoneSystem`이다. 콘텐츠·카탈로그·대기열 계약도 관련 테스트에 포함되므로 파일 위치가 바뀌었다는 이유로 등록 이름을 바꾸지 않는다. 화면 렌더링 테스트는 실제 RHI의 `-RenderOffscreen`으로 실행하며, 실행별 `-UserDir`를 분리해 플레이어 저장과 섞이지 않게 한다.

이번 이동 목록·include 대조·자동화 등록 대조 결과는 로컬 `Saved/SourceOrganization`에 기록한다. 이 문서의 실행 방법과 실제 검증 결과는 구분하며, 최종 빌드·자동화 결과는 해당 실행 로그와 리포트를 기준으로 확인한다.

2026-11-02 실행 결과와 해결한 UI·콘텐츠 계약은 [검증 기록](BuildingZone_Validation_20261102_KO.md)에 정리했다.
