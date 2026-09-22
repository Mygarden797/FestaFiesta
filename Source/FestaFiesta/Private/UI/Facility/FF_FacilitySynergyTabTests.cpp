#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilitySynergyEntryWidget.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "Building/Synergy/FF_FacilitySynergyEvaluator.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Input/HittestGrid.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SVirtualWindow.h"

namespace
{
    struct FScopedSynergyTabPlayerless
    {
        explicit FScopedSynergyTabPlayerless(UWidgetBlueprintGeneratedClass& InClass)
            : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
        { Class.bCanCallInitializedWithoutPlayerContext = true; }
        ~FScopedSynergyTabPlayerless() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
        UWidgetBlueprintGeneratedClass& Class;
        bool bPrevious;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilitySynergyDisplayNamesTest,
    "FestaFiesta.UI.FacilityInfo.SynergyDisplayNames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilitySynergyDisplayNamesTest::RunTest(const FString& Parameters)
{
    // Recipe titles are independently authored FText; facility identity never comes from them.
    FFF_FacilitySynergyRule Rule;
    Rule.Id = TEXT("FixtureRecipeId");
    Rule.Row.TargetFacilityId = TEXT("FixtureRevenueFacilityId");
    Rule.Row.PartnerFacilityId = TEXT("FixtureSupportingFacilityId");
    Rule.Row.DisplayName = NSLOCTEXT("FacilitySynergyDisplayNamesTest", "Recipe", "여유로운 한 끼");
    const FFF_FacilitySynergyPoint Target { FGuid(0, 0, 0, 1), Rule.Row.TargetFacilityId, FVector::ZeroVector };
    const FFF_FacilitySynergyPoint Partner { FGuid(0, 0, 0, 2), Rule.Row.PartnerFacilityId, FVector(100, 0, 0) };
    FFF_FacilitySynergyEvaluator Evaluator;
    Evaluator.SetRules({Rule}, 50.f);
    Evaluator.SetFacilities({Target, Partner});
    FFF_FacilitySynergyPreview Preview;
    Evaluator.BuildPreview(Target.DefinitionId, Target.WorldLocation, Target.InstanceId, Preview);
    if (!TestEqual(TEXT("Production evaluator supplies the recipe"), Preview.Entries.Num(), 1)) return false;
    TestTrue(TEXT("Preview preserves authored FText identity instead of serializing the title"),
        Preview.Entries[0].DisplayName.IdenticalTo(Rule.Row.DisplayName));

    TMap<FName, FText> Names {
        { Rule.Row.TargetFacilityId, NSLOCTEXT("FacilitySynergyDisplayNamesTest", "Target", "햄버거 가게") },
        { Rule.Row.PartnerFacilityId, NSLOCTEXT("FacilitySynergyDisplayNamesTest", "Partner", "벤치") } };
    FFF_FacilityInfoViewData Data;
    const float Bonus = Evaluator.Evaluate(Target).TotalPercent;
    FFFacilityInfoModel::ApplySynergyPreview(&Preview, true, Bonus, Names, Data);
    if (!TestEqual(TEXT("Inspector maps the evaluated recipe"), Data.SynergyEntries.Num(), 1)) return false;
    TestTrue(TEXT("Custom recipe title preserves authored localized text identity"),
        Data.SynergyEntries[0].TitleText.IdenticalTo(Rule.Row.DisplayName));
    TestEqual(TEXT("Recipe identity remains the rule ID"), Data.SynergyEntries[0].RuleId, Rule.Id);
    TestEqual(TEXT("Partner navigation remains the definition ID"),
        Data.SynergyEntries[0].PartnerFacilityDefinitionId, Rule.Row.PartnerFacilityId);

    Names[Rule.Row.TargetFacilityId] = NSLOCTEXT("FacilitySynergyDisplayNamesTest", "RenamedTarget", "Grillküche");
    Names[Rule.Row.PartnerFacilityId] = NSLOCTEXT("FacilitySynergyDisplayNamesTest", "RenamedPartner", "Ruhebank");
    FFFacilityInfoModel::ApplySynergyPreview(&Preview, true, Bonus, Names, Data);
    TestTrue(TEXT("Facility rename or translation cannot overwrite an independent recipe title"),
        Data.SynergyEntries[0].TitleText.IdenticalTo(Rule.Row.DisplayName));
    TestTrue(TEXT("Partner label uses the current ID-resolved display name"),
        Data.SynergyEntries[0].PartnerText.ToString().Contains(Names[Rule.Row.PartnerFacilityId].ToString()));
    TestTrue(TEXT("Beneficiary effect uses the current ID-resolved display name"),
        Data.SynergyEntries[0].EffectText.ToString().Contains(Names[Rule.Row.TargetFacilityId].ToString()));
    TestEqual(TEXT("Renaming leaves the navigation ID unchanged"),
        Data.SynergyEntries[0].PartnerFacilityDefinitionId, Rule.Row.PartnerFacilityId);
    TestEqual(TEXT("Renaming leaves the paid bonus unchanged"), Evaluator.Evaluate(Target).TotalPercent, Bonus);

    // Incomplete runtime/transient rules still need safe text even though asset validation rejects empty titles.
    Rule.Row.DisplayName = FText::GetEmpty();
    Evaluator.SetRules({Rule}, 50.f);
    Evaluator.BuildPreview(Target.DefinitionId, Target.WorldLocation, Target.InstanceId, Preview);
    if (!TestEqual(TEXT("Untitled preview still describes its rule"), Preview.Entries.Num(), 1)) return false;
    TestTrue(TEXT("Evaluator does not turn an internal rule ID into a player-facing title"),
        Preview.Entries[0].DisplayName.IsEmpty());
    FFFacilityInfoModel::ApplySynergyPreview(&Preview, true, Bonus, Names, Data);
    const FText ExpectedFallback = FText::Format(
        NSLOCTEXT("FacilitySynergyDisplayNamesTest", "Fallback", "{0} + {1}"),
        Names[Rule.Row.TargetFacilityId], Names[Rule.Row.PartnerFacilityId]);
    TestEqual(TEXT("Untitled recipe uses the current localized ingredient names"),
        Data.SynergyEntries[0].TitleText.ToString(), ExpectedFallback.ToString());
    Names.Reset();
    FFFacilityInfoModel::ApplySynergyPreview(&Preview, true, Bonus, Names, Data);
    const FString UnknownTitle = Data.SynergyEntries[0].TitleText.ToString();
    TestTrue(TEXT("Missing title and catalog names remain readable without exposing IDs"),
        !UnknownTitle.IsEmpty() && !UnknownTitle.Contains(Rule.Id.ToString()) &&
        !UnknownTitle.Contains(Rule.Row.TargetFacilityId.ToString()) &&
        !UnknownTitle.Contains(Rule.Row.PartnerFacilityId.ToString()));
    TestEqual(TEXT("Missing display data still preserves partner navigation"),
        Data.SynergyEntries[0].PartnerFacilityDefinitionId, Rule.Row.PartnerFacilityId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilitySynergyTabTest,
    "FestaFiesta.UI.FacilityInfo.SynergyTab",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilitySynergyTabTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel.WBP_FF_FacilityInfoPanel_C")));
    auto* EntryClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilitySynergyEntryWidget>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilitySynergyEntry.WBP_FF_FacilitySynergyEntry_C")));
    const auto* Burger = LoadObject<UFFZSFacilityDefinition>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_FoodStall_Burger.PDA_FFZS_Booth_FoodStall_Burger"));
    const auto* Seating = LoadObject<UFFZSFacilityDefinition>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Rest/PDA_FFZS_Rest_Seating_Seating.PDA_FFZS_Rest_Seating_Seating"));
    const auto* Shade = LoadObject<UFFZSFacilityDefinition>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Rest/PDA_FFZS_Rest_Shade_Shade.PDA_FFZS_Rest_Shade_Shade"));
    const auto* SynergyTable = LoadObject<UDataTable>(nullptr,
        TEXT("/Game/FestaFiesta/Data/Economy/DT_FacilitySynergies.DT_FacilitySynergies"));
    if (!TestNotNull(TEXT("Editor fixture world"), World) || !TestNotNull(TEXT("Actual facility inspector WBP"), PanelClass) ||
        !TestNotNull(TEXT("Authored synergy entry WBP"), EntryClass) || !TestNotNull(TEXT("Catalog burger definition"), Burger) ||
        !TestNotNull(TEXT("Catalog seating definition"), Seating) || !TestNotNull(TEXT("Catalog shade definition"), Shade)) return false;
    const auto* BurgerRule = SynergyTable ? SynergyTable->FindRow<FFF_FacilitySynergyRow>(TEXT("BurgerPicnic"), TEXT("SynergyTab")) : nullptr;
    if (!TestNotNull(TEXT("Actual starter combination row"), BurgerRule)) return false;
    TestTrue(TEXT("Shipped recipe is enabled with a valid configuration and an authored title"),
        BurgerRule->bEnabled && BurgerRule->IsUsable() && !BurgerRule->DisplayName.IsEmpty());
    TestEqual(TEXT("Shipped recipe targets the actual burger definition ID"),
        BurgerRule->TargetFacilityId, Burger->Spec.FacilityDefinitionId);
    TestEqual(TEXT("Shipped recipe references the actual seating definition ID"),
        BurgerRule->PartnerFacilityId, Seating->Spec.FacilityDefinitionId);
    FScopedSynergyTabPlayerless PanelInitialization(*PanelClass);
    FScopedSynergyTabPlayerless EntryInitialization(*EntryClass);
    TStrongObjectPtr<UFF_FacilityInfoPanel> Panel(CreateWidget<UFF_FacilityInfoPanel>(World, PanelClass));
    if (!TestNotNull(TEXT("Inspector instance"), Panel.Get())) return false;
    const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
    auto* Overview = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabOverview")));
    auto* Synergy = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabSynergy")));
    auto* Operation = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabOperation")));
    auto* Upgrade = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabUpgrade")));
    UWidget* Section = Panel->GetWidgetFromName(TEXT("Panel_SynergySection"));
    auto* Entries = Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("VerticalBox_SynergyEntries")));
    auto* Summary = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("Text_SynergySummary")));
    auto* Scroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("ScrollBox_Details")));
    if (!TestNotNull(TEXT("Overview tab"), Overview) || !TestNotNull(TEXT("Synergy tab"), Synergy) ||
        !TestNotNull(TEXT("Operation tab"), Operation) || !TestNotNull(TEXT("Upgrade tab"), Upgrade) ||
        !TestNotNull(TEXT("Synergy section"), Section) || !TestNotNull(TEXT("Pooled row container"), Entries) ||
        !TestNotNull(TEXT("Synergy summary"), Summary) || !TestNotNull(TEXT("Existing shared detail scroll"), Scroll)) return false;
    UPanelWidget* TabRow = Overview->GetParent();
    if (!TestNotNull(TEXT("Authored tab row"), TabRow)) return false;
    TestTrue(TEXT("Tab order is overview, synergy, operation, upgrade"),
        Synergy->GetParent() == TabRow && Operation->GetParent() == TabRow && Upgrade->GetParent() == TabRow &&
        TabRow->GetChildIndex(Synergy) == TabRow->GetChildIndex(Overview) + 1 &&
        TabRow->GetChildIndex(Operation) == TabRow->GetChildIndex(Synergy) + 1 &&
        TabRow->GetChildIndex(Upgrade) == TabRow->GetChildIndex(Operation) + 1);

    FFF_FacilityInfoViewData Data;
    Data.FacilityInstanceId = FGuid::NewGuid();
    Data.FacilityDefinitionId = Burger->Spec.FacilityDefinitionId;
    Data.Context = EFF_FacilityInfoContext::Build;
    Data.ContextTitle = FText::FromString(TEXT("배치 시설"));
    Data.DisplayName = Burger->Spec.DisplayName;
    Data.CategoryText = FText::FromString(TEXT("음식 판매"));
    Data.FacilityStateText = FText::FromString(TEXT("운영 준비 완료"));
    Data.FacilityLevelText = FText::FromString(TEXT("Lv. 1"));
    Data.bHasFacilityLevels = true;
    Data.bCanMove = true;
    Data.bCanRemove = true;
    FFF_FacilitySynergyPreview Preview;
    Preview.bRulesAvailable = true;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        // One shipped title plus one independent presentation-only recipe title.
        // This fixture never changes the shipped balance table or any installed facility.
        auto& Entry = Preview.Entries.AddDefaulted_GetRef();
        Entry.RuleId = Index == 0 ? TEXT("BurgerPicnic") : TEXT("FixtureBurgerShade");
        Entry.DisplayName = Index == 0 ? BurgerRule->DisplayName
            : NSLOCTEXT("FacilitySynergyTabTest", "FixtureShadeRecipe", "그늘 아래 휴식");
        Entry.TargetFacilityId = Burger->Spec.FacilityDefinitionId;
        Entry.PartnerFacilityId = Index == 0 ? Seating->Spec.FacilityDefinitionId : Shade->Spec.FacilityDefinitionId;
        Entry.MaxDistanceMeters = 15.0f;
        Entry.DistanceMeters = Index == 0 ? 9.0f : 21.0f;
        Entry.ConfiguredBonusPercent = 10.0f;
        Entry.AppliedBonusPercent = Index == 0 ? 10.0f : 0.0f;
        Entry.CurrentTotalBonusPercent = Entry.ProposedTotalBonusPercent = 10.0f;
        Entry.bHasPartner = true;
        Entry.bInRange = Index == 0;
    }
    const TMap<FName, FText> Names = {
        { Burger->Spec.FacilityDefinitionId, Burger->Spec.DisplayName },
        { Seating->Spec.FacilityDefinitionId, Seating->Spec.DisplayName },
        { Shade->Spec.FacilityDefinitionId, Shade->Spec.DisplayName } };
    FFFacilityInfoModel::ApplySynergyPreview(&Preview, true, 10.0f, Names, Data);
    if (!TestEqual(TEXT("Read model produces two distinct rule cards"), Data.SynergyEntries.Num(), 2)) return false;
    TestTrue(TEXT("Shipped recipe title is preserved separately from current catalog facility names"),
        Data.SynergyEntries[0].TitleText.IdenticalTo(BurgerRule->DisplayName));
    TestTrue(TEXT("Displayed partner matches the actual build catalog name"),
        Data.SynergyEntries[0].PartnerText.ToString().Contains(Seating->Spec.DisplayName.ToString()));
    TestFalse(TEXT("Internal facility ID never leaks into the player-facing partner label"),
        Data.SynergyEntries[0].PartnerText.ToString().Contains(Seating->Spec.FacilityDefinitionId.ToString()));
    TestEqual(TEXT("Partner navigation uses the actual catalog definition ID"),
        Data.SynergyEntries[0].PartnerFacilityDefinitionId, Seating->Spec.FacilityDefinitionId);
    FFF_FacilitySynergyPreview SupportingPreview = Preview;
    SupportingPreview.Entries.SetNum(1);
    SupportingPreview.Entries[0].bCandidateIsTarget = false;
    FFF_FacilityInfoViewData SupportingData;
    FFFacilityInfoModel::ApplySynergyPreview(&SupportingPreview, true, 0.0f, Names, SupportingData);
    TestEqual(TEXT("Inspecting the supporting facility navigates to its income beneficiary"),
        SupportingData.SynergyEntries[0].PartnerFacilityDefinitionId, Burger->Spec.FacilityDefinitionId);
    Panel->ApplyViewData(Data);
    Panel->SetPanelOpen(true);
    TestEqual(TEXT("Overview does not eagerly create hidden combo rows"), Entries->GetChildrenCount(), 0);
    Synergy->OnClicked.Broadcast();
    TestTrue(TEXT("Actual button opens synergy content while retaining enabled contrast"), Section->GetVisibility() == ESlateVisibility::Visible && Synergy->GetIsEnabled());
    const bool bPaged = Panel->GetWidgetFromName(TEXT("Button_SynergyNext")) != nullptr;
    const int32 ExpectedPoolSize = bPaged ? 1 : 2;
    if (!TestEqual(TEXT("Rules use the authored visible-card pool"), Entries->GetChildrenCount(), ExpectedPoolSize)) return false;
    UWidget* FirstRow = Entries->GetChildAt(0);
    UWidget* SecondRow = bPaged ? nullptr : Entries->GetChildAt(1);
    auto* Row = Cast<UFF_FacilitySynergyEntryWidget>(FirstRow);
    if (!TestNotNull(TEXT("Row uses the authored child native contract"), Row)) return false;
    for (const FName Name : { TEXT("Text_Title"), TEXT("Text_Partner"), TEXT("Text_Effect"), TEXT("Text_Condition"), TEXT("Text_Status") })
    {
        if (!TestNotNull(*FString::Printf(TEXT("Authored row binding %s"), *Name.ToString()),
            Cast<UTextBlock>(Row->GetWidgetFromName(Name)))) return false;
    }
    auto* PartnerText = CastChecked<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Partner")));
    TestTrue(TEXT("Authored row presents the original localized recipe title"),
        CastChecked<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Title")))->GetText().IdenticalTo(BurgerRule->DisplayName));
    auto* PartnerButton = Cast<UButton>(Row->GetWidgetFromName(TEXT("Button_OpenPartnerCatalog")));
    if (!TestNotNull(TEXT("Authored partner navigation button"), PartnerButton)) return false;
    TestTrue(TEXT("Original partner label is the clickable content with a valid destination"),
        PartnerButton->GetContent() == PartnerText && PartnerButton->GetIsEnabled());
    auto CheckInputAncestors = [&](UWidget* Start)
    {
        for (UWidget* Ancestor = Start; Ancestor; Ancestor = Ancestor->GetParent())
            TestTrue(*FString::Printf(TEXT("Partner input is not blocked by %s"), *Ancestor->GetName()),
                Ancestor->GetVisibility() != ESlateVisibility::HitTestInvisible && Ancestor->IsVisible() && Ancestor->GetIsEnabled());
    };
    CheckInputAncestors(PartnerButton);
    CheckInputAncestors(Row);
    CheckInputAncestors(Panel.Get());
    const FSlateFontInfo AuthoredFont = PartnerText->GetFont();
    const FSlateColor AuthoredColor = PartnerText->GetColorAndOpacity();
    Panel->ApplyViewData(Data);
    TestTrue(TEXT("World refreshes reuse existing row objects and designer typography"),
        Entries->GetChildAt(0) == FirstRow && (bPaged || Entries->GetChildAt(1) == SecondRow) &&
        PartnerText->GetFont() == AuthoredFont && PartnerText->GetColorAndOpacity() == AuthoredColor);
    int32 PartnerRequests = 0;
    FName RequestedDefinition;
    const FDelegateHandle PartnerHandle = Row->OnPartnerCatalogRequested.AddLambda([&](const FName Id)
    {
        ++PartnerRequests;
        RequestedDefinition = Id;
    });
    // Exercise actual Slate button activation rather than broadcasting its delegate.
    // The editor-only fixture has no gameplay controller: its panel must show a clear
    // unavailable message after routing the same ID to BuildMode's guarded entry point.
    const TSharedRef<SWidget> PartnerSlate = PartnerButton->TakeWidget();
    const FGeometry InputGeometry = FGeometry::MakeRoot(FVector2D(280, 28), FSlateLayoutTransform());
    const FKeyEvent Activate(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0);
    PartnerSlate->OnKeyDown(InputGeometry, Activate);
    PartnerSlate->OnKeyUp(InputGeometry, Activate);
    TestTrue(TEXT("Slate activation dispatches one exact partner ID"),
        PartnerRequests == 1 && RequestedDefinition == Seating->Spec.FacilityDefinitionId);
    TestTrue(TEXT("Panel handles rejected catalog navigation without losing its selection"),
        Panel->GetCurrentViewData().bFeedbackIsError && !Panel->GetCurrentViewData().FeedbackText.IsEmpty() &&
        Panel->GetCurrentViewData().FacilityInstanceId == Data.FacilityInstanceId);
    FFF_FacilityInfoSynergyEntry NoDestination = Data.SynergyEntries[0];
    NoDestination.PartnerFacilityDefinitionId = NAME_None;
    Row->ApplyEntry(NoDestination);
    PartnerSlate->OnKeyDown(InputGeometry, Activate);
    PartnerSlate->OnKeyUp(InputGeometry, Activate);
    TestTrue(TEXT("Missing destination disables navigation even when the label is unchanged"),
        !PartnerButton->GetIsEnabled() && PartnerRequests == 1);
    Row->OnPartnerCatalogRequested.Remove(PartnerHandle);
    Panel->ApplyViewData(Data);
    if (bPaged)
    {
        auto* Next = CastChecked<UButton>(Panel->GetWidgetFromName(TEXT("Button_SynergyNext")));
        auto* Previous = CastChecked<UButton>(Panel->GetWidgetFromName(TEXT("Button_SynergyPrevious")));
        Next->OnClicked.Broadcast();
        TestTrue(TEXT("Next displays the second actual recipe in the same authored card"),
            Entries->GetChildAt(0) == FirstRow &&
            CastChecked<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Title")))->GetText().IdenticalTo(Data.SynergyEntries[1].TitleText));
        Previous->OnClicked.Broadcast();
        TestTrue(TEXT("Previous restores the first actual recipe"),
            CastChecked<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Title")))->GetText().IdenticalTo(Data.SynergyEntries[0].TitleText));
    }
    Operation->OnClicked.Broadcast();
    TestEqual(TEXT("Switching tabs hides the combo section"), Section->GetVisibility(), ESlateVisibility::Collapsed);
    Synergy->OnClicked.Broadcast();

    auto Capture = [&](const TCHAR* Name)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
        FWidgetRenderer Renderer(false, true);
        const FVector2D Size(1280.0, 720.0);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
        if (!TestNotNull(TEXT("Facility synergy sRGB target"), Target.Get())) return false;
        const TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(Size);
        Window->SetContent(PanelSlate);
        FHittestGrid HitTestGrid;
        Renderer.DrawWindow(Target.Get(), HitTestGrid, Window, 1.0f, Size, 0.0f, false);
        auto RecordSummaryGeometry = [&](const TCHAR* Frame)
        {
            const FSlateRect SummaryRect = Summary->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect ScrollRect = Scroll->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect SectionRect = Section->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FVector2D Desired = Summary->GetDesiredSize();
            AddInfo(FString::Printf(TEXT("%s %s: summary=(%.1f,%.1f..%.1f,%.1f), desired=(%.1f,%.1f), scroll=(%.1f,%.1f..%.1f,%.1f), sectionY=%.1f..%.1f"),
                Name, Frame, SummaryRect.Left, SummaryRect.Top, SummaryRect.Right, SummaryRect.Bottom, Desired.X, Desired.Y,
                ScrollRect.Left, ScrollRect.Top, ScrollRect.Right, ScrollRect.Bottom, SectionRect.Top, SectionRect.Bottom));
        };
        RecordSummaryGeometry(TEXT("first frame"));
        // Auto-wrapped Slate text learns its actual available width during arrangement.
        // A second real frame must settle to the same readable geometry as normal play.
        Panel->ForceLayoutPrepass();
        Renderer.DrawWindow(Target.Get(), HitTestGrid, Window, 1.0f, Size, 0.0f, false);
        RecordSummaryGeometry(TEXT("settled frame"));
        const FSlateRect SummaryRect = Summary->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        const FSlateRect ScrollRect = Scroll->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Complete synergy summary stays inside the visible scroll viewport"),
            SummaryRect.Top >= ScrollRect.Top && SummaryRect.Bottom <= ScrollRect.Bottom &&
            SummaryRect.Left >= ScrollRect.Left && SummaryRect.Right <= ScrollRect.Right &&
            SummaryRect.GetSize().Y + 0.5f >= Summary->GetDesiredSize().Y);
        TestTrue(TEXT("Synergy selection starts at the top of the existing scroll"), Scroll->GetScrollOffset() < 1.0f);
        if (!Data.SynergyEntries.IsEmpty())
        {
            const FSlateRect PartnerRect = PartnerText->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect ButtonRect = PartnerButton->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Partner link preserves the full readable text inside the button and scroll viewport"),
                PartnerRect.Top >= ScrollRect.Top && PartnerRect.Bottom <= ScrollRect.Bottom &&
                PartnerRect.Left >= ButtonRect.Left - 0.5f && PartnerRect.Right <= ButtonRect.Right + 0.5f &&
                PartnerRect.Top >= ButtonRect.Top - 0.5f && PartnerRect.Bottom <= ButtonRect.Bottom + 0.5f);
            const FVector2D LinkCenter((PartnerRect.Left + PartnerRect.Right) * 0.5f, (PartnerRect.Top + PartnerRect.Bottom) * 0.5f);
            const TArray<FWidgetAndPointer> HitPath = HitTestGrid.GetBubblePath(LinkCenter, 0.0f, false);
            TestTrue(TEXT("Rendered mouse hit path reaches the actual partner button through all panel ancestors"),
                HitPath.ContainsByPredicate([&](const FWidgetAndPointer& Hit) { return Hit.Widget == PartnerSlate; }));
        }
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags;
        Flags.SetLinearToGamma(false);
        if (!TestTrue(TEXT("Authored inspector renders a complete capture"),
            Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) && Pixels.Num() == 1280 * 720)) return false;
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(1280, 720, Pixels, Png);
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/FacilitySynergy"));
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Path = Directory / Name;
        AddInfo(FString::Printf(TEXT("Facility synergy tab PNG: %s"), *FPaths::ConvertRelativePathToFull(Path)));
        return TestTrue(TEXT("Facility synergy tab PNG saved"), FFileHelper::SaveArrayToFile(Png, *Path));
    };
    // Capture the shipped Burger rule alone; the second fixture row only exercises pooling.
    Data.SynergyEntries.SetNum(1);
    Data.SynergySummaryText = FText::FromString(TEXT("조합 1개 · 적용 중 1개\n이 시설의 이용 수수료 보너스 +10%"));
    Panel->ApplyViewData(Data);
    bool bSaved = Capture(TEXT("FacilitySynergyTab720.png"));
    const FFF_FacilitySynergyPreview EmptyPreview;
    FFFacilityInfoModel::ApplySynergyPreview(&EmptyPreview, true, 0.0f, Names, Data);
    Panel->ApplyViewData(Data);
    TestTrue(TEXT("Empty selection clears old cards while retaining the pool"),
        !Summary->GetText().IsEmpty() && Entries->GetChildrenCount() == ExpectedPoolSize &&
        FirstRow->GetVisibility() == ESlateVisibility::Collapsed && (!SecondRow || SecondRow->GetVisibility() == ESlateVisibility::Collapsed));
    bSaved &= Capture(TEXT("FacilitySynergyTabEmpty720.png"));
    Data.FacilityInstanceId = FGuid::NewGuid();
    Panel->ApplyViewData(Data);
    TestEqual(TEXT("Selecting another facility restores overview instead of stale combos"), Section->GetVisibility(), ESlateVisibility::Collapsed);
    return bSaved;
}

#endif
