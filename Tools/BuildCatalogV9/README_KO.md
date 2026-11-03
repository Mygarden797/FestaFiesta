# 건축 카탈로그 시안 적용 도구

`search-v9` 시안을 기존 위젯 4개에 적용한다. 왼쪽 아이콘 분류, 12열·2행 썸네일, 170 논리 단위 검색창, 통합 우측 정보와 기존 하단 HUD를 사용한다. 시설 이미지는 투명 여백을 포함한 원본 텍스처를 96×80 프레임에 비율 유지로 표시하고 세로 중앙에 정렬한다.

에디터와 PIE를 종료하고 `FestaFiestaEditor`를 빌드한 후 프로젝트 루트의 PowerShell에서 실행한다.

```powershell
& .\Tools\BuildCatalogV9\run.ps1 -Phase Inspect
& .\Tools\BuildCatalogV9\run.ps1 -Phase Apply
& .\Tools\BuildCatalogV9\run.ps1 -Phase Verify
& .\Tools\BuildCatalogV9\run.ps1 -Phase Test -TestFilter 'FestaFiesta.UI.PrepBuild+FestaFiesta.Building.BuildTools.CatalogHelpLifecycle'
& .\Tools\BuildCatalogV9\run.ps1 -Phase Capture
```

`Apply`는 변경 전 에셋을 `Saved/BuildCatalogV9/apply-*/Backup`에 복사하고 기존 위젯 바인딩 이름을 보존한다. 대상 외 UI·아이콘·설정 파일의 해시도 검사한다. 추가 위젯의 GUID 등록에는 `FestaFiestaEditor`의 `FF_CatalogEditorLibrary`를 사용한다.

각 실행의 로그·검사 보고서는 `Saved/BuildCatalogV9`에 남는다. `Capture`는 별도 사용자 저장 경로에서 실제 `Test_Main` 축제를 시작해 목록과 시설 선택 화면을 촬영한다. 시설을 설치하거나 에셋을 저장하지 않으며, 촬영을 위해 현재 대사만 종료한다. 프로젝트의 기존 플레이어 저장 파일이 바뀌지 않았는지도 검사한다. 자동 입력 검사는 `Test`가 담당한다.

`Verify`는 저장된 위젯을 새 프로세스에서 다시 읽는 검사이며, `Capture`의 스크린샷은 실제 렌더 결과다. 비표시 창의 Python geometry가 제공되지 않으면 해당 좌표는 `null`로 기록하고, 화면 배치 검사는 네이티브 Slate 테스트에서 수행한다.
