// Copyright Epic Games, Inc. All Rights Reserved.

#include "Secretary/FF_SecretaryTipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Secretary/FF_SecretarySettings.h"
#include "Tutorial/FF_TutorialSettings.h"
#include "UI/Data/FF_UIPalette.h"

namespace
{
	// 게임 UI 글꼴. 대사창 · 사건 토스트와 같은 것.
	const TCHAR* const SecretaryUiFontPath = TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard");

	constexpr float SecretaryFaceSize = 72.f;
	constexpr float SecretaryTextMaxWidth = 400.f;

	FSlateFontInfo MakeSecretaryFont(UFont* Font, const int32 Size, const TCHAR* Typeface)
	{
		return Font ? FSlateFontInfo(Font, Size, FName(Typeface)) : FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

	UTexture2D* LoadSecretaryPortrait()
	{
		const UFF_TutorialSettings* Tutorial = UFF_TutorialSettings::Get();
		const UFF_SecretarySettings* Secretary = UFF_SecretarySettings::Get();
		const TSoftObjectPtr<UTexture2D>* Entry = (Tutorial && Secretary)
			? Tutorial->SpeakerPortraits.Find(Secretary->SpeakerName) : nullptr;
		return Entry ? Entry->LoadSynchronous() : nullptr;
	}
}

TSharedRef<SWidget> UFF_SecretaryTipWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UFF_SecretaryTipWidget::BuildTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	const UFF_UIPalette* PaletteAsset = UFF_UIPalette::Get();
	const FFF_UIPaletteInGame Palette = PaletteAsset ? PaletteAsset->InGame : FFF_UIPaletteInGame();
	const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
	UFont* Font = LoadObject<UFont>(nullptr, SecretaryUiFontPath);

	// 화면 전체 판은 클릭을 받지 않는다. 말풍선 칸만 받는다(누르면 닫힘).
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_SecretaryTip"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	Box_Tip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Box_Tip"));
	if (UCanvasPanelSlot* TipSlot = Canvas->AddChildToCanvas(Box_Tip))
	{
		TipSlot->SetAnchors(FAnchors(0.f, 1.f));
		TipSlot->SetAlignment(FVector2D(0.f, 1.f));
		TipSlot->SetAutoSize(true);
		TipSlot->SetPosition(Settings ? Settings->BubbleOffset : FVector2D(24.0, -120.0));
	}

	// ── 동그란 얼굴 ──
	USizeBox* FaceBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size_Face"));
	FaceBox->SetWidthOverride(SecretaryFaceSize);
	FaceBox->SetHeightOverride(SecretaryFaceSize);
	Image_Face = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Face"));
	Image_Face->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UTexture2D* Portrait = LoadSecretaryPortrait())
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Portrait);
		Brush.ImageSize = FVector2D(SecretaryFaceSize, SecretaryFaceSize);
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.OutlineSettings.Color = FSlateColor(Palette.Teal);
		Brush.OutlineSettings.Width = 2.f;
		if (Settings)
		{
			Brush.SetUVRegion(FBox2f(
				FVector2f(Settings->FaceUVMin.X, Settings->FaceUVMin.Y),
				FVector2f(Settings->FaceUVMax.X, Settings->FaceUVMax.Y)));
		}
		Image_Face->SetBrush(Brush);
	}
	FaceBox->AddChild(Image_Face);
	if (UHorizontalBoxSlot* FaceSlot = Box_Tip->AddChildToHorizontalBox(FaceBox))
	{
		FaceSlot->SetVerticalAlignment(VAlign_Bottom);
		FaceSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	// ── 말풍선 ──
	UBorder* Bubble = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Bubble"));
	Bubble->SetBrush(FSlateRoundedBoxBrush(Palette.Paper, 14.f, Palette.Line, 1.5f));
	Bubble->SetPadding(FMargin(16.f, 10.f, 16.f, 12.f));
	Bubble->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column_Bubble"));
	Text_Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Name"));
	Text_Name->SetText(FText::FromString(Settings ? Settings->SpeakerName : TEXT("비서")));
	Text_Name->SetFont(MakeSecretaryFont(Font, 14, TEXT("SemiBold")));
	Text_Name->SetColorAndOpacity(FSlateColor(Palette.Teal));
	if (UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(Text_Name))
	{
		NameSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	USizeBox* TextWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size_Text"));
	TextWidth->SetMaxDesiredWidth(SecretaryTextMaxWidth);
	Text_Tip = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Tip"));
	Text_Tip->SetFont(MakeSecretaryFont(Font, 17, TEXT("Medium")));
	Text_Tip->SetColorAndOpacity(FSlateColor(Palette.Ink));
	Text_Tip->SetAutoWrapText(true);
	TextWidth->AddChild(Text_Tip);
	Column->AddChildToVerticalBox(TextWidth);

	Bubble->SetContent(Column);
	if (UHorizontalBoxSlot* BubbleSlot = Box_Tip->AddChildToHorizontalBox(Bubble))
	{
		BubbleSlot->SetVerticalAlignment(VAlign_Bottom);
	}
}

void UFF_SecretaryTipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyPhaseLook();
}

void UFF_SecretaryTipWidget::ShowTip(const FText& Text, const float VisibleOverride)
{
	if (Text_Tip)
	{
		Text_Tip->SetText(Text);
	}
	const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
	VisibleSeconds = VisibleOverride > 0.f ? VisibleOverride : (Settings ? Settings->TipVisibleSeconds : 7.f);

	// 이미 떠 있으면 다시 나타나는 모양 없이 시간만 처음부터.
	Phase = (Phase == EPhase::Visible) ? EPhase::Visible : EPhase::FadingIn;
	PhaseTime = 0.f;
	ApplyPhaseLook();
}

void UFF_SecretaryTipWidget::Dismiss()
{
	if (Phase == EPhase::Hidden || Phase == EPhase::FadingOut)
	{
		return;
	}
	Phase = EPhase::FadingOut;
	PhaseTime = 0.f;
	ApplyPhaseLook();
}

void UFF_SecretaryTipWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Phase == EPhase::Hidden)
	{
		return;
	}

	PhaseTime += InDeltaTime;
	if (Phase == EPhase::FadingIn && PhaseTime >= FadeInSeconds)
	{
		Phase = EPhase::Visible;
		PhaseTime = 0.f;
	}
	else if (Phase == EPhase::Visible && PhaseTime >= VisibleSeconds)
	{
		Phase = EPhase::FadingOut;
		PhaseTime = 0.f;
	}
	else if (Phase == EPhase::FadingOut && PhaseTime >= FadeOutSeconds)
	{
		Phase = EPhase::Hidden;
		PhaseTime = 0.f;
	}
	ApplyPhaseLook();
}

void UFF_SecretaryTipWidget::ApplyPhaseLook()
{
	if (!Box_Tip)
	{
		return;
	}

	float Alpha = 1.f;
	switch (Phase)
	{
	case EPhase::Hidden:
		Box_Tip->SetVisibility(ESlateVisibility::Collapsed);
		return;
	case EPhase::FadingIn:
		Alpha = FMath::Clamp(PhaseTime / FadeInSeconds, 0.f, 1.f);
		break;
	case EPhase::FadingOut:
		Alpha = 1.f - FMath::Clamp(PhaseTime / FadeOutSeconds, 0.f, 1.f);
		break;
	default:
		break;
	}

	// 사라지는 중에는 누를 수 없게 한다. 그 클릭은 아래 화면으로 간다.
	Box_Tip->SetVisibility(Phase == EPhase::FadingOut ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
	Box_Tip->SetRenderOpacity(Alpha);
	// 나타날 때 아래에서 살짝 올라온다.
	Box_Tip->SetRenderTranslation(FVector2D(0.f, (1.f - Alpha) * (Phase == EPhase::FadingIn ? SlideDistance : 0.f)));
}

FReply UFF_SecretaryTipWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Phase == EPhase::FadingIn || Phase == EPhase::Visible)
	{
		Dismiss();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
