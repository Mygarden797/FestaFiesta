# 도감 보상 UI

건설 목록과 시설 정보의 도감 버튼에는 발견한 조합의 미수령 보상 또는 달성한 수집 보상이 남아 있으면 빨간 `!` 아이콘을 표시합니다. 도감을 열거나 일부 보상을 받아도 유지되며, 받을 보상을 모두 수령하면 즉시 사라집니다. 새 보상이 생기면 다시 표시됩니다. 미발견 조합, 아직 달성하지 않은 수집 단계, 골드가 없는 조합은 보상 알림에 포함하지 않습니다. 기존 첫 도감 안내 아이콘은 처음 열기 전까지 같은 위치에 표시합니다.

보상 수령은 수수료 오른쪽의 선물 아이콘으로 실행합니다. 수령 가능 상태는 이리디센스 테두리를 두른 짙은 청록 버튼, 수령 완료는 움직임 없는 비취색 체크 배지로 구분합니다. 골드는 발견할 때가 아니라 버튼을 눌렀을 때 한 번 지급합니다.

미발견 조합에도 보상이 있으면 선물 아이콘과 `잠김` 표시를 보여 줍니다. 마우스를 올리면 발견 후 수령할 골드를 확인할 수 있습니다. 이 표시는 클릭으로 수령할 수 없는 안내이며, 발견하면 `받기` 버튼으로 바뀝니다. 보상이 없는 미발견 조합에는 표시하지 않습니다.

잠김·수령 가능·완료 모두 같은 `Size_RewardState` 영역(72×32 UMG 단위)을 채웁니다. 화면 DPI와 카드 축소율을 함께 적용하므로 전환 시 외곽 크기와 위치, 수수료 배치가 유지됩니다. 보상 없음 상태에서는 이 영역을 접되, 기존 수령 이력은 완료 표시로 유지합니다.

## 저장 에셋

- 카드: `/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard`
- 선물: `/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Icons/T_FF_Codex_RewardGift`
- 재질: `/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/M_FF_CodexRewardIridescent`
- 상태 인스턴스: `MI_FF_CodexReward_Ready`, `_Hover`, `_Pressed`, `_Claimed`

UI 도메인의 투명 재질이 둥근 윤곽, 입체 테두리, 금빛·민트·보랏빛 간섭색과 느린 광택을 그립니다. UV와 시간에 따른 위상 변화로 이리디센스를 표현하며, 글자와 선물은 별도의 UMG 위젯으로 유지합니다. 수령 완료 재질은 시간과 무관한 정적인 색을 출력합니다. 매 카드에 동적 재질이나 타이머를 만들지 않고 저장된 상태 인스턴스를 공유합니다.

`Aspect`는 브러시 가로/세로 비율, `FilmPhase`는 색 위상, `RimStrength`는 테두리 강도입니다. `MotionStrength`는 기본·마우스 오버에서 1, 눌림·완료에서 0입니다. `State`는 기본 0 / 마우스 오버 1 / 눌림 2 / 완료 3입니다. 버튼 전경은 크림색, 완료 체크와 글자는 연한 민트색입니다.

## 명시적 에디터 반영

최신 Editor 빌드를 사용하고 PIE를 중지한 `/Engine/Maps/Entry`에서 `apply_reward_iridescence.py`를 `ExecutePythonScript`로 실행합니다. 스크립트는 `create_reward_materials.py`로 재질을 생성한 뒤 `FF.ApplyFacilityCodexRewardIridescence` 명령으로 기존 카드의 브러시와 전경 색을 저장합니다.

선행 반영 순서는 `apply_actions.py` → `apply_reward_icon.py` → `apply_reward_status.py`입니다. 현재 Content에는 이 과정의 결과가 저장되어 있습니다. 런타임에 위젯 트리를 다시 만들지 않습니다. 버전 메타데이터로 중복 반영을 막아 이후 Designer 편집을 보존합니다.

수령 전후 크기 통일은 이리디센스 반영 후 `apply_reward_bounds.py`로 저장합니다. 명령은 `FF.ApplyFacilityCodexRewardBounds`이며, 검증 결과는 `Saved/FacilityCodexRewardBounds_20260921/`에 기록합니다.

미발견 보상 안내는 그다음 `apply_reward_hint.py`로 저장합니다. 명령은 `FF.ApplyFacilityCodexRewardHint`이며, 검증 결과는 `Saved/FacilityCodexRewardHint_20260921/`에 기록합니다.

반영 결과·이전 에셋 백업은 `Saved/FacilityCodexRewardIridescence_20260921/`에 저장합니다. 검증 스크립트는 대상 6개 에셋의 재실행 시 해시 불변과 다른 UI 에셋 보존을 확인합니다.

`FestaFiesta.FacilityCodex.UI.AuthoredWidgets`는 실제 저장 위젯을 렌더링하고 재질 상태·마우스 입력·수령 후 표시 전환·목록 재사용·여러 해상도의 배치를 확인합니다. 화면 캡처는 `Saved/Automation/FacilityCodex/FacilityCodexRewardStates720.png`, `FacilityCodexRewardStates1080.png`, `FacilityCodexRewardHover1080.png`입니다.
