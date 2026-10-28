// FF_TutorialSkipWidget.cpp
// 역할: 국가 지도의 "튜토리얼 건너뛰기" 버튼을 코드로 그리고, 누르면 건너뛰기 확인 창을 띄운다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialSkipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Tutorial/FF_SkipConfirmWidget.h"
#include "Tutorial/FF_TutorialSettings.h"
#include "Tutorial/FF_TutorialTypes.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FestaFiestaTutorialSkip"

// 이름 있는 이름공간: 유니티 빌드에서 FF_TutorialExitWidget.cpp 의 같은 이름 도우미와 한 파일로 묶인다.
namespace FFTutorialSkipWidgetStyle
{
	// 에디터 생성기와 같은 글꼴. FFUIWbpBuilder::GetUiFontPackage 와 같은 경로다.
	const TCHAR* const UiFontPath = TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard");

	FSlateBrush MakeRounded(const FLinearColor& Fill, const float Radius, const FLinearColor& Outline, const float Width)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		return Brush;
	}
}

void UFF_TutorialSkipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	using FFTutorialSkipWidgetStyle::MakeRounded;
	using FFTutorialSkipWidgetStyle::UiFontPath;

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);

	// 화면 전체 캔버스. 버튼 밖의 클릭은 그대로 지도로 흘려보낸다.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasPanel_Root"));
	SkipButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Skip"));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Skip"));
	if (!Root || !SkipButton || !Label)
	{
		return;
	}
	WidgetTree->RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	FButtonStyle Style;
	Style.Normal = MakeRounded(FFPauseOptionsLightTeal::WithAlpha(Tokens.PaperStrong, 0.96f), 8.f, Tokens.Line, 1.f);
	Style.Hovered = MakeRounded(Tokens.Hover, 8.f, Tokens.TealDeep, 1.f);
	Style.Pressed = MakeRounded(Tokens.TealSoft, 8.f, Tokens.TealDeep, 1.f);
	Style.Disabled = MakeRounded(Tokens.Disabled, 8.f, Tokens.Line, 1.f);
	Style.NormalPadding = FMargin(18.f, 8.f);
	Style.PressedPadding = Style.NormalPadding;
	SkipButton->SetStyle(Style);
	SkipButton->SetToolTipText(LOCTEXT("SkipTip", "실습장을 건너뛰고 다른 축제를 엽니다"));
	SkipButton->OnClicked.AddDynamic(this, &UFF_TutorialSkipWidget::HandleSkipClicked);

	FSlateFontInfo Font = Label->GetFont();
	if (UFont* UiFont = LoadObject<UFont>(nullptr, UiFontPath))
	{
		Font.FontObject = UiFont;
		Font.TypefaceFontName = TEXT("SemiBold");
	}
	Font.Size = 14;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(Tokens.TealDeep));
	Label->SetText(LOCTEXT("Skip", "튜토리얼 건너뛰기"));

	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(SkipButton->AddChild(Label)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// 오른쪽 위. 로비 이야기 중 대사창의 건너뛰기 버튼이 있던 자리다.
	if (UCanvasPanelSlot* SkipSlot = Root->AddChildToCanvas(SkipButton))
	{
		SkipSlot->SetAnchors(FAnchors(1.f, 0.f));
		SkipSlot->SetAlignment(FVector2D(1.f, 0.f));
		SkipSlot->SetAutoSize(true);
		SkipSlot->SetPosition(FVector2D(-24.f, 24.f));
	}
}

void UFF_TutorialSkipWidget::NativeDestruct()
{
	if (SkipButton)
	{
		SkipButton->OnClicked.RemoveDynamic(this, &UFF_TutorialSkipWidget::HandleSkipClicked);
	}
	Super::NativeDestruct();
}

void UFF_TutorialSkipWidget::HandleSkipClicked()
{
	// 바로 끝내지 않는다. 실습장이 잠기는 되돌릴 수 없는 선택이라 대사창의 건너뛰기와 같은 확인을 받는다.
	if (const UFF_SkipConfirmWidget* Existing = OpenSkipConfirm.Get())
	{
		if (Existing->IsInViewport())
		{
			return;
		}
	}

	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	UClass* ConfirmClass = Settings ? Settings->SkipConfirmWidgetClass.LoadSynchronous() : nullptr;
	if (!ConfirmClass)
	{
		// 확인 창이 지정되지 않았다. 확인 없이 끝내지는 않는다.
		UE_LOG(LogFestaFiestaTutorial, Warning,
			TEXT("[튜토리얼] 건너뛰기 확인 창이 지정되지 않았다. Project Settings 를 확인할 것."));
		return;
	}

	UFF_SkipConfirmWidget* Confirm = CreateWidget<UFF_SkipConfirmWidget>(GetOwningPlayer(), ConfirmClass);
	if (Confirm)
	{
		Confirm->AddToViewport(Settings->SkipConfirmZOrder);
		OpenSkipConfirm = Confirm;
	}
}

#undef LOCTEXT_NAMESPACE
