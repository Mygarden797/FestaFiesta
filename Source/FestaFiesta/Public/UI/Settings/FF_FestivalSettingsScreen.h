// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_FestivalSettingsScreen.generated.h"

class UButton;
class UGameUserSettings;
class UFF_SettingSelectRow;
class UFF_SettingSliderRow;
class UFF_SettingToggleRow;
class UTextBlock;
class UVerticalBox;

UENUM(BlueprintType)
enum class EFF_FestivalSettingsCategory : uint8
{
	Display       UMETA(DisplayName = "화면"),
	Audio         UMETA(DisplayName = "오디오"),
	Gameplay      UMETA(DisplayName = "게임플레이"),
	Accessibility UMETA(DisplayName = "접근성")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnFestivalSettingsCloseRequested);

/**
 * 메인 축제 화면 위에 올라가는 Light Teal 설정 화면.
 *
 * 레이아웃·폰트·간격은 WBP가 소유하고, 실제 값은 각 전용 서브시스템이 소유한다.
 * 화면 문구와 슬라이더 범위는 WBP Class Defaults에 노출되어 디자이너와 기획자가 C++ 수정 없이 바꾼다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Festival Settings Screen"))
class FESTAFIESTA_API UFF_FestivalSettingsScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_FestivalSettingsScreen(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Settings")
	FFF_OnFestivalSettingsCloseRequested OnCloseRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void SelectCategory(EFF_FestivalSettingsCategory Category);
	void RebuildRows();
	void BuildDisplayRows(UGameUserSettings& UserSettings);
	void BuildResolutionOptions(const UGameUserSettings& UserSettings);
	void BuildResolutionScaleOptions(const UGameUserSettings& UserSettings);
	void BuildFrameRateOptions(const UGameUserSettings& UserSettings);
	void RefreshCategoryVisuals();
	void RefreshCurrentSummary();

	UFF_SettingToggleRow* AddToggleRow(const FText& Label, const FText& Description, bool bInitialValue);
	UFF_SettingSliderRow* AddSliderRow(const FText& Label, const FText& Description, float InitialValue,
		float MinValue, float MaxValue, float StepSize);
	UFF_SettingSelectRow* AddSelectRow(FName SettingId, const FText& Label, const FText& Description,
		const TArray<FText>& Options, int32 InitialIndex);

	TSubclassOf<UFF_SettingToggleRow> ResolveToggleRowClass() const;
	TSubclassOf<UFF_SettingSliderRow> ResolveSliderRowClass() const;
	TSubclassOf<UFF_SettingSelectRow> ResolveSelectRowClass() const;
	TArray<FText> GetQualityLevelOptions(bool bIncludeCustom) const;

	UFUNCTION()
	void HandleDisplayClicked();

	UFUNCTION()
	void HandleAudioClicked();

	UFUNCTION()
	void HandleGameplayClicked();

	UFUNCTION()
	void HandleAccessibilityClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleVSyncToggled(bool bEnabled);

	UFUNCTION()
	void HandleDynamicResolutionToggled(bool bEnabled);

	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UFUNCTION()
	void HandleSelectionChanged(FName SettingId, int32 SelectedIndex);

	UFUNCTION()
	void HandleZoomToCursorToggled(bool bEnabled);

	UFUNCTION()
	void HandleEdgeScrollToggled(bool bEnabled);

	UFUNCTION()
	void HandleReduceMotionToggled(bool bEnabled);

	// --- WBP 바인딩 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Display;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Audio;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Gameplay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Accessibility;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Reset;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Done;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_DisplayCategory;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_AudioCategory;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_GameplayCategory;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_AccessibilityCategory;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_CategoryTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_CurrentSummary;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UVerticalBox> VerticalBox_Rows;

	// --- WBP Class Defaults: 문구와 범위는 코드 수정 없이 변경한다 ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Categories")
	FText DisplayCategoryLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Categories")
	FText AudioCategoryLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Categories")
	FText GameplayCategoryLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Categories")
	FText AccessibilityCategoryLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText WindowModeLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText WindowModeDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText ResolutionLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText ResolutionDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText ResolutionScaleLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText ResolutionScaleDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText VSyncLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText VSyncDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText DynamicResolutionLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText DynamicResolutionDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText FrameRateLimitLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Basic")
	FText FrameRateLimitDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText GraphicsPresetLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText GraphicsPresetDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText ViewDistanceQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText ViewDistanceQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText AntiAliasingQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText AntiAliasingQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText ShadowQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText ShadowQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText GlobalIlluminationQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText GlobalIlluminationQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText ReflectionQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText ReflectionQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText PostProcessingQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText PostProcessingQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText TextureQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText TextureQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText EffectsQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText EffectsQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText FoliageQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText FoliageQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText ShadingQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Quality")
	FText ShadingQualityDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Options")
	TArray<FText> WindowModeOptionLabels;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Options")
	TArray<FText> QualityLevelOptionLabels;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Options")
	FText CustomQualityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Display|Options")
	FText UnlimitedFrameRateLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Audio")
	FText MasterVolumeLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Audio")
	FText MasterVolumeDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Gameplay")
	FText ZoomToCursorLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Gameplay")
	FText ZoomToCursorDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Gameplay")
	FText EdgeScrollLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Gameplay")
	FText EdgeScrollDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Accessibility")
	FText ReduceMotionLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Accessibility")
	FText ReduceMotionDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rows")
	TSubclassOf<UFF_SettingToggleRow> ToggleRowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rows")
	TSubclassOf<UFF_SettingSliderRow> SliderRowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rows")
	TSubclassOf<UFF_SettingSelectRow> SelectRowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behavior")
	EFF_FestivalSettingsCategory DefaultCategory = EFF_FestivalSettingsCategory::Display;

private:
	UButton* GetCategoryButton(EFF_FestivalSettingsCategory Category) const;
	UTextBlock* GetCategoryText(EFF_FestivalSettingsCategory Category) const;
	FText GetCategoryLabel(EFF_FestivalSettingsCategory Category) const;

	TArray<FIntPoint> ResolutionOptions;
	TArray<int32> ResolutionScaleOptions;
	TArray<int32> FrameRateOptions;
	TWeakObjectPtr<UFF_SettingSelectRow> QualityPresetRow;
	EFF_FestivalSettingsCategory SelectedCategory = EFF_FestivalSettingsCategory::Display;
};
