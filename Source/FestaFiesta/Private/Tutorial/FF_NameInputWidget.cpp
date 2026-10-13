// FF_NameInputWidget.cpp
// 역할: 주인공 이름 입력 창을 코드로 그리고, 입력한 이름을 튜토리얼 시스템에 넘긴다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_NameInputWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"
#include "Tutorial/FF_TutorialSettings.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FestaFiestaNameInput"

// 이름 있는 이름공간: 유니티 빌드에서 다른 코드 위젯의 같은 이름 도우미와 한 파일로 묶인다.
namespace FFNameInputWidgetStyle
{
	// 에디터 생성기와 같은 글꼴. FFUIWbpBuilder::GetUiFontPackage 와 같은 경로다.
	const TCHAR* const UiFontPath = TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard");

	FSlateFontInfo MakeFont(const FSlateFontInfo& Base, const int32 Size, const TCHAR* Typeface)
	{
		FSlateFontInfo Font = Base;
		if (UFont* UiFont = LoadObject<UFont>(nullptr, UiFontPath))
		{
			Font.FontObject = UiFont;
			Font.TypefaceFontName = Typeface;
		}
		Font.Size = Size;
		return Font;
	}

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

void UFF_NameInputWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	using namespace FFNameInputWidgetStyle;
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasPanel_Root"));
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Panel"));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_Column"));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	USizeBox* BoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_Name"));
	NameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("EditableTextBox_Name"));
	UTextBlock* Limit = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Limit"));
	ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Confirm"));
	ConfirmLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Confirm"));
	if (!Root || !Panel || !Column || !Title || !BoxSize || !NameBox || !Limit || !ConfirmButton || !ConfirmLabel)
	{
		return;
	}
	WidgetTree->RootWidget = Root;

	// 화면 전체 캔버스. 창 밖은 클릭을 막지 않는다 — 이 줄에서는 대사창도 클릭으로 넘어가지 않는다.
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	/* 판은 테마 그림(MakeBodyBrush) 대신 둥근 상자로 그린다.

	   테마 그림은 가장자리에 투명한 여백이 있어 보이는 테두리가 위젯 안쪽으로 들어간다. 그 위에 여백을 주면
	   내용이 보이는 테두리에 붙었다(PIE 스크린샷). 둥근 상자는 보이는 테두리 = 위젯 경계라 여백이 그대로 보인다.
	*/
	Panel->SetBrush(MakeRounded(Tokens.PaperStrong, 12.f, Tokens.Line, 1.f));
	Panel->SetContent(Column);
	if (UBorderSlot* ColumnSlot = Cast<UBorderSlot>(Panel->GetContentSlot()))
	{
		ColumnSlot->SetPadding(FMargin(32.f, 24.f));
		ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
		ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	}

	Title->SetFont(MakeFont(Title->GetFont(), 17, TEXT("SemiBold")));
	Title->SetColorAndOpacity(FSlateColor(Tokens.TealDeep));
	Title->SetText(LOCTEXT("Title", "이름"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	FEditableTextBoxStyle BoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
	const FSlateBrush Field = MakeRounded(Tokens.PaperSunken, 6.f, Tokens.Line, 1.f);
	const FSlateBrush FieldFocused = MakeRounded(Tokens.PaperSunken, 6.f, Tokens.TealDeep, 1.5f);
	BoxStyle.SetBackgroundImageNormal(Field)
		.SetBackgroundImageHovered(Field)
		.SetBackgroundImageFocused(FieldFocused)
		.SetBackgroundImageReadOnly(Field)
		.SetPadding(FMargin(12.f, 9.f))
		.SetFont(MakeFont(BoxStyle.TextStyle.Font, 18, TEXT("Regular")))
		.SetForegroundColor(FSlateColor(Tokens.Ink))
		.SetFocusedForegroundColor(FSlateColor(Tokens.Ink));
	NameBox->SetWidgetStyle(BoxStyle);
	NameBox->SetHintText(LOCTEXT("Hint", "이름을 입력해 주세요"));
	NameBox->SetJustification(ETextJustify::Center);
	NameBox->OnTextChanged.AddDynamic(this, &UFF_NameInputWidget::HandleTextChanged);
	NameBox->OnTextCommitted.AddDynamic(this, &UFF_NameInputWidget::HandleTextCommitted);

	BoxSize->SetWidthOverride(300.f);
	BoxSize->SetContent(NameBox);
	Column->AddChildToVerticalBox(BoxSize);

	Limit->SetFont(MakeFont(Limit->GetFont(), 12, TEXT("Regular")));
	Limit->SetColorAndOpacity(FSlateColor(Tokens.InkMuted));
	Limit->SetText(FText::Format(LOCTEXT("Limit", "{0}자까지 · Enter 로 확인"), FText::AsNumber(GetMaxLength())));
	if (UVerticalBoxSlot* LimitSlot = Column->AddChildToVerticalBox(Limit))
	{
		LimitSlot->SetHorizontalAlignment(HAlign_Center);
		LimitSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 14.f));
	}

	FButtonStyle Style;
	Style.Normal = FFPauseOptionsLightTeal::MakeRectButtonBrush(FFPauseOptionsLightTeal::RectPrimaryPath, Tokens.Teal, Tokens.TealDeep);
	Style.Hovered = FFPauseOptionsLightTeal::MakeRectButtonBrush(FFPauseOptionsLightTeal::RectPrimaryHoverPath, Tokens.TealDeep, Tokens.TealDeep);
	Style.Pressed = Style.Hovered;
	Style.Disabled = FFPauseOptionsLightTeal::MakeRectButtonBrush(FFPauseOptionsLightTeal::RectNavDisabledPath, Tokens.Disabled, Tokens.Line);
	Style.NormalPadding = FMargin(24.f, 8.f);
	Style.PressedPadding = Style.NormalPadding;
	ConfirmButton->SetStyle(Style);
	ConfirmButton->OnClicked.AddDynamic(this, &UFF_NameInputWidget::HandleConfirmClicked);

	ConfirmLabel->SetFont(MakeFont(ConfirmLabel->GetFont(), 15, TEXT("SemiBold")));
	ConfirmLabel->SetText(LOCTEXT("Confirm", "확인"));
	ConfirmReadyColor = Tokens.White;
	ConfirmIdleColor = Tokens.InkMuted;
	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(ConfirmButton->AddChild(ConfirmLabel)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(ConfirmButton))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// 화면 가운데 조금 위. 아래쪽 대사창(묻는 말)을 가리지 않는다.
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.4f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}

	RefreshConfirm();
}

void UFF_NameInputWidget::NativeDestruct()
{
	if (NameBox)
	{
		NameBox->OnTextChanged.RemoveDynamic(this, &UFF_NameInputWidget::HandleTextChanged);
		NameBox->OnTextCommitted.RemoveDynamic(this, &UFF_NameInputWidget::HandleTextCommitted);
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &UFF_NameInputWidget::HandleConfirmClicked);
	}
	Super::NativeDestruct();
}

void UFF_NameInputWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bFocusedOnce && NameBox && IsInViewport())
	{
		bFocusedOnce = true;
		NameBox->SetKeyboardFocus();
	}

	// 코드에서 글자를 바꾸면(한도 자르기 등) 바뀜 알림이 안 오는 경우가 있어 매 틱 맞춘다. 값이 같으면 하는 일이 없다.
	RefreshConfirm();
}

int32 UFF_NameInputWidget::GetMaxLength() const
{
	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	return Settings ? FMath::Max(1, Settings->PlayerNameMaxLength) : 8;
}

void UFF_NameInputWidget::HandleTextChanged(const FText& Text)
{
	// 한도를 넘긴 글자는 바로 잘라 낸다. 확인을 눌렀을 때 거절하면 무엇이 틀렸는지 모른다.
	const FString Value = Text.ToString();
	const int32 MaxLength = GetMaxLength();
	if (Value.Len() > MaxLength && NameBox)
	{
		NameBox->SetText(FText::FromString(Value.Left(MaxLength)));
	}
	RefreshConfirm();
}

void UFF_NameInputWidget::HandleTextCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Submit();
	}
}

void UFF_NameInputWidget::HandleConfirmClicked()
{
	Submit();
}

void UFF_NameInputWidget::Submit()
{
	if (!NameBox)
	{
		return;
	}

	UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	if (!Tutorial || !Tutorial->SubmitPlayerName(NameBox->GetText().ToString()))
	{
		// 비었거나 받지 않았다. 다시 넣을 수 있게 커서를 돌려 둔다.
		NameBox->SetKeyboardFocus();
	}
}

void UFF_NameInputWidget::RefreshConfirm()
{
	if (!ConfirmButton || !NameBox)
	{
		return;
	}

	const bool bReady = !NameBox->GetText().ToString().TrimStartAndEnd().IsEmpty();
	if (ConfirmButton->GetIsEnabled() != bReady || !bConfirmStyled)
	{
		bConfirmStyled = true;
		ConfirmButton->SetIsEnabled(bReady);
		// 꺼진 버튼은 옅은 판이라 흰 글자가 묻힌다. 글자색도 같이 바꾼다.
		if (ConfirmLabel)
		{
			ConfirmLabel->SetColorAndOpacity(FSlateColor(bReady ? ConfirmReadyColor : ConfirmIdleColor));
		}
	}
}

#undef LOCTEXT_NAMESPACE
