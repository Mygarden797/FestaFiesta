# 시설 수익 순위

`FF_FacilityRevenueRanking`은 경제 장부의 복사본을 표시용 순위·비율로 변환하고 선택한 시설의 정보 화면으로 연결한다. 수입의 원본과 정산 규칙은 경제 시스템이 소유한다.

동점은 같은 순위를 사용하고 시설 이름·고유 ID로 표시 순서를 고정한다. 영업 상태는 시설 정보 창과 같은 대기열 시스템에서 읽는다. 목록은 가상화된 행만 갱신하며 영업 상태 확인을 위해 장부 전체를 다시 정렬하지 않는다.

구현과 기존 회귀 검증은 `Private/Building/FacilityRevenue`와 하위 `Tests`에, 위젯 제작 도구는 `FestaFiestaEditor/Private/Building/FacilityRevenue`에 둔다. Blueprint 타입명과 기존 에셋 패키지 경로는 폴더 이동 후에도 유지한다.
