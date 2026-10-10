# 중앙형 매출 순위

에디터 빌드 후, 에디터를 닫고 `Tools/ui/revenue_sky/run.ps1`을 실행한다.
열린 에디터에서는 저장을 마친 뒤 콘솔 `FF.ApplyFacilityRevenueSky`로 적용할 수 있다.

- 대상은 `WBP_FF_FacilityRevenueRanking`, `WBP_FF_FacilityRevenueRankRow` 두 개뿐이다.
- 원본 파일은 `Saved/UIBackups/RevenueSky/<실행별 GUID>/`에 먼저 복사한다.
- 패널/행의 기존 위젯과 이름을 유지하며 재배치·색·폰트·단위 표시를 변경한다. 이벤트 그래프는 변경하지 않는다.
- 처음 사용 안내·시설 선택·정보 보기·ESC·바깥 클릭 닫기는 기존 경로를 사용한다.
- 금액에는 숫자만 표시하며 상단에 금화 단위를 한 번 표시한다. 위치 좌표는 행 툴팁에서 확인할 수 있다.
- 메타데이터 `FF.FacilityRevenue.Sky20261010=1`인 에셋은 재실행해도 디자이너 편집을 덮어쓰지 않는다.
- 중앙 위치는 `Size_ClarityFrame`의 Canvas 슬롯, 크기는 같은 위젯의 Width/Height Override(기본 1060×690)에서 조정한다.
- `List_Ranking`은 가상 목록을 유지한다. 스크롤바 스타일은 ListView의 `Scroll Bar Style`, 행 크기는 `Size_Row`에서 조정한다.
- 적용 결과는 `Saved/RevenueSky/apply-report.json`. 실행기는 개인 에디터 설정·세이브를 백업/복원한다. 실제 화면은 사용자 확인 대상이다.
