// Editor-only, additive authoring migration for the facility's per-team quote.
#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "WidgetBlueprint.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace FFFacilityTeamRevenueBuilder
{
using namespace FFUIWbpBuilder;

static bool Apply(UWidgetBlueprint& Blueprint, const bool bSave)
{
    UWidgetTree* Tree = Blueprint.WidgetTree;
    if (!Tree)
    {
        UE_LOG(LogFFUIWbpBuilder, Warning, TEXT("FF_FACILITY_TEAM_REVENUE: WidgetTree is missing."));
        return false;
    }

    UWidget* RevenueWidget = Tree->FindWidget(TEXT("Text_FacilityRevenue"));
    // A designer may have detached the old optional binding from the root tree.
    // Reuse that object too, rather than allocating a second object with its name.
    if (!RevenueWidget) RevenueWidget = FindObjectFast<UWidget>(Tree, TEXT("Text_FacilityRevenue"));
    auto* Revenue = Cast<UCommonTextBlock>(RevenueWidget);
    UWidget* CardWidget = Tree->FindWidget(TEXT("TeamRevenue_OverviewCard"));
    auto* Card = Cast<UBorder>(CardWidget);
    if ((RevenueWidget && !Revenue) || (CardWidget && !Card))
    {
        UE_LOG(LogFFUIWbpBuilder, Warning, TEXT("FF_FACILITY_TEAM_REVENUE: Existing revenue binding or card has an incompatible widget class."));
        return false;
    }

    // The authored card is the migration marker. Repeated runs preserve Designer edits.
    if (Card && Revenue && Card->GetContent() == Revenue) return true;
    if (Card && Card->GetContent())
    {
        UE_LOG(LogFFUIWbpBuilder, Warning, TEXT("FF_FACILITY_TEAM_REVENUE: Revenue card contains a different authored widget; preserving its content."));
        return false;
    }

    auto* Basics = Cast<UVerticalBox>(Tree->FindWidget(TEXT("Clarity_BasicFacts")));
    auto* BuildSection = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_BuildSection")));
    auto* CardStyle = Cast<UBorder>(Tree->FindWidget(TEXT("Reference_QueuePill")));
    if (!CardStyle) CardStyle = Cast<UBorder>(Tree->FindWidget(TEXT("Clarity_RevenueCard")));
    auto* TextStyle = Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_InstallCost")));
    if (!Basics || !BuildSection || !CardStyle || !TextStyle || BuildSection->GetContent() != Basics)
    {
        UE_LOG(LogFFUIWbpBuilder, Warning, TEXT("FF_FACILITY_TEAM_REVENUE: Required layout missing or changed: Basics=%s BuildSection=%s CardStyle=%s TextStyle=%s BuildContent=%s"),
            *GetNameSafe(Basics), *GetNameSafe(BuildSection), *GetNameSafe(CardStyle), *GetNameSafe(TextStyle),
            BuildSection ? *GetNameSafe(BuildSection->GetContent()) : TEXT("None"));
        return false;
    }

    Blueprint.Modify();
    Tree->Modify();
    if (!Revenue)
    {
        // Some saved inspector revisions removed this optional native binding
        // entirely. Restore only that named text; never rebuild the authored tree.
        Revenue = Tree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("Text_FacilityRevenue"));
    }
    Revenue->Modify();
    Revenue->bIsVariable = true;
    if (!Blueprint.WidgetVariableNameToGuidMap.Contains(Revenue->GetFName()))
        Blueprint.OnVariableAdded(Revenue->GetFName());
    Basics->Modify();

    // Reuse an existing bound object when present; earlier compact revisions
    // archived it under Clarity_InfoV1. Keep the quote on the first page without reducing the
    // fixed space available to the upgrade and lighting pages.
    const bool bNewCard = !Card;
    if (bNewCard)
    {
        Card = MakeWidget<UBorder>(Blueprint, *Tree, TEXT("TeamRevenue_OverviewCard"));
        Card->SetBrush(CardStyle->Background);
        Card->SetBrushColor(CardStyle->GetBrushColor());
        Card->SetPadding(FMargin(10, 7));
        Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    Revenue->RemoveFromParent();
    FSlateFontInfo Font = TextStyle->GetFont();
    Font.Size = 14;
    Revenue->SetFont(Font);
    Revenue->SetColorAndOpacity(TextStyle->GetColorAndOpacity());
    Revenue->SetAutoWrapText(true);
    Revenue->SetWrapTextAt(0.f);
    Revenue->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    Revenue->SetLineHeightPercentage(1.f);
    Revenue->SetJustification(ETextJustify::Left);
    Revenue->SetVisibility(ESlateVisibility::Visible);
    Card->AddChild(Revenue);
    if (bNewCard)
    {
        auto* Slot = Basics->AddChildToVerticalBox(Card);
        Slot->SetPadding(FMargin(0, 0, 0, 8));
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Basics->ShiftChild(0, Card);
    }

    return !bSave || SaveNewBlueprint(TEXT("FF_FACILITY_TEAM_REVENUE"), Blueprint);
}

static void Run()
{
    UWidgetBlueprint* Blueprint = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    const bool bSuccess = Blueprint && Apply(*Blueprint, true);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_FACILITY_TEAM_REVENUE_%s"),
        bSuccess ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand Command(TEXT("FF.MigrateFacilityTeamRevenue"),
    TEXT("Restore the existing per-team revenue binding to the first facility info page, preserving all other authored controls."),
    FConsoleCommandDelegate::CreateStatic(&Run));

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityTeamRevenueMigrationTest,
    "FestaFiesta.UI.FacilityInfo.TeamRevenueMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityTeamRevenueMigrationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // A transient pre-migration fixture exercises the actual migration without
    // rewriting the project's current WBP or its designer-authored appearance.
    auto* Blueprint = NewObject<UWidgetBlueprint>(GetTransientPackage());
    auto* Tree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"));
    Blueprint->WidgetTree = Tree;
    auto* Shell = MakeWidget<UVerticalBox>(*Blueprint, *Tree, TEXT("Panel_Shell"));
    Tree->RootWidget = Shell;
    auto* Archive = MakeWidget<UVerticalBox>(*Blueprint, *Tree, TEXT("Clarity_InfoV1"));
    Shell->AddChildToVerticalBox(Archive);
    Archive->SetVisibility(ESlateVisibility::Collapsed);
    auto* Revenue = MakeWidget<UCommonTextBlock>(*Blueprint, *Tree, TEXT("Text_FacilityRevenue"), true);
    Archive->AddChildToVerticalBox(Revenue);
    auto* Build = MakeWidget<UBorder>(*Blueprint, *Tree, TEXT("Panel_BuildSection"));
    Shell->AddChildToVerticalBox(Build);
    auto* Basics = MakeWidget<UVerticalBox>(*Blueprint, *Tree, TEXT("Clarity_BasicFacts"));
    Build->AddChild(Basics);
    auto* Cost = MakeWidget<UCommonTextBlock>(*Blueprint, *Tree, TEXT("Text_InstallCost"), true);
    Basics->AddChildToVerticalBox(Cost);
    auto* Style = MakeWidget<UBorder>(*Blueprint, *Tree, TEXT("Reference_QueuePill"));
    Basics->AddChildToVerticalBox(Style);
    const FGuid BindingGuid = Blueprint->WidgetVariableNameToGuidMap.FindChecked(Revenue->GetFName());

    if (!TestTrue(TEXT("Migration accepts the compact inspector contract"), Apply(*Blueprint, false))) return false;
    auto* Card = Cast<UBorder>(Tree->FindWidget(TEXT("TeamRevenue_OverviewCard")));
    if (!TestNotNull(TEXT("Revenue card authored once"), Card)) return false;
    TestTrue(TEXT("Existing native binding is moved out of the collapsed archive"), Card->GetContent() == Revenue);
    TestTrue(TEXT("The quote is the first overview content"), Basics->GetChildAt(0) == Card);
    TestTrue(TEXT("Existing basic facts remain intact"), Cost->GetParent() == Basics && Build->GetContent() == Basics);
    TestEqual(TEXT("Binding GUID is preserved"),
        Blueprint->WidgetVariableNameToGuidMap.FindChecked(Revenue->GetFName()), BindingGuid);

    Card->SetPadding(FMargin(19, 13));
    FSlateFontInfo DesignerFont = Revenue->GetFont();
    DesignerFont.Size = 17;
    Revenue->SetFont(DesignerFont);
    const int32 ChildCount = Basics->GetChildrenCount();
    TestTrue(TEXT("Repeated migration succeeds"), Apply(*Blueprint, false));
    TestEqual(TEXT("Repeated migration creates no extra card"), Basics->GetChildrenCount(), ChildCount);
    TestTrue(TEXT("Repeated migration preserves Designer padding"), Card->GetPadding() == FMargin(19, 13));
    TestEqual(TEXT("Repeated migration preserves Designer text style"), Revenue->GetFont().Size, 17.f);

    for (const bool bDetachedBinding : {false, true})
    {
        auto* MissingBlueprint = NewObject<UWidgetBlueprint>(GetTransientPackage());
        auto* MissingTree = NewObject<UWidgetTree>(MissingBlueprint, TEXT("WidgetTree"));
        MissingBlueprint->WidgetTree = MissingTree;
        auto* MissingBuild = MakeWidget<UBorder>(*MissingBlueprint, *MissingTree, TEXT("Panel_BuildSection"));
        MissingTree->RootWidget = MissingBuild;
        auto* MissingBasics = MakeWidget<UVerticalBox>(*MissingBlueprint, *MissingTree, TEXT("Clarity_BasicFacts"));
        MissingBuild->AddChild(MissingBasics);
        auto* MissingCost = MakeWidget<UCommonTextBlock>(*MissingBlueprint, *MissingTree, TEXT("Text_InstallCost"), true);
        MissingBasics->AddChildToVerticalBox(MissingCost);
        auto* MissingStyle = MakeWidget<UBorder>(*MissingBlueprint, *MissingTree, TEXT("Reference_QueuePill"));
        MissingBasics->AddChildToVerticalBox(MissingStyle);
        UCommonTextBlock* DetachedBinding = nullptr;
        FGuid DetachedGuid;
        if (bDetachedBinding)
        {
            DetachedBinding = MakeWidget<UCommonTextBlock>(*MissingBlueprint, *MissingTree, TEXT("Text_FacilityRevenue"), true);
            DetachedGuid = MissingBlueprint->WidgetVariableNameToGuidMap.FindChecked(DetachedBinding->GetFName());
        }
        if (!TestTrue(TEXT("Migration restores absent or detached optional bindings"), Apply(*MissingBlueprint, false))) return false;
        auto* Restored = Cast<UCommonTextBlock>(MissingTree->FindWidget(TEXT("Text_FacilityRevenue")));
        auto* RestoredCard = Cast<UBorder>(MissingTree->FindWidget(TEXT("TeamRevenue_OverviewCard")));
        if (!TestNotNull(TEXT("Revenue binding is reachable from the saved root tree"), Restored) ||
            !TestNotNull(TEXT("Restored binding has an overview card"), RestoredCard)) return false;
        TestTrue(TEXT("Restored text remains a named native binding"), Restored->bIsVariable &&
            MissingBlueprint->WidgetVariableNameToGuidMap.Contains(Restored->GetFName()));
        TestTrue(TEXT("Restoring missing revenue preserves existing basic controls"),
            MissingBuild->GetContent() == MissingBasics && MissingCost->GetParent() == MissingBasics &&
            RestoredCard->GetContent() == Restored);
        if (DetachedBinding)
        {
            TestTrue(TEXT("Detached binding is reused without name suffixes"), Restored == DetachedBinding);
            TestEqual(TEXT("Detached binding retains its original Designer GUID"),
                MissingBlueprint->WidgetVariableNameToGuidMap.FindChecked(Restored->GetFName()), DetachedGuid);
        }
        TestTrue(TEXT("Restored binding migration remains idempotent"), Apply(*MissingBlueprint, false));
        TestEqual(TEXT("Only one revenue card is added"), MissingBasics->GetChildrenCount(), 3);
    }
    return true;
}
#endif
}
#endif
