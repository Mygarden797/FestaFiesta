# 시설 정보 가독성 적용 도구

승인된 `readability-v10` 시안을 기존 시설 정보 위젯에 적용한다. 에디터와 PIE를 종료한 상태에서 `FestaFiestaEditor`를 빌드한 뒤 프로젝트 루트 PowerShell에서 실행한다.

```powershell
& .\Tools\BuildCatalogV10\run.ps1 -Phase Inspect
& .\Tools\BuildCatalogV10\run.ps1 -Phase Apply
& .\Tools\BuildCatalogV10\run.ps1 -Phase Verify
& .\Tools\BuildCatalogV10\run.ps1 -Phase Test
& .\Tools\BuildCatalogV10\run.ps1 -Phase Capture
```

기존 BuildTools·FacilityInfo·LightingControls·SynergyEntry 위젯을 백업하고 이름과 기능 바인딩을 보존한다. 카탈로그의 12열·96×80 시설 이미지·170 검색창과 기존 하단 위젯은 유지한다. 시설 비용은 하단 HUD와 같은 골드 이미지를 사용한다.

시설 정보는 400×300 패널 안에서 탭별로 읽도록 구성한다. 지원하지 않는 스태프·프로그램·매출·강화·조명·대기열·조합 탭과 빈 정보 행을 숨긴다. 상태는 실제 읽기 모델에서 전달하며, 정상적인 0원·0건 값은 숨기지 않는다.

조명의 원형 드래그 색상 선택기는 사용자의 추가 지시에 따라 96에서 32로 줄인다. 밝기·색상 프리셋·HSV/RGB·HEX와 적용/취소 기능은 유지한다. 카탈로그 카드에는 필수·조명 지원 아이콘을 좌측 상단에 표시한다.

로그·백업·독립 저장 검증·실제 Slate 입력 검사 보고서는 `Saved/BuildCatalogV10`에 저장된다. `Capture`는 격리된 사용자 경로의 실제 축제 맵에서 목록과 설치 미리보기만 촬영하고 시설을 설치하거나 에셋을 저장하지 않는다.
