// Adds only an independent synergy accent layer to existing Designer-owned card assets.
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Building/UI/FF_BuildFacilityCard.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"

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
	const bool bPrep = AddHighlightLayer(TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile"),
		*UFF_PrepBuildTileWidget::StaticClass());
	const bool bLegacy = AddHighlightLayer(TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildFacilityCard"),
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
	const bool bPrep = AddRequiredFacilityBadges(TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile"),
		*UFF_PrepBuildTileWidget::StaticClass());
	const bool bLegacy = AddRequiredFacilityBadges(TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildFacilityCard"),
		*UFF_BuildFacilityCard::StaticClass());
	UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_REQUIRED_CARD_UMG_RESULT: Prep=%s Legacy=%s"),
		bPrep ? TEXT("OK") : TEXT("FAIL"), bLegacy ? TEXT("OK") : TEXT("FAIL"));
}

FAutoConsoleCommand AddBuildRequiredFacilityBadgesCommand(TEXT("FF.AddBuildRequiredFacilityBadges"),
	TEXT("Adds editable required-facility status badges once; preserves existing card and synergy styles."),
	FConsoleCommandDelegate::CreateStatic(&AddBuildRequiredFacilityBadges));
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
