# Keyboard Bindings · Light Teal

`WBP_FF_PauseOptions`의 **키보드 키 배치** 탭을 실제 리바인딩 화면으로 확장한 작업이다.
첨부 레퍼런스는 키보드 배치와 정보 구조만 참고하고, 화면 전체의 색·폰트·간격·상태 표현은
종태님이 만든 기존 `UFF_UIPalette::InGame` Light Teal 설정 UI를 그대로 따른다.

## 런타임 자산

- 화면: `/Game/FestaFiesta/UI/Settings/WBP_FF_PauseOptions`
- 키보드 패널: `/Game/FestaFiesta/UI/Settings/WBP_FF_KeyBindingPanel`
- 생성 텍스처: `/Game/FestaFiesta/UI/Settings/Textures/T_UI_Settings_Keyboard_ImageGen_v2`
- 원본 PNG: `SourceAssets/UI/Settings/T_UI_Settings_Keyboard_ImageGen_v2.png`

생성 이미지는 장식용 하부 레이어다. 키 이름, 선택 상태, 현재 바인딩, 포커스와 클릭 영역은
모두 UMG가 그린다. 따라서 사용자가 키를 바꾸면 다시 이미지를 만들지 않아도 즉시 표시가 갱신된다.

## 조작

1. 실제 키를 누르거나 화면의 키캡을 클릭하면 그 키에 현재 연결된 모든 동작을 확인한다.
2. 키를 바꾸려면 아래 액션 목록에서 변경할 항목을 선택한다.
3. 실제 키보드의 새 키를 누르거나 화면의 가상 키캡을 클릭한다.
4. 변경은 Enhanced Input 사용자 설정에 즉시 적용되고 로컬 SaveGame에 저장된다.
5. `기본값 복원`은 이 화면이 소유하는 모든 카메라·캐릭터 키를 원래 IMC 값으로 되돌린다.

`ESC`와 `F5`는 옵션 화면을 항상 닫을 수 있도록 예약한다. 같은 조작 그룹(카메라 또는 캐릭터)에서
이미 사용 중인 키를 고르면 기존 바인딩을 몰래 덮어쓰지 않고 충돌 안내를 표시한다. 서로 다른
컨텍스트는 기존 기본값처럼 `WASD`를 공유할 수 있다.

`B`는 실제 런타임에서 건축 모드와 시설 목록 전환을 직접 처리하는 고정 키다. 키보드와
`고정 단축키` 목록 양쪽에 표시되며 클릭하거나 누르면 현재 설정을 확인할 수 있다. 아직
Enhanced Input으로 이전되지 않았으므로 이 화면에서는 변경할 수 없고, 다른 동작에 B를
지정하려는 시도도 이중 실행을 막기 위해 거부한다.

## 실제 리바인딩 범위

현재 Enhanced Input을 사용하는 관찰 카메라와 캐릭터 입력만 변경 가능 항목으로 노출한다.
건축 B는 실제 동작을 확인할 수 있도록 고정·읽기 전용으로 따로 표시한다. Atlas·미니게임의 일부
키도 아직 `EKeys` 직접 비교로 구현되어 있으므로 변경 가능 항목으로 거짓 노출하지 않는다. 해당
소비자가 InputAction 또는 공용 키 바인딩 계층으로 이전되면 같은 패널에 매핑 이름만 추가해
확장할 수 있다.

## 생성 및 검증

- `FF.KeyBindingUMG`: 텍스처 import, IMC Player Mappable 메타데이터 설정, 전용 WBP 생성
- `FestaFiesta.UI.Settings.KeyBinding`: config·IMC·WBP·텍스처 계약 자동화 테스트
