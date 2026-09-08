// Explicit, editor-only migration for the build catalog's Designer surfaces.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FFBuildStudio"
namespace FFBuildStudioWbp
{
using namespace FFUIWbpBuilder;
const FString ChoicePackage = TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildCategoryChoice");
const FString TilePackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile");
const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild");
constexpr const TCHAR* LogPrefix = TEXT("FF_BUILD_STUDIO_UMG");

FSlateBrush Brush(const FLinearColor Color, float Radius = 6, const FLinearColor Outline = FLinearColor::Transparent)
{
	FSlateBrush B;
	B.DrawAs = ESlateBrushDrawType::RoundedBox;
	B.TintColor = Color;
	B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	B.OutlineSettings.CornerRadii = FVector4(Radius);
	B.OutlineSettings.Color = Outline;
	B.OutlineSettings.Width = Outline.A > 0 ? 1 : 0;
	return B;
}
UTextBlock* Text(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, FText Value,
	int32 Size, FLinearColor Color, bool Variable = false)
{
	auto* W = MakeWidget<UTextBlock>(BP, Tree, Name, Variable);
	StyleText(*W, Size, Color, TEXT("Medium"));
	W->SetText(Value);
	W->SetAutoWrapText(false);
	W->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	W->SetVisibility(ESlateVisibility::HitTestInvisible);
	return W;
}
UVerticalBoxSlot* V(UVerticalBox& Parent, UWidget& Child, bool Fill = false, FMargin Padding = FMargin(0))
{
	auto* Slot = Parent.AddChildToVerticalBox(&Child);
	Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
	Slot->SetHorizontalAlignment(HAlign_Fill);
	Slot->SetVerticalAlignment(VAlign_Fill);
	Slot->SetPadding(Padding);
	return Slot;
}
UHorizontalBoxSlot* H(UHorizontalBox& Parent, UWidget& Child, bool Fill = false, FMargin Padding = FMargin(0))
{
	auto* Slot = Parent.AddChildToHorizontalBox(&Child);
	Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
	Slot->SetVerticalAlignment(VAlign_Fill);
	Slot->SetPadding(Padding);
	return Slot;
}
USizeBox* Size(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, UWidget& Child, float Width, float Height)
{
	auto* Box = MakeWidget<USizeBox>(BP, Tree, Name);
	if (Width > 0) { Box->SetWidthOverride(Width); }
	if (Height > 0) { Box->SetHeightOverride(Height); }
	Box->AddChild(&Child);
	return Box;
}
UButton* Button(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, FName LabelName, FText Label, bool Primary = false)
{
	const auto& P = UFF_UIPalette::Get()->InGame;
	auto* B = MakeWidget<UButton>(BP, Tree, Name, true);
	FButtonStyle S;
	S.Normal = Brush(Primary ? P.TealDeep : P.PaperSunken);
	S.Hovered = Brush(P.Hover, 6, P.Teal);
	S.Pressed = Brush(P.TealDeep);
	S.Disabled = Brush(P.PaperSunken);
	S.NormalPadding = S.PressedPadding = FMargin(10, 3);
	B->SetStyle(S);
	auto* LabelWidget = Text(BP, Tree, LabelName, Label, 12, Primary ? P.White : P.Ink);
	LabelWidget->bIsVariable = LabelName == TEXT("Text_Label");
	auto* Slot = Cast<UButtonSlot>(B->AddChild(LabelWidget));
	Slot->SetHorizontalAlignment(HAlign_Center);
	Slot->SetVerticalAlignment(VAlign_Center);
	return B;
}

bool BuildChoice(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
	const auto& P = UFF_UIPalette::Get()->InGame;
	auto* B = Button(BP, Tree, TEXT("Button_Root"), TEXT("Text_Label"), LOCTEXT("Choice", "전체 시설"));
	Tree.RootWidget = Size(BP, Tree, TEXT("Size_Choice"), *B, 0, 24);
	return true;
}

bool BuildTile(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
	const auto& P = UFF_UIPalette::Get()->InGame;
	auto* Card = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Card"), true);
	Card->SetBrush(Brush(P.White, 8, P.Line));
	Card->SetPadding(FMargin(10, 7, 10, 7));
	Card->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	Tree.RootWidget = Size(BP, Tree, TEXT("SizeBox_Card"), *Card,
		FFPrepBuildTileLayout::CardWidth, FFPrepBuildTileLayout::CardHeight);
	auto* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Content"));
	Card->AddChild(Content);
	auto* Backdrop = MakeWidget<UBorder>(BP, Tree, TEXT("Border_ThumbnailBackdrop"));
	Backdrop->SetBrush(Brush(P.StatusBar, 6));
	Backdrop->SetPadding(FMargin(0));
	auto* Scale = MakeWidget<UScaleBox>(BP, Tree, TEXT("ScaleBox_Thumbnail"));
	Scale->SetStretch(EStretch::ScaleToFit);
	auto* Image = MakeWidget<UImage>(BP, Tree, TEXT("Image_Thumbnail"), true);
	Image->SetDesiredSizeOverride(FVector2D(FFPrepBuildTileLayout::ThumbnailWidth, FFPrepBuildTileLayout::ThumbnailHeight));
	Scale->AddChild(Image);
	auto* ThumbnailStack = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Thumbnail"));
	ThumbnailStack->AddChildToOverlay(Scale)->SetHorizontalAlignment(HAlign_Center);
	auto* Placeholder = Text(BP, Tree, TEXT("Text_ThumbnailPlaceholder"), LOCTEXT("ThumbnailLoading", "이미지 준비 중"), 10, P.InkMuted, true);
	auto* PlaceholderSlot = ThumbnailStack->AddChildToOverlay(Placeholder);
	PlaceholderSlot->SetHorizontalAlignment(HAlign_Center);
	PlaceholderSlot->SetVerticalAlignment(VAlign_Center);
	Backdrop->AddChild(ThumbnailStack);
	V(*Content, *Size(BP, Tree, TEXT("SizeBox_Thumbnail"), *Backdrop,
		FFPrepBuildTileLayout::ThumbnailWidth, FFPrepBuildTileLayout::ThumbnailHeight));
	V(*Content, *Text(BP, Tree, TEXT("Text_CategoryPath"), LOCTEXT("TileCategory", "부스  /  먹거리"), 10, P.InkMuted, true), false, FMargin(0, 4, 0, 0));
	auto* Name = Text(BP, Tree, TEXT("Text_Name"), LOCTEXT("TileName", "푸드트럭"), 14, P.Ink, true);
	StyleText(*Name, 14, P.Ink, TEXT("SemiBold"));
	V(*Content, *Name, false, FMargin(0, 1, 0, 2));
	auto* Meta = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Meta"));
	H(*Meta, *Text(BP, Tree, TEXT("Text_Cost"), LOCTEXT("Price", "700 Gold"), 12, P.TealDeep, true), true);
	H(*Meta, *Text(BP, Tree, TEXT("Text_State"), LOCTEXT("Available", "배치 가능"), 10, P.TealDeep, true));
	V(*Content, *Meta);
	V(*Content, *Text(BP, Tree, TEXT("Text_BuildLimit"), LOCTEXT("Count", "설치 0 / 10"), 10, P.InkMuted, true), false, FMargin(0, 2, 0, 0));
	return true;
}

bool BuildPanel(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass* TileClass, UWidget* PreservedCompact)
{
	const auto& P = UFF_UIPalette::Get()->InGame;
	auto* Root = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PanelRoot"), true);
	Root->SetBrush(Brush(P.Paper, 10, P.LineStrong));
	Root->SetPadding(FMargin(18, 10));
	Tree.RootWidget = Root;
	auto* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Content"));
	Root->AddChild(Content);
	auto* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Box_Header"), true);
	auto* Title = Text(BP, Tree, TEXT("Text_PanelTitle"), LOCTEXT("Title", "축제 만들기"), 20, P.TealDeep, true);
	StyleText(*Title, 20, P.TealDeep, TEXT("SemiBold"));
	H(*Header, *Title)->SetVerticalAlignment(VAlign_Center);
	auto* Subtitle = Text(BP, Tree, TEXT("Text_StudioHint"), LOCTEXT("StudioHint", "시설을 고르고, 나만의 공간을 꾸며 보세요"), 11, P.InkMuted);
	H(*Header, *Subtitle, true, FMargin(14, 0, 14, 0))->SetVerticalAlignment(VAlign_Center);
	auto* Search = MakeWidget<UEditableTextBox>(BP, Tree, TEXT("Search_Facilities"), true);
	Search->SetHintText(LOCTEXT("Search", "이름 · 종류 검색"));
	Search->SetMinDesiredWidth(180);
	FEditableTextBoxStyle SearchStyle = Search->GetWidgetStyle();
	SearchStyle.BackgroundImageNormal = Brush(P.White, 6, P.Line);
	SearchStyle.BackgroundImageHovered = Brush(P.White, 6, P.Teal);
	SearchStyle.BackgroundImageFocused = Brush(P.White, 6, P.TealDeep);
	SearchStyle.ForegroundColor = P.Ink;
	SearchStyle.FocusedForegroundColor = P.Ink;
	SearchStyle.Padding = FMargin(10, 5);
	SearchStyle.TextStyle.SetFont(Subtitle->GetFont());
	Search->SetWidgetStyle(SearchStyle);
	H(*Header, *Search, false, FMargin(0, 0, 14, 0))->SetVerticalAlignment(VAlign_Center);
	auto* GoldBacking = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Gold"));
	GoldBacking->SetBrush(Brush(P.TealDeep, 8));
	GoldBacking->SetPadding(FMargin(12, 6));
	GoldBacking->AddChild(Text(BP, Tree, TEXT("Text_Gold"), LOCTEXT("Gold", "보유 4,106 Gold"), 13, P.White, true));
	H(*Header, *GoldBacking, false, FMargin(0, 0, 8, 0))->SetVerticalAlignment(VAlign_Center);
	H(*Header, *Button(BP, Tree, TEXT("Button_InspectFacilities"), TEXT("Text_InspectFacilities"), LOCTEXT("InspectFacilities", "시설 확인  B")), false, FMargin(0, 0, 8, 0))->SetVerticalAlignment(VAlign_Center);
	H(*Header, *Button(BP, Tree, TEXT("Button_Close"), TEXT("Text_CloseLabel"), FText::FromString(TEXT("×"))))->SetVerticalAlignment(VAlign_Center);
	V(*Content, *Header, false, FMargin(0, 0, 0, 8));

	auto* Catalog = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Panel_Catalog"), true);
	auto* Sidebar = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Categories"));
	Sidebar->SetBrush(Brush(P.StatusBar, 8));
	Sidebar->SetPadding(FMargin(8, 4));
	auto* Tools = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Tools"), true);
	Sidebar->AddChild(Tools);
	// Stable compatibility names; the full data-driven major category list replaces these at runtime.
	for (auto* B : {
		Button(BP, Tree, TEXT("Button_FilterAll"), TEXT("Text_FilterAll"), LOCTEXT("All", "전체"), true),
		Button(BP, Tree, TEXT("Button_FilterBooth"), TEXT("Text_FilterBooth"), LOCTEXT("Booth", "부스")),
		Button(BP, Tree, TEXT("Button_FilterPerformance"), TEXT("Text_FilterPerformance"), LOCTEXT("Performance", "공연")) })
	{
		H(*Tools, *B);
	}
	H(*Catalog, *Size(BP, Tree, TEXT("Size_CategorySidebar"), *Sidebar, 128, 0), false, FMargin(0, 0, 14, 0));
	auto* Browser = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Browser"));
	H(*Catalog, *Browser, true);
	auto* SubRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_SubcategoryHeader"));
	H(*SubRow, *Text(BP, Tree, TEXT("Text_CategoryBreadcrumb"), LOCTEXT("AllFacilities", "전체 시설"), 13, P.TealDeep, true), false, FMargin(0, 0, 12, 0))->SetVerticalAlignment(VAlign_Center);
	auto* SubScroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("ScrollBox_Subcategories"));
	SubScroll->SetOrientation(Orient_Horizontal);
	SubScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	SubScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	auto* Sub = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Subcategories"), true);
	SubScroll->AddChild(Sub);
	H(*SubRow, *SubScroll, true);
	V(*Browser, *Size(BP, Tree, TEXT("Size_SubcategoryHeader"), *SubRow, 0, 26), false, FMargin(0, 0, 0, 6));
	auto* Tiles = MakeWidget<UFF_PrepBuildTileView>(BP, Tree, TEXT("TileView_Facilities"), true);
	Tiles->ConfigureEntryClass(TileClass);
	Tiles->SetScrollbarVisibility(ESlateVisibility::Visible);
	V(*Browser, *Tiles, true);
	auto* Footer = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Footer"));
	H(*Footer, *Text(BP, Tree, TEXT("Text_Status"), LOCTEXT("Hint", "카드 선택: 배치 시작  ·  휠: 목록 탐색  ·  B: 배치 시설 확인"), 10, P.InkMuted, true), true);
	H(*Footer, *Text(BP, Tree, TEXT("Text_ItemCount"), LOCTEXT("ItemCount", "시설 14 / 14"), 10, P.InkMuted, true));
	V(*Browser, *Footer, false, FMargin(0, 4, 0, 0));
	V(*Content, *Catalog, true);
	if (PreservedCompact)
	{
		// The existing compact placement strip and its team-tuned padding are preserved verbatim.
		// UMG children are owned by WidgetTree, not by their visual parent. Duplicate the
		// tree first so a copied panel cannot retain private references into the old asset tree.
		TStrongObjectPtr<UWidgetTree> CopyTree(DuplicateObject<UWidgetTree>(
			CastChecked<UWidgetTree>(PreservedCompact->GetOuter()), GetTransientPackage()));
		UWidget* Compact = CopyTree->FindWidget(TEXT("Panel_PlacementCompact"));
		if (!Compact) { return false; }
		Compact->RemoveFromParent();
		Compact->Rename(*Compact->GetName(), &Tree, REN_DontCreateRedirectors | REN_NonTransactional);
		UWidgetTree::ForWidgetAndChildren(Compact, [&Tree](UWidget* Widget)
		{
			Widget->Rename(*Widget->GetName(), &Tree, REN_DontCreateRedirectors | REN_NonTransactional);
		});
		V(*Content, *Compact);
		Tree.ForEachWidget([&BP](UWidget* W)
		{
			if (!BP.WidgetVariableNameToGuidMap.Contains(W->GetFName()))
			{
				BP.OnVariableAdded(W->GetFName());
			}
		});
	}
	return PreservedCompact != nullptr;
}

template <typename BuildFunction>
bool ReplaceVisualTree(UWidgetBlueprint& BP, BuildFunction Build)
{
	UWidgetTree* OldTree = BP.WidgetTree;
	if (!OldTree) { return false; }
	TStrongObjectPtr<UWidgetTree> PreviousTree(OldTree);
	const auto PreviousGuids = BP.WidgetVariableNameToGuidMap;
	// The command is explicit and limited to these build assets. Graphs and class defaults stay intact.
	OldTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	BP.WidgetTree = NewObject<UWidgetTree>(&BP, TEXT("WidgetTree"), RF_Transactional);
	BP.WidgetVariableNameToGuidMap.Reset();
	if (!Build(*BP.WidgetTree) || !SaveNewBlueprint(LogPrefix, BP))
	{
		BP.WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		OldTree->Rename(TEXT("WidgetTree"), &BP, REN_DontCreateRedirectors | REN_NonTransactional);
		BP.WidgetTree = OldTree;
		BP.WidgetVariableNameToGuidMap = PreviousGuids;
		SaveNewBlueprint(LogPrefix, BP);
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: restored previous visual tree: %s"), LogPrefix, *BP.GetName());
		return false;
	}
	return true;
}

void RefreshBuildStudio()
{
	UWidgetBlueprint* Choice = EnsureWidgetBlueprint(LogPrefix, ChoicePackage,
		TEXT("/Game/FestaFiesta/UI/Building"), *UFF_PrepChoiceButton::StaticClass(), BuildChoice);
	UWidgetBlueprint* Tile = LoadExistingBlueprint(TilePackage);
	UWidgetBlueprint* Panel = LoadExistingBlueprint(PanelPackage);
	if (!Choice || !Choice->GeneratedClass || !Tile || !Panel || !Panel->WidgetTree)
	{
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: required build WBPs are missing"), LogPrefix);
		return;
	}
	UWidget* Compact = Panel->WidgetTree->FindWidget(TEXT("Panel_PlacementCompact"));
	if (!ReplaceVisualTree(*Choice, [Choice](UWidgetTree& Tree) { return BuildChoice(*Choice, Tree); })) { return; }
	if (!Compact || !ReplaceVisualTree(*Tile, [Tile](UWidgetTree& Tree) { return BuildTile(*Tile, Tree); })) { return; }
	if (!ReplaceVisualTree(*Panel, [Panel, Tile, Compact](UWidgetTree& Tree)
		{ return BuildPanel(*Panel, Tree, Tile->GeneratedClass, Compact); })) { return; }
	if (FClassProperty* Property = FindFProperty<FClassProperty>(Panel->GeneratedClass.Get(), TEXT("CategoryChoiceClass")))
	{
		Property->SetObjectPropertyValue_InContainer(Panel->GeneratedClass->GetDefaultObject(), Choice->GeneratedClass);
	}
	if (SaveNewBlueprint(LogPrefix, *Panel))
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_DONE: catalog, tile and category choice saved; compact strip preserved"), LogPrefix);
	}
}
FAutoConsoleCommand RefreshCommand(TEXT("FF.RefreshBuildStudioUMG"),
	TEXT("Rebuilds only the build catalog/tile Designer surfaces; preserves the existing compact placement strip."),
	FConsoleCommandDelegate::CreateStatic(&RefreshBuildStudio));
}
#undef LOCTEXT_NAMESPACE
#endif
