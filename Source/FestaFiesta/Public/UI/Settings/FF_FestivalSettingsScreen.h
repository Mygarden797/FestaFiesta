// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_FestivalSettingsScreen.generated.h"

class UButton;
class UGameUserSettings;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UFF_KeyBindingPanel;
class UFF_KeyBindingRuntimeSubsystem;
class UFF_SettingSelectRow;
class UFF_SettingSliderRow;
class UFF_SettingToggleRow;

/** ESC 옵션의 다섯 개 최상위 목적. 이름과 순서는 사용자에게 보이는 정보 구조의 계약이다. */
UENUM(BlueprintType)
enum class EFF_FestivalSettingsCategory : uint8
{
	Basic       UMETA(DisplayName = "기본설정"),
	Account     UMETA(DisplayName = "계정정보"),
	Keyboard    UMETA(DisplayName = "키보드 키 배치"),
	Graphics    UMETA(DisplayName = "그래픽"),
	ExitGame    UMETA(DisplayName = "게임종료")
};

/** 키가 같은 상황마다 다른 일을 하므로 문맥을 함께 보여 주는 읽기 전용 조작 안내 행. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SettingsKeyGuideEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Guide")
	FText Context;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Guide")
	FText Keys;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Guide")
	FText Action;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnFestivalSettingsCloseRequested);

/**
 * 메인 축제 화면 위에 올라가는 Light Teal ESC 옵션 화면.
 *
 * 행 조작은 이 화면의 draft만 바꾼다. `저장`을 누를 때만 엔진/런타임 설정에 반영하고,
 * 해상도 변경은 15초 유지 확인을 거친다. `이전으로`와 ESC는 저장되지 않은 변경을 버리기 전에
 * 한 번 더 확인하므로 자동 저장이나 뜻하지 않은 설정 유실이 없다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Festival Settings Screen"))
class FESTAFIESTA_API UFF_FestivalSettingsScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_FestivalSettingsScreen(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Settings")
	FFF_OnFestivalSettingsCloseRequested OnCloseRequested;

	/** ESC/F5와 모든 이전 버튼이 공유하는 안전한 닫기 경로. false면 확인을 기다리는 중이다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	bool RequestNavigateBack();

	/** 일시정지 허브의 바로가기에서 열 때 첫 카테고리를 명시한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void ShowCategory(EFF_FestivalSettingsCategory Category);

	/** WBP 리바인딩 패널을 만들 수 없을 때 보여 줄 읽기 전용 안전 안내. */
	const TArray<FFF_SettingsKeyGuideEntry>& GetKeyboardGuideEntries() const { return KeyboardGuideEntries; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void CaptureDraft();
	void SelectCategory(EFF_FestivalSettingsCategory Category);
	void RebuildRows();
	void BuildBasicRows();
	void BuildAccountRows();
	void BuildKeyboardRows();
	void PopulateDefaultKeyboardGuide();
	void BuildGraphicsRows(UGameUserSettings& UserSettings);
	void BuildResolutionOptions(const UGameUserSettings& UserSettings, bool bPreserveAppliedResolution = false);
	void BuildResolutionScaleOptions(const UGameUserSettings& UserSettings);
	void BuildFrameRateOptions(const UGameUserSettings& UserSettings);
	int32 GetGraphicsPresetExcludingResolution(const UGameUserSettings& UserSettings) const;
	void RefreshCategoryVisuals();
	void RefreshSummary();
	void RefreshOptionKeyHint();
	void RefreshActionButtons();
	void RefreshButtonFocusPresentation();
	void HandleKeyBindingDraftChanged(bool bHasPendingChanges);
	UFF_KeyBindingRuntimeSubsystem* ResolveKeyBindingRuntime() const;
	void UpdateDirtyState();
	void CommitDraft();
	void CaptureActiveVideoModeForRollback(const UGameUserSettings& UserSettings);
	void BeginVideoModePreview(UGameUserSettings& UserSettings);
	void ConfirmVideoModeAndCommit();
	void RollbackVideoModePreview();
	bool TickVideoModeConfirmation(float DeltaSeconds);
	void SetNavigationEnabled(bool bEnabled);
	void EnsureExitCategoryButton();

	UFF_SettingToggleRow* AddToggleRow(const FText& Label, const FText& Description, bool bInitialValue);
	UFF_SettingSliderRow* AddSliderRow(const FText& Label, const FText& Description, float InitialValue,
		float MinValue, float MaxValue, float StepSize);
	UFF_SettingSelectRow* AddSelectRow(FName SettingId, const FText& Label, const FText& Description,
		const TArray<FText>& Options, int32 InitialIndex);
	void AddInfoRow(const FText& Label, const FText& Value, const FText& Description = FText::GetEmpty());
	void AddSectionHeader(const FText& Label);
	UButton* AddActionButton(const FText& Label, bool bDanger);

	TSubclassOf<UFF_SettingToggleRow> ResolveToggleRowClass() const;
	TSubclassOf<UFF_SettingSliderRow> ResolveSliderRowClass() const;
	TSubclassOf<UFF_SettingSelectRow> ResolveSelectRowClass() const;
	TSubclassOf<UFF_KeyBindingPanel> ResolveKeyBindingPanelClass() const;
	TArray<FText> GetQualityLevelOptions(bool bIncludeCustom) const;

	UFUNCTION() void HandleBasicClicked();
	UFUNCTION() void HandleAccountClicked();
	UFUNCTION() void HandleKeyboardClicked();
	UFUNCTION() void HandleGraphicsClicked();
	UFUNCTION() void HandleExitGameClicked();
	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleSaveClicked();
	UFUNCTION() void HandleVSyncToggled(bool bEnabled);
	UFUNCTION() void HandleMasterVolumeChanged(float Value);
	UFUNCTION() void HandleSelectionChanged(FName SettingId, int32 SelectedIndex);
	UFUNCTION() void HandleZoomToCursorToggled(bool bEnabled);
	UFUNCTION() void HandleEdgeScrollToggled(bool bEnabled);
	UFUNCTION() void HandleReduceMotionToggled(bool bEnabled);
	UFUNCTION() void HandleQuitGameClicked();
	UFUNCTION() void HandleQuitGameCancelled();
	UFUNCTION() void HandleQuitGameConfirmed();

	// 새 WBP의 명시 바인딩. Optional인 이유는 사용하지 않는 구형 WBP도 데이터 검증을 통과해야 하기 때문이다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Basic;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Account;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Keyboard;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Graphics;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_ExitGame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_BasicCategory;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_AccountCategory;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_KeyboardCategory;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_GraphicsCategory;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_ExitGameCategory;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Close;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Back;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Save;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Back;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Save;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Intro;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_KeyHint;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_CategoryTitle;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_CurrentSummary;
	/** LightTeal 전체 화면 옵션의 가로 최상위 카테고리 탭. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UHorizontalBox> HorizontalBox_Categories;
	/** 구형 세로 카테고리 WBP 호환용. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UVerticalBox> VerticalBox_Categories;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UVerticalBox> VerticalBox_Rows;

	// 구형 WBP 호환 바인딩. 새 화면이 생성되면 사용하지 않는다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UButton> Button_Display;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UButton> Button_Audio;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UButton> Button_Gameplay;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UButton> Button_Accessibility;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UButton> Button_Reset;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UButton> Button_Done;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UTextBlock> Text_DisplayCategory;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UTextBlock> Text_AudioCategory;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UTextBlock> Text_GameplayCategory;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings|Legacy")
	TObjectPtr<UTextBlock> Text_AccessibilityCategory;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rows")
	TSubclassOf<UFF_SettingToggleRow> ToggleRowClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rows")
	TSubclassOf<UFF_SettingSliderRow> SliderRowClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rows")
	TSubclassOf<UFF_SettingSelectRow> SelectRowClass;

	/** 키보드 이미지와 실제 Enhanced Input 리바인딩을 함께 제공하는 UMG/WBP 패널. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rows")
	TSubclassOf<UFF_KeyBindingPanel> KeyBindingPanelClass;

	/** 실제 입력 계약에서 매번 다시 만드는 읽기 전용 안내. Blueprint에 오래된 키 표를 저장하지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Content|Keyboard")
	TArray<FFF_SettingsKeyGuideEntry> KeyboardGuideEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior")
	EFF_FestivalSettingsCategory DefaultCategory = EFF_FestivalSettingsCategory::Basic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior",
		meta = (ClampMin = "5.0", ClampMax = "30.0", ForceUnits = "s"))
	float VideoModeConfirmationSeconds = 15.f;

private:
	struct FSettingsDraft
	{
		EWindowMode::Type WindowMode = EWindowMode::WindowedFullscreen;
		FIntPoint Resolution = FIntPoint(1920, 1080);
		int32 ResolutionScalePercent = 100;
		int32 FrameRateLimit = 60;
		int32 GraphicsPreset = 3;
		bool bVSync = true;
		float MasterVolume = 0.8f;
		bool bZoomToCursor = true;
		bool bEdgeScroll = true;
		bool bReduceMotion = false;

		bool Equals(const FSettingsDraft& Other) const;
	};

	UButton* GetCategoryButton(EFF_FestivalSettingsCategory Category) const;
	UTextBlock* GetCategoryText(EFF_FestivalSettingsCategory Category) const;
	UButton* GetBackButton() const;
	UButton* GetSaveButton() const;
	FText GetCategoryLabel(EFF_FestivalSettingsCategory Category) const;
	void SetButtonLabel(UButton* Button, UTextBlock* BoundText, const FText& Label) const;

	TArray<FIntPoint> ResolutionOptions;
	TArray<int32> ResolutionScaleOptions;
	TArray<int32> FrameRateOptions;
	FSettingsDraft Draft;
	FSettingsDraft Saved;
	EFF_FestivalSettingsCategory SelectedCategory = EFF_FestivalSettingsCategory::Basic;
	bool bHasSelectedCategory = false;
	FTSTicker::FDelegateHandle VideoModeTickerHandle;
	FDelegateHandle KeyBindingDraftChangedHandle;
	EWindowMode::Type PreviewRollbackWindowMode = EWindowMode::WindowedFullscreen;
	FIntPoint PreviewRollbackResolution = FIntPoint::ZeroValue;
	float VideoModeSecondsRemaining = 0.f;
	bool bDraftCaptured = false;
	bool bDirty = false;
	bool bSavedNotice = false;
	bool bDiscardConfirmationArmed = false;
	bool bExitConfirmationArmed = false;
	bool bAwaitingVideoModeConfirmation = false;
	bool bHasPreviewRollbackVideoMode = false;
	bool bPreserveAppliedResolutionOption = false;
	bool bNormalizedUnsupportedVideoMode = false;
	bool bQuitRequested = false;
	bool bIsTearingDown = false;
	TArray<FKey> PresentedOpenCloseKeys;
	uint64 LastBackRequestFrame = MAX_uint64;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_QuitAction;
	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_QuitCancel;
	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_QuitConfirm;
	UPROPERTY(Transient)
	TObjectPtr<UFF_SettingSelectRow> Row_WindowMode;
	UPROPERTY(Transient)
	TObjectPtr<UFF_SettingSelectRow> Row_GraphicsPreset;
	/** 현재 키보드 카테고리에 생성된 패널. 화면 포커스가 카테고리 버튼에 있어도 키 조회를 전달한다. */
	UPROPERTY(Transient)
	TObjectPtr<UFF_KeyBindingPanel> ActiveKeyBindingPanel;
};
