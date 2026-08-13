# FESTA FIESTA — Digital Twin UI/UX Design System v2

Unreal Engine 5 기반 다대포 축제 운영 게임을 위한 협업용 고해상도 콘셉트와 구현용 UI 이미지 키트입니다.

## 포함된 화면

| 파일 | 워크스페이스 | 핵심 UX |
| --- | --- | --- |
| `01-spatial-operations.png` | 공간 관제 | 레이어, 월드 핀, 혼잡/대기 경고, 실시간 타임라인 |
| `02-facility-asset-inspector.png` | 시설 자산 | 자산 트리, 선택 시설 3D 뷰, 속성, 운영 상태와 이력 |
| `03-scenario-layer-analysis.png` | 분석 | 동일 시점 A/B 비교, 레이어/필터, 결과 델타, 시간 스크러빙 |
| `04-build-workspace.png` | 건축 | 시설 카탈로그, 배치 프리뷰, 공간 예약, 속성/유효성 검사 |
| `05-settings-commonui.png` | 설정 | 메뉴 계층, 선택/포커스 분리, 적용/취소/복원 흐름 |

모든 화면은 1672×941 PNG(16:9)이며, 외부 제품의 화면을 복제하지 않고 검증된 상호작용 원칙만 FESTA FIESTA의 시각 언어로 재구성했습니다.

## ComponentKit

`ComponentKit`은 1920×1080 기준 UMG/CommonUI 사용을 위한 텍스트 없는 투명 PNG와 동일한 SVG 원본을 포함합니다. 현지화 문자열과 프로젝트 아이콘은 Unreal에서 오버레이합니다.

- 버튼: Primary, Secondary, Ghost, Danger, Tab, IconCircle
- 버튼 상태: Default, Hover, Pressed, Selected, Focus, Disabled
- 기본 컨트롤: Toggle, Checkbox, Radio, Chip, Search/Input, Slider, Segmented Control
- 위젯: Dark/Light Panel, KPI Card, Asset Status Card, Layer Row, Timeline, Density Legend, Scenario Compare, Warning/Danger Toast, Tooltip, Inspector, Modal
- 아이콘: Layers, Pin, Clock, Asset, Filter, Compare, Warning, Search, Eye, Timeline, Route, Sensor
- `manifest.json`: 크기, 상태, Draw As, 9-slice margin, 1080p 권장 크기, 용도 메타데이터
- 연락판: `buttons-contact-sheet.png`, `controls-contact-sheet.png`, `widgets-contact-sheet.png`

## Unreal 적용 규칙

1. PNG를 `Content/FestaFiesta/UI/DigitalTwinV2`에 Import합니다.
2. 버튼/패널 중 manifest에서 `draw_as: Box`인 항목은 `margin_px`를 기준으로 9-slice를 설정합니다.
3. 버튼 이미지는 `CommonButtonStyle`의 Normal, Hovered, Pressed, Selected, Disabled에 연결합니다.
4. Focus 이미지는 선택 상태와 별개로 `CommonActivatableWidget`/포커스 브러시에 사용합니다.
5. 모든 텍스트와 실제 기능 아이콘은 UMG 레이어로 올립니다. 이미지 안에 문자열을 굽지 않습니다.
6. 1080p에서 상호작용 높이는 44 px 이상, 화면 안전 여백은 32 px 이상을 권장합니다.
7. 위험 상태는 색상만 쓰지 않고 아이콘, 테두리, 패턴, 문구를 함께 사용합니다.

## 시각 토큰

| 역할 | 값 |
| --- | --- |
| Abyss | `#071314` |
| Navy | `#0B1D1E` |
| Panel | `#102627` |
| Panel Raised | `#173031` |
| Mint | `#57D4B8` |
| Mint Bright | `#8BE9D4` |
| Mint Deep | `#238C79` |
| Ivory | `#F6F5EF` |
| Amber | `#F0B44D` |
| Coral | `#F16D5C` |
| Info | `#5BA6D9` |

## 참고한 공식 UX 원칙

- Bentley iTwin: 3D 뷰포트 중심 선택/강조, 속성 패널, 센서와 변화의 공간 문맥화
- Autodesk Tandem: 자산/필터/스트림/이력과 시간 탐색, 선택 시설의 단일 정보 창
- NVIDIA Omniverse Kit: 워크플로별 도킹 작업공간과 속성 검사기
- Cesium for Unreal: 월드 공간 메타데이터 팝오버와 거리 독립 가독성
- ArcGIS Maps SDK for Unreal: 레이어 가시성, 순서, 투명도, 속성/공간 필터
- Unreal CommonUI: 스타일 데이터, Activatable Stack, 입력 라우팅, 명시적 포커스

Cesium과 ArcGIS는 프로젝트에 비활성/미사용인 실제 GIS 데이터를 추가하는 근거가 아닙니다. 본 디자인에서는 authored Dadaepo 축제장 내부의 레이어와 분석 모델로만 해석했습니다.

## 재생성

```powershell
$env:NODE_PATH='C:\Users\kpo91\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\node_modules'
& 'C:\Users\kpo91\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' 'Tools\ui\generate_festafiesta_dt_ui_v2_kit.js'
```

생성 스크립트: `Tools/ui/generate_festafiesta_dt_ui_v2_kit.js`

