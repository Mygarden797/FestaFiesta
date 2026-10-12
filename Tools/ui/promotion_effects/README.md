# 홍보 카드 효과 정보 적용

기존 `/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepPromotionCard`에 두 정보 행만 추가한다. 전체 HUD 생성기는 실행하지 않는다.

- `VerticalBox_Card`의 `Text_Effect` 다음에 `Text_Buzz`, `Text_Forecast`를 넣고 `Text_Badge` 및 나머지 기존 행의 상대 순서를 보존한다.
- 새 행은 기존 `Text_Effect`의 폰트·색을 복사한 15pt 중앙 정렬이다. 기본 문구는 `화제성 +0`, `예상 방문객 확인 중\n현재 입장료 기준`이며 아래 여백은 각각 8px·12px다.
- `SizeBox_Card`의 기존 폭·최대 높이·고정 높이는 보존하고 `MinDesiredHeight`만 최소 320px로 설정한다. 이미 더 크면 그 값을 유지한다.
- 기존 위젯 인스턴스·스타일·슬롯 설정을 보존한다. 재실행하면 동명 행을 재사용하며, 기존 추가 행의 Designer 편집을 덮어쓰지 않는다.

## 실행

관련 C++의 `Text_Buzz`, `Text_Forecast` `BindWidgetOptional` 멤버가 포함된 Development Editor 빌드 후 모든 에디터를 닫고 실행한다.

```powershell
& .\Tools\ui\promotion_effects\run.ps1
```

`run.ps1`은 기존 공용 `bright_globe/run_editor.ps1`을 사용해 UE 5.7을 숨김·NullRHI로 실행한다. `EditorPerProjectUserSettings.ini`, `GameUserSettings.ini`, `Saved/SaveGames` 파일은 실행 전 백업하고 종료 후 해시 검증하여 복원한다. 새 개인 파일은 백업 폴더로 옮겨 보관한다.

- 에셋 백업: `Saved/UIBackups/PromotionEffects/<시각>/WBP_FF_PrepPromotionCard.uasset`
- 적용 결과·전후 비교: `Saved/PromotionEffects/apply-report.json`
- 개인 상태 백업·복원 결과: `Saved/PromotionEffects/SessionBefore-*`, `apply-session-restored.json`
- 프로세스 로그: `Saved/BrightGlobe/promotion_effects_apply.log`

컴파일된 클래스와 두 C++ 멤버에 대응하는 위젯 이름·타입을 확인하고 저장한다. 저장 후 행 순서·원본 스타일을 재조회한다. 이는 현재 프로세스의 로드된 에셋 확인이다. 실제 값 표시·글자 줄바꿈·카드 높이는 플레이 화면에서 확인한다.

## UE 5.7 API 근거

- `UWidgetTree::ConstructWidget`은 트리를 Outer로 둔 `NewObject`를 사용한다. 같은 방식의 `unreal.new_object(..., outer=tree)`로 추가한다.
- `UPanelWidget::InsertChildAt`·`ShiftChild`는 UFUNCTION이 아니므로 Python에 노출되지 않는다. 공개된 `remove_child`·`add_child_to_vertical_box`로 뒤쪽 행만 재연결하고 `VerticalBoxSlot`의 `Size`, `Padding`, `HorizontalAlignment`, `VerticalAlignment`를 복원한다.
- `UWidgetBlueprintGeneratedClass::InitializeWidgetStatic`은 위젯과 같은 이름의 C++ 객체 속성을 찾아 연결한다. 이미 `BindWidgetOptional` 멤버가 있으므로 새 행의 `bIsVariable` 변경은 필요 없다. 이 플래그는 Blueprint 변수 생성에 쓰인다.
- 편집기 변수 GUID는 별도로 필요하다. Python에 노출되지 않은 `OnVariableAdded`를 `FF.RegisterPrepPromotionEffectWidgets` C++ 명령으로 두 신규 이름에만 호출한다. 기존 GUID는 보존한다.
- 기존 카드에 사용자 지정 `HeightOverride` 또는 `MaxDesiredHeight`가 있으면 그대로 보존하므로 실제 콘텐츠가 잘리는지는 Designer에서 확인한다.
- `TextBlock.set_justification`과 보호된 `WidgetBlueprint.Status` 읽기는 UE 5.7 Python에 공개되지 않는다. 정렬은 `set_editor_property('justification', ...)`, 컴파일 확인은 생성 클래스 조회와 템플릿 확인을 사용한다.
- `UWidgetBlueprintLibrary`의 Python 이름은 `WidgetLibrary`지만 `Create`는 내부 전용 Blueprint 함수여서 Python에 노출되지 않는다. 런타임 인스턴스 검사를 이 도구에서 시도하지 않는다.
