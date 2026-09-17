// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_OptionsScreen.generated.h"

class UFF_CommonButtonBase;
class UFF_KeyBindingPanel;
class UFF_SettingSelectRow;
class UFF_SettingSliderRow;
class UFF_SettingToggleRow;
class UTextBlock;
class UVerticalBox;

/** 옵션 분류 6종. `docs/ui/main-menu-and-save.md` 3절의 목록과 같다. */
UENUM(BlueprintType)
enum class EFF_OptionCategory : uint8
{
	Language		UMETA(DisplayName = "언어"),
	Accessibility	UMETA(DisplayName = "접근성"),
	Sound			UMETA(DisplayName = "사운드"),
	Graphics		UMETA(DisplayName = "그래픽"),
	Controls		UMETA(DisplayName = "조작"),
	Credits			UMETA(DisplayName = "크레딧")
};

/**
 * 크레딧 한 줄.
 *
 * **원천은 `THIRD_PARTY_NOTICES.md` 의 "현재 등록 목록" 표다.** 여기 값이 그 표와 어긋나면
 * 문서가 맞다. 파일을 런타임에 읽지 않는 이유는 `.md` 가 쿠킹에 들어가지 않아
 * 패키징 빌드에서 크레딧이 통째로 비기 때문이다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_OptionCreditEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits")
	FText Asset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits")
	FText Usage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits")
	FText License;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnOptionsBackRequested);

/**
 * 옵션. 좌측 분류 6개 + 우측 설정 행.
 *
 * === 값을 소유하지 않는다 (AGENTS.md 규칙 2) ===
 * 이 화면은 읽고 요청만 한다. 실제 소유자는 다음과 같다.
 *
 * | 분류 | 소유자 | 저장 위치 |
 * | --- | --- | --- |
 * | 접근성 · 움직임 감소 | `UFF_UIAccessibilitySubsystem` | 세션 |
 * | 사운드 · 마스터 음량 | `UFF_RuntimeSettingsSubsystem` | `GameUserSettings.ini` |
 * | 조작 · 카메라 토글 2개 | `UFF_RuntimeSettingsSubsystem` | `GameUserSettings.ini` |
 * | 조작 · 키 배치 | `UFF_KeyBindingRuntimeSubsystem` | Enhanced Input User Settings |
 * | 그래픽 | `UGameUserSettings` | `GameUserSettings.ini` |
 *
 * 서브시스템·행 위젯·키 배치 패널은 **한광훈님이 만든 ESC 옵션의 것을 그대로 재사용한다.**
 * 값이 한 곳에만 있어야 타이틀에서 바꾼 설정이 축제에서도 같은 값이다
 * (GameInstance·LocalPlayer 스코프라 맵을 옮겨도 살아 있다).
 *
 * === 반영 시점이 두 가지다 ===
 * - **즉시**: 접근성. 화면 표현만 바뀌므로 바로 확인시키는 것이 값어치다.
 * - **`적용`**: 사운드 · 조작. 게임 동작과 `.ini` 를 건드린다.
 *   슬라이더를 끌 때마다 `SetMasterVolume` 을 부르면 **프레임마다 ini 를 쓴다** —
 *   그래서 세 값을 `CommitSettings` 로 한 번에 확정한다 (`UFF_RuntimeSettingsSubsystem` 주석).
 *
 * `Button_Apply` 가 없는 구형 WBP 는 확정할 길이 없으므로 그때만 즉시 반영으로 되돌아간다.
 *
 * === 분류 버튼을 런타임에 만들지 않는다 ===
 * 6개를 컨테이너에 런타임으로 채우면 **디자이너에서 하나도 보이지 않아 위치·크기를 손으로
 * 만질 수 없다.** Live HUD 탭 9개에서 실제로 겪고 명시 바인딩으로 되돌린 문제다
 * (`docs/memory/wbp-codegen-workflow.md`). 그래서 여섯 개를 각각 바인딩한다.
 * 설정 행은 분류마다 개수가 달라 런타임 생성이 맞다.
 *
 * === 문구를 `UMETA` 에서 읽지 않는다 ===
 * `UEnum::GetDisplayValueAsText` 는 에디터 메타데이터를 읽고 그것은 패키징에서 떨어져 나간다.
 * 2026-08-28 시연 패키징에서 하단 탭이 전부 영문이 된 원인이다. 분류 이름은
 * `UFF_UIDisplayTextLibrary::GetOptionCategoryText` 한 곳에서 낸다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Options Screen"))
class FESTAFIESTA_API UFF_OptionsScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_OptionsScreen(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnOptionsBackRequested OnBackRequested;

#if WITH_EDITOR
	/**
	 * **WBP 생성기 전용.** 컴파일된 CDO 에 행 위젯 클래스를 박는다. 런타임에서 부르지 않는다.
	 *
	 * 비어 있어도 `Resolve*RowClass()` 의 경로 대비책이 있지만, **문자열 경로는 쿠커의 참조
	 * 그래프에 잡히지 않는다.** 그래서 실제 참조는 WBP 가 들고 있어야 하고 생성기가 채운다.
	 * `protected` 멤버라 밖에서 못 만지므로 클래스 안에 둔다 — 리플렉션으로 프로퍼티 이름을
	 * 찾아 쓰면 이름을 바꾸는 순간 조용히 깨진다 (`UFF_CommonButtonBase` 와 같은 이유).
	 */
	void ApplyGeneratedRowClasses(
		TSubclassOf<UFF_SettingToggleRow> InToggleRowClass,
		TSubclassOf<UFF_SettingSliderRow> InSliderRowClass,
		TSubclassOf<UFF_SettingSelectRow> InSelectRowClass,
		TSubclassOf<UFF_KeyBindingPanel> InKeyBindingPanelClass);
#endif // WITH_EDITOR

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	/** 처음 포커스는 선택된 분류 버튼이다. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/**
	 * 조작 분류의 키 배치 패널이 다음 키를 기다리는 중이면 그쪽으로 넘긴다.
	 * 포커스가 분류 버튼에 있어도 리바인딩이 되게 하려는 것이다 (한광훈님 ESC 화면과 같은 처리).
	 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleCategoryClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleBackClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleApplyClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleReduceMotionToggled(bool bEnabled);

	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UFUNCTION()
	void HandleZoomToCursorToggled(bool bEnabled);

	UFUNCTION()
	void HandleEdgeScrollToggled(bool bEnabled);

	/** 해상도 되돌림 확인의 남은 시간을 센다. 확인 중이 아니면 아무 일도 하지 않는다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleGraphicsSelectionChanged(FName SettingId, int32 SelectedIndex);

	UFUNCTION()
	void HandleVSyncToggled(bool bEnabled);

	/** 분류를 고르고 우측을 다시 그린다. */
	void SelectCategory(EFF_OptionCategory Category);

	/** 우측 설정 행을 전부 다시 만든다. 부분 갱신을 하지 않는다. */
	void RebuildRows();

	void BuildLanguageRows();
	void BuildAccessibilityRows();
	void BuildSoundRows();
	void BuildGraphicsRows();
	void BuildControlsRows();
	void BuildCreditsRows();

	/** 저장된 값을 draft 로 복사한다. 화면을 열 때와 `적용` 뒤에 부른다. */
	void CaptureDraft();

	/** draft 세 값을 한 번에 확정한다. 실제 저장은 `UFF_RuntimeSettingsSubsystem` 이 한다. */
	void CommitDraft();

	/** draft 가 저장값과 다른지 보고 `적용` 버튼과 안내 문구를 갱신한다. */
	void RefreshDirtyState();

	/** `적용` 버튼이 없는 WBP 에서만 true. 그 경우 값 변경을 즉시 확정한다. */
	bool ShouldCommitImmediately() const { return Button_Apply == nullptr; }

	UFF_SettingToggleRow* AddToggleRow(const FText& Label, const FText& Description, bool bInitialValue);
	UFF_SettingSliderRow* AddSliderRow(const FText& Label, const FText& Description, float InitialValue,
		float MinValue, float MaxValue, float StepSize);

	/** 유한한 선택지를 순환하는 행. 그래픽의 창 모드·해상도·프레임 제한이 쓴다. */
	UFF_SettingSelectRow* AddSelectRow(FName SettingId, const FText& Label, const FText& Description,
		const TArray<FText>& Options, int32 InitialIndex);

	/** 조작할 것이 없는 읽기 전용 한 줄. 크레딧·언어처럼 값이 아니라 사실을 보여줄 때 쓴다. */
	void AddInfoRow(const FText& Label, const FText& Value, const FText& Description = FText::GetEmpty());

	/** 아직 붙지 않은 분류에 **왜** 비었는지 적는다. 만져도 아무 일이 없으면 고장으로 보인다. */
	void AddPendingRow(const FText& Reason);

	/** 분류에 해당하는 버튼. 없으면 nullptr. */
	UFF_CommonButtonBase* GetCategoryButton(EFF_OptionCategory Category) const;

	// --- 그래픽. 값은 엔진 `UGameUserSettings` 가 소유한다 ---

	/** 저장된 그래픽 값을 draft 로 복사하고 선택지 목록을 다시 만든다. */
	void CaptureGraphicsDraft();

	/** draft 를 엔진에 반영한다. 창 모드·해상도가 바뀌면 되돌림 확인을 시작한다. */
	void CommitGraphicsDraft();

	/** 15초 확인을 시작한다. 이 동안 `적용` 버튼은 `유지` 가 된다. */
	void BeginVideoModeConfirmation();

	/** 사용자가 유지를 택했다. 지금 화면 모드를 확정한다. */
	void ConfirmVideoMode();

	/** 시간이 다 됐거나 화면을 닫았다. 이전 화면 모드로 되돌린다. */
	void RollbackVideoMode();

	/** 선택지 목록. 원천은 `UFF_FestivalSettingsDeveloperSettings` (Project Settings > Game) 다. */
	void RebuildGraphicsOptionLists();

	TArray<FText> GetWindowModeOptions() const;
	TArray<FText> GetQualityPresetOptions() const;

	TSubclassOf<UFF_SettingSelectRow> ResolveSelectRowClass() const;
	TSubclassOf<UFF_SettingToggleRow> ResolveToggleRowClass() const;
	TSubclassOf<UFF_SettingSliderRow> ResolveSliderRowClass() const;
	TSubclassOf<UFF_KeyBindingPanel> ResolveKeyBindingPanelClass() const;

	// --- 분류 버튼. 런타임 생성이 아니라 명시 바인딩이다 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Language;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Accessibility;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Sound;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Graphics;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Controls;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Credits;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Back;

	/**
	 * `적용`. **`BindWidgetOptional` 이다** — 이 버튼이 생기기 전의 WBP 도 계속 열려야 한다.
	 * 없으면 `ShouldCommitImmediately()` 가 true 가 되어 값 변경이 바로 저장된다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Apply;

	// --- 우측 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_CategoryTitle;

	/** 설정 행이 들어가는 곳. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UVerticalBox> VerticalBox_Rows;

	/** 아직 붙지 않은 분류에서 보이는 사유. 값 행이 있는 분류에서는 접힌다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_NotImplemented;

	/** `적용` 안내. 바꾼 값이 있는지·방금 저장했는지를 글로 적는다 (AT-011 — 색만 쓰지 않는다). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_ApplyHint;

	// --- 행 위젯 클래스. 한광훈님 설정 화면의 것을 그대로 쓴다 ---

	/** 보통 `WBP_FF_SettingToggleRow`. 비어 있으면 그 경로를 직접 불러온다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Rows")
	TSubclassOf<UFF_SettingToggleRow> ToggleRowClass;

	/** 보통 `WBP_FF_SettingSliderRow`. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Rows")
	TSubclassOf<UFF_SettingSliderRow> SliderRowClass;

	/** 보통 `WBP_FF_SettingSelectRow`. **그래픽 분류를 붙일 때 쓴다** — 지금은 참조만 들고 있다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Rows")
	TSubclassOf<UFF_SettingSelectRow> SelectRowClass;

	/** 보통 `WBP_FF_KeyBindingPanel`. 조작 분류의 키 배치 표를 그린다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Rows")
	TSubclassOf<UFF_KeyBindingPanel> KeyBindingPanelClass;

	/** 크레딧 표. 기본값은 `THIRD_PARTY_NOTICES.md` 의 등록 목록을 옮긴 것이다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Credits")
	TArray<FFF_OptionCreditEntry> CreditEntries;

	/**
	 * 해상도·창 모드를 되돌리기까지 기다리는 시간.
	 * 한광훈님 ESC 옵션과 같은 15초다 — 두 화면의 약속이 달라지면 플레이어가 헷갈린다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Graphics",
		meta = (ClampMin = "5.0", ClampMax = "30.0", ForceUnits = "s"))
	float VideoModeConfirmationSeconds = 15.f;

	/**
	 * 처음 보여줄 분류. **기본을 `접근성` 으로 둔다** — 언어보다 실제로 조작할 것이 있어
	 * 화면을 열자마자 사유 문구만 보이는 첫인상을 피한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI")
	EFF_OptionCategory DefaultCategory = EFF_OptionCategory::Accessibility;

private:
	/** `UFF_RuntimeSettingsSubsystem` 이 한 번에 받는 세 값. 화면은 이 사본만 만진다. */
	struct FDraft
	{
		float MasterVolume = 0.8f;
		bool bZoomToCursor = true;
		bool bEdgeScroll = true;

		bool Equals(const FDraft& Other) const;
	};

	/**
	 * 엔진 `UGameUserSettings` 가 소유하는 그래픽 값의 사본.
	 *
	 * 화면은 이 사본만 만지고 `적용` 에서 한 번에 반영한다. 해상도·창 모드는 잘못 고르면
	 * 화면이 깨져 되돌릴 수 없으므로 **적용 뒤 15초 확인**을 거친다.
	 */
	struct FGraphicsDraft
	{
		EWindowMode::Type WindowMode = EWindowMode::WindowedFullscreen;
		FIntPoint Resolution = FIntPoint(1920, 1080);
		int32 ResolutionScalePercent = 100;
		int32 FrameRateLimit = 60;
		/** `UGameUserSettings::GetOverallScalabilityLevel`. -1 은 사용자 지정이다. */
		int32 QualityPreset = 3;
		bool bVSync = true;

		bool Equals(const FGraphicsDraft& Other) const;
		/** 창 모드나 해상도가 다르면 true. 이 둘만 되돌림 확인이 필요하다. */
		bool DiffersInVideoMode(const FGraphicsDraft& Other) const;
	};

	FGraphicsDraft GraphicsDraft;
	FGraphicsDraft SavedGraphics;

	TArray<FIntPoint> ResolutionOptions;
	TArray<int32> ResolutionScaleOptions;
	TArray<int32> FrameRateOptions;

	/** 되돌림 확인 중인지. 이 동안 `적용` 은 `유지` 로 동작한다. */
	bool bAwaitingVideoConfirm = false;
	float VideoConfirmSecondsRemaining = 0.f;
	/** 마지막으로 안내 문구에 찍은 남은 초. 매 프레임 같은 문구를 다시 만들지 않기 위함이다. */
	int32 PresentedConfirmSeconds = -1;

	EFF_OptionCategory SelectedCategory = EFF_OptionCategory::Accessibility;
	bool bHasSelectedCategory = false;
	FDraft Draft;
	FDraft Saved;

	/** 조작 분류에 지금 올라가 있는 키 배치 패널. 행을 다시 그리기 전에 캡처를 반드시 해제한다. */
	UPROPERTY(Transient)
	TObjectPtr<UFF_KeyBindingPanel> ActiveKeyBindingPanel;

	bool bDirty = false;
	bool bSavedNotice = false;

	/**
	 * `뒤로` 를 한 번 눌러 "버릴 거냐" 를 물어본 상태.
	 *
	 * 저장하지 않은 변경을 말없이 버리지 않으려는 것이다. 모달 확인창을 띄우지 않는 이유는
	 * 화면 스택에 한 겹을 더 얹는 것보다 같은 버튼을 두 번 누르는 쪽이 값이 싸고,
	 * 타이틀에서도 게임패드 조작이 그대로 유지되기 때문이다.
	 */
	bool bDiscardArmed = false;
};
