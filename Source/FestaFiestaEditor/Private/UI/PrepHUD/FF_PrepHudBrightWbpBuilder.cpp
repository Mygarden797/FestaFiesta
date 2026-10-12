// 승인된 밝은 하단 HUD를 한 번 적용한다. 이후 치수/글꼴/배치는 UMG Designer가 소유한다.
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Building/Presentation/FF_BuildUIAssetPaths.h"
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
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepHudBrightBuilder"

namespace FFPrepHudBrightBuilder
{
	using namespace FFUIWbpBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_PREP_BRIGHT");
	const FString Folder = FFBuildUIAssetPaths::SharedFolder;
	const FString BarPackage = Folder / TEXT("WBP_FF_PrepHudBar");
	const FString TabPackage = Folder / TEXT("WBP_FF_PrepHudTabButton");
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
		Frame->SetMinDesiredWidth(132.f);
		Frame->AddChild(Button);
		FButtonStyle Style = Button->GetStyle();
		Style.Normal = Solid(Paper, 8.f);
		Style.Hovered = Solid(Mint, 8.f, 1.f, Green);
		// 선택 상태는 테두리 두께도 달라진다. 색상만으로 구분하지 않는다.
		Style.Pressed = Solid(Green, 8.f, 2.f, Ink);
		Style.Disabled = Solid(Paper, 8.f);
		Style.NormalPadding = Style.PressedPadding = FMargin(18.f, 3.f);
		Button->SetStyle(Style);
		Icon->SetBrush(IconBrush(TEXT("Navigation/T_FF_LH_Icon_Facilities_White_256"), 19.f));
		Icon->SetDesiredSizeOverride(FVector2D(19.f, 19.f));
		Icon->SetColorAndOpacity(Ink);
		AddHorizontal(*Content, *Icon, FMargin(0.f, 0.f, 8.f, 0.f));
		StyleText(*Label, 17, Ink, TEXT("SemiBold"));
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
			Icon->SetBrush(IconBrush(IconPath, 23.f));
			Icon->SetDesiredSizeOverride(FVector2D(23.f, 23.f));
			Icon->SetColorAndOpacity(IconTint);
			AddHorizontal(*Group, *Icon, FMargin(0.f, 0.f, 7.f, 0.f));
		}
		StyleText(*Text, 17, Ink, TEXT("SemiBold"));
		Text->SetText(Preview);
		Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddHorizontal(*Group, *Text);
		AddHorizontal(Parent, *Group, FMargin(0.f, 0.f, 24.f, 0.f));
		return Group;
	}

	bool BuildBar(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass& TabClass)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_Root"));
		UOverlay* Host = MakeWidget<UOverlay>(BP, Tree, TEXT("Panel_PanelHost"), true);
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Bar"));
		Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Host->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* HostSlot = Root->AddChildToCanvas(Host);
		HostSlot->SetAnchors(FAnchors(0.5f, 1.f));
		HostSlot->SetAlignment(FVector2D(.5f, 1.f));
		HostSlot->SetOffsets(FMargin(0.f, -52.f, 1160.f, 640.f));
		UCanvasPanelSlot* RowsSlot = Root->AddChildToCanvas(Rows);
		RowsSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		RowsSlot->SetAlignment(FVector2D(0.f, 1.f));
		RowsSlot->SetOffsets(FMargin(0.f));
		RowsSlot->SetAutoSize(true);
		RowsSlot->SetZOrder(10);

		// 자동 크기와 아래쪽 앵커를 함께 써서 행 높이를 바꿔도 화면 최하단에 붙인다.
		UBorder* Menu = MakeWidget<UBorder>(BP, Tree, TEXT("Border_MenuRow"), true);
		USizeBox* MenuSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_MenuRow"), true);
		UHorizontalBox* Tabs = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Tabs"));
		FSlateBrush MenuBrush = Solid(Paper, 14.f);
		MenuBrush.OutlineSettings.CornerRadii = FVector4(14.f, 14.f, 0.f, 0.f);
		Menu->SetBrush(MenuBrush);
		Menu->SetPadding(FMargin(6.f, 0.f));
		MenuSize->SetHeightOverride(44.f);
		MenuSize->AddChild(Tabs);
		SetBorderContent(*Menu, *MenuSize);
		AddRow(*Rows, *Menu, HAlign_Center);
		const TCHAR* Names[] = { TEXT("Build"), TEXT("Program"), TEXT("Staff"), TEXT("Promotion"), TEXT("Finale"), TEXT("Check") };
		const TCHAR* Labels[] = { TEXT("배치"), TEXT("프로그램"), TEXT("스태프"), TEXT("홍보"), TEXT("피날레"), TEXT("점검") };
		const TCHAR* Icons[] = { TEXT("Navigation/T_FF_LH_Icon_Facilities_White_256"), TEXT("Navigation/T_FF_LH_Icon_Events_White_256"),
			TEXT("Navigation/T_FF_LH_Icon_Staff_White_256"), TEXT("Navigation/T_FF_LH_Icon_SNS_White_256"),
			TEXT("Navigation/T_FF_LH_Icon_Events_White_256"), TEXT("Status/T_FF_LH_Icon_StatusCheck_White_256") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			UFF_PrepHudTabButton* Tab = Cast<UFF_PrepHudTabButton>(MakeWidgetOfClass(BP, Tree,
				FName(FString(TEXT("Tab_")) + Names[Index]), &TabClass));
			if (!Tab) { return false; }
			Tab->DisplayLabelOverride = FText::FromString(Labels[Index]);
			Tab->DesignerIcon = IconBrush(Icons[Index], 19.f);
			UHorizontalBoxSlot* Slot = Tabs->AddChildToHorizontalBox(Tab);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetPadding(FMargin(1.f, 3.f, 1.f, 0.f));
			// 마지막 날 피날레 노출이라는 기존 규칙은 유지한다. 셋업에는 요청한 다섯 탭만 보인다.
			if (Index == 4) { Tab->SetVisibility(ESlateVisibility::Collapsed); }
		}

		USizeBox* StatusSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_StatusRow"), true);
		UBorder* Status = MakeWidget<UBorder>(BP, Tree, TEXT("Border_StatusRow"), true);
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_StatusColumns"));
		UHorizontalBox* Left = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_StatusLeft"));
		UHorizontalBox* Right = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_StatusRight"));
		StatusSize->SetHeightOverride(52.f);
		StatusSize->AddChild(Status);
		Status->SetBrush(Solid(Paper));
		Status->SetPadding(FMargin(24.f, 0.f));
		SetBorderContent(*Status, *Columns);
		AddRow(*Rows, *StatusSize, HAlign_Fill);
		AddStatusGroup(BP, Tree, *Left, TEXT("Box_FestivalName"), TEXT("Text_FestivalName"), LOCTEXT("FestivalPreview", "다대포 바다축제"));
		AddStatusGroup(BP, Tree, *Left, TEXT("Box_Gold"), TEXT("Text_Gold"), LOCTEXT("GoldPreview", "10,000"),
			TEXT("Metrics/T_FF_LH_Icon_Gold_White_256"), FLinearColor::White);
		AddStatusGroup(BP, Tree, *Left, TEXT("Box_Staff"), TEXT("Text_Staff"), LOCTEXT("StaffPreview", "6명"),
			TEXT("Metrics/T_FF_LH_Icon_StaffCount_White_256"));
		// 가변 길이 좌우 그룹이 중앙 단계 칩을 밀지 않도록 같은 Fill 비율을 사용한다.
		Left->SetClipping(EWidgetClipping::ClipToBounds);
		AddHorizontal(*Columns, *Left, FMargin(0.f, 0.f, 12.f, 0.f), true);
		UBorder* Day = MakeWidget<UBorder>(BP, Tree, TEXT("Border_DayStage"), true);
		UTextBlock* DayText = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_DayStage"), true);
		Day->SetBrush(Solid(Mint, 17.f, 1.f, Green));
		Day->SetPadding(FMargin(22.f, 5.f));
		StyleText(*DayText, 17, Green, TEXT("SemiBold"));
		DayText->SetText(LOCTEXT("DayPreview", "1일차 · 셋업"));
		SetBorderContent(*Day, *DayText);
		AddHorizontal(*Columns, *Day);
		AddHorizontal(*Columns, *Right, FMargin(12.f, 0.f, 0.f, 0.f), true);

		// 사유 배지는 오른쪽 폭 안에서 말줄임하고, 전문은 기존 텍스트를 툴팁으로 제공한다.
		UBorder* ReasonBox = MakeWidget<UBorder>(BP, Tree, TEXT("Box_Cta"), true);
		UHorizontalBox* ReasonContent = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_CtaReason"));
		UTextBlock* ReasonIcon = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_CtaReasonIcon"));
		UTextBlock* Reason = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_CtaReason"), true);
		ReasonBox->SetBrush(Solid(Color(255, 248, 226), 17.f, 1.f, Color(218, 167, 68)));
		ReasonBox->SetPadding(FMargin(14.f, 5.f));
		StyleText(*ReasonIcon, 17, Amber, TEXT("Bold"));
		ReasonIcon->SetText(FText::FromString(TEXT("!")));
		AddHorizontal(*ReasonContent, *ReasonIcon, FMargin(0.f, 0.f, 8.f, 0.f));
		StyleText(*Reason, 15, Amber, TEXT("Medium"));
		Reason->SetText(LOCTEXT("ReasonPreview", "메인공연장 설치 필요 · 0/1"));
		Reason->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddHorizontal(*ReasonContent, *Reason, FMargin(0.f), true);
		SetBorderContent(*ReasonBox, *ReasonContent);
		UHorizontalBoxSlot* ReasonSlot = Right->AddChildToHorizontalBox(ReasonBox);
		ReasonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ReasonSlot->SetHorizontalAlignment(HAlign_Right);
		ReasonSlot->SetVerticalAlignment(VAlign_Center);
		return true;
	}

	void Apply()
	{
		// 이 명령은 승인된 HUD/탭 트리 둘만 교체한다. 시설 카탈로그·패널·클래스 매핑은 건드리지 않는다.
		UWidgetBlueprint* Tab = ReplaceWidgetBlueprint(LogPrefix, TabPackage, Folder,
			*UFF_PrepHudTabButton::StaticClass(), BuildTab);
		if (!Tab || !Tab->GeneratedClass) { return; }
		if (UFF_PrepHudTabButton* TabDefaults = Cast<UFF_PrepHudTabButton>(Tab->GeneratedClass->GetDefaultObject()))
		{
			TabDefaults->Modify();
			TabDefaults->SetContentColors(Ink, FLinearColor::White);
			Tab->MarkPackageDirty();
			if (!SaveAssetPackage(LogPrefix, *Tab)) { return; }
		}
		UClass* TabClass = Tab->GeneratedClass;
		UWidgetBlueprint* Bar = ReplaceWidgetBlueprint(LogPrefix, BarPackage, Folder, *UFF_PrepHudBar::StaticClass(),
			[TabClass](UWidgetBlueprint& BP, UWidgetTree& Tree) { return BuildBar(BP, Tree, *TabClass); });
		if (!Bar || !Bar->GeneratedClass) { return; }
		UObject* Defaults = Bar->GeneratedClass->GetDefaultObject();
		Defaults->Modify();
		if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Defaults->GetClass(), TEXT("bUseDesignerStyle")))
		{
			Property->SetPropertyValue_InContainer(Defaults, true);
		}
		// 탭 별 아이콘은 DesignerIcon에 저장했다. 기존 이름 규칙으로 없는 아이콘을 재검색하지 않는다.
		if (FStrProperty* Property = FindFProperty<FStrProperty>(Defaults->GetClass(), TEXT("TabIconFolder")))
		{
			Property->SetPropertyValue_InContainer(Defaults, FString());
		}
		Bar->MarkPackageDirty();
		if (SaveAssetPackage(LogPrefix, *Bar))
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("FF_PREP_BRIGHT_APPLIED: HUD와 탭 2종 적용. 행 높이/글꼴/여백은 Designer에서 조절한다."));
		}
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepHudBrightBottomDesign"),
		TEXT("승인된 밝은 하단 HUD를 적용한다. HUD/탭 WBP 두 트리를 교체하므로 디자이너 마감 후 재실행하지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
#endif
