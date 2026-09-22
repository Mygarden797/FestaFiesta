# 시설 조합 도감 검증 기록

2026-09-16 · Unreal Engine 5.7.4 · Windows Development Editor

## 후속 수정: 검색 옆 도감 버튼과 설치 완료 기준

- 시설 배치 WBP의 기존 검색창 바로 뒤에 `WBP_FF_FacilityCodexCatalogLauncher`를 추가했습니다. 기존 위젯·슬롯·연결을 보존하며 화면 전체를 재생성하지 않습니다.
- 조합의 두 시설 설치 완료로 발견됩니다. 거리가 멀거나 재배치가 필요해도 설치 기록에 포함되며, 현재 효과는 기존 거리·배치·중복 그룹·상한 조건을 사용합니다. 같은 종류끼리는 서로 다른 설치 2개가 필요합니다.
- 모델 자동 검사 5건, 실제 저장 WBP UI 검사 2건 성공. `Saved/FacilityCodexTask/ReportCatalogInstall/index.json`.
- 최종 Editor 재빌드 성공: `Saved/FacilityCodexTask/EditorBuildCatalogFinal.log`. `PresenterLifecycle` 재검사도 성공하여 이번 변경 관련 8건이 모두 통과했습니다. `Saved/FacilityCodexTask/ReportCatalogFinal/index.json`.
- 실제 검색 옆 버튼으로 도감을 열고 닫아 전체 조합 표시, 동일 카탈로그 객체·검색어·2페이지·타일 순서 유지, 입력 복원을 확인했습니다. 첫 검사에서 테스트의 비동기 완료 콜백 대기가 누락되어 실패했으며, fixture만 보완해 다시 통과했습니다. 기존 EnhancedInput 경고 2회는 아래와 같습니다.
- 720p·1080p에서 검색·도감·금화 영역 겹침, 버튼 글자 잘림, 큰 금액 표시를 검사하고 아래 화면을 확인했습니다.
- `Saved/Automation/FacilityCodex/FacilityCodexCatalog720.png`
- `Saved/Automation/FacilityCodex/FacilityCodexCatalog1080.png`
- 생성·연결 명령 반복 실행 시 관련 WBP 6개가 바이트 단위로 유지됩니다. 기존 도감 WBP 4개는 이번 연결에서도 수정되지 않았습니다.

## 후속 수정: 전체 조합과 미발견 회색 표시

- 시설창에서 열 때 선택 시설을 자동 필터로 사용하지 않도록 변경했습니다. 실제 시설을 설치·선택하고 시설창의 도감 버튼을 눌러, 선택 시설과 무관한 조합까지 표시되는 것을 검사했습니다.
- 미발견 카드는 회색 배경·글자와 흐린 썸네일로 표시됩니다. 클릭해서 상세를 볼 수 있고, 발견 시 기존 WBP 색상이 복원됩니다. 데이터 갱신 후 재사용되는 카드의 복원도 검사했습니다.
- 후속 Editor 빌드 성공: `Saved/FacilityCodexTask/EditorBuildAllGray.log`.
- 변경 관련 자동 검사 2/2 성공: `AuthoredWidgets`, `PresenterLifecycle`. 보고서: `Saved/FacilityCodexTask/ReportAllGray/index.json`.
- 아래 화면 PNG를 새 동작으로 다시 렌더링해 회색 표시를 확인했습니다. 기존 WBP 4개는 재생성하거나 저장하지 않았습니다.

## 결과

- Editor 빌드 성공. `Saved/FacilityCodexTask/EditorBuild.log`
- 자동 검사 **12/12 성공**, 실패 0건. `Saved/FacilityCodexTask/ReportFinal/index.json`
- 모델 검사 4건: 발견 유지, 유효하지 않은 행·미설치 시설 제외, 양쪽 시설·이름 필터, 기존 수익의 방향·그룹·상한 일치.
- 도감 UI 검사: 실제 WBP 바인딩, 검색 이벤트·공개 검색 API, 필터·선택·닫기, 디자이너 글꼴 유지, 여러 열 배치와 카드 상태 표시 영역, 120개 항목의 가상화.
- 도감 열기/닫기 검사: 입력 소유권, 건축 입력 복원, 중복 열기·닫기, 단계 전환·스냅샷·위젯 제거·컨트롤러 종료 시 정리, 시간 정지 없음.
- 기존 기능 회귀 검사 6건: 시설 선택 입력, 카메라 입력 정책 2건, 시설창 일시정지 표현, 조합 탭, 일시정지 WBP 계약.

Presenter fixture를 만드는 동안 LocalPlayer에 PlayerInput이 아직 없다는 EnhancedInput 경고가 2회 발생합니다. 해당 검사도 성공했으며, 보고서는 성공 11건과 경고 포함 성공 1건입니다. 전체 게임 플레이나 배포 패키징을 실행한 결과는 아닙니다.

## 화면 확인

실제 저장된 WBP를 RHI로 렌더링한 다음 아래 PNG를 확인했습니다. 120개 목록은 테스트용이며 게임 조합표나 발견 기록에 저장하지 않습니다.

- `Saved/Automation/FacilityCodex/FacilityCodex720.png`
- `Saved/Automation/FacilityCodex/FacilityCodex1080.png`
- `Saved/Automation/FacilityCodex/FacilityCodexGrid720.png`
- `Saved/Automation/FacilityCodex/FacilityCodexEmpty720.png`
- `Saved/Automation/FacilitySynergy/FacilitySynergyTab720.png`

720p·1080p에서 카드·선택 테두리·닫기 버튼과 빈 결과 표시를 확인했습니다. 긴 상세 설명은 패널 내부에서 스크롤됩니다. 카드는 3열로 배치되고 화면 밖 항목은 가상화됩니다.

## 파일 보존

- 기존 수정 중 파일 11개의 SHA-256이 작업 시작 시점과 모두 일치합니다. `Saved/FacilityCodexTask/ExistingDirtyVerification.json`
- 기존 파일의 의도적인 변경은 시설 담당자의 `Content/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel.uasset`과 `Content/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild.uasset`입니다. 각각 기존 조합 섹션과 검색 옆에 도감 버튼 WBP만 추가했습니다.
- 나머지 구현·UMG 에셋·문서·도구는 새 경로에 추가했습니다. 기존 C++·조합표·시설 정의를 수정하지 않았습니다.
- 생성 및 연결 명령을 반복해도 WBP 6개가 바이트 단위로 그대로임을 확인했습니다. `Saved/FacilityCodex/asset_verification.json`

## 문서와 에디터

- 요청한 [노션 페이지](https://app.notion.com/p/3dd96c43a7f78089a3b7c6252f0c3f1e)에 사용법과 조합표·UMG 편집 안내를 작성하고 저장 결과를 확인했습니다.
- 새 빌드로 에디터를 다시 열고 해당 프로젝트와 도감 WBP 로드를 확인했습니다. 플레이는 시작하지 않았습니다.

## 재실행

`UnrealEditor-Cmd`에 `-RenderOffscreen -unattended -nop4 -nosound`와 다음 옵션을 전달합니다.

```text
-ExecCmds="Automation RunTests FestaFiesta.Building.Codex+FestaFiesta.FacilityCodex+FestaFiesta.UI.FacilityCodex+FestaFiesta.UI.FacilityInfo.SynergyTab+FestaFiesta.UI.FacilityInfo.PausePresentationLifecycle+FestaFiesta.Building.BuildMode.FacilityInspectionInput+FestaFiesta.Camera.Input+FestaFiesta.UI.Pause.WidgetContract"
-TestExit="Automation Test Queue Empty"
-ReportExportPath="<project>/Saved/FacilityCodexTask/ReportFinal"
```
