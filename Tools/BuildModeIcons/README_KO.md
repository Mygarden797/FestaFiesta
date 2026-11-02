# 건축모드 HKH 아이콘

2026-11-02 · 상단 기능 22개 + 시설 분류 11개를 개별 PNG와 Unreal Texture2D로 연결했다.

- 원본과 에셋: `Content/Asset/Icon_HKH/T_HKH_*.png`, 같은 이름의 `.uasset`.
- 원본: 네이티브 이미지 생성 도구로 개별 제작한 1254×1254 RGBA. 투명 배경, 흰색 그림, 넉넉한 여백. 프롬프트·생성 경로·SHA256은 이 폴더의 `*-generation.json`에 기록했다.
- 텍스처: UI 그룹, Editor Icon 압축, sRGB, 밉맵 없음, Clamp, 스트리밍 없음, 최대 256px. 원본 파일은 수정하지 않았다.
- 밝은 Pearl 툴바에서는 흰 그림에 진한 청록색을 입힌다. 분류 탭은 기존 비선택·선택 색상과 배경 형태를 함께 사용한다.
- 분류 아이콘은 24px 정사각형으로 배치한다. 기존 32px 버튼의 세로 여백 안에서 그림 비율을 보존한다.

## 디자이너에서 수정

| 대상 | Unreal 에셋 / 설정 |
| --- | --- |
| 22개 상단 기능 | `Controls/FeatureGuide/WBP_FF_BuildFeatureGuideToolbar`의 `Guide_*` → IconBrush |
| 공통 버튼 스타일 | `Controls/FeatureGuide/WBP_FF_BuildFeatureGuideButton`의 `Button_Guide`, `Image_GuideIcon` |
| 11개 분류 | `Catalog/WBP_FF_PrepPanelBuild`의 `Choice_CategoryAll`, `Choice_Category0`~`9` → AuthoredIcon |
| 반응 모션 | 각 `Guide_*` → Feature Guide / Motion |

위 경로는 `/Game/FestaFiesta/Building/FF_WBP_Building/` 아래에 있다.

상단 기능은 개별 텍스처를 사용하므로 `Use Icon Atlas`를 끈다. 분류 탭도 `IconAtlas`를 비우고 `Preserve Icon Colours`를 꺼야 흰 원본에 선택 상태 색상이 적용된다. `FeatureTopicId`와 분류 ID는 기능 연결이므로 유지한다.

마우스 오버와 키보드 포커스는 그림을 7.5% 확대하고 1.5 Slate 단위 올린다. 누르면 94%로 눌리고, 해제하면 약 120ms에 걸쳐 부드럽게 돌아온다. 버튼의 실제 배치와 클릭 영역은 움직이지 않는다. `Enable Icon Motion`으로 끄거나 강도를 조절할 수 있다. 기존 0.25초 건축 진입 페이드와 시설 카드의 확대 모션도 유지한다.

## 적용과 검증

`apply_icons.py`는 원본 33개와 실제 위젯 이름·기능 ID를 먼저 검사한다. 새 텍스처만 가져오고, 변경하는 WBP 3개를 `Saved/BuildModeIcons/backups/`에 백업한다. 구형 UI를 재생성하지 않는다.

`run_apply.py`는 별도 Entry 에디터에서 적용 후 종료한다. 실행 중인 다른 에디터가 같은 에셋을 잡고 있으면 저장이 실패할 수 있으므로, 저장을 마친 뒤 에디터를 닫고 실행한다.

`verify_icons.py`는 새 에디터에서 디스크에 저장된 텍스처 설정과 33개 참조를 읽는다. `run_verify_and_tests.py`는 이 검사 후 기존 `FestaFiesta.Building.FeatureGuide`, `FestaFiesta.UI.PrepBuild` 자동화 검사를 실행한다. 실제 화면 검사는 `-RenderOffscreen`으로 실행한다.

적용 기록: `Saved/BuildModeIcons/apply-result.json`. 독립 저장 검증: `disk-verification.json`. 최종 자동화 결과: `ReportVerified/index.json`. 사용자 저장 파일은 별도의 테스트 UserDir로 격리한다. `create_preview.ps1`은 최종 Unreal 렌더 캡처의 실제 UI 영역을 모아 `BuildUI-preview.png`로 보여 준다.

최종 검증은 Development Editor 빌드 성공, 독립 프로세스의 텍스처 33개·참조 33개 확인, 기존 UI 자동화 검사 31개 통과·실패 0개다. 30개는 경고 없이 통과했고, 툴바 팝업 검사 1개는 격리된 테스트 플레이어의 PlayerInput 경고 등을 포함해 통과했다. 실제 RHI에서 720p·1080p 툴바와 3개 해상도의 시설 목록 배치·입력·포커스를 검사했다. 검사용 크기 비교는 최종 24px 정사각형을 확인한다.
