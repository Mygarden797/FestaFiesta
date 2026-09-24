// Saved visual migration for the facility inspector's combination tab and recipe cards.

#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilitySynergyEntryWidget.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace FFFacilitySynergyPearl
{
using namespace FFUIWbpBuilder;

constexpr const TCHAR* Version = TEXT("FF.FacilitySynergy.Pearl.20260924.v2");
constexpr const TCHAR* Prefix = TEXT("FF_FACILITY_SYNERGY_PEARL");

template <typename T>
static T* Find(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(Name));
}

static void Paint(UBorder& Border, const FSlateBrush& Brush, const FMargin Padding)
{
    Border.Modify();
    Border.SetBrush(Brush);
    Border.SetBrushColor(FLinearColor::White);
    Border.SetContentColorAndOpacity(FLinearColor::White);
    Border.SetPadding(Padding);
}

static void Label(UTextBlock& Text, const int32 Size, const FLinearColor& Color,
    const bool bStrong = false)
{
    Text.Modify();
    // CommonTextBlock would otherwise restore the previous green text style on creation.
    if (auto* Common = Cast<UCommonTextBlock>(&Text)) Common->SetStyle(nullptr);
    StyleText(Text, Size, Color, bStrong ? TEXT("SemiBold") : TEXT("Regular"));
    Text.SetAutoWrapText(true);
    Text.SetWrapTextAt(0);
    Text.SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    Text.SetLineHeightPercentage(1.f);
    Text.SetJustification(ETextJustify::Left);
}

static void Spacing(UWidget& Widget, const FMargin Padding)
{
    if (auto* Slot = Cast<UVerticalBoxSlot>(Widget.Slot))
    {
        Slot->Modify();
        Slot->SetPadding(Padding);
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        Slot->SetHorizontalAlignment(HAlign_Fill);
    }
}

static void Button(UButton& Control, const bool bPartner)
{
    Control.Modify();
    FButtonStyle Style = Control.GetStyle();
    Style.Normal = FFBuildPearl::Rounded(
        bPartner ? FFBuildPearl::Color(231, 243, 253) : FFBuildPearl::Color(241, 244, 253),
        14.f, FFBuildPearl::Line, 1.f);
    Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(218, 239, 253),
        14.f, FFBuildPearl::Cyan, 1.f);
    Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Color(215, 225, 245),
        14.f, FFBuildPearl::Color(156, 190, 228), 1.f);
    Style.Disabled = FFBuildPearl::Rounded(FFBuildPearl::Color(240, 241, 247),
        14.f, FFBuildPearl::Line, 1.f);
    Style.NormalPadding = Style.PressedPadding = FMargin(10, 5);
    Control.SetStyle(Style);
    Control.SetColorAndOpacity(FLinearColor::White);
    Control.SetBackgroundColor(FLinearColor::White);
    if (auto* Text = Cast<UTextBlock>(Control.GetContent()))
    {
        Label(*Text, bPartner ? 14 : 13, FFBuildPearl::Ink, true);
        if (!bPartner)
        {
            Text->SetAutoWrapText(false);
            Text->SetJustification(ETextJustify::Center);
        }
        if (auto* Slot = Cast<UButtonSlot>(Text->Slot))
        {
            Slot->SetPadding(FMargin(0));
            Slot->SetHorizontalAlignment(HAlign_Fill);
            Slot->SetVerticalAlignment(VAlign_Center);
        }
    }
}

static UBorder* Wrap(UWidgetBlueprint& BP, UWidgetTree& Tree,
    UTextBlock& Text, const TCHAR* Name)
{
    if (auto* Existing = Find<UBorder>(Tree, Name))
        return Existing->GetContent() == &Text ? Existing : nullptr;
    UPanelWidget* Parent = Text.GetParent();
    if (!Parent) return nullptr;
    const int32 Index = Parent->GetChildIndex(&Text);
    if (Index == INDEX_NONE) return nullptr;
    UBorder* Border = MakeWidget<UBorder>(BP, Tree, Name);
    if (!Border || !Parent->ReplaceChildAt(Index, Border)) return nullptr;
    // Preserve the authored ordering and parent slot; retain the bound text object.
    Text.Slot = nullptr;
    Border->AddChild(&Text);
    Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    return Border;
}

static bool Save(UWidgetBlueprint& BP)
{
    BP.GetOutermost()->GetMetaData().SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    BP.GetOutermost()->GetMetaData().RemoveValue(&BP, Version);
    return false;
}

static bool Entry(UWidgetBlueprint& BP)
{
    if (!BP.WidgetTree || !BP.ParentClass ||
        !BP.ParentClass->IsChildOf(UFF_FacilitySynergyEntryWidget::StaticClass())) return false;
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Card = Find<UBorder>(Tree, TEXT("Border_Entry"));
    auto* Outer = Find<UBorder>(Tree, TEXT("Border_EntrySpacing"));
    auto* Partner = Find<UButton>(Tree, TEXT("Button_OpenPartnerCatalog"));
    auto* Title = Find<UTextBlock>(Tree, TEXT("Text_Title"));
    auto* PartnerLabel = Find<UTextBlock>(Tree, TEXT("Text_Partner"));
    auto* Effect = Find<UTextBlock>(Tree, TEXT("Text_Effect"));
    auto* Condition = Find<UTextBlock>(Tree, TEXT("Text_Condition"));
    auto* Status = Find<UTextBlock>(Tree, TEXT("Text_Status"));
    if (!Card || !Outer || !Partner || !Title || !PartnerLabel || !Effect ||
        !Condition || !Status || Partner->GetContent() != PartnerLabel) return false;
    BP.Modify();
    Tree.Modify();

    FSlateBrush Empty;
    Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
    Paint(*Outer, Empty, FMargin(0, 0, 0, 6));
    Outer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Paint(*Card, FFBuildPearl::Rounded(FFBuildPearl::Color(250, 250, 255, .97f),
        16.f, FFBuildPearl::Line, 1.f), FMargin(12, 8));
    Label(*Title, 16, FFBuildPearl::Ink, true);
    Spacing(*Title, FMargin(0, 0, 0, 6));
    Button(*Partner, true);
    Spacing(*Partner, FMargin(0, 0, 0, 8));
    Label(*Effect, 14, FFBuildPearl::Color(63, 101, 151), true);
    Label(*Condition, 12, FFBuildPearl::Muted);
    Label(*Status, 12, FFBuildPearl::Ink, true);

    auto* EffectCard = Wrap(BP, Tree, *Effect, TEXT("Pearl_SynergyEffect"));
    auto* StatusCard = Wrap(BP, Tree, *Status, TEXT("Pearl_SynergyStatus"));
    if (!EffectCard || !StatusCard) return false;
    Paint(*EffectCard, FFBuildPearl::Rounded(FFBuildPearl::Color(231, 243, 253), 10.f),
        FMargin(10, 6));
    Paint(*StatusCard, FFBuildPearl::Rounded(FFBuildPearl::Color(240, 240, 250), 10.f),
        FMargin(10, 5));
    Spacing(*EffectCard, FMargin(0, 0, 0, 6));
    Spacing(*Condition, FMargin(2, 0, 2, 6));
    Spacing(*StatusCard, FMargin(0));
    return Save(BP);
}

static bool CodexLauncher(UWidgetBlueprint& BP)
{
    if (!BP.WidgetTree || !BP.ParentClass ||
        !BP.ParentClass->IsChildOf(UFF_FacilityCodexLauncher::StaticClass())) return false;
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Bounds = Find<USizeBox>(Tree, TEXT("Size_Launcher"));
    auto* Open = Find<UButton>(Tree, TEXT("Button_OpenCodex"));
    if (!Bounds || !Open || !Cast<UTextBlock>(Open->GetContent())) return false;
    BP.Modify();
    Tree.Modify();
    Bounds->Modify();
    Bounds->SetHeightOverride(32);
    Button(*Open, false);
    return Save(BP);
}

static bool Inspector(UWidgetBlueprint& BP)
{
    if (!BP.WidgetTree || !BP.ParentClass ||
        !BP.ParentClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass())) return false;
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* SummaryCard = Find<UBorder>(Tree, TEXT("V2_SynergySummaryCard"));
    auto* Summary = Find<UTextBlock>(Tree, TEXT("Text_SynergySummary"));
    auto* Page = Find<UTextBlock>(Tree, TEXT("Text_SynergyPage"));
    auto* Previous = Find<UButton>(Tree, TEXT("Button_SynergyPrevious"));
    auto* Next = Find<UButton>(Tree, TEXT("Button_SynergyNext"));
    auto* Launcher = Find<UFF_FacilityCodexLauncher>(Tree, TEXT("Widget_FacilityCodexLauncher"));
    if (!SummaryCard || !Summary || !Page || !Previous || !Next || !Launcher) return false;
    BP.Modify();
    Tree.Modify();
    Paint(*SummaryCard, FFBuildPearl::Rounded(FFBuildPearl::Color(231, 243, 253, .96f),
        15.f, FFBuildPearl::Color(190, 222, 245, .8f), 1.f), FMargin(12, 8));
    Label(*Summary, 14, FFBuildPearl::Ink, true);
    // Keep the independent, working codex launcher while removing its old CTA-sized gap.
    Spacing(*SummaryCard, FMargin(0, 0, 0, 6));
    Spacing(*Launcher, FMargin(0, 0, 0, 6));
    Button(*Previous, false);
    Button(*Next, false);
    Label(*Page, 13, FFBuildPearl::Muted, true);
    Page->SetAutoWrapText(false);
    Page->SetJustification(ETextJustify::Center);
    if (auto* Slot = Cast<UHorizontalBoxSlot>(Page->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);
    return Save(BP);
}

static void Run()
{
    // Compile the nested launcher first so the inspector uses its saved compact button.
    UWidgetBlueprint* Launcher = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexLauncherPackage);
    const bool bLauncher = Launcher && CodexLauncher(*Launcher);
    UWidgetBlueprint* Recipe = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilitySynergyEntryPackage);
    UWidgetBlueprint* Panel = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    const bool bRecipe = Recipe && Entry(*Recipe);
    const bool bPanel = Panel && Inspector(*Panel);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s: recipe=%s inspector=%s launcher=%s"), Prefix,
        bRecipe && bPanel && bLauncher ? TEXT("PASS") : TEXT("FAIL"),
        bRecipe ? TEXT("PASS") : TEXT("FAIL"), bPanel ? TEXT("PASS") : TEXT("FAIL"),
        bLauncher ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilitySynergyPearl"),
    TEXT("Save the facility combination summary, recipe cards, codex launcher and pager in the shared pearl build theme."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}

#endif // WITH_EDITOR
