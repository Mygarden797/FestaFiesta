// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_SurfingScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Minigame/FF_SurfingRuntime.h"
#include "UI/FF_UILog.h"
#include "UI/Minigame/FF_SurfingSceneActor.h"

#define LOCTEXT_NAMESPACE "FFSurfingScreen"

namespace
{
	constexpr TCHAR SurfingBackdropPath[] =
		TEXT("/Game/FestaFiesta/UI/Minigame/AAA/T_MG_Surfing_Backdrop_AAA.T_MG_Surfing_Backdrop_AAA");

	void StyleSurfingNativeText(
		UTextBlock* TextBlock,
		const float FontSize,
		const FLinearColor& Color = FLinearColor::White)
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
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetShadowOffset(FVector2D(1.5f, 2.0f));
		TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.02f, 0.04f, 0.88f));
	}

	UCanvasPanelSlot* AddStretchedToCanvas(
		UCanvasPanel& Canvas,
		UWidget& Widget,
		const FAnchors& Anchors,
		const int32 ZOrder = 0)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Widget);
		if (Slot)
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(FMargin(0.0f));
			Slot->SetZOrder(ZOrder);
		}
		return Slot;
	}

	UCanvasPanelSlot* AddAutoSizedToCanvas(
		UCanvasPanel& Canvas,
		UWidget& Widget,
		const FVector2D& Anchor,
		const FVector2D& Alignment,
		const int32 ZOrder = 0)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Widget);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(FVector2D::ZeroVector);
			Slot->SetAutoSize(true);
			Slot->SetZOrder(ZOrder);
		}
		return Slot;
	}

	UCanvasPanelSlot* AddSizedToCanvas(
		UCanvasPanel& Canvas,
		UWidget& Widget,
		const FVector2D& Anchor,
		const FVector2D& Alignment,
		const FVector2D& Size,
		const int32 ZOrder = 0)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Widget);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(FVector2D::ZeroVector);
			Slot->SetSize(Size);
			Slot->SetAutoSize(false);
			Slot->SetZOrder(ZOrder);
		}
		return Slot;
	}

	UTextBlock* MakeNativeText(
		UWidgetTree& Tree,
		const FName Name,
		const FText& Text,
		const float FontSize,
		const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* TextBlock = Tree.ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), Name);
		if (TextBlock)
		{
			TextBlock->SetText(Text);
			TextBlock->SetJustification(ETextJustify::Center);
			TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
			StyleSurfingNativeText(TextBlock, FontSize, Color);
		}
		return TextBlock;
	}

	bool IsLeftMovementKey(const FKey& Key)
	{
		return Key == EKeys::A
			|| Key == EKeys::Left
			|| Key == EKeys::Gamepad_DPad_Left;
	}

	bool IsRightMovementKey(const FKey& Key)
	{
		return Key == EKeys::D
			|| Key == EKeys::Right
			|| Key == EKeys::Gamepad_DPad_Right;
	}

	bool IsUpMovementKey(const FKey& Key)
	{
		return Key == EKeys::W
			|| Key == EKeys::Up
			|| Key == EKeys::Gamepad_DPad_Up;
	}

	bool IsDownMovementKey(const FKey& Key)
	{
		return Key == EKeys::S
			|| Key == EKeys::Down
			|| Key == EKeys::Gamepad_DPad_Down;
	}

	bool IsMovementKey(const FKey& Key)
	{
		return IsLeftMovementKey(Key)
			|| IsRightMovementKey(Key)
			|| IsUpMovementKey(Key)
			|| IsDownMovementKey(Key);
	}

	bool IsBoostKey(const FKey& Key)
	{
		return Key == EKeys::SpaceBar
			|| Key == EKeys::Gamepad_FaceButton_Bottom;
	}
}

UFF_SurfingScreen::UFF_SurfingScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VisualStyle.BackdropTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(SurfingBackdropPath));
}

bool UFF_SurfingScreen::SetRuntime(UFF_SurfingRuntime* InRuntime)
{
	if (!IsValid(InRuntime))
	{
		ReleaseAllMovementInput();
		Runtime = nullptr;
		SetSessionId(FGuid());
		ResetTransientPresentationState();
		Release3DScene();
		RefreshPresentation();
		return false;
	}

	const FFF_SurfingSnapshot Snapshot = InRuntime->GetSurfingSnapshot();
	const FGuid RuntimeSessionId = Snapshot.Session.Context.SessionId;
	if (!RuntimeSessionId.IsValid())
	{
		return false;
	}

	if (GetSessionId().IsValid() && GetSessionId() != RuntimeSessionId)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 다른 세션의 Surfing runtime 연결을 거부했다. Screen=%s Runtime=%s"),
			*GetName(), *GetSessionId().ToString(), *RuntimeSessionId.ToString());
		return false;
	}

	if (!GetSessionId().IsValid())
	{
		SetSessionId(RuntimeSessionId);
	}

	Runtime = InRuntime;
	bHasResult = false;
	ApplySnapshot(Snapshot);
	return true;
}

bool UFF_SurfingScreen::BindRuntime(UFF_MinigameRuntime* InRuntime)
{
	if (WidgetTree && WidgetTree->RootWidget)
	{
		ResolveBindingsFromWidgetTree();
		if (!HasUsableDesignerBindings())
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] Surfing WBP의 필수 위젯 또는 Canvas anchor 계약이 올바르지 않다."),
				*GetName());
			return false;
		}
	}
	return SetRuntime(Cast<UFF_SurfingRuntime>(InRuntime));
}

void UFF_SurfingScreen::ShowResult(const FFF_MinigameResult& Result)
{
	if (GetSessionId().IsValid() && Result.SessionId != GetSessionId())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 다른 세션의 Surfing 결과 표시를 무시했다. Screen=%s Result=%s"),
			*GetName(), *GetSessionId().ToString(), *Result.SessionId.ToString());
		return;
	}

	ReleaseAllMovementInput();
	if (IsValid(Runtime))
	{
		ApplySnapshot(Runtime->GetSurfingSnapshot());
	}
	DisplayedResult = Result;
	bHasResult = true;
	RefreshPresentation();

	if (Button_Close && GetOwningLocalPlayer())
	{
		InitialFocusTarget = Button_Close;
		Button_Close->SetFocus();
	}
}

void UFF_SurfingScreen::ApplySnapshot(const FFF_SurfingSnapshot& Snapshot)
{
	const FGuid SnapshotSessionId = Snapshot.Session.Context.SessionId;
	if (GetSessionId().IsValid()
		&& (!SnapshotSessionId.IsValid() || SnapshotSessionId != GetSessionId()))
	{
		return;
	}

	if (Snapshot.LatestEventSerial > 0
		&& Snapshot.LatestEventSerial != PresentedEventSerial)
	{
		PresentedEventSerial = Snapshot.LatestEventSerial;
		PresentedEventType = Snapshot.LatestEventType;
		EventPulseRemainingSeconds = FMath::Max(0.0f, VisualStyle.EventPulseSeconds);
	}

	DisplayedSnapshot = Snapshot;
	bHasSnapshot = true;
	RefreshPresentation();
}

TSharedRef<SWidget> UFF_SurfingScreen::RebuildWidget()
{
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		if (!BuildNativeWidgetTree())
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] Surfing native fallback WidgetTree 생성에 실패했다."), *GetName());
		}
	}

	ResolveBindingsFromWidgetTree();
	bUsingDesignerWidgetTree = !bBuiltNativeWidgetTree && HasUsableDesignerBindings();
	InitializeBindings();
	RefreshPresentation();
	return Super::RebuildWidget();
}

void UFF_SurfingScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ResolveBindingsFromWidgetTree();
	InitializeBindings();
	RefreshPresentation();
}

void UFF_SurfingScreen::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveBindingsFromWidgetTree();
	InitializeBindings();
	Initialize3DScene();
	RefreshPresentation();
}

void UFF_SurfingScreen::NativeDestruct()
{
	ReleaseAllMovementInput();
	Release3DScene();
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveAll(this);
	}

	TSet<UBorder*> DynamicEntityWidgets;
	for (const TPair<int32, TObjectPtr<UBorder>>& Pair : EntityWidgetsById)
	{
		if (IsValid(Pair.Value))
		{
			DynamicEntityWidgets.Add(Pair.Value);
		}
	}
	for (const TObjectPtr<UBorder>& Widget : RecycledEntityWidgets)
	{
		if (IsValid(Widget))
		{
			DynamicEntityWidgets.Add(Widget);
		}
	}
	if (Canvas_EntityLayer)
	{
		for (UBorder* Widget : DynamicEntityWidgets)
		{
			Canvas_EntityLayer->RemoveChild(Widget);
		}
	}

	EntityWidgetsById.Reset();
	RecycledEntityWidgets.Reset();
	ResetTransientPresentationState();
	InitialFocusTarget = nullptr;
	Runtime = nullptr;
	SetSessionId(FGuid());
	bBindingsInitialized = false;
	Super::NativeDestruct();
}

void UFF_SurfingScreen::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	ReleaseAllMovementInput();
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
}

void UFF_SurfingScreen::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bUsing3DScene
		&& (!IsValid(SurfingSceneActor) || !SurfingSceneActor->IsSceneReady()))
	{
		Release3DScene();
	}

	if (IsValid(Runtime))
	{
		ApplySnapshot(Runtime->GetSurfingSnapshot());
	}
	else if (bUsing3DScene && IsValid(SurfingSceneActor) && bHasSnapshot)
	{
		SurfingSceneActor->ApplySnapshot(DisplayedSnapshot);
	}
	else
	{
		RefreshArenaLayout();
		RefreshEntityWidgets();
	}

	EventPulseRemainingSeconds = FMath::Max(
		0.0f,
		EventPulseRemainingSeconds - FMath::Max(0.0f, InDeltaTime));
	RefreshFocusPresentation();
}

FReply UFF_SurfingScreen::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bGameplayInputEnabled = IsValid(Runtime)
		&& bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	if (!bGameplayInputEnabled && (IsMovementKey(Key) || IsBoostKey(Key)))
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}

	if (IsMovementKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			if (IsLeftMovementKey(Key))
			{
				bLeftKeyHeld = true;
			}
			if (IsRightMovementKey(Key))
			{
				bRightKeyHeld = true;
			}
			if (IsUpMovementKey(Key))
			{
				bUpKeyHeld = true;
			}
			if (IsDownMovementKey(Key))
			{
				bDownKeyHeld = true;
			}
			UpdateHeldMovementInput();
		}
		return FReply::Handled();
	}

	if (IsBoostKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			TryBoost();
		}
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UFF_SurfingScreen::NativeOnKeyUp(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (IsMovementKey(Key))
	{
		if (IsLeftMovementKey(Key))
		{
			bLeftKeyHeld = false;
		}
		if (IsRightMovementKey(Key))
		{
			bRightKeyHeld = false;
		}
		if (IsUpMovementKey(Key))
		{
			bUpKeyHeld = false;
		}
		if (IsDownMovementKey(Key))
		{
			bDownKeyHeld = false;
		}
		UpdateHeldMovementInput();
		return FReply::Handled();
	}

	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

FReply UFF_SurfingScreen::NativeOnAnalogValueChanged(
	const FGeometry& InGeometry,
	const FAnalogInputEvent& InAnalogInputEvent)
{
	const FKey Key = InAnalogInputEvent.GetKey();
	if (Key != EKeys::Gamepad_LeftX && Key != EKeys::Gamepad_LeftY)
	{
		return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
	}

	if (!IsValid(Runtime)
		|| !bHasSnapshot
		|| bHasResult
		|| DisplayedSnapshot.Session.State != EFF_MinigameSessionState::Playing)
	{
		return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
	}

	const float Value = FMath::IsFinite(InAnalogInputEvent.GetAnalogValue())
		? InAnalogInputEvent.GetAnalogValue()
		: 0.0f;
	if (Key == EKeys::Gamepad_LeftX)
	{
		AnalogMovementInput.X = Value;
	}
	else
	{
		AnalogMovementInput.Y = Value;
	}
	UpdateHeldMovementInput();
	return FReply::Handled();
}

FReply UFF_SurfingScreen::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (!IsValid(Runtime)
		|| !bHasSnapshot
		|| bHasResult
		|| DisplayedSnapshot.Session.State != EFF_MinigameSessionState::Playing)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FVector2D LocalSize = InGeometry.GetLocalSize();
	if (LocalSize.X <= UE_SMALL_NUMBER || LocalSize.Y <= UE_SMALL_NUMBER)
	{
		return FReply::Handled();
	}

	const FVector2D ClickLocal = InGeometry.AbsoluteToLocal(
		InMouseEvent.GetScreenSpacePosition());
	const FVector2D ClickNormalized(
		ClickLocal.X / LocalSize.X,
		ClickLocal.Y / LocalSize.Y);
	const FVector2D PlayerPosition(
		FMath::Clamp(DisplayedSnapshot.PlayerPositionNormalized.X, -1.0f, 1.0f),
		FMath::Clamp(DisplayedSnapshot.PlayerPositionNormalized.Y, -1.0f, 1.0f));
	if (bUsing3DScene && IsValid(SurfingSceneActor) && Border_SurfBackdrop)
	{
		const FGeometry& CaptureGeometry = Border_SurfBackdrop->GetCachedGeometry();
		const FVector2D CaptureSize = CaptureGeometry.GetLocalSize();
		if (CaptureSize.X <= UE_SMALL_NUMBER || CaptureSize.Y <= UE_SMALL_NUMBER)
		{
			return FReply::Handled();
		}

		const FVector2D CaptureLocal = CaptureGeometry.AbsoluteToLocal(
			InMouseEvent.GetScreenSpacePosition());
		if (CaptureLocal.X < 0.0 || CaptureLocal.Y < 0.0
			|| CaptureLocal.X > CaptureSize.X || CaptureLocal.Y > CaptureSize.Y)
		{
			return FReply::Handled();
		}

		const FVector2D CaptureNormalized(
			CaptureLocal.X / CaptureSize.X,
			CaptureLocal.Y / CaptureSize.Y);
		FVector2D TargetArenaPosition;
		if (SurfingSceneActor->DeprojectCapturePointToArena(
			CaptureNormalized, TargetArenaPosition))
		{
			FVector2D ArenaDirection = TargetArenaPosition - PlayerPosition;
			if (!ArenaDirection.IsNearlyZero())
			{
				ArenaDirection.Normalize();
				TrySetMovementInput(ArenaDirection);
			}
			return FReply::Handled();
		}
		return FReply::Handled();
	}

	const FVector2D ArenaHalfExtents = GetProjectedArenaHalfExtents(LocalSize);
	const FVector2D PlayerAnchor(
		VisualStyle.ArenaCenter.X + PlayerPosition.X * ArenaHalfExtents.X,
		VisualStyle.ArenaCenter.Y - PlayerPosition.Y * ArenaHalfExtents.Y);
	FVector2D Direction(
		(ClickNormalized.X - PlayerAnchor.X) / FMath::Max(ArenaHalfExtents.X, 0.01f),
		(PlayerAnchor.Y - ClickNormalized.Y) / FMath::Max(ArenaHalfExtents.Y, 0.01f));
	if (!Direction.IsNearlyZero())
	{
		Direction.Normalize();
		TrySetMovementInput(Direction);
	}
	return FReply::Handled();
}

UWidget* UFF_SurfingScreen::NativeGetDesiredFocusTarget() const
{
	if ((bHasResult
		|| (bHasSnapshot
			&& DisplayedSnapshot.Session.State != EFF_MinigameSessionState::Playing))
		&& Button_Close)
	{
		return Button_Close;
	}

	return const_cast<UFF_SurfingScreen*>(this);
}

bool UFF_SurfingScreen::BuildNativeWidgetTree()
{
	if (!WidgetTree)
	{
		return false;
	}

	bBuiltNativeWidgetTree = true;
	bUsingDesignerWidgetTree = false;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("Overlay_Root"));
	Canvas_SurfArea = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("Canvas_SurfArea"));
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_SurfBackdrop"));
	if (!Root || !Canvas_SurfArea || !Backdrop)
	{
		return false;
	}

	WidgetTree->RootWidget = Root;
	Canvas_SurfArea->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	if (UOverlaySlot* SurfSlot = Root->AddChildToOverlay(Canvas_SurfArea))
	{
		SurfSlot->SetHorizontalAlignment(HAlign_Fill);
		SurfSlot->SetVerticalAlignment(VAlign_Fill);
	}

	Backdrop->SetPadding(FMargin(0.0f));
	Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UTexture2D* BackdropTexture = VisualStyle.BackdropTexture.IsNull()
		? nullptr
		: VisualStyle.BackdropTexture.LoadSynchronous())
	{
		FSlateBrush BackdropBrush;
		BackdropBrush.DrawAs = ESlateBrushDrawType::Image;
		BackdropBrush.SetResourceObject(BackdropTexture);
		BackdropBrush.TintColor = FSlateColor(VisualStyle.BackdropTint);
		Backdrop->SetBrush(BackdropBrush);
	}
	else
	{
		Backdrop->SetBrushColor(VisualStyle.OceanColor);
	}
	AddStretchedToCanvas(
		*Canvas_SurfArea, *Backdrop, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 0);

	UBorder* TopVignette = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_TopVignette"));
	UBorder* BottomVignette = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_BottomVignette"));
	UBorder* ArenaShadow = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ArenaShadow"));
	UBorder* ArenaGlass = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ArenaGlass"));
	if (!TopVignette || !BottomVignette || !ArenaShadow || !ArenaGlass)
	{
		return false;
	}
	TopVignette->SetBrushColor(FLinearColor(0.002f, 0.018f, 0.045f, 0.76f));
	BottomVignette->SetBrushColor(FLinearColor(0.002f, 0.020f, 0.050f, 0.72f));
	ArenaShadow->SetBrushColor(FLinearColor(0.001f, 0.014f, 0.033f, 0.30f));
	ArenaGlass->SetBrushColor(VisualStyle.ArenaScrimColor);
	for (UBorder* Decorative : { TopVignette, BottomVignette, ArenaShadow, ArenaGlass })
	{
		Decorative->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	AddStretchedToCanvas(
		*Canvas_SurfArea, *TopVignette, FAnchors(0.0f, 0.0f, 1.0f, 0.155f), 2);
	AddStretchedToCanvas(
		*Canvas_SurfArea, *BottomVignette, FAnchors(0.0f, 0.84f, 1.0f, 1.0f), 2);
	AddStretchedToCanvas(
		*Canvas_SurfArea, *ArenaShadow, FAnchors(0.055f, 0.145f, 0.945f, 0.875f), 4);
	AddStretchedToCanvas(
		*Canvas_SurfArea, *ArenaGlass, FAnchors(0.060f, 0.150f, 0.940f, 0.870f), 5);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UBorder* WakeLine = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("Border_WakeGuide_%02d"), Index + 1));
		if (!WakeLine)
		{
			return false;
		}
		FLinearColor WakeColor = VisualStyle.FoamColor;
		WakeColor.A = 0.10f + static_cast<float>(Index % 2) * 0.06f;
		WakeLine->SetBrushColor(WakeColor);
		WakeLine->SetVisibility(ESlateVisibility::HitTestInvisible);
		const float Top = 0.265f + static_cast<float>(Index) * 0.155f;
		AddStretchedToCanvas(
			*Canvas_SurfArea,
			*WakeLine,
			FAnchors(0.085f + 0.015f * (Index % 2), Top, 0.915f, Top + 0.004f),
			6);
	}

	Canvas_EntityLayer = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("Canvas_EntityLayer"));
	if (!Canvas_EntityLayer)
	{
		return false;
	}
	Canvas_EntityLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	Canvas_EntityLayer->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	AddStretchedToCanvas(
		*Canvas_SurfArea,
		*Canvas_EntityLayer,
		FAnchors(0.0f, 0.0f, 1.0f, 1.0f),
		20);

	Border_SurferAvatar = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_SurferAvatar"));
	Border_SurferCore = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_SurferCore"));
	UVerticalBox* SurferColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VerticalBox_SurferAvatar"));
	UTextBlock* RiderMark = MakeNativeText(
		*WidgetTree,
		TEXT("Text_SurferRider"),
		LOCTEXT("NativeRiderMark", "▲"),
		22.0f,
		FLinearColor(0.015f, 0.06f, 0.09f, 1.0f));
	UTextBlock* SurferLabel = MakeNativeText(
		*WidgetTree,
		TEXT("Text_SurfboardMark"),
		LOCTEXT("NativeSurfer", "RIDER"),
		12.0f,
		FLinearColor(0.015f, 0.06f, 0.09f, 1.0f));
	if (!Border_SurferAvatar || !Border_SurferCore || !SurferColumn
		|| !RiderMark || !SurferLabel)
	{
		return false;
	}
	Border_SurferAvatar->SetBrushColor(VisualStyle.EntityHaloColor);
	Border_SurferAvatar->SetPadding(FMargin(5.0f));
	Border_SurferAvatar->SetContent(Border_SurferCore);
	Border_SurferAvatar->SetVisibility(ESlateVisibility::HitTestInvisible);
	Border_SurferCore->SetBrushColor(VisualStyle.SurferColor);
	Border_SurferCore->SetPadding(FMargin(14.0f, 9.0f));
	Border_SurferCore->SetContent(SurferColumn);
	SurferColumn->AddChildToVerticalBox(RiderMark);
	SurferColumn->AddChildToVerticalBox(SurferLabel);
	AddAutoSizedToCanvas(
		*Canvas_SurfArea,
		*Border_SurferAvatar,
		VisualStyle.ArenaCenter,
		FVector2D(0.5f, 0.5f),
		100);

	UBorder* IdentityCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_GameIdentity"));
	UVerticalBox* IdentityColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VerticalBox_GameIdentity"));
	UTextBlock* IdentityEyebrow = MakeNativeText(
		*WidgetTree, TEXT("Text_IdentityEyebrow"),
		LOCTEXT("IdentityEyebrow", "FESTA SPORTS  //  LIVE"), 11.0f,
		VisualStyle.AmberAccentColor);
	UTextBlock* IdentityTitle = MakeNativeText(
		*WidgetTree, TEXT("Text_IdentityTitle"),
		LOCTEXT("IdentityTitle", "TIDAL RUN"), 23.0f, FLinearColor::White);
	UTextBlock* IdentitySubtitle = MakeNativeText(
		*WidgetTree, TEXT("Text_IdentitySubtitle"),
		LOCTEXT("IdentitySubtitle", "DADAEPO  /  FREE RIDE"), 10.0f,
		VisualStyle.MintAccentColor);
	if (!IdentityCard || !IdentityColumn || !IdentityEyebrow
		|| !IdentityTitle || !IdentitySubtitle)
	{
		return false;
	}
	IdentityCard->SetBrushColor(VisualStyle.HudPanelColor);
	IdentityCard->SetPadding(FMargin(18.0f, 10.0f));
	IdentityCard->SetContent(IdentityColumn);
	IdentityCard->SetVisibility(ESlateVisibility::HitTestInvisible);
	for (UTextBlock* IdentityText : { IdentityEyebrow, IdentityTitle, IdentitySubtitle })
	{
		IdentityText->SetJustification(ETextJustify::Left);
		IdentityColumn->AddChildToVerticalBox(IdentityText);
	}
	AddSizedToCanvas(
		*Canvas_SurfArea, *IdentityCard, FVector2D(0.025f, 0.026f),
		FVector2D(0.0f, 0.0f), FVector2D(330.0f, 98.0f), 150);

	UBorder* HudBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_PillHud"));
	UVerticalBox* TimerColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VerticalBox_TimerCard"));
	UTextBlock* TimerCaption = MakeNativeText(
		*WidgetTree, TEXT("Text_TimerCaption"),
		LOCTEXT("TimerCaption", "ROUND TIMER"), 11.0f, VisualStyle.MintAccentColor);
	Text_RemainingTime = MakeNativeText(
		*WidgetTree, TEXT("Text_RemainingTime"), LOCTEXT("TimePlaceholder", "30"), 48.0f);
	if (!HudBorder || !TimerColumn || !TimerCaption || !Text_RemainingTime)
	{
		return false;
	}
	HudBorder->SetBrushColor(VisualStyle.HudPanelColor);
	HudBorder->SetPadding(FMargin(28.0f, 9.0f));
	HudBorder->SetContent(TimerColumn);
	HudBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	TimerColumn->AddChildToVerticalBox(TimerCaption);
	TimerColumn->AddChildToVerticalBox(Text_RemainingTime);
	AddSizedToCanvas(
		*Canvas_SurfArea, *HudBorder, FVector2D(0.5f, 0.022f),
		FVector2D(0.5f, 0.0f), FVector2D(244.0f, 98.0f), 200);

	UBorder* ScoreCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ScoreCard"));
	UVerticalBox* ScoreColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VerticalBox_ScoreCard"));
	UTextBlock* ScoreCaption = MakeNativeText(
		*WidgetTree, TEXT("Text_ScoreCaption"), LOCTEXT("ScoreCaption", "RUN SCORE"),
		11.0f, VisualStyle.MintAccentColor);
	Text_Score = MakeNativeText(
		*WidgetTree, TEXT("Text_Score"), LOCTEXT("ScorePlaceholder", "0"), 34.0f,
		VisualStyle.AmberAccentColor);
	if (!ScoreCard || !ScoreColumn || !ScoreCaption || !Text_Score)
	{
		return false;
	}
	ScoreCard->SetBrushColor(VisualStyle.HudPanelColor);
	ScoreCard->SetPadding(FMargin(18.0f, 9.0f));
	ScoreCard->SetContent(ScoreColumn);
	ScoreCard->SetVisibility(ESlateVisibility::HitTestInvisible);
	ScoreColumn->AddChildToVerticalBox(ScoreCaption);
	ScoreColumn->AddChildToVerticalBox(Text_Score);
	AddSizedToCanvas(
		*Canvas_SurfArea, *ScoreCard, FVector2D(0.952f, 0.026f),
		FVector2D(1.0f, 0.0f), FVector2D(244.0f, 94.0f), 150);

	UBorder* LivesCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_LivesCombatCard"));
	UVerticalBox* LivesColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VerticalBox_LivesCombatCard"));
	UTextBlock* LivesCaption = MakeNativeText(
		*WidgetTree, TEXT("Text_LivesCaption"), LOCTEXT("LivesCaption", "HULL INTEGRITY"),
		11.0f, VisualStyle.MintAccentColor);
	Text_Lives = MakeNativeText(
		*WidgetTree, TEXT("Text_Lives"), LOCTEXT("LivesPlaceholder", "3 / 3   ■■■"), 25.0f,
		FLinearColor(1.0f, 0.58f, 0.52f, 1.0f));
	UBorder* BoostCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_BoostCombatCard"));
	UVerticalBox* BoostColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VerticalBox_BoostCombatCard"));
	UTextBlock* BoostCaption = MakeNativeText(
		*WidgetTree, TEXT("Text_BoostCaption"), LOCTEXT("BoostCaption", "TURBO CHARGE"),
		11.0f, VisualStyle.MintAccentColor);
	Text_Boost = MakeNativeText(
		*WidgetTree, TEXT("Text_Boost"), LOCTEXT("BoostPlaceholder", "■■■   READY"), 20.0f,
		VisualStyle.MintAccentColor);
	if (!LivesCard || !LivesColumn || !LivesCaption || !Text_Lives
		|| !BoostCard || !BoostColumn || !BoostCaption || !Text_Boost)
	{
		return false;
	}
	LivesCard->SetBrushColor(VisualStyle.HudPanelColor);
	LivesCard->SetPadding(FMargin(18.0f, 10.0f));
	LivesCard->SetContent(LivesColumn);
	LivesCard->SetVisibility(ESlateVisibility::HitTestInvisible);
	LivesColumn->AddChildToVerticalBox(LivesCaption);
	LivesColumn->AddChildToVerticalBox(Text_Lives);
	BoostCard->SetBrushColor(VisualStyle.HudPanelColor);
	BoostCard->SetPadding(FMargin(18.0f, 10.0f));
	BoostCard->SetContent(BoostColumn);
	BoostCard->SetVisibility(ESlateVisibility::HitTestInvisible);
	BoostColumn->AddChildToVerticalBox(BoostCaption);
	BoostColumn->AddChildToVerticalBox(Text_Boost);
	AddSizedToCanvas(
		*Canvas_SurfArea, *LivesCard, FVector2D(0.025f, 0.315f),
		FVector2D(0.0f, 0.0f), FVector2D(236.0f, 82.0f), 150);
	AddSizedToCanvas(
		*Canvas_SurfArea, *BoostCard, FVector2D(0.025f, 0.415f),
		FVector2D(0.0f, 0.0f), FVector2D(236.0f, 82.0f), 150);

	UBorder* ControlsBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ControlsHint"));
	Text_Controls = MakeNativeText(
		*WidgetTree,
		TEXT("Text_Controls"),
		LOCTEXT(
			"ControlsHint",
			"[ WASD / STICK ]  MOVE     [ MOUSE ]  TURN     [ SPACE / A ]  BOOST"),
		13.0f,
		FLinearColor(0.90f, 1.0f, 0.98f, 0.96f));
	if (!ControlsBorder || !Text_Controls)
	{
		return false;
	}
	ControlsBorder->SetBrushColor(FLinearColor(0.006f, 0.035f, 0.072f, 0.91f));
	ControlsBorder->SetPadding(FMargin(22.0f, 10.0f));
	ControlsBorder->SetContent(Text_Controls);
	ControlsBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddSizedToCanvas(
		*Canvas_SurfArea, *ControlsBorder, FVector2D(0.5f, 0.977f),
		FVector2D(0.5f, 1.0f), FVector2D(820.0f, 52.0f), 200);

	Button_Close = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("Button_Close"));
	CloseLabel = MakeNativeText(
		*WidgetTree,
		TEXT("Text_CloseLabel"),
		LOCTEXT("NativeClose", "ESC"),
		12.0f,
		FLinearColor::White);
	if (!Button_Close || !CloseLabel)
	{
		return false;
	}
	Button_Close->SetBackgroundColor(FLinearColor(0.006f, 0.035f, 0.072f, 0.96f));
	Button_Close->SetContent(CloseLabel);
	AddSizedToCanvas(
		*Canvas_SurfArea, *Button_Close, FVector2D(0.982f, 0.028f),
		FVector2D(1.0f, 0.0f), FVector2D(54.0f, 46.0f), 400);

	Border_ResultDimmer = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ResultDimmer"));
	if (!Border_ResultDimmer)
	{
		return false;
	}
	Border_ResultDimmer->SetBrushColor(FLinearColor(0.001f, 0.010f, 0.026f, 0.78f));
	Border_ResultDimmer->SetVisibility(ESlateVisibility::Collapsed);
	AddStretchedToCanvas(
		*Canvas_SurfArea, *Border_ResultDimmer,
		FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 300);

	Panel_Result = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Panel_Result"));
	UOverlay* ResultStage = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("Overlay_ResultStage"));
	USizeBox* ResultCardSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("SizeBox_ResultCard"));
	UBorder* ResultCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ResultCard"));
	UVerticalBox* ResultColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VerticalBox_Result"));
	UTextBlock* ResultEyebrow = MakeNativeText(
		*WidgetTree, TEXT("Text_ResultHeading"),
		LOCTEXT("ResultEyebrow", "TIDAL RUN  //  FINAL TELEMETRY"), 12.0f,
		VisualStyle.MintAccentColor);
	Text_Result = MakeNativeText(
		*WidgetTree,
		TEXT("Text_Result"),
		LOCTEXT("ResultPlaceholder", "RIDE COMPLETE"),
		30.0f,
		VisualStyle.AmberAccentColor);
	UTextBlock* ResultExitHint = MakeNativeText(
		*WidgetTree, TEXT("Text_ResultExitHint"),
		LOCTEXT("ResultExitHint", "아래 CONTINUE 버튼으로 라이드 종료"), 12.0f,
		FLinearColor(0.74f, 0.88f, 0.91f, 0.90f));
	if (!Panel_Result || !ResultStage || !ResultCardSize || !ResultCard
		|| !ResultColumn || !ResultEyebrow || !Text_Result || !ResultExitHint)
	{
		return false;
	}
	Panel_Result->SetBrushColor(FLinearColor::Transparent);
	Panel_Result->SetPadding(FMargin(0.0f));
	Panel_Result->SetContent(ResultStage);
	Panel_Result->SetVisibility(ESlateVisibility::Collapsed);
	ResultStage->SetVisibility(ESlateVisibility::HitTestInvisible);
	ResultCardSize->SetWidthOverride(660.0f);
	ResultCardSize->SetHeightOverride(380.0f);
	ResultCardSize->SetContent(ResultCard);
	ResultCard->SetBrushColor(VisualStyle.HudPanelColor);
	ResultCard->SetPadding(FMargin(56.0f, 44.0f));
	ResultCard->SetContent(ResultColumn);
	ResultCard->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* CardSlot = ResultStage->AddChildToOverlay(ResultCardSize))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* EyebrowSlot = ResultColumn->AddChildToVerticalBox(ResultEyebrow))
	{
		EyebrowSlot->SetHorizontalAlignment(HAlign_Center);
		EyebrowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
	}
	Text_Result->SetAutoWrapText(true);
	Text_Result->SetMinDesiredWidth(540.0f);
	if (UVerticalBoxSlot* ResultSlot = ResultColumn->AddChildToVerticalBox(Text_Result))
	{
		ResultSlot->SetHorizontalAlignment(HAlign_Center);
		ResultSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));
	}
	if (UVerticalBoxSlot* ExitHintSlot = ResultColumn->AddChildToVerticalBox(ResultExitHint))
	{
		ExitHintSlot->SetHorizontalAlignment(HAlign_Center);
	}
	if (UOverlaySlot* ResultOverlaySlot = Root->AddChildToOverlay(Panel_Result))
	{
		ResultOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		ResultOverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}

	return true;
}

void UFF_SurfingScreen::ResolveBindingsFromWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	Canvas_SurfArea = Cast<UCanvasPanel>(
		WidgetTree->FindWidget(TEXT("Canvas_SurfArea")));
	Border_SurfBackdrop = Cast<UBorder>(
		WidgetTree->FindWidget(TEXT("Border_SurfBackdrop")));
	Canvas_EntityLayer = Cast<UCanvasPanel>(
		WidgetTree->FindWidget(TEXT("Canvas_EntityLayer")));
	Border_SurferAvatar = Cast<UBorder>(
		WidgetTree->FindWidget(TEXT("Border_SurferAvatar")));
	Border_SurferCore = Cast<UBorder>(
		WidgetTree->FindWidget(TEXT("Border_SurferCore")));
	Text_RemainingTime = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_RemainingTime")));
	Text_Score = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_Score")));
	Text_Lives = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_Lives")));
	Text_Boost = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_Boost")));
	Text_Controls = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_Controls")));
	Panel_Result = Cast<UBorder>(
		WidgetTree->FindWidget(TEXT("Panel_Result")));
	Border_ResultDimmer = Cast<UBorder>(
		WidgetTree->FindWidget(TEXT("Border_ResultDimmer")));
	Text_Result = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_Result")));
	Button_Close = Cast<UButton>(
		WidgetTree->FindWidget(TEXT("Button_Close")));
	CloseLabel = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_CloseLabel")));

	RefreshBackdropBrush();
}

bool UFF_SurfingScreen::HasUsableDesignerBindings() const
{
	if (!IsValid(Canvas_SurfArea)
		|| !IsValid(Canvas_EntityLayer)
		|| Canvas_EntityLayer->GetParent() != Canvas_SurfArea
		|| !IsValid(Border_SurferAvatar)
		|| !IsValid(Text_RemainingTime)
		|| !IsValid(Text_Score)
		|| !IsValid(Text_Lives)
		|| !IsValid(Text_Boost)
		|| !IsValid(Text_Controls)
		|| !IsValid(Panel_Result)
		|| !IsValid(Text_Result)
		|| !IsValid(Button_Close))
	{
		return false;
	}

	const UCanvasPanelSlot* EntityLayerSlot =
		Cast<UCanvasPanelSlot>(Canvas_EntityLayer->Slot);
	const UCanvasPanelSlot* AvatarSlot =
		Cast<UCanvasPanelSlot>(Border_SurferAvatar->Slot);
	if (!EntityLayerSlot || !AvatarSlot)
	{
		return false;
	}

	const FAnchors EntityAnchors = EntityLayerSlot->GetAnchors();
	const FMargin EntityOffsets = EntityLayerSlot->GetOffsets();
	const bool bEntityLayerFillsArena =
		EntityAnchors.Minimum.Equals(FVector2D::ZeroVector)
		&& EntityAnchors.Maximum.Equals(FVector2D(1.0f, 1.0f))
		&& FMath::IsNearlyZero(EntityOffsets.Left)
		&& FMath::IsNearlyZero(EntityOffsets.Top)
		&& FMath::IsNearlyZero(EntityOffsets.Right)
		&& FMath::IsNearlyZero(EntityOffsets.Bottom);
	const auto* AvatarParent = Border_SurferAvatar->GetParent();
	const bool bAvatarUsesArenaProjection =
		AvatarParent == Canvas_SurfArea || AvatarParent == Canvas_EntityLayer;
	return bEntityLayerFillsArena && bAvatarUsesArenaProjection;
}

void UFF_SurfingScreen::InitializeBindings()
{
	if (bBindingsInitialized || !HasUsableDesignerBindings())
	{
		return;
	}

	Button_Close->OnClicked.RemoveAll(this);
	Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	InitialFocusTarget = NativeGetDesiredFocusTarget();
	bBindingsInitialized = true;
}

bool UFF_SurfingScreen::Initialize3DScene()
{
	if (bUsing3DScene && IsValid(SurfingSceneActor)
		&& SurfingSceneActor->IsSceneReady())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer
		|| !Border_SurfBackdrop)
	{
		Release3DScene();
		return false;
	}

	Release3DScene();
	SurfingRenderTarget = NewObject<UTextureRenderTarget2D>(
		this, UTextureRenderTarget2D::StaticClass(), NAME_None, RF_Transient);
	if (!SurfingRenderTarget)
	{
		Release3DScene();
		return false;
	}

	SurfingRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	SurfingRenderTarget->ClearColor = FLinearColor(0.015f, 0.16f, 0.28f, 1.0f);
	SurfingRenderTarget->bAutoGenerateMips = false;
	SurfingRenderTarget->InitAutoFormat(
		FMath::Clamp(VisualStyle.RenderTargetWidth, 320, 3840),
		FMath::Clamp(VisualStyle.RenderTargetHeight, 180, 2160));
	SurfingRenderTarget->UpdateResourceImmediate(true);

	const uint32 IsolationIndex = GetSessionId().IsValid()
		? GetTypeHash(GetSessionId()) % 128u
		: GetTypeHash(GetFName()) % 128u;
	const FVector IsolatedArenaLocation(
		300000.0 + static_cast<double>(IsolationIndex % 8u) * 18000.0,
		300000.0 + static_cast<double>(IsolationIndex / 8u) * 18000.0,
		100000.0);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwningPlayer();
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.Name = MakeUniqueObjectName(
		World,
		AFF_SurfingSceneActor::StaticClass(),
		TEXT("FF_Surfing3DScene"));
	SurfingSceneActor = World->SpawnActor<AFF_SurfingSceneActor>(
		AFF_SurfingSceneActor::StaticClass(),
		IsolatedArenaLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!IsValid(SurfingSceneActor))
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Surfing 3D scene actor 생성에 실패해 2D 안전 화면을 사용한다."),
			*GetName());
		Release3DScene();
		return false;
	}

	SurfingSceneActor->SetReplicates(false);
	SurfingSceneActor->SetReplicateMovement(false);
	SurfingSceneActor->InitializeScene(VisualStyle, SurfingRenderTarget);
	if (!SurfingSceneActor->IsSceneReady())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Surfing 3D scene resource가 준비되지 않아 2D 안전 화면을 사용한다."),
			*GetName());
		Release3DScene();
		return false;
	}

	Set3DPresentationActive(true);
	if (bHasSnapshot)
	{
		SurfingSceneActor->ApplySnapshot(DisplayedSnapshot);
	}
	return true;
}

void UFF_SurfingScreen::Release3DScene()
{
	Set3DPresentationActive(false);
	if (IsValid(SurfingSceneActor))
	{
		SurfingSceneActor->ReleaseScene();
		SurfingSceneActor->Destroy();
	}
	SurfingSceneActor = nullptr;
	if (SurfingRenderTarget)
	{
		SurfingRenderTarget->ReleaseResource();
	}
	SurfingRenderTarget = nullptr;
	RefreshBackdropBrush();
}

void UFF_SurfingScreen::Set3DPresentationActive(const bool bActive)
{
	bUsing3DScene = bActive
		&& IsValid(SurfingSceneActor)
		&& SurfingSceneActor->IsSceneReady()
		&& IsValid(SurfingRenderTarget);
	if (Canvas_EntityLayer)
	{
		Canvas_EntityLayer->SetVisibility(
			bUsing3DScene
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
	if (Border_SurferAvatar)
	{
		Border_SurferAvatar->SetVisibility(
			bUsing3DScene
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
	RefreshBackdropBrush();
}

void UFF_SurfingScreen::RefreshBackdropBrush()
{
	if (!Border_SurfBackdrop)
	{
		return;
	}

	if (bUsing3DScene && SurfingRenderTarget)
	{
		FSlateBrush CaptureBrush;
		CaptureBrush.DrawAs = ESlateBrushDrawType::Image;
		CaptureBrush.SetResourceObject(SurfingRenderTarget);
		CaptureBrush.SetImageSize(FVector2D(
			static_cast<double>(SurfingRenderTarget->SizeX),
			static_cast<double>(SurfingRenderTarget->SizeY)));
		CaptureBrush.TintColor = FSlateColor(FLinearColor::White);
		Border_SurfBackdrop->SetBrush(CaptureBrush);
		Border_SurfBackdrop->SetBrushColor(FLinearColor::White);
		return;
	}

	if (UTexture2D* BackdropTexture = VisualStyle.BackdropTexture.IsNull()
		? nullptr
		: VisualStyle.BackdropTexture.LoadSynchronous())
	{
		FSlateBrush BackdropBrush;
		BackdropBrush.DrawAs = ESlateBrushDrawType::Image;
		BackdropBrush.SetResourceObject(BackdropTexture);
		BackdropBrush.TintColor = FSlateColor(VisualStyle.BackdropTint);
		Border_SurfBackdrop->SetBrush(BackdropBrush);
		Border_SurfBackdrop->SetBrushColor(FLinearColor::White);
	}
	else
	{
		Border_SurfBackdrop->SetBrush(FSlateBrush());
		Border_SurfBackdrop->SetBrushColor(VisualStyle.OceanColor);
	}
}

void UFF_SurfingScreen::ResetTransientPresentationState()
{
	DisplayedSnapshot = FFF_SurfingSnapshot();
	DisplayedResult = FFF_MinigameResult();
	bHasSnapshot = false;
	bHasResult = false;
	PresentedEventSerial = 0;
	PresentedEventType = EFF_SurfingEventType::None;
	EventPulseRemainingSeconds = 0.0f;
	PresentedEntityHash = 0;
	bHasPresentedEntityHash = false;
}

void UFF_SurfingScreen::RefreshPresentation()
{
	if (!HasUsableDesignerBindings())
	{
		return;
	}

	const float RemainingSeconds = bHasSnapshot
		? FMath::Max(0.0f, DisplayedSnapshot.Session.RemainingSeconds)
		: 0.0f;
	const int32 Score = bHasSnapshot ? DisplayedSnapshot.Session.Score : 0;
	const int32 Lives = bHasSnapshot ? FMath::Max(0, DisplayedSnapshot.Lives) : 0;
	const int32 MaxLives = bHasSnapshot ? FMath::Max(0, DisplayedSnapshot.MaxLives) : 0;
	const int32 BoostCharges = bHasSnapshot ? FMath::Max(0, DisplayedSnapshot.BoostCharges) : 0;
	const int32 MaxBoostCharges = bHasSnapshot
		? FMath::Max(0, DisplayedSnapshot.MaxBoostCharges)
		: 0;
	auto MakeSegments = [](const int32 Filled, const int32 Maximum)
	{
		FString Segments;
		const int32 SegmentCount = FMath::Clamp(Maximum, 0, 6);
		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			Segments += Index < Filled ? TEXT("■") : TEXT("□");
		}
		return FText::FromString(Segments);
	};
	FNumberFormattingOptions OneDecimal;
	OneDecimal.SetMinimumFractionalDigits(1);
	OneDecimal.SetMaximumFractionalDigits(1);

	Text_RemainingTime->SetText(FText::AsNumber(FMath::CeilToInt(RemainingSeconds)));
	Text_RemainingTime->SetColorAndOpacity(FSlateColor(
		RemainingSeconds <= 10.0f
		? VisualStyle.AmberAccentColor
		: FLinearColor::White));
	Text_Score->SetText(FText::AsNumber(Score));
	Text_Score->SetColorAndOpacity(FSlateColor(VisualStyle.AmberAccentColor));
	Text_Lives->SetText(FText::Format(
		LOCTEXT("LivesFormat", "{0} / {1}   {2}"),
		FText::AsNumber(Lives),
		FText::AsNumber(MaxLives),
		MakeSegments(Lives, MaxLives)));
	Text_Lives->SetColorAndOpacity(FSlateColor(
		Lives <= 1
		? VisualStyle.AmberAccentColor
		: FLinearColor(1.0f, 0.58f, 0.52f, 1.0f)));
	if (bHasSnapshot && DisplayedSnapshot.bBoosting)
	{
		Text_Boost->SetText(FText::Format(
			LOCTEXT("BoostingFormat", "{0}  {1}  {2}s"),
			FText::AsNumber(BoostCharges),
			MakeSegments(BoostCharges, MaxBoostCharges),
			FText::AsNumber(
				FMath::Max(0.0f, DisplayedSnapshot.BoostRemainingSeconds),
				&OneDecimal)));
		Text_Boost->SetColorAndOpacity(FSlateColor(VisualStyle.AmberAccentColor));
	}
	else
	{
		Text_Boost->SetText(FText::Format(
			LOCTEXT("BoostChargeFormat", "{0}  {1}  READY"),
			FText::AsNumber(BoostCharges),
			MakeSegments(BoostCharges, MaxBoostCharges)));
		Text_Boost->SetColorAndOpacity(FSlateColor(
			BoostCharges > 0
			? VisualStyle.MintAccentColor
			: FLinearColor(0.48f, 0.60f, 0.63f, 1.0f)));
	}

	const bool bPlaying = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	const ESlateVisibility ControlsVisibility = bPlaying
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	Text_Controls->SetVisibility(ControlsVisibility);
	if (UBorder* ControlsContainer = Cast<UBorder>(Text_Controls->GetParent()))
	{
		ControlsContainer->SetVisibility(ControlsVisibility);
	}

	const bool bSessionFinished = bHasSnapshot
		&& (DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Completed
			|| DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Cancelled);
	const bool bShowResult = bHasResult || bSessionFinished;
	Panel_Result->SetVisibility(
		bShowResult ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (Border_ResultDimmer)
	{
		Border_ResultDimmer->SetVisibility(
			bShowResult ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UCanvasPanelSlot* CloseSlot = Cast<UCanvasPanelSlot>(Button_Close->Slot))
	{
		if (bShowResult)
		{
			CloseSlot->SetAnchors(FAnchors(0.5f, 0.72f));
			CloseSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CloseSlot->SetPosition(FVector2D::ZeroVector);
			CloseSlot->SetSize(FVector2D(220.0f, 58.0f));
			CloseSlot->SetAutoSize(false);
			CloseSlot->SetZOrder(400);
		}
		else
		{
			CloseSlot->SetAnchors(FAnchors(0.982f, 0.028f));
			CloseSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			CloseSlot->SetPosition(FVector2D::ZeroVector);
			CloseSlot->SetSize(FVector2D(54.0f, 46.0f));
			CloseSlot->SetAutoSize(false);
			CloseSlot->SetZOrder(400);
		}
	}
	Button_Close->SetBackgroundColor(
		bShowResult
		? VisualStyle.AmberAccentColor
		: FLinearColor(0.006f, 0.035f, 0.072f, 0.96f));

	if (bHasResult)
	{
		const FText Grade = DisplayedResult.GradeText.IsEmpty()
			? LOCTEXT("SurfComplete", "RIDE COMPLETE")
			: DisplayedResult.GradeText;
		Text_Result->SetText(FText::Format(
			LOCTEXT(
				"ResultWithRewardFormat",
				"{0}\n\nRUN SCORE   {1}\nCOINS   {2}     IMPACTS   {3}\nFESTA GOLD   +{4}"),
			Grade,
			FText::AsNumber(DisplayedResult.Score),
			FText::AsNumber(DisplayedSnapshot.CoinCount),
			FText::AsNumber(DisplayedSnapshot.CollisionCount),
			FText::AsNumber(DisplayedResult.RewardGold)));
	}
	else if (bSessionFinished)
	{
		const FText ResultTitle =
			DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Completed
			? LOCTEXT("TimeExpired", "RIDE COMPLETE")
			: LOCTEXT("SessionCancelled", "RIDE ENDED");
		Text_Result->SetText(FText::Format(
			LOCTEXT("SnapshotResultFormat", "{0}\n\nRUN SCORE   {1}\nCOINS   {2}"),
			ResultTitle,
			FText::AsNumber(Score),
			FText::AsNumber(DisplayedSnapshot.CoinCount)));
	}

	if (CloseLabel)
	{
		CloseLabel->SetText(bShowResult
			? LOCTEXT("NativeResultCloseLabel", "CONTINUE")
			: LOCTEXT("NativePlayingCloseLabel", "ESC"));
		CloseLabel->SetColorAndOpacity(FSlateColor(
			bShowResult
			? FLinearColor(0.015f, 0.045f, 0.070f, 1.0f)
			: FLinearColor::White));
	}

	InitialFocusTarget = bShowResult && Button_Close
		? static_cast<UWidget*>(Button_Close.Get())
		: static_cast<UWidget*>(this);
	if (bUsing3DScene && IsValid(SurfingSceneActor))
	{
		if (bHasSnapshot)
		{
			SurfingSceneActor->ApplySnapshot(DisplayedSnapshot);
		}
	}
	else
	{
		RefreshArenaLayout();
		RefreshEntityWidgets();
	}
	RefreshFocusPresentation();
}

void UFF_SurfingScreen::RefreshFocusPresentation()
{
	if (!Button_Close)
	{
		return;
	}

	const float Scale = Button_Close->HasKeyboardFocus()
		? FMath::Max(1.0f, VisualStyle.FocusedControlScale)
		: 1.0f;
	Button_Close->SetRenderScale(FVector2D(Scale, Scale));
}

void UFF_SurfingScreen::RefreshArenaLayout()
{
	if (bUsing3DScene || !Border_SurferAvatar)
	{
		return;
	}

	const FVector2D Position = bHasSnapshot
		? FVector2D(
			FMath::Clamp(DisplayedSnapshot.PlayerPositionNormalized.X, -1.0f, 1.0f),
			FMath::Clamp(DisplayedSnapshot.PlayerPositionNormalized.Y, -1.0f, 1.0f))
		: FVector2D::ZeroVector;
	const FVector2D LocalSize = GetCachedGeometry().GetLocalSize();
	const FVector2D ArenaHalfExtents = GetProjectedArenaHalfExtents(LocalSize);
	if (UCanvasPanelSlot* SurferSlot = Cast<UCanvasPanelSlot>(Border_SurferAvatar->Slot))
	{
		const FVector2D Anchor(
			VisualStyle.ArenaCenter.X + Position.X * ArenaHalfExtents.X,
			VisualStyle.ArenaCenter.Y - Position.Y * ArenaHalfExtents.Y);
		SurferSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		SurferSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SurferSlot->SetPosition(FVector2D::ZeroVector);
		SurferSlot->SetAutoSize(true);
	}

	const FVector2D Heading = bHasSnapshot
		? DisplayedSnapshot.CurrentHeading.GetSafeNormal()
		: FVector2D(0.0f, 1.0f);
	const FVector2D ProjectedHeading(
		Heading.X * ArenaHalfExtents.X
			* FMath::Max(static_cast<float>(LocalSize.X), 1.0f),
		Heading.Y * ArenaHalfExtents.Y
			* FMath::Max(static_cast<float>(LocalSize.Y), 1.0f));
	const float HeadingAngle = ProjectedHeading.IsNearlyZero()
		? 0.0f
		: FMath::RadiansToDegrees(FMath::Atan2(ProjectedHeading.X, ProjectedHeading.Y));
	Border_SurferAvatar->SetRenderTransformAngle(HeadingAngle);

	FLinearColor SurferColor =
		bHasSnapshot && DisplayedSnapshot.bBoosting
		? VisualStyle.BoostingSurferColor
		: VisualStyle.SurferColor;
	if (bHasSnapshot && DisplayedSnapshot.bInvincible)
	{
		SurferColor = SurferColor * 0.62f + FLinearColor::White * 0.38f;
		SurferColor.A = 1.0f;
	}
	UBorder* SurferVisual = Border_SurferCore
		? Border_SurferCore.Get()
		: Border_SurferAvatar.Get();
	SurferVisual->SetBrushColor(SurferColor);
	if (Border_SurferCore)
	{
		Border_SurferAvatar->SetBrushColor(VisualStyle.EntityHaloColor);
	}

	float Scale = bHasSnapshot && DisplayedSnapshot.bBoosting ? 1.12f : 1.0f;
	if (EventPulseRemainingSeconds > 0.0f)
	{
		const float PulseAlpha = VisualStyle.EventPulseSeconds > UE_SMALL_NUMBER
			? EventPulseRemainingSeconds / VisualStyle.EventPulseSeconds
			: 0.0f;
		Scale *= FMath::Lerp(1.0f, VisualStyle.EventPulseScale, PulseAlpha);
	}
	Border_SurferAvatar->SetRenderScale(FVector2D(Scale, Scale));
}

void UFF_SurfingScreen::RefreshEntityWidgets()
{
	if (bUsing3DScene || !Canvas_EntityLayer)
	{
		return;
	}

	const FVector2D ArenaHalfExtents = GetProjectedArenaHalfExtents(
		GetCachedGeometry().GetLocalSize());
	uint32 EntityHash = bHasSnapshot
		? GetTypeHash(DisplayedSnapshot.VisibleObjects.Num())
		: 0u;
	EntityHash = HashCombineFast(EntityHash, GetTypeHash(ArenaHalfExtents.X));
	EntityHash = HashCombineFast(EntityHash, GetTypeHash(ArenaHalfExtents.Y));
	if (bHasSnapshot)
	{
		for (const FFF_SurfingObjectSnapshot& ObjectSnapshot : DisplayedSnapshot.VisibleObjects)
		{
			EntityHash = HashCombineFast(EntityHash, GetTypeHash(ObjectSnapshot.ObjectId));
			EntityHash = HashCombineFast(
				EntityHash,
				GetTypeHash(static_cast<uint8>(ObjectSnapshot.Type)));
			EntityHash = HashCombineFast(
				EntityHash,
				GetTypeHash(ObjectSnapshot.PositionNormalized.X));
			EntityHash = HashCombineFast(
				EntityHash,
				GetTypeHash(ObjectSnapshot.PositionNormalized.Y));
			EntityHash = HashCombineFast(
				EntityHash,
				GetTypeHash(ObjectSnapshot.CollisionRadiusNormalized));
		}
	}
	if (bHasPresentedEntityHash && PresentedEntityHash == EntityHash)
	{
		return;
	}

	TSet<int32> VisibleObjectIds;
	if (bHasSnapshot)
	{
		for (const FFF_SurfingObjectSnapshot& ObjectSnapshot : DisplayedSnapshot.VisibleObjects)
		{
			if (ObjectSnapshot.ObjectId < 0)
			{
				continue;
			}

			VisibleObjectIds.Add(ObjectSnapshot.ObjectId);
			UBorder* EntityWidget = AcquireEntityWidget(ObjectSnapshot.ObjectId);
			if (!EntityWidget)
			{
				continue;
			}

			ConfigureEntityWidget(*EntityWidget, ObjectSnapshot);
			const FVector2D Position(
				FMath::Clamp(ObjectSnapshot.PositionNormalized.X, -1.0f, 1.0f),
				FMath::Clamp(ObjectSnapshot.PositionNormalized.Y, -1.0f, 1.0f));
			if (UCanvasPanelSlot* EntitySlot = Cast<UCanvasPanelSlot>(EntityWidget->Slot))
			{
				const FVector2D Anchor(
					VisualStyle.ArenaCenter.X + Position.X * ArenaHalfExtents.X,
					VisualStyle.ArenaCenter.Y - Position.Y * ArenaHalfExtents.Y);
				EntitySlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
				EntitySlot->SetAlignment(FVector2D(0.5f, 0.5f));
				EntitySlot->SetPosition(FVector2D::ZeroVector);
				EntitySlot->SetAutoSize(true);
				const bool bPickup =
					ObjectSnapshot.Type == EFF_SurfingObjectType::Coin
					|| ObjectSnapshot.Type == EFF_SurfingObjectType::Heart
					|| ObjectSnapshot.Type == EFF_SurfingObjectType::BoostPickup;
				EntitySlot->SetZOrder(bPickup ? 60 : 45);
			}

			const float RadiusScale = FMath::Clamp(
				ObjectSnapshot.CollisionRadiusNormalized / 0.075f,
				0.65f,
				1.45f);
			const float Scale = FMath::Max(0.1f, VisualStyle.EntityScale) * RadiusScale;
			EntityWidget->SetRenderScale(FVector2D(Scale, Scale));
			EntityWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	RecycleMissingEntityWidgets(VisibleObjectIds);
	PresentedEntityHash = EntityHash;
	bHasPresentedEntityHash = true;
}

UBorder* UFF_SurfingScreen::AcquireEntityWidget(const int32 ObjectId)
{
	if (const TObjectPtr<UBorder>* Existing = EntityWidgetsById.Find(ObjectId))
	{
		return Existing->Get();
	}

	UBorder* EntityWidget = nullptr;
	while (!EntityWidget && !RecycledEntityWidgets.IsEmpty())
	{
		EntityWidget = RecycledEntityWidgets.Pop(EAllowShrinking::No);
	}
	if (!EntityWidget)
	{
		if (!WidgetTree || !Canvas_EntityLayer)
		{
			return nullptr;
		}

		EntityWidget = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("Border_RuntimeSurfObject_%d"), ObjectId));
		UBorder* EntityArt = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("Border_RuntimeSurfObjectArt_%d"), ObjectId));
		UTextBlock* EntityLabel = MakeNativeText(
			*WidgetTree,
			*FString::Printf(TEXT("Text_RuntimeSurfObject_%d"), ObjectId),
			FText::GetEmpty(),
			VisualStyle.EntityLabelFontSize,
			FLinearColor::White);
		if (!EntityWidget || !EntityArt || !EntityLabel)
		{
			return nullptr;
		}

		EntityWidget->SetBrushColor(VisualStyle.EntityHaloColor);
		EntityWidget->SetPadding(FMargin(4.0f));
		EntityWidget->SetContent(EntityArt);
		EntityArt->SetContent(EntityLabel);
		EntityWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddAutoSizedToCanvas(
			*Canvas_EntityLayer,
			*EntityWidget,
			VisualStyle.ArenaCenter,
			FVector2D(0.5f, 0.5f),
			45);
	}

	EntityWidgetsById.Add(ObjectId, EntityWidget);
	return EntityWidget;
}

void UFF_SurfingScreen::RecycleMissingEntityWidgets(
	const TSet<int32>& VisibleObjectIds)
{
	TArray<int32> MissingIds;
	for (const TPair<int32, TObjectPtr<UBorder>>& Pair : EntityWidgetsById)
	{
		if (!VisibleObjectIds.Contains(Pair.Key))
		{
			MissingIds.Add(Pair.Key);
		}
	}

	for (const int32 MissingId : MissingIds)
	{
		TObjectPtr<UBorder> MissingWidget;
		if (EntityWidgetsById.RemoveAndCopyValue(MissingId, MissingWidget)
			&& IsValid(MissingWidget))
		{
			MissingWidget->SetVisibility(ESlateVisibility::Collapsed);
			RecycledEntityWidgets.Add(MissingWidget);
		}
	}
}

void UFF_SurfingScreen::ConfigureEntityWidget(
	UBorder& EntityWidget,
	const FFF_SurfingObjectSnapshot& ObjectSnapshot) const
{
	FLinearColor Color = VisualStyle.RockColor;
	FText Label = LOCTEXT("RockLabel", "ROCK");
	TSoftObjectPtr<UTexture2D> TextureAsset = VisualStyle.RockTexture;
	switch (ObjectSnapshot.Type)
	{
	case EFF_SurfingObjectType::Log:
		Color = VisualStyle.LogColor;
		Label = LOCTEXT("LogLabel", "LOG");
		TextureAsset = VisualStyle.LogTexture;
		break;
	case EFF_SurfingObjectType::Buoy:
		Color = VisualStyle.BuoyColor;
		Label = LOCTEXT("BuoyLabel", "BUOY");
		TextureAsset = VisualStyle.BuoyTexture;
		break;
	case EFF_SurfingObjectType::Coin:
		Color = VisualStyle.CoinColor;
		Label = LOCTEXT("CoinLabel", "COIN");
		TextureAsset = VisualStyle.CoinTexture;
		break;
	case EFF_SurfingObjectType::Heart:
		Color = VisualStyle.HeartColor;
		Label = LOCTEXT("HeartLabel", "HEART");
		TextureAsset = VisualStyle.HeartTexture;
		break;
	case EFF_SurfingObjectType::BoostPickup:
		Color = VisualStyle.BoostPickupColor;
		Label = LOCTEXT("BoostPickupLabel", "BOOST");
		TextureAsset = VisualStyle.BoostPickupTexture;
		break;
	case EFF_SurfingObjectType::Rock:
	default:
		break;
	}

	UBorder* ArtBorder = Cast<UBorder>(EntityWidget.GetContent());
	UTextBlock* LabelWidget = ArtBorder
		? Cast<UTextBlock>(ArtBorder->GetContent())
		: Cast<UTextBlock>(EntityWidget.GetContent());
	UBorder& VisualBorder = ArtBorder ? *ArtBorder : EntityWidget;
	if (ArtBorder)
	{
		EntityWidget.SetBrushColor(VisualStyle.EntityHaloColor);
		EntityWidget.SetPadding(FMargin(4.0f));
	}
	UTexture2D* Texture = TextureAsset.IsNull()
		? nullptr
		: TextureAsset.LoadSynchronous();
	if (Texture)
	{
		FSlateBrush TextureBrush;
		TextureBrush.DrawAs = ESlateBrushDrawType::Image;
		TextureBrush.SetResourceObject(Texture);
		TextureBrush.SetImageSize(VisualStyle.EntityTextureSize);
		TextureBrush.TintColor = FSlateColor(FLinearColor::White);
		VisualBorder.SetBrush(TextureBrush);
		VisualBorder.SetPadding(FMargin(0.0f));
		if (LabelWidget)
		{
			LabelWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		const FSlateColorBrush ColorBrush(Color);
		VisualBorder.SetBrush(ColorBrush);
		VisualBorder.SetPadding(FMargin(12.0f, 8.0f));
		if (LabelWidget)
		{
			LabelWidget->SetText(Label);
			LabelWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

FVector2D UFF_SurfingScreen::GetProjectedArenaHalfExtents(
	const FVector2D& LocalSize) const
{
	const float MaximumHalfWidth = FMath::Clamp(
		VisualStyle.ArenaHalfWidth,
		0.01f,
		0.5f);
	const float MaximumHalfHeight = FMath::Clamp(
		VisualStyle.ArenaHalfHeight,
		0.01f,
		0.5f);
	if (LocalSize.X <= 1.0 || LocalSize.Y <= 1.0)
	{
		return FVector2D(MaximumHalfWidth, MaximumHalfHeight);
	}

	constexpr float ReferenceViewportAspect = 16.0f / 9.0f;
	const float StyleMetricScale =
		MaximumHalfWidth * ReferenceViewportAspect / MaximumHalfHeight;
	const float MetricScale = bHasSnapshot
		&& FMath::IsFinite(DisplayedSnapshot.ArenaHorizontalMetricScale)
		&& DisplayedSnapshot.ArenaHorizontalMetricScale > UE_SMALL_NUMBER
		? DisplayedSnapshot.ArenaHorizontalMetricScale
		: StyleMetricScale;
	const double MaximumHalfWidthPixels =
		static_cast<double>(MaximumHalfWidth) * LocalSize.X;
	const double MaximumHalfHeightPixels =
		static_cast<double>(MaximumHalfHeight) * LocalSize.Y;
	const double HalfHeightPixels = FMath::Min(
		MaximumHalfHeightPixels,
		MaximumHalfWidthPixels / static_cast<double>(MetricScale));
	const double HalfWidthPixels =
		HalfHeightPixels * static_cast<double>(MetricScale);
	return FVector2D(
		HalfWidthPixels / LocalSize.X,
		HalfHeightPixels / LocalSize.Y);
}

FVector2D UFF_SurfingScreen::ConvertDisplayDirectionToArenaInput(
	const FVector2D& DisplayDirection) const
{
	if (DisplayDirection.IsNearlyZero())
	{
		return FVector2D::ZeroVector;
	}
	if (bUsing3DScene && IsValid(SurfingSceneActor))
	{
		const FVector2D UnitDisplayDirection = DisplayDirection.GetSafeNormal();
		const FVector2D CenterUv(0.5f, 0.5f);
		const FVector2D DirectionUv = CenterUv + FVector2D(
			UnitDisplayDirection.X,
			-UnitDisplayDirection.Y) * 0.08f;
		FVector2D CenterArenaPosition;
		FVector2D DirectionArenaPosition;
		if (SurfingSceneActor->DeprojectCapturePointToArena(
				CenterUv, CenterArenaPosition)
			&& SurfingSceneActor->DeprojectCapturePointToArena(
				DirectionUv, DirectionArenaPosition))
		{
			FVector2D CameraRelativeInput =
				DirectionArenaPosition - CenterArenaPosition;
			if (!CameraRelativeInput.IsNearlyZero())
			{
				CameraRelativeInput.Normalize();
				return CameraRelativeInput;
			}
		}
	}

	const FVector2D LocalSize = GetCachedGeometry().GetLocalSize();
	const FVector2D ArenaHalfExtents = GetProjectedArenaHalfExtents(LocalSize);
	const float PixelScaleX = ArenaHalfExtents.X
		* FMath::Max(static_cast<float>(LocalSize.X), 1.0f);
	const float PixelScaleY = ArenaHalfExtents.Y
		* FMath::Max(static_cast<float>(LocalSize.Y), 1.0f);
	FVector2D ArenaInput(
		DisplayDirection.X / FMath::Max(PixelScaleX, 0.01f),
		DisplayDirection.Y / FMath::Max(PixelScaleY, 0.01f));
	ArenaInput.Normalize();
	return ArenaInput;
}

bool UFF_SurfingScreen::TrySetMovementInput(const FVector2D& InputAxis)
{
	if (!IsValid(Runtime)
		|| !GetSessionId().IsValid()
		|| !bHasSnapshot
		|| bHasResult
		|| DisplayedSnapshot.Session.State != EFF_MinigameSessionState::Playing
		|| !FMath::IsFinite(InputAxis.X)
		|| !FMath::IsFinite(InputAxis.Y))
	{
		return false;
	}

	const FVector2D ClampedInput = InputAxis.GetClampedToMaxSize(1.0f);
	const EFF_SurfingSteerResult Result = Runtime->SetMovementInput(
		GetSessionId(),
		ClampedInput.X,
		ClampedInput.Y);
	ApplySnapshot(Runtime->GetSurfingSnapshot());
	return Result == EFF_SurfingSteerResult::Applied;
}

bool UFF_SurfingScreen::TryBoost()
{
	if (!IsValid(Runtime)
		|| !GetSessionId().IsValid()
		|| !bHasSnapshot
		|| bHasResult
		|| DisplayedSnapshot.Session.State != EFF_MinigameSessionState::Playing)
	{
		return false;
	}

	const EFF_SurfingBoostResult Result = Runtime->SubmitBoost(GetSessionId());
	ApplySnapshot(Runtime->GetSurfingSnapshot());
	return Result == EFF_SurfingBoostResult::Activated;
}

void UFF_SurfingScreen::UpdateHeldMovementInput()
{
	FVector2D DigitalInput(
		(bRightKeyHeld ? 1.0f : 0.0f) - (bLeftKeyHeld ? 1.0f : 0.0f),
		(bUpKeyHeld ? 1.0f : 0.0f) - (bDownKeyHeld ? 1.0f : 0.0f));
	if (!DigitalInput.IsNearlyZero())
	{
		DigitalInput.Normalize();
		TrySetMovementInput(ConvertDisplayDirectionToArenaInput(DigitalInput));
		return;
	}

	constexpr float AnalogDeadZone = 0.12f;
	FVector2D AnalogInput = AnalogMovementInput.GetClampedToMaxSize(1.0f);
	if (AnalogInput.SizeSquared() < FMath::Square(AnalogDeadZone))
	{
		AnalogInput = FVector2D::ZeroVector;
	}
	TrySetMovementInput(ConvertDisplayDirectionToArenaInput(AnalogInput));
}

void UFF_SurfingScreen::ReleaseAllMovementInput()
{
	bLeftKeyHeld = false;
	bRightKeyHeld = false;
	bUpKeyHeld = false;
	bDownKeyHeld = false;
	AnalogMovementInput = FVector2D::ZeroVector;

	if (IsValid(Runtime)
		&& GetSessionId().IsValid()
		&& bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing)
	{
		Runtime->SetMovementInput(GetSessionId(), 0.0f, 0.0f);
	}
}

void UFF_SurfingScreen::HandleCloseClicked()
{
	RequestClose();
}

#undef LOCTEXT_NAMESPACE
