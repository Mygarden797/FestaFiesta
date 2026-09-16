// One-time, editor-only UMG scaffolding. Existing assets are never regenerated.
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexImage.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodexBuilder"

namespace FFFacilityCodexWbpBuilder
{
	using namespace FFUIWbpBuilder;
	const TCHAR* const Prefix = TEXT("FF_FACILITY_CODEX_UMG");
	const FString Folder(TEXT("/Game/FestaFiesta/UI/FacilityCodex"));
	const FString CardPath = Folder / TEXT("WBP_FF_FacilityCodexCard");
	const FString ScreenPath = Folder / TEXT("WBP_FF_FacilityCodexScreen");
	const FString LauncherPath = Folder / TEXT("WBP_FF_FacilityCodexLauncher");
	const FString CatalogLauncherPath = Folder / TEXT("WBP_FF_FacilityCodexCatalogLauncher");

	FSlateBrush Brush(FLinearColor Fill, FLinearColor Line = FLinearColor::Transparent, float Width = 0.f, float Radius = 10.f)
	{
		FSlateBrush Result;
		Result.DrawAs = ESlateBrushDrawType::RoundedBox;
		Result.TintColor = FSlateColor(Fill);
		Result.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), Width);
		return Result;
	}

	struct FLayout
	{
		UWidgetBlueprint& BP;
		UWidgetTree& Tree;
		const FFF_UIPaletteInGame& P = GetDefault<UFF_UIPalette>()->InGame;

		template<class T> T* Make(FName Name, bool Variable = false) { return MakeWidget<T>(BP, Tree, Name, Variable); }
		UTextBlock* Text(FName Name, FText Value, int32 Size = 18, bool Variable = false, bool Muted = false)
		{
			UTextBlock* Result = Make<UTextBlock>(Name, Variable);
			StyleText(*Result, Size, Muted ? P.InkMuted : P.Ink, TEXT("Medium"));
			Result->SetText(Value);
			Result->SetAutoWrapText(true);
			Result->SetVisibility(ESlateVisibility::HitTestInvisible);
			return Result;
		}
		UBorder* Border(FName Name, FLinearColor Color, FMargin Padding = FMargin(0), bool Variable = false)
		{
			UBorder* Result = Make<UBorder>(Name, Variable);
			Result->SetBrush(Brush(Color));
			Result->SetPadding(Padding);
			return Result;
		}
		UButton* Button(FName Name, FText Caption, bool Primary = false)
		{
			UButton* Result = Make<UButton>(Name, true);
			FButtonStyle Style;
			Style.Normal = Brush(Primary ? P.Teal : P.Paper, Primary ? P.Teal : P.Line, 1.f, 7.f);
			Style.Hovered = Brush(Primary ? P.TealDeep : P.Hover, P.Teal, 1.f, 7.f);
			Style.Pressed = Brush(P.TealSoft, P.TealDeep, 2.f, 7.f);
			// Selected tabs are disabled: an explicit outline and filled background identifies them.
			Style.Disabled = Brush(P.TealSoft, P.TealDeep, 2.f, 7.f);
			Style.NormalPadding = FMargin(18.f, 10.f);
			Style.PressedPadding = FMargin(18.f, 11.f, 18.f, 9.f);
			Result->SetStyle(Style);
			UTextBlock* Label = Text(FName(Name.ToString() + TEXT("_Label")), Caption, 17);
			Label->SetAutoWrapText(false);
			Label->SetJustification(ETextJustify::Center);
			if (Primary) Label->SetColorAndOpacity(P.White);
			Result->AddChild(Label);
			return Result;
		}
		USizeBox* Size(FName Name, float Width, float Height)
		{
			USizeBox* Result = Make<USizeBox>(Name);
			if (Width > 0) Result->SetWidthOverride(Width);
			if (Height > 0) Result->SetHeightOverride(Height);
			return Result;
		}
		UVerticalBoxSlot* V(UVerticalBox* Parent, UWidget* Child, FMargin Padding = FMargin(0), bool Fill = false)
		{
			UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
			Slot->SetPadding(Padding);
			Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
			return Slot;
		}
		UHorizontalBoxSlot* H(UHorizontalBox* Parent, UWidget* Child, FMargin Padding = FMargin(0), bool Fill = false)
		{
			UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Child);
			Slot->SetPadding(Padding);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
			return Slot;
		}
		UOverlaySlot* O(UOverlay* Parent, UWidget* Child)
		{
			UOverlaySlot* Slot = Parent->AddChildToOverlay(Child);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			return Slot;
		}
		UWidget* Thumbnail(const FString& Side, float Width, float Height)
		{
			USizeBox* Bounds = Size(FName(TEXT("Size_Thumb") + Side), Width, Height);
			UBorder* Back = Border(FName(TEXT("Border_Thumb") + Side), P.PaperSunken, FMargin(7));
			Bounds->AddChild(Back);
			UOverlay* Overlay = Make<UOverlay>(FName(TEXT("Overlay_Thumb") + Side));
			Back->AddChild(Overlay);
			UTextBlock* Placeholder = Text(FName(TEXT("Text_") + Side + TEXT("Fallback")), LOCTEXT("Facility", "시설"), 18, true, true);
			UOverlaySlot* PlaceholderSlot = Overlay->AddChildToOverlay(Placeholder);
			PlaceholderSlot->SetHorizontalAlignment(HAlign_Center);
			PlaceholderSlot->SetVerticalAlignment(VAlign_Center);
			UScaleBox* Fit = Make<UScaleBox>(FName(TEXT("Scale_Thumb") + Side));
			Fit->SetStretch(EStretch::ScaleToFit);
			UFF_FacilityCodexImage* Image = Make<UFF_FacilityCodexImage>(FName(TEXT("Image_") + Side), true);
			Image->SetVisibility(ESlateVisibility::Hidden);
			Fit->AddChild(Image);
			O(Overlay, Fit);
			return Bounds;
		}
	};

	bool Card(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		FLayout L{ BP, Tree };
		USizeBox* Bounds = L.Size(TEXT("Size_Card"), 236, 224);
		Tree.RootWidget = Bounds;
		UOverlay* Overlay = L.Make<UOverlay>(TEXT("Overlay_Card"));
		Bounds->AddChild(Overlay);
		UBorder* CardBody = L.Border(TEXT("Border_Card"), L.P.White, FMargin(12));
		CardBody->SetBrush(Brush(L.P.White, L.P.Line, 1));
		L.O(Overlay, CardBody);
		UVerticalBox* Content = L.Make<UVerticalBox>(TEXT("Vertical_Card"));
		CardBody->AddChild(Content);
		USizeBox* NameBounds = L.Size(TEXT("Size_Name"), 0, 28);
		NameBounds->AddChild(L.Text(TEXT("Text_Name"), LOCTEXT("CardName", "함께 즐기는 달콤한 휴식"), 16, true));
		NameBounds->SetClipping(EWidgetClipping::ClipToBounds);
		L.V(Content, NameBounds);
		UHorizontalBox* RecipeImages = L.Make<UHorizontalBox>(TEXT("Horizontal_RecipeImages"));
		L.H(RecipeImages, L.Thumbnail(TEXT("Target"), 88, 68));
		L.H(RecipeImages, L.Text(TEXT("Text_Plus"), LOCTEXT("Plus", "+"), 23), FMargin(7, 0));
		L.H(RecipeImages, L.Thumbnail(TEXT("Partner"), 88, 68));
		L.V(Content, RecipeImages, FMargin(0, 0, 0, 5));
		USizeBox* RecipeBounds = L.Size(TEXT("Size_Recipe"), 0, 34);
		RecipeBounds->AddChild(L.Text(TEXT("Text_Recipe"), LOCTEXT("Recipe", "아이스크림 + 그늘 쉼터"), 13, true, true));
		RecipeBounds->SetClipping(EWidgetClipping::ClipToBounds);
		L.V(Content, RecipeBounds);
		UTextBlock* Bonus = L.Text(TEXT("Text_Bonus"), LOCTEXT("CardBonus", "수익 +10%"), 19, true);
		Bonus->SetColorAndOpacity(L.P.TealDeep);
		L.V(Content, Bonus);
		L.V(Content, L.Text(TEXT("Text_State"), LOCTEXT("CardState", "미발견 · 시설 설치 필요"), 12, true, true), FMargin(0, 3, 0, 0));
		UBorder* Selection = L.Border(TEXT("Border_Selection"), FLinearColor::Transparent, FMargin(0), true);
		Selection->SetBrush(Brush(FLinearColor::Transparent, L.P.TealDeep, 3));
		Selection->SetVisibility(ESlateVisibility::Collapsed);
		L.O(Overlay, Selection);
		return true;
	}

	bool Screen(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass* CardClass)
	{
		FLayout L{ BP, Tree };
		// This transparent fullscreen hit target blocks clicks outside the panel without implying pause.
		UBorder* Root = L.Border(TEXT("Border_InputShield"), FLinearColor::Transparent, FMargin(24));
		Tree.RootWidget = Root;
		UScaleBox* Fit = L.Make<UScaleBox>(TEXT("Scale_Screen"));
		Fit->SetStretch(EStretch::ScaleToFit);
		Fit->SetStretchDirection(EStretchDirection::DownOnly);
		Root->AddChild(Fit);
		USizeBox* Bounds = L.Size(TEXT("Size_Screen"), 1400, 860);
		Fit->AddChild(Bounds);
		UBorder* Paper = L.Border(TEXT("Border_Paper"), L.P.Paper);
		Paper->SetBrush(Brush(L.P.Paper, L.P.LineStrong, 1.f, 14.f));
		Bounds->AddChild(Paper);
		UVerticalBox* ScreenColumn = L.Make<UVerticalBox>(TEXT("Vertical_Screen"));
		Paper->AddChild(ScreenColumn);
		UBorder* Header = L.Border(TEXT("Border_Header"), L.P.Teal, FMargin(26, 18));
		UHorizontalBox* HeaderRow = L.Make<UHorizontalBox>(TEXT("Horizontal_Header"));
		Header->AddChild(HeaderRow);
		UVerticalBox* Heading = L.Make<UVerticalBox>(TEXT("Vertical_Heading"));
		UTextBlock* Title = L.Text(TEXT("Text_Title"), LOCTEXT("Title", "시설 조합 도감"), 30);
		Title->SetColorAndOpacity(L.P.White);
		L.V(Heading, Title);
		UTextBlock* Intro = L.Text(TEXT("Text_Intro"), LOCTEXT("Intro", "시설을 함께 배치하고, 새로운 시너지를 발견해 보세요."), 16);
		Intro->SetColorAndOpacity(L.P.White);
		L.V(Heading, Intro, FMargin(0, 5, 0, 0));
		L.H(HeaderRow, Heading, FMargin(0), true);
		L.H(HeaderRow, L.Button(TEXT("Button_Close"), LOCTEXT("Close", "닫기  ×")));
		L.V(ScreenColumn, Header);
		UBorder* Body = L.Border(TEXT("Border_Body"), FLinearColor::Transparent, FMargin(24, 18));
		L.V(ScreenColumn, Body, FMargin(0), true);
		UVerticalBox* BodyColumn = L.Make<UVerticalBox>(TEXT("Vertical_Body"));
		Body->AddChild(BodyColumn);
		UBorder* Summary = L.Border(TEXT("Border_Summary"), L.P.PaperSunken, FMargin(16, 12));
		Summary->AddChild(L.Text(TEXT("Text_Summary"), LOCTEXT("Summary", "발견 0 / 0    ·    효과 적용 중 0"), 21, true));
		L.V(BodyColumn, Summary, FMargin(0, 0, 0, 14));
		UHorizontalBox* Toolbar = L.Make<UHorizontalBox>(TEXT("Horizontal_Toolbar"));
		L.H(Toolbar, L.Button(TEXT("Button_All"), LOCTEXT("All", "전체")), FMargin(0, 0, 8, 0));
		L.H(Toolbar, L.Button(TEXT("Button_Discovered"), LOCTEXT("Discovered", "발견")), FMargin(0, 0, 8, 0));
		L.H(Toolbar, L.Button(TEXT("Button_Undiscovered"), LOCTEXT("Undiscovered", "미발견")), FMargin(0, 0, 18, 0));
		UEditableTextBox* Search = L.Make<UEditableTextBox>(TEXT("Search_Entries"), true);
		Search->SetHintText(LOCTEXT("SearchHint", "시설 또는 조합 이름 검색"));
		FEditableTextBoxStyle SearchStyle = Search->GetWidgetStyle();
		SearchStyle.SetBackgroundImageNormal(Brush(L.P.White, L.P.Line, 1.f, 7.f))
			.SetBackgroundImageHovered(Brush(L.P.Hover, L.P.Teal, 1.f, 7.f))
			.SetBackgroundImageFocused(Brush(L.P.White, L.P.TealDeep, 2.f, 7.f))
			.SetForegroundColor(L.P.Ink).SetFocusedForegroundColor(L.P.Ink).SetPadding(FMargin(16, 10));
		SearchStyle.TextStyle.Font.FontObject = LoadObject<UFont>(nullptr, *MakeObjectPath(GetUiFontPackage()));
		SearchStyle.TextStyle.Font.TypefaceFontName = TEXT("Regular");
		SearchStyle.TextStyle.Font.Size = 17;
		Search->SetWidgetStyle(SearchStyle);
		L.H(Toolbar, Search, FMargin(0), true);
		L.V(BodyColumn, Toolbar, FMargin(0, 0, 0, 10));
		UHorizontalBox* FilterRow = L.Make<UHorizontalBox>(TEXT("Horizontal_Filter"));
		L.H(FilterRow, L.Text(TEXT("Text_Filter"), LOCTEXT("Filter", "모든 시설 · 0개 조합"), 16, true, true), FMargin(0), true);
		UButton* ClearFacility = L.Button(TEXT("Button_ClearFacility"), LOCTEXT("ClearFacility", "모든 시설 보기"));
		ClearFacility->SetVisibility(ESlateVisibility::Collapsed);
		L.H(FilterRow, ClearFacility);
		L.V(BodyColumn, FilterRow, FMargin(0, 0, 0, 12));
		UHorizontalBox* MainRow = L.Make<UHorizontalBox>(TEXT("Horizontal_Main"));
		L.V(BodyColumn, MainRow, FMargin(0), true);
		UOverlay* GridOverlay = L.Make<UOverlay>(TEXT("Overlay_Grid"));
		L.H(MainRow, GridOverlay, FMargin(0, 0, 20, 0), true)->SetVerticalAlignment(VAlign_Fill);
		UFF_FacilityCodexTileView* Tiles = L.Make<UFF_FacilityCodexTileView>(TEXT("TileView_Entries"), true);
		Tiles->ConfigureEntryClass(CardClass);
		Tiles->SetScrollbarVisibility(ESlateVisibility::Visible);
		L.O(GridOverlay, Tiles);
		UTextBlock* Empty = L.Text(TEXT("Text_Empty"), LOCTEXT("Empty", "등록된 시설 조합이 없습니다."), 19, true, true);
		Empty->SetVisibility(ESlateVisibility::Collapsed);
		Empty->SetJustification(ETextJustify::Center);
		UOverlaySlot* EmptySlot = GridOverlay->AddChildToOverlay(Empty);
		EmptySlot->SetHorizontalAlignment(HAlign_Center);
		EmptySlot->SetVerticalAlignment(VAlign_Center);
		// Detail is an authored UMG composition. Designers can freely adjust all spacing and typography.
		USizeBox* DetailBounds = L.Size(TEXT("Size_Detail"), 500, 0);
		L.H(MainRow, DetailBounds)->SetVerticalAlignment(VAlign_Fill);
		UBorder* Detail = L.Border(TEXT("Panel_Detail"), L.P.White, FMargin(22, 16), true);
		Detail->SetBrush(Brush(L.P.White, L.P.Line, 1));
		DetailBounds->AddChild(Detail);
		UScrollBox* DetailScroll = L.Make<UScrollBox>(TEXT("Scroll_Detail"));
		DetailScroll->SetAllowOverscroll(false);
		DetailScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		Detail->AddChild(DetailScroll);
		UVerticalBox* DetailColumn = L.Make<UVerticalBox>(TEXT("Vertical_Detail"));
		DetailScroll->AddChild(DetailColumn);
		L.V(DetailColumn, L.Text(TEXT("Text_DetailKicker"), LOCTEXT("DetailKicker", "조합 레시피"), 14, false, true));
		L.V(DetailColumn, L.Text(TEXT("Text_DetailTitle"), LOCTEXT("DetailTitle", "함께 즐기는 달콤한 휴식"), 22, true), FMargin(0, 5, 0, 10));
		UHorizontalBox* Ingredients = L.Make<UHorizontalBox>(TEXT("Horizontal_Ingredients"));
		for (const FString Side : { FString(TEXT("Target")), FString(TEXT("Partner")) })
		{
			if (Side == TEXT("Partner")) L.H(Ingredients, L.Text(TEXT("Text_DetailPlus"), LOCTEXT("Plus", "+"), 32), FMargin(10, 0));
			UVerticalBox* Ingredient = L.Make<UVerticalBox>(FName(TEXT("Vertical_") + Side));
			L.V(Ingredient, L.Thumbnail(Side, 172, 80));
			UTextBlock* Name = L.Text(FName(TEXT("Text_") + Side + TEXT("Name")),
				Side == TEXT("Target") ? LOCTEXT("Target", "아이스크림") : LOCTEXT("Partner", "그늘 쉼터"), 17, true);
			Name->SetJustification(ETextJustify::Center);
			L.V(Ingredient, Name, FMargin(0, 8, 0, 0));
			L.H(Ingredients, Ingredient);
		}
		L.V(DetailColumn, Ingredients, FMargin(0, 0, 0, 10))->SetHorizontalAlignment(HAlign_Center);
		UBorder* Bonus = L.Border(TEXT("Border_Bonus"), L.P.PaperSunken, FMargin(12, 8));
		UHorizontalBox* ResultRow = L.Make<UHorizontalBox>(TEXT("Horizontal_Result"));
		Bonus->AddChild(ResultRow);
		L.H(ResultRow, L.Text(TEXT("Text_Equals"), LOCTEXT("Equals", "="), 34), FMargin(0, 0, 14, 0));
		UTextBlock* Result = L.Text(TEXT("Text_Bonus"), LOCTEXT("DetailBonus", "아이스크림\n수익 +10%"), 22, true);
		Result->SetColorAndOpacity(L.P.TealDeep);
		L.H(ResultRow, Result, FMargin(0), true);
		L.V(DetailColumn, Bonus, FMargin(0, 0, 0, 10));
		L.V(DetailColumn, L.Text(TEXT("Text_Distance"), LOCTEXT("Distance", "시설 중심 사이 10m 이내에 배치"), 17, true), FMargin(0, 0, 0, 9));
		L.V(DetailColumn, L.Text(TEXT("Text_Discovery"), LOCTEXT("Discovery", "미발견 · 조합에 필요한 두 시설을 설치해 보세요"), 15, true), FMargin(0, 0, 0, 9));
		L.V(DetailColumn, L.Text(TEXT("Text_Condition"), LOCTEXT("Condition", "현재 조건 미충족: 필요한 시설을 설치해 주세요"), 15, true, true), FMargin(0, 0, 0, 9));
		L.V(DetailColumn, L.Text(TEXT("Text_Applied"), LOCTEXT("Applied", "현재 적용 효과 없음 · 배치 조건을 충족하면 자동으로 적용됩니다."), 14, true, true));
		L.V(BodyColumn, L.Text(TEXT("Text_Footer"), LOCTEXT("Footer", "한 번 발견한 조합은 기록에 남아요. 현재 배치에 따라 실제 효과는 달라질 수 있어요."), 15, false, true), FMargin(0, 14, 0, 0));
		return true;
	}

	bool Launcher(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		FLayout L{ BP, Tree };
		USizeBox* Bounds = L.Size(TEXT("Size_Launcher"), 0, 42);
		Tree.RootWidget = Bounds;
		Bounds->AddChild(L.Button(TEXT("Button_OpenCodex"), LOCTEXT("OpenCodex", "시설 조합 도감  →"), true));
		return true;
	}

	bool CatalogLauncher(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		FLayout L{ BP, Tree };
		USizeBox* Bounds = L.Size(TEXT("Size_Launcher"), 76, 30);
		Tree.RootWidget = Bounds;
		UButton* Open = L.Button(TEXT("Button_OpenCodex"), LOCTEXT("OpenCatalogCodex", "도감"));
		Open->SetToolTipText(LOCTEXT("CatalogCodexTooltip", "모든 시설 조합과 발견 기록 보기"));
		FButtonStyle Style = Open->GetStyle();
		Style.Normal = Brush(L.P.PaperSunken, L.P.Line, 1.f, 6.f);
		Style.NormalPadding = Style.PressedPadding = FMargin(10, 3);
		Open->SetStyle(Style);
		if (UTextBlock* Label = Cast<UTextBlock>(Open->GetContent()))
		{
			FSlateFontInfo Font = Label->GetFont();
			Font.Size = 12;
			Label->SetFont(Font);
		}
		Bounds->AddChild(Open);
		return true;
	}

	void Build()
	{
		if (!EnsureWidgetBlueprint(Prefix, CardPath, Folder, *UFF_FacilityCodexCard::StaticClass(), Card)) return;
		UClass* CardClass = LoadGeneratedClass(CardPath);
		if (!CardClass || !CardClass->IsChildOf(UFF_FacilityCodexCard::StaticClass())) return;
		if (!EnsureWidgetBlueprint(Prefix, ScreenPath, Folder, *UFF_FacilityCodexScreen::StaticClass(),
			[CardClass](UWidgetBlueprint& BP, UWidgetTree& Tree) { return Screen(BP, Tree, CardClass); })) return;
		if (!EnsureWidgetBlueprint(Prefix, LauncherPath, Folder, *UFF_FacilityCodexLauncher::StaticClass(), Launcher)) return;
		if (!EnsureWidgetBlueprint(Prefix, CatalogLauncherPath, Folder, *UFF_FacilityCodexLauncher::StaticClass(), CatalogLauncher)) return;
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_READY: Screen, Card, Launcher, CatalogLauncher ready. Existing designer layouts were preserved."), Prefix);
	}

	FAutoConsoleCommand Command(TEXT("FF.BuildFacilityCodexUMG"),
		TEXT("Create missing facility codex WBP assets only. Existing designer layouts are preserved."),
		FConsoleCommandDelegate::CreateStatic(&Build));
}

#undef LOCTEXT_NAMESPACE
#endif
