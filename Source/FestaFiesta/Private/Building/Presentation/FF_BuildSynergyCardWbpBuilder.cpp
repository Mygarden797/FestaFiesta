// Adds only an independent synergy accent layer to existing Designer-owned card assets.
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Building/UI/FF_BuildFacilityCard.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFBuildSynergyCardWbpBuilder"

namespace FFBuildSynergyCardWbpBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* LogPrefix = TEXT("FF_BUILD_SYNERGY_CARD_UMG");

bool AddHighlightLayer(const FString& Package, const UClass& ExpectedParent)
{
	UWidgetBlueprint* Blueprint = LoadExistingBlueprint(Package);
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(&ExpectedParent) ||
		!Blueprint->WidgetTree || !Blueprint->WidgetTree->RootWidget)
	{
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: missing/incompatible card preserved: %s"), LogPrefix, *Package);
		return false;
	}
	UWidgetTree& Tree = *Blueprint->WidgetTree;
	UWidget* ExistingHighlight = Tree.FindWidget(TEXT("Border_SynergyHighlight"));
	UWidget* ExistingBadge = Tree.FindWidget(TEXT("Text_SynergyBadge"));
	if (ExistingHighlight || ExistingBadge)
	{
		// A designer may rearrange this layer later. The two native binding types are
		// the contract; their layout, colors and text must never be re-applied here.
		if (Cast<UBorder>(ExistingHighlight) && Cast<UTextBlock>(ExistingBadge) &&
			ExistingHighlight->bIsVariable && ExistingBadge->bIsVariable)
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: designer accent preserved without saving: %s"), LogPrefix, *Package);
			return true;
		}
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: incomplete/incompatible synergy bindings preserved: %s"), LogPrefix, *Package);
		return false;
	}
	for (const FName Name : { FName(TEXT("Overlay_SynergyAccent")), FName(TEXT("Overlay_SynergyBadge")),
		FName(TEXT("Border_SynergyBadge")) })
	{
		if (Tree.FindWidget(Name))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: reserved widget name already exists; card preserved: %s/%s"),
				LogPrefix, *Package, *Name.ToString());
			return false;
		}
	}

	Blueprint->Modify();
	Tree.Modify();
	UWidget* OriginalRoot = Tree.RootWidget;
	UOverlay* Root = MakeWidget<UOverlay>(*Blueprint, Tree, TEXT("Overlay_SynergyAccent"));
	UBorder* Highlight = MakeWidget<UBorder>(*Blueprint, Tree, TEXT("Border_SynergyHighlight"), true);
	UOverlay* BadgeLayer = MakeWidget<UOverlay>(*Blueprint, Tree, TEXT("Overlay_SynergyBadge"));
	UBorder* Badge = MakeWidget<UBorder>(*Blueprint, Tree, TEXT("Border_SynergyBadge"));
	UTextBlock* Label = MakeWidget<UTextBlock>(*Blueprint, Tree, TEXT("Text_SynergyBadge"), true);
	if (!Root || !Highlight || !BadgeLayer || !Badge || !Label) { return false; }

	// The previous root and every one of its properties/descendant slots stay intact.
	// The new overlay measures the original card; its small badge cannot enlarge it.
	Tree.RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlaySlot* OriginalSlot = Root->AddChildToOverlay(OriginalRoot);
	OriginalSlot->SetHorizontalAlignment(HAlign_Fill);
	OriginalSlot->SetVerticalAlignment(VAlign_Fill);
	UOverlaySlot* HighlightSlot = Root->AddChildToOverlay(Highlight);
	HighlightSlot->SetHorizontalAlignment(HAlign_Fill);
	HighlightSlot->SetVerticalAlignment(VAlign_Fill);

	const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
	FSlateBrush Outline;
	Outline.DrawAs = ESlateBrushDrawType::RoundedBox;
	Outline.TintColor = FSlateColor(FLinearColor::Transparent);
	Outline.OutlineSettings = FSlateBrushOutlineSettings(7.0f, FSlateColor(Palette.Info), 2.0f);
	Highlight->SetBrush(Outline);
	Highlight->SetBrushColor(FLinearColor::White);
	Highlight->SetPadding(FMargin(5.0f));
	Highlight->SetVisibility(ESlateVisibility::Collapsed);
	Highlight->AddChild(BadgeLayer);
	BadgeLayer->SetVisibility(ESlateVisibility::HitTestInvisible);

	FSlateBrush BadgeBrush;
	BadgeBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	BadgeBrush.TintColor = FSlateColor(Palette.Info);
	BadgeBrush.OutlineSettings = FSlateBrushOutlineSettings(4.0f);
	Badge->SetBrush(BadgeBrush);
	Badge->SetBrushColor(FLinearColor::White);
	Badge->SetPadding(FMargin(5.0f, 2.0f));
	Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
	Badge->AddChild(Label);
	UOverlaySlot* BadgeSlot = BadgeLayer->AddChildToOverlay(Badge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Left);
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	StyleText(*Label, 12, Palette.White, TEXT("SemiBold"));
	Label->SetText(LOCTEXT("SynergyBadge", "조합 시설"));
	Label->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (!SaveNewBlueprint(LogPrefix, *Blueprint)) { return false; }
	UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_ADD: original card tree retained under accent overlay: %s"), LogPrefix, *Package);
	return true;
}

void AddBuildSynergyCardHighlights()
{
	const bool bPrep = AddHighlightLayer(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile"),
		*UFF_PrepBuildTileWidget::StaticClass());
	const bool bLegacy = AddHighlightLayer(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildFacilityCard"),
		*UFF_BuildFacilityCard::StaticClass());
	UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_RESULT: Prep=%s Legacy=%s"), LogPrefix,
		bPrep ? TEXT("OK") : TEXT("FAIL"), bLegacy ? TEXT("OK") : TEXT("FAIL"));
}

FAutoConsoleCommand AddBuildSynergyCardHighlightsCommand(TEXT("FF.AddBuildSynergyCardHighlights"),
	TEXT("Adds editable synergy badge/outline layers to existing build cards once; never rebuilds Designer content."),
	FConsoleCommandDelegate::CreateStatic(&AddBuildSynergyCardHighlights));

bool AddRequiredFacilityBadges(const FString& Package, const UClass& ExpectedParent)
{
	constexpr const TCHAR* RequiredLog = TEXT("FF_BUILD_REQUIRED_CARD_UMG");
	UWidgetBlueprint* Blueprint = LoadExistingBlueprint(Package);
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(&ExpectedParent) ||
		!Blueprint->WidgetTree || !Blueprint->WidgetTree->RootWidget)
	{
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: missing/incompatible card preserved: %s"), RequiredLog, *Package);
		return false;
	}
	UWidgetTree& Tree = *Blueprint->WidgetTree;
	const FName Names[] = { TEXT("Overlay_RequiredStatus"), TEXT("Border_RequiredMissing"),
		TEXT("Border_RequiredSatisfied"), TEXT("Text_RequiredMissing"), TEXT("Text_RequiredSatisfied") };
	bool bHasAnyBinding = false;
	for (const FName Name : Names) { bHasAnyBinding |= Tree.FindWidget(Name) != nullptr; }
	if (bHasAnyBinding)
	{
		UWidget* Missing = Tree.FindWidget(Names[1]);
		UWidget* Satisfied = Tree.FindWidget(Names[2]);
		UWidget* MissingText = Tree.FindWidget(Names[3]);
		UWidget* SatisfiedText = Tree.FindWidget(Names[4]);
		if (Cast<UOverlay>(Tree.FindWidget(Names[0])) && Cast<UBorder>(Missing) && Cast<UBorder>(Satisfied) &&
			Cast<UTextBlock>(MissingText) && Cast<UTextBlock>(SatisfiedText) &&
			Missing->bIsVariable && Satisfied->bIsVariable && MissingText->bIsVariable && SatisfiedText->bIsVariable)
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: designer badges preserved without saving: %s"), RequiredLog, *Package);
			return true;
		}
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: incomplete/incompatible required bindings preserved: %s"), RequiredLog, *Package);
		return false;
	}
	UOverlay* Root = Cast<UOverlay>(Tree.RootWidget);
	if (!Root && Tree.FindWidget(TEXT("Overlay_RequiredAccent")))
	{
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: reserved root name already exists; card preserved: %s"), RequiredLog, *Package);
		return false;
	}

	Blueprint->Modify();
	Tree.Modify();
	if (!Root)
	{
		// A custom card without the synergy layer can still receive requirements.
		// Preserve its complete tree as the first child of one new measuring overlay.
		UWidget* OriginalRoot = Tree.RootWidget;
		Root = MakeWidget<UOverlay>(*Blueprint, Tree, TEXT("Overlay_RequiredAccent"));
		if (!Root) { return false; }
		Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UOverlaySlot* OriginalSlot = Root->AddChildToOverlay(OriginalRoot);
		OriginalSlot->SetHorizontalAlignment(HAlign_Fill);
		OriginalSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UOverlay* Layer = MakeWidget<UOverlay>(*Blueprint, Tree, Names[0]);
	if (!Layer) { return false; }
	Layer->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Layer);
	LayerSlot->SetHorizontalAlignment(HAlign_Fill);
	LayerSlot->SetVerticalAlignment(VAlign_Fill);
	const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
	auto AddBadge = [&](const FName BorderName, const FName TextName, const FText& Label,
		const FLinearColor& BackgroundColor, const FLinearColor& TextColor)
	{
		UBorder* Badge = MakeWidget<UBorder>(*Blueprint, Tree, BorderName, true);
		UTextBlock* Text = MakeWidget<UTextBlock>(*Blueprint, Tree, TextName, true);
		if (!Badge || !Text) { return false; }
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(BackgroundColor);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(4.0f);
		Badge->SetBrush(Brush);
		Badge->SetBrushColor(FLinearColor::White);
		Badge->SetPadding(FMargin(5.0f, 2.0f));
		Badge->SetVisibility(ESlateVisibility::Collapsed);
		Badge->AddChild(Text);
		StyleText(*Text, 12, TextColor, TEXT("SemiBold"));
		Text->SetText(Label);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* BadgeSlot = Layer->AddChildToOverlay(Badge);
		BadgeSlot->SetHorizontalAlignment(HAlign_Left);
		BadgeSlot->SetVerticalAlignment(VAlign_Top);
		// Keep the existing synergy badge at (5,5). Requirements occupy a second
		// thumbnail row, leaving favorite controls and all original labels untouched.
		BadgeSlot->SetPadding(FMargin(5.0f, 30.0f, 5.0f, 5.0f));
		return true;
	};
	if (!AddBadge(Names[1], Names[3], LOCTEXT("RequiredMissingBadge", "필수 설치"), Palette.Amber, Palette.Ink) ||
		!AddBadge(Names[2], Names[4], LOCTEXT("RequiredSatisfiedBadge", "필수 충족"), Palette.Teal, Palette.White)) { return false; }
	if (!SaveNewBlueprint(RequiredLog, *Blueprint)) { return false; }
	UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_ADD: original widgets and synergy badge positions retained: %s"), RequiredLog, *Package);
	return true;
}

void AddBuildRequiredFacilityBadges()
{
	const bool bPrep = AddRequiredFacilityBadges(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile"),
		*UFF_PrepBuildTileWidget::StaticClass());
	const bool bLegacy = AddRequiredFacilityBadges(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildFacilityCard"),
		*UFF_BuildFacilityCard::StaticClass());
	UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_REQUIRED_CARD_UMG_RESULT: Prep=%s Legacy=%s"),
		bPrep ? TEXT("OK") : TEXT("FAIL"), bLegacy ? TEXT("OK") : TEXT("FAIL"));
}

FAutoConsoleCommand AddBuildRequiredFacilityBadgesCommand(TEXT("FF.AddBuildRequiredFacilityBadges"),
	TEXT("Adds editable required-facility status badges once; preserves existing card and synergy styles."),
	FConsoleCommandDelegate::CreateStatic(&AddBuildRequiredFacilityBadges));

// This migration edits only the emphasis widgets and the existing header hint row.
// The saved WBP remains the design source after the first successful application.
constexpr const TCHAR* EmphasisLog = TEXT("FF_BUILD_CATALOG_EMPHASIS_UMG");
constexpr const TCHAR* EmphasisVersionKey = TEXT("FF.BuildCatalog.EmphasisVersion");
constexpr const TCHAR* EmphasisVersion = TEXT("20260920.v1");

FLinearColor EmphasisColor(const TCHAR* Hex)
{
	return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
}

void StyleEmphasisBadge(UBorder& Border, UTextBlock& Label, const FText& Text,
	const FLinearColor& Fill, const FLinearColor& Foreground)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Fill);
	Brush.OutlineSettings = FSlateBrushOutlineSettings(4.0f);
	Border.SetBrush(Brush);
	Border.SetBrushColor(FLinearColor::White);
	Border.SetPadding(FMargin(6.0f, 2.0f));
	Border.SetVisibility(ESlateVisibility::Collapsed);
	StyleText(Label, 12, Foreground, TEXT("SemiBold"));
	Label.SetAutoWrapText(false);
	Label.SetText(Text);
	Label.SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool HasEmphasisVersion(UWidgetBlueprint& Blueprint)
{
	return Blueprint.GetOutermost()->GetMetaData().GetValue(&Blueprint, EmphasisVersionKey) == EmphasisVersion;
}

bool SaveEmphasis(UWidgetBlueprint& Blueprint)
{
	FMetaData& Metadata = Blueprint.GetOutermost()->GetMetaData();
	const FString PreviousVersion = Metadata.GetValue(&Blueprint, EmphasisVersionKey);
	Metadata.SetValue(&Blueprint, EmphasisVersionKey, EmphasisVersion);
	if (SaveNewBlueprint(EmphasisLog, Blueprint)) { return true; }
	if (PreviousVersion.IsEmpty()) { Metadata.RemoveValue(&Blueprint, EmphasisVersionKey); }
	else { Metadata.SetValue(&Blueprint, EmphasisVersionKey, *PreviousVersion); }
	return false;
}

bool ValidateEmphasisCard(UWidgetBlueprint* Blueprint, const UClass& Parent, const FName ThumbnailName)
{
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(&Parent) ||
		!Blueprint->WidgetTree || !Blueprint->WidgetTree->RootWidget) { return false; }
	UWidgetTree& Tree = *Blueprint->WidgetTree;
	if (!Cast<UOverlay>(Tree.FindWidget(ThumbnailName))) { return false; }
	const FName RequiredNames[] = { TEXT("Overlay_RequiredStatus"), TEXT("Border_RequiredMissing"),
		TEXT("Border_RequiredSatisfied"), TEXT("Text_RequiredMissing"), TEXT("Text_RequiredSatisfied") };

	bool bHasRequired = false;
	for (const FName Name : RequiredNames) { bHasRequired |= Tree.FindWidget(Name) != nullptr; }
	if (bHasRequired)
	{
		auto* Layer = Cast<UOverlay>(Tree.FindWidget(RequiredNames[0]));
		auto* Missing = Cast<UBorder>(Tree.FindWidget(RequiredNames[1]));
		auto* Satisfied = Cast<UBorder>(Tree.FindWidget(RequiredNames[2]));
		auto* MissingLabel = Cast<UTextBlock>(Tree.FindWidget(RequiredNames[3]));
		auto* SatisfiedLabel = Cast<UTextBlock>(Tree.FindWidget(RequiredNames[4]));
		if (!Layer || !Missing || !Satisfied || !MissingLabel || !SatisfiedLabel ||
			!Missing->bIsVariable || !Satisfied->bIsVariable || !MissingLabel->bIsVariable || !SatisfiedLabel->bIsVariable ||
			Missing->GetParent() != Layer || Satisfied->GetParent() != Layer ||
			Missing->GetContent() != MissingLabel || Satisfied->GetContent() != SatisfiedLabel) { return false; }
	}
	UWidget* ProgramBorder = Tree.FindWidget(TEXT("Border_ProgramBadge"));
	UWidget* ProgramText = Tree.FindWidget(TEXT("Text_ProgramBadge"));
	if (HasEmphasisVersion(*Blueprint))
	{
		return bHasRequired && Cast<UBorder>(ProgramBorder) && Cast<UTextBlock>(ProgramText) &&
			ProgramBorder->bIsVariable && ProgramText->bIsVariable;
	}
	// A matching name without our version could belong to a designer's custom widget.
	return !ProgramBorder && !ProgramText;
}

bool MigrateEmphasisCard(UWidgetBlueprint& Blueprint, const FName ThumbnailName)
{
	if (HasEmphasisVersion(Blueprint))
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer edits preserved: %s"), EmphasisLog, *Blueprint.GetName());
		return true;
	}
	Blueprint.Modify();
	UWidgetTree& Tree = *Blueprint.WidgetTree;
	Tree.Modify();
	auto* Thumbnail = CastChecked<UOverlay>(Tree.FindWidget(ThumbnailName));
	auto* Required = Cast<UOverlay>(Tree.FindWidget(TEXT("Overlay_RequiredStatus")));
	if (!Required) { Required = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_RequiredStatus")); }
	if (!Required) { return false; }
	Required->RemoveFromParent();
	Required->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* RequiredSlot = Thumbnail->AddChildToOverlay(Required);
	RequiredSlot->SetHorizontalAlignment(HAlign_Fill);
	RequiredSlot->SetVerticalAlignment(VAlign_Fill);

	auto RequiredBadge = [&](const FName BorderName, const FName LabelName, const FText& Text,
		const FLinearColor& Fill, const FLinearColor& Foreground)
	{
		auto* Border = Cast<UBorder>(Tree.FindWidget(BorderName));
		auto* Label = Cast<UTextBlock>(Tree.FindWidget(LabelName));
		if (!Border)
		{
			Border = MakeWidget<UBorder>(Blueprint, Tree, BorderName, true);
			Label = MakeWidget<UTextBlock>(Blueprint, Tree, LabelName, true);
			if (!Border || !Label) { return false; }
			Border->AddChild(Label);
			Required->AddChildToOverlay(Border);
		}
		StyleEmphasisBadge(*Border, *Label, Text, Fill, Foreground);
		auto* Slot = CastChecked<UOverlaySlot>(Border->Slot);
		Slot->SetHorizontalAlignment(HAlign_Left);
		Slot->SetVerticalAlignment(VAlign_Top);
		// Favorite occupies y=3..33 in the 86-unit thumbnail. Keep the full day-one
		// message below it, then reserve a distinct bottom row for program support.
		Slot->SetPadding(FMargin(4.0f, 35.0f, 4.0f, 0.0f));
		return true;
	};
	if (!RequiredBadge(TEXT("Border_RequiredMissing"), TEXT("Text_RequiredMissing"),
		LOCTEXT("DayOneRequiredMissing", "1일차 필수 설치"), EmphasisColor(TEXT("F2B544")), EmphasisColor(TEXT("442B09"))) ||
		!RequiredBadge(TEXT("Border_RequiredSatisfied"), TEXT("Text_RequiredSatisfied"),
		LOCTEXT("DayOneRequiredSatisfied", "1일차 필수 완료"), EmphasisColor(TEXT("28735F")), FLinearColor::White)) { return false; }

	auto* Program = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ProgramBadge"), true);
	auto* ProgramLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ProgramBadge"), true);
	if (!Program || !ProgramLabel) { return false; }
	Program->AddChild(ProgramLabel);
	StyleEmphasisBadge(*Program, *ProgramLabel, LOCTEXT("ProgramAvailableBadge", "프로그램"),
		EmphasisColor(TEXT("7053A0")), FLinearColor::White);
	auto* ProgramSlot = Thumbnail->AddChildToOverlay(Program);
	ProgramSlot->SetHorizontalAlignment(HAlign_Left);
	ProgramSlot->SetVerticalAlignment(VAlign_Bottom);
	ProgramSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 3.0f));
	return SaveEmphasis(Blueprint);
}

bool ValidateEmphasisPanel(UWidgetBlueprint* Blueprint)
{
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(UFF_PrepPanelBuild::StaticClass()) ||
		!Blueprint->WidgetTree) { return false; }
	UWidgetTree& Tree = *Blueprint->WidgetTree;
	auto* Title = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_CatalogTitle")));
	auto* Hint = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_StudioHint")));
	if (!Title || !Hint) { return false; }
	const FName AddedNames[] = { TEXT("HorizontalBox_CatalogEmphasisGuide"), TEXT("Border_CatalogRequiredGuide"),
		TEXT("Border_CatalogProgramLegend"), TEXT("Text_CatalogProgramLegend"), TEXT("Text_CatalogProgramGuide") };
	if (HasEmphasisVersion(*Blueprint))
	{
		return Cast<UHorizontalBox>(Tree.FindWidget(AddedNames[0])) && Cast<UBorder>(Tree.FindWidget(AddedNames[1])) &&
			Cast<UBorder>(Tree.FindWidget(AddedNames[2])) && Cast<UTextBlock>(Tree.FindWidget(AddedNames[3])) &&
			Cast<UTextBlock>(Tree.FindWidget(AddedNames[4]));
	}
	for (const FName Name : AddedNames) { if (Tree.FindWidget(Name)) { return false; } }
	return Hint->GetParent() == Title && Cast<UVerticalBoxSlot>(Hint->Slot);
}

bool MigrateRequiredGuideFlow(UWidgetBlueprint& Blueprint)
{
	constexpr const TCHAR* FlowKey = TEXT("FF.BuildCatalog.RequiredGuideVersion");
	FMetaData& Metadata = Blueprint.GetOutermost()->GetMetaData();
	if (Metadata.GetValue(&Blueprint, FlowKey) == TEXT("1")) { return true; }
	auto* Required = Cast<UBorder>(Blueprint.WidgetTree->FindWidget(TEXT("Border_CatalogRequiredGuide")));
	auto* Program = Cast<UBorder>(Blueprint.WidgetTree->FindWidget(TEXT("Border_CatalogProgramLegend")));
	auto* RequiredSlot = Required ? Cast<UHorizontalBoxSlot>(Required->Slot) : nullptr;
	auto* ProgramSlot = Program ? Cast<UHorizontalBoxSlot>(Program->Slot) : nullptr;
	if (!RequiredSlot || !ProgramSlot) { return false; }
	Blueprint.Modify();
	Blueprint.WidgetTree->Modify();
	Required->Modify();
	RequiredSlot->Modify();
	ProgramSlot->Modify();
	Required->bIsVariable = true;
	// Keep the existing gap while the instruction is present. Collapsing the
	// instruction now removes its gap too, leaving the program legend flush left.
	FMargin RequiredPadding = RequiredSlot->GetPadding();
	FMargin ProgramPadding = ProgramSlot->GetPadding();
	RequiredPadding.Right += ProgramPadding.Left;
	ProgramPadding.Left = 0.0f;
	RequiredSlot->SetPadding(RequiredPadding);
	ProgramSlot->SetPadding(ProgramPadding);
	Metadata.SetValue(&Blueprint, FlowKey, TEXT("1"));
	if (SaveNewBlueprint(EmphasisLog, Blueprint)) { return true; }
	Metadata.RemoveValue(&Blueprint, FlowKey);
	return false;
}

bool MigrateEmphasisPanel(UWidgetBlueprint& Blueprint)
{
	if (HasEmphasisVersion(Blueprint))
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer header preserved: %s"), EmphasisLog, *Blueprint.GetName());
		return MigrateRequiredGuideFlow(Blueprint);
	}
	Blueprint.Modify();
	UWidgetTree& Tree = *Blueprint.WidgetTree;
	Tree.Modify();
	auto* Title = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_CatalogTitle")));
	auto* Hint = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_StudioHint")));
	auto* OldSlot = CastChecked<UVerticalBoxSlot>(Hint->Slot);
	const FMargin Padding = OldSlot->GetPadding();
	const FSlateChildSize SlotSize = OldSlot->GetSize();
	const EHorizontalAlignment HorizontalAlignment = OldSlot->GetHorizontalAlignment();
	const EVerticalAlignment VerticalAlignment = OldSlot->GetVerticalAlignment();
	const int32 HintIndex = Title->GetChildIndex(Hint);
	auto* Guide = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_CatalogEmphasisGuide"));
	auto* Required = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CatalogRequiredGuide"));
	auto* Program = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CatalogProgramLegend"));
	auto* ProgramLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CatalogProgramLegend"));
	auto* ProgramHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CatalogProgramGuide"));
	if (!Guide || !Required || !Program || !ProgramLabel || !ProgramHint) { return false; }
	Hint->RemoveFromParent();
	Required->AddChild(Hint);
	StyleEmphasisBadge(*Required, *Hint, LOCTEXT("DayOneCatalogGuide", "메인시설은 1일차 시작 전 필수 설치"),
		EmphasisColor(TEXT("FFF0CE")), EmphasisColor(TEXT("805013")));
	Required->SetVisibility(ESlateVisibility::HitTestInvisible);
	Program->AddChild(ProgramLabel);
	StyleEmphasisBadge(*Program, *ProgramLabel, LOCTEXT("CatalogProgramLegend", "프로그램"),
		EmphasisColor(TEXT("7053A0")), FLinearColor::White);
	Program->SetVisibility(ESlateVisibility::HitTestInvisible);
	StyleText(*ProgramHint, 12, EmphasisColor(TEXT("675775")), TEXT("Medium"));
	ProgramHint->SetText(LOCTEXT("CatalogProgramGuide", "편성 가능한 시설"));
	ProgramHint->SetVisibility(ESlateVisibility::HitTestInvisible);
	Guide->SetVisibility(ESlateVisibility::HitTestInvisible);
	Guide->AddChildToHorizontalBox(Required)->SetVerticalAlignment(VAlign_Center);
	auto* ProgramSlot = Guide->AddChildToHorizontalBox(Program);
	ProgramSlot->SetVerticalAlignment(VAlign_Center);
	ProgramSlot->SetPadding(FMargin(12.0f, 0.0f, 5.0f, 0.0f));
	Guide->AddChildToHorizontalBox(ProgramHint)->SetVerticalAlignment(VAlign_Center);
	auto* GuideSlot = CastChecked<UVerticalBoxSlot>(Title->InsertChildAt(HintIndex, Guide));
	GuideSlot->SetPadding(Padding);
	GuideSlot->SetSize(SlotSize);
	GuideSlot->SetHorizontalAlignment(HorizontalAlignment);
	GuideSlot->SetVerticalAlignment(VerticalAlignment);
	return SaveEmphasis(Blueprint) && MigrateRequiredGuideFlow(Blueprint);
}

void RefreshBuildCatalogEmphasis()
{
	UWidgetBlueprint* Prep = LoadExistingBlueprint(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile"));
	UWidgetBlueprint* Legacy = LoadExistingBlueprint(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildFacilityCard"));
	UWidgetBlueprint* Panel = LoadExistingBlueprint(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild"));
	// Validate all existing structures before modifying or saving any target.
	if (!ValidateEmphasisCard(Prep, *UFF_PrepBuildTileWidget::StaticClass(), TEXT("Overlay_Thumbnail")) ||
		!ValidateEmphasisCard(Legacy, *UFF_BuildFacilityCard::StaticClass(), TEXT("Thumbnail_Layer")) ||
		!ValidateEmphasisPanel(Panel))
	{
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_ABORT: card/header contract differs; all assets were preserved."), EmphasisLog);
		return;
	}
	const bool bPrep = MigrateEmphasisCard(*Prep, TEXT("Overlay_Thumbnail"));
	const bool bLegacy = bPrep && MigrateEmphasisCard(*Legacy, TEXT("Thumbnail_Layer"));
	const bool bPanel = bLegacy && MigrateEmphasisPanel(*Panel);
	UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_RESULT: Prep=%s Legacy=%s Panel=%s"), EmphasisLog,
		bPrep ? TEXT("OK") : TEXT("FAIL"), bLegacy ? TEXT("OK") : TEXT("FAIL"), bPanel ? TEXT("OK") : TEXT("FAIL"));
}

FAutoConsoleCommand RefreshBuildCatalogEmphasisCommand(TEXT("FF.RefreshBuildCatalogEmphasisUMG"),
	TEXT("Adds day-one required/program catalog badges and a header legend once; keeps later Designer edits."),
	FConsoleCommandDelegate::CreateStatic(&RefreshBuildCatalogEmphasis));
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
