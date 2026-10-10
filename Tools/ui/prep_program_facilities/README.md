# 프로그램 시설 목록 · 위치 구분

설치 여부와 관계없이 프로그램에 맞는 시설 종류 목록을 유지한다. 각 종류에는 `미설치` 또는 `✓ 설치 N곳`을 표시하며, 누르면 기존 건축목록으로 이동한다.

## 에디터 조정

| 에셋 | 조정할 항목 |
| --- | --- |
| `/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram` | 기존 패널 크기 유지. `Border_FacilityGuide`의 기본색·여백, `SizeBox_FacilityInstances`의 목록 최대 높이(기본 240), Class Defaults의 `FacilityGuide` 문구·미설치 강조색 |
| `/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCatalogChoice` | `Button_Root` 크기·테두리, `Text_Label` 글꼴, `Border_InstallationStatus` 상태 칩. Class Defaults의 설치 표시 문구·색 |
| `/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramFacilityRow` | `SizeBox_FacilityRow` 최소 높이(66), 이름·위치 글꼴, `SizeBox_Locate` 버튼 크기(최소 76×32), `Border_Selection` 기본 테두리, Class Defaults의 선택색 |

`위치 보기`는 편성 대상을 바꾸지 않고 시설로 카메라를 이동한다. 월드가 보이도록 프로그램 창을 접으며, 프로그램 탭을 다시 열면 직전 선택을 복원한다. 이미 철거된 시설은 복원하지 않는다.

시설 번호는 실제 GUID별·시설 종류별로 현재 월드 세션 동안 유지한다. 이동·철거 때문에 남은 시설 번호를 다시 매기지 않는다. 번호는 저장 데이터가 아니므로 새 게임·세이브 로드 후에는 재생성된다. 위치는 실제 구역명만 표시하며, X/Y 좌표는 표시하지 않는다. `위치 보기`는 게임 화면으로 입력 포커스를 돌린 뒤 카메라 이동을 요청한다.

## 적용

에디터를 닫고 에디터용 빌드 후 `run.ps1`을 실행한다. 기존 WBP를 백업하고 `FF.ApplyPrepProgramFacilities`로 필요한 위젯만 추가한다. 다시 실행하면 버전 메타데이터를 확인해 사용자 편집을 보존한다.

음악/SNS 도크 순서는 프로젝트 설정의 Audio Settings `Playlist Player Z Order`(98)로 지정한다. 시설 라벨과 일반 패널 위에 보이고, ESC·확인창이 열려 있으면 도크만 잠시 숨겼다가 같은 상태로 복원한다.
