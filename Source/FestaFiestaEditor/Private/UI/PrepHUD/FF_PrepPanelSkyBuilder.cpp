#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace FFPrepPanelSkyBuilder
{
	using namespace FFUIWbpBuilder;
	FLinearColor Color(uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R, G, B)); }
	const FLinearColor Paper = Color(252, 253, 249);
	const FLinearColor Ink = Color(23, 66, 55);
	const FLinearColor Muted = Color(101, 127, 117);
	const FLinearColor SkyLine = Color(175, 216, 232);
	const FLinearColor SkyWash = Color(241, 248, 249);
	const FLinearColor Green = Color(25, 139, 91);

	FSlateBrush Brush(FLinearColor Fill, float Radius, float Width, FLinearColor Stroke)
	{
		FSlateBrush Result;
		Result.DrawAs = ESlateBrushDrawType::RoundedBox;
		Result.TintColor = Fill;
		Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Result.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Result.OutlineSettings.Width = Width;
		Result.OutlineSettings.Color = Stroke;
		return Result;
	}

	void Row(UVerticalBox& Parent, UWidget& Child, FMargin Padding, bool bFill)
	{
		UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(&Child);
		Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetPadding(Padding);
	}

	void Column(UHorizontalBox& Parent, UWidget& Child, FMargin Padding, bool bFill)
	{
		UHorizontalBoxSlot* Slot = Parent.AddChildToHorizontalBox(&Child);
		Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetVerticalAlignment(VAlign_Fill);
		Slot->SetPadding(Padding);
	}

	UTextBlock* Text(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, const FText& Preview,
		int32 FontSize, FLinearColor Tint, bool bVariable)
	{
		UTextBlock* Result = MakeWidget<UTextBlock>(BP, Tree, Name, bVariable);
		StyleText(*Result, FontSize, Tint, FontSize >= 20 ? TEXT("SemiBold") : TEXT("Medium"));
		Result->SetText(Preview);
		return Result;
	}

	USizeBox* Rule(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, bool bVertical)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(FString(Name) + TEXT("_Size")));
		if (bVertical) { Size->SetWidthOverride(1.f); } else { Size->SetHeightOverride(1.f); }
		UBorder* Line = MakeWidget<UBorder>(BP, Tree, Name);
		Line->SetBrush(Brush(SkyLine));
		Line->SetPadding(FMargin(0.f));
		Size->AddChild(Line);
		return Size;
	}

	UButton* Button(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& Content, bool bPrimary)
	{
		UButton* Result = MakeWidget<UButton>(BP, Tree, Name, true);
		FButtonStyle Style = Result->GetStyle();
		Style.Normal = Brush(bPrimary ? Green : Paper, 7.f, 1.f, bPrimary ? Green : SkyLine);
		Style.Hovered = Brush(bPrimary ? Color(21, 122, 80) : SkyWash, 7.f, 2.f, bPrimary ? Ink : Color(100, 177, 210));
		Style.Pressed = Brush(bPrimary ? Ink : Color(221, 239, 246), 7.f, 2.f, Ink);
		Style.Disabled = Brush(bPrimary ? Color(138, 164, 153) : SkyWash, 7.f, 1.f, SkyLine);
		Style.NormalPadding = Style.PressedPadding = FMargin(12.f, 5.f);
		Result->SetStyle(Style);
		UButtonSlot* Slot = CastChecked<UButtonSlot>(Result->AddChild(&Content));
		Slot->SetPadding(FMargin(0.f));
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetVerticalAlignment(VAlign_Center);
		return Result;
	}

	FPanelFrame MakeFrame(UWidgetBlueprint& BP, UWidgetTree& Tree, const FText& Title, const FString& IconPackage)
	{
		UBorder* Root = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PanelRoot"));
		Root->SetBrush(Brush(Paper, 10.f, 1.5f, SkyLine));
		Root->SetPadding(FMargin(2.f));
		Tree.RootWidget = Root;
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Sections"));
		Root->AddChild(Sections);
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Header"));
		USizeBox* HeaderSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Header"));
		HeaderSize->SetHeightOverride(68.f);
		HeaderSize->AddChild(Header);
		Row(*Sections, *HeaderSize, FMargin(24.f, 0.f));
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_PanelIcon"));
		Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(IconPackage)));
		Icon->SetDesiredSizeOverride(FVector2D(25.f, 25.f));
		Icon->SetColorAndOpacity(Ink);
		Column(*Header, *Icon, FMargin(0.f, 0.f, 12.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(Icon->Slot)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* TitleText = Text(BP, Tree, TEXT("Text_PanelTitle"), Title, 25, Ink, true);
		Column(*Header, *TitleText, FMargin(0.f), true);
		CastChecked<UHorizontalBoxSlot>(TitleText->Slot)->SetVerticalAlignment(VAlign_Center);
		UButton* Close = Button(BP, Tree, TEXT("Button_Close"),
			*CloseIcon(BP, Tree));
		Close->SetToolTipText(NSLOCTEXT("FestaFiestaPrepSky", "Close", "닫기"));
		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.NormalPadding = CloseStyle.PressedPadding = FMargin(2.f);
		Close->SetStyle(CloseStyle);
		USizeBox* CloseSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Close"));
		CloseSize->SetWidthOverride(36.f); CloseSize->SetHeightOverride(36.f); CloseSize->AddChild(Close);
		Column(*Header, *CloseSize);
		CastChecked<UHorizontalBoxSlot>(CloseSize->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(*Sections, *Rule(BP, Tree, TEXT("Border_HeaderDivider")), FMargin(24.f, 0.f));

		UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("ScrollBox_Body"));
		Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetAllowOverscroll(false);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Body"));
		Scroll->AddChild(Body);
		Row(*Sections, *Scroll, FMargin(24.f, 20.f, 24.f, 20.f), true);
		UBorder* FooterBorder = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Footer"));
		FooterBorder->SetBrush(Brush(SkyWash, 8.f));
		FooterBorder->SetPadding(FMargin(24.f, 16.f));
		UVerticalBox* Footer = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Footer"));
		FooterBorder->AddChild(Footer);
		Row(*Sections, *FooterBorder);
		return { Body, Footer };
	}

	USizeBox* CloseIcon(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_CloseIcon"));
		Size->SetWidthOverride(18.f); Size->SetHeightOverride(18.f);
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_Close"));
		Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			*MakeObjectPath(TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Close_White_256"))));
		Icon->SetColorAndOpacity(Muted);
		Size->AddChild(Icon);
		return Size;
	}
}
