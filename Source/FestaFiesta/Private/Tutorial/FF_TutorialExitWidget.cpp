// FF_TutorialExitWidget.cpp
// 역할: 튜토리얼을 마친 뒤 레벨 선택으로 돌아가는 버튼을 코드로 그리고 연결한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialExitWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FestaFiestaTutorialExit"

namespace
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

void UFF_TutorialExitWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);

	// 화면 전체 캔버스. 버튼 밖의 클릭은 그대로 아래로 흘려보낸다.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasPanel_Root"));
	ExitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Exit"));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Exit"));
	if (!Root || !ExitButton || !Label)
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
	ExitButton->SetStyle(Style);
	ExitButton->SetToolTipText(LOCTEXT("ExitTip", "실습을 마치고 레벨 선택으로 돌아갑니다"));
	ExitButton->OnClicked.AddDynamic(this, &UFF_TutorialExitWidget::HandleExitClicked);

	FSlateFontInfo Font = Label->GetFont();
	if (UFont* UiFont = LoadObject<UFont>(nullptr, UiFontPath))
	{
		Font.FontObject = UiFont;
		Font.TypefaceFontName = TEXT("SemiBold");
	}
	Font.Size = 14;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(Tokens.TealDeep));
	Label->SetText(LOCTEXT("Exit", "나가기"));

	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(ExitButton->AddChild(Label)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// 오른쪽 위. 대사 중 건너뛰기 버튼이 있던 자리다.
	if (UCanvasPanelSlot* ExitSlot = Root->AddChildToCanvas(ExitButton))
	{
		ExitSlot->SetAnchors(FAnchors(1.f, 0.f));
		ExitSlot->SetAlignment(FVector2D(1.f, 0.f));
		ExitSlot->SetAutoSize(true);
		ExitSlot->SetPosition(FVector2D(-24.f, 24.f));
	}
}

void UFF_TutorialExitWidget::NativeDestruct()
{
	if (ExitButton)
	{
		ExitButton->OnClicked.RemoveDynamic(this, &UFF_TutorialExitWidget::HandleExitClicked);
	}
	Super::NativeDestruct();
}

void UFF_TutorialExitWidget::HandleExitClicked()
{
	if (ExitButton)
	{
		// 연타로 레벨 이동이 두 번 걸리지 않게 한다.
		ExitButton->SetIsEnabled(false);
	}

	if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
	{
		Tutorial->ExitTutorialLevel();
	}
}

#undef LOCTEXT_NAMESPACE
