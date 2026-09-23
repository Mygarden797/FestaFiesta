// FF_FacilityAuroraRevenueProgramsMigration.cpp
// 역할: 시설 수입과 유료 프로그램 화면에 Aurora 배치를 적용한다.

// Explicit V7 layout migration. Runtime continues to bind the original Designer widgets.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/StackBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "UI/Facility/FF_FacilityProgramMarkerWidget.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FFFacilityAuroraRevenuePrograms"

namespace FFFacilityAuroraRevenuePrograms
{
using namespace FFUIWbpBuilder;
const TCHAR* Prefix = TEXT("FF_AURORA_REVENUE_PROGRAMS");
const FName Version(TEXT("FF.Facility.AuroraRevenuePrograms.V7.20260920"));

struct FEdit
{
	UWidgetBlueprint& BP;
	UWidgetTree& Tree;
	const FFF_UIPaletteInGame& P = GetDefault<UFF_UIPalette>()->InGame;
	template<class T> T* Find(const TCHAR* Name) const { return Cast<T>(Tree.FindWidget(FName(Name))); }
	template<class T> T* Make(const TCHAR* Name, bool Variable = false)
	{ return MakeWidget<T>(BP, Tree, FName(Name), Variable); }
	bool Has(std::initializer_list<const TCHAR*> Names) const
	{
		for (const TCHAR* Name : Names)
			if (!Tree.FindWidget(FName(Name)))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: %s is missing %s; no layout change."), Prefix, *BP.GetName(), Name);
				return false;
			}
		return true;
	}
	void Font(const TCHAR* Name, int32 Size, bool Strong = false)
	{
		if (auto* Text = Find<UTextBlock>(Name))
		{
			FSlateFontInfo Info = Text->GetFont();
			Info.Size = Size;
			Info.TypefaceFontName = Strong ? TEXT("SemiBold") : TEXT("Medium");
			Text->SetFont(Info);
		}
	}
	void Padding(const TCHAR* Name, FMargin Value)
	{ if (auto* Border = Find<UBorder>(Name)) Border->SetPadding(Value); }
	void VPadding(const TCHAR* Name, FMargin Value)
	{
		if (auto* Widget = Tree.FindWidget(FName(Name)))
			if (auto* Slot = Cast<UVerticalBoxSlot>(Widget->Slot)) Slot->SetPadding(Value);
	}
	void Collapse(const TCHAR* Name)
	{ if (auto* Widget = Tree.FindWidget(FName(Name))) Widget->SetVisibility(ESlateVisibility::Collapsed); }
	UTextBlock* Text(const TCHAR* Name, FText Value, int32 Size, bool Variable = false)
	{
		auto* Result = Make<UTextBlock>(Name, Variable);
		StyleText(*Result, Size, P.Ink, TEXT("Medium"));
		Result->SetText(Value);
		Result->SetAutoWrapText(true);
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}
	UBorder* Surface(const TCHAR* Name, FMargin Padding, bool Variable = false)
	{
		auto* Border = Make<UBorder>(Name, Variable);
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = P.Hover;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(8.f, FSlateColor(P.Line), 1.f);
		Border->SetBrush(Brush);
		Border->SetPadding(Padding);
		return Border;
	}
	void Dock(const TCHAR* Name, float Width)
	{
		if (auto* Widget = Tree.FindWidget(FName(Name)))
			if (auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				Slot->SetAnchors(FAnchors(1, 0, 1, 1));
				Slot->SetAlignment(FVector2D(1, 0));
				Slot->SetAutoSize(false);
				Slot->SetOffsets(FMargin(-18, 18, Width, 70));
			}
	}
};

bool RevenueRow(FEdit& E)
{
	if (!E.Has({TEXT("Size_Row"), TEXT("Size_Thumbnail"), TEXT("Column_Facility"), TEXT("Row_Contents"),
		TEXT("Text_Amount"), TEXT("Size_RevenueBar")})) return false;
	E.Find<USizeBox>(TEXT("Size_Row"))->SetHeightOverride(142.f);
	E.Find<USizeBox>(TEXT("Size_Thumbnail"))->SetWidthOverride(80.f);
	E.Find<USizeBox>(TEXT("Size_Thumbnail"))->SetHeightOverride(80.f);
	E.Padding(TEXT("Border_Row"), FMargin(12, 14));
	E.Font(TEXT("Text_Name"), 21, true);
	E.Font(TEXT("Text_Location"), 14);
	E.Font(TEXT("Text_Rank"), 27, true);
	E.Font(TEXT("Text_Amount"), 27, true);
	auto* Amount = E.Find<UTextBlock>(TEXT("Text_Amount"));
	Amount->SetAutoWrapText(false);
	Amount->SetJustification(ETextJustify::Right);
	Amount->RemoveFromParent();
	auto* Fit = E.Make<UScaleBox>(TEXT("Scale_AuroraRevenueAmount"));
	Fit->SetStretch(EStretch::ScaleToFit);
	Fit->SetStretchDirection(EStretchDirection::DownOnly);
	Fit->AddChild(Amount);
	auto* Values = E.Make<UVerticalBox>(TEXT("Column_AuroraRevenueAmount"));
	Values->AddChildToVerticalBox(Fit)->SetHorizontalAlignment(HAlign_Right);
	auto* Share = E.Text(TEXT("Text_Share"), LOCTEXT("RowShare", "비중 —"), 15, true);
	Share->SetJustification(ETextJustify::Right);
	Share->SetColorAndOpacity(E.P.InkMuted);
	Values->AddChildToVerticalBox(Share)->SetPadding(FMargin(0, 8, 0, 0));
	auto* Width = E.Make<USizeBox>(TEXT("Size_AuroraRevenueAmount"));
	Width->SetWidthOverride(148.f);
	Width->AddChild(Values);
	auto* Right = E.Find<UHorizontalBox>(TEXT("Row_Contents"))->AddChildToHorizontalBox(Width);
	Right->SetVerticalAlignment(VAlign_Center);
	Right->SetPadding(FMargin(10, 0, 0, 0));
	return true;
}

bool RevenuePanel(FEdit& E)
{
	if (!E.Has({TEXT("Border_Panel"), TEXT("Column_Panel"), TEXT("List_Ranking"), TEXT("Text_Empty"),
		TEXT("Border_Total"), TEXT("Border_Selected"), TEXT("Row_SelectedMetrics"), TEXT("Text_SelectedName")})) return false;
	E.Dock(TEXT("Border_Panel"), 600.f);
	E.Padding(TEXT("Border_Panel"), FMargin(18));
	E.Padding(TEXT("Border_Total"), FMargin(18, 14));
	E.Padding(TEXT("Border_Selected"), FMargin(15, 12));
	E.Font(TEXT("Text_Title"), 28, true);
	E.Font(TEXT("Text_Total"), 54, true);
	E.Font(TEXT("Text_TotalLabel"), 17);
	E.Font(TEXT("Text_Count"), 15);
	E.Font(TEXT("Text_Order"), 15);
	E.Font(TEXT("Text_SelectedName"), 23, true);
	E.Font(TEXT("Text_SelectedRank"), 21, true);
	E.Font(TEXT("Text_SelectedShare"), 20, true);
	E.Font(TEXT("Text_Footnote"), 14);
	E.Font(TEXT("Button_Inspect_Label"), 21, true);
	E.Font(TEXT("Button_Today_Label"), 18, true);
	E.Font(TEXT("Button_Total_Label"), 18, true);
	auto* Column = E.Find<UVerticalBox>(TEXT("Column_Panel"));
	auto* List = E.Find<UFF_FacilityRevenueListView>(TEXT("List_Ranking"));
	auto* Empty = E.Find<UTextBlock>(TEXT("Text_Empty"));
	const int32 Index = Column->GetChildIndex(List);
	if (Index == INDEX_NONE) return false;
	List->RemoveFromParent();
	Empty->RemoveFromParent();
	auto* Area = E.Make<UOverlay>(TEXT("Overlay_AuroraRevenueList"));
	auto* ListSlot = Area->AddChildToOverlay(List);
	ListSlot->SetHorizontalAlignment(HAlign_Fill);
	ListSlot->SetVerticalAlignment(VAlign_Fill);
	Empty->SetJustification(ETextJustify::Center);
	auto* EmptySlot = Area->AddChildToOverlay(Empty);
	EmptySlot->SetHorizontalAlignment(HAlign_Center);
	EmptySlot->SetVerticalAlignment(VAlign_Center);
	auto* AreaSlot = Cast<UVerticalBoxSlot>(Column->InsertChildAt(Index, Area));
	AreaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AreaSlot->SetPadding(FMargin(0, 0, 0, 12));
	// This state is shown only for installed facilities with a valid zero aggregate.
	auto* FirstRevenue = E.Surface(TEXT("Border_FirstRevenue"), FMargin(12), true);
	auto* Message = E.Make<UVerticalBox>(TEXT("Column_AuroraFirstRevenue"));
	FirstRevenue->AddChild(Message);
	Message->AddChildToVerticalBox(E.Text(TEXT("Text_FirstRevenueTitle"), LOCTEXT("FirstSale", "첫 매출을 기다리는 중"), 19));
	Message->AddChildToVerticalBox(E.Text(TEXT("Text_FirstRevenueDescription"),
		LOCTEXT("FirstSaleHelp", "손님이 시설을 이용하면 매출과 비중이 표시됩니다.\n합계가 0 Gold일 때 매출 비중은 계산하지 않습니다."), 15))
		->SetPadding(FMargin(0, 6, 0, 0));
	FirstRevenue->SetVisibility(ESlateVisibility::Collapsed);
	Cast<UVerticalBoxSlot>(Column->InsertChildAt(Index + 1, FirstRevenue))->SetPadding(FMargin(0, 0, 0, 12));
	// Give long facility names their own line; the rank and share never squeeze the name.
	auto* Selected = E.Find<UBorder>(TEXT("Border_Selected"));
	auto* Metrics = E.Find<UHorizontalBox>(TEXT("Row_SelectedMetrics"));
	auto* Name = E.Find<UTextBlock>(TEXT("Text_SelectedName"));
	Name->RemoveFromParent();
	Metrics->RemoveFromParent();
	auto* SelectedColumn = E.Make<UVerticalBox>(TEXT("Column_AuroraSelectedFacility"));
	Selected->AddChild(SelectedColumn);
	SelectedColumn->AddChildToVerticalBox(Name)->SetPadding(FMargin(0, 0, 0, 5));
	SelectedColumn->AddChildToVerticalBox(Metrics);
	return true;
}

bool CodexCard(FEdit& E)
{
	if (!E.Has({TEXT("Size_Card"), TEXT("Size_Name"), TEXT("Size_ThumbTarget"), TEXT("Size_ThumbPartner"),
		TEXT("Border_State"), TEXT("Text_Bonus"), TEXT("Vertical_Card")})) return false;
	E.Find<USizeBox>(TEXT("Size_Card"))->SetHeightOverride(242.f);
	E.Find<USizeBox>(TEXT("Size_Name"))->SetHeightOverride(38.f);
	for (const TCHAR* Side : {TEXT("Size_ThumbTarget"), TEXT("Size_ThumbPartner")})
		E.Find<USizeBox>(Side)->SetHeightOverride(74.f);
	E.Padding(TEXT("Border_Card"), FMargin(16, 13));
	E.Font(TEXT("Text_Name"), 20, true);
	E.Font(TEXT("Text_Bonus"), 25, true);
	E.Font(TEXT("Text_State"), 16);
	E.VPadding(TEXT("Horizontal_RecipeImages"), FMargin(0, 0, 0, 14));
	auto* Column = E.Find<UVerticalBox>(TEXT("Vertical_Card"));
	auto* Bonus = E.Find<UTextBlock>(TEXT("Text_Bonus"));
	auto* State = E.Find<UBorder>(TEXT("Border_State"));
	Bonus->RemoveFromParent();
	State->RemoveFromParent();
	auto* Summary = E.Make<UHorizontalBox>(TEXT("Horizontal_AuroraCardSummary"));
	Summary->AddChildToHorizontalBox(Bonus)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Summary->AddChildToHorizontalBox(State)->SetVerticalAlignment(VAlign_Center);
	Column->AddChildToVerticalBox(Summary);
	// Undiscovered recipes keep the green paper surface; only their source images are dimmed.
	if (UObject* CDO = E.BP.GeneratedClass ? E.BP.GeneratedClass->GetDefaultObject() : nullptr)
	{
		for (const TPair<const TCHAR*, FLinearColor>& Value : {
			TPair<const TCHAR*, FLinearColor>(TEXT("UndiscoveredBackgroundColor"), E.P.Paper),
			{TEXT("UndiscoveredOutlineColor"), E.P.Line}, {TEXT("UndiscoveredTitleColor"), E.P.Ink},
			{TEXT("UndiscoveredBodyColor"), E.P.InkMuted}})
			if (auto* Property = FindFProperty<FStructProperty>(CDO->GetClass(), Value.Key))
				*Property->ContainerPtrToValuePtr<FLinearColor>(CDO) = Value.Value;
	}
	return true;
}

bool CodexScreen(FEdit& E)
{
	if (!E.Has({TEXT("Horizontal_Header"), TEXT("Horizontal_Toolbar"), TEXT("Border_Summary"),
		TEXT("TileView_Entries"), TEXT("Size_Detail"), TEXT("Button_Undiscovered"), TEXT("Search_Entries")})) return false;
	E.Padding(TEXT("Border_InputShield"), FMargin(24, 24, 24, 64));
	E.Padding(TEXT("Border_Header"), FMargin(22, 13));
	E.Padding(TEXT("Border_Body"), FMargin(20, 14));
	E.Padding(TEXT("Panel_Detail"), FMargin(22, 20));
	E.Collapse(TEXT("Text_Intro"));
	E.Font(TEXT("Text_Title"), 30, true);
	if (auto* Title = E.Find<UTextBlock>(TEXT("Text_Title"))) Title->SetColorAndOpacity(E.P.Ink);
	E.Font(TEXT("Text_Summary"), 17);
	E.Font(TEXT("Text_Filter"), 15);
	E.Font(TEXT("Text_DetailTitle"), 30, true);
	E.Font(TEXT("Text_Bonus"), 27, true);
	E.Font(TEXT("Text_Distance"), 20, true);
	E.Font(TEXT("Text_Discovery"), 17);
	E.Font(TEXT("Text_Condition"), 18);
	E.Font(TEXT("Text_Applied"), 17);
	E.Font(TEXT("Text_TargetName"), 19, true);
	E.Font(TEXT("Text_PartnerName"), 19, true);
	E.Collapse(TEXT("Text_Equals"));
	E.Find<USizeBox>(TEXT("Size_Detail"))->SetWidthOverride(540.f);
	auto* Tiles = E.Find<UFF_FacilityCodexTileView>(TEXT("TileView_Entries"));
	Tiles->ColumnCount = 3;
	Tiles->SetEntryHeight(258.f);
	Tiles->SetHorizontalEntrySpacing(14.f);
	Tiles->SetVerticalEntrySpacing(14.f);
	auto* Summary = E.Find<UBorder>(TEXT("Border_Summary"));
	Summary->RemoveFromParent();
	auto* Header = E.Find<UHorizontalBox>(TEXT("Horizontal_Header"));
	auto* SummarySlot = Header->AddChildToHorizontalBox(Summary);
	SummarySlot->SetVerticalAlignment(VAlign_Center);
	SummarySlot->SetPadding(FMargin(12, 0));
	Header->ShiftChild(1, Summary);
	auto* Active = E.Make<UButton>(TEXT("Button_Active"), true);
	Active->SetStyle(E.Find<UButton>(TEXT("Button_Undiscovered"))->GetStyle());
	Active->AddChild(E.Text(TEXT("Button_Active_Label"), LOCTEXT("Active", "적용 중"), 18));
	auto* Toolbar = E.Find<UHorizontalBox>(TEXT("Horizontal_Toolbar"));
	Toolbar->AddChildToHorizontalBox(Active)->SetPadding(FMargin(0, 0, 16, 0));
	Toolbar->ShiftChild(3, Active);
	for (const TCHAR* Name : {TEXT("Button_All_Label"), TEXT("Button_Discovered_Label"), TEXT("Button_Undiscovered_Label")})
		E.Font(Name, 18, true);
	if (UObject* CDO = E.BP.GeneratedClass ? E.BP.GeneratedClass->GetDefaultObject() : nullptr)
	{
		if (auto* Property = FindFProperty<FFloatProperty>(CDO->GetClass(), TEXT("MinimumCardWidth")))
			Property->SetPropertyValue_InContainer(CDO, 320.f);
		if (auto* Property = FindFProperty<FFloatProperty>(CDO->GetClass(), TEXT("CompactDetailHeight")))
			Property->SetPropertyValue_InContainer(CDO, 160.f);
	}
	return true;
}

bool EnsureWorldBadgeAmountFit(FEdit& E)
{
	if (!E.Has({TEXT("Horizontal_Revenue"), TEXT("Text_RevenueValue")})) return false;
	auto* Amount = E.Find<UTextBlock>(TEXT("Text_RevenueValue"));
	if (E.Find<UScaleBox>(TEXT("Scale_AuroraWorldRevenueAmount"))) return true;
	auto* AmountSlot = Cast<UHorizontalBoxSlot>(Amount->Slot);
	if (!AmountSlot) return false;
	const FMargin Padding = AmountSlot->GetPadding();
	Amount->RemoveFromParent();
	auto* Fit = E.Make<UScaleBox>(TEXT("Scale_AuroraWorldRevenueAmount"));
	Fit->SetStretch(EStretch::ScaleToFit);
	Fit->SetStretchDirection(EStretchDirection::DownOnly);
	Fit->AddChild(Amount);
	auto* Slot = E.Find<UHorizontalBox>(TEXT("Horizontal_Revenue"))->AddChildToHorizontalBox(Fit);
	Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Slot->SetPadding(Padding);
	Slot->SetVerticalAlignment(VAlign_Center);
	return true;
}

bool WorldBadge(FEdit& E)
{
	if (!E.Has({TEXT("Size_Badge"), TEXT("Panel_Program"), TEXT("Panel_Revenue"),
		TEXT("Text_Available"), TEXT("Text_Scheduled"), TEXT("Text_RevenueValue")})) return false;
	E.Find<USizeBox>(TEXT("Size_Badge"))->SetWidthOverride(240.f);
	E.Find<USizeBox>(TEXT("Panel_Program"))->SetHeightOverride(44.f);
	E.Padding(TEXT("Border_Badge"), FMargin(2));
	E.Padding(TEXT("Border_Available"), FMargin(11, 8));
	E.Padding(TEXT("Border_Scheduled"), FMargin(11, 8));
	E.Padding(TEXT("Panel_Revenue"), FMargin(11, 6));
	E.Font(TEXT("Text_Available"), 16, true);
	E.Font(TEXT("Text_Scheduled"), 15, true);
	E.Font(TEXT("Text_RevenueLabel"), 11);
	E.Font(TEXT("Text_RevenueValue"), 14, true);
	// Root's material pass gives these two header states and the income band separate green films.
	for (const TCHAR* Name : {TEXT("Text_Available"), TEXT("Text_Scheduled")})
		E.Find<UTextBlock>(Name)->SetColorAndOpacity(E.P.Ink);
	for (const TCHAR* State : {TEXT("Available"), TEXT("Scheduled")})
		for (int32 Stroke = 0; Stroke < 7; ++Stroke)
			if (auto* Line = E.Find<UBorder>(*FString::Printf(TEXT("Icon_%s_%d"), State, Stroke)))
				Line->SetBrushColor(E.P.TealDeep);
	return EnsureWorldBadgeAmountFit(E);
}

bool ProgramCard(FEdit& E)
{
	if (!E.Has({TEXT("Button_Select"), TEXT("Text_ProgramName"), TEXT("Text_ProgramMeta"), TEXT("Text_ProgramAvailability")})) return false;
	E.Font(TEXT("Text_ProgramName"), 22, true);
	E.Font(TEXT("Text_ProgramMeta"), 16);
	E.Font(TEXT("Text_ProgramAvailability"), 16, true);
	E.VPadding(TEXT("Text_ProgramName"), FMargin(16, 11, 16, 5));
	E.VPadding(TEXT("Text_ProgramMeta"), FMargin(16, 0, 16, 4));
	E.VPadding(TEXT("Text_ProgramAvailability"), FMargin(16, 0, 16, 11));
	return true;
}

bool ProgramPicker(FEdit& E)
{
	if (!E.Has({TEXT("Border_Picker"), TEXT("Panel_PickerShell"), TEXT("Panel_Preview"),
		TEXT("Text_PreviewTime"), TEXT("Text_ResourcePreview"), TEXT("Text_PurchaseCost"), TEXT("Size_ProgramList")})) return false;
	E.Dock(TEXT("Border_Picker"), 600.f);
	E.Padding(TEXT("Border_Header"), FMargin(18, 12));
	E.Padding(TEXT("Border_Body"), FMargin(16, 12));
	E.Padding(TEXT("Border_Preview"), FMargin(16, 14));
	E.Padding(TEXT("Border_Footer"), FMargin(18, 12));
	E.Collapse(TEXT("Text_Eyebrow"));
	E.Font(TEXT("Text_Title"), 28, true);
	E.Find<UTextBlock>(TEXT("Text_Title"))->SetText(LOCTEXT("ProgramTitle", "유료 프로그램 편성"));
	E.Find<UTextBlock>(TEXT("Text_Title"))->SetColorAndOpacity(E.P.Ink);
	E.Font(TEXT("Text_FacilityName"), 21, true);
	E.Font(TEXT("Text_Instruction"), 15);
	E.Font(TEXT("Text_PreviewName"), 28, true);
	E.Font(TEXT("Text_PreviewTime"), 18);
	E.Font(TEXT("Text_PreviewDescription"), 17);
	E.Font(TEXT("Text_PreviewAvailability"), 17, true);
	E.Font(TEXT("Text_RequiredStaff"), 24, true);
	E.Font(TEXT("Text_Capacity"), 24, true);
	E.Font(TEXT("Text_TicketFee"), 24, true);
	E.Font(TEXT("Text_MaxRevenue"), 19, true);
	E.Font(TEXT("Text_ResourcePreview"), 19, true);
	E.Font(TEXT("Text_PurchaseCost"), 33, true);
	E.Font(TEXT("Text_PurchaseCostLabel"), 17);
	E.Font(TEXT("Text_NoRefundNotice"), 14);
	E.Font(TEXT("Text_Confirm"), 22, true);
	E.Font(TEXT("Text_Feedback"), 17);
	E.Find<USizeBox>(TEXT("Size_ProgramList"))->SetMaxDesiredHeight(205.f);
	// The preview description is content, not a duplicate metric: keep it visible with real data.
	E.Find<UTextBlock>(TEXT("Text_PreviewDescription"))->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

bool Apply(const TCHAR* Path, UClass* Parent, bool (*Edit)(FEdit&))
{
	auto* BP = LoadExistingBlueprint(Path);
	if (!BP || !BP->WidgetTree || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(Parent))
	{
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: missing or incompatible %s; existing assets preserved."), Prefix, Path);
		return false;
	}
	if (BP->GetOutermost()->GetMetaData().GetValue(BP, Version) == TEXT("1")) return true;
	BP->Modify();
	BP->WidgetTree->Modify();
	FEdit Context{*BP, *BP->WidgetTree};
	if (!Edit(Context)) return false;
	BP->GetOutermost()->GetMetaData().SetValue(BP, Version, TEXT("1"));
	return SaveNewBlueprint(Prefix, *BP);
}

void MigrateWorldBadgeAmountFit()
{
	auto* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityWorldBadgePackage);
	if (!BP || !BP->WidgetTree || !BP->GeneratedClass ||
		!BP->GeneratedClass->IsChildOf(UFF_FacilityProgramMarkerWidget::StaticClass())) return;
	FEdit Context{*BP, *BP->WidgetTree};
	if (Context.Find<UScaleBox>(TEXT("Scale_AuroraWorldRevenueAmount"))) return;
	BP->Modify();
	BP->WidgetTree->Modify();
	if (EnsureWorldBadgeAmountFit(Context) && SaveNewBlueprint(Prefix, *BP))
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_AURORA_WORLD_BADGE_AMOUNT_FIT_DONE"));
}

void Migrate()
{
	bool Success = true;
	Success &= Apply(FFBuildUIAssetPaths::FacilityRevenueRankRowPackage, UFF_FacilityRevenueRankRow::StaticClass(), RevenueRow);
	Success &= Apply(FFBuildUIAssetPaths::FacilityRevenueRankingPackage, UFF_FacilityRevenueRankingPanel::StaticClass(), RevenuePanel);
	Success &= Apply(FFBuildUIAssetPaths::FacilityWorldBadgePackage, UFF_FacilityProgramMarkerWidget::StaticClass(), WorldBadge);
	MigrateWorldBadgeAmountFit();
	Success &= Apply(FFBuildUIAssetPaths::FacilityCodexCardPackage, UFF_FacilityCodexCard::StaticClass(), CodexCard);
	Success &= Apply(FFBuildUIAssetPaths::FacilityCodexScreenPackage, UFF_FacilityCodexScreen::StaticClass(), CodexScreen);
	Success &= Apply(FFBuildUIAssetPaths::FacilityPaidProgramCardPackage, UFF_FacilityPaidProgramCard::StaticClass(), ProgramCard);
	Success &= Apply(FFBuildUIAssetPaths::FacilityPaidProgramPickerPackage, UFF_FacilityPaidProgramPicker::StaticClass(), ProgramPicker);
	if (Success) UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_AURORA_REVENUE_PROGRAMS_DONE: V7 layouts saved; runtime ledger and scheduling authority unchanged."));
}

FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityAuroraRevenueAndPrograms"),
	TEXT("Apply approved V7 revenue, codex, paid-program and world badge layouts once. Preserve bindings and later Designer edits."),
	FConsoleCommandDelegate::CreateStatic(&Migrate));
FAutoConsoleCommand AmountFitCommand(TEXT("FF.ApplyFacilityAuroraWorldBadgeAmountFit"),
	TEXT("Fit large revenue numbers inside the existing V7 world badge without changing its theme or width."),
	FConsoleCommandDelegate::CreateStatic(&MigrateWorldBadgeAmountFit));
}
#undef LOCTEXT_NAMESPACE
#endif
