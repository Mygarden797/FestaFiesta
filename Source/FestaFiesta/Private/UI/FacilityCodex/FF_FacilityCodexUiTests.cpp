#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexImage.h"
#include "UI/FacilityCodex/FF_FacilityCodexPreview.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "Building/Codex/FF_FacilityCodexModel.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/StackBox.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Editor.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "StaticMeshCompiler.h"
#include "TextureCompiler.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"

namespace
{
    struct FScopedCodexPlayerless
    {
        explicit FScopedCodexPlayerless(UWidgetBlueprintGeneratedClass& InClass)
            : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
        { Class.bCanCallInitializedWithoutPlayerContext = true; }
        ~FScopedCodexPlayerless() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
        UWidgetBlueprintGeneratedClass& Class;
        bool bPrevious;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexUiTest,
    "FestaFiesta.FacilityCodex.UI.AuthoredWidgets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexUiTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto LoadClass = [](const TCHAR* Name) {
        return Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(FString::Printf(
            TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodex%s.WBP_FF_FacilityCodex%s_C"), Name, Name)).TryLoadClass<UUserWidget>());
    };
    auto* ScreenClass = LoadClass(TEXT("Screen"));
    auto* CardClass = LoadClass(TEXT("Card"));
    auto* LauncherClass = LoadClass(TEXT("Launcher"));
    auto* InspectorClass = Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(TEXT(
        "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel.WBP_FF_FacilityInfoPanel_C")).TryLoadClass<UUserWidget>());
    if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Screen WBP"), ScreenClass) ||
        !TestNotNull(TEXT("Card WBP"), CardClass) || !TestNotNull(TEXT("Launcher WBP"), LauncherClass) ||
        !TestNotNull(TEXT("Existing inspector WBP"), InspectorClass)) return false;
    FScopedCodexPlayerless ScreenContext(*ScreenClass), CardContext(*CardClass), LauncherContext(*LauncherClass), InspectorContext(*InspectorClass);
    TStrongObjectPtr<UFF_FacilityCodexScreen> Screen(CreateWidget<UFF_FacilityCodexScreen>(World, ScreenClass));
    TStrongObjectPtr<UFF_FacilityInfoPanel> Inspector(CreateWidget<UFF_FacilityInfoPanel>(World, InspectorClass));
    if (!TestNotNull(TEXT("Screen instance"), Screen.Get()) || !TestNotNull(TEXT("Inspector instance"), Inspector.Get())) return false;
    const TSharedRef<SWidget> Slate = Screen->TakeWidget();
    auto* Launcher = Cast<UFF_FacilityCodexLauncher>(Inspector->GetWidgetFromName(TEXT("Widget_FacilityCodexLauncher")));
    TestTrue(TEXT("Inspector's original synergy section contains authored codex launcher"), Launcher &&
        Launcher->GetParent() == Inspector->GetWidgetFromName(TEXT("Panel_SynergySection")));
    auto* Tile = Cast<UTileView>(Screen->GetWidgetFromName(TEXT("TileView_Entries")));
    auto* Search = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("Search_Entries")));
    auto* Close = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_Close")));
    auto* Detail = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailTitle")));
    auto* Discovery = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_Discovery")));
    if (!TestNotNull(TEXT("Virtualized tile view"), Tile) || !TestNotNull(TEXT("Search input"), Search) ||
        !TestNotNull(TEXT("Close button"), Close) || !TestNotNull(TEXT("Detail heading"), Detail) ||
        !TestNotNull(TEXT("Discovery reward detail"), Discovery)) return false;
    for (const FName Name : { TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"), TEXT("Button_Active"), TEXT("Button_ClearFacility") })
        if (!TestNotNull(*Name.ToString(), Cast<UButton>(Screen->GetWidgetFromName(Name)))) return false;

    auto Definition = [](const TCHAR* Suffix) {
        return LoadObject<UFFZSFacilityDefinition>(nullptr, *FString::Printf(TEXT(
            "/Game/FestaFiesta/Building/FF_Data_Building/%s"), Suffix));
    };
    const UFFZSFacilityDefinition* Burger = Definition(TEXT("Booth/PDA_FFZS_Booth_FoodStall_Burger.PDA_FFZS_Booth_FoodStall_Burger"));
    const UFFZSFacilityDefinition* Seating = Definition(TEXT("Rest/PDA_FFZS_Rest_Seating_Seating.PDA_FFZS_Rest_Seating_Seating"));
    const UFFZSFacilityDefinition* IceCream = Definition(TEXT("Booth/PDA_FFZS_Booth_FoodStall_IceCream.PDA_FFZS_Booth_FoodStall_IceCream"));
    const UFFZSFacilityDefinition* Shade = Definition(TEXT("Rest/PDA_FFZS_Rest_Shade_Shade.PDA_FFZS_Rest_Shade_Shade"));
    if (!TestNotNull(TEXT("Burger definition"), Burger) || !TestNotNull(TEXT("Seating definition"), Seating) ||
        !TestNotNull(TEXT("IceCream definition"), IceCream) || !TestNotNull(TEXT("Shade definition"), Shade)) return false;
    TArray<FFF_FacilityCodexEntry> Entries;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const UFFZSFacilityDefinition* Target = Index == 0 ? Burger : IceCream;
        const UFFZSFacilityDefinition* Partner = Index == 0 ? Seating : Shade;
        auto& Entry = Entries.AddDefaulted_GetRef();
        Entry.RuleId = Index == 0 ? TEXT("BurgerPicnic") : TEXT("IceCreamShade");
        Entry.TargetFacilityId = Target->Spec.FacilityDefinitionId;
        Entry.PartnerFacilityId = Partner->Spec.FacilityDefinitionId;
        Entry.TargetName = Target->Spec.DisplayName;
        Entry.PartnerName = Partner->Spec.DisplayName;
        Entry.TargetCategory = Target->Spec.Category;
        Entry.DisplayName = FText::Format(FText::FromString(TEXT("{0} + {1}")), Entry.TargetName, Entry.PartnerName);
        Entry.TargetThumbnail = Target->CatalogThumbnail;
        Entry.PartnerThumbnail = Partner->CatalogThumbnail;
        Entry.TargetThumbnail.LoadSynchronous();
        Entry.PartnerThumbnail.LoadSynchronous();
        Entry.MaxDistanceMeters = 15.f;
        Entry.ConfiguredBonusPercent = 10.f;
        Entry.DiscoveryRewardGold = Index == 0 ? 450 : 1250;
        Entry.bDiscovered = Index == 0;
        Entry.bConditionMet = Index == 0;
        Entry.bEffectApplied = Index == 0;
        Entry.bTargetInstalled = true;
        Entry.bPartnerInstalled = Index == 0;
        Entry.bHasInstalledPair = Index == 0;
        Entry.AppliedBonusPercent = Index == 0 ? 10.f : 0.f;
        Entry.ConditionMetCount = Entry.EffectAppliedCount = Index == 0 ? 1 : 0;
        Entry.ClosestDistanceMeters = Index == 0 ? 9.f : -1.f;
    }
    Screen->SetPreviewEntries(Entries);
    TestEqual(TEXT("All recipes visible"), Screen->GetVisibleEntryCount(), 2);
    auto* CollectionProgress = Cast<UProgressBar>(Screen->GetWidgetFromName(TEXT("Progress_Discovery")));
    if (!TestNotNull(TEXT("Festival guide has an authored discovery progress bar"), CollectionProgress)) return false;
    TestTrue(TEXT("Collection progress counts discovered recipes"), FMath::IsNearlyEqual(CollectionProgress->GetPercent(), .5f));
    auto* TargetCatalog = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_TargetCatalog")));
    auto* PartnerCatalog = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_PartnerCatalog")));
    if (!TestNotNull(TEXT("Target facility catalog link"), TargetCatalog) ||
        !TestNotNull(TEXT("Partner facility catalog link"), PartnerCatalog)) return false;
    auto* BrowseBounds = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_Browse")));
    auto* ShowcaseImage = Cast<UFF_FacilityCodexImage>(Screen->GetWidgetFromName(TEXT("Image_Showcase")));
    auto* ShowcaseViewport = Cast<UFF_FacilityCodexPreview>(Screen->GetWidgetFromName(TEXT("Viewport_Showcase")));
    auto* PreviewName = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_PreviewFacilityName")));
    auto* PreviewTarget = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_PreviewTarget")));
    auto* PreviewPartner = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_PreviewPartner")));
    auto* DetailReward = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_DetailClaimReward")));
    auto* DetailClaimed = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Border_DetailRewardClaimed")));
    auto* DetailLocked = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Border_DetailRewardLocked")));
    auto* DetailRewardBounds = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_DetailRewardState")));
    auto* RewardReady = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_RewardReady")));
    auto* CategoryCombo = Cast<UComboBoxString>(Screen->GetWidgetFromName(TEXT("Combo_Category")));
    if (!TestNotNull(TEXT("Showcase collection browser"), BrowseBounds) ||
        !TestNotNull(TEXT("Showcase fallback image"), ShowcaseImage) ||
        !TestNotNull(TEXT("Isolated showcase viewport"), ShowcaseViewport) ||
        !TestNotNull(TEXT("Showcase facility name"), PreviewName) ||
        !TestNotNull(TEXT("Showcase target toggle"), PreviewTarget) ||
        !TestNotNull(TEXT("Showcase partner toggle"), PreviewPartner) ||
        !TestNotNull(TEXT("Selected recipe reward action"), DetailReward) ||
        !TestNotNull(TEXT("Selected recipe completion badge"), DetailClaimed) ||
        !TestNotNull(TEXT("Selected recipe locked reward"), DetailLocked) ||
        !TestNotNull(TEXT("Selected recipe reward bounds"), DetailRewardBounds) ||
        !TestNotNull(TEXT("Reward-ready filter"), RewardReady) ||
        !TestNotNull(TEXT("Beneficiary category filter"), CategoryCombo)) return false;
    TestTrue(TEXT("The selected recipe's reward keeps the established 72 by 32 bounds"),
        FMath::IsNearlyEqual(DetailRewardBounds->GetWidthOverride(), 72.f) &&
        FMath::IsNearlyEqual(DetailRewardBounds->GetHeightOverride(), 32.f));

    // Exercise the actual screen controls against a mixed collection. An installed
    // supporting facility's category must not masquerade as the beneficiary category.
    TArray<FFF_FacilityCodexEntry> FilterEntries = Entries;
    auto& ClaimedFilterEntry = FilterEntries.Add_GetRef(Entries[0]);
    ClaimedFilterEntry.RuleId = TEXT("Filter_Claimed");
    ClaimedFilterEntry.bDiscoveryRewardClaimed = true;
    auto& ZeroFilterEntry = FilterEntries.Add_GetRef(Entries[0]);
    ZeroFilterEntry.RuleId = TEXT("Filter_ZeroReward");
    ZeroFilterEntry.DiscoveryRewardGold = 0;
    auto& RestFilterEntry = FilterEntries.Add_GetRef(Entries[0]);
    RestFilterEntry.RuleId = TEXT("Filter_RestBeneficiary");
    RestFilterEntry.TargetFacilityId = Entries[0].PartnerFacilityId;
    RestFilterEntry.PartnerFacilityId = Entries[0].TargetFacilityId;
    RestFilterEntry.TargetName = Entries[0].PartnerName;
    RestFilterEntry.PartnerName = Entries[0].TargetName;
    RestFilterEntry.TargetThumbnail = Entries[0].PartnerThumbnail;
    RestFilterEntry.PartnerThumbnail = Entries[0].TargetThumbnail;
    RestFilterEntry.TargetCategory = Seating->Spec.Category;
    Screen->SetPreviewEntries(FilterEntries);
    Tile->SetSelectedItem(Tile->GetListItems()[0]);
    RewardReady->OnClicked.Broadcast();
    TestEqual(TEXT("Reward-ready filter excludes undiscovered, claimed, and zero rewards"), Screen->GetVisibleEntryCount(), 2);
    TestEqual(TEXT("Reward-ready filtering preserves an eligible selection"), Screen->GetSelectedRuleId(), Entries[0].RuleId);
    for (UObject* Object : Tile->GetListItems())
    {
        const auto& Entry = CastChecked<UFF_FacilityCodexItem>(Object)->Entry;
        TestTrue(TEXT("Every reward-ready result is actually claimable"),
            Entry.bDiscovered && !Entry.bDiscoveryRewardClaimed && Entry.DiscoveryRewardGold > 0);
    }
    Screen->SetCategoryFilter(true, Seating->Spec.Category);
    TestEqual(TEXT("Category filtering applies to the beneficiary, not a supporting facility"), Screen->GetVisibleEntryCount(), 1);
    TestEqual(TEXT("Category filtering selects the matching beneficiary recipe"),
        Screen->GetSelectedRuleId(), FName(TEXT("Filter_RestBeneficiary")));
    Screen->SetCategoryFilter(false, EFFZSFacilityCategory::Booth);
    TestEqual(TEXT("Clearing category filtering retains the reward filter"), Screen->GetVisibleEntryCount(), 2);
    TestEqual(TEXT("Clearing a category retains its still-visible selection"),
        Screen->GetSelectedRuleId(), FName(TEXT("Filter_RestBeneficiary")));
    Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::All);
    TestEqual(TEXT("All recipes return after both filter dimensions reset"), Screen->GetVisibleEntryCount(), FilterEntries.Num());
    Screen->SetPreviewEntries(Entries);
    Tile->SetSelectedItem(Tile->GetListItems()[0]);
    TestTrue(TEXT("Discovered target is named in the showcase"), PreviewName->GetText().EqualTo(Entries[0].TargetName));
    TestEqual(TEXT("Discovered target fallback remains full color"),
        ShowcaseImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Color);
    if (!Burger->Spec.VisualMesh.IsNull())
        TestTrue(TEXT("A discovered production target may request only its authored display mesh"),
            ShowcaseViewport->GetRequestedMesh() == Burger->Spec.VisualMesh);
    PreviewPartner->OnClicked.Broadcast();
    TestTrue(TEXT("Partner preview switches the visible facility name"), PreviewName->GetText().EqualTo(Entries[0].PartnerName));
    TestEqual(TEXT("Discovered partner fallback remains full color"),
        ShowcaseImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Color);
    PreviewTarget->OnClicked.Broadcast();
    TestTrue(TEXT("Target preview restores its facility name"), PreviewName->GetText().EqualTo(Entries[0].TargetName));
    TestTrue(TEXT("Selected discovered recipe exposes only its claim action"),
        DetailReward->IsVisible() && DetailReward->GetIsEnabled() && !DetailClaimed->IsVisible() && !DetailLocked->IsVisible());
    DetailReward->OnClicked.Broadcast();
    DetailReward->OnClicked.Broadcast();
    TestFalse(TEXT("Preview-only detail reward clicks cannot mark a reward paid"),
        CastChecked<UFF_FacilityCodexItem>(Tile->GetSelectedItem())->Entry.bDiscoveryRewardClaimed);
    FName RequestedFacility;
    Screen->OnFacilityCatalogRequested.AddLambda([&](FName Id) { RequestedFacility = Id; });
    TargetCatalog->OnClicked.Broadcast();
    TestEqual(TEXT("Target button requests the selected target definition"), RequestedFacility, Entries[0].TargetFacilityId);
    PartnerCatalog->OnClicked.Broadcast();
    TestEqual(TEXT("Partner button requests the selected partner definition"), RequestedFacility, Entries[0].PartnerFacilityId);
    TestTrue(TEXT("Already-discovered recipe shows its configured first-discovery reward without claiming payment"),
        Discovery->GetText().ToString().Contains(FText::AsNumber(450).ToString()) &&
        Discovery->GetText().ToString().Contains(TEXT("발견 완료")) &&
        Discovery->GetText().ToString().Contains(TEXT("최초 발견 보상")) &&
        Discovery->GetText().ToString().Contains(TEXT("1회")) &&
        !Discovery->GetText().ToString().Contains(TEXT("지급 완료")));
    Screen->SetFacilityFilter(Seating->Spec.FacilityDefinitionId);
    TestEqual(TEXT("Supporting facility matches its recipe"), Screen->GetVisibleEntryCount(), 1);
    TestTrue(TEXT("Filtering does not turn the global collection progress into a completed subset"),
        FMath::IsNearlyEqual(CollectionProgress->GetPercent(), .5f));
    Screen->SetFacilityFilter(NAME_None);
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_Active")))->OnClicked.Broadcast();
    TestEqual(TEXT("Applied filter shows only recipes with a live nonzero effect"), Screen->GetVisibleEntryCount(), 1);
    TestEqual(TEXT("Applied filter selects the actual applied recipe"), Screen->GetSelectedRuleId(), FName(TEXT("BurgerPicnic")));
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_Undiscovered")))->OnClicked.Broadcast();
    TestEqual(TEXT("Actual undiscovered button filters"), Screen->GetVisibleEntryCount(), 1);
    TestEqual(TEXT("Detail follows filtered selection"), Screen->GetSelectedRuleId(), FName(TEXT("IceCreamShade")));
    TestTrue(TEXT("A newly selected unknown recipe previews its beneficiary"), PreviewName->GetText().EqualTo(Entries[1].TargetName));
    TestEqual(TEXT("An installed unknown showcase target uses grayscale"),
        ShowcaseImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Grayscale);
    TestTrue(TEXT("Unknown recipes never request or reveal the unrestricted 3D mesh"),
        ShowcaseViewport->GetRequestedMesh().IsNull() && !ShowcaseViewport->IsVisible());
    PreviewPartner->OnClicked.Broadcast();
    TestTrue(TEXT("Unknown partner preview switches its facility name"), PreviewName->GetText().EqualTo(Entries[1].PartnerName));
    TestEqual(TEXT("An uninstalled showcase partner remains a silhouette"),
        ShowcaseImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Silhouette);
    PreviewTarget->OnClicked.Broadcast();
    TestEqual(TEXT("Switching back restores the target's grayscale state"),
        ShowcaseImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Grayscale);
    TestTrue(TEXT("Undiscovered selected rewards show one passive locked surface"),
        DetailLocked->IsVisible() && DetailLocked->GetIsEnabled() && !DetailReward->IsVisible() &&
        !DetailReward->GetIsEnabled() && !DetailClaimed->IsVisible());
    DetailReward->OnClicked.Broadcast();
    TestFalse(TEXT("A synthetic selected-detail claim cannot pay an undiscovered recipe"),
        CastChecked<UFF_FacilityCodexItem>(Tile->GetSelectedItem())->Entry.bDiscoveryRewardClaimed);
    TargetCatalog->OnClicked.Broadcast();
    TestEqual(TEXT("Undiscovered detail links follow selection instead of retaining the earlier facility"), RequestedFacility, Entries[1].TargetFacilityId);
    TestTrue(TEXT("Undiscovered recipe shows its own configured reward and one-time restriction"),
        Discovery->GetText().ToString().Contains(FText::AsNumber(1250).ToString()) &&
        Discovery->GetText().ToString().Contains(TEXT("미발견")) &&
        Discovery->GetText().ToString().Contains(TEXT("최초 발견 보상")) &&
        Discovery->GetText().ToString().Contains(TEXT("1회")));
    Entries[1].DiscoveryRewardGold = 0;
    Screen->SetPreviewEntries(Entries);
    TestTrue(TEXT("A selected zero reward hides all reward surfaces"),
        !DetailRewardBounds->IsVisible() && !DetailReward->IsVisible() &&
        !DetailClaimed->IsVisible() && !DetailLocked->IsVisible());
    DetailReward->OnClicked.Broadcast();
    TestFalse(TEXT("A synthetic selected-detail zero-reward claim remains unpaid"),
        CastChecked<UFF_FacilityCodexItem>(Tile->GetSelectedItem())->Entry.bDiscoveryRewardClaimed);
    TestTrue(TEXT("Refreshing a selected zero-reward recipe does not retain the earlier amount"),
        Discovery->GetText().ToString().Contains(TEXT("0 골드")) &&
        !Discovery->GetText().ToString().Contains(FText::AsNumber(1250).ToString()));
    Entries[1].DiscoveryRewardGold = 1250;
    Screen->SetPreviewEntries(Entries);
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_All")))->OnClicked.Broadcast();
    Search->SetText(FText::FromString(TEXT("no_matching_recipe")));
    Search->OnTextChanged.Broadcast(Search->GetText());
    auto* SearchHint = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_SearchHint")));
    if (TestNotNull(TEXT("White search has a readable passive hint"), SearchHint))
        TestEqual(TEXT("Entering a query hides the hint"), SearchHint->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Search input filters recipes"), Screen->GetVisibleEntryCount(), 0);
    TestTrue(TEXT("Empty results clear detail selection"), Screen->GetSelectedRuleId().IsNone());
    TestTrue(TEXT("Empty-state copy is visible"), Screen->GetWidgetFromName(TEXT("Text_Empty"))->IsVisible());
    Screen->SetSearchText(FText::GetEmpty());
    TestEqual(TEXT("Programmatic search refreshes results"), Screen->GetVisibleEntryCount(), 2);
    TestTrue(TEXT("Programmatic search updates input text"), Search->GetText().IsEmpty());
    if (SearchHint) TestEqual(TEXT("Clearing search restores its hint without blocking input"),
        SearchHint->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Tile->SetSelectedItem(Tile->GetListItems()[0]);
    TestEqual(TEXT("Tile selection drives recipe detail"), Screen->GetSelectedRuleId(), FName(TEXT("BurgerPicnic")));
    TArray<FFF_FacilityCodexEntry> ClaimedDetailEntries = Entries;
    ClaimedDetailEntries[0].bDiscoveryRewardClaimed = true;
    Screen->SetPreviewEntries(ClaimedDetailEntries);
    TestTrue(TEXT("A selected claimed reward replaces its action with a full-contrast passive badge"),
        DetailClaimed->IsVisible() && DetailClaimed->GetIsEnabled() && !DetailReward->IsVisible() &&
        !DetailReward->GetIsEnabled() && !DetailLocked->IsVisible());
    const FName ClaimedSelection = Screen->GetSelectedRuleId();
    DetailReward->OnClicked.Broadcast();
    TestTrue(TEXT("A synthetic claimed-detail click leaves its receipt and selection intact"),
        CastChecked<UFF_FacilityCodexItem>(Tile->GetSelectedItem())->Entry.bDiscoveryRewardClaimed &&
        Screen->GetSelectedRuleId() == ClaimedSelection);
    ClaimedDetailEntries[0].DiscoveryRewardGold = 0;
    Screen->SetPreviewEntries(ClaimedDetailEntries);
    TestTrue(TEXT("Selected detail keeps claim history after a reward amount changes to zero"),
        DetailClaimed->IsVisible() && DetailRewardBounds->IsVisible());
    Screen->SetPreviewEntries(Entries);
    TestTrue(TEXT("Rebinding the selected detail restores only its current reward action"),
        DetailReward->IsVisible() && DetailReward->GetIsEnabled() && !DetailClaimed->IsVisible() &&
        !DetailLocked->IsVisible());
    auto CheckSelectedTab = [&](const TCHAR* Name)
    {
        auto* Button = CastChecked<UButton>(Screen->GetWidgetFromName(FName(Name)));
        TestTrue(TEXT("Selected codex tab stays enabled so Slate does not gray out its contents"), Button->GetIsEnabled());
        TestTrue(TEXT("Selected codex tab uses its saved selected brush"), Button->GetStyle().Normal == Button->GetStyle().Disabled);
        const FLinearColor SelectedInk = CastChecked<UTextBlock>(Button->GetContent())->GetColorAndOpacity().GetSpecifiedColor();
        TestTrue(TEXT("Selected codex label keeps bright opaque contrast on the dark glass surface"),
            SelectedInk.GetLuminance() > .6f && SelectedInk.A > .95f);
    };
    CheckSelectedTab(TEXT("Button_All"));
    const FName CurrentRule = Screen->GetSelectedRuleId();
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_All")))->OnClicked.Broadcast();
    TestEqual(TEXT("Reactivating the selected filter preserves its current recipe"),Screen->GetSelectedRuleId(),CurrentRule);
    const FSlateFontInfo AuthoredFont = Detail->GetFont();
    const float AuthoredEntryWidth = Tile->GetEntryWidth();
    const float AuthoredEntryHeight = Tile->GetEntryHeight();
    auto* Main = Cast<UStackBox>(Screen->GetWidgetFromName(TEXT("Stack_Main")));
    auto* DetailBounds = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_Detail")));
    if (!TestNotNull(TEXT("Migrated responsive main panel"), Main) ||
        !TestNotNull(TEXT("Authored detail bounds"), DetailBounds)) return false;
    auto* Dialog = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_Dialog")));
    auto* DetailPages = Cast<UWidgetSwitcher>(Screen->GetWidgetFromName(TEXT("Switcher_Detail")));
    if (!TestNotNull(TEXT("Full viewport codex bounds"), Dialog) ||
        !TestNotNull(TEXT("Direct detail tabs"), DetailPages)) return false;
    TestTrue(TEXT("Detail typography uses the supplied project fonts"),
        AuthoredFont.FontObject && AuthoredFont.FontObject->GetPathName().StartsWith(TEXT("/Game/Asset/Font/")));
    int32 BlurLayers = 0;
    Screen->WidgetTree->ForEachWidget([&](UWidget* Widget) {
        if (const auto* Blur = Cast<UBackgroundBlur>(Widget))
        {
            ++BlurLayers;
            TestTrue(TEXT("The glass backdrop has actual blur and a non-opaque low-quality fallback"),
                Blur->GetBlurStrength() > 0.f && Blur->GetLowQualityFallbackBrush().TintColor.GetSpecifiedColor().A < 1.f);
        }
    });
    TestEqual(TEXT("Fullscreen glass uses one shared blur rather than a blur per virtualized card"), BlurLayers, 1);
    for (const FName Name : {TEXT("Button_DetailRecipe"), TEXT("Button_DetailPlacement"), TEXT("Button_DetailEffect")})
        if (!TestNotNull(TEXT("Direct detail navigation button"), Cast<UButton>(Screen->GetWidgetFromName(Name)))) return false;
    Screen->SetPreviewEntries(Entries);
    TestTrue(TEXT("Data refresh preserves Designer typography"), AuthoredFont == Detail->GetFont());

    // TileView cards are separate widget trees and are not generated until the first
    // layout. Inspect an authored instance so their UI shaders are warm before that draw.
    TStrongObjectPtr<UFF_FacilityCodexCard> AuthoredCard(CreateWidget<UFF_FacilityCodexCard>(World, CardClass));
    if (!TestNotNull(TEXT("Authored reward card instance"), AuthoredCard.Get())) return false;
    auto* AuthoredReward = Cast<UButton>(AuthoredCard->GetWidgetFromName(TEXT("Button_ClaimReward")));
    auto* AuthoredClaimed = Cast<UBorder>(AuthoredCard->GetWidgetFromName(TEXT("Border_RewardClaimed")));
    auto* AuthoredLocked = Cast<UBorder>(AuthoredCard->GetWidgetFromName(TEXT("Border_RewardLocked")));
    if (!TestNotNull(TEXT("Authored reward action"), AuthoredReward) ||
        !TestNotNull(TEXT("Authored completion surface"), AuthoredClaimed) ||
        !TestNotNull(TEXT("Authored undiscovered reward hint"), AuthoredLocked)) return false;
    AuthoredCard->RefreshPresentation();
    TestFalse(TEXT("A card without an item hides its reward region"),
        AuthoredCard->GetWidgetFromName(TEXT("Size_RewardState"))->IsVisible());
    TestTrue(TEXT("A card without an item clears its reward hints"),
        !AuthoredLocked->IsVisible() && AuthoredLocked->GetToolTipText().IsEmpty());
    TSet<UMaterialInterface*> RewardMaterials;
    const FSlateBrush* RewardBrushes[] = {&AuthoredReward->GetStyle().Normal, &AuthoredReward->GetStyle().Hovered,
        &AuthoredReward->GetStyle().Pressed, &AuthoredClaimed->Background};
    const TCHAR* RewardStates[] = {TEXT("Ready"), TEXT("Hover"), TEXT("Pressed"), TEXT("Claimed")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(RewardBrushes); ++Index)
    {
        auto* Material = Cast<UMaterialInterface>(RewardBrushes[Index]->GetResourceObject());
        if (!TestNotNull(FString::Printf(TEXT("%s reward uses a saved iridescent material"), RewardStates[Index]), Material)) continue;
        RewardMaterials.Add(Material);
        TestTrue(TEXT("Reward materials use the UI domain for Slate rendering"), Material->IsUIMaterial());
        TestEqual(TEXT("Reward state resolves its production material"), Material->GetPathName(),
            FString::Printf(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/MI_FF_CodexReward_%s.MI_FF_CodexReward_%s"),
                RewardStates[Index], RewardStates[Index]));
        float MotionStrength = -1.f;
        if (TestTrue(TEXT("Reward shader exposes its motion strength"),
            Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("MotionStrength")), MotionStrength)))
        {
            TestTrue(TEXT("Available rewards shimmer; pressed and completed states stay still"),
                Index < 2 ? MotionStrength > 0.f : FMath::IsNearlyZero(MotionStrength));
        }
    }
    TestEqual(TEXT("Ready, hover, pressed and completion have distinct material resources"), RewardMaterials.Num(), 4);

    // Standalone runs cannot rely on BuildTools.RenderedPreview warming shared materials.
    if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        TSet<UMaterialInterface*> Materials = RewardMaterials;
        auto GatherMaterial = [&Materials](const FSlateBrush& Brush)
        {
            if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) Materials.Add(Material);
        };
        auto GatherWidgetMaterials = [&](UWidget* Widget)
        {
            if (const UBorder* Border = Cast<UBorder>(Widget)) GatherMaterial(Border->Background);
            if (const UButton* Button = Cast<UButton>(Widget))
            {
                GatherMaterial(Button->GetStyle().Normal);
                GatherMaterial(Button->GetStyle().Hovered);
                GatherMaterial(Button->GetStyle().Pressed);
                GatherMaterial(Button->GetStyle().Disabled);
            }
            if (const UImage* Image = Cast<UImage>(Widget)) GatherMaterial(Image->GetBrush());
            if (const auto* Image = Cast<UFF_FacilityCodexImage>(Widget))
                if (Image->DiscoveryMaterial) Materials.Add(Image->DiscoveryMaterial);
        };
        Screen->WidgetTree->ForEachWidget(GatherWidgetMaterials);
        AuthoredCard->WidgetTree->ForEachWidget(GatherWidgetMaterials);
        for (UMaterialInterface* Material : Materials) Material->EnsureIsComplete();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        FlushRenderingCommands();
    }

    // A deterministic preview backdrop makes transparency visible without opening a
    // gameplay map or reading the player's profile. This is render-fixture art only.
    constexpr int32 BackdropSide = 128;
    TStrongObjectPtr<UTexture2D> PreviewBackdrop(UTexture2D::CreateTransient(BackdropSide, BackdropSide, PF_B8G8R8A8));
    auto* BackdropPixels = static_cast<FColor*>(PreviewBackdrop->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
    for (int32 Y = 0; Y < BackdropSide; ++Y)
        for (int32 X = 0; X < BackdropSide; ++X)
        {
            const float U = float(X) / (BackdropSide - 1), V = float(Y) / (BackdropSide - 1);
            const float Glow = FMath::Exp(-((U - .88f) * (U - .88f) + (V - .2f) * (V - .2f)) * 9.f);
            const float Wave = .5f + .5f * FMath::Sin(U * 12.f + V * 8.f);
            const FLinearColor Sea(.025f + Wave * .025f, .07f + V * .08f, .065f + V * .07f, 1.f);
            const FLinearColor Sand(.54f, .35f, .15f, 1.f);
            BackdropPixels[Y * BackdropSide + X] = FMath::Lerp(Sea, Sand, Glow * .75f).ToFColorSRGB();
        }
    PreviewBackdrop->GetPlatformData()->Mips[0].BulkData.Unlock();
    PreviewBackdrop->SRGB = true;
    PreviewBackdrop->UpdateResource();
    FSlateBrush BackdropBrush;
    BackdropBrush.SetResourceObject(PreviewBackdrop.Get());
    BackdropBrush.ImageSize = FVector2D(BackdropSide, BackdropSide);
    auto Capture = [&](int32 Width, int32 Height, const TCHAR* Filename) {
        if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
        FWidgetRenderer Renderer(false, true);
        FVector2D Size(Width, Height);
        const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
        if (!Target) return false;
        auto Window = SNew(SVirtualWindow).Size(Size);
        Window->SetContent(SNew(SOverlay)
            + SOverlay::Slot()[SNew(SImage).Image(&BackdropBrush)]
            + SOverlay::Slot()[SNew(SDPIScaler).DPIScale(Dpi)[Slate]]);
        FHittestGrid Grid;
        for (int32 Frame = 0; Frame < 8; ++Frame)
        {
            // Offscreen paints do not advance SObjectTableRow's selection timer.
            // Query the live list selection so this fixture cannot retain an old highlight.
            for (UUserWidget* Widget : Tile->GetDisplayedEntryWidgets())
                if (auto* Card = Cast<UFF_FacilityCodexCard>(Widget)) Card->RefreshPresentation();
            Screen->ForceLayoutPrepass();
            Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
        }
        if (FString(Filename).Contains(TEXT("RewardHover")))
        {
            UButton* HoverReward = nullptr;
            for (UUserWidget* Card : Tile->GetDisplayedEntryWidgets())
            {
                auto* Button = Cast<UButton>(Card->GetWidgetFromName(TEXT("Button_ClaimReward")));
                if (Button && Button->IsVisible() && Button->GetIsEnabled()) { HoverReward = Button; break; }
            }
            if (TestNotNull(TEXT("Hover preview has an available reward"), HoverReward))
            {
                const FGeometry& Geometry = HoverReward->GetPaintSpaceGeometry();
                const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
                TArray<FWidgetAndPointer> Bubble = Grid.GetBubblePath(Point, 0.f, false);
                FWidgetPath Path(MakeArrayView(Bubble));
                if (TestTrue(TEXT("Reward hover follows the rendered Slate hit path"),
                    Path.IsValid() && Path.ContainsWidget(HoverReward->GetCachedWidget().Get())))
                {
                    const FPointerEvent Move(0, Point, Point, TSet<FKey>(), EKeys::Invalid, 0.f, FModifierKeysState());
                    FSlateApplication::Get().RoutePointerMoveEvent(Path, Move, false);
                    TestTrue(TEXT("Available reward enters its iridescent hover state"), HoverReward->IsHovered());
                    for (int32 Frame = 0; Frame < 8; ++Frame)
                        Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
                }
            }
        }
        const FSlateRect CloseRect = Close->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        if (Width >= 1280)
            TestTrue(TEXT("Search keeps a usable full-width input beneath its passive hint"),
                Search->GetPaintSpaceGeometry().GetLayoutBoundingRect().GetSize().X > Width * .35f);
        TestTrue(TEXT("Close button stays inside viewport"), CloseRect.Left >= 0 && CloseRect.Top >= 0 &&
            CloseRect.Right <= Width && CloseRect.Bottom <= Height && CloseRect.GetSize().GetMin() > 0);
        TArray<FSlateRect> ActionRects;
        for (const TCHAR* Name : {TEXT("Button_Close"), TEXT("Button_All"), TEXT("Button_Discovered"),
            TEXT("Button_Undiscovered"), TEXT("Button_Active"), TEXT("Button_RewardReady"), TEXT("Button_ClearFacility"),
            TEXT("Button_DetailRecipe"), TEXT("Button_DetailPlacement"), TEXT("Button_DetailEffect")})
        {
            UWidget* Action = Screen->GetWidgetFromName(FName(Name));
            bool bVisible = Action && Action->IsVisible();
            for (UWidget* Parent = Action ? Action->GetParent() : nullptr; Parent && bVisible; Parent = Parent->GetParent())
                bVisible = Parent->IsVisible();
            if (!bVisible) continue;
            const FSlateRect Rect = Action->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Codex actions share a consistent 40px hit target"), FMath::IsNearlyEqual(Rect.GetSize().Y / Dpi,40.f,1.f));
            for (const FSlateRect& Other : ActionRects)
                TestFalse(TEXT("Codex actions never overlap"), Rect.Left < Other.Right -.5f && Rect.Right > Other.Left +.5f &&
                    Rect.Top < Other.Bottom -.5f && Rect.Bottom > Other.Top +.5f);
            ActionRects.Add(Rect);
        }
        TestTrue(TEXT("Visible recipes generate cards"), Screen->GetVisibleEntryCount() == 0 || Tile->GetDisplayedEntryWidgets().Num() > 0);
        const TArray<UUserWidget*>& Cards = Tile->GetDisplayedEntryWidgets();
        for (UUserWidget* Widget : Cards)
        {
            auto* Card = CastChecked<UFF_FacilityCodexCard>(Widget);
            const auto* Selection = CastChecked<UBorder>(Card->GetWidgetFromName(TEXT("Border_Selection")));
            TestEqual(TEXT("Rendered selection highlight matches the live list selection"),
                Selection->IsVisible(), Card->IsListItemSelected());
        }
        TestEqual(TEXT("Responsive layout preserves authored TileView width"), Tile->GetEntryWidth(), AuthoredEntryWidth);
        TestEqual(TEXT("Responsive layout preserves authored TileView height"), Tile->GetEntryHeight(), AuthoredEntryHeight);
        TestTrue(TEXT("Responsive layout preserves Designer typography"), Detail->GetFont() == AuthoredFont);
        const FSlateRect GridRect = Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        const FSlateRect InspectorRect = DetailBounds->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Responsive inspector and collection grid never overlap"),
            Main->GetOrientation() == Orient_Horizontal ? InspectorRect.Left >= GridRect.Right - 1.f
                : InspectorRect.Top >= GridRect.Bottom - 1.f);
        const float LogicalWidth = Width / Dpi, LogicalHeight = Height / Dpi;
        const bool bWideShowcase = LogicalWidth >= 1100.f;
        TestEqual(TEXT("Showcase switches layout at its readable 1100 logical-pixel threshold"),
            Main->GetOrientation(), bWideShowcase ? Orient_Horizontal : Orient_Vertical);
        if (bWideShowcase)
        {
            TestTrue(TEXT("Wide showcase reserves a bounded collection browser and gives the remaining width to detail"),
                FMath::IsNearlyEqual(BrowseBounds->GetWidthOverride(), FMath::Clamp(LogicalWidth * .37f, 420.f, 720.f), 1.f) &&
                !DetailBounds->IsWidthOverride());
        }
        else
        {
            TestTrue(TEXT("Compact showcase bounds its collection height while preserving space for detail"),
                BrowseBounds->IsHeightOverride() && FMath::IsNearlyEqual(BrowseBounds->GetHeightOverride(),
                    FMath::Clamp(LogicalHeight * .30f, 160.f, 260.f), 1.f));
        }
        const FSlateRect DialogRect = Dialog->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("The codex fills all four viewport edges for full, filtered and empty collections"),
            FMath::IsNearlyZero(DialogRect.Left, 1.f) && FMath::IsNearlyZero(DialogRect.Top, 1.f) &&
            FMath::IsNearlyEqual(DialogRect.Right, float(Width), 1.f) && FMath::IsNearlyEqual(DialogRect.Bottom, float(Height), 1.f));
        const FSlateRect DetailRect = DetailBounds->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        if (Screen->GetVisibleEntryCount() > 0)
        {
            auto* DetailScroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("Scroll_DetailGlass")));
            if (!TestNotNull(TEXT("Showcase details retain their authored vertical scroll viewport"), DetailScroll)) return false;
            const FSlateRect ScrollRect = DetailScroll->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("The detail scroll viewport stays inside its reserved region"),
                ScrollRect.Left >= DetailRect.Left - 1.f && ScrollRect.Right <= DetailRect.Right + 1.f &&
                ScrollRect.Top >= DetailRect.Top - 1.f && ScrollRect.Bottom <= DetailRect.Bottom + 1.f);
            bool bPagesInsideScroll = false;
            for (UWidget* Parent = DetailPages->GetParent(); Parent; Parent = Parent->GetParent())
                bPagesInsideScroll |= Parent == DetailScroll;
            TestTrue(TEXT("Long showcase detail pages are reachable through the vertical scroll view"), bPagesInsideScroll);
            const FSlateRect ActivePageRect = DetailPages->GetActiveWidget()->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(FString::Printf(TEXT("%s: detail pages fit the scroll viewport without horizontal overflow"), Filename),
                ActivePageRect.Left >= ScrollRect.Left - 1.f && ActivePageRect.Right <= ScrollRect.Right + 1.f);
            const TArray<FName> DetailNames = DetailPages->GetActiveWidgetIndex() == 0
                ? TArray<FName>{TEXT("Text_TargetName"), TEXT("Text_PartnerName"), TEXT("Text_Bonus")}
                : DetailPages->GetActiveWidgetIndex() == 1
                ? TArray<FName>{TEXT("Text_Distance"), TEXT("Text_Condition"), TEXT("Text_Discovery")}
                : TArray<FName>{TEXT("Text_Applied")};
            for (FName Name : DetailNames)
            {
                const FSlateRect Rect = Screen->GetWidgetFromName(Name)->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Selected detail text fits the inspectable scroll content width"),
                    Rect.Left >= ScrollRect.Left - 1.f && Rect.Right <= ScrollRect.Right + 1.f);
            }
            const auto* Selected = Cast<UFF_FacilityCodexItem>(Tile->GetSelectedItem());
            if (TestNotNull(TEXT("Visible showcase has a selected recipe"), Selected))
            {
                const bool bClaimed = Selected->Entry.bDiscovered && Selected->Entry.bDiscoveryRewardClaimed;
                const bool bReady = Selected->Entry.bDiscovered && !bClaimed && Selected->Entry.DiscoveryRewardGold > 0;
                const bool bLocked = !Selected->Entry.bDiscovered && Selected->Entry.DiscoveryRewardGold > 0;
                TestEqual(TEXT("Selected-detail reward action follows only its selected recipe"), DetailReward->IsVisible(), bReady);
                TestEqual(TEXT("Selected-detail completion follows only its selected recipe"), DetailClaimed->IsVisible(), bClaimed);
                TestEqual(TEXT("Selected-detail locked hint follows only its selected recipe"), DetailLocked->IsVisible(), bLocked);
                if (bReady || bClaimed || bLocked)
                {
                    const UWidget* Surface = bReady ? static_cast<UWidget*>(DetailReward) : bClaimed
                        ? static_cast<UWidget*>(DetailClaimed) : static_cast<UWidget*>(DetailLocked);
                    const FSlateRect RewardRect = Surface->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("Selected-detail reward states all paint the same 72 by 32 allocation"),
                        FMath::IsNearlyEqual(RewardRect.GetSize().X / Dpi, 72.f, 1.f) &&
                        FMath::IsNearlyEqual(RewardRect.GetSize().Y / Dpi, 32.f, 1.f));
                }
            }
        }
        else
        {
            // Slate does not arrange hidden descendants. Their cached geometry can belong
            // to the preceding resolution, so verify the empty-state contract instead.
            TestEqual(TEXT("Empty results hide the detail surface"),
                Screen->GetWidgetFromName(TEXT("Panel_Detail"))->GetVisibility(), ESlateVisibility::Hidden);
        }
        const FSlateRect ListBounds = Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        const int32 Columns = CastChecked<UFF_FacilityCodexTileView>(Tile)->ColumnCount;
        TestTrue(TEXT("Responsive collection keeps a readable multi-column layout"), Columns >= 2 && Columns <= 8);
        for (int32 Index = 0; Index < FMath::Min(Columns, Tile->GetListItems().Num()); ++Index)
        {
            UUserWidget* Card = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[Index]);
            if (!TestNotNull(TEXT("First card row is generated at project DPI"), Card)) continue;
            TestTrue(TEXT("Cards do not cover neighbouring recipes with repeated-name tooltips"),Card->GetToolTipText().IsEmpty() && Card->GetToolTip() == nullptr);
            const FSlateRect CardBounds = Card->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Showcase cards retain a readable logical width"), CardBounds.GetSize().X / Dpi >= 170.f);
            TestTrue(TEXT("Project-DPI viewport shows the complete first card row including its status"),
                CardBounds.Top >= ListBounds.Top - 1.f && CardBounds.Bottom <= ListBounds.Bottom + 1.f);
            auto* NameText = CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_Name")));
            UWidget* NameBounds = Card->GetWidgetFromName(TEXT("Size_Name"));
            UWidget* Images = Card->GetWidgetFromName(TEXT("Horizontal_RecipeImages"));
            const FSlateRect NameRect = NameText->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect NameRegion = NameBounds->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect ImagesRect = Images->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(FString::Printf(TEXT("%s: complete wrapped title '%s' fits its reserved height (desired %.2f, allotted %.2f)"),
                Filename, *NameText->GetText().ToString(), NameText->GetDesiredSize().Y,
                NameText->GetPaintSpaceGeometry().GetLocalSize().Y),
                NameText->GetDesiredSize().Y <= NameText->GetPaintSpaceGeometry().GetLocalSize().Y + 1.f);
            TestTrue(TEXT("Wrapped card titles never extend into the ingredient images"),
                NameRect.Bottom <= NameRegion.Bottom + 1.f && NameRegion.Bottom <= ImagesRect.Top + 1.f);
            auto* Bonus = CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_Bonus")));
            UWidget* State = Card->GetWidgetFromName(TEXT("Border_State"));
            const FSlateRect BonusRect = Bonus->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect StateRect = State->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Bonus text fits one full-width line, including the percent suffix"),
                Bonus->GetDesiredSize().X <= Bonus->GetPaintSpaceGeometry().GetLocalSize().X + 1.f && BonusRect.GetSize().Y / Dpi < 30.f);
            TestTrue(TEXT("A discovery badge cannot squeeze or overlap the bonus"), StateRect.Top >= BonusRect.Bottom - 1.f);
            const auto* Item = CastChecked<UFF_FacilityCodexItem>(Tile->GetListItems()[Index]);
            auto* RewardButton = Cast<UButton>(Card->GetWidgetFromName(TEXT("Button_ClaimReward")));
            if (TestNotNull(TEXT("Saved card exposes its claim action"), RewardButton))
            {
                const bool bClaimed = Item->Entry.bDiscovered && Item->Entry.bDiscoveryRewardClaimed;
                const bool bRewardVisible = Item->Entry.bDiscovered && Item->Entry.DiscoveryRewardGold > 0 && !bClaimed;
                const bool bLocked = !Item->Entry.bDiscovered && Item->Entry.DiscoveryRewardGold > 0;
                auto* ClaimedBadge = CastChecked<UBorder>(Card->GetWidgetFromName(TEXT("Border_RewardClaimed")));
                auto* LockedBadge = CastChecked<UBorder>(Card->GetWidgetFromName(TEXT("Border_RewardLocked")));
                auto* RewardRegion = CastChecked<USizeBox>(Card->GetWidgetFromName(TEXT("Size_RewardState")));
                TestEqual(TEXT("Only unclaimed discovered rewards show their action"), RewardButton->IsVisible(), bRewardVisible);
                TestEqual(TEXT("Claimed history shows a separate completion badge"), ClaimedBadge->IsVisible(), bClaimed);
                TestEqual(TEXT("Unknown recipes reveal a locked discovery reward"), LockedBadge->IsVisible(), bLocked);
                TestEqual(TEXT("The shared reward region includes future rewards and excludes absent rewards"),
                    RewardRegion->IsVisible(), bRewardVisible || bClaimed || bLocked);
                TestEqual(TEXT("A claimed reward cannot be clicked again"), RewardButton->GetIsEnabled(),
                    bRewardVisible);
                if (bLocked)
                {
                    TestTrue(TEXT("Undiscovered reward hints keep readable contrast and tooltips"), LockedBadge->GetIsEnabled());
                    const FString LockedTip = LockedBadge->GetToolTipText().ToString();
                    TestTrue(TEXT("Undiscovered reward tooltip explains its amount and discovery prerequisite"),
                        LockedTip.Contains(FText::AsNumber(Item->Entry.DiscoveryRewardGold).ToString()) &&
                        LockedTip.Contains(TEXT("발견하면")) && LockedTip.Contains(TEXT("골드")));
                    auto* LockedText = CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_RewardLocked")));
                    TestEqual(TEXT("Undiscovered reward state is explicit without hovering"),
                        LockedText->GetText().ToString(), FString(TEXT("잠김")));
                    TestTrue(TEXT("Locked reward caption fits its allotted width"),
                        LockedText->GetDesiredSize().X <= LockedText->GetPaintSpaceGeometry().GetLocalSize().X + 1.f);
                    auto* LockedImage = CastChecked<UImage>(Card->GetWidgetFromName(TEXT("Image_RewardLocked")));
                    auto* LockedGiftMaterial = Cast<UMaterialInterface>(LockedImage->GetBrush().GetResourceObject());
                    if (TestNotNull(TEXT("Unknown recipe uses the grayscale gift shader"), LockedGiftMaterial))
                    {
                        float VisualState = 0.f;
                        UTexture* GiftTexture = nullptr;
                        TestTrue(TEXT("Locked gift has a grayscale state"), LockedGiftMaterial->GetScalarParameterValue(
                            FHashedMaterialParameterInfo(TEXT("VisualState")), VisualState) && FMath::IsNearlyEqual(VisualState, 1.f));
                        TestTrue(TEXT("Locked gift retains its original gift artwork"), LockedGiftMaterial->GetTextureParameterValue(
                            FHashedMaterialParameterInfo(TEXT("ThumbnailTexture")), GiftTexture) && GiftTexture &&
                            GiftTexture->GetName() == TEXT("T_FF_Codex_RewardGift"));
                    }
                    TestEqual(TEXT("Locked gift leaves its tooltip surface reachable"),
                        LockedImage->GetVisibility(), ESlateVisibility::HitTestInvisible);
                    const FSlateRect LockedRect = LockedBadge->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("Locked hint fits to the right of the fee inside its card"),
                        LockedRect.Left >= BonusRect.Right - 1.f && LockedRect.Right <= CardBounds.Right + 1.f &&
                        LockedRect.Top < BonusRect.Bottom && LockedRect.Bottom <= StateRect.Top + 1.f);
                }
                if (bRewardVisible)
                {
                    const FSlateRect RewardRect = RewardButton->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("Reward action sits to the right of the fee, inside its card"),
                        RewardRect.Left >= BonusRect.Right - 1.f && RewardRect.Right <= CardBounds.Right + 1.f &&
                        RewardRect.Top < BonusRect.Bottom && RewardRect.Bottom > BonusRect.Top &&
                        RewardRect.Bottom <= StateRect.Top + 1.f);
                    auto* RewardText = CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_ClaimReward")));
                    auto* RewardImage = Cast<UImage>(Card->GetWidgetFromName(TEXT("Image_ClaimReward")));
                    TestTrue(TEXT("Ready reward has a visible action caption beside its gift"), RewardText->IsVisible());
                    TestEqual(TEXT("Ready reward caption is concise"), RewardText->GetText().ToString(), FString(TEXT("받기")));
                    if (TestNotNull(TEXT("Saved card exposes its gift icon"), RewardImage))
                    {
                        TestNotNull(TEXT("Gift icon uses an authored texture"), Cast<UTexture2D>(RewardImage->GetBrush().GetResourceObject()));
                        TestEqual(TEXT("Gift icon lets the parent button receive clicks"),
                            RewardImage->GetVisibility(), ESlateVisibility::HitTestInvisible);
                        const FSlateRect IconRect = RewardImage->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                        TestTrue(TEXT("Gift icon has a visible area inside the reward button"),
                            IconRect.GetSize().X > 0.f && IconRect.GetSize().Y > 0.f &&
                            IconRect.Left >= RewardRect.Left - 1.f && IconRect.Right <= RewardRect.Right + 1.f &&
                            IconRect.Top >= RewardRect.Top - 1.f && IconRect.Bottom <= RewardRect.Bottom + 1.f);
                    }
                    TestTrue(TEXT("Gift tooltip describes the reward amount"),
                        RewardButton->GetToolTipText().ToString().Contains(FText::AsNumber(Item->Entry.DiscoveryRewardGold).ToString()));
                }
                if (bClaimed)
                {
                    TestTrue(TEXT("Completion badge keeps full contrast instead of disabled shading"), ClaimedBadge->GetIsEnabled());
                    auto* CompletedText = CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_RewardClaimed")));
                    TestEqual(TEXT("Completion is explicit without hovering"), CompletedText->GetText().ToString(), FString(TEXT("완료")));
                    TestTrue(TEXT("Completion text fits its allotted width"),
                        CompletedText->GetDesiredSize().X <= CompletedText->GetPaintSpaceGeometry().GetLocalSize().X + 1.f);
                    auto* CheckImage = CastChecked<UImage>(Card->GetWidgetFromName(TEXT("Image_RewardClaimed")));
                    TestNotNull(TEXT("Completion uses a distinct check icon"), Cast<UTexture2D>(CheckImage->GetBrush().GetResourceObject()));
                    const FSlateRect BadgeRect = ClaimedBadge->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("Completion badge occupies the reward region without overflowing"),
                        BadgeRect.Left >= BonusRect.Right - 1.f && BadgeRect.Right <= CardBounds.Right + 1.f &&
                        BadgeRect.Top < BonusRect.Bottom && BadgeRect.Bottom <= StateRect.Top + 1.f);
                }
            }
            TestTrue(TEXT("The complete card information stays inside its surface"), StateRect.Bottom <= CardBounds.Bottom + 1.f);
        }
        AddInfo(FString::Printf(TEXT("Codex viewport %dx%d uses project DPI %.3f and %s detail."),
            Width, Height, Dpi, Main->GetOrientation() == Orient_Horizontal ? TEXT("side") : TEXT("stacked")));
        if (Tile->GetListItems().Num() > Columns)
        {
            auto* FirstCard = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[0]);
            auto* NextRowCard = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[Columns]);
            if (TestNotNull(TEXT("First virtualized row exists"), FirstCard))
            {
                const FSlateRect First = FirstCard->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                for (int32 Index = 1; Index < Columns; ++Index)
                {
                    auto* Card = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[Index]);
                    if (!TestNotNull(TEXT("Each responsive column is generated"), Card)) continue;
                    const FSlateRect Rect = Card->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("First-row recipes share one row"), FMath::IsNearlyEqual(Rect.Top, First.Top, 1.f));
                    TestTrue(TEXT("Each column advances horizontally"), Rect.Left > First.Left);
                    TestTrue(TEXT("Last responsive column remains inside the list"), Rect.Right <=
                        Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect().Right + 1.f);
                }
                if (Width == 1920) TestNotNull(TEXT("Wide grid generates another visible row"), NextRowCard);
                if (NextRowCard)
                {
                    const FSlateRect Next = NextRowCard->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("Next recipe begins the next responsive row"), Next.Top > First.Top &&
                        FMath::IsNearlyEqual(Next.Left, First.Left, 1.f));
                }
            }
        }
        if (Cards.Num() >= 2)
        {
            const FSlateRect First = Cards[0]->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            bool bHasSecondColumn = false;
            for (const UUserWidget* Card : Cards)
            {
                const FSlateRect Rect = Card->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                bHasSecondColumn |= FMath::Abs(Rect.Left - First.Left) > 10.f;
            }
            TestTrue(TEXT("Recipes use multiple grid columns"), bHasSecondColumn);
            const UWidget* State = Cards[0]->GetWidgetFromName(TEXT("Text_State"));
            const UWidget* Body = Cards[0]->GetWidgetFromName(TEXT("Border_Card"));
            if (TestNotNull(TEXT("Card state label"), State) && TestNotNull(TEXT("Card body"), Body))
                TestTrue(TEXT("Card state remains inside its surface"), State->GetPaintSpaceGeometry().GetLayoutBoundingRect().Bottom <=
                    Body->GetPaintSpaceGeometry().GetLayoutBoundingRect().Bottom + 1.f);
        }
        if (FString(Filename) == TEXT("FacilityCodex720.png"))
        {
            auto PointerClick = [&](UWidget* HitRegion, UButton* ExpectedButton)
            {
                const FGeometry& Geometry = HitRegion->GetPaintSpaceGeometry();
                const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
                TArray<FWidgetAndPointer> Bubble = Grid.GetBubblePath(Point, 0.f, false);
                FWidgetPath Path(MakeArrayView(Bubble));
                if (!TestTrue(TEXT("Rendered action is reachable through the actual Slate hit path"),
                    Path.IsValid() && Path.ContainsWidget(ExpectedButton->GetCachedWidget().Get()))) return;
                FSlateApplication& App = FSlateApplication::Get();
                const TSharedPtr<SWidget> PreviousFocus = App.GetUserFocusedWidget(0);
                const TSet<FKey> Pressed{EKeys::LeftMouseButton}, Released;
                const FPointerEvent Move(0, Point, Point, Released, EKeys::Invalid, 0.f, FModifierKeysState());
                const FPointerEvent Down(0, Point, Point, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                const FPointerEvent Up(0, Point, Point, Released, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                // DownAndUp buttons need the preceding pointer-enter event, just as a real cursor does.
                App.RoutePointerMoveEvent(Path, Move, false);
                TestTrue(TEXT("Pointer reaches the button hover state"), ExpectedButton->IsHovered());
                TestTrue(TEXT("Action consumes pointer down"), App.RoutePointerDownEvent(Path, Down).IsEventHandled());
                TestTrue(TEXT("Action receives pointer press before its parent row"), ExpectedButton->IsPressed());
                TestTrue(TEXT("Action consumes pointer up"), App.RoutePointerUpEvent(Path, Up).IsEventHandled());
                if (PreviousFocus) App.SetUserFocus(0, PreviousFocus, EFocusCause::SetDirectly);
            };
            for (const TCHAR* Side : {TEXT("Target"), TEXT("Partner")})
            {
                const bool bTarget = FString(Side) == TEXT("Target");
                const FName ExpectedId = bTarget ? Entries[0].TargetFacilityId : Entries[0].PartnerFacilityId;
                UButton* Link = bTarget ? TargetCatalog : PartnerCatalog;
                for (const FString Region : {FString(TEXT("Text_")) + Side + TEXT("Name"), FString(TEXT("Size_Thumb")) + Side})
                {
                    RequestedFacility = NAME_None;
                    PointerClick(Screen->GetWidgetFromName(FName(*Region)), Link);
                    TestEqual(TEXT("Clicking either the facility name or image requests its catalog entry"), RequestedFacility, ExpectedId);
                }
            }
            const FName PreviewSelection = Screen->GetSelectedRuleId();
            PointerClick(PreviewPartner->GetContent(), PreviewPartner);
            TestTrue(TEXT("Pointer interaction switches the showcase to its supporting facility"),
                PreviewName->GetText().EqualTo(Entries[0].PartnerName));
            PointerClick(PreviewTarget->GetContent(), PreviewTarget);
            TestTrue(TEXT("Pointer interaction restores the showcase target without changing recipe selection"),
                PreviewName->GetText().EqualTo(Entries[0].TargetName) && Screen->GetSelectedRuleId() == PreviewSelection);
            PointerClick(Screen->GetWidgetFromName(TEXT("Image_DetailClaimReward")), DetailReward);
            TestFalse(TEXT("An actual detail reward click in preview cannot mark the reward paid"),
                CastChecked<UFF_FacilityCodexItem>(Tile->GetSelectedItem())->Entry.bDiscoveryRewardClaimed);
            auto* ClaimItem = CastChecked<UFF_FacilityCodexItem>(Tile->GetListItems()[0]);
            auto* ClaimCard = Tile->GetEntryWidgetFromItem(ClaimItem);
            auto* ClaimButton = CastChecked<UButton>(ClaimCard->GetWidgetFromName(TEXT("Button_ClaimReward")));
            const auto OriginalClaimHandler = ClaimItem->OnClaimRewardRequested;
            int32 RoutedClaims = 0;
            ClaimItem->OnClaimRewardRequested.BindLambda([&](FName RuleId)
            {
                ++RoutedClaims;
                TestEqual(TEXT("Routed reward click uses its card rule"), RuleId, ClaimItem->Entry.RuleId);
            });
            const FName SelectionBeforeClaim = Screen->GetSelectedRuleId();
            PointerClick(ClaimCard->GetWidgetFromName(TEXT("Image_ClaimReward")), ClaimButton);
            ClaimItem->OnClaimRewardRequested = OriginalClaimHandler;
            TestEqual(TEXT("A full mouse click requests exactly one reward"), RoutedClaims, 1);
            TestEqual(TEXT("A reward click preserves detail selection"), Screen->GetSelectedRuleId(), SelectionBeforeClaim);
        }
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags;
        Flags.SetLinearToGamma(false);
        if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) return false;
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityCodex");
        IFileManager::Get().MakeDirectory(*Directory, true);
        return FFileHelper::SaveArrayToFile(Png, *(Directory / Filename));
    };
    TestTrue(TEXT("720p visual preview"), Capture(1280, 720, TEXT("FacilityCodex720.png")));
    TestTrue(TEXT("Narrow project-DPI responsive preview"), Capture(1024, 768, TEXT("FacilityCodex1024x768.png")));
    TestTrue(TEXT("Compact showcase stacks its browser above the scrollable detail"),
        Capture(720, 960, TEXT("FacilityCodexShowcaseCompact.png")));
    TestTrue(TEXT("1080p visual preview"), Capture(1920, 1080, TEXT("FacilityCodex1080.png")));
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_DetailPlacement")))->OnClicked.Broadcast();
    TestEqual(TEXT("Placement conditions open directly without scrolling"), DetailPages->GetActiveWidgetIndex(), 1);
    CheckSelectedTab(TEXT("Button_DetailPlacement"));
    TestTrue(TEXT("Placement tab visual preview"), Capture(1280, 720, TEXT("FacilityCodexPlacement720.png")));
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_DetailEffect")))->OnClicked.Broadcast();
    TestEqual(TEXT("Current effects open directly without scrolling"), DetailPages->GetActiveWidgetIndex(), 2);
    CheckSelectedTab(TEXT("Button_DetailEffect"));
    TestTrue(TEXT("Effect tab visual preview"), Capture(1280, 720, TEXT("FacilityCodexEffect720.png")));
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_DetailRecipe")))->OnClicked.Broadcast();
    TestEqual(TEXT("Recipe navigation returns to ingredients"), DetailPages->GetActiveWidgetIndex(), 0);
    CheckSelectedTab(TEXT("Button_DetailRecipe"));
    TestTrue(TEXT("Recipe tab restores the original content"), Capture(1920, 1080, TEXT("FacilityCodexRecipe1080.png")));
    if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        UUserWidget* FoundCard = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[0]);
        UUserWidget* UnknownCard = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[1]);
        if (TestNotNull(TEXT("Discovered card remains visible"), FoundCard) &&
            TestNotNull(TEXT("Undiscovered card remains visible"), UnknownCard))
        {
            auto* FoundBonus = CastChecked<UTextBlock>(FoundCard->GetWidgetFromName(TEXT("Text_Bonus")));
            auto* UnknownBonus = CastChecked<UTextBlock>(UnknownCard->GetWidgetFromName(TEXT("Text_Bonus")));
            auto* UnknownBody = CastChecked<UBorder>(UnknownCard->GetWidgetFromName(TEXT("Border_Card")));
            auto* UnknownImage = CastChecked<UFF_FacilityCodexImage>(UnknownCard->GetWidgetFromName(TEXT("Image_Target")));
            auto* UnknownPartner = CastChecked<UFF_FacilityCodexImage>(UnknownCard->GetWidgetFromName(TEXT("Image_Partner")));
            const FSlateColor DiscoveredColor = FoundBonus->GetColorAndOpacity();
            const FSlateColor UnknownColor = UnknownBonus->GetColorAndOpacity();
            const FLinearColor UnknownInk = UnknownColor.GetSpecifiedColor();
            // Use linear-sRGB relative luminance, independent of the engine's
            // optional legacy luminance factors. Black behind the translucent
            // paper gives the conservative contrast for this dark foreground.
            const auto RelativeLuminance = [](const FLinearColor& Color)
            { return .2126f * Color.R + .7152f * Color.G + .0722f * Color.B; };
            const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(0xF7, 0xF8, 0xF6));
            float CardOpacity = 0.f;
            if (const auto* Material = Cast<UMaterialInterface>(UnknownBody->Background.GetResourceObject()))
                Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("SurfaceOpacity")), CardOpacity);
            const float PaperLuminance = RelativeLuminance(Paper) * CardOpacity;
            const float InkLuminance = RelativeLuminance(UnknownInk);
            const float CompositedInkLuminance = FMath::Lerp(PaperLuminance, InkLuminance, UnknownInk.A);
            const float ContrastRatio = (PaperLuminance + .05f) / (CompositedInkLuminance + .05f);
            TestTrue(FString::Printf(TEXT("Undiscovered bonus uses opaque dark ink with at least 4.5:1 contrast on translucent pearl paper (%.2f:1)"), ContrastRatio),
                InkLuminance < .18f && UnknownInk.A > .95f && CardOpacity > 0.f && ContrastRatio >= 4.5f);
            TestTrue(TEXT("Undiscovered card can still be selected"), UnknownCard->GetIsEnabled());
            TestTrue(TEXT("Unknown artwork uses the discovery shader at full UI contrast"), UnknownImage->GetIsEnabled());
            TestEqual(TEXT("Installed facility in an unknown recipe is grayscale"),
                UnknownImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Grayscale);
            TestEqual(TEXT("Uninstalled facility in an unknown recipe is a silhouette"),
                UnknownPartner->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Silhouette);
            Tile->SetSelectedItem(Tile->GetListItems()[1]);
            TestEqual(TEXT("Undiscovered card reveals its recipe detail"), Screen->GetSelectedRuleId(), FName(TEXT("IceCreamShade")));
            const FSlateBrush UnknownBrush = UnknownBody->Background;
            Entries[1].bDiscovered = true;
            Screen->SetPreviewEntries(Entries);
            TestTrue(TEXT("Discovery restores the authored accent on the existing card"),
                UnknownBonus->GetColorAndOpacity() == DiscoveredColor);
            TestTrue(TEXT("Discovery restores thumbnail presentation"), UnknownImage->GetIsEnabled());
            TestEqual(TEXT("Discovery restores the original full-color source"),
                UnknownImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Color);
            TestTrue(TEXT("Both discovered recipes fill the collection progress"), FMath::IsNearlyEqual(CollectionProgress->GetPercent(), 1.f));
            Entries[1].bDiscovered = false;
            Screen->SetPreviewEntries(Entries);
            TestTrue(TEXT("A reused unknown card returns to its authored undiscovered palette"),
                UnknownBonus->GetColorAndOpacity() == UnknownColor && UnknownBody->Background == UnknownBrush);
            auto* FoundReward = CastChecked<UButton>(FoundCard->GetWidgetFromName(TEXT("Button_ClaimReward")));
            auto* UnknownReward = CastChecked<UButton>(UnknownCard->GetWidgetFromName(TEXT("Button_ClaimReward")));
            auto* UnknownLocked = CastChecked<UBorder>(UnknownCard->GetWidgetFromName(TEXT("Border_RewardLocked")));
            auto* UnknownCompleted = CastChecked<UBorder>(UnknownCard->GetWidgetFromName(TEXT("Border_RewardClaimed")));
            auto* FoundItem = CastChecked<UFF_FacilityCodexItem>(Tile->GetListItems()[0]);
            auto* UnknownItem = CastChecked<UFF_FacilityCodexItem>(Tile->GetListItems()[1]);
            int32 ClaimRequests = 0;
            FName RequestedRule;
            auto RecordClaim = [&](FName RuleId) { ++ClaimRequests; RequestedRule = RuleId; };
            FoundItem->OnClaimRewardRequested.BindLambda(RecordClaim);
            UnknownItem->OnClaimRewardRequested.BindLambda(RecordClaim);
            FoundReward->OnClicked.Broadcast();
            TestEqual(TEXT("One click requests one claim"), ClaimRequests, 1);
            TestEqual(TEXT("Claim uses the clicked card even when another recipe is selected"), RequestedRule, Entries[0].RuleId);
            UnknownReward->OnClicked.Broadcast();
            TestEqual(TEXT("Unknown card rejects synthetic claim clicks too"), ClaimRequests, 1);
            TestTrue(TEXT("Unknown card advertises its reward without exposing a claim action"),
                UnknownLocked->IsVisible() && !UnknownReward->IsVisible() && !UnknownReward->GetIsEnabled());
            FoundItem->Entry.bDiscoveryRewardClaimed = true;
            CastChecked<UFF_FacilityCodexCard>(FoundCard)->RefreshPresentation();
            FoundReward->OnClicked.Broadcast();
            TestEqual(TEXT("Claimed card rejects repeat clicks before reaching the economy"), ClaimRequests, 1);
            TestFalse(TEXT("Received action is disabled"), FoundReward->GetIsEnabled());
            TestFalse(TEXT("Received gift action disappears"), FoundReward->IsVisible());
            auto* CompletedBadge = CastChecked<UBorder>(FoundCard->GetWidgetFromName(TEXT("Border_RewardClaimed")));
            TestTrue(TEXT("Completion badge replaces the gift immediately"), CompletedBadge->IsVisible());
            TestTrue(TEXT("Completion badge explains its state in the tooltip"),
                CompletedBadge->GetToolTipText().ToString().Contains(TEXT("수령 완료")));
            TestTrue(TEXT("Claimed reward visual preview"), Capture(1280, 720, TEXT("FacilityCodexClaimed720.png")));
            FoundItem->Entry.DiscoveryRewardGold = 0;
            CastChecked<UFF_FacilityCodexCard>(FoundCard)->RefreshPresentation();
            TestTrue(TEXT("A changed zero reward amount does not erase claim history"), CompletedBadge->IsVisible());
            // Restore screen-owned delegates before local assertion captures leave scope.
            Screen->SetPreviewEntries(Entries);
            TestFalse(TEXT("Pooled card clears the previous completion badge"), CompletedBadge->IsVisible());
            TestTrue(TEXT("Pooled card restores an available claim"), FoundReward->IsVisible() && FoundReward->GetIsEnabled());
            UnknownItem->Entry.bDiscovered = true;
            CastChecked<UFF_FacilityCodexCard>(UnknownCard)->RefreshPresentation();
            TestTrue(TEXT("Discovery replaces a pooled locked hint with its claim action"),
                !UnknownLocked->IsVisible() && UnknownLocked->GetToolTipText().IsEmpty() &&
                UnknownReward->IsVisible() && UnknownReward->GetIsEnabled());
            UnknownItem->Entry.bDiscoveryRewardClaimed = true;
            CastChecked<UFF_FacilityCodexCard>(UnknownCard)->RefreshPresentation();
            TestTrue(TEXT("Claiming replaces the available state without restoring its locked hint"),
                UnknownCompleted->IsVisible() && !UnknownLocked->IsVisible() && !UnknownReward->IsVisible());
            UnknownItem->Entry.bDiscovered = false;
            UnknownItem->Entry.bDiscoveryRewardClaimed = false;
            CastChecked<UFF_FacilityCodexCard>(UnknownCard)->RefreshPresentation();
            TestTrue(TEXT("Reusing a completed card for an unknown recipe restores only its locked hint"),
                UnknownLocked->IsVisible() && !UnknownCompleted->IsVisible() &&
                UnknownCompleted->GetToolTipText().IsEmpty() && !UnknownReward->IsVisible());
            UnknownItem->Entry.DiscoveryRewardGold = 0;
            CastChecked<UFF_FacilityCodexCard>(UnknownCard)->RefreshPresentation();
            TestTrue(TEXT("A zero-reward unknown recipe clears its previous gift hint and amount"),
                !UnknownLocked->IsVisible() && UnknownLocked->GetToolTipText().IsEmpty() &&
                !UnknownCard->GetWidgetFromName(TEXT("Size_RewardState"))->IsVisible());
            Screen->SetPreviewEntries(Entries);
        }
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        auto SameRect = [](const FSlateRect& Before, const FSlateRect& After)
        {
            return FMath::IsNearlyEqual(Before.Left, After.Left, .25f) &&
                FMath::IsNearlyEqual(Before.Top, After.Top, .25f) &&
                FMath::IsNearlyEqual(Before.Right, After.Right, .25f) &&
                FMath::IsNearlyEqual(Before.Bottom, After.Bottom, .25f);
        };
        for (const FIntPoint Viewport : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
        {
            TArray<FFF_FacilityCodexEntry> LockedEntries = Entries;
            LockedEntries[0].bDiscovered = false;
            LockedEntries[0].bConditionMet = false;
            LockedEntries[0].bEffectApplied = false;
            Screen->SetPreviewEntries(LockedEntries);
            Tile->SetSelectedItem(Tile->GetListItems()[0]);
            const FString LockedFilename = FString::Printf(TEXT("FacilityCodexRewardLocked%d.png"), Viewport.Y);
            const FString BeforeFilename = FString::Printf(TEXT("FacilityCodexRewardBefore%d.png"), Viewport.Y);
            const FString AfterFilename = FString::Printf(TEXT("FacilityCodexRewardAfter%d.png"), Viewport.Y);
            // Capture runs eight layout/draw frames, so all measurements use painted
            // geometry after Slate has processed the visibility and desired-size change.
            if (!TestTrue(TEXT("Locked reward transition preview"), Capture(Viewport.X, Viewport.Y, *LockedFilename))) continue;
            auto* TransitionItem = CastChecked<UFF_FacilityCodexItem>(Tile->GetListItems()[0]);
            auto* TransitionCard = Cast<UFF_FacilityCodexCard>(Tile->GetEntryWidgetFromItem(TransitionItem));
            if (!TestNotNull(TEXT("Reward transition uses a rendered card"), TransitionCard)) continue;
            auto* LockedBadge = CastChecked<UBorder>(TransitionCard->GetWidgetFromName(TEXT("Border_RewardLocked")));
            auto* ReadyButton = CastChecked<UButton>(TransitionCard->GetWidgetFromName(TEXT("Button_ClaimReward")));
            auto* CompletedBadge = CastChecked<UBorder>(TransitionCard->GetWidgetFromName(TEXT("Border_RewardClaimed")));
            UWidget* RewardRow = TransitionCard->GetWidgetFromName(TEXT("Horizontal_RewardActions"));
            UWidget* FeeText = TransitionCard->GetWidgetFromName(TEXT("Text_Bonus"));
            const FSlateRect LockedRect = LockedBadge->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FVector2D LockedDetailSize = DetailLocked->GetPaintSpaceGeometry().GetLayoutBoundingRect().GetSize();
            const FSlateRect LockedRowRect = RewardRow->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect LockedFeeRect = FeeText->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Pre-discovery geometry belongs to a visible locked hint with positive dimensions"),
                LockedBadge->IsVisible() && LockedRect.GetSize().GetMin() > 0.f);

            TransitionItem->Entry = Entries[0];
            TransitionCard->RefreshPresentation();
            Screen->SetPreviewEntries(Entries);
            if (!TestTrue(TEXT("Ready reward transition preview"), Capture(Viewport.X, Viewport.Y, *BeforeFilename))) continue;
            const FSlateRect ReadyRect = ReadyButton->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FVector2D ReadyDetailSize = DetailReward->GetPaintSpaceGeometry().GetLayoutBoundingRect().GetSize();
            const FSlateRect ReadyRowRect = RewardRow->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect ReadyFeeRect = FeeText->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Pre-claim geometry belongs to a visible button with positive dimensions"),
                ReadyButton->IsVisible() && ReadyRect.GetSize().GetMin() > 0.f);
            TestTrue(FString::Printf(TEXT("Discovery preserves the reward surface bounds at %dp"), Viewport.Y),
                SameRect(LockedRect, ReadyRect));
            TestTrue(FString::Printf(TEXT("Discovery does not resize or move the fee row at %dp"), Viewport.Y),
                SameRect(LockedRowRect, ReadyRowRect) && SameRect(LockedFeeRect, ReadyFeeRect));
            TestTrue(FString::Printf(TEXT("Selected-detail reward retains its size when discovered at %dp"), Viewport.Y),
                LockedDetailSize.Equals(ReadyDetailSize, .25f));

            TransitionItem->Entry.bDiscoveryRewardClaimed = true;
            TransitionCard->RefreshPresentation();
            TArray<FFF_FacilityCodexEntry> CompletedEntries = Entries;
            CompletedEntries[0].bDiscoveryRewardClaimed = true;
            Screen->SetPreviewEntries(CompletedEntries);
            if (!TestTrue(TEXT("Completed reward transition preview"), Capture(Viewport.X, Viewport.Y, *AfterFilename))) continue;
            TestTrue(TEXT("Claim transition measures the same rendered card instance"),
                Tile->GetEntryWidgetFromItem(TransitionItem) == TransitionCard);
            TestTrue(TEXT("Post-claim geometry belongs to the visible completion surface"), CompletedBadge->IsVisible());
            const FSlateRect CompletedRect = CompletedBadge->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FVector2D CompletedDetailSize = DetailClaimed->GetPaintSpaceGeometry().GetLayoutBoundingRect().GetSize();
            TestTrue(FString::Printf(TEXT("Reward width and height remain identical after claiming at %dp"), Viewport.Y),
                FMath::IsNearlyEqual(ReadyRect.GetSize().X, CompletedRect.GetSize().X, .25f) &&
                FMath::IsNearlyEqual(ReadyRect.GetSize().Y, CompletedRect.GetSize().Y, .25f));
            TestTrue(FString::Printf(TEXT("Reward surface stays in the same position after claiming at %dp"), Viewport.Y),
                SameRect(ReadyRect, CompletedRect));
            TestTrue(FString::Printf(TEXT("Selected-detail claim replaces its button with an identically sized receipt at %dp"), Viewport.Y),
                DetailClaimed->IsVisible() && ReadyDetailSize.Equals(CompletedDetailSize, .25f));
            TestTrue(FString::Printf(TEXT("Claiming does not resize or move the fee row at %dp"), Viewport.Y),
                SameRect(ReadyRowRect, RewardRow->GetPaintSpaceGeometry().GetLayoutBoundingRect()));
            TestTrue(FString::Printf(TEXT("Claiming does not resize or move fee text at %dp"), Viewport.Y),
                SameRect(ReadyFeeRect, FeeText->GetPaintSpaceGeometry().GetLayoutBoundingRect()));
        }
        Screen->SetPreviewEntries(Entries);
    }
    TArray<FFF_FacilityCodexEntry> RewardStateEntries = Entries;
    RewardStateEntries[0].bDiscoveryRewardClaimed = true;
    RewardStateEntries[1].bDiscovered = true;
    auto& LockedPreview = RewardStateEntries.Add_GetRef(Entries[1]);
    LockedPreview.RuleId = TEXT("RewardLockedPreview");
    Screen->SetPreviewEntries(RewardStateEntries);
    TestTrue(TEXT("Completed, available and locked rewards can be compared in one viewport"),
        Capture(1280, 720, TEXT("FacilityCodexRewardStates720.png")));
    TestTrue(TEXT("Reward states remain clear at 1080p"),
        Capture(1920, 1080, TEXT("FacilityCodexRewardStates1080.png")));
    TestTrue(TEXT("Iridescent hover and static completed reward are visible together"),
        Capture(1920, 1080, TEXT("FacilityCodexRewardHover1080.png")));
    Screen->SetPreviewEntries(Entries);
    TArray<FFF_FacilityCodexEntry> ManyEntries;
    for (int32 Index = 0; Index < 120; ++Index)
    {
        auto& Entry = ManyEntries.Add_GetRef(Entries[Index % 2]);
        Entry.RuleId = FName(*FString::Printf(TEXT("FixtureRecipe_%03d"), Index));
    }
    Screen->SetPreviewEntries(ManyEntries);
    TestEqual(TEXT("Large recipe table remains available"), Screen->GetVisibleEntryCount(), 120);
    TestTrue(TEXT("Virtualized grid visual preview"), Capture(1280, 720, TEXT("FacilityCodexGrid720.png")));
    Screen->SetFacilityFilter(Burger->Spec.FacilityDefinitionId);
    TestTrue(TEXT("Filtered grid keeps a complete row with its clear-filter action"),
        Capture(1280, 720, TEXT("FacilityCodexFilteredGrid720.png")));
    Screen->SetFacilityFilter(NAME_None);
    TestTrue(TEXT("Full-screen wide grid visual preview"), Capture(1920, 1080, TEXT("FacilityCodexGrid1080.png")));
    TestTrue(TEXT("Ultrawide screen fills the viewport without scaling up typography"), Capture(2560, 1080, TEXT("FacilityCodexUltrawide.png")));
    if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
        TestTrue(TEXT("Large table creates only viewport cards"), Tile->GetDisplayedEntryWidgets().Num() < 120);
    Screen->SetPreviewEntries({});
    TestEqual(TEXT("Empty table releases all items"), Screen->GetVisibleEntryCount(), 0);
    TestTrue(TEXT("An empty collection clears its progress without dividing by zero"), FMath::IsNearlyZero(CollectionProgress->GetPercent()));
    RequestedFacility = NAME_None;
    TargetCatalog->OnClicked.Broadcast();
    PartnerCatalog->OnClicked.Broadcast();
    TestTrue(TEXT("Empty selection cannot navigate using stale facility IDs"), RequestedFacility.IsNone());
    PreviewTarget->OnClicked.Broadcast();
    PreviewPartner->OnClicked.Broadcast();
    DetailReward->OnClicked.Broadcast();
    TestTrue(TEXT("Empty selection clears the showcase model and every reward state"),
        ShowcaseViewport->GetRequestedMesh().IsNull() && !ShowcaseViewport->IsVisible() &&
        !DetailRewardBounds->IsVisible() && !DetailReward->IsVisible() &&
        !DetailClaimed->IsVisible() && !DetailLocked->IsVisible());
    TestTrue(TEXT("Empty table visual preview"), Capture(1280, 720, TEXT("FacilityCodexEmpty720.png")));

    // Keep the synthetic 2/120-item cases above. These last previews use every enabled,
    // usable authored recipe and real PDA presentation, evaluated against an empty placement
    // fixture. They neither read nor write the player's discovery history or game save.
    const auto* SynergySettings = GetDefault<UFF_FacilitySynergySettings>();
    TStrongObjectPtr<UDataTable> ProductionTable(SynergySettings->SynergyTable.LoadSynchronous());
    if (TestNotNull(TEXT("Configured production synergy table"), ProductionTable.Get()) &&
        TestTrue(TEXT("Production table uses the synergy row schema"),
            ProductionTable->GetRowStruct() == FFF_FacilitySynergyRow::StaticStruct()))
    {
        TArray<FFF_FacilitySynergyRule> ProductionRules;
        for (const auto& Pair : ProductionTable->GetRowMap())
        {
            const auto& Row = *reinterpret_cast<const FFF_FacilitySynergyRow*>(Pair.Value);
            if (Row.bEnabled && Row.IsUsable()) ProductionRules.Add({Pair.Key, Row});
        }
        const int32 EnabledUsableCount = ProductionRules.Num();
        TestTrue(TEXT("Production table contains enabled usable recipes"), EnabledUsableCount > 0);
        FFF_FacilityCodexModel ProductionModel;
        ProductionModel.SetRules(MoveTemp(ProductionRules), SynergySettings->MaxTotalBonusPercent);
        ProductionModel.Evaluate(FFFZSReadModel(), TSet<FName>());
        TArray<FFF_FacilityCodexEntry> ProductionEntries = ProductionModel.GetEntries();
        TArray<TStrongObjectPtr<UFFZSFacilityDefinition>> KeptDefinitions;
        TArray<TStrongObjectPtr<UTexture2D>> KeptThumbnails;
        auto LoadProductionDefinition = [&](FName Id, FName RuleId) {
            const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(
                FPrimaryAssetId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Id));
            auto* Asset = Cast<UFFZSFacilityDefinition>(Path.TryLoad());
            if (TestNotNull(FString::Printf(TEXT("Recipe %s resolves facility %s (%s)"),
                *RuleId.ToString(), *Id.ToString(), *Path.ToString()), Asset))
            {
                KeptDefinitions.Emplace(Asset);
                if (!Asset->CatalogThumbnail.IsNull())
                {
                    UTexture2D* Texture = Asset->CatalogThumbnail.LoadSynchronous();
                    TestNotNull(FString::Printf(TEXT("%s loads authored thumbnail %s"),
                        *Asset->GetPathName(), *Asset->CatalogThumbnail.ToString()), Texture);
                    if (Texture) KeptThumbnails.Emplace(Texture);
                }
            }
            return Asset;
        };
        for (auto& Entry : ProductionEntries)
        {
            if (const auto* Asset = LoadProductionDefinition(Entry.TargetFacilityId, Entry.RuleId))
            {
                Entry.TargetName = Asset->Spec.DisplayName;
                Entry.TargetThumbnail = Asset->CatalogThumbnail;
                Entry.TargetCategory = Asset->Spec.Category;
            }
            if (const auto* Asset = LoadProductionDefinition(Entry.PartnerFacilityId, Entry.RuleId))
            {
                Entry.PartnerName = Asset->Spec.DisplayName;
                Entry.PartnerThumbnail = Asset->CatalogThumbnail;
            }
            TestTrue(FString::Printf(TEXT("Recipe %s remains undiscovered and unapplied in the empty placement fixture"),
                *Entry.RuleId.ToString()), !Entry.bDiscovered && !Entry.bConditionMet && !Entry.bEffectApplied &&
                !Entry.bTargetInstalled && !Entry.bPartnerInstalled && Entry.AppliedBonusPercent == 0.f);
        }
        Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::All);
        Screen->SetCategoryFilter(false, EFFZSFacilityCategory::Booth);
        Screen->SetFacilityFilter(NAME_None);
        Screen->SetSearchText(FText::GetEmpty());
        Screen->SetPreviewEntries(ProductionEntries);
        TestEqual(TEXT("Every enabled usable production recipe reaches the visible list"),
            Screen->GetVisibleEntryCount(), EnabledUsableCount);
        auto* TargetName = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_TargetName")));
        auto* PartnerName = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_PartnerName")));
        auto* TargetImage = Cast<UFF_FacilityCodexImage>(Screen->GetWidgetFromName(TEXT("Image_Target")));
        auto* PartnerImage = Cast<UFF_FacilityCodexImage>(Screen->GetWidgetFromName(TEXT("Image_Partner")));
        if (TestNotNull(TEXT("Production detail target name"), TargetName) &&
            TestNotNull(TEXT("Production detail partner name"), PartnerName) &&
            TestNotNull(TEXT("Production detail target image"), TargetImage) &&
            TestNotNull(TEXT("Production detail partner image"), PartnerImage))
        {
            for (UObject* Object : Tile->GetListItems())
            {
                const auto* Item = CastChecked<UFF_FacilityCodexItem>(Object);
                const auto& Entry = Item->Entry;
                Tile->SetSelectedItem(Object);
                const FString Context = FString::Printf(TEXT("Production recipe %s"), *Entry.RuleId.ToString());
                TestTrue(Context + TEXT(" displays its authored title"), Detail->GetText().EqualTo(Entry.DisplayName));
                TestTrue(Context + TEXT(" displays both PDA names"), TargetName->GetText().EqualTo(Entry.TargetName) &&
                    PartnerName->GetText().EqualTo(Entry.PartnerName));
                if (Entry.TargetThumbnail.IsValid())
                    TestTrue(Context + TEXT(" displays its target PDA thumbnail"),
                        TargetImage->GetLoadedThumbnail() == Entry.TargetThumbnail.Get());
                if (Entry.PartnerThumbnail.IsValid())
                    TestTrue(Context + TEXT(" displays its partner PDA thumbnail"),
                        PartnerImage->GetLoadedThumbnail() == Entry.PartnerThumbnail.Get());
                TestEqual(Context + TEXT(" shows the uninstalled target as a silhouette"),
                    TargetImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Silhouette);
                TestEqual(Context + TEXT(" shows the uninstalled partner as a silhouette"),
                    PartnerImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Silhouette);
            }
        }
        if (!Tile->GetListItems().IsEmpty()) Tile->SetSelectedItem(Tile->GetListItems()[0]);
        Tile->ScrollToTop();
        AddInfo(FString::Printf(TEXT("ProductionCodex1080.png / ProductionCodex720.png: %d actual enabled usable "
            "recipes from %s, real PDA names/thumbnails, empty-placement fixture (undiscovered/unapplied)."),
            EnabledUsableCount, *ProductionTable->GetPathName()));
        TestTrue(TEXT("Production data empty-placement fixture at 1080p"), Capture(1920, 1080, TEXT("ProductionCodex1080.png")));
        TestTrue(TEXT("Production data empty-placement fixture at 720p"), Capture(1280, 720, TEXT("ProductionCodex720.png")));

        // A separate history fixture deliberately selects discovered production art.
        // The all-unknown captures above must remain silhouettes; they cannot prove
        // the new hero's colored/3D presentation. Only this evidence preloads its mesh.
        TArray<FFF_FacilityCodexEntry> ShowcaseEntries = ProductionEntries;
        const int32 ShowcaseIndex = ShowcaseEntries.IndexOfByPredicate([&](const FFF_FacilityCodexEntry& Entry)
        {
            return Entry.TargetFacilityId == Burger->Spec.FacilityDefinitionId ||
                Entry.TargetFacilityId == IceCream->Spec.FacilityDefinitionId;
        });
        if (TestTrue(TEXT("Production recipes include a real burger or ice-cream showcase target"), ShowcaseIndex != INDEX_NONE))
        {
            auto& ShowcaseEntry = ShowcaseEntries[ShowcaseIndex];
            ShowcaseEntry.bDiscovered = true;
            ShowcaseEntry.bDiscoveryRewardClaimed = false;
            const UFFZSFacilityDefinition* ShowcaseDefinition =
                ShowcaseEntry.TargetFacilityId == Burger->Spec.FacilityDefinitionId ? Burger : IceCream;
            if (UStaticMesh* Mesh = ShowcaseDefinition->Spec.VisualMesh.LoadSynchronous())
            {
                // This evidence capture introduces its mesh after the UI material warm-up.
                // Complete only this facility's mesh and source materials/textures, so a
                // temporary editor default material cannot masquerade as the real preview.
                UStaticMesh* Meshes[] = {Mesh};
                FStaticMeshCompilingManager::Get().FinishCompilation(Meshes);
                if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
                {
                    TArray<UTexture*> Textures;
                    for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
                    {
                        if (UMaterialInterface* Material = Slot.MaterialInterface)
                        {
                            Material->EnsureIsComplete();
                            TArray<UTexture*> UsedTextures;
                            Material->GetUsedTextures(UsedTextures);
                            for (UTexture* Texture : UsedTextures)
                                if (Texture) Textures.AddUnique(Texture);
                        }
                    }
                    FTextureCompilingManager::Get().FinishCompilation(Textures);
                    for (UTexture* Texture : Textures)
                    {
                        Texture->SetForceMipLevelsToBeResident(30.f);
                        Texture->WaitForStreaming();
                    }
                    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
                    FlushRenderingCommands();
                }
            }
            Screen->SetPreviewEntries(ShowcaseEntries);
            for (UObject* Object : Tile->GetListItems())
            {
                if (CastChecked<UFF_FacilityCodexItem>(Object)->Entry.RuleId == ShowcaseEntry.RuleId)
                {
                    Tile->SetSelectedItem(Object);
                    break;
                }
            }
            PreviewTarget->OnClicked.Broadcast();
            Tile->ScrollToTop();
            TestEqual(TEXT("Discovered showcase evidence selects the intended production recipe"),
                Screen->GetSelectedRuleId(), ShowcaseEntry.RuleId);
            TestTrue(TEXT("Discovered showcase evidence names its real facility"), PreviewName->GetText().EqualTo(ShowcaseEntry.TargetName));
            TestEqual(TEXT("Discovered showcase evidence has a colored fallback, not an unknown silhouette"),
                ShowcaseImage->GetDiscoveryVisualState(), EFF_FacilityCodexDiscoveryVisualState::Color);
            if (!ShowcaseDefinition->Spec.VisualMesh.IsNull())
                TestTrue(TEXT("Discovered showcase requests the exact production display mesh"),
                    ShowcaseViewport->GetRequestedMesh() == ShowcaseDefinition->Spec.VisualMesh);
            TestTrue(TEXT("Discovered production showcase visual preview"),
                Capture(1920, 1080, TEXT("FacilityCodexShowcaseDiscovered1080.png")));
        }
    }
    int32 CloseCount = 0;
    Screen->OnCloseRequested.AddLambda([&CloseCount]() { ++CloseCount; });
    Close->OnClicked.Broadcast();
    TestEqual(TEXT("Actual close button dispatches once"), CloseCount, 1);
    Screen->OnCloseRequested.Clear();
    return true;
}
#endif
