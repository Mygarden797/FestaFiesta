# 시설 정보와 조작 화면

시설 선택, 상세 정보, 조명·대기열·유료 프로그램 조작을 `Building/FacilityInfo`에서 관리한다. 게임 런타임 헤더와 구현은 같은 기능 경로를 사용하고 자동화 검증은 구현 옆의 `Tests/`에 둔다. 위젯 제작·디자인 명령은 `FestaFiestaEditor/Private/Building/FacilityInfo`에만 둔다.

`FF_FacilityInfoSubsystem`은 시설·경제·프로그램 시스템의 읽기 모델을 모으고 사용자 요청을 해당 소유자에게 전달한다. 화면에서 시설 상태나 수입 장부를 직접 변경하지 않는다. `FF_FacilityInfoSelection`, `FF_FacilityInfoUpgradeLayout`, `FF_FacilityPaidProgramLayout`은 런타임과 에디터 도구가 공유하는 선택·배치 규칙이다.

`FF_FacilityInfoPanel` 및 보조 위젯의 공개 이름과 기존 에셋 경로는 Blueprint 호환성을 위해 유지한다. 조명 초안은 적용 전까지 실제 시설 설정과 구분하고, 화면·서브시스템을 해제할 때 이벤트 구독과 선택 표시도 함께 정리한다.
