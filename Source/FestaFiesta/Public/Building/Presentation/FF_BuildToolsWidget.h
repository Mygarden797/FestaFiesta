// FF_BuildToolsWidget.h
// 역할: 건설 도구 버튼과 각도 입력을 건설 상태 및 명령에 연결한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Types/SlateEnums.h"
#include "TimerManager.h"
#include "FF_BuildToolsWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UComboBoxString;
class UEditableTextBox;
class UImage;
class USizeBox;
class UTextBlock;
class UWidgetAnimation;
class UWidgetTree;
class UFF_BuildModeSubsystem;
class UFF_BuildCatalogEconomyReadModel;
class UFF_BuildCatalogSubsystem;
class UFF_BuildingFeatureDiscoverySubsystem;
struct FKey;
struct FFFZSFacilitySpec;

UENUM(BlueprintType)
enum class EFF_BuildToolsDesignerPage : uint8
{
    Placement UMETA(DisplayName = "배치"),
    Catalog UMETA(DisplayName = "카탈로그"),
    Settings UMETA(DisplayName = "건축 설정"),
    Guide UMETA(DisplayName = "건축 기능 안내")
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_BuildToolsViewData
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bPreviewActive = false;
    UPROPERTY(BlueprintReadOnly) bool bMoving = false;
    UPROPERTY(BlueprintReadOnly) bool bDragPlacementEnabled = false;
    UPROPERTY(BlueprintReadOnly) bool bCanConfirm = false;
    UPROPERTY(BlueprintReadOnly) bool bGridVisible = false;
    UPROPERTY(BlueprintReadOnly) int32 PreferredSnapCm = 100;
    UPROPERTY(BlueprintReadOnly) int32 ActiveSnapCm = 100;
    UPROPERTY(BlueprintReadOnly) int32 DragPlacementGapCm = 0;
    UPROPERTY(BlueprintReadOnly) float YawDegrees = 0.0f;
    UPROPERTY(BlueprintReadOnly) FText FacilityName;
    UPROPERTY(BlueprintReadOnly) FText CostText;
    UPROPERTY(BlueprintReadOnly) FText StatusText;
    UPROPERTY(BlueprintReadOnly) FText BalanceText;
    UPROPERTY(BlueprintReadOnly) FText FootprintText;
    UPROPERTY(BlueprintReadOnly) FText QueueText;
    UPROPERTY(BlueprintReadOnly) FText AudienceText;
};

namespace FFBuildToolsModel
{
    /** Persistent toolbar controls stay above the game's ordinary window layers. */
    inline constexpr int32 ForegroundViewportZOrder = 1000;
    FESTAFIESTA_API FText FormatAudienceSpace(const FFFZSFacilitySpec& Spec);
    FESTAFIESTA_API FText FormatProblemStatus(const FText& Title, int32 ProblemIndex, int32 ProblemCount);
    // Explicit degree input is finite, decimal and within the displayed 0..360 range.
    // Invalid edits leave the caller's previous value untouched; 360 wraps to 0.
    FESTAFIESTA_API bool TryParseYawText(const FString& Text, float& OutYawDegrees);
    /** Extra edge-to-edge spacing for a drag stroke; invalid edits preserve the prior value. */
    FESTAFIESTA_API bool TryParseDragPlacementGapText(const FString& Text, int32& OutGapCm);
    /** Shared by runtime compatibility and the saved WBP builder; does not replace authored controls. */
    FESTAFIESTA_API UEditableTextBox* EnsureDragPlacementGapControl(UWidgetTree& Tree);
    /** Queue inputs commit together so a temporary partial edit never changes the reservation. */
    FESTAFIESTA_API bool TryParseQueueSettingsText(const FString& TeamsText, const FString& SpacingText,
        const FString& WidthText, int32& OutTeams, float& OutSpacingCm, float& OutWidthCm);
    FESTAFIESTA_API bool EnsureQueueSettingsControls(UWidgetTree& Tree);
    /** Top and height of the guide, bounded above the measured catalog edge. */
    FESTAFIESTA_API FVector2D ResolveHelpVerticalBounds(float ContentHeight, float AvailableBottom,
        float PreferredTop = 525.f, float ReservedTop = 104.f);
    /** A nearby label position clear of projected reservations and persistent HUD surfaces. */
    FESTAFIESTA_API bool ResolveContextPingPosition(const FVector2D& Marker, const FVector2D& PingSize,
        const FVector2D& ViewSize, const FBox2D& PreviewBounds, const TArray<FBox2D>& Occupied,
        float Gap, FVector2D& OutPosition);
    /** Required local dimensions, not a measurement of remaining free space. */
    FESTAFIESTA_API void FormatRequiredSpace(const FFFZSFacilitySpec& Spec, FText& OutFootprint, FText& OutQueue);
}

// UButton exposes focus initialization to subclasses, avoiding deprecated property writes.
UCLASS()
class FESTAFIESTA_API UFF_BuildToolsActionButton final : public UButton
{
    GENERATED_BODY()
public:
    UFF_BuildToolsActionButton();
    /** Editor builder only, before Slate construction. Catalog activation remains focusable. */
    void ConfigureFocusable(bool bFocusable) { InitIsFocusable(bFocusable); }

    // Normal/hover/pressed remain the ordinary Designer Button Style. Only the selected
    // variant lives here, so refreshes never replace a designer's brushes or padding.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection") bool bUseSelectionStyle = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection") FButtonStyle SelectionStyle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection") FSlateColor SelectionTextColor = FSlateColor(FLinearColor::White);
    void ApplySelection(bool bSelected, UTextBlock* Label);

private:
    UPROPERTY(Transient) FButtonStyle RestingStyle;
    FSlateColor RestingTextColor;
    bool bHasRestingStyle = false;
    bool bAppliedSelected = false;
};

// The editable WBP owns layout and styling. This class only binds user actions and projects
// BuildModeSubsystem state; tick only reprojects the small, non-interactive context marker.
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Tools"))
class FESTAFIESTA_API UFF_BuildToolsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Only chooses visible sections in the Designer; never replaces authored text or styling. */
    UPROPERTY(EditAnywhere, Category = "Designer Preview")
    bool bPreviewDesignerPage = true;
    UPROPERTY(EditAnywhere, Category = "Designer Preview", meta = (EditCondition = "bPreviewDesignerPage"))
    EFF_BuildToolsDesignerPage DesignerPage = EFF_BuildToolsDesignerPage::Placement;
    UPROPERTY(EditAnywhere, Category = "Designer Preview", meta = (EditCondition = "bPreviewDesignerPage"))
    bool bDesignerShowHelp = true;
    /** Dynamic values stay authoritative; designers can change the surrounding copy. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Appearance|Text", meta = (MultiLine = true))
    FText InstallationCostFormat = NSLOCTEXT("FFBuildToolsDesigner", "Cost", "설치 비용  {Cost} Gold");
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Appearance|Text", meta = (MultiLine = true))
    FText FootprintFormat = NSLOCTEXT("FFBuildToolsDesigner", "Footprint", "시설 크기  {Width} × {Depth} m");
    /** Keys are widget names (e.g. Text_Queue); {Value} is the current data text. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Appearance|Text")
    TMap<FName, FText> DynamicTextFormats;
    bool IsPointerOverTools() const;
    bool IsEditingAngle() const;
    bool IsEditingText() const;
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleRadialMenu();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleHelp();
    bool IsRadialMenuOpen() const;
    bool IsAuxiliaryPanelOpen() const;
    bool IsBuildSettingsOpen() const;
    bool IsFeatureGuideOpen() const;
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void OpenFeatureGuide();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") bool OpenFeatureGuideTopic(FName FeatureId);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void CloseFeatureGuide();

    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools")
    void RefreshTools();

    UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Tools")
    FFF_BuildToolsViewData GetToolsViewData() const { return ViewData; }
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ConfirmPlacement();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ReturnToCatalog();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleGrid();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void SelectSnapStep(int32 StepCm);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void RotateToNextSnap(int32 Direction);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void MoveInZoneAxes(FIntPoint Direction);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") bool ApplyYawText(const FText& Text);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleDirectAdjustment();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void FindNearbyPlacement();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ShowNextPlacementProblem();
    /** Legacy keyboard entry point. Toolbar actions are now explicit mouse buttons. */
    bool HandleToolbarKey(const FKey& Key);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleBuildSettings();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Status")
    FSlateColor ReadyStatusColor = FSlateColor(FLinearColor::FromSRGBColor(FColor(0x3D, 0x75, 0xA4)));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Status")
    FSlateColor InvalidStatusColor = FSlateColor(FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43)));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Status")
    FSlateColor NeutralStatusColor = FSlateColor(FLinearColor::FromSRGBColor(FColor(0x7B, 0x7D, 0x92)));

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Building|Tools")
    void BP_OnToolsStateChanged(const FFF_BuildToolsViewData& Data);

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_Tools;
    /** Editable entry motion authored on the build tools Widget Blueprint. */
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> Anim_BuildToolsIntro;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_PlacementInfo;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_PlacementHelp;
    /** Build-session toolbar shared by catalog browsing and active placement. */
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_BuildTopBar;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_BuildBack;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_BuildToolbarGold;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Label_BuildMode;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Size_BuildWorldHelp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Column_BuildIconHelp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_BuildBack;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_BuildBack;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Panel_BuildSettings;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Size_BuildSettings;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_BuildSettings;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_MultiSelect;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_MultiSelect;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SnapCycle;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SnapCycle;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ToolbarRanges;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ToolbarRanges;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ToolbarCatalog;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ToolbarHelp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FeatureGuide;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Panel_FeatureGuide;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_FeatureGuideTopic;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FeatureGuideTitle;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FeatureGuideBody;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FeatureGuideHint;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FeatureGuideConfirm;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FeatureGuideClose;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ToolbarGold;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_PlacementFacility;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_BuildSettingsClose;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsGrid;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsGrid;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsSnapMeter;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsSnapMeter;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsSnapPrecision;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsSnapPrecision;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsSnapFree;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsSnapFree;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsDirectAdjustment;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsDirectAdjustment;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsFacilityRanges;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsFacilityRanges;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsSynergyPreview;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsSynergyPreview;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsZoomToCursor;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsZoomToCursor;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SettingsEdgeScroll;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SettingsEdgeScroll;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_ContextPing;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ContextPing;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementAdvice;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementObstruction;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_AssistanceMessage;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_DirectAdjustment;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FindNearby;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NextPlacementProblem;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_Radial;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_PlacementFacts;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Radial;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Facts;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Help;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Balance;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Footprint;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Queue;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Audience;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementSynergySummary;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementSynergyTitle1;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementSynergyDetail1;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementSynergyTitle2;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementSynergyDetail2;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementSynergyTitle3;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementSynergyDetail3;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ControlHelp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_BuildHelpUndo;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_BuildHelpRedo;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Row_BuildHelpQueue;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_BuildHelpQueue;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_BuildHelpQueueHint;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_BuildHelpPrimary;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_BuildHelpRotate;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_BuildHelpMove;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_BuildHelpPrecision;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Row_BuildHelpRotate;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Row_BuildHelpPrecision;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Details;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_Details;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Mode;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Legend;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FacilityName;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Cost;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_RotationHint;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Grid;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Grid;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SnapMeter;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SnapMeter;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SnapPrecision;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SnapPrecision;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SnapFree;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SnapFree;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_RotateLeft;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_RotateRight;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FreeRotateLeft;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FreeRotateRight;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> Editable_Angle;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> Editable_DragPlacementGap;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_QueueSettings;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> Editable_QueueMaxTeams;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> Editable_QueueSlotSpacing;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> Editable_QueueLaneWidth;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_QueueSettingsSummary;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ApplyQueueSettings;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ResetAllQueues;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeLeft;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeUp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeDown;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeRight;

private:
    TWeakObjectPtr<UFF_BuildingFeatureDiscoverySubsystem> FeatureDiscovery;
    TMap<TWeakObjectPtr<UButton>, FText> AuthoredFeatureButtonTooltips;
    friend class FFFBuildFeatureGuideAcknowledgementTest;
    friend class FFFBuildFeatureGuideInputTest;
    friend class FFFBuildFeatureGuideRenderTest;
    friend class FFFBuildToolsCatalogHelpTest;
    friend class FFFBuildToolsQueueSettingsTest;
    friend class FFFBuildToolsAdaptivePanelsTest;
    FName CurrentGuideFeature;
    FName PendingGuideAction;
    bool bUpdatingGuideTopic = false;
    bool ShowFeatureGuide(FName FeatureId, bool bRunAfterConfirm);
    void ShowFeatureSummary(FName FeatureId);
    void HideFeatureSummary();
    void RunFeatureToolbarAction(FName FeatureId);
    void RequestFeatureToolbarAction(FName FeatureId);
    void RefreshFeatureHints();
    UFUNCTION() void HandleFeatureSeen(FName FeatureId);
    UFUNCTION() void ConfirmFeatureGuide();
    UFUNCTION() void HandleFeatureGuideTopicChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleGuidedMultiSelect();
    UFUNCTION() void HandleGuidedGrid();
    UFUNCTION() void HandleGuidedSnap();
    UFUNCTION() void HandleGuidedRanges();
    UFUNCTION() void HandleGuidedSettings();
    UFUNCTION() void HandleGuidedCatalog();
    UFUNCTION() void HandleGuidedControls();
    FVector2D AuthoredHelpPosition = FVector2D(32.f, 525.f);
    float AuthoredSettingsHeight = 950.f;
    TWeakObjectPtr<UFF_BuildModeSubsystem> BuildMode;
    TWeakObjectPtr<UFF_BuildCatalogEconomyReadModel> Economy;
    TWeakObjectPtr<UFF_BuildCatalogSubsystem> CatalogModel;
    TWeakObjectPtr<UWidget> CatalogHelpAvoidanceSurface;
    TWeakObjectPtr<UWidget> GroupHelpAvoidanceSurface;
    TWeakObjectPtr<UWidget> SynergyHelpAvoidanceSurface;
    double NextSynergyHelpSearchTime = 0.0;
    double NextPlacementSynergyRefresh = 0.0;
    double NextCatalogHelpSearchTime = 0.0;
    double NextGroupHelpSearchTime = 0.0;
    UPROPERTY(Transient) FFF_BuildToolsViewData ViewData;
    FTimerHandle FreeRotationTimer;
    FTimerHandle FeatureSummaryTimer;
    UPROPERTY(Transient) TObjectPtr<USizeBox> FeatureSummarySize;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FeatureSummaryTitle;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FeatureSummaryBody;
    int32 PointerRotationDirection = 0;
    int32 PlacementProblemIndex = 0;
    bool bRestoreHelpAfterSettings = false;
    bool bHelpExpanded = false;
    bool bBuildSessionVisible = false;
    bool bHelpDismissedByUser = false;
    bool bHelpForegroundSuppressed = false;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> ForegroundBodyCanvas;
    bool bBodyForegroundSuppressed = false;
    double LastPointerRotationTime = 0.0;
    UFUNCTION() void ToggleDetails();
    UFUNCTION() void HandleMultiSelect();
    UFUNCTION() void HandleUndoConstruction();
    UFUNCTION() void HandleRedoConstruction();
    void RefreshEditingHelp();
    UFUNCTION() void HandleSnapCycle();
    UFUNCTION() void HandleBuildBack();
    UFUNCTION() void HandleFacilityRanges();
    UFUNCTION() void HandleSynergyPreview();
    UFUNCTION() void HandleZoomToCursor();
    UFUNCTION() void HandleEdgeScroll();
    UFUNCTION() void ToggleFacts();
    void SetHelpExpanded(bool bExpanded);
    void RefreshHelpPresentation();
    void RefreshPlacementSurfaces();
    void RefreshPlacementSynergy();
    TWeakObjectPtr<UUserWidget> FacilityInfoAvoidanceSurface;
    double NextFacilityInfoSearchTime = 0.0;
    void InstallToolbarForegroundLayer();
    void RefreshBodyPresentation();
    void CloseRadialMenu();
    void StartPointerFreeRotation(int32 Direction);
    void AdvancePointerFreeRotation();
    UFUNCTION() void StopPointerFreeRotation();
    UFUNCTION() void HandleFreeRotateLeftPressed();
    UFUNCTION() void HandleFreeRotateRightPressed();
    UFUNCTION() void HandleGoldChanged(int32 Gold);
    void RefreshAngleText();
    void RefreshDragPlacementGapText();
    bool IsEditingDragPlacementGap() const;
    bool IsEditingQueueSettings() const;
    void RefreshQueueSettingsText();
    void RefreshQueueSettingsControls();
    bool bRefreshingQueueSettings = false;
    bool bQueueSettingsDirty = false;
    FName QueueEditingDefinitionId;
    FGuid QueueEditingInstanceId;
    uint8 QueueEditingState = MAX_uint8;
    bool ApplyAngleText(const FText& Text);
    bool ApplyPendingAngle();
    void SetSnap(int32 StepCm);
    void Nudge(FIntPoint Direction);
    void SetChoiceState(UButton* Button, UTextBlock* Label, bool bSelected);

    UFUNCTION() void HandleGrid();
    UFUNCTION() void HandleConfirm();
    UFUNCTION() void HandleSnapMeter();
    UFUNCTION() void HandleSnapPrecision();
    UFUNCTION() void HandleSnapFree();
    UFUNCTION() void HandleRotateLeft();
    UFUNCTION() void HandleRotateRight();
    UFUNCTION() void HandleNudgeLeft();
    UFUNCTION() void HandleNudgeUp();
    UFUNCTION() void HandleNudgeDown();
    UFUNCTION() void HandleNudgeRight();
    UFUNCTION() void HandleAngleCommitted(const FText& Text, ETextCommit::Type CommitMethod);
    UFUNCTION() void HandleDragPlacementGapCommitted(const FText& Text, ETextCommit::Type CommitMethod);
    UFUNCTION() void HandleQueueSettingsChanged(const FText& Text);
    UFUNCTION() void HandleQueueSettingsCommitted(const FText& Text, ETextCommit::Type CommitMethod);
    UFUNCTION() void HandleApplyQueueSettings();
    UFUNCTION() void HandleResetAllQueues();
};
