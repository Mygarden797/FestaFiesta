// FF_FacilityCodexScreen.cpp
// 역할: 도감 목록·상세 정보·발견 보상을 표시하고 사용자 선택을 전달한다.

#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"

#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Building/Codex/FF_FacilityCodexSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/StackBox.h"
#include "Components/StackBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "InputCoreTypes.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresentation.h"
#include "UI/FacilityCodex/FF_FacilityCodexPreview.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodexScreen"

void UFF_FacilityCodexScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (Button_Close) Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClose);
	if (Button_ExitCodex) Button_ExitCodex->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitCodex);
	if (Button_DismissIntroduction) Button_DismissIntroduction->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDismissIntroduction);
	if (Button_All) Button_All->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAll);
	if (Button_Discovered) Button_Discovered->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDiscovered);
	if (Button_Undiscovered) Button_Undiscovered->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUndiscovered);
	if (Button_Active) Button_Active->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleActive);
	if (Button_RewardReady) Button_RewardReady->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRewardReady);
	if (Combo_Category) Combo_Category->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleCategoryChanged);
	if (Button_PreviewTarget) Button_PreviewTarget->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePreviewTarget);
	if (Button_PreviewPartner) Button_PreviewPartner->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePreviewPartner);
	if (Button_ShowcaseCatalog) Button_ShowcaseCatalog->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleShowcaseCatalog);
	if (Button_RotatePreviewLeft) Button_RotatePreviewLeft->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRotateLeft);
	if (Button_RotatePreviewRight) Button_RotatePreviewRight->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRotateRight);
	if (Button_ResetPreview) Button_ResetPreview->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetPreview);
	if (Button_DetailClaimReward) Button_DetailClaimReward->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDetailClaimReward);
	if (Button_ClearFacility) Button_ClearFacility->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClearFacility);
	if (Button_DetailRecipe) Button_DetailRecipe->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDetailRecipe);
	if (Button_DetailPlacement) Button_DetailPlacement->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDetailPlacement);
	if (Button_DetailEffect) Button_DetailEffect->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDetailEffect);
	if (Button_TargetCatalog) Button_TargetCatalog->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTargetCatalog);
	if (Button_PartnerCatalog) Button_PartnerCatalog->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePartnerCatalog);
	SetDetailPage(0);
	ClearRewardFeedback();
	if (Search_Entries) Search_Entries->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleSearchChanged);
	if (TileView_Entries) TileView_Entries->OnItemSelectionChanged().AddUObject(this, &ThisClass::HandleSelectionChanged);
	if (Column_CollectionCategories && TileView_Entries)
		TileView_Entries->OnItemClicked().AddUObject(this, &ThisClass::HandleCollectionClicked);
	if (Button_CollectionPrevious) Button_CollectionPrevious->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCollectionPrevious);
	if (Button_CollectionNext) Button_CollectionNext->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCollectionNext);
	if (Column_CollectionCategories && Search_Entries && !bUseAuthoredCollectionLayout)
	{
		// ContrastBuilder supplies a full-contrast passive label in place of Slate's
		// translucent native hint. Update that label, keeping exactly one hint visible.
		const FText CompactHint = LOCTEXT("CollectionSearchCompact", "시설·조합 검색");
		if (auto* Hint = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SearchHint"))))
		{
			Hint->SetText(CompactHint);
			Hint->SetAutoWrapText(false);
			Hint->SetClipping(EWidgetClipping::ClipToBounds);
			Search_Entries->SetHintText(FText::GetEmpty());
		}
		else Search_Entries->SetHintText(CompactHint);
		if (UWidget* HintOverlay = GetWidgetFromName(TEXT("Overlay_SearchHint")))
			HintOverlay->SetClipping(EWidgetClipping::ClipToBounds);
		// Keep both input and hint inside the category column in portrait windows.
		Search_Entries->SetMinDesiredWidth(0.f);
		Search_Entries->SetToolTipText(LOCTEXT("CollectionSearchHelp", "시설 이름 또는 조합 이름으로 검색합니다."));
		Search_Entries->SetClipping(EWidgetClipping::ClipToBounds);
		if (auto* Bounds = Cast<USizeBox>(GetWidgetFromName(TEXT("Size_Refined_Search_Entries"))))
		{
			Bounds->ClearWidthOverride();
			Bounds->ClearMinDesiredWidth();
			Bounds->ClearMaxDesiredWidth();
		}
	}
	EnsureCloseTargetLayer();
}

void UFF_FacilityCodexScreen::EnsureCloseTargetLayer()
{
	if (CloseTargetCanvas || !WidgetTree || !Button_ExitCodex || !Size_Dialog) return;
	UWidget* ReferenceScale = GetWidgetFromName(TEXT("Scale_ApprovedCodexReference"));
	auto* Shield = Cast<UBorder>(GetWidgetFromName(TEXT("Border_InputShield")));
	auto* OriginalSlot = Cast<UCanvasPanelSlot>(Button_ExitCodex->Slot);
	auto* ScaleSlot = ReferenceScale ? Cast<UBorderSlot>(ReferenceScale->Slot) : nullptr;
	if (!Shield || Shield->GetContent() != ReferenceScale || !OriginalSlot || !ScaleSlot) return;
	// Keep the authored button, its delegates and its styles, but let its hit area
	// stay usable when the six-column reference composition scales down sharply.
	// This layer remains inside the shield, below the separate introduction modal.
	AuthoredClosePosition = OriginalSlot->GetPosition();
	AuthoredCloseSize = OriginalSlot->GetSize();
	const FMargin ContentPadding = ScaleSlot->GetPadding();
	const EHorizontalAlignment Horizontal = ScaleSlot->GetHorizontalAlignment();
	const EVerticalAlignment Vertical = ScaleSlot->GetVerticalAlignment();
	ReferenceScale->RemoveFromParent();
	auto* Layer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_ResponsiveCodexClose"));
	Layer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Shield->SetContent(Layer);
	auto* LayerSlot = CastChecked<UBorderSlot>(Layer->Slot);
	LayerSlot->SetPadding(ContentPadding);
	LayerSlot->SetHorizontalAlignment(Horizontal);
	LayerSlot->SetVerticalAlignment(Vertical);
	auto* ReferenceSlot = Layer->AddChildToOverlay(ReferenceScale);
	ReferenceSlot->SetHorizontalAlignment(HAlign_Fill);
	ReferenceSlot->SetVerticalAlignment(VAlign_Fill);
	CloseTargetCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_ResponsiveCodexClose"));
	CloseTargetCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* TargetSlot = Layer->AddChildToOverlay(CloseTargetCanvas);
	TargetSlot->SetHorizontalAlignment(HAlign_Fill);
	TargetSlot->SetVerticalAlignment(VAlign_Fill);
	Button_ExitCodex->RemoveFromParent();
	CloseTargetCanvas->AddChildToCanvas(Button_ExitCodex)->SetAutoSize(false);
	// The parent canvas must paint once to establish its geometry. Keep the
	// reparented button transparent until its initial position has been resolved.
	AuthoredCloseOpacity = Button_ExitCodex->GetRenderOpacity();
	bCloseTargetLayoutPending = true;
	Button_ExitCodex->SetRenderOpacity(0.f);
}

void UFF_FacilityCodexScreen::UpdateCloseTargetLayout()
{
	if (!CloseTargetCanvas || !Button_ExitCodex || !Size_Dialog) return;
	const FGeometry& Dialog = Size_Dialog->GetPaintSpaceGeometry();
	const FGeometry& Layer = CloseTargetCanvas->GetPaintSpaceGeometry();
	if (Dialog.GetLocalSize().GetMin() <= 1.f || Layer.GetLocalSize().GetMin() <= 1.f) return;
	const FVector2D ReferencePosition(Dialog.GetLocalSize().X + AuthoredClosePosition.X, AuthoredClosePosition.Y);
	const FVector2D OriginalTopLeft = Dialog.LocalToAbsolute(ReferencePosition);
	const FVector2D OriginalBottomRight = Dialog.LocalToAbsolute(ReferencePosition + AuthoredCloseSize);
	const FVector2D OriginalSize = OriginalBottomRight - OriginalTopLeft;
	const FVector2D PixelSize(FMath::Max(32.0, OriginalSize.X), FMath::Max(32.0, OriginalSize.Y));
	FVector2D PixelPosition = OriginalTopLeft + (OriginalSize - PixelSize) * .5;
	// Reserve the authored filter row. In very narrow windows the button can
	// extend slightly above the paper, inside its existing outer screen inset.
	if (UWidget* Filters = GetWidgetFromName(TEXT("Horizontal_Toolbar")))
		if (const auto* FilterSlot = Cast<UCanvasPanelSlot>(Filters->Slot))
			PixelPosition.Y = FMath::Min(PixelPosition.Y,
				Dialog.LocalToAbsolute(FVector2D(0, FilterSlot->GetPosition().Y)).Y - PixelSize.Y - 2.0);
	const FVector2D Position = Layer.AbsoluteToLocal(PixelPosition);
	const FVector2D Size = Layer.AbsoluteToLocal(PixelPosition + PixelSize) - Position;
	if (auto* CloseCanvasSlot = Cast<UCanvasPanelSlot>(Button_ExitCodex->Slot))
	{
		if (!CloseCanvasSlot->GetPosition().Equals(Position, .01f)) CloseCanvasSlot->SetPosition(Position);
		if (!CloseCanvasSlot->GetSize().Equals(Size, .01f)) CloseCanvasSlot->SetSize(Size);
		if (bCloseTargetLayoutPending)
		{
			Button_ExitCodex->SetRenderOpacity(AuthoredCloseOpacity);
			bCloseTargetLayoutPending = false;
		}
	}
}

void UFF_FacilityCodexScreen::NativePreConstruct()
{
	Super::NativePreConstruct();
	// Apply the navigation affordance to existing saved widgets as well as Designer previews.
	if (Button_ExitCodex)
	{
		Button_ExitCodex->SetToolTipText(LOCTEXT("BackHint", "뒤로가기 · Esc"));
		if (auto* Label = Cast<UTextBlock>(Button_ExitCodex->GetContent()))
			Label->SetText(LOCTEXT("BackIcon", "‹"));
	}
#if WITH_EDITORONLY_DATA
	if (IsDesignTime() && Column_CollectionCategories)
	{
		// Preview only the authored widgets: no subsystem, rewards or profile writes.
		SetCollectionDetailOpen(bPreviewCollectionDetail);
		if (Panel_Detail) Panel_Detail->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ClearRewardFeedback();
		if (Text_Empty) Text_Empty->SetVisibility(ESlateVisibility::Collapsed);
		// Designer uses its own visibility flag instead of runtime Visibility.
		// Update only controls whose state is chosen by this page preview.
		for (const TCHAR* Name : {TEXT("Horizontal_Toolbar"), TEXT("Size_CollectionSearch"),
			TEXT("Scroll_CollectionCategories"), TEXT("Size_Browse"), TEXT("Row_CollectionProgress"),
			TEXT("Size_Detail"), TEXT("Size_Showcase"), TEXT("Horizontal_PreviewRotation"),
			TEXT("Button_CollectionPrevious"), TEXT("Button_CollectionNext"), TEXT("Panel_Detail"),
			TEXT("Text_Empty"), TEXT("Border_RewardFeedback"), TEXT("Text_RewardFeedback")})
			if (UWidget* Widget = GetWidgetFromName(Name))
			{
				const ESlateVisibility AuthoredVisibility = Widget->GetVisibility();
				Widget->bHiddenInDesigner = AuthoredVisibility == ESlateVisibility::Collapsed || AuthoredVisibility == ESlateVisibility::Hidden;
			}
		// The repeated controls show configured thresholds and amounts, without
		// reading a player profile or simulating a successful claim.
		const auto& Rewards = GetDefault<UFF_FacilityCodexSettings>()->CollectionRewards;
		if (Row_CollectionMilestones)
			for (int32 Index = 0; Index < Row_CollectionMilestones->GetChildrenCount(); ++Index)
				if (auto* Reward = Cast<UUserWidget>(Row_CollectionMilestones->GetChildAt(Index)))
				{
					if (Rewards.IsValidIndex(Index))
					{
						if (auto* Threshold = Cast<UTextBlock>(Reward->GetWidgetFromName(TEXT("Text_Label"))))
							Threshold->SetText(FText::AsNumber(Rewards[Index].RequiredDiscoveries));
						if (auto* Gold = Cast<UTextBlock>(Reward->GetWidgetFromName(TEXT("Text_MilestoneAmount"))))
							Gold->SetText(FText::AsNumber(Rewards[Index].RewardGold));
					}
					if (auto* Badge = Reward->GetWidgetFromName(TEXT("Border_MilestoneStatus")))
					{ Badge->SetVisibility(ESlateVisibility::Collapsed); Badge->bHiddenInDesigner = true; }
				}
	}
#endif
}

// 도감 화면의 버튼과 변경 알림을 연결한다.
void UFF_FacilityCodexScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsDesignTime()) return;
	if (Viewport_Showcase)
	{
		Viewport_Showcase->OnPreviewReadyChanged.RemoveAll(this);
		Viewport_Showcase->OnPreviewReadyChanged.AddUObject(this, &ThisClass::RefreshPreviewVisibility);
	}
	if (!bPreviewData)
	{
		Codex = GetWorld() ? GetWorld()->GetSubsystem<UFF_FacilityCodexSubsystem>() : nullptr;
		if (Codex)
		{
			Codex->Refresh();
			Codex->OnCodexChanged.AddUniqueDynamic(this, &ThisClass::RefreshFromSubsystem);
		}
		RefreshFromSubsystem();
	}
}

// 도감을 닫을 때 외부 알림과 화면 자원을 정리한다.
void UFF_FacilityCodexScreen::NativeDestruct()
{
	ClearRewardFeedback();
	if (Viewport_Showcase)
	{
		Viewport_Showcase->OnPreviewReadyChanged.RemoveAll(this);
		Viewport_Showcase->ResetPreview();
	}
	if (Codex) Codex->OnCodexChanged.RemoveDynamic(this, &ThisClass::RefreshFromSubsystem);
	Codex = nullptr;
	// Also release presenter-owned input/build leases when another screen removes this widget.
	OnCloseRequested.Broadcast();
	Super::NativeDestruct();
}

void UFF_FacilityCodexScreen::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	UpdateCloseTargetLayout();
	if (IsDesignTime()) { UpdateResponsiveLayout(); return; }
	UpdateRewardFeedback(DeltaTime);
	// Layout-only resize handling. Gameplay data still refreshes exclusively through events.
	UpdateResponsiveLayout();
	// Thumbnail requests can finish after the model request. Keep the fallback
	// hidden once the isolated model viewport is ready without requesting new art.
	if (bShowPreviewModel)
	{
		if (Viewport_Showcase) Viewport_Showcase->RefreshViewForSize();
		RefreshPreviewVisibility();
	}
}

void UFF_FacilityCodexScreen::UpdateResponsiveLayout()
{
	const FVector2D Viewport = GetCachedGeometry().GetLocalSize();
	if (Viewport.X <= 1.f || Viewport.Y <= 1.f) return;
	if (UpdateCollectionLayout(Viewport)) return;
	if (Size_Dialog)
	{
		// Fill the actual logical viewport, including DPI scale. Filtering never shrinks the screen.
		if (!FMath::IsNearlyEqual(Size_Dialog->GetWidthOverride(), Viewport.X)) Size_Dialog->SetWidthOverride(Viewport.X);
		if (!FMath::IsNearlyEqual(Size_Dialog->GetHeightOverride(), Viewport.Y)) Size_Dialog->SetHeightOverride(Viewport.Y);
	}
	// The build-reference WBP uses a translucent lower browse dock and an
	// independent detail card. Keep the input shield full viewport while these
	// canvas slots follow the logical size used by the UMG DPI scaler.
	if (UWidget* Dock = GetWidgetFromName(TEXT("Border_CodexDock")))
	{
		UCanvasPanelSlot* DockSlot = Cast<UCanvasPanelSlot>(Dock->Slot);
		UCanvasPanelSlot* DetailSlot = Size_Detail ? Cast<UCanvasPanelSlot>(Size_Detail->Slot) : nullptr;
		if (!DockSlot || !DetailSlot) return;
		const float Side = FMath::Clamp(Viewport.X * .018f, 16.f, 26.f);
		const float Bottom = FMath::Clamp(Viewport.Y * .018f, 14.f, 22.f);
		const bool bSideBySide = Viewport.X >= 1100.f;
		const float DockHeight = FMath::Clamp(Viewport.Y * .55f, 400.f, 460.f);
		const float DockTop = Viewport.Y - Bottom - DockHeight;
		const float DetailWidth = FMath::Min(FMath::Clamp(Viewport.X * .245f, 340.f, 450.f), Viewport.X - Side * 2.f);
		const float DockWidth = bSideBySide
			? Viewport.X - DetailWidth - Side * 3.f : Viewport.X - Side * 2.f;
		const float DetailHeight = bSideBySide
			? FMath::Min(Viewport.Y - Side * 2.f, 740.f)
			: FMath::Max(220.f, DockTop - Side * 2.f);
		const FVector2D DockPosition(Side, DockTop), DockSize(DockWidth, DockHeight);
		const FVector2D DetailPosition(Viewport.X - Side - DetailWidth, Side), DetailSize(DetailWidth, DetailHeight);
		if (!DockSlot->GetPosition().Equals(DockPosition, .5f)) DockSlot->SetPosition(DockPosition);
		if (!DockSlot->GetSize().Equals(DockSize, .5f)) DockSlot->SetSize(DockSize);
		if (!DetailSlot->GetPosition().Equals(DetailPosition, .5f)) DetailSlot->SetPosition(DetailPosition);
		if (!DetailSlot->GetSize().Equals(DetailSize, .5f)) DetailSlot->SetSize(DetailSize);
		UWidget* CloseContainer = GetWidgetFromName(TEXT("Size_Refined_Button_Close"));
		if (!CloseContainer || !Cast<UCanvasPanelSlot>(CloseContainer->Slot)) CloseContainer = Button_Close;
		if (auto* CloseSlot = CloseContainer ? Cast<UCanvasPanelSlot>(CloseContainer->Slot) : nullptr)
		{
			const FVector2D ClosePosition(Viewport.X - Side - 90.f, Side + 10.f), CloseSize(78.f, 40.f);
			if (!CloseSlot->GetPosition().Equals(ClosePosition, .5f)) CloseSlot->SetPosition(ClosePosition);
			if (!CloseSlot->GetSize().Equals(CloseSize, .5f)) CloseSlot->SetSize(CloseSize);
		}
		if (Size_Showcase)
		{
			// The preview, page tabs, scroll content and reward must all fit the
			// independently floating inspector at the project's 720p DPI scale.
			const float HeroHeight = FMath::Clamp(DetailHeight * .21f, 112.f, 154.f);
			if (!FMath::IsNearlyEqual(Size_Showcase->GetHeightOverride(), HeroHeight, .5f))
				Size_Showcase->SetHeightOverride(HeroHeight);
		}
		if (Size_ShowcaseRewardInfo)
		{
			if (Size_ShowcaseRewardInfo->GetParent() == GetWidgetFromName(TEXT("Vertical_ShowcaseDetail")))
			{
				if (Size_ShowcaseRewardInfo->IsWidthOverride()) Size_ShowcaseRewardInfo->ClearWidthOverride();
			}
			else Size_ShowcaseRewardInfo->SetWidthOverride(FMath::Clamp(DetailWidth * .33f, 130.f, 180.f));
		}
		if (TileView_Entries)
		{
			const float Width = TileView_Entries->GetCachedGeometry().GetLocalSize().X;
			if (Width > 1.f) TileView_Entries->SetResponsiveColumns(FMath::FloorToInt(
				(Width - 16.f) / (MinimumCardWidth + TileView_Entries->GetHorizontalEntrySpacing())));
		}
		return;
	}
	if (!Stack_Main || !Size_Detail || Stack_Main->GetChildrenCount() != 2) return;
	UStackBoxSlot* GridSlot = Cast<UStackBoxSlot>(Stack_Main->GetChildAt(0)->Slot);
	if (!GridSlot) return;
	if (Size_Browse && Size_Showcase)
	{
		if (UWidget* Legend = GetWidgetFromName(TEXT("Text_VisualLegend")))
			Legend->SetVisibility(Viewport.X >= 1400.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Text_CollectionRemaining) Text_CollectionRemaining->SetVisibility(Viewport.X >= 1100.f
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const bool bVertical = bEnableResponsiveDetail && Viewport.X < 1100.f;
		Stack_Main->SetOrientation(bVertical ? Orient_Vertical : Orient_Horizontal);
		GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		GridSlot->SetPadding(bVertical ? FMargin(0, 0, 0, 16) : FMargin(0, 0, 24, 0));
		if (auto* DetailSlot = Cast<UStackBoxSlot>(Size_Detail->Slot))
			DetailSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Size_Detail->ClearWidthOverride(); Size_Detail->ClearHeightOverride();
		const float BrowseWidth = FMath::Clamp(Viewport.X * .37f, 420.f, 720.f);
		if (bVertical)
		{
			Size_Browse->ClearWidthOverride();
			Size_Browse->SetHeightOverride(FMath::Clamp(Viewport.Y * .30f, 160.f, 260.f));
		}
		else
		{
			Size_Browse->SetWidthOverride(BrowseWidth);
			Size_Browse->ClearHeightOverride();
		}
		Size_Showcase->SetHeightOverride(FMath::Clamp((Viewport.Y - 460.f) * .60f, 150.f, 370.f));
		const float DetailWidth = bVertical ? Viewport.X - 64.f : Viewport.X - BrowseWidth - 88.f;
		if (Size_ShowcaseRewardInfo)
			Size_ShowcaseRewardInfo->SetWidthOverride(FMath::Clamp(DetailWidth * .30f, 180.f, 280.f));
		if (TileView_Entries)
		{
			const float Width = TileView_Entries->GetCachedGeometry().GetLocalSize().X;
			if (Width > 1.f) TileView_Entries->SetResponsiveColumns(FMath::FloorToInt(
				(Width - 16.f) / (MinimumCardWidth + TileView_Entries->GetHorizontalEntrySpacing())));
		}
		return;
	}
	const bool bFirstLayout = !bCapturedResponsiveLayout;
	const bool bCompact = bEnableResponsiveDetail && Viewport.X < 1600.f;
	if (bFirstLayout || bCompact != bCompactDetail)
	{
		bCapturedResponsiveLayout = true;
		bCompactDetail = bCompact;
		Stack_Main->SetOrientation(bCompact ? Orient_Vertical : Orient_Horizontal);
		if (bCompact)
		{
			Size_Detail->ClearWidthOverride();
			Size_Detail->SetHeightOverride(CompactDetailHeight);
			GridSlot->SetPadding(FMargin(0, 0, 0, CompactDetailGap));
		}
		else
		{
			Size_Detail->SetWidthOverride(420.f);
			Size_Detail->ClearHeightOverride();
			GridSlot->SetPadding(FMargin(0, 0, 24, 0));
		}
		if (Stack_GlassDetailHeading)
		{
			Stack_GlassDetailHeading->SetOrientation(bCompact ? Orient_Horizontal : Orient_Vertical);
			if (auto* HeadingSlot = Cast<UStackBoxSlot>(Stack_GlassDetailHeading->GetChildAt(0)->Slot))
			{
				HeadingSlot->SetSize(FSlateChildSize(bCompact ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
				HeadingSlot->SetPadding(bCompact ? FMargin(0, 0, 16, 0) : FMargin(0, 0, 0, 18));
			}
		}
		if (Stack_GlassRecipe)
		{
			Stack_GlassRecipe->SetOrientation(bCompact ? Orient_Horizontal : Orient_Vertical);
			if (auto* IngredientSlot = Cast<UStackBoxSlot>(Stack_GlassRecipe->GetChildAt(0)->Slot))
				IngredientSlot->SetPadding(bCompact ? FMargin(0, 0, 24, 0) : FMargin(0, 4, 0, 24));
			if (auto* BonusSlot = Cast<UStackBoxSlot>(Stack_GlassRecipe->GetChildAt(1)->Slot))
				BonusSlot->SetSize(FSlateChildSize(bCompact ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		}
	}
	if (TileView_Entries)
	{
		const float Width = TileView_Entries->GetCachedGeometry().GetLocalSize().X;
		if (Width > 1.f)
		{
			const float Spacing = TileView_Entries->GetHorizontalEntrySpacing();
			TileView_Entries->SetResponsiveColumns(FMath::FloorToInt((Width - 16.f) / (MinimumCardWidth + Spacing)));
		}
	}
}

FReply UFF_FacilityCodexScreen::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (IsIntroductionVisible())
	{
		if (!KeyEvent.IsRepeat() && (KeyEvent.GetKey() == EKeys::Escape ||
			KeyEvent.GetKey() == EKeys::Enter || KeyEvent.GetKey() == EKeys::SpaceBar))
		{
			HandleDismissIntroduction();
		}
		// Keep keyboard navigation on the guide until it has been dismissed.
		return FReply::Handled();
	}
	if (Column_CollectionCategories && !KeyEvent.IsRepeat())
	{
		if (bCollectionDetailOpen && (KeyEvent.GetKey() == EKeys::Left || KeyEvent.GetKey() == EKeys::Right))
		{
			NavigateCollectionDetail(KeyEvent.GetKey() == EKeys::Right ? 1 : -1);
			return FReply::Handled();
		}
		if (!bCollectionDetailOpen && TileView_Entries && TileView_Entries->HasKeyboardFocus() &&
			(KeyEvent.GetKey() == EKeys::Enter || KeyEvent.GetKey() == EKeys::SpaceBar))
		{
			OpenCombinationDetail(SelectedRuleId);
			return FReply::Handled();
		}
	}
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		HandleClose();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

FReply UFF_FacilityCodexScreen::NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&)
{
	return FReply::Handled();
}

void UFF_FacilityCodexScreen::SetFacilityFilter(FName FacilityId)
{
	Filter.FacilityDefinitionId = FacilityId;
	RefreshFilter();
}

void UFF_FacilityCodexScreen::SetDiscoveryFilter(EFF_FacilityCodexFilterState State)
{
	if (Filter.State == State) return;
	Filter.State = State;
	RefreshFilter();
}

void UFF_FacilityCodexScreen::SetSearchText(const FText& SearchText)
{
	Filter.SearchText = SearchText.ToString();
	if (Search_Entries && !Search_Entries->GetText().EqualTo(SearchText)) Search_Entries->SetText(SearchText);
	// UEditableTextBox::SetText does not broadcast user edit events.
	RefreshFilter();
}

void UFF_FacilityCodexScreen::SetCategoryFilter(bool bEnabled, EFFZSFacilityCategory Category)
{
	Filter.bFilterByCategory = bEnabled;
	Filter.Category = Category;
	if (Combo_Category)
	{
		TGuardValue<bool> Guard(bUpdatingCategoryChoices, true);
		Combo_Category->SetSelectedIndex(bEnabled ? CategoryChoices.IndexOfByKey(Category) + 1 : 0);
	}
	RefreshFilter();
}

void UFF_FacilityCodexScreen::RefreshFromSubsystem()
{
	if (bPreviewData) return;
	if (Codex) ApplyEntries(Codex->GetEntriesView());
	else ApplyEntries({});
}

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
void UFF_FacilityCodexScreen::SetPreviewEntries(const TArray<FFF_FacilityCodexEntry>& Entries)
{
	ClearRewardFeedback();
	bPreviewData = true;
	if (Codex) Codex->OnCodexChanged.RemoveDynamic(this, &ThisClass::RefreshFromSubsystem);
	Codex = nullptr;
	ApplyEntries(Entries);
}
#endif

void UFF_FacilityCodexScreen::ApplyEntries(const TArray<FFF_FacilityCodexEntry>& Entries)
{
	TSet<FName> CurrentIds;
	AllItems.Reset(Entries.Num());
	for (const FFF_FacilityCodexEntry& Entry : Entries)
	{
		if (Entry.RuleId.IsNone() || CurrentIds.Contains(Entry.RuleId)) continue;
		CurrentIds.Add(Entry.RuleId);
		TObjectPtr<UFF_FacilityCodexItem>& Item = ItemsByRule.FindOrAdd(Entry.RuleId);
		if (!Item) Item = NewObject<UFF_FacilityCodexItem>(this);
		Item->OnClaimRewardRequested.BindUObject(this, &ThisClass::HandleClaimReward);
		Item->Entry = Entry;
		AllItems.Add(Item);
	}
	for (auto It = ItemsByRule.CreateIterator(); It; ++It)
	{
		if (!CurrentIds.Contains(It.Key())) It.RemoveCurrent();
	}
	Summary = FFF_FacilityCodexModel::Summarize(Entries);
	RefreshCategoryChoices();
	RefreshFilter();
}

void UFF_FacilityCodexScreen::RefreshFilter()
{
	if (bApplyingFilter) return;
	TGuardValue<bool> Guard(bApplyingFilter, true);
	if (UWidget* Hint = GetWidgetFromName(TEXT("Text_SearchHint")))
		Hint->SetVisibility(Search_Entries && Search_Entries->GetText().IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	TArray<TObjectPtr<UFF_FacilityCodexItem>> NewVisible;
	for (UFF_FacilityCodexItem* Item : AllItems)
	{
		if (Item && FFF_FacilityCodexModel::MatchesFilter(Item->Entry, Filter)) NewVisible.Add(Item);
	}
	const bool bMembershipChanged = NewVisible != VisibleItems;
	VisibleItems = MoveTemp(NewVisible);
	UFF_FacilityCodexItem* Selected = ItemsByRule.FindRef(SelectedRuleId);
	if (!VisibleItems.Contains(Selected)) Selected = VisibleItems.IsEmpty() ? nullptr : VisibleItems[0].Get();
	SelectedRuleId = Selected ? Selected->Entry.RuleId : NAME_None;
	if (TileView_Entries)
	{
		if (bMembershipChanged)
		{
			TArray<UObject*> Objects;
			Objects.Reserve(VisibleItems.Num());
			for (UFF_FacilityCodexItem* Item : VisibleItems) Objects.Add(Item);
			TileView_Entries->SetListItems(Objects);
			TileView_Entries->ScrollToTop();
		}
		TileView_Entries->SetSelectedItem(Selected);
		for (UUserWidget* Widget : TileView_Entries->GetDisplayedEntryWidgets())
		{
			if (auto* Card = Cast<UFF_FacilityCodexCard>(Widget)) Card->RefreshPresentation();
		}
	}
	SetTabSelected(Button_All, Filter.State == EFF_FacilityCodexFilterState::All);
	SetTabSelected(Button_Discovered, Filter.State == EFF_FacilityCodexFilterState::Discovered);
	SetTabSelected(Button_Undiscovered, Filter.State == EFF_FacilityCodexFilterState::Undiscovered);
	SetTabSelected(Button_Active, Filter.State == EFF_FacilityCodexFilterState::Active);
	SetTabSelected(Button_RewardReady, Filter.State == EFF_FacilityCodexFilterState::RewardReady);
	int32 ReadyRewards = 0;
	for (const UFF_FacilityCodexItem* Item : AllItems)
		if (Item && Item->Entry.bDiscovered && !Item->Entry.bDiscoveryRewardClaimed && Item->Entry.DiscoveryRewardGold > 0) ++ReadyRewards;
	if (Text_CollectionRemaining) Text_CollectionRemaining->SetText(FText::Format(
		LOCTEXT("CollectionRemaining", "미발견 {0}  ·  수령 가능 {1}"),
		FText::AsNumber(Summary.TotalCount - Summary.DiscoveredCount), FText::AsNumber(ReadyRewards)));
	if (Button_RewardReady)
	{
		Button_RewardReady->SetToolTipText(FText::Format(
			LOCTEXT("ReadyRewardsTip", "아직 수령하지 않은 발견 보상 {0}개"), FText::AsNumber(ReadyRewards)));
		if (auto* Label = Cast<UTextBlock>(Button_RewardReady->GetContent()))
			Label->SetText(FText::Format(LOCTEXT("ReadyRewardsCount", "수령 가능 {0}"), FText::AsNumber(ReadyRewards)));
	}
	if (Button_ClearFacility) Button_ClearFacility->SetVisibility(Filter.FacilityDefinitionId.IsNone()
		? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (Text_Summary) Text_Summary->SetText(FText::Format(LOCTEXT("Summary", "발견 {0} / {1}    ·    효과 적용 중 {2}"),
		FText::AsNumber(Summary.DiscoveredCount), FText::AsNumber(Summary.TotalCount), FText::AsNumber(Summary.EffectAppliedCount)));
	if (Progress_Discovery)
	{
		// The collection's progress does not change when browsing a filtered subset.
		Progress_Discovery->SetPercent(Summary.TotalCount > 0
			? FMath::Clamp(float(Summary.DiscoveredCount) / float(Summary.TotalCount), 0.f, 1.f) : 0.f);
		Progress_Discovery->SetToolTipText(FText::Format(LOCTEXT("CollectionProgress", "전체 조합 발견 {0} / {1}"),
			FText::AsNumber(Summary.DiscoveredCount), FText::AsNumber(Summary.TotalCount)));
	}
	FText FacilityName = LOCTEXT("AllFacilities", "모든 시설");
	if (!Filter.FacilityDefinitionId.IsNone())
	{
		FacilityName = LOCTEXT("SelectedFacility", "선택한 시설");
		for (const UFF_FacilityCodexItem* Item : AllItems)
		{
			if (Item->Entry.TargetFacilityId == Filter.FacilityDefinitionId) { FacilityName = Item->Entry.TargetName; break; }
			if (Item->Entry.PartnerFacilityId == Filter.FacilityDefinitionId) { FacilityName = Item->Entry.PartnerName; break; }
		}
	}
	if (Text_Filter) Text_Filter->SetText(FText::Format(LOCTEXT("Filter", "{0} · {1}개 조합"), FacilityName, FText::AsNumber(VisibleItems.Num())));
	if (Text_Empty)
	{
		Text_Empty->SetVisibility(VisibleItems.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Text_Empty->SetText(AllItems.IsEmpty() ? LOCTEXT("NoRules", "등록된 시설 조합이 없습니다.")
			: LOCTEXT("NoMatches", "조건에 맞는 조합이 없습니다.\n검색어나 필터를 바꿔 보세요."));
	}
	// A claim can remove the last result from the reward-ready filter while its
	// detail page is open. Keep the empty state and filter controls reachable.
	if (Column_CollectionCategories && bCollectionDetailOpen && VisibleItems.IsEmpty())
		SetCollectionDetailOpen(false);
	RefreshDetail();
	RefreshCollectionNavigation();
	RefreshCollectionMilestones();
}

void UFF_FacilityCodexScreen::HandleSelectionChanged(UObject* SelectedItem)
{
	if (bApplyingFilter) return;
	const auto* Item = Cast<UFF_FacilityCodexItem>(SelectedItem);
	SelectedRuleId = Item ? Item->Entry.RuleId : NAME_None;
	RefreshDetail();
}

void UFF_FacilityCodexScreen::RefreshDetail()
{
	using namespace FFFacilityCodexPresentation;
	const UFF_FacilityCodexItem* Item = ItemsByRule.FindRef(SelectedRuleId);
	if (ShownPreviewRuleId != SelectedRuleId)
	{
		ShownPreviewRuleId = SelectedRuleId;
		bPreviewPartner = false;
	}
	if (!Column_CollectionCategories || bCollectionDetailOpen) RefreshShowcase();
	if (Panel_Detail) Panel_Detail->SetVisibility(Item && (!Column_CollectionCategories || bCollectionDetailOpen)
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	if (Button_TargetCatalog) Button_TargetCatalog->SetIsEnabled(Item && !Item->Entry.TargetFacilityId.IsNone());
	if (Button_PartnerCatalog) Button_PartnerCatalog->SetIsEnabled(Item && !Item->Entry.PartnerFacilityId.IsNone());
	if (!Item)
	{
		if (Image_Target) Image_Target->SetDiscoveryVisualState(EFF_FacilityCodexDiscoveryVisualState::Color);
		if (Image_Partner) Image_Partner->SetDiscoveryVisualState(EFF_FacilityCodexDiscoveryVisualState::Color);
		Thumbnail(Image_Target, Text_TargetFallback, {}, ShownTarget);
		Thumbnail(Image_Partner, Text_PartnerFallback, {}, ShownPartner);
		return;
	}
	const FFF_FacilityCodexEntry& Entry = Item->Entry;
	if (Image_Target) Image_Target->SetDiscoveryVisualState(Entry.bDiscovered ? EFF_FacilityCodexDiscoveryVisualState::Color
		: Entry.bTargetInstalled ? EFF_FacilityCodexDiscoveryVisualState::Grayscale : EFF_FacilityCodexDiscoveryVisualState::Silhouette);
	if (Image_Partner) Image_Partner->SetDiscoveryVisualState(Entry.bDiscovered ? EFF_FacilityCodexDiscoveryVisualState::Color
		: Entry.bPartnerInstalled ? EFF_FacilityCodexDiscoveryVisualState::Grayscale : EFF_FacilityCodexDiscoveryVisualState::Silhouette);
	if (Text_DetailTitle) Text_DetailTitle->SetText(Entry.DisplayName);
	if (Text_TargetName) Text_TargetName->SetText(Entry.TargetName);
	if (Text_PartnerName) Text_PartnerName->SetText(Entry.PartnerName);
	if (Text_Bonus) Text_Bonus->SetText(FText::Format(LOCTEXT("DetailBonus", "{0} 부스 매출 +{1}%"), Entry.TargetName, Number(Entry.ConfiguredBonusPercent)));
	if (Text_Distance) Text_Distance->SetText(FText::Format(LOCTEXT("Distance", "시설 중심 사이 {0}m 이내에 배치"), Number(Entry.MaxDistanceMeters)));
	if (Text_Discovery)
	{
		// The dedicated reward card owns amount, eligibility, and action. Keep this line
		// focused on discovery; legacy WBPs without that card retain their full explanation.
		Text_Discovery->SetText(Text_DetailRewardAmount
			? (Entry.bDiscovered ? LOCTEXT("FoundStatus", "조합 발견 완료") : LOCTEXT("NotFoundStatus", "미발견 조합"))
			: FText::Format(LOCTEXT("LegacyDiscoveryReward", "{0} · 최초 발견 보상 {1} 골드 (1회)"),
				Entry.bDiscovered ? LOCTEXT("LegacyFound", "발견 완료") : LOCTEXT("LegacyNotFound", "미발견"),
				FText::AsNumber(Entry.DiscoveryRewardGold)));
	}
	FText Condition;
	if (Entry.bConditionMet)
	{
		Condition = FText::Format(LOCTEXT("Met", "현재 조건 충족: {0}개 시설"), FText::AsNumber(Entry.ConditionMetCount));
	}
	else if (!Entry.bTargetInstalled || !Entry.bPartnerInstalled)
	{
		Condition = LOCTEXT("Install", "현재 조건 미충족: 필요한 시설을 설치해 주세요");
	}
	else if (Entry.ClosestDistanceMeters >= 0.f)
	{
		Condition = FText::Format(LOCTEXT("Far", "현재 가장 가까운 거리 {0}m · 더 가까이 배치해 주세요"), Number(Entry.ClosestDistanceMeters));
	}
	else if (Entry.bHasInstalledPair) Condition = LOCTEXT("Placement", "두 시설 설치 완료 · 현재 배치 상태를 확인해 주세요");
	else Condition = LOCTEXT("Pair", "현재 조건 미충족: 서로 다른 두 시설이 필요해요");
	if (Text_Condition) Text_Condition->SetText(FText::Format(
		LOCTEXT("ConditionChecklistLabels", "{0} · {1} 설치\n{2} · {3} 설치\n{4}"),
		Entry.bTargetInstalled ? LOCTEXT("InstallationComplete", "완료") : LOCTEXT("InstallationNeeded", "필요"), Entry.TargetName,
		Entry.bPartnerInstalled ? LOCTEXT("InstallationComplete", "완료") : LOCTEXT("InstallationNeeded", "필요"), Entry.PartnerName,
		Condition));
	if (Text_Applied) Text_Applied->SetText(Entry.bEffectApplied
		? FText::Format(LOCTEXT("EffectApplied", "현재 {0}개 시설에 적용 중 · 최대 +{1}%\n같은 그룹의 더 높은 효과와 수익 상한에 따라 실제 보너스가 달라집니다."),
			FText::AsNumber(Entry.EffectAppliedCount), Number(Entry.AppliedBonusPercent))
		: Entry.bConditionMet ? LOCTEXT("EffectCapped", "배치 조건은 충족했어요. 더 높은 같은 그룹 효과 또는 수익 상한으로 현재 추가 적용은 없어요.")
		: LOCTEXT("NoEffect", "현재 적용 효과 없음 · 배치 조건을 충족하면 자동으로 적용됩니다."));
	Thumbnail(Image_Target, Text_TargetFallback, Entry.TargetThumbnail, ShownTarget);
	Thumbnail(Image_Partner, Text_PartnerFallback, Entry.PartnerThumbnail, ShownPartner);
}

void UFF_FacilityCodexScreen::RefreshCategoryChoices()
{
	if (!Combo_Category) return;
	const auto Order = FFF_PrepBuildPresentationRules::GetCategoryOrder();
	if (CategoryChoices.Num() == Order.Num() && Combo_Category->GetOptionCount() == Order.Num() + 1) return;
	TGuardValue<bool> Guard(bUpdatingCategoryChoices, true);
	CategoryChoices.Reset();
	Combo_Category->ClearOptions();
	Combo_Category->AddOption(LOCTEXT("AllCategoryChoice", "전체 시설").ToString());
	for (EFFZSFacilityCategory Category : Order)
	{
		CategoryChoices.Add(Category);
		Combo_Category->AddOption(FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(Category).ToString());
	}
	Combo_Category->SetToolTipText(LOCTEXT("CategoryMeaning", "조합 효과를 받는 시설의 분류"));
	Combo_Category->SetSelectedIndex(Filter.bFilterByCategory ? CategoryChoices.IndexOfByKey(Filter.Category) + 1 : 0);
}

void UFF_FacilityCodexScreen::RefreshShowcase()
{
	using namespace FFFacilityCodexPresentation;
	const UFF_FacilityCodexItem* Item = ItemsByRule.FindRef(SelectedRuleId);
	const FFF_FacilityCodexEntry* Entry = Item ? &Item->Entry : nullptr;
	if (Button_ShowcaseCatalog) Button_ShowcaseCatalog->SetIsEnabled(Entry &&
		!(bPreviewPartner ? Entry->PartnerFacilityId : Entry->TargetFacilityId).IsNone());
	const bool bClaimed = Entry && Entry->bDiscovered && Entry->bDiscoveryRewardClaimed;
	const bool bReady = Entry && Entry->bDiscovered && !bClaimed && Entry->DiscoveryRewardGold > 0;
	const bool bLocked = Entry && !Entry->bDiscovered && Entry->DiscoveryRewardGold > 0;
	if (Text_DetailRewardLabel) Text_DetailRewardLabel->SetText(Entry
		? LOCTEXT("DiscoveryRewardHeading", "발견 보상") : FText::GetEmpty());
	const FText RewardGold = Entry
		? FText::Format(LOCTEXT("DiscoveryRewardRowAmount", "{0} 골드"), FText::AsNumber(Entry->DiscoveryRewardGold))
		: FText::GetEmpty();
	if (Text_DetailClaimReward) Text_DetailClaimReward->SetText(RewardGold);
	if (Text_DetailRewardClaimed) Text_DetailRewardClaimed->SetText(!Entry ? FText::GetEmpty()
		: Entry->DiscoveryRewardGold > 0
			? FText::Format(LOCTEXT("DiscoveryRewardRowClaimed", "{0} · 수령 완료"), RewardGold)
			: LOCTEXT("DiscoveryRewardRowReceipt", "수령 완료"));
	if (Text_DetailRewardLocked) Text_DetailRewardLocked->SetText(Entry
		? FText::Format(LOCTEXT("DiscoveryRewardRowLocked", "{0} · 미발견"), RewardGold) : FText::GetEmpty());
	if (Text_DetailRewardHint) Text_DetailRewardHint->SetText(bReady
		? LOCTEXT("DetailRewardRowHint", "골드 보상 버튼을 눌러 수령하세요") : bClaimed
			? LOCTEXT("DetailRewardClaimedHint", "이미 수령했어요") : bLocked
				? LOCTEXT("DetailRewardLockedHint", "조합을 발견하면 열려요") : FText::GetEmpty());
	if (Size_DetailRewardState) Size_DetailRewardState->SetVisibility(bReady || bClaimed || bLocked
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (Button_DetailClaimReward)
	{
		Button_DetailClaimReward->SetVisibility(bReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_DetailClaimReward->SetIsEnabled(bReady);
		Button_DetailClaimReward->SetToolTipText(bReady ? FText::Format(
			LOCTEXT("DetailRewardClaimTip", "발견 보상 {0} 골드 받기 (1회)"), FText::AsNumber(Entry->DiscoveryRewardGold)) : FText::GetEmpty());
	}
	if (Border_DetailRewardClaimed)
	{
		Border_DetailRewardClaimed->SetVisibility(bClaimed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Border_DetailRewardClaimed->SetToolTipText(bClaimed ? LOCTEXT("DetailRewardClaimedTip", "발견 보상 수령 완료") : FText::GetEmpty());
	}
	if (Border_DetailRewardLocked)
	{
		Border_DetailRewardLocked->SetVisibility(bLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Border_DetailRewardLocked->SetToolTipText(bLocked ? FText::Format(
			LOCTEXT("DetailRewardLockedTip", "조합을 발견하면 {0} 골드 수령 가능 (1회)"), FText::AsNumber(Entry->DiscoveryRewardGold)) : FText::GetEmpty());
	}
	if (Text_DetailRewardAmount) Text_DetailRewardAmount->SetText(!Entry ? FText::GetEmpty()
		: Entry->DiscoveryRewardGold > 0
			? FText::Format(LOCTEXT("DetailRewardAmount", "{0} 골드"), FText::AsNumber(Entry->DiscoveryRewardGold))
			: bClaimed ? LOCTEXT("DetailRewardCollected", "수령 완료")
				: LOCTEXT("DetailRewardNone", "보상 없음"));
	if (Image_DetailRewardGold) Image_DetailRewardGold->SetVisibility(Entry && Entry->DiscoveryRewardGold > 0
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Text_ShowcaseDistance) Text_ShowcaseDistance->SetText(Entry
		? FText::Format(LOCTEXT("ShowcaseDistance", "함께 배치 · {0}m 이내"), Number(Entry->MaxDistanceMeters)) : FText::GetEmpty());
	if (Text_DetailStatus) Text_DetailStatus->SetText(!Entry ? FText::GetEmpty() : Entry->bEffectApplied
		? LOCTEXT("ShowcaseActive", "효과 적용 중") : Entry->bDiscovered
			? LOCTEXT("ShowcaseDiscovered", "발견 완료") : LOCTEXT("ShowcaseUndiscovered", "아직 발견하지 못한 조합"));
	bShowPreviewModel = false;
	if (!Entry)
	{
		if (Viewport_Showcase) Viewport_Showcase->ResetPreview();
		Thumbnail(Image_Showcase, Text_ShowcaseFallback, {}, ShownShowcase);
		for (UTextBlock* Label : {Text_PreviewFacilityName.Get(), Text_PreviewCategory.Get(), Text_PreviewHint.Get()})
			if (Label) Label->SetText(FText::GetEmpty());
		RefreshPreviewVisibility();
		return;
	}
	const FName FacilityId = bPreviewPartner ? Entry->PartnerFacilityId : Entry->TargetFacilityId;
	const FText FacilityName = bPreviewPartner ? Entry->PartnerName : Entry->TargetName;
	const bool bInstalled = bPreviewPartner ? Entry->bPartnerInstalled : Entry->bTargetInstalled;
	const auto VisualState = Entry->bDiscovered ? EFF_FacilityCodexDiscoveryVisualState::Color : bInstalled
		? EFF_FacilityCodexDiscoveryVisualState::Grayscale : EFF_FacilityCodexDiscoveryVisualState::Silhouette;
	if (Image_Showcase) Image_Showcase->SetDiscoveryVisualState(VisualState);
	Thumbnail(Image_Showcase, Text_ShowcaseFallback,
		bPreviewPartner ? Entry->PartnerThumbnail : Entry->TargetThumbnail, ShownShowcase);
	if (Text_PreviewFacilityName) Text_PreviewFacilityName->SetText(FacilityName);
	SetTabSelected(Button_PreviewTarget, !bPreviewPartner);
	SetTabSelected(Button_PreviewPartner, bPreviewPartner);
	const FPrimaryAssetId PrimaryId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), FacilityId);
	const auto DefinitionPath = UAssetManager::Get().GetPrimaryAssetPath(PrimaryId);
	const auto* Definition = FacilityId.IsNone() || DefinitionPath.IsNull() ? nullptr
		: Cast<UFFZSFacilityDefinition>(DefinitionPath.TryLoad());
	if (Text_PreviewCategory) Text_PreviewCategory->SetText(Definition
		? FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(Definition->Spec.Category)
		: bPreviewPartner ? LOCTEXT("PartnerFacility", "함께 놓는 시설")
			: FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(Entry->TargetCategory));
	if (Viewport_Showcase)
	{
		// The collection detail uses the authored transparent facility cutout, matching
		// its stamp card and keeping the sea backdrop continuous behind the visual.
		bShowPreviewModel = !Column_CollectionCategories && Entry->bDiscovered && Definition && !Definition->Spec.VisualMesh.IsNull();
		if (bShowPreviewModel)
		{
			// Give the preview a Slate viewport even while it is loading. Hidden (not
			// collapsed) preserves its geometry and the isolated scene's lifecycle.
			Viewport_Showcase->SetVisibility(ESlateVisibility::Hidden);
			Viewport_Showcase->ShowMesh(Definition->Spec.VisualMesh);
		}
		else Viewport_Showcase->ResetPreview();
	}
	if (Text_PreviewHint) Text_PreviewHint->SetText(Entry->bDiscovered
		? LOCTEXT("ShowcaseKnownHint", "시설 이름을 누르면 건축 카탈로그로 이동합니다")
		: bInstalled ? LOCTEXT("ShowcaseInstalledHint", "설치한 시설 · 함께 놓을 시설을 찾아 조합을 발견하세요")
			: LOCTEXT("ShowcaseUnknownHint", "미발견 시설 · 조합을 발견하면 컬러로 확인할 수 있어요"));
	RefreshPreviewVisibility();
}

void UFF_FacilityCodexScreen::RefreshPreviewVisibility()
{
	const bool bModelVisible = bShowPreviewModel && Viewport_Showcase && Viewport_Showcase->IsPreviewReady();
	if (Viewport_Showcase) Viewport_Showcase->SetVisibility(bModelVisible
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (Image_Showcase) Image_Showcase->SetVisibility(bModelVisible ? ESlateVisibility::Hidden
		: Image_Showcase->GetLoadedThumbnail() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (Text_ShowcaseFallback) Text_ShowcaseFallback->SetVisibility(bModelVisible || (Image_Showcase && Image_Showcase->GetLoadedThumbnail())
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	for (UButton* Button : {Button_RotatePreviewLeft.Get(), Button_RotatePreviewRight.Get(), Button_ResetPreview.Get()})
		if (Button) Button->SetIsEnabled(bModelVisible);
}

bool UFF_FacilityCodexScreen::ShowIntroduction()
{
	if (!Border_Introduction || !Button_DismissIntroduction) return false;
	Border_Introduction->SetVisibility(ESlateVisibility::Visible);
	Button_DismissIntroduction->SetKeyboardFocus();
	return true;
}

bool UFF_FacilityCodexScreen::IsIntroductionVisible() const
{
	return Border_Introduction && Border_Introduction->IsVisible();
}

void UFF_FacilityCodexScreen::HandleDismissIntroduction()
{
	if (Border_Introduction) Border_Introduction->SetVisibility(ESlateVisibility::Collapsed);
	SetKeyboardFocus();
}

void UFF_FacilityCodexScreen::HandleClose()
{
	if (IsIntroductionVisible())
	{
		HandleDismissIntroduction();
		return;
	}
	if (Column_CollectionCategories && bCollectionDetailOpen)
	{
		SetCollectionDetailOpen(false);
		if (TileView_Entries) TileView_Entries->SetKeyboardFocus();
		return;
	}
	OnCloseRequested.Broadcast();
}
void UFF_FacilityCodexScreen::HandleExitCodex()
{
	// Keep the saved widget binding while sharing the same back policy as Escape and the title button.
	HandleClose();
}
void UFF_FacilityCodexScreen::HandleAll() { SetDiscoveryFilter(EFF_FacilityCodexFilterState::All); }
void UFF_FacilityCodexScreen::HandleDiscovered() { SetDiscoveryFilter(EFF_FacilityCodexFilterState::Discovered); }
void UFF_FacilityCodexScreen::HandleUndiscovered() { SetDiscoveryFilter(EFF_FacilityCodexFilterState::Undiscovered); }
void UFF_FacilityCodexScreen::HandleActive() { SetDiscoveryFilter(EFF_FacilityCodexFilterState::Active); }
void UFF_FacilityCodexScreen::HandleRewardReady() { SetDiscoveryFilter(EFF_FacilityCodexFilterState::RewardReady); }
void UFF_FacilityCodexScreen::HandleCategoryChanged(FString Selected, ESelectInfo::Type)
{
	if (bUpdatingCategoryChoices || !Combo_Category) return;
	const int32 Index = Combo_Category->FindOptionIndex(Selected) - 1;
	SetCategoryFilter(CategoryChoices.IsValidIndex(Index), CategoryChoices.IsValidIndex(Index)
		? CategoryChoices[Index] : EFFZSFacilityCategory::Booth);
}
void UFF_FacilityCodexScreen::HandlePreviewTarget() { bPreviewPartner = false; RefreshShowcase(); }
void UFF_FacilityCodexScreen::HandlePreviewPartner() { bPreviewPartner = true; RefreshShowcase(); }
void UFF_FacilityCodexScreen::HandleShowcaseCatalog()
{
	if (bPreviewPartner) HandlePartnerCatalog(); else HandleTargetCatalog();
}
void UFF_FacilityCodexScreen::HandleRotateLeft() { if (Viewport_Showcase) Viewport_Showcase->RotatePreview(-30.f); }
void UFF_FacilityCodexScreen::HandleRotateRight() { if (Viewport_Showcase) Viewport_Showcase->RotatePreview(30.f); }
void UFF_FacilityCodexScreen::HandleResetPreview() { if (Viewport_Showcase) Viewport_Showcase->ResetView(); }
void UFF_FacilityCodexScreen::HandleDetailClaimReward()
{
	const UFF_FacilityCodexItem* Item = ItemsByRule.FindRef(SelectedRuleId);
	if (Item && Item->Entry.bDiscovered && !Item->Entry.bDiscoveryRewardClaimed && Item->Entry.DiscoveryRewardGold > 0)
		HandleClaimReward(Item->Entry.RuleId);
}
void UFF_FacilityCodexScreen::HandleClearFacility() { SetFacilityFilter(NAME_None); }
void UFF_FacilityCodexScreen::HandleDetailRecipe() { SetDetailPage(0); }
void UFF_FacilityCodexScreen::HandleDetailPlacement() { SetDetailPage(1); }
void UFF_FacilityCodexScreen::HandleDetailEffect() { SetDetailPage(2); }
void UFF_FacilityCodexScreen::HandleTargetCatalog()
{
	const UFF_FacilityCodexItem* Item = ItemsByRule.FindRef(SelectedRuleId);
	if (Item && !Item->Entry.TargetFacilityId.IsNone()) OnFacilityCatalogRequested.Broadcast(Item->Entry.TargetFacilityId);
}
void UFF_FacilityCodexScreen::HandlePartnerCatalog()
{
	const UFF_FacilityCodexItem* Item = ItemsByRule.FindRef(SelectedRuleId);
	if (Item && !Item->Entry.PartnerFacilityId.IsNone()) OnFacilityCatalogRequested.Broadcast(Item->Entry.PartnerFacilityId);
}
void UFF_FacilityCodexScreen::HandleClaimReward(FName RuleId)
{
	// Preview data can demonstrate every state without ever modifying the player's profile or wallet.
	if (bPreviewData || !Codex) return;
	FFF_FacilityCodexEntry Entry;
	if (!Codex->FindEntry(RuleId, Entry) || !Entry.bDiscovered || Entry.bDiscoveryRewardClaimed || Entry.DiscoveryRewardGold <= 0) return;
	UGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const auto* Economy = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (!Economy) return;
	// Match the authoritative Funds.Refund saturation before wallet listeners run.
	// A listener may spend gold or claim another recipe, so a later balance delta
	// does not reliably describe this particular reward.
	const int32 PaidGold = static_cast<int32>(FMath::Min<int64>(Entry.DiscoveryRewardGold,
		FMath::Max<int64>(0, int64(MAX_int32) - Economy->GetGold())));
	UFF_FacilityCodexSubsystem* ClaimCodex = Codex;
	if (ClaimCodex->ClaimDiscoveryReward(RuleId) && Codex == ClaimCodex && !bPreviewData)
		ShowRewardFeedback(PaidGold);
}

void UFF_FacilityCodexScreen::ShowRewardFeedback(int32 PaidGold)
{
	if (!Text_RewardFeedback) return;
	Text_RewardFeedback->SetText(FText::Format(LOCTEXT("RewardCollectedFeedback", "＋{0} 골드 · 보상 수령 완료"),
		FText::AsNumber(PaidGold)));
	Text_RewardFeedback->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Border_RewardFeedback) Border_RewardFeedback->SetVisibility(ESlateVisibility::HitTestInvisible);
	// A fresh successful claim replaces the previous toast and restarts its lifetime.
	// Selection/filter refreshes intentionally leave this independent feedback alone.
	RewardFeedbackRemaining = 3.f;
	UpdateRewardFeedback(0.f);
}

void UFF_FacilityCodexScreen::ClearRewardFeedback()
{
	RewardFeedbackRemaining = 0.f;
	if (Text_RewardFeedback)
	{
		Text_RewardFeedback->SetText(FText::GetEmpty());
		Text_RewardFeedback->SetVisibility(ESlateVisibility::Hidden);
		Text_RewardFeedback->SetRenderOpacity(1.f);
		Text_RewardFeedback->SetRenderTranslation(FVector2D::ZeroVector);
	}
	if (Border_RewardFeedback)
	{
		Border_RewardFeedback->SetVisibility(ESlateVisibility::Collapsed);
		Border_RewardFeedback->SetRenderOpacity(1.f);
		Border_RewardFeedback->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void UFF_FacilityCodexScreen::UpdateRewardFeedback(float DeltaTime)
{
	if (RewardFeedbackRemaining <= 0.f) return;
	RewardFeedbackRemaining = FMath::Max(0.f, RewardFeedbackRemaining - FMath::Max(DeltaTime, 0.f));
	if (RewardFeedbackRemaining <= 0.f) { ClearRewardFeedback(); return; }
	UWidget* Surface = Border_RewardFeedback ? static_cast<UWidget*>(Border_RewardFeedback.Get()) : Text_RewardFeedback.Get();
	if (!Surface) { RewardFeedbackRemaining = 0.f; return; }
	const auto* Accessibility = UFF_UIAccessibilitySubsystem::Get(this);
	const float MotionDuration = Accessibility ? Accessibility->GetMotionDuration(EFF_UIMotionSpeed::Base) : .2f;
	const float Elapsed = 3.f - RewardFeedbackRemaining;
	const float Entrance = MotionDuration > 0.f ? FMath::Clamp(Elapsed / MotionDuration, 0.f, 1.f) : 1.f;
	const float Fade = MotionDuration > 0.f ? FMath::Clamp(RewardFeedbackRemaining / MotionDuration, 0.f, 1.f) : 1.f;
	Surface->SetRenderTranslation(FVector2D(0.f, 4.f * (1.f - Entrance)));
	Surface->SetRenderOpacity(Fade);
}
void UFF_FacilityCodexScreen::SetDetailPage(int32 Page)
{
	if (!Switcher_Detail) return;
	if (ActiveDetailPage == Page && Switcher_Detail->GetActiveWidgetIndex() == Page) return;
	ActiveDetailPage = Page;
	Switcher_Detail->SetActiveWidgetIndex(Page);
	const TArray<UButton*> Buttons{ Button_DetailRecipe, Button_DetailPlacement, Button_DetailEffect };
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		SetTabSelected(Buttons[Index], Index == Page);
	}
}
void UFF_FacilityCodexScreen::SetTabSelected(UButton* Button, bool bSelected)
{
	if (!Button) return;
	const FButtonStyle& Authored = AuthoredTabStyles.FindOrAdd(Button->GetFName(), Button->GetStyle());
	FButtonStyle Style = Authored;
	// Keep a selected tab focusable and fully colored; its handler prevents a
	// redundant refresh. The selected fill remains editable in the saved WBP.
	if (bSelected) Style.Normal = Style.Hovered = Style.Pressed = Authored.Disabled;
	Button->SetStyle(Style);
	Button->SetIsEnabled(true);
	if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
		Label->SetColorAndOpacity(FSlateColor(bSelected ? SelectedFilterTextColor : InactiveFilterTextColor));
}
void UFF_FacilityCodexScreen::HandleSearchChanged(const FText& Text) { Filter.SearchText = Text.ToString(); RefreshFilter(); }

#undef LOCTEXT_NAMESPACE
