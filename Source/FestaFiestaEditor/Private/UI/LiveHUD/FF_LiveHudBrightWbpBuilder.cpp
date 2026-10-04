// 승인된 밝은 하단 HUD를 한 번 적용한다. 이후 치수/글꼴/배치는 UMG Designer가 소유한다.
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UI/LiveHUD/FF_LiveTabButton.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveHudBrightBuilder"

namespace FFLiveHudBrightBuilder
{
	using namespace FFUIWbpBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_BRIGHT");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/LiveHUD");
	const FString BarPackage = Folder / TEXT("WBP_FF_LiveHudScreen");
	const FString TabPackage = Folder / TEXT("WBP_FF_LiveTabButton");
	const FString IconRoot = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/");

	FLinearColor Color(uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R, G, B)); }
	const FLinearColor Paper = Color(250, 251, 245);
	const FLinearColor Ink = Color(20, 65, 54);
	const FLinearColor Green = Color(23, 128, 85);
	const FLinearColor Mint = Color(224, 245, 226);
	const FLinearColor Line = Color(168, 204, 178);
	const FLinearColor Amber = Color(188, 111, 10);

	FSlateBrush Solid(const FLinearColor& Fill, float Radius = 0.f, float Width = 0.f, FLinearColor Stroke = FLinearColor::Transparent)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Fill;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Width = Width;
		Brush.OutlineSettings.Color = Stroke;
		return Brush;
	}

	FSlateBrush IconBrush(const FString& RelativePath, float Size)
	{
		const FString Package = IconRoot + RelativePath;
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageType = ESlateBrushImageType::FullColor;
		Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Package)));
		Brush.SetImageSize(FVector2D(Size, Size));
		return Brush;
	}

	void AddHorizontal(UHorizontalBox& Parent, UWidget& Child, const FMargin& Padding = FMargin(0.f), bool bFill = false)
	{
		UHorizontalBoxSlot* Slot = Parent.AddChildToHorizontalBox(&Child);
		Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(Padding);
	}

	void AddRow(UVerticalBox& Parent, UWidget& Child, EHorizontalAlignment Align)
	{
		UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(&Child);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		Slot->SetHorizontalAlignment(Align);
		Slot->SetPadding(FMargin(0.f));
	}

	void SetBorderContent(UBorder& Parent, UWidget& Child)
	{
		const FMargin Padding = Parent.GetPadding();
		UBorderSlot* Slot = CastChecked<UBorderSlot>(Parent.AddChild(&Child));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(Padding);
	}

	bool BuildTab(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Frame = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Tab"));
		UButton* Button = MakeWidget<UButton>(BP, Tree, TEXT("Button_Root"), true);
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Content"));
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_Icon"), true);
		UTextBlock* Label = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_Label"), true);
		Tree.RootWidget = Frame;
		Frame->SetMinDesiredWidth(108.f);
		Frame->AddChild(Button);
		FButtonStyle Style = Button->GetStyle();
		Style.Normal = Solid(Paper, 8.f);
		Style.Hovered = Solid(Mint, 8.f, 1.f, Green);
		// 선택 상태는 테두리 두께도 달라진다. 색상만으로 구분하지 않는다.
		Style.Pressed = Solid(Green, 8.f, 2.f, Ink);
		Style.Disabled = Solid(Paper, 8.f);
		Style.NormalPadding = Style.PressedPadding = FMargin(12.f, 2.f);
		Button->SetStyle(Style);
		Icon->SetBrush(IconBrush(TEXT("Navigation/T_FF_LH_Icon_Facilities_White_256"), 17.f));
		Icon->SetDesiredSizeOverride(FVector2D(17.f, 17.f));
		Icon->SetColorAndOpacity(Ink);
		AddHorizontal(*Content, *Icon, FMargin(0.f, 0.f, 8.f, 0.f));
		StyleText(*Label, 15, Ink, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("TabPreview", "배치"));
		AddHorizontal(*Content, *Label);
		UButtonSlot* Slot = CastChecked<UButtonSlot>(Button->AddChild(Content));
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(0.f));
		return true;
	}

	UHorizontalBox* AddStatusGroup(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Parent,
		const TCHAR* GroupName, const TCHAR* TextName, const FText& Preview, const FString& IconPath = FString(),
		FLinearColor IconTint = Ink)
	{
		UHorizontalBox* Group = MakeWidget<UHorizontalBox>(BP, Tree, FName(GroupName), true);
		UTextBlock* Text = MakeWidget<UTextBlock>(BP, Tree, FName(TextName), true);
		if (!IconPath.IsEmpty())
		{
			UImage* Icon = MakeWidget<UImage>(BP, Tree, FName(FString(GroupName) + TEXT("_Icon")));
			Icon->SetBrush(IconBrush(IconPath, 20.f));
			Icon->SetDesiredSizeOverride(FVector2D(20.f, 20.f));
			Icon->SetColorAndOpacity(IconTint);
			AddHorizontal(*Group, *Icon, FMargin(0.f, 0.f, 7.f, 0.f));
		}
		StyleText(*Text, 15, Ink, TEXT("SemiBold"));
		Text->SetText(Preview);
		Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddHorizontal(*Group, *Text);
		AddHorizontal(Parent, *Group, FMargin(0.f, 0.f, 16.f, 0.f));
		return Group;
	}

	// 양쪽 그룹을 같은 폭에 배치한다. 작은 창에서는 그룹만 축소해 중앙 칩을 침범하지 않는다.
	void AddScaledColumn(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Columns,
		UWidget& Content, const TCHAR* Name, EHorizontalAlignment Align)
	{
		UScaleBox* Scale = MakeWidget<UScaleBox>(BP, Tree, Name);
		Scale->SetStretch(EStretch::ScaleToFit);
		Scale->SetStretchDirection(EStretchDirection::DownOnly);
		UScaleBoxSlot* ContentSlot = CastChecked<UScaleBoxSlot>(Scale->AddChild(&Content));
		ContentSlot->SetHorizontalAlignment(Align);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
		AddHorizontal(Columns, *Scale, FMargin(8.f, 0.f), true);
	}

	UButton* AddButton(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Parent,
		const TCHAR* Name, const TCHAR* TextName, const FText& Label, bool bPrimary = false)
	{
		UButton* Button = MakeWidget<UButton>(BP, Tree, Name, true);
		UTextBlock* Text = MakeWidget<UTextBlock>(BP, Tree, TextName, true);
		FButtonStyle Style = Button->GetStyle();
		Style.Normal = Solid(bPrimary ? Green : Paper, 7.f, 1.f, Line);
		Style.Hovered = Solid(bPrimary ? Color(20, 107, 70) : Mint, 7.f, 1.f, Green);
		Style.Pressed = Solid(bPrimary ? Ink : Mint, 7.f, 2.f, Green);
		Style.Disabled = Solid(Color(232, 236, 228), 7.f, 1.f, Line);
		Style.NormalPadding = Style.PressedPadding = FMargin(11.f, 2.f);
		Button->SetStyle(Style);
		StyleText(*Text, 15, bPrimary ? FLinearColor::White : Ink, TEXT("SemiBold"));
		Text->SetText(Label);
		UButtonSlot* Slot = CastChecked<UButtonSlot>(Button->AddChild(Text));
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(0.f));
		AddHorizontal(Parent, *Button, FMargin(2.f, 0.f));
		return Button;
	}

	bool BuildScreen(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass& TabClass)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_Root"));
		UOverlay* Host = MakeWidget<UOverlay>(BP, Tree, TEXT("Panel_PanelHost"), true);
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Bar"));
		Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Host->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* HostSlot = Root->AddChildToCanvas(Host);
		HostSlot->SetAnchors(FAnchors(.5f, 1.f));
		HostSlot->SetAlignment(FVector2D(.5f, 1.f));
		HostSlot->SetOffsets(FMargin(0.f, -36.f, 1160.f, 640.f));
		UCanvasPanelSlot* RowsSlot = Root->AddChildToCanvas(Rows);
		RowsSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		RowsSlot->SetAlignment(FVector2D(0.f, 1.f));
		RowsSlot->SetOffsets(FMargin(0.f));
		RowsSlot->SetAutoSize(true);
		RowsSlot->SetZOrder(10);

		UBorder* Menu = MakeWidget<UBorder>(BP, Tree, TEXT("Border_MenuRow"), true);
		USizeBox* MenuSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_MenuRow"), true);
		UHorizontalBox* Tabs = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Tabs"));
		FSlateBrush MenuBrush = Solid(Paper, 10.f);
		MenuBrush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 0.f, 0.f);
		Menu->SetBrush(MenuBrush);
		Menu->SetPadding(FMargin(4.f, 0.f));
		MenuSize->SetHeightOverride(32.f);
		MenuSize->AddChild(Tabs);
		SetBorderContent(*Menu, *MenuSize);
		AddRow(*Rows, *Menu, HAlign_Center);
		const TCHAR* Names[] = { TEXT("Operations"), TEXT("Facilities"), TEXT("Guests"), TEXT("Queues"),
			TEXT("SNS"), TEXT("Report"), TEXT("Staff"), TEXT("Density"), TEXT("Finale") };
		const TCHAR* Labels[] = { TEXT("운영"), TEXT("시설"), TEXT("관객"), TEXT("대기열"), TEXT("SNS"),
			TEXT("결산"), TEXT("스태프"), TEXT("밀집"), TEXT("피날레") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			UFF_LiveTabButton* Tab = Cast<UFF_LiveTabButton>(MakeWidgetOfClass(BP, Tree,
				FName(FString(TEXT("Tab_")) + Names[Index]), &TabClass));
			if (!Tab) { return false; }
			Tab->DisplayLabelOverride = FText::FromString(Labels[Index]);
			const FString IconName = Index == 8 ? TEXT("Events") : Names[Index];
			Tab->DesignerIcon = IconBrush(TEXT("Navigation/T_FF_LH_Icon_") + IconName + TEXT("_White_256"), 17.f);
			UHorizontalBoxSlot* Slot = Tabs->AddChildToHorizontalBox(Tab);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetPadding(FMargin(1.f, 1.f, 1.f, 0.f));
			if (Index == 5 || Index == 8) { Tab->SetVisibility(ESlateVisibility::Collapsed); }
		}

		USizeBox* StatusSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_StatusRow"), true);
		UBorder* Status = MakeWidget<UBorder>(BP, Tree, TEXT("Border_StatusRow"), true);
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_StatusColumns"));
		UHorizontalBox* Left = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Box_StatusLeft"));
		UHorizontalBox* Right = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Box_StatusRight"));
		StatusSize->SetHeightOverride(36.f);
		StatusSize->AddChild(Status);
		Status->SetBrush(Solid(Paper));
		Status->SetPadding(FMargin(8.f, 0.f));
		SetBorderContent(*Status, *Columns);
		AddRow(*Rows, *StatusSize, HAlign_Fill);
		AddStatusGroup(BP, Tree, *Left, TEXT("Box_FestivalName"), TEXT("Text_FestivalName"), LOCTEXT("FestivalPreview", "다대포 바다축제"));
		AddStatusGroup(BP, Tree, *Left, TEXT("Box_Gold"), TEXT("Text_Gold"), LOCTEXT("GoldPreview", "10,000"),
			TEXT("Metrics/T_FF_LH_Icon_Gold_White_256"), FLinearColor::White);
		UWidget* Revenue = AddStatusGroup(BP, Tree, *Left, TEXT("Box_TodayRevenue"), TEXT("Text_TodayRevenue"), LOCTEXT("RevenuePreview", "840"),
			TEXT("Metrics/T_FF_LH_Icon_Revenue_White_256"));
		Revenue->SetToolTipText(LOCTEXT("RevenueTooltip", "오늘 매출"));
		AddStatusGroup(BP, Tree, *Left, TEXT("Box_Staff"), TEXT("Text_Staff"), LOCTEXT("StaffPreview", "6명"),
			TEXT("Metrics/T_FF_LH_Icon_StaffCount_White_256"));
		UWidget* Guests = AddStatusGroup(BP, Tree, *Left, TEXT("Box_VisitorCount"), TEXT("Text_VisitorCount"), LOCTEXT("GuestsPreview", "104"),
			TEXT("Metrics/T_FF_LH_Icon_Visitors_White_256"));
		Guests->SetToolTipText(LOCTEXT("GuestsTooltip", "현재 축제장 안의 관객 수"));
		AddScaledColumn(BP, Tree, *Columns, *Left, TEXT("ScaleBox_StatusLeft"), HAlign_Left);

		UBorder* Day = MakeWidget<UBorder>(BP, Tree, TEXT("Border_DayStage"), true);
		UHorizontalBox* DayContent = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_DayStage"));
		UImage* Dot = MakeWidget<UImage>(BP, Tree, TEXT("Image_LiveDot"), true);
		FSlateBrush DotBrush = Solid(Color(223, 97, 64), 3.f);
		DotBrush.SetImageSize(FVector2D(6.f));
		Dot->SetBrush(DotBrush);
		AddHorizontal(*DayContent, *Dot, FMargin(0.f, 0.f, 7.f, 0.f));
		UTextBlock* DayText = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_DayStage"), true);
		Day->SetBrush(Solid(Mint, 14.f, 1.f, Green));
		Day->SetPadding(FMargin(14.f, 2.f));
		StyleText(*DayText, 15, Green, TEXT("SemiBold"));
		DayText->SetText(LOCTEXT("DayPreview", "1일차 · 라이브"));
		AddHorizontal(*DayContent, *DayText);
		SetBorderContent(*Day, *DayContent);
		AddHorizontal(*Columns, *Day);
		AddScaledColumn(BP, Tree, *Columns, *Right, TEXT("ScaleBox_StatusRight"), HAlign_Right);

		AddStatusGroup(BP, Tree, *Right, TEXT("Box_Clock"), TEXT("Text_Clock"), LOCTEXT("ClockPreview", "18:20 · 남은 160초"),
			TEXT("Metrics/T_FF_LH_Icon_Clock_White_256"));
		UHorizontalBox* Speed = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Box_Speed"), true);
		AddHorizontal(*Right, *Speed);
		AddButton(BP, Tree, *Speed, TEXT("Button_Speed1"), TEXT("Text_Speed1"), FText::FromString(TEXT("x1")));
		AddButton(BP, Tree, *Speed, TEXT("Button_Speed2"), TEXT("Text_Speed2"), FText::FromString(TEXT("x2")));
		AddButton(BP, Tree, *Speed, TEXT("Button_Speed4"), TEXT("Text_Speed4"), FText::FromString(TEXT("x4")));
		UButton* Cta = AddButton(BP, Tree, *Right, TEXT("Button_Cta"), TEXT("Text_CtaLabel"), LOCTEXT("CtaPreview", "다음으로"), true);
		Cta->SetVisibility(ESlateVisibility::Collapsed);

		// 잠금 설명은 상태 바를 밀지 않도록 바로 위 오른쪽에 배치한다.
		UTextBlock* Reason = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_SpeedReason"), true);
		StyleText(*Reason, 13, Amber, TEXT("Medium"));
		Reason->SetText(LOCTEXT("LockPreview", "메인 공연 중에는 1배속으로 고정됩니다."));
		Reason->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* ReasonSlot = Root->AddChildToCanvas(Reason);
		ReasonSlot->SetAnchors(FAnchors(1.f, 1.f));
		ReasonSlot->SetAlignment(FVector2D(1.f, 1.f));
		ReasonSlot->SetOffsets(FMargin(-16.f, -72.f, 0.f, 0.f));
		ReasonSlot->SetAutoSize(true);
		return true;
	}

	void Apply()
	{
		// 라이브 2종만 갱신한다. 셋업 HUD와 패널 및 클래스 설정은 보존한다.
		UWidgetBlueprint* Tab = ReplaceWidgetBlueprint(LogPrefix, TabPackage, Folder,
			*UFF_LiveTabButton::StaticClass(), BuildTab);
		if (!Tab || !Tab->GeneratedClass) { return; }
		UFF_LiveTabButton* TabDefaults = CastChecked<UFF_LiveTabButton>(Tab->GeneratedClass->GetDefaultObject());
		TabDefaults->SetContentColors(Ink, FLinearColor::White);
		Tab->MarkPackageDirty();
		if (!SaveAssetPackage(LogPrefix, *Tab)) { return; }
		UClass* TabClass = Tab->GeneratedClass;
		UWidgetBlueprint* Screen = ReplaceWidgetBlueprint(LogPrefix, BarPackage, Folder, *UFF_LiveHudScreen::StaticClass(),
			[TabClass](UWidgetBlueprint& BP, UWidgetTree& Tree) { return BuildScreen(BP, Tree, *TabClass); });
		if (Screen)
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_LIVE_BRIGHT_APPLIED: 라이브 HUD/탭 적용. 메뉴 32, 상태 36. 이후 Designer에서 수정한다."));
		}
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveHudBrightBottomDesign"),
		TEXT("라이브 HUD와 탭 WBP에 밝은 디자인을 적용한다. 디자이너 마감 후 재실행하지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
#endif
