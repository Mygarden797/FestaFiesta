// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FacilityRevenueWbpBuilder.cpp
// 역할: 시설 수입 순위와 행 위젯의 초기 편집용 에셋을 만든다.

// Editor-only scaffolding. Existing assets are always preserved.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#if WITH_EDITOR
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FacilityCodex/FF_FacilityCodexImage.h"
#include "AssetToolsModule.h"
#include "Factories/DataAssetFactory.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFRevenueBuilder"
namespace FFRevenueBuilder
{
using namespace FFUIWbpBuilder;
const TCHAR *Prefix = TEXT("FF_REVENUE_UMG");
const FString Folder(FFBuildUIAssetPaths::FacilityRevenueFolder);
const FString RowPath = Folder / TEXT("WBP_FF_FacilityRevenueRankRow");
const FString PanelPath = Folder / TEXT("WBP_FF_FacilityRevenueRanking");
FSlateBrush Brush(FLinearColor Color, FLinearColor Line = FLinearColor::Transparent, float Width = 0.f)
{
	FSlateBrush B;
	B.DrawAs = ESlateBrushDrawType::RoundedBox;
	B.TintColor = Color;
	B.OutlineSettings = FSlateBrushOutlineSettings(8.f, FSlateColor(Line), Width);
	return B;
}
struct FLayout
{
	UWidgetBlueprint &BP;
	UWidgetTree &Tree;
	const FFF_UIPaletteInGame &P = GetDefault<UFF_UIPalette>()->InGame;
	template <class T> T *Make(FName Name, bool Variable = false)
	{
		return MakeWidget<T>(BP, Tree, Name, Variable);
	}
	UTextBlock *Text(FName Name, FText Value, int Size = 18, bool Variable = false, bool Muted = false)
	{
		auto *W = Make<UTextBlock>(Name, Variable);
		StyleText(*W, Size, Muted ? P.InkMuted : P.Ink, TEXT("Medium"));
		W->SetText(Value);
		W->SetAutoWrapText(true);
		W->SetVisibility(ESlateVisibility::HitTestInvisible);
		return W;
	}
	UBorder *Border(FName Name, FLinearColor Color, FMargin Padding, bool Variable = false)
	{
		auto *W = Make<UBorder>(Name, Variable);
		W->SetBrush(Brush(Color));
		W->SetPadding(Padding);
		return W;
	}
	UButton *Button(FName Name, FText Value, bool Primary = false)
	{
		auto *W = Make<UButton>(Name, true);
		FButtonStyle S;
		S.Normal = Brush(Primary ? P.TealDeep : P.Paper, P.Line, 1);
		S.Hovered = Brush(Primary ? P.Teal : P.Hover, P.Teal, 1);
		S.Pressed = Brush(P.TealSoft, P.TealDeep, 2);
		S.Disabled = Brush(P.TealSoft, P.TealDeep, 2);
		S.NormalPadding = FMargin(12, 7);
		S.PressedPadding = S.NormalPadding;
		W->SetStyle(S);
		auto *Label = Text(FName(Name.ToString() + TEXT("_Label")), Value, 16);
		Label->SetJustification(ETextJustify::Center);
		if (Primary)
			Label->SetColorAndOpacity(P.White);
		W->AddChild(Label);
		return W;
	}
	USizeBox *Size(FName Name, float Width, float Height)
	{
		auto *W = Make<USizeBox>(Name);
		if (Width > 0)
			W->SetWidthOverride(Width);
		if (Height > 0)
			W->SetHeightOverride(Height);
		return W;
	}
	void V(UVerticalBox *Parent, UWidget *W, FMargin Padding = FMargin(0), bool Fill = false)
	{
		auto *S = Parent->AddChildToVerticalBox(W);
		S->SetPadding(Padding);
		S->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
	}
	void H(UHorizontalBox *Parent, UWidget *W, FMargin Padding = FMargin(0), bool Fill = false)
	{
		auto *S = Parent->AddChildToHorizontalBox(W);
		S->SetPadding(Padding);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
	}
};
bool Row(UWidgetBlueprint &BP, UWidgetTree &Tree)
{
	FLayout L{BP, Tree};
	auto *Bounds = L.Size(TEXT("Size_Row"), 0, 96);
	Tree.RootWidget = Bounds;
	auto *B = L.Border(TEXT("Border_Row"), L.P.White, FMargin(10, 8), true);
	Bounds->AddChild(B);
	auto *H = L.Make<UHorizontalBox>(TEXT("Row_Contents"));
	B->AddChild(H);
	auto *RankBounds = L.Size(TEXT("Size_Rank"), 32, 0);
	auto *Rank = L.Text(TEXT("Text_Rank"), LOCTEXT("Dash", "—"), 24, true);
	RankBounds->AddChild(Rank);
	L.H(H, RankBounds, FMargin(0, 0, 8, 0));
	auto *ThumbBounds = L.Size(TEXT("Size_Thumbnail"), 56, 56);
	L.H(H, ThumbBounds, FMargin(0, 0, 12, 0));
	auto *ThumbBack = L.Border(TEXT("Border_Thumbnail"), L.P.PaperSunken, FMargin(4));
	ThumbBounds->AddChild(ThumbBack);
	auto *Overlay = L.Make<UOverlay>(TEXT("Overlay_Thumbnail"));
	ThumbBack->AddChild(Overlay);
	auto *Fallback = L.Text(TEXT("Text_ThumbnailFallback"), LOCTEXT("Facility", "시설"), 14, true, true);
	auto *FallbackSlot = Overlay->AddChildToOverlay(Fallback);
	FallbackSlot->SetHorizontalAlignment(HAlign_Center);
	FallbackSlot->SetVerticalAlignment(VAlign_Center);
	auto *Scale = L.Make<UScaleBox>(TEXT("Scale_Thumbnail"));
	Scale->SetStretch(EStretch::ScaleToFit);
	auto *Image = L.Make<UFF_FacilityCodexImage>(TEXT("Image_Thumbnail"), true);
	Scale->AddChild(Image);
	Overlay->AddChildToOverlay(Scale)->SetHorizontalAlignment(HAlign_Fill);
	auto *Content = L.Make<UVerticalBox>(TEXT("Column_Facility"));
	L.H(H, Content, FMargin(0), true);
	L.V(Content, L.Text(TEXT("Text_Name"), LOCTEXT("FacilityName", "시설 이름"), 16, true));
	L.V(Content, L.Text(TEXT("Text_Location"), LOCTEXT("Location", "구역 · 위치"), 11, true, true),
		FMargin(0, 2, 0, 4));
	L.V(Content, L.Text(TEXT("Text_Amount"), LOCTEXT("Dash", "—"), 18, true));
	auto *BarBounds = L.Size(TEXT("Size_RevenueBar"), 0, 4);
	L.V(Content, BarBounds, FMargin(0, 6, 0, 0));
	auto *Bar = L.Make<UProgressBar>(TEXT("Bar_Revenue"), true);
	Bar->SetFillColorAndOpacity(L.P.Teal);
	FProgressBarStyle Style;
	Style.BackgroundImage = Brush(L.P.PaperSunken);
	Style.FillImage = Brush(L.P.White);
	Bar->SetWidgetStyle(Style);
	BarBounds->AddChild(Bar);
	return true;
}
bool Panel(UWidgetBlueprint &BP, UWidgetTree &Tree)
{
	FLayout L{BP, Tree};
	auto *Canvas = L.Make<UCanvasPanel>(TEXT("Canvas_Root"));
	Tree.RootWidget = Canvas;
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto *B = L.Border(TEXT("Border_Panel"), L.P.Paper, FMargin(16), true);
	B->SetBrush(Brush(L.P.Paper, L.P.Line, 1));
	auto *Slot = Canvas->AddChildToCanvas(B);
	Slot->SetAnchors(FAnchors(1, 0, 1, 1));
	Slot->SetAlignment(FVector2D(1, 0));
	Slot->SetOffsets(FMargin(-24, 18, 460, 80));
	auto *Contents = L.Make<UVerticalBox>(TEXT("Column_Panel"));
	B->AddChild(Contents);
	auto *Header = L.Make<UHorizontalBox>(TEXT("Row_Header"));
	L.V(Contents, Header, FMargin(0, 0, 0, 10));
	L.H(Header, L.Text(TEXT("Text_Title"), LOCTEXT("Title", "시설별 매출 순위"), 25), FMargin(0), true);
	L.H(Header, L.Button(TEXT("Button_Close"), LOCTEXT("Close", "닫기 ×")));
	auto *Tabs = L.Make<UHorizontalBox>(TEXT("Row_Period"));
	L.V(Contents, Tabs, FMargin(0, 0, 0, 12));
	L.H(Tabs, L.Button(TEXT("Button_Today"), LOCTEXT("Today", "오늘")), FMargin(0, 0, 6, 0), true);
	L.H(Tabs, L.Button(TEXT("Button_Total"), LOCTEXT("Cumulative", "축제 누적")), FMargin(0), true);
	auto *TotalCard = L.Border(TEXT("Border_Total"), L.P.Hover, FMargin(8));
	L.V(Contents, TotalCard, FMargin(0, 0, 0, 10));
	auto *TotalColumn = L.Make<UVerticalBox>(TEXT("Column_Total"));
	TotalCard->AddChild(TotalColumn);
	L.V(TotalColumn, L.Text(TEXT("Text_TotalLabel"), LOCTEXT("TotalLabel", "선택 기간 매출 합계"), 13));
	L.V(TotalColumn, L.Text(TEXT("Text_Total"), LOCTEXT("Dash", "—"), 26, true), FMargin(0, 4, 0, 4));
	L.V(TotalColumn, L.Text(TEXT("Text_Count"), LOCTEXT("Scope", "현재 설치된 수익 시설 기준"), 12, true, true));
	L.V(Contents,
		L.Text(TEXT("Text_Order"), LOCTEXT("Order", "매출 높은 순  ·  같은 매출은 공동 순위"), 13, false, true),
		FMargin(0, 0, 0, 8));
	auto *List = L.Make<UFF_FacilityRevenueListView>(TEXT("List_Ranking"), true);
	UClass *RowClass = LoadGeneratedClass(RowPath);
	if (!RowClass)
		return false;
	List->ConfigureEntryClass(RowClass);
	L.V(Contents, List, FMargin(0, 0, 0, 10), true);
	L.V(Contents, L.Text(TEXT("Text_Empty"), LOCTEXT("Empty", "매출 정보를 준비 중입니다."), 17, true), FMargin(0, 8));
	auto *Selection = L.Border(TEXT("Border_Selected"), L.P.PaperSunken, FMargin(8));
	L.V(Contents, Selection, FMargin(0, 0, 0, 10));
	auto *SelectedMetrics = L.Make<UHorizontalBox>(TEXT("Row_SelectedMetrics"));
	Selection->AddChild(SelectedMetrics);
	auto* SelectedName=L.Text(TEXT("Text_SelectedName"),LOCTEXT("Choose","시설을 선택하세요"),16,true);
	SelectedName->SetAutoWrapText(false);
	SelectedName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	L.H(SelectedMetrics,SelectedName,FMargin(0,0,8,0),true);
	L.H(SelectedMetrics, L.Text(TEXT("Text_SelectedRank"), LOCTEXT("Dash", "—"), 18, true), FMargin(0,0,12,0));
	L.H(SelectedMetrics, L.Text(TEXT("Text_ShareLabel"), LOCTEXT("Share", "매출 비중"), 14, false, true),
		FMargin(0, 0, 8, 0));
	L.H(SelectedMetrics, L.Text(TEXT("Text_SelectedShare"), LOCTEXT("Dash", "—"), 18, true));
	L.V(Contents, L.Button(TEXT("Button_Inspect"), LOCTEXT("Inspect", "선택 시설 정보 보기"), true));
	L.V(Contents,
		L.Text(TEXT("Text_Footnote"), LOCTEXT("Footnote", "시설 장부 기준 · 철거된 시설은 목록에서 제외됩니다."), 12,
			   false, true),
		FMargin(0, 10, 0, 0));
	return true;
}
bool EnsureCookLabel()
{
	const FString Path = Folder / TEXT("PAL_FF_FacilityRevenueUI");
	if (LoadObject<UPrimaryAssetLabel>(nullptr, *MakeObjectPath(Path)))
		return true;
	auto *Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = UPrimaryAssetLabel::StaticClass();
	auto &Assets = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	auto *Label = Cast<UPrimaryAssetLabel>(
		Assets.Get().CreateAsset(TEXT("PAL_FF_FacilityRevenueUI"), Folder, UPrimaryAssetLabel::StaticClass(), Factory));
	if (!Label)
		return false;
	Label->bIsRuntimeLabel = true;
	Label->bLabelAssetsInMyDirectory = true;
	Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
	Label->Rules.bApplyRecursively = true;
	Label->UpdateAssetBundleData();
	return SaveAssetPackage(Prefix, *Label);
}
void Build()
{
	auto *RowBP = EnsureWidgetBlueprint(Prefix, RowPath, Folder, *UFF_FacilityRevenueRankRow::StaticClass(), Row);
	if (!RowBP)
		return;
	const bool bNewPanel = LoadExistingBlueprint(PanelPath) == nullptr;
	auto *PanelBP =
		EnsureWidgetBlueprint(Prefix, PanelPath, Folder, *UFF_FacilityRevenueRankingPanel::StaticClass(), Panel);
	if (!PanelBP)
		return;
	if (bNewPanel && PanelBP->GeneratedClass)
	{
		auto *Defaults = PanelBP->GeneratedClass->GetDefaultObject<UUserWidget>();
		Defaults->SetIsFocusable(true);
		SaveAssetPackage(Prefix, *PanelBP);
	}
	if (EnsureCookLabel())
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_REVENUE_UMG_DONE: saved WBP layout is now the Designer source."));
}
void FinalizeCompactLayout()
{
	const FName Version(TEXT("FF.Revenue.FinalCompact20260919"));
	for (const FString& Path : {RowPath, PanelPath})
	{
		auto* BP=LoadExistingBlueprint(Path);
		if(!BP || !BP->WidgetTree)return;
		auto& Meta=BP->GetOutermost()->GetMetaData();
		if(Meta.GetValue(BP,Version)==TEXT("1"))continue;
		if(Path==RowPath)
		{
			auto* Bounds=Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("Size_Row")));
			if(!Bounds)return;
			Bounds->SetHeightOverride(96);
		}
		else
		{
			// The first compact migration detached this empty container. Remove its editor GUID too.
			if(!BP->WidgetTree->FindWidget(TEXT("Column_Selected")))BP->OnVariableRemoved(TEXT("Column_Selected"));
			auto* Body=Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Border_Panel")));
			auto* Slot=Body?Cast<UCanvasPanelSlot>(Body->Slot):nullptr;
			if(!Body || !Slot)return;
			Body->SetPadding(FMargin(16));
			Slot->SetOffsets(FMargin(-24,18,460,80));
		}
		Meta.SetValue(BP,Version,TEXT("1"));
		if(!SaveNewBlueprint(Prefix,*BP))return;
	}
	UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_REVENUE_FINAL_COMPACT_DONE"));
}
FAutoConsoleCommand FinalizeCommand(TEXT("FF.FinalizeFacilityRevenueLayout20260919"),
	TEXT("Finalize the approved compact revenue layout once, preserving subsequent Designer edits."),
	FConsoleCommandDelegate::CreateStatic(&FinalizeCompactLayout));
FAutoConsoleCommand Command(
	TEXT("FF.BuildFacilityRevenueUMG"),
	TEXT("Create missing revenue rank row, screen and cook label. Existing Designer edits are preserved."),
	FConsoleCommandDelegate::CreateStatic(&Build));
} // namespace FFRevenueBuilder
#undef LOCTEXT_NAMESPACE
#endif

