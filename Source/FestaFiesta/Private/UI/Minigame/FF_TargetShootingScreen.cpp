// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_TargetShootingScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Minigame/FF_TargetShootingRuntime.h"
#include "Rendering/DrawElementTypes.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FFTargetShootingScreen"

namespace
{
	const TCHAR* const TargetShootingBackdropPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/AAA/T_MG_TargetShooting_Backdrop_AAA."
			 "T_MG_TargetShooting_Backdrop_AAA");

	void StyleNativeText(
		UTextBlock* TextBlock,
		const float FontSize,
		const FLinearColor& Color = FLinearColor::White,
		const FName Typeface = TEXT("Bold"))
	{
		if (!TextBlock)
		{
			return;
		}

		FSlateFontInfo Font = TextBlock->GetFont();
		if (UFont* UiFont = LoadObject<UFont>(
			nullptr,
			TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard")))
		{
			Font.FontObject = UiFont;
		}
		Font.Size = FMath::RoundToInt(FontSize);
		Font.TypefaceFontName = Typeface;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetShadowOffset(FVector2D(1.0f, 2.0f));
		TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.01f, 0.02f, 0.82f));
	}
}

void UFF_TargetShootingTargetVisual::Configure(
	const FLinearColor& InOuterColor,
	const FLinearColor& InPrecisionColor,
	const FLinearColor& InBullseyeColor,
	const float InPrecisionRatio,
	const float InBullseyeRatio,
	const float InLineThickness)
{
	if (OuterColor.Equals(InOuterColor)
		&& PrecisionColor.Equals(InPrecisionColor)
		&& BullseyeColor.Equals(InBullseyeColor)
		&& FMath::IsNearlyEqual(PrecisionRatio, InPrecisionRatio)
		&& FMath::IsNearlyEqual(BullseyeRatio, InBullseyeRatio)
		&& FMath::IsNearlyEqual(LineThickness, InLineThickness))
	{
		return;
	}
	OuterColor = InOuterColor;
	PrecisionColor = InPrecisionColor;
	BullseyeColor = InBullseyeColor;
	PrecisionRatio = FMath::Clamp(InPrecisionRatio, 0.01f, 0.99f);
	BullseyeRatio = FMath::Clamp(InBullseyeRatio, 0.01f, PrecisionRatio);
	LineThickness = FMath::Clamp(InLineThickness, 1.0f, 12.0f);
	InvalidateLayoutAndVolatility();
}

int32 UFF_TargetShootingTargetVisual::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (LocalSize.X <= UE_SMALL_NUMBER || LocalSize.Y <= UE_SMALL_NUMBER)
	{
		return BaseLayer;
	}

	constexpr int32 SegmentCount = 64;
	auto PaintEllipse = [&](const float RadiusRatio, const FLinearColor& Color, const int32 RingLayer)
	{
		TArray<FVector2f> Points;
		Points.Reserve(SegmentCount + 1);
		const FVector2f Center(
			static_cast<float>(LocalSize.X * 0.5),
			static_cast<float>(LocalSize.Y * 0.5));
		const FVector2f Radius(
			FMath::Max(0.0f, static_cast<float>(LocalSize.X * 0.5) - LineThickness * 0.5f),
			FMath::Max(0.0f, static_cast<float>(LocalSize.Y * 0.5) - LineThickness * 0.5f));
		for (int32 Index = 0; Index <= SegmentCount; ++Index)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(Index) / SegmentCount;
			Points.Emplace(
				Center.X + Radius.X * RadiusRatio * FMath::Cos(Angle),
				Center.Y + Radius.Y * RadiusRatio * FMath::Sin(Angle));
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			RingLayer,
			AllottedGeometry.ToPaintGeometry(),
			MoveTemp(Points),
			ESlateDrawEffect::None,
			Color * InWidgetStyle.GetColorAndOpacityTint(),
			true,
			LineThickness);
	};

	PaintEllipse(1.0f, OuterColor, BaseLayer + 1);
	PaintEllipse(PrecisionRatio, PrecisionColor, BaseLayer + 2);
	PaintEllipse(BullseyeRatio, BullseyeColor, BaseLayer + 3);

	// Crisp cardinal acquisition marks make the target readable against detailed art.
	// They remain inside the authoritative outer ellipse and never imply a larger hit area.
	const FVector2f Center(
		static_cast<float>(LocalSize.X * 0.5),
		static_cast<float>(LocalSize.Y * 0.5));
	const FVector2f Radius(
		FMath::Max(0.0f, static_cast<float>(LocalSize.X * 0.5) - LineThickness),
		FMath::Max(0.0f, static_cast<float>(LocalSize.Y * 0.5) - LineThickness));
	auto PaintTick = [&](const FVector2f& Start, const FVector2f& End, const int32 TickLayer)
	{
		TArray<FVector2f> Points { Start, End };
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			TickLayer,
			AllottedGeometry.ToPaintGeometry(),
			MoveTemp(Points),
			ESlateDrawEffect::None,
			BullseyeColor * InWidgetStyle.GetColorAndOpacityTint(),
			true,
			FMath::Max(1.0f, LineThickness * 0.72f));
	};
	constexpr float TickInnerRatio = 0.76f;
	PaintTick(
		FVector2f(Center.X - Radius.X, Center.Y),
		FVector2f(Center.X - Radius.X * TickInnerRatio, Center.Y),
		BaseLayer + 4);
	PaintTick(
		FVector2f(Center.X + Radius.X * TickInnerRatio, Center.Y),
		FVector2f(Center.X + Radius.X, Center.Y),
		BaseLayer + 4);
	PaintTick(
		FVector2f(Center.X, Center.Y - Radius.Y),
		FVector2f(Center.X, Center.Y - Radius.Y * TickInnerRatio),
		BaseLayer + 4);
	PaintTick(
		FVector2f(Center.X, Center.Y + Radius.Y * TickInnerRatio),
		FVector2f(Center.X, Center.Y + Radius.Y),
		BaseLayer + 4);
	const float CenterTick = FMath::Max(3.0f, FMath::Min(LocalSize.X, LocalSize.Y) * 0.055f);
	PaintTick(
		FVector2f(Center.X - CenterTick, Center.Y),
		FVector2f(Center.X + CenterTick, Center.Y),
		BaseLayer + 5);
	PaintTick(
		FVector2f(Center.X, Center.Y - CenterTick),
		FVector2f(Center.X, Center.Y + CenterTick),
		BaseLayer + 5);
	return BaseLayer + 5;
}

UFF_TargetShootingScreen::UFF_TargetShootingScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VisualStyle.BackdropTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TargetShootingBackdropPath));
}

bool UFF_TargetShootingScreen::SetRuntime(UFF_TargetShootingRuntime* InRuntime)
{
	if (!IsValid(InRuntime))
	{
		Runtime = nullptr;
		SetSessionId(FGuid());
		ResetTransientPresentationState();
		RefreshPresentation();
		return false;
	}

	const FFF_TargetShootingSnapshot Snapshot = InRuntime->GetTargetSnapshot();
	const FGuid RuntimeSessionId = Snapshot.Session.Context.SessionId;
	if (!RuntimeSessionId.IsValid())
	{
		return false;
	}

	if (GetSessionId().IsValid() && GetSessionId() != RuntimeSessionId)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 다른 세션의 TargetShooting runtime 연결을 거부했다. Screen=%s Runtime=%s"),
			*GetName(), *GetSessionId().ToString(), *RuntimeSessionId.ToString());
		return false;
	}

	if (!GetSessionId().IsValid())
	{
		// Direct CreateWidget 사용과 자동화에서도 안전하게 동작한다. 일반 게임 흐름에서는
		// subsystem이 BindRuntime 전에 동일한 SessionId를 먼저 지정한다.
		SetSessionId(RuntimeSessionId);
	}

	if (Runtime != InRuntime)
	{
		ResetTransientPresentationState();
	}
	Runtime = InRuntime;
	ApplySnapshot(Snapshot);
	return true;
}

bool UFF_TargetShootingScreen::BindRuntime(UFF_MinigameRuntime* InRuntime)
{
	return SetRuntime(Cast<UFF_TargetShootingRuntime>(InRuntime));
}

void UFF_TargetShootingScreen::ShowResult(const FFF_MinigameResult& Result)
{
	if (GetSessionId().IsValid() && Result.SessionId != GetSessionId())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 다른 세션의 결과 표시를 무시했다. Screen=%s Result=%s"),
			*GetName(), *GetSessionId().ToString(), *Result.SessionId.ToString());
		return;
	}

	DisplayedResult = Result;
	bHasResult = true;
	RefreshPresentation();
}

void UFF_TargetShootingScreen::ApplySnapshot(const FFF_TargetShootingSnapshot& Snapshot)
{
	const FGuid SnapshotSessionId = Snapshot.Session.Context.SessionId;
	if (GetSessionId().IsValid() && SnapshotSessionId != GetSessionId())
	{
		return;
	}

	if (Snapshot.TargetSerial > 0 && Snapshot.TargetSerial != PresentedTargetSerial)
	{
		PresentedTargetSerial = Snapshot.TargetSerial;
		TargetSpawnPulseRemainingSeconds = FMath::Max(
			0.0f,
			VisualStyle.TargetSpawnPulseSeconds);
	}
	if (Snapshot.LatestFeedbackSerial > 0
		&& Snapshot.LatestFeedbackSerial != PresentedFeedbackSerial)
	{
		PresentedFeedbackSerial = Snapshot.LatestFeedbackSerial;
		FeedbackRemainingSeconds = FMath::Max(
			0.0f,
			VisualStyle.FeedbackDurationSeconds);
		if (Snapshot.LatestFeedbackType != EFF_TargetShootingFeedbackType::Escaped)
		{
			ReticleShotPulseRemainingSeconds = FMath::Max(
				0.0f,
				VisualStyle.ReticleShotPulseSeconds);
		}
	}

	DisplayedSnapshot = Snapshot;
	bHasSnapshot = true;
	RefreshPresentation();
}

TSharedRef<SWidget> UFF_TargetShootingScreen::RebuildWidget()
{
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		if (!BuildNativeWidgetTree())
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] TargetShooting native fallback WidgetTree 생성에 실패했다."), *GetName());
		}
	}

	ResolveBindingsFromWidgetTree();
	bUsingDesignerWidgetTree = !bBuiltNativeWidgetTree && HasUsableDesignerBindings();
	InitializeBindings();
	RefreshPresentation();
	return Super::RebuildWidget();
}

void UFF_TargetShootingScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ResolveBindingsFromWidgetTree();
	InitializeBindings();
	RefreshPresentation();
}

void UFF_TargetShootingScreen::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveBindingsFromWidgetTree();
	InitializeBindings();
	RefreshPresentation();
}

void UFF_TargetShootingScreen::NativeDestruct()
{
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveAll(this);
	}

	InitialFocusTarget = nullptr;
	Runtime = nullptr;
	SetSessionId(FGuid());
	ResetTransientPresentationState();
	if (Border_PlayArea)
	{
		Border_PlayArea->SetCursor(EMouseCursor::Default);
		if (bHasBasePlayAreaBrushColor)
		{
			Border_PlayArea->SetBrushColor(BasePlayAreaBrushColor);
		}
	}
	bHasBasePlayAreaBrushColor = false;
	bBindingsInitialized = false;
	bResultFocusApplied = false;
	Super::NativeDestruct();
}

void UFF_TargetShootingScreen::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (IsValid(Runtime))
	{
		ApplySnapshot(Runtime->GetTargetSnapshot());
	}
	else
	{
		RefreshTargetLayout();
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, InDeltaTime);
	TargetSpawnPulseRemainingSeconds = FMath::Max(
		0.0f,
		TargetSpawnPulseRemainingSeconds - SafeDeltaSeconds);
	FeedbackRemainingSeconds = FMath::Max(
		0.0f,
		FeedbackRemainingSeconds - SafeDeltaSeconds);
	ReticleShotPulseRemainingSeconds = FMath::Max(
		0.0f,
		ReticleShotPulseRemainingSeconds - SafeDeltaSeconds);
	RefreshTargetLayout();
	RefreshFeedbackPresentation();
	RefreshReticlePresentation();
}

FReply UFF_TargetShootingScreen::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	// The modal screen consumes left-clicks even while a result is visible or the
	// runtime is being detached. This prevents the same click from selecting a
	// facility in the world underneath the CommonUI stack.
	if (!bHasSnapshot
		|| bHasResult
		|| DisplayedSnapshot.Session.State != EFF_MinigameSessionState::Playing
		|| !IsValid(Runtime))
	{
		return FReply::Handled();
	}

	FVector2D NormalizedPoint = FVector2D::ZeroVector;
	if (!TryMakeNormalizedPoint(InMouseEvent.GetScreenSpacePosition(), NormalizedPoint))
	{
		return FReply::Handled();
	}
	ReticlePointNormalized = NormalizedPoint;
	bReticleInsidePlayArea = true;

	// UI deliberately does not branch on Hit/Miss. The runtime validates the
	// session id, target serial and normalized point, then owns all scoring.
	Runtime->SubmitShot(
		GetSessionId(),
		NormalizedPoint,
		DisplayedSnapshot.TargetSerial);
	ApplySnapshot(Runtime->GetTargetSnapshot());
	return FReply::Handled();
}

FReply UFF_TargetShootingScreen::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	FVector2D NormalizedPoint = FVector2D::ZeroVector;
	bReticleInsidePlayArea = TryMakeNormalizedPoint(
		InMouseEvent.GetScreenSpacePosition(),
		NormalizedPoint);
	if (bReticleInsidePlayArea)
	{
		ReticlePointNormalized = NormalizedPoint;
	}
	RefreshReticlePresentation();
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UFF_TargetShootingScreen::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	bReticleInsidePlayArea = false;
	RefreshReticlePresentation();
	Super::NativeOnMouseLeave(InMouseEvent);
}

UWidget* UFF_TargetShootingScreen::NativeGetDesiredFocusTarget() const
{
	const bool bSessionFinished = bHasSnapshot
		&& (DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Completed
			|| DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Cancelled);
	if (Button_Close && (bHasResult || bSessionFinished))
	{
		return Button_Close;
	}

	// The screen itself is focusable (configured by UFF_MinigameScreenBase) and has
	// no activation action. Giving gameplay focus to the close button would let an
	// incidental Enter/Space cancel the live session.
	return const_cast<UFF_TargetShootingScreen*>(this);
}

bool UFF_TargetShootingScreen::BuildNativeWidgetTree()
{
	if (!WidgetTree)
	{
		return false;
	}

	bBuiltNativeWidgetTree = true;
	bUsingDesignerWidgetTree = false;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("Root_NativeTargetShooting"));
	Image_Backdrop = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("Image_Backdrop"));
	UBorder* ScreenWash = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ScreenWash"));
	UCanvasPanel* LayoutCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("Canvas_AAAHud"));
	UBorder* ArenaFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ArenaFrame"));
	Border_PlayArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_PlayArea"));
	Canvas_PlayArea = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("Canvas_PlayArea"));
	Border_Target = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_Target"));

	if (!Root || !Image_Backdrop || !ScreenWash || !LayoutCanvas || !ArenaFrame
		|| !Border_PlayArea || !Canvas_PlayArea || !Border_Target)
	{
		return false;
	}

	WidgetTree->RootWidget = Root;
	auto StretchOnRoot = [Root](UWidget* Widget) -> bool
	{
		if (UOverlaySlot* Slot = Root->AddChildToOverlay(Widget))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			return true;
		}
		return false;
	};
	auto PlaceAnchored = [LayoutCanvas](
		UWidget* Widget,
		const FAnchors& Anchors,
		const int32 ZOrder) -> bool
	{
		if (UCanvasPanelSlot* Slot = LayoutCanvas->AddChildToCanvas(Widget))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(FMargin(0.0f));
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetZOrder(ZOrder);
			return true;
		}
		return false;
	};
	auto ConfigureCard = [](UBorder* Card, const FLinearColor& Color, const FMargin& InPadding)
	{
		Card->SetBrushColor(Color);
		Card->SetPadding(InPadding);
		Card->SetHorizontalAlignment(HAlign_Fill);
		Card->SetVerticalAlignment(VAlign_Center);
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
	};

	Image_Backdrop->SetColorAndOpacity(VisualStyle.BackdropTint);
	Image_Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
	ScreenWash->SetBrushColor(VisualStyle.ScreenWashColor);
	ScreenWash->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!StretchOnRoot(Image_Backdrop)
		|| !StretchOnRoot(ScreenWash)
		|| !StretchOnRoot(LayoutCanvas))
	{
		return false;
	}

	ArenaFrame->SetBrushColor(VisualStyle.ArenaFrameColor);
	ArenaFrame->SetPadding(FMargin(2.0f));
	ArenaFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Border_PlayArea->SetBrushColor(VisualStyle.ArenaSurfaceColor);
	Border_PlayArea->SetPadding(FMargin(0.0f));
	Border_PlayArea->SetContent(Canvas_PlayArea);
	ArenaFrame->SetContent(Border_PlayArea);
	if (!PlaceAnchored(ArenaFrame, FAnchors(0.17f, 0.18f, 0.91f, 0.83f), 10))
	{
		return false;
	}

	Border_Target->SetBrushColor(VisualStyle.NormalTargetColor);
	Border_Target->SetPadding(FMargin(0.0f));
	Border_Target->SetVisibility(ESlateVisibility::HitTestInvisible);
	UTextBlock* TargetMark = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_TargetMark"));
	if (!TargetMark)
	{
		return false;
	}
	TargetMark->SetText(LOCTEXT("NativeTargetMark", "+"));
	TargetMark->SetJustification(ETextJustify::Center);
	StyleNativeText(TargetMark, 36.0f, VisualStyle.PrimaryTextColor);
	Border_Target->SetContent(TargetMark);

	UCanvasPanelSlot* TargetSlot = Canvas_PlayArea->AddChildToCanvas(Border_Target);
	if (!TargetSlot)
	{
		return false;
	}
	TargetSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	TargetSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TargetSlot->SetPosition(FVector2D::ZeroVector);
	TargetSlot->SetSize(FVector2D(112.0f, 112.0f));
	TargetSlot->SetZOrder(1);

	Text_ShotFeedback = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_ShotFeedback"));
	Text_Reticle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Reticle"));
	UTextBlock* RangeLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_RangeLabel"));
	if (!Text_ShotFeedback || !Text_Reticle || !RangeLabel)
	{
		return false;
	}
	Text_ShotFeedback->SetText(LOCTEXT("NativeShotFeedbackPlaceholder", "BULLSEYE  +200"));
	Text_ShotFeedback->SetJustification(ETextJustify::Center);
	Text_ShotFeedback->SetVisibility(ESlateVisibility::Collapsed);
	StyleNativeText(Text_ShotFeedback, 28.0f, VisualStyle.HudAmberColor);
	if (UCanvasPanelSlot* FeedbackSlot = Canvas_PlayArea->AddChildToCanvas(Text_ShotFeedback))
	{
		FeedbackSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FeedbackSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FeedbackSlot->SetPosition(FVector2D::ZeroVector);
		FeedbackSlot->SetSize(FVector2D(360.0f, 96.0f));
		FeedbackSlot->SetZOrder(10);
	}
	else
	{
		return false;
	}
	Text_Reticle->SetText(FText::FromString(VisualStyle.ReticleGlyph));
	Text_Reticle->SetJustification(ETextJustify::Center);
	Text_Reticle->SetVisibility(ESlateVisibility::Collapsed);
	StyleNativeText(Text_Reticle, VisualStyle.ReticleFontSize, VisualStyle.ReticleColor);
	if (UCanvasPanelSlot* ReticleSlot = Canvas_PlayArea->AddChildToCanvas(Text_Reticle))
	{
		ReticleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ReticleSlot->SetPosition(FVector2D::ZeroVector);
		ReticleSlot->SetAutoSize(true);
		ReticleSlot->SetZOrder(20);
	}
	else
	{
		return false;
	}
	RangeLabel->SetText(LOCTEXT("RangeLabel", "LIVE RANGE  //  ACQUIRE & ENGAGE"));
	StyleNativeText(RangeLabel, 11.0f, VisualStyle.SecondaryTextColor, TEXT("SemiBold"));
	RangeLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* RangeSlot = Canvas_PlayArea->AddChildToCanvas(RangeLabel))
	{
		RangeSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		RangeSlot->SetAlignment(FVector2D::ZeroVector);
		RangeSlot->SetPosition(FVector2D(18.0f, 14.0f));
		RangeSlot->SetAutoSize(true);
		RangeSlot->SetZOrder(30);
	}
	else
	{
		return false;
	}

	UBorder* TitleCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_TitleCard"));
	UVerticalBox* TitleColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("Column_Title"));
	UTextBlock* TitleEyebrow = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_TitleEyebrow"));
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Title"));
	if (!TitleCard || !TitleColumn || !TitleEyebrow || !TitleText)
	{
		return false;
	}
	ConfigureCard(TitleCard, VisualStyle.HudPanelColor, FMargin(20.0f, 12.0f));
	TitleEyebrow->SetText(LOCTEXT("TitleEyebrow", "BUSAN WATERFRONT // LIVE EVENT"));
	StyleNativeText(TitleEyebrow, 10.0f, VisualStyle.HudAccentColor, TEXT("SemiBold"));
	TitleText->SetText(LOCTEXT("Title", "NIGHT RANGE"));
	StyleNativeText(TitleText, 27.0f, VisualStyle.PrimaryTextColor);
	TitleColumn->AddChildToVerticalBox(TitleEyebrow);
	TitleColumn->AddChildToVerticalBox(TitleText);
	TitleCard->SetContent(TitleColumn);
	if (!PlaceAnchored(TitleCard, FAnchors(0.025f, 0.035f, 0.285f, 0.145f), 30))
	{
		return false;
	}

	UBorder* TimerCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_TimerCard"));
	UVerticalBox* TimerColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("Column_Timer"));
	USizeBox* TimerGaugeSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("Size_TimeGauge"));
	Progress_Time = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("Progress_Time"));
	UBorder* ScoreCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ScoreCard"));
	UBorder* HitCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_HitCard"));
	UBorder* PerformanceCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_PerformanceCard"));
	UBorder* StreakCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_StreakCard"));
	Text_RemainingTime = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_RemainingTime"));
	Text_Score = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Score"));
	Text_HitCount = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_HitCount"));
	Text_MissCount = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_MissCount"));
	Text_Streak = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Streak"));

	if (!TimerCard || !TimerColumn || !TimerGaugeSize || !Progress_Time
		|| !ScoreCard || !HitCard || !PerformanceCard || !StreakCard
		|| !Text_RemainingTime || !Text_Score
		|| !Text_HitCount || !Text_MissCount || !Text_Streak)
	{
		return false;
	}

	ConfigureCard(TimerCard, VisualStyle.HudPanelColor, FMargin(18.0f, 6.0f));
	ConfigureCard(ScoreCard, VisualStyle.HudPanelColor, FMargin(18.0f, 10.0f));
	ConfigureCard(HitCard, VisualStyle.HudPanelMutedColor, FMargin(16.0f, 12.0f));
	ConfigureCard(PerformanceCard, VisualStyle.HudPanelMutedColor, FMargin(16.0f, 12.0f));
	ConfigureCard(StreakCard, VisualStyle.HudPanelColor, FMargin(18.0f, 8.0f));
	Text_RemainingTime->SetText(LOCTEXT("NativeTimePlaceholder", "00:30"));
	Text_RemainingTime->SetJustification(ETextJustify::Center);
	StyleNativeText(Text_RemainingTime, 48.0f, VisualStyle.PrimaryTextColor);
	Progress_Time->SetPercent(1.0f);
	Progress_Time->SetFillColorAndOpacity(VisualStyle.HudAccentColor);
	Progress_Time->SetVisibility(ESlateVisibility::HitTestInvisible);
	TimerGaugeSize->SetHeightOverride(5.0f);
	TimerGaugeSize->SetContent(Progress_Time);
	Text_Score->SetText(LOCTEXT("NativeScorePlaceholder", "SCORE  000000"));
	Text_Score->SetJustification(ETextJustify::Right);
	StyleNativeText(Text_Score, 27.0f, VisualStyle.HudAmberColor);
	Text_HitCount->SetText(LOCTEXT("NativeHitsPlaceholder", "HITS  0\nACCURACY  0%"));
	Text_HitCount->SetAutoWrapText(true);
	StyleNativeText(Text_HitCount, 16.0f, VisualStyle.PrimaryTextColor, TEXT("SemiBold"));
	Text_MissCount->SetText(LOCTEXT("NativePerformancePlaceholder", "RATE  0.0 /s\nREACTION  0ms\nMISS  0"));
	Text_MissCount->SetAutoWrapText(true);
	StyleNativeText(Text_MissCount, 15.0f, VisualStyle.SecondaryTextColor, TEXT("SemiBold"));
	Text_Streak->SetText(LOCTEXT("NativeStreakPlaceholder", "COMBO x0     BEST 0"));
	Text_Streak->SetJustification(ETextJustify::Center);
	StyleNativeText(Text_Streak, 22.0f, VisualStyle.HudAccentColor);
	if (UVerticalBoxSlot* TimerTextSlot = TimerColumn->AddChildToVerticalBox(Text_RemainingTime))
	{
		TimerTextSlot->SetHorizontalAlignment(HAlign_Fill);
		TimerTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UVerticalBoxSlot* GaugeSlot = TimerColumn->AddChildToVerticalBox(TimerGaugeSize))
	{
		GaugeSlot->SetHorizontalAlignment(HAlign_Fill);
		GaugeSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 4.0f));
	}
	TimerCard->SetContent(TimerColumn);
	ScoreCard->SetContent(Text_Score);
	HitCard->SetContent(Text_HitCount);
	PerformanceCard->SetContent(Text_MissCount);
	StreakCard->SetContent(Text_Streak);
	if (!PlaceAnchored(TimerCard, FAnchors(0.405f, 0.025f, 0.605f, 0.150f), 31)
		|| !PlaceAnchored(ScoreCard, FAnchors(0.715f, 0.035f, 0.910f, 0.145f), 30)
		|| !PlaceAnchored(HitCard, FAnchors(0.025f, 0.270f, 0.150f, 0.395f), 30)
		|| !PlaceAnchored(PerformanceCard, FAnchors(0.025f, 0.420f, 0.150f, 0.590f), 30)
		|| !PlaceAnchored(StreakCard, FAnchors(0.315f, 0.855f, 0.610f, 0.945f), 30))
	{
		return false;
	}

	UBorder* HintCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_InputHint"));
	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_InputHint"));
	if (!HintCard || !HintText)
	{
		return false;
	}
	ConfigureCard(HintCard, VisualStyle.HudPanelMutedColor, FMargin(16.0f, 8.0f));
	HintText->SetText(LOCTEXT("InputHint", "MOUSE  AIM     [LMB]  FIRE     [ESC]  ABORT"));
	HintText->SetJustification(ETextJustify::Center);
	StyleNativeText(HintText, 13.0f, VisualStyle.SecondaryTextColor, TEXT("SemiBold"));
	HintCard->SetContent(HintText);
	if (!PlaceAnchored(HintCard, FAnchors(0.655f, 0.855f, 0.910f, 0.945f), 30))
	{
		return false;
	}

	Panel_Result = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Panel_Result"));
	UOverlay* ResultStage = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("Overlay_ResultStage"));
	USizeBox* ResultSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("Size_ResultCard"));
	UBorder* ResultFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ResultFrame"));
	UBorder* ResultSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ResultSurface"));
	UVerticalBox* ResultColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("Column_Result"));
	UTextBlock* ResultEyebrow = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_ResultEyebrow"));
	Text_Result = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Result"));
	USizeBox* ResultCtaReserve = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("Size_ResultCtaReserve"));
	if (!Panel_Result || !ResultStage || !ResultSize || !ResultFrame || !ResultSurface
		|| !ResultColumn || !ResultEyebrow || !Text_Result || !ResultCtaReserve)
	{
		return false;
	}

	Panel_Result->SetBrushColor(FLinearColor(0.002f, 0.010f, 0.024f, 0.88f));
	Panel_Result->SetPadding(FMargin(0.0f));
	Panel_Result->SetContent(ResultStage);
	ResultSize->SetWidthOverride(720.0f);
	ResultSize->SetHeightOverride(500.0f);
	ResultSize->SetContent(ResultFrame);
	ResultFrame->SetBrushColor(VisualStyle.HudAmberColor);
	ResultFrame->SetPadding(FMargin(2.0f));
	ResultFrame->SetContent(ResultSurface);
	ResultSurface->SetBrushColor(VisualStyle.HudPanelColor);
	ResultSurface->SetPadding(FMargin(48.0f, 40.0f));
	ResultSurface->SetContent(ResultColumn);
	ResultEyebrow->SetText(LOCTEXT("ResultEyebrow", "AFTER ACTION REPORT  //  NIGHT RANGE"));
	ResultEyebrow->SetJustification(ETextJustify::Center);
	StyleNativeText(ResultEyebrow, 13.0f, VisualStyle.HudAccentColor, TEXT("SemiBold"));
	Text_Result->SetJustification(ETextJustify::Center);
	Text_Result->SetAutoWrapText(true);
	StyleNativeText(Text_Result, 26.0f, VisualStyle.PrimaryTextColor);
	if (UVerticalBoxSlot* EyebrowSlot = ResultColumn->AddChildToVerticalBox(ResultEyebrow))
	{
		EyebrowSlot->SetHorizontalAlignment(HAlign_Center);
		EyebrowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	}
	if (UVerticalBoxSlot* ResultTextSlot = ResultColumn->AddChildToVerticalBox(Text_Result))
	{
		ResultTextSlot->SetHorizontalAlignment(HAlign_Center);
		ResultTextSlot->SetVerticalAlignment(VAlign_Center);
		ResultTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	ResultCtaReserve->SetHeightOverride(62.0f);
	if (!ResultColumn->AddChildToVerticalBox(ResultCtaReserve))
	{
		return false;
	}
	if (UOverlaySlot* ResultCardSlot = ResultStage->AddChildToOverlay(ResultSize))
	{
		ResultCardSlot->SetHorizontalAlignment(HAlign_Center);
		ResultCardSlot->SetVerticalAlignment(VAlign_Center);
	}
	else
	{
		return false;
	}
	Panel_Result->SetVisibility(ESlateVisibility::Collapsed);
	if (!PlaceAnchored(Panel_Result, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 100))
	{
		return false;
	}

	Button_Close = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("Button_Close"));
	NativeCloseLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_CloseLabel"));
	if (!Button_Close || !NativeCloseLabel)
	{
		return false;
	}

	NativeCloseLabel->SetText(LOCTEXT("NativeClose", "EXIT  [ESC]"));
	NativeCloseLabel->SetJustification(ETextJustify::Center);
	StyleNativeText(NativeCloseLabel, 13.0f, VisualStyle.PrimaryTextColor, TEXT("SemiBold"));
	Button_Close->SetContent(NativeCloseLabel);
	Button_Close->SetBackgroundColor(FLinearColor(
		VisualStyle.HudPanelMutedColor.R,
		VisualStyle.HudPanelMutedColor.G,
		VisualStyle.HudPanelMutedColor.B,
		1.0f));
	// UButton is focusable by default; keeping that default gives keyboard and
	// gamepad users a deterministic close target without version-specific setters.
	if (!PlaceAnchored(Button_Close, FAnchors(0.925f, 0.050f, 0.982f, 0.110f), 110))
	{
		return false;
	}

	return HasUsableDesignerBindings();
}

void UFF_TargetShootingScreen::ResolveBindingsFromWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	Border_PlayArea = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_PlayArea")));
	Image_Backdrop = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_Backdrop")));
	Canvas_PlayArea = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("Canvas_PlayArea")));
	Border_Target = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_Target")));
	TargetVisual = Cast<UFF_TargetShootingTargetVisual>(
		WidgetTree->FindWidget(TEXT("TargetVisual_AuthoritativeRings")));
	Text_ShotFeedback = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_ShotFeedback")));
	Text_Reticle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Reticle")));
	Text_RemainingTime = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_RemainingTime")));
	Progress_Time = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("Progress_Time")));
	Text_Score = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Score")));
	Text_HitCount = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_HitCount")));
	Text_MissCount = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_MissCount")));
	Text_Streak = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Streak")));
	Panel_Result = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Panel_Result")));
	Text_Result = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Result")));
	Button_Close = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_Close")));
	NativeCloseLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_CloseLabel")));
}

bool UFF_TargetShootingScreen::HasUsableDesignerBindings() const
{
	if (!Border_PlayArea || !Canvas_PlayArea || !Border_Target
		|| !Text_RemainingTime || !Text_Score || !Text_HitCount
		|| !Text_MissCount || !Text_Streak || !Panel_Result
		|| !Text_Result || !Button_Close)
	{
		return false;
	}

	return Border_PlayArea->GetContent() == Canvas_PlayArea
		&& Border_Target->GetParent() == Canvas_PlayArea
		&& Cast<UCanvasPanelSlot>(Border_Target->Slot) != nullptr;
}

void UFF_TargetShootingScreen::InitializeBindings()
{
	if (bBindingsInitialized)
	{
		return;
	}

	if (!HasUsableDesignerBindings())
	{
		if (WidgetTree && WidgetTree->RootWidget)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] TargetShooting WBP의 BindWidget 이름, 타입 또는 play-area 계층이 올바르지 않다."),
				*GetName());
		}
		return;
	}

	Border_Target->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Image_Backdrop)
	{
		Image_Backdrop->SetColorAndOpacity(VisualStyle.BackdropTint);
		Image_Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UTexture2D* BackdropTexture = VisualStyle.BackdropTexture.LoadSynchronous())
		{
			Image_Backdrop->SetBrushFromTexture(BackdropTexture, true);
		}
	}
	if (!TargetVisual)
	{
		TargetVisual = WidgetTree->ConstructWidget<UFF_TargetShootingTargetVisual>(
			UFF_TargetShootingTargetVisual::StaticClass(),
			TEXT("TargetVisual_AuthoritativeRings"));
	}
	if (TargetVisual)
	{
		TargetVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
		Border_Target->SetPadding(FMargin(0.0f));
		Border_Target->SetContent(TargetVisual);
		FSlateBrush NoBackground;
		NoBackground.DrawAs = ESlateBrushDrawType::NoDrawType;
		Border_Target->SetBrush(NoBackground);
	}
	EnsureTransientPresentationWidgets();
	if (!bHasBasePlayAreaBrushColor)
	{
		BasePlayAreaBrushColor = Border_PlayArea->GetBrushColor();
		bHasBasePlayAreaBrushColor = true;
	}
	Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	InitialFocusTarget = this;
	bBindingsInitialized = true;
}

void UFF_TargetShootingScreen::EnsureTransientPresentationWidgets()
{
	if (!WidgetTree || !Canvas_PlayArea)
	{
		return;
	}

	auto EnsureDirectCanvasChild = [this](
		UWidget* Widget,
		const FVector2D& DefaultSize,
		const int32 ZOrder,
		const bool bAutoSize) -> UCanvasPanelSlot*
	{
		if (!Widget)
		{
			return nullptr;
		}

		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
		if (Widget->GetParent() != Canvas_PlayArea || !CanvasSlot)
		{
			// Optional designer widgets are runtime-positioned in normalized play-area
			// coordinates. Reparenting them here keeps WBP layout edits from silently
			// separating the visible reticle/feedback from the authoritative hit canvas.
			Widget->RemoveFromParent();
			CanvasSlot = Canvas_PlayArea->AddChildToCanvas(Widget);
			if (CanvasSlot)
			{
				CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				CanvasSlot->SetPosition(FVector2D::ZeroVector);
				CanvasSlot->SetSize(DefaultSize);
			}
		}

		if (CanvasSlot)
		{
			CanvasSlot->SetZOrder(ZOrder);
			CanvasSlot->SetAutoSize(bAutoSize);
		}
		return CanvasSlot;
	};

	if (!Text_ShotFeedback)
	{
		Text_ShotFeedback = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Text_ShotFeedback"));
	}
	EnsureDirectCanvasChild(Text_ShotFeedback, FVector2D(320.0f, 90.0f), 10, false);
	if (Text_ShotFeedback)
	{
		Text_ShotFeedback->SetJustification(ETextJustify::Center);
		Text_ShotFeedback->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Text_ShotFeedback->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
		StyleNativeText(Text_ShotFeedback, FMath::Max(8.0f, VisualStyle.FeedbackFontSize));
		Text_ShotFeedback->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!Text_Reticle)
	{
		Text_Reticle = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Text_Reticle"));
	}
	EnsureDirectCanvasChild(Text_Reticle, FVector2D(64.0f, 64.0f), 20, true);
	if (Text_Reticle)
	{
		Text_Reticle->SetText(FText::FromString(VisualStyle.ReticleGlyph));
		Text_Reticle->SetJustification(ETextJustify::Center);
		Text_Reticle->SetColorAndOpacity(FSlateColor(VisualStyle.ReticleColor));
		Text_Reticle->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text_Reticle->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.70f));
		StyleNativeText(Text_Reticle, FMath::Max(8.0f, VisualStyle.ReticleFontSize));
		Text_Reticle->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFF_TargetShootingScreen::ResetTransientPresentationState()
{
	DisplayedSnapshot = FFF_TargetShootingSnapshot();
	DisplayedResult = FFF_MinigameResult();
	bHasSnapshot = false;
	bHasResult = false;
	LastPlayAreaSize = FVector2D::ZeroVector;
	PresentedTargetSerial = 0;
	PresentedFeedbackSerial = 0;
	TargetSpawnPulseRemainingSeconds = 0.0f;
	FeedbackRemainingSeconds = 0.0f;
	ReticleShotPulseRemainingSeconds = 0.0f;
	ReticlePointNormalized = FVector2D(0.5f, 0.5f);
	bReticleInsidePlayArea = false;
}

void UFF_TargetShootingScreen::RefreshPresentation()
{
	if (!HasUsableDesignerBindings())
	{
		return;
	}

	const float RemainingSeconds = bHasSnapshot
		? FMath::Max(0.0f, DisplayedSnapshot.Session.RemainingSeconds)
		: 0.0f;
	const int32 RemainingWholeSeconds = FMath::CeilToInt(RemainingSeconds);
	const int32 RemainingMinutes = RemainingWholeSeconds / 60;
	const int32 RemainingSecondPart = RemainingWholeSeconds % 60;
	const int32 Score = bHasSnapshot ? DisplayedSnapshot.Session.Score : 0;
	const int32 HitCount = bHasSnapshot ? DisplayedSnapshot.HitCount : 0;
	const int32 MissCount = bHasSnapshot ? DisplayedSnapshot.MissCount : 0;
	const int32 CurrentCombo = bHasSnapshot ? DisplayedSnapshot.CurrentCombo : 0;
	const int32 BestCombo = bHasSnapshot ? DisplayedSnapshot.BestCombo : 0;
	const int32 AccuracyPercent = bHasSnapshot
		? FMath::Clamp(FMath::RoundToInt(DisplayedSnapshot.AccuracyPercent), 0, 100)
		: 0;
	const int32 AverageReactionMilliseconds = bHasSnapshot
		? FMath::Max(0, FMath::RoundToInt(DisplayedSnapshot.AverageHitReactionSeconds * 1000.0f))
		: 0;
	FNumberFormattingOptions OneDecimal;
	OneDecimal.SetMinimumFractionalDigits(1);
	OneDecimal.SetMaximumFractionalDigits(1);
	FNumberFormattingOptions TwoDigits;
	TwoDigits.SetMinimumIntegralDigits(2);
	TwoDigits.SetMaximumIntegralDigits(2);
	FNumberFormattingOptions SixDigits;
	SixDigits.SetMinimumIntegralDigits(6);

	Text_RemainingTime->SetText(FText::Format(
		LOCTEXT("RemainingTimeFormat", "{0}:{1}"),
		FText::AsNumber(RemainingMinutes, &TwoDigits),
		FText::AsNumber(RemainingSecondPart, &TwoDigits)));
	Text_RemainingTime->SetColorAndOpacity(FSlateColor(
		bHasSnapshot && RemainingSeconds <= 5.0f
			? VisualStyle.HudAmberColor
			: VisualStyle.PrimaryTextColor));
	if (Progress_Time)
	{
		const float TotalSessionSeconds = bHasSnapshot
			? DisplayedSnapshot.Session.ElapsedSeconds + RemainingSeconds
			: 0.0f;
		Progress_Time->SetPercent(TotalSessionSeconds > UE_SMALL_NUMBER
			? FMath::Clamp(RemainingSeconds / TotalSessionSeconds, 0.0f, 1.0f)
			: 0.0f);
		Progress_Time->SetFillColorAndOpacity(
			bHasSnapshot && RemainingSeconds <= 5.0f
				? VisualStyle.HudAmberColor
				: VisualStyle.HudAccentColor);
	}
	Text_Score->SetText(FText::Format(
		LOCTEXT("ScoreFormat", "SCORE  {0}"),
		FText::AsNumber(Score, &SixDigits)));
	Text_HitCount->SetText(FText::Format(
		LOCTEXT("HitCountFormat", "HITS  {0}\nACCURACY  {1}%"),
		FText::AsNumber(HitCount),
		FText::AsNumber(AccuracyPercent)));
	Text_MissCount->SetText(FText::Format(
		LOCTEXT("PerformanceFormat", "RATE  {0} /s\nREACTION  {1}ms\nMISS  {2}"),
		FText::AsNumber(bHasSnapshot ? DisplayedSnapshot.HitsPerSecond : 0.0f, &OneDecimal),
		FText::AsNumber(AverageReactionMilliseconds),
		FText::AsNumber(MissCount)));
	Text_Streak->SetText(CurrentCombo >= 5
		? FText::Format(
			LOCTEXT("FeverStreakFormat", "FEVER     COMBO x{0}     BEST {1}"),
			FText::AsNumber(CurrentCombo),
			FText::AsNumber(BestCombo))
		: FText::Format(
			LOCTEXT("StreakFormat", "COMBO x{0}     BEST {1}"),
			FText::AsNumber(CurrentCombo),
			FText::AsNumber(BestCombo)));
	Text_Streak->SetColorAndOpacity(FSlateColor(
		CurrentCombo >= 5 ? VisualStyle.HudAmberColor : VisualStyle.HudAccentColor));

	const bool bSessionFinished = bHasSnapshot
		&& (DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Completed
			|| DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Cancelled);
	const bool bShowResult = bHasResult || bSessionFinished;
	Panel_Result->SetVisibility(bShowResult
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
	InitialFocusTarget = bShowResult
		? static_cast<UWidget*>(Button_Close.Get())
		: static_cast<UWidget*>(this);
	if (bShowResult && !bResultFocusApplied && GetOwningLocalPlayer())
	{
		Button_Close->SetFocus();
		bResultFocusApplied = true;
	}
	else if (!bShowResult && bResultFocusApplied)
	{
		if (GetOwningLocalPlayer())
		{
			SetFocus();
		}
		bResultFocusApplied = false;
	}

	if (bHasResult)
	{
		const FText Grade = DisplayedResult.GradeText.IsEmpty()
			? LOCTEXT("Completed", "게임 종료")
			: DisplayedResult.GradeText;
		FFormatNamedArguments ResultArguments;
		ResultArguments.Add(TEXT("Grade"), Grade);
		ResultArguments.Add(TEXT("Score"), DisplayedResult.Score);
		ResultArguments.Add(TEXT("Reward"), DisplayedResult.RewardGold);
		ResultArguments.Add(TEXT("Accuracy"), AccuracyPercent);
		ResultArguments.Add(TEXT("Tps"), FText::AsNumber(
			bHasSnapshot ? DisplayedSnapshot.HitsPerSecond : 0.0f,
			&OneDecimal));
		ResultArguments.Add(TEXT("Reaction"), AverageReactionMilliseconds);
		ResultArguments.Add(TEXT("Combo"), BestCombo);
		Text_Result->SetText(FText::Format(
			LOCTEXT(
				"ResultWithRewardFormat",
				"{Grade}\n\nFINAL SCORE  {Score}\nFESTIVAL GOLD  +{Reward}\n\n"
				"ACCURACY  {Accuracy}%     RATE  {Tps}/s\nREACTION  {Reaction}ms     BEST COMBO  {Combo}"),
			ResultArguments));
	}
	else if (bSessionFinished)
	{
		const FText ResultTitle = DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Completed
			? LOCTEXT("TimeExpired", "30초 플레이 완료!")
			: LOCTEXT("SessionCancelled", "게임이 종료되었습니다");
		FFormatNamedArguments SnapshotArguments;
		SnapshotArguments.Add(TEXT("Title"), ResultTitle);
		SnapshotArguments.Add(TEXT("Score"), Score);
		SnapshotArguments.Add(TEXT("Accuracy"), AccuracyPercent);
		SnapshotArguments.Add(TEXT("Tps"), FText::AsNumber(
			bHasSnapshot ? DisplayedSnapshot.HitsPerSecond : 0.0f,
			&OneDecimal));
		SnapshotArguments.Add(TEXT("Reaction"), AverageReactionMilliseconds);
		SnapshotArguments.Add(TEXT("Combo"), BestCombo);
		Text_Result->SetText(FText::Format(
			LOCTEXT(
				"SnapshotResultFormat",
				"{Title}\n\nFINAL SCORE  {Score}\n\nACCURACY  {Accuracy}%     RATE  {Tps}/s\n"
				"REACTION  {Reaction}ms     BEST COMBO  {Combo}"),
			SnapshotArguments));
	}

	if (NativeCloseLabel)
	{
		NativeCloseLabel->SetText(bShowResult
			? LOCTEXT("NativeResultClose", "CONTINUE  [ESC]")
			: LOCTEXT("NativePlayingClose", "EXIT  [ESC]"));
		NativeCloseLabel->SetColorAndOpacity(FSlateColor(bShowResult
			? FLinearColor(0.008f, 0.028f, 0.045f, 1.0f)
			: VisualStyle.PrimaryTextColor));
	}
	if (Button_Close)
	{
		Button_Close->SetBackgroundColor(bShowResult
			? VisualStyle.HudAmberColor
			: FLinearColor(
				VisualStyle.HudPanelMutedColor.R,
				VisualStyle.HudPanelMutedColor.G,
				VisualStyle.HudPanelMutedColor.B,
				1.0f));
		if (UCanvasPanelSlot* CloseSlot = Cast<UCanvasPanelSlot>(Button_Close->Slot))
		{
			// One contracted button serves both states: top-right abort while playing,
			// then a prominent CTA visually inside the AAR card after completion.
			CloseSlot->SetAnchors(bShowResult
				? FAnchors(0.435f, 0.665f, 0.565f, 0.720f)
				: FAnchors(0.925f, 0.050f, 0.982f, 0.110f));
			CloseSlot->SetOffsets(FMargin(0.0f));
			CloseSlot->SetAlignment(FVector2D::ZeroVector);
			CloseSlot->SetZOrder(110);
		}
	}

	const bool bPlaying = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	if (Border_PlayArea)
	{
		const bool bReticleUsesPlayCanvas = Text_Reticle
			&& Text_Reticle->GetParent() == Canvas_PlayArea
			&& Cast<UCanvasPanelSlot>(Text_Reticle->Slot) != nullptr;
		Border_PlayArea->SetCursor(bPlaying
			&& bReticleUsesPlayCanvas
			&& !VisualStyle.ReticleGlyph.IsEmpty()
			? EMouseCursor::None
			: EMouseCursor::Default);
	}

	RefreshTargetLayout();
	RefreshFeedbackPresentation();
	RefreshReticlePresentation();
}

void UFF_TargetShootingScreen::RefreshTargetLayout()
{
	if (!Border_Target || !Canvas_PlayArea)
	{
		return;
	}

	const bool bShowTarget = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing
		&& DisplayedSnapshot.bTargetVisible;
	Border_Target->SetVisibility(bShowTarget
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (!bShowTarget)
	{
		Border_Target->SetRenderScale(FVector2D(1.0f, 1.0f));
		Border_Target->SetRenderOpacity(1.0f);
		return;
	}

	UCanvasPanelSlot* TargetSlot = Cast<UCanvasPanelSlot>(Border_Target->Slot);
	if (!TargetSlot)
	{
		return;
	}

	const FVector2D Center(
		FMath::Clamp(DisplayedSnapshot.TargetCenterNormalized.X, 0.0, 1.0),
		FMath::Clamp(DisplayedSnapshot.TargetCenterNormalized.Y, 0.0, 1.0));
	const FVector2D Radius(
		FMath::Clamp(DisplayedSnapshot.TargetRadiusNormalized.X, 0.0, 0.5),
		FMath::Clamp(DisplayedSnapshot.TargetRadiusNormalized.Y, 0.0, 0.5));

	TargetSlot->SetAnchors(FAnchors(Center.X, Center.Y));
	TargetSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TargetSlot->SetPosition(FVector2D::ZeroVector);

	const FVector2D PlayAreaSize = Canvas_PlayArea->GetCachedGeometry().GetLocalSize();
	if (PlayAreaSize.X > UE_SMALL_NUMBER && PlayAreaSize.Y > UE_SMALL_NUMBER)
	{
		TargetSlot->SetSize(FVector2D(
			Radius.X * 2.0 * PlayAreaSize.X,
			Radius.Y * 2.0 * PlayAreaSize.Y));
		LastPlayAreaSize = PlayAreaSize;
	}

	Border_Target->SetBrushColor(DisplayedSnapshot.bBonusTarget
		? VisualStyle.BonusTargetColor
		: VisualStyle.NormalTargetColor);
	if (TargetVisual)
	{
		TargetVisual->Configure(
			DisplayedSnapshot.bBonusTarget
				? VisualStyle.BonusTargetColor
				: VisualStyle.NormalTargetColor,
			VisualStyle.PrecisionColor,
			VisualStyle.BullseyeColor,
			DisplayedSnapshot.PrecisionRadiusRatio,
			DisplayedSnapshot.BullseyeRadiusRatio,
			VisualStyle.TargetRingThickness);
	}
	// Never scale the target art: the runtime judges the exact ellipse represented
	// by this slot. Opacity-only motion prevents visible-but-unclickable edges.
	Border_Target->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Border_Target->SetRenderScale(FVector2D(1.0f, 1.0f));
	float TargetOpacity = 1.0f;
	if (TargetSpawnPulseRemainingSeconds > 0.0f
		&& VisualStyle.TargetSpawnPulseSeconds > UE_SMALL_NUMBER)
	{
		const float SpawnRemainingAlpha = FMath::Clamp(
			TargetSpawnPulseRemainingSeconds / VisualStyle.TargetSpawnPulseSeconds,
			0.0f,
			1.0f);
		const float SpawnProgress = 1.0f - SpawnRemainingAlpha;
		TargetOpacity *= FMath::Lerp(
			FMath::Clamp(VisualStyle.TargetSpawnMinOpacity, 0.0f, 1.0f),
			1.0f,
			SpawnProgress);
	}
	if (DisplayedSnapshot.TargetLifetimeSeconds > UE_SMALL_NUMBER)
	{
		const float RemainingRatio = FMath::Clamp(
			DisplayedSnapshot.TargetRemainingSeconds / DisplayedSnapshot.TargetLifetimeSeconds,
			0.0f,
			1.0f);
		if (RemainingRatio < 0.35f)
		{
			const float UrgencyAlpha = 1.0f - RemainingRatio / 0.35f;
			const float PulseWave = 0.5f + 0.5f * FMath::Sin(
				DisplayedSnapshot.TargetAgeSeconds * 10.0f * UE_PI);
			TargetOpacity *= FMath::Lerp(
				1.0f,
				FMath::Clamp(VisualStyle.TargetUrgencyPulseMinOpacity, 0.0f, 1.0f),
				UrgencyAlpha * PulseWave);
		}
	}
	Border_Target->SetRenderOpacity(FMath::Clamp(TargetOpacity, 0.0f, 1.0f));
}

void UFF_TargetShootingScreen::RefreshFeedbackPresentation()
{
	if (!Text_ShotFeedback)
	{
		return;
	}

	const bool bShowFeedback = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing
		&& PresentedFeedbackSerial > 0
		&& FeedbackRemainingSeconds > 0.0f
		&& DisplayedSnapshot.LatestFeedbackType != EFF_TargetShootingFeedbackType::None;
	Text_ShotFeedback->SetVisibility(bShowFeedback
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (!bShowFeedback)
	{
		Text_ShotFeedback->SetRenderOpacity(0.0f);
		Text_ShotFeedback->SetRenderScale(FVector2D(1.0f, 1.0f));
		if (Border_PlayArea && bHasBasePlayAreaBrushColor)
		{
			Border_PlayArea->SetBrushColor(BasePlayAreaBrushColor);
		}
		return;
	}

	const float DurationSeconds = FMath::Max(
		VisualStyle.FeedbackDurationSeconds,
		UE_SMALL_NUMBER);
	const float FeedbackAlpha = FMath::Clamp(
		FeedbackRemainingSeconds / DurationSeconds,
		0.0f,
		1.0f);
	const FLinearColor FeedbackColor = GetFeedbackColor(DisplayedSnapshot.LatestFeedbackType);
	Text_ShotFeedback->SetText(MakeFeedbackText());
	Text_ShotFeedback->SetColorAndOpacity(FSlateColor(FeedbackColor));
	Text_ShotFeedback->SetRenderOpacity(FMath::Clamp(FeedbackAlpha * 1.55f, 0.0f, 1.0f));
	Text_ShotFeedback->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	const float FeedbackScale = FMath::Lerp(0.92f, 1.22f, FeedbackAlpha);
	Text_ShotFeedback->SetRenderScale(FVector2D(FeedbackScale, FeedbackScale));

	if (UCanvasPanelSlot* FeedbackSlot = Cast<UCanvasPanelSlot>(Text_ShotFeedback->Slot))
	{
		const FVector2D FeedbackPoint(
			FMath::Clamp(DisplayedSnapshot.LatestFeedbackPointNormalized.X, 0.0, 1.0),
			FMath::Clamp(DisplayedSnapshot.LatestFeedbackPointNormalized.Y, 0.0, 1.0));
		// Keep the score callout clear of the authoritative target/reticle. Near the
		// top edge it travels downward; everywhere else it rises above the hit.
		const float FeedbackDirection = FeedbackPoint.Y < 0.24f ? 1.0f : -1.0f;
		const float FeedbackOffset = 110.0f
			+ FMath::Max(0.0f, VisualStyle.FeedbackRisePixels) * (1.0f - FeedbackAlpha);
		FeedbackSlot->SetAnchors(FAnchors(FeedbackPoint.X, FeedbackPoint.Y));
		FeedbackSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FeedbackSlot->SetPosition(FVector2D(0.0f, FeedbackDirection * FeedbackOffset));
	}

	if (Border_PlayArea && bHasBasePlayAreaBrushColor)
	{
		const float FlashAlpha = FMath::Clamp(
			VisualStyle.PlayAreaFlashStrength * FeedbackAlpha,
			0.0f,
			0.5f);
		FLinearColor FlashColor = FLinearColor::LerpUsingHSV(
			BasePlayAreaBrushColor,
			FeedbackColor,
			FlashAlpha);
		FlashColor.A = BasePlayAreaBrushColor.A;
		Border_PlayArea->SetBrushColor(FlashColor);
	}
}

void UFF_TargetShootingScreen::RefreshReticlePresentation()
{
	UCanvasPanelSlot* ReticleSlot = Text_Reticle
		? Cast<UCanvasPanelSlot>(Text_Reticle->Slot)
		: nullptr;
	if (!Text_Reticle || Text_Reticle->GetParent() != Canvas_PlayArea || !ReticleSlot)
	{
		if (Text_Reticle)
		{
			Text_Reticle->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const bool bPlaying = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	const bool bShowReticle = bPlaying
		&& bReticleInsidePlayArea
		&& !VisualStyle.ReticleGlyph.IsEmpty();
	Text_Reticle->SetVisibility(bShowReticle
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (!bShowReticle)
	{
		Text_Reticle->SetRenderScale(FVector2D(1.0f, 1.0f));
		return;
	}

	Text_Reticle->SetText(FText::FromString(VisualStyle.ReticleGlyph));
	const bool bMissFlash = FeedbackRemainingSeconds > 0.0f
		&& DisplayedSnapshot.LatestFeedbackType == EFF_TargetShootingFeedbackType::Miss;
	Text_Reticle->SetColorAndOpacity(FSlateColor(
		bMissFlash ? VisualStyle.MissColor : VisualStyle.ReticleColor));
	ReticleSlot->SetAnchors(FAnchors(
		FMath::Clamp(ReticlePointNormalized.X, 0.0, 1.0),
		FMath::Clamp(ReticlePointNormalized.Y, 0.0, 1.0)));
	ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ReticleSlot->SetPosition(FVector2D::ZeroVector);

	float ReticleScale = 1.0f;
	if (ReticleShotPulseRemainingSeconds > 0.0f
		&& VisualStyle.ReticleShotPulseSeconds > UE_SMALL_NUMBER)
	{
		const float PulseAlpha = FMath::Clamp(
			ReticleShotPulseRemainingSeconds / VisualStyle.ReticleShotPulseSeconds,
			0.0f,
			1.0f);
		ReticleScale = FMath::Lerp(
			1.0f,
			FMath::Max(1.0f, VisualStyle.ReticleShotPulseScale),
			PulseAlpha);
	}
	Text_Reticle->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Text_Reticle->SetRenderScale(FVector2D(ReticleScale, ReticleScale));
}

FLinearColor UFF_TargetShootingScreen::GetFeedbackColor(
	const EFF_TargetShootingFeedbackType FeedbackType) const
{
	if (DisplayedSnapshot.bLatestFeedbackWasBonusTarget
		&& FeedbackType != EFF_TargetShootingFeedbackType::Miss
		&& FeedbackType != EFF_TargetShootingFeedbackType::Escaped)
	{
		return VisualStyle.BonusTargetColor;
	}

	switch (FeedbackType)
	{
	case EFF_TargetShootingFeedbackType::Bullseye:
		return VisualStyle.BullseyeColor;
	case EFF_TargetShootingFeedbackType::Precision:
		return VisualStyle.PrecisionColor;
	case EFF_TargetShootingFeedbackType::Hit:
		return VisualStyle.HitColor;
	case EFF_TargetShootingFeedbackType::Miss:
		return VisualStyle.MissColor;
	case EFF_TargetShootingFeedbackType::Escaped:
		return VisualStyle.EscapedColor;
	default:
		return FLinearColor::White;
	}
}

FText UFF_TargetShootingScreen::MakeFeedbackText() const
{
	if (DisplayedSnapshot.LatestFeedbackType == EFF_TargetShootingFeedbackType::Escaped)
	{
		return LOCTEXT("EscapedFeedback", "TOO SLOW\nCOMBO LOST");
	}
	if (DisplayedSnapshot.LatestFeedbackType == EFF_TargetShootingFeedbackType::Miss)
	{
		return DisplayedSnapshot.LatestScoreDelta < 0
			? FText::Format(
				LOCTEXT("MissPenaltyFeedback", "MISS  {0}\nCOMBO LOST"),
				FText::AsNumber(DisplayedSnapshot.LatestScoreDelta))
			: LOCTEXT("MissFeedback", "MISS\nCOMBO LOST");
	}

	FText QualityText;
	switch (DisplayedSnapshot.LatestFeedbackType)
	{
	case EFF_TargetShootingFeedbackType::Bullseye:
		QualityText = LOCTEXT("BullseyeFeedback", "BULLSEYE");
		break;
	case EFF_TargetShootingFeedbackType::Precision:
		QualityText = LOCTEXT("PrecisionFeedback", "PRECISION");
		break;
	default:
		QualityText = LOCTEXT("HitFeedback", "HIT");
		break;
	}
	if (DisplayedSnapshot.bLatestFeedbackWasBonusTarget)
	{
		QualityText = FText::Format(
			LOCTEXT("GoldenFeedback", "GOLD {0}"),
			QualityText);
	}

	return DisplayedSnapshot.CurrentCombo >= 2
		? FText::Format(
			LOCTEXT("ScoredComboFeedback", "{0}  +{1}\nCOMBO x{2}"),
			QualityText,
			FText::AsNumber(FMath::Max(0, DisplayedSnapshot.LatestScoreDelta)),
			FText::AsNumber(DisplayedSnapshot.CurrentCombo))
		: FText::Format(
			LOCTEXT("ScoredFeedback", "{0}  +{1}"),
			QualityText,
			FText::AsNumber(FMath::Max(0, DisplayedSnapshot.LatestScoreDelta)));
}

bool UFF_TargetShootingScreen::TryMakeNormalizedPoint(
	const FVector2D& ScreenSpacePoint,
	FVector2D& OutNormalizedPoint) const
{
	if (!Canvas_PlayArea)
	{
		return false;
	}

	// Target, reticle and feedback all anchor to this canvas. Using its geometry
	// keeps pointer normalization exact even when a designer changes border padding.
	const FGeometry& PlayGeometry = Canvas_PlayArea->GetCachedGeometry();
	const FVector2D LocalSize = PlayGeometry.GetLocalSize();
	if (LocalSize.X <= UE_SMALL_NUMBER || LocalSize.Y <= UE_SMALL_NUMBER)
	{
		return false;
	}

	const FVector2D LocalPoint = PlayGeometry.AbsoluteToLocal(ScreenSpacePoint);
	if (!FMath::IsFinite(LocalPoint.X) || !FMath::IsFinite(LocalPoint.Y)
		|| LocalPoint.X < 0.0 || LocalPoint.Y < 0.0
		|| LocalPoint.X > LocalSize.X || LocalPoint.Y > LocalSize.Y)
	{
		return false;
	}

	OutNormalizedPoint = FVector2D(
		LocalPoint.X / LocalSize.X,
		LocalPoint.Y / LocalSize.Y);
	return FMath::IsFinite(OutNormalizedPoint.X)
		&& FMath::IsFinite(OutNormalizedPoint.Y);
}

void UFF_TargetShootingScreen::HandleCloseClicked()
{
	RequestClose();
}

#undef LOCTEXT_NAMESPACE
