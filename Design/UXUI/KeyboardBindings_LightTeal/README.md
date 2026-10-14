<!-- 파일: README.md / 역할: 키 설정 화면의 조작법과 입력 정의·저장 호환성·검증 방법을 설명한다. -->
# Keyboard Bindings · Light Teal

`WBP_FF_PauseOptions`의 **키보드 키 배치** 탭을 실제 리바인딩 화면으로 확장한 작업이다（10/14 기준: `WBP_FF_PauseOptions` 애셋은 저장소에 없다. 키 배치 패널은 축제 중 설정 화면 `WBP_FF_FullscreenOptions`(`UFF_FestivalSettingsScreen`, 일시정지 허브에서 진입)와 프론트엔드 옵션 `WBP_FF_Options`(`UFF_OptionsScreen`)가 얹는다）.
첨부 레퍼런스는 키보드 배치와 정보 구조만 참고하고, 화면 전체의 색·폰트·간격·상태 표현은
종태님이 만든 기존 `UFF_UIPalette::InGame` Light Teal 설정 UI를 그대로 따른다.

## 런타임 자산

- 화면: `/Game/FestaFiesta/UI/Settings/WBP_FF_PauseOptions`（10/14 기준 없음 → `/Game/FestaFiesta/UI/Settings/WBP_FF_FullscreenOptions`, `/Game/FestaFiesta/UI/Frontend/WBP_FF_Options`）
- 키보드 패널: `/Game/FestaFiesta/UI/Settings/WBP_FF_KeyBindingPanel`
- 생성 텍스처: `/Game/FestaFiesta/UI/Settings/Textures/T_UI_Settings_Keyboard_ImageGen_v2`
- 원본 PNG: `Design/SourceAssets/UI/Settings/T_UI_Settings_Keyboard_ImageGen_v2.png`

생성 이미지는 장식용 하부 레이어다. 키 이름, 선택 상태, 현재 바인딩, 포커스와 클릭 영역은
모두 UMG가 그린다. 따라서 사용자가 키를 바꾸면 다시 이미지를 만들지 않아도 즉시 표시가 갱신된다.

## 조작

1. 실제 키를 누르거나 화면의 키캡을 클릭하면 그 키에 현재 연결된 모든 동작을 확인한다.
2. 키를 바꾸려면 아래 액션 목록에서 변경할 항목을 선택한다.
3. 실제 키보드의 새 키를 누르거나 화면의 가상 키캡을 클릭한다.
4. 변경은 즉시 조작에 반영된다. 설정 화면의 `저장`으로 확정하며, 변경을 버리면 화면을 열기 전 키로 돌아간다.
5. `기본값 복원`은 이 화면이 관리하는 시스템·카메라·캐릭터 키를 코드에 정의된 기본 키로 되돌린다. 저장 전에 취소할 수 있다.

옵션 열기/닫기의 기본 키는 `ESC`와 `F5`이며 시스템 항목에서 변경할 수 있다. 시스템 키는 모든
그룹과 충돌을 검사한다. 같은 조작 그룹(카메라 또는 캐릭터)에서 이미 사용 중인 키를 고르면
충돌 안내를 표시한다. 서로 다른 카메라·캐릭터 그룹은 기존 기본값처럼 `WASD`를 공유할 수 있다.

캐릭터의 `LeftShift`는 **시점 고정 켜기 / 끄기 (Shift Lock)**다. 한 번 누르면 캐릭터가
카메라 방향을 따르고 다시 누르면 해제된다. 달리기 키가 아니다. 키를 바꾸려면 해당 항목을
선택한 뒤 원하는 키를 누른다.

`B`는 실제 런타임에서 건축 모드와 시설 목록 전환을 직접 처리하는 고정 키다. 키보드와
`고정 단축키` 목록 양쪽에 표시되며 클릭하거나 누르면 현재 설정을 확인할 수 있다. 아직
Enhanced Input으로 이전되지 않았으므로 이 화면에서는 변경할 수 없고, 다른 동작에 B를
지정하려는 시도도 이중 실행을 막기 위해 거부한다.

## 실제 리바인딩 범위

현재 Enhanced Input을 사용하는 관찰 카메라와 캐릭터 입력만 변경 가능 항목으로 노출한다.
건축 B는 실제 동작을 확인할 수 있도록 고정·읽기 전용으로 따로 표시한다. Atlas의 일부
키도 아직 `EKeys` 직접 비교로 구현되어 있으므로 변경 가능 항목으로 거짓 노출하지 않는다. 해당
소비자가 InputAction 또는 공용 키 바인딩 계층으로 이전되면 같은 패널에 매핑 이름만 추가해
확장할 수 있다.

## 생성 및 검증

- `FF_KeyBindingDefinitions.cpp`의 `GetAll()`이 동작 ID·화면 이름·기본 키·실제 입력 Action을
  함께 정의한다. 화면, 런타임, 작성기는 이 목록을 공유하므로 새 동작을 추가할 때 별도 표를 만들지 않는다.
- ShiftLock의 정의는 `FF.Character.ShiftLock` → `IA_ShiftLock` + `LeftShift`다.
  실제 입력은 `/Game/FestaFiesta/Input/Character/IMC_Character`를 사용한다.
- `FF.KeyBindingUMG`: 텍스처 import, IMC Player Mappable 메타데이터 설정, 전용 WBP 생성.
  실제 Action과 기본 키 조합을 먼저 검사한다. 런타임은 임시 프로필을 만들기 때문에 이번 이름 수정만으로
  기존 IMC나 WBP를 다시 저장할 필요는 없다. 이 작성기 명령은 IMC도 저장하므로 입력 에셋 담당자가 실행한다.
- `FestaFiesta.UI.Settings.KeyBinding`: config·IMC·WBP·텍스처 계약 자동화 테스트
- `FestaFiesta.UI.Settings.KeyBinding.DefaultKeyFallbackContract`: 화면의 기본 키와 실제 입력 Action,
  LeftShift의 ShiftLock 안내가 일치하는지 검사한다.
- `FestaFiesta.UI.Settings.KeyBinding.RetiredMappingProfileCompatibility`: 이전 저장 행이 남은 프로필,
  새 ShiftLock 키 변경·기본값 복원, 다른 동작의 사용자 키 보존을 검사한다. 테스트는 저장 파일을 쓰지 않는다.

## 이전 Sprint 저장값 처리

이전 버전은 실제 IMC에 없는 `IA_Sprint + LeftShift`를 설정 화면에 잘못 표시했다. 저장된
`FF.Character.Sprint` 키는 실제 게임 매핑에 적용되지 않았다. 이를 ShiftLock 키로 자동 변환하면
다른 기능을 새 키로 켜게 되므로 변환하지 않는다. ShiftLock은 처음에 LeftShift를 사용하며,
새 항목에서 직접 바꿔 저장한 키가 있으면 그 키를 사용한다.

이전 Sprint 행은 저장 데이터에서 삭제하지 않는다. `ResolveCurrentKey()`가 현재 동작 목록에 있는
행만 읽으므로 화면이나 실제 입력에 다시 나타나지 않는다. 점프·이동 등 다른 사용자 설정도 유지된다.
