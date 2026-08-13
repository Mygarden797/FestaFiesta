# 공식 디지털 트윈 UI/UX 리서치

조사 기준일: 2026-08-13. 공식 제품/개발 문서만 사용했습니다.

## 적용한 패턴

### Bentley iTwin

- 중앙 3D 뷰포트에서 객체를 선택, 숨김, 강조하고 우측 속성으로 연결
- 센서, 알림, 주석을 장면 속 대상과 직접 연결
- 현재 상태와 변경 결과를 시간/색상 문맥으로 비교

공식 자료:

- https://developer.bentley.com/apis/visualization/overview/
- https://developer.bentley.com/use-cases/visualizing-an-itwin/
- https://developer.bentley.com/tutorials/changed-elements-api-v3/

### Autodesk Tandem

- 기능 레일에서 필터, 자산, 문서, 시스템, 스트림, 이력을 전환
- 선택 시설의 세부 정보, 검색, 필터, 상태 색상화와 시간 탐색
- 공간, 자산, 운영 이력을 하나의 정보 창에서 연결

공식 자료:

- https://help.autodesk.com/cloudhelp/ENU/Tandem-Getting-Started/files/tandem-navigate-ui.html
- https://help.autodesk.com/cloudhelp/ENU/Tandem-Facilities/files/tandem-filters.html
- https://www.autodesk.com/products/Tandem/overview

### NVIDIA Omniverse Kit

- Viewport, Stage/Layers, Content Browser, Property Inspector, Toolbar의 모듈식 작업공간
- 업무에 맞춘 패널 조합과 재사용 가능한 확장 구조

공식 자료:

- https://docs.omniverse.nvidia.com/kit/docs/kit-app-template/105.1/app_from_scratch.html
- https://docs.omniverse.nvidia.com/kit/docs/kit-app-template/105.1/kit_sdk_overview.html
- https://docs.omniverse.nvidia.com/extensions/latest/ext_digital-twins.html

### Cesium for Unreal

- 객체 선택 지점에 메타데이터 팝오버 표시
- 카메라 거리와 무관하게 정보의 지각 크기와 가독성을 유지

공식 자료:

- https://cesium.com/learn/unreal/metadata-dynamic-ui/
- https://cesium.com/learn/unreal/unreal-datasets/

### ArcGIS Maps SDK for Unreal

- 레이어의 가시성, 순서, 투명도, 필터를 독립 제어
- 속성 및 공간 필터를 통해 중요한 정보만 장면에 강조

공식 자료:

- https://developers.arcgis.com/unreal-engine/layers/
- https://developers.arcgis.com/unreal-engine/filtering-and-modification/

### Unreal CommonUI

- 스타일 데이터 에셋으로 상태별 외형을 중앙 관리
- Activatable Widget/Stack과 입력 라우팅으로 메뉴, 모달, 팝업의 계층과 포커스를 분리
- 키보드/게임패드 포커스와 문맥형 입력 힌트 지원

공식 자료:

- https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-advanced-multiplatform-user-interfaces-with-common-ui-for-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-quickstart-guide-for-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/design-guidelines-for-using-commonui-in-unreal-engine

## 프로젝트화 원칙

- 외부 제품의 로고, 고유 아이콘, 패널 형태, 정확한 배치를 복제하지 않습니다.
- FESTA FIESTA의 딥 틸, 민트, 아이보리, 랜턴 앰버와 다대포 시설 아트를 유지합니다.
- 전 지구 GIS, 위성 스트리밍, 멀티플레이, 엔지니어링 CAD 기능을 추가하지 않습니다.
- 정보 공개 순서는 맵 상태 → 호버 요약 → 선택 인스펙터 → 실행 모달입니다.
- 실시간 HUD는 가볍게 유지하고 CommonUI는 메뉴, 설정, 모달, 다층 흐름에 집중합니다.

