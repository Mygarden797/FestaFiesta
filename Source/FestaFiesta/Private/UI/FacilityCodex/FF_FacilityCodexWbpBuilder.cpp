// FF_FacilityCodexWbpBuilder.cpp
// 역할: 도감 화면·카드·진입 버튼의 초기 편집용 에셋을 만든다.

// Editor-only UMG scaffolding. Normal builds preserve existing assets; layout migrations are explicit.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

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
#include "Components/StackBox.h"
#include "Components/StackBoxSlot.h"
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
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodexBuilder"

namespace FFFacilityCodexWbpBuilder
{
	using namespace FFUIWbpBuilder;
	const TCHAR* const Prefix = TEXT("FF_FACILITY_CODEX_UMG");
	const FString Folder(FFBuildUIAssetPaths::FacilityCodexFolder);
	const FString CardPath = Folder / TEXT("WBP_FF_FacilityCodexCard");
	const FString ScreenPath = Folder / TEXT("WBP_FF_FacilityCodexScreen");
	const FString LauncherPath = Folder / TEXT("WBP_FF_FacilityCodexLauncher");
	const FString CatalogLauncherPath = Folder / TEXT("WBP_FF_FacilityCodexCatalogLauncher");
	const TCHAR* const LayoutVersionKey = TEXT("FF.FacilityCodex.Layout");
	const TCHAR* const LayoutVersion = TEXT("SixColumns20260919");
	const TCHAR* const CardFitKey = TEXT("FF.FacilityCodex.CardFit20260919");
	const TCHAR* const ScreenFitKey = TEXT("FF.FacilityCodex.ScreenFit72020260919");

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
			Style.NormalPadding = FMargin(18.f, 8.f);
			Style.PressedPadding = FMargin(18.f, 9.f, 18.f, 7.f);
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
		// Width comes from the six-column TileView. Every visual remains editable in the WBP.
		USizeBox* Bounds = L.Size(TEXT("Size_Card"), 0, 208);
		Tree.RootWidget = Bounds;
		UOverlay* Overlay = L.Make<UOverlay>(TEXT("Overlay_Card"));
		Bounds->AddChild(Overlay);
		UBorder* CardBody = L.Border(TEXT("Border_Card"), L.P.White, FMargin(10));
		CardBody->SetBrush(Brush(L.P.White, L.P.Line, 1));
		L.O(Overlay, CardBody);
		UVerticalBox* Content = L.Make<UVerticalBox>(TEXT("Vertical_Card"));
		CardBody->AddChild(Content);
		USizeBox* NameBounds = L.Size(TEXT("Size_Name"), 0, 52);
		NameBounds->AddChild(L.Text(TEXT("Text_Name"), LOCTEXT("CardName", "함께 즐기는 달콤한 휴식"), 16, true));
		NameBounds->SetClipping(EWidgetClipping::ClipToBounds);
		L.V(Content, NameBounds);
		UHorizontalBox* RecipeImages = L.Make<UHorizontalBox>(TEXT("Horizontal_RecipeImages"));
		L.H(RecipeImages, L.Thumbnail(TEXT("Target"), 0, 56), FMargin(0), true);
		L.H(RecipeImages, L.Text(TEXT("Text_Plus"), LOCTEXT("Plus", "+"), 20), FMargin(6, 0));
		L.H(RecipeImages, L.Thumbnail(TEXT("Partner"), 0, 56), FMargin(0), true);
		L.V(Content, RecipeImages, FMargin(0, 0, 0, 10));
		USizeBox* RecipeBounds = L.Size(TEXT("Size_Recipe"), 0, 34);
		RecipeBounds->AddChild(L.Text(TEXT("Text_Recipe"), LOCTEXT("Recipe", "아이스크림 + 그늘 쉼터"), 13, true, true));
		RecipeBounds->SetClipping(EWidgetClipping::ClipToBounds);
		// The recipe names already appear in the title. Keep the binding for designer variants.
		RecipeBounds->SetVisibility(ESlateVisibility::Collapsed);
		L.V(Content, RecipeBounds);
		UTextBlock* Bonus = L.Text(TEXT("Text_Bonus"), LOCTEXT("CardBonus", "수익 +10%"), 19, true);
		Bonus->SetColorAndOpacity(L.P.TealDeep);
		L.V(Content, Bonus);
		UBorder* State = L.Border(TEXT("Border_State"), L.P.TealSoft, FMargin(8, 3));
		UTextBlock* StateText = L.Text(TEXT("Text_State"), LOCTEXT("CardState", "미발견"), 13, true, true);
		StateText->SetAutoWrapText(false);
		State->AddChild(StateText);
		L.V(Content, State, FMargin(0, 7, 0, 0))->SetHorizontalAlignment(HAlign_Left);
		UBorder* Selection = L.Border(TEXT("Border_Selection"), FLinearColor::Transparent, FMargin(0), true);
		Selection->SetBrush(Brush(FLinearColor::Transparent, L.P.TealDeep, 3));
		Selection->SetVisibility(ESlateVisibility::Collapsed);
		L.O(Overlay, Selection);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, LayoutVersionKey, LayoutVersion);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, CardFitKey, TEXT("1"));
		return true;
	}

	bool FitScreenTop(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		// Keep the screen frame, card metrics and detail width. Reclaim vertical space from
		// the title/controls so a complete card row remains visible beside the compact detail.
		for (const FName Name : { TEXT("Border_Header"), TEXT("Border_Body"), TEXT("Border_Summary"),
			TEXT("Text_Title"), TEXT("Text_Intro"), TEXT("Text_Summary"), TEXT("Text_Filter"), TEXT("Text_Footer"),
			TEXT("Horizontal_Toolbar"), TEXT("Horizontal_Filter"), TEXT("Search_Entries"),
			TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"), TEXT("Button_Close"), TEXT("Button_ClearFacility") })
		{
			if (!Tree.FindWidget(Name)) return false;
		}
		auto Font = [&](FName Name, int32 Before, int32 After)
		{
			if (UTextBlock* Text = Cast<UTextBlock>(Tree.FindWidget(Name)))
			{
				FSlateFontInfo Value = Text->GetFont();
				if (Value.Size == Before) { Value.Size = After; Text->SetFont(Value); }
			}
		};
		auto Padding = [&](FName Name, FMargin Before, FMargin After)
		{
			if (UBorder* Border = Cast<UBorder>(Tree.FindWidget(Name)); Border && Border->GetPadding() == Before) Border->SetPadding(After);
		};
		auto SlotPadding = [&](FName Name, FMargin Before, FMargin After)
		{
			if (UWidget* Widget = Tree.FindWidget(Name))
				if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Widget->Slot); Slot && Slot->GetPadding() == Before) Slot->SetPadding(After);
		};
		Font(TEXT("Text_Title"), 30, 24);
		Font(TEXT("Text_Intro"), 16, 13);
		Font(TEXT("Text_Summary"), 18, 16);
		Font(TEXT("Text_Filter"), 16, 14);
		Font(TEXT("Text_Footer"), 14, 13);
		Padding(TEXT("Border_Header"), FMargin(24, 14), FMargin(24, 10));
		Padding(TEXT("Border_Body"), FMargin(16), FMargin(16, 12));
		Padding(TEXT("Border_Summary"), FMargin(14, 8), FMargin(14, 5));
		SlotPadding(TEXT("Text_Intro"), FMargin(0, 5, 0, 0), FMargin(0, 3, 0, 0));
		SlotPadding(TEXT("Border_Summary"), FMargin(0, 0, 0, 10), FMargin(0, 0, 0, 6));
		SlotPadding(TEXT("Horizontal_Toolbar"), FMargin(0, 0, 0, 8), FMargin(0, 0, 0, 6));
		SlotPadding(TEXT("Horizontal_Filter"), FMargin(0, 0, 0, 8), FMargin(0, 0, 0, 4));
		SlotPadding(TEXT("Text_Footer"), FMargin(0, 8, 0, 0), FMargin(0, 6, 0, 0));
		for (const FName Name : { TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"), TEXT("Button_Close"), TEXT("Button_ClearFacility") })
		{
			if (UButton* Button = Cast<UButton>(Tree.FindWidget(Name)))
			{
				const bool bClear = Name == TEXT("Button_ClearFacility");
				Font(FName(Name.ToString() + TEXT("_Label")), 17, bClear ? 13 : 15);
				FButtonStyle Style = Button->GetStyle();
				if (Style.NormalPadding == FMargin(18, 8)) Style.NormalPadding = bClear ? FMargin(12, 4) : FMargin(18, 6);
				if (Style.PressedPadding == FMargin(18, 9, 18, 7)) Style.PressedPadding = bClear ? FMargin(12, 5, 12, 3) : FMargin(18, 7, 18, 5);
				Button->SetStyle(Style);
			}
		}
		if (UEditableTextBox* Search = Cast<UEditableTextBox>(Tree.FindWidget(TEXT("Search_Entries"))))
		{
			FEditableTextBoxStyle Style = Search->GetWidgetStyle();
			if (Style.TextStyle.Font.Size == 17) Style.TextStyle.Font.Size = 15;
			if (Style.Padding == FMargin(16, 8)) Style.SetPadding(FMargin(16, 6));
			Search->SetWidgetStyle(Style);
		}
		BP.GetOutermost()->GetMetaData().SetValue(&BP, ScreenFitKey, TEXT("1"));
		return true;
	}

	bool Screen(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass* CardClass)
	{
		FLayout L{ BP, Tree };
		// This transparent fullscreen hit target blocks clicks outside the panel without implying pause.
		UBorder* Root = L.Border(TEXT("Border_InputShield"), FLinearColor::Transparent, FMargin(24, 24, 24, 64));
		Tree.RootWidget = Root;
		UBorder* Paper = L.Border(TEXT("Border_Paper"), L.P.Paper);
		Paper->SetBrush(Brush(L.P.Paper, L.P.LineStrong, 1.f, 14.f));
		Root->AddChild(Paper);
		UVerticalBox* ScreenColumn = L.Make<UVerticalBox>(TEXT("Vertical_Screen"));
		Paper->AddChild(ScreenColumn);
		UBorder* Header = L.Border(TEXT("Border_Header"), L.P.Teal, FMargin(24, 14));
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
		UBorder* Body = L.Border(TEXT("Border_Body"), FLinearColor::Transparent, FMargin(16));
		L.V(ScreenColumn, Body, FMargin(0), true);
		UVerticalBox* BodyColumn = L.Make<UVerticalBox>(TEXT("Vertical_Body"));
		Body->AddChild(BodyColumn);
		UBorder* Summary = L.Border(TEXT("Border_Summary"), L.P.PaperSunken, FMargin(14, 8));
		Summary->AddChild(L.Text(TEXT("Text_Summary"), LOCTEXT("Summary", "발견 0 / 0    ·    효과 적용 중 0"), 18, true));
		L.V(BodyColumn, Summary, FMargin(0, 0, 0, 10));
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
			.SetForegroundColor(L.P.Ink).SetFocusedForegroundColor(L.P.Ink).SetPadding(FMargin(16, 8));
		SearchStyle.TextStyle.Font.FontObject = LoadObject<UFont>(nullptr, *MakeObjectPath(GetUiFontPackage()));
		SearchStyle.TextStyle.Font.TypefaceFontName = TEXT("Regular");
		SearchStyle.TextStyle.Font.Size = 17;
		Search->SetWidgetStyle(SearchStyle);
		L.H(Toolbar, Search, FMargin(0), true);
		L.V(BodyColumn, Toolbar, FMargin(0, 0, 0, 8));
		UHorizontalBox* FilterRow = L.Make<UHorizontalBox>(TEXT("Horizontal_Filter"));
		L.H(FilterRow, L.Text(TEXT("Text_Filter"), LOCTEXT("Filter", "모든 시설 · 0개 조합"), 16, true, true), FMargin(0), true);
		UButton* ClearFacility = L.Button(TEXT("Button_ClearFacility"), LOCTEXT("ClearFacility", "모든 시설 보기"));
		ClearFacility->SetVisibility(ESlateVisibility::Collapsed);
		L.H(FilterRow, ClearFacility);
		L.V(BodyColumn, FilterRow, FMargin(0, 0, 0, 8));
		UStackBox* MainRow = L.Make<UStackBox>(TEXT("Stack_Main"), true);
		MainRow->SetOrientation(Orient_Horizontal);
		L.V(BodyColumn, MainRow, FMargin(0), true);
		UOverlay* GridOverlay = L.Make<UOverlay>(TEXT("Overlay_Grid"));
		UStackBoxSlot* GridSlot = MainRow->AddChildToStackBox(GridOverlay);
		GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GridSlot->SetPadding(FMargin(0, 0, 20, 0));
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
		USizeBox* DetailBounds = L.Size(TEXT("Size_Detail"), 460, 0);
		DetailBounds->bIsVariable = true;
		MainRow->AddChildToStackBox(DetailBounds);
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
		L.V(BodyColumn, L.Text(TEXT("Text_Footer"), LOCTEXT("Footer", "한 번 발견한 조합은 기록에 남아요. 현재 배치에 따라 실제 효과는 달라질 수 있어요."), 14, false, true), FMargin(0, 8, 0, 0));
		BP.GetOutermost()->GetMetaData().SetValue(&BP, LayoutVersionKey, LayoutVersion);
		return FitScreenTop(BP, Tree);
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

	bool HasSixColumnLayout(UWidgetBlueprint* BP)
	{
		return BP && BP->GetOutermost()->GetMetaData().GetValue(BP, LayoutVersionKey) == LayoutVersion;
	}

	void ApplySixColumnsOnce()
	{
		// This is the only replacement path, deliberately separate from normal authoring.
		// The saved per-asset version makes later invocations preserve designer edits as well.
		for (const TPair<FString, UClass*>& Asset : {
			TPair<FString, UClass*>(CardPath, UFF_FacilityCodexCard::StaticClass()),
			TPair<FString, UClass*>(ScreenPath, UFF_FacilityCodexScreen::StaticClass()) })
		{
			if (UWidgetBlueprint* BP = LoadExistingBlueprint(Asset.Key))
			{
				if (!BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(Asset.Value))
				{
					UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_MIGRATION_ABORT: unexpected parent at %s"), Prefix, *Asset.Key);
					return;
				}
			}
		}
		if (!HasSixColumnLayout(LoadExistingBlueprint(CardPath)) &&
			!ReplaceWidgetBlueprint(Prefix, CardPath, Folder, *UFF_FacilityCodexCard::StaticClass(), Card)) return;
		UClass* CardClass = LoadGeneratedClass(CardPath);
		if (!CardClass || !CardClass->IsChildOf(UFF_FacilityCodexCard::StaticClass())) return;
		if (!HasSixColumnLayout(LoadExistingBlueprint(ScreenPath)) &&
			!ReplaceWidgetBlueprint(Prefix, ScreenPath, Folder, *UFF_FacilityCodexScreen::StaticClass(),
				[CardClass](UWidgetBlueprint& BP, UWidgetTree& Tree) { return Screen(BP, Tree, CardClass); })) return;
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_SIX_COLUMNS_READY: Card and Screen migrated once. Versioned layouts and all launchers preserved."), Prefix);
	}

	void ApplyCardFitOnce()
	{
		UWidgetBlueprint* BP = LoadExistingBlueprint(CardPath);
		if (!BP || !BP->WidgetTree || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(UFF_FacilityCodexCard::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, CardFitKey) == TEXT("1")) return;
		auto* State = Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("Text_State")));
		auto* Name = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_Name")));
		auto* Target = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_ThumbTarget")));
		auto* Partner = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_ThumbPartner")));
		auto* Body = Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Border_Card")));
		if (!State || !Name || !Target || !Partner || !Body) return;
		// Patch only the known seed values; retain unrelated designer overrides and the outer card.
		State->SetAutoWrapText(false);
		if (Name->IsHeightOverride() && FMath::IsNearlyEqual(Name->GetHeightOverride(), 42.f)) Name->SetHeightOverride(52.f);
		for (USizeBox* Thumbnail : { Target, Partner })
			if (Thumbnail->IsHeightOverride() && FMath::IsNearlyEqual(Thumbnail->GetHeightOverride(), 64.f)) Thumbnail->SetHeightOverride(56.f);
		if (Body->GetPadding() == FMargin(12)) Body->SetPadding(FMargin(10));
		BP->GetOutermost()->GetMetaData().SetValue(BP, CardFitKey, TEXT("1"));
		SaveNewBlueprint(Prefix, *BP);
	}

	void ApplyScreenFitOnce()
	{
		UWidgetBlueprint* BP = LoadExistingBlueprint(ScreenPath);
		if (!BP || !BP->WidgetTree || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(UFF_FacilityCodexScreen::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, ScreenFitKey) == TEXT("1")) return;
		if (FitScreenTop(*BP, *BP->WidgetTree)) SaveNewBlueprint(Prefix, *BP);
	}

	FAutoConsoleCommand Command(TEXT("FF.BuildFacilityCodexUMG"),
		TEXT("Create missing facility codex WBP assets only. Existing designer layouts are preserved."),
		FConsoleCommandDelegate::CreateStatic(&Build));
	FAutoConsoleCommand SixColumnMigration(TEXT("FF.ApplyFacilityCodexSixColumns20260919"),
		TEXT("Explicit one-time replacement of the codex Card and Screen layouts only. Already migrated assets preserve designer edits."),
		FConsoleCommandDelegate::CreateStatic(&ApplySixColumnsOnce));
	FAutoConsoleCommand CardFitMigration(TEXT("FF.ApplyFacilityCodexCardFit20260919"),
		TEXT("One-time internal card fit repair; preserve card/screen outer dimensions and unrelated designer values."),
		FConsoleCommandDelegate::CreateStatic(&ApplyCardFitOnce));
	FAutoConsoleCommand ScreenFitMigration(TEXT("FF.ApplyFacilityCodexScreenFit72020260919"),
		TEXT("One-time title/control spacing repair for a complete 720p card row. Preserve screen frame, detail width and custom values."),
		FConsoleCommandDelegate::CreateStatic(&ApplyScreenFitOnce));
}

#undef LOCTEXT_NAMESPACE
#endif
