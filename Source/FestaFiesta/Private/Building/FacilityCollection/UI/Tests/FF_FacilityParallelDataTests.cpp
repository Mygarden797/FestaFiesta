// Protect the selection/filter data contract while the collection layout changes.
// This fixture uses preview entries only and never writes discovery or reward saves.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Building/FacilityCollection/UI/FF_FacilityCard.h"
#include "Building/FacilityCollection/UI/FF_FacilityScreen.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/SVirtualWindow.h"

namespace
{
bool RunParallelDataTest(FAutomationTestBase& Test, const TCHAR* ScreenClassPath)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityCodexScreen>(nullptr,
        ScreenClassPath));
    if (!Test.TestNotNull(TEXT("Editor world"), World) || !Test.TestNotNull(TEXT("Saved codex WBP"), Class)) return false;
    const bool Previous = Class->bCanCallInitializedWithoutPlayerContext;
    Class->bCanCallInitializedWithoutPlayerContext = true;
    TStrongObjectPtr<UFF_FacilityCodexScreen> Screen(CreateWidget<UFF_FacilityCodexScreen>(World, Class));
    Class->bCanCallInitializedWithoutPlayerContext = Previous;
    if (!Test.TestNotNull(TEXT("Transient codex instance"), Screen.Get())) return false;
    const TSharedRef<SWidget> Slate = Screen->TakeWidget();
    (void)Slate;
    auto* List = Cast<UFF_FacilityCodexTileView>(Screen->GetWidgetFromName(TEXT("TileView_Entries")));
    auto* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailTitle")));
    auto* Browse = Screen->GetWidgetFromName(TEXT("Size_Browse"));
    auto* Detail = Screen->GetWidgetFromName(TEXT("Size_Detail"));
    auto* Showcase = Screen->GetWidgetFromName(TEXT("Size_Showcase"));
    auto* Rotation = Screen->GetWidgetFromName(TEXT("Horizontal_PreviewRotation"));
    auto* Open3D = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_ApprovedOpen3D")));
    auto* Close3D = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_ApprovedClose3D")));
    auto* PreviewHint = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_PreviewHint")));
    auto* StatusSummary = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_ApprovedSummaryStatus")));
    auto* ConditionSummary = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_ApprovedSummaryCondition")));
    auto* EffectSummary = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_ApprovedSummaryEffect")));
    auto* Parallel = FindFProperty<FBoolProperty>(Class, TEXT("bUseParallelCollectionLayout"));
    if (!Test.TestNotNull(TEXT("Real virtualized list"), List) || !Test.TestNotNull(TEXT("Detail title"), Title) ||
        !Test.TestNotNull(TEXT("Browse region"), Browse) || !Test.TestNotNull(TEXT("Detail region"), Detail) ||
        !Test.TestNotNull(TEXT("Preview region"), Showcase) || !Test.TestNotNull(TEXT("Preview controls"), Rotation) ||
        !Test.TestNotNull(TEXT("3D open action"), Open3D) || !Test.TestNotNull(TEXT("3D close action"), Close3D) ||
        !Test.TestNotNull(TEXT("Preview status hint"), PreviewHint) ||
        !Test.TestNotNull(TEXT("Parallel status summary"), StatusSummary) ||
        !Test.TestNotNull(TEXT("Parallel condition summary"), ConditionSummary) ||
        !Test.TestNotNull(TEXT("Parallel effect summary"), EffectSummary) ||
        !Test.TestNotNull(TEXT("Parallel layout setting"), Parallel)) return false;
    Parallel->SetPropertyValue_InContainer(Screen.Get(), true);
    if (!Test.TestTrue(TEXT("Slate is initialized"), FSlateApplication::IsInitialized())) return false;
    FSlateApplication& App = FSlateApplication::Get();
    const TSharedPtr<SWidget> PreviousFocus = App.GetKeyboardFocusedWidget();
    const TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(FVector2D(1584, 864));
    Window->SetContent(Slate);
    App.RegisterVirtualWindow(Window);
    ON_SCOPE_EXIT
    {
        App.ClearKeyboardFocus(EFocusCause::Cleared);
        App.UnregisterVirtualWindow(Window);
        if (PreviousFocus) App.SetKeyboardFocus(PreviousFocus);
    };
    Window->SlatePrepass();

    FFF_FacilityCodexEntry Active;
    Active.RuleId = TEXT("ParallelTest_Active");
    Active.DisplayName = FText::FromString(TEXT("활성 조합"));
    Active.TargetFacilityId = TEXT("ParallelTest_TargetA");
    Active.PartnerFacilityId = TEXT("ParallelTest_PartnerA");
    Active.TargetName = FText::FromString(TEXT("시설 A"));
    Active.PartnerName = FText::FromString(TEXT("시설 B"));
    Active.bDiscovered = true;
    Active.bConditionMet = true;
    Active.bEffectApplied = true;
    Active.DiscoveryRewardGold = 100;
    Active.bDiscoveryRewardClaimed = false;

    FFF_FacilityCodexEntry Claimed = Active;
    Claimed.RuleId = TEXT("ParallelTest_Claimed");
    Claimed.DisplayName = FText::FromString(TEXT("보상 완료 조합"));
    Claimed.TargetFacilityId = TEXT("ParallelTest_TargetC");
    Claimed.bConditionMet = false;
    Claimed.bEffectApplied = false;
    Claimed.bDiscoveryRewardClaimed = true;

    FFF_FacilityCodexEntry Hidden = Active;
    Hidden.RuleId = TEXT("ParallelTest_Undiscovered");
    Hidden.DisplayName = FText::FromString(TEXT("미발견 조합"));
    Hidden.bDiscovered = false;
    Hidden.bConditionMet = false;
    Hidden.bEffectApplied = false;
    Hidden.bDiscoveryRewardClaimed = false;
    Screen->SetPreviewEntries({Active, Claimed, Hidden});
    Test.TestEqual(TEXT("All preview entries remain available"), Screen->GetVisibleEntryCount(), 3);
    Test.TestEqual(TEXT("The first item is selected without persisting profile data"), Screen->GetSelectedRuleId(), Active.RuleId);
    Test.TestEqual(TEXT("The detail projects the selected entry"), Title->GetText().ToString(), Active.DisplayName.ToString());
    Test.TestTrue(TEXT("List and selected detail are simultaneously visible"), Browse->IsVisible() && Detail->IsVisible());
    Test.TestTrue(TEXT("The summary separates discovery, placement and actual effect"),
        StatusSummary->GetText().ToString().Contains(TEXT("발견 상태  발견")) &&
        StatusSummary->GetText().ToString().Contains(TEXT("배치 조건  충족")) &&
        StatusSummary->GetText().ToString().Contains(TEXT("현재 효과  적용 중")) &&
        !ConditionSummary->GetText().IsEmpty() && !EffectSummary->GetText().IsEmpty());
    Test.TestTrue(TEXT("A discovered entry can open its preview"), Open3D->GetIsEnabled());
    Open3D->OnClicked.Broadcast();
    Test.TestTrue(TEXT("Preview action reveals the separate showcase and controls"), Showcase->IsVisible() && Rotation->IsVisible());
    Test.TestFalse(TEXT("Separate preview does not overlay the list"), Browse->IsVisible());
    Test.TestTrue(TEXT("A discovered entry without a model explains the image fallback"),
        PreviewHint->GetText().ToString().Contains(TEXT("3D 모델이 없어")));
    const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(1584, 864), FSlateLayoutTransform(1.f));
    Test.TestTrue(TEXT("Escape closes the separate preview"), Slate->OnPreviewKeyDown(Geometry, Escape).IsEventHandled());
    Window->SlatePrepass();
    Test.TestTrue(TEXT("Escape returns to the list and selected detail"), Browse->IsVisible() && Detail->IsVisible() && !Showcase->IsVisible());
    Test.TestEqual(TEXT("Closing preview retains the recipe"), Screen->GetSelectedRuleId(), Active.RuleId);
    Test.TestTrue(TEXT("Closing preview restores keyboard focus to the list"), List->HasKeyboardFocus() || List->HasFocusedDescendants());
    Screen->OpenCombinationDetail(Claimed.RuleId);
    Test.TestEqual(TEXT("Selecting another recipe updates the same detail"), Screen->GetSelectedRuleId(), Claimed.RuleId);
    Test.TestEqual(TEXT("The selected detail title changes"), Title->GetText().ToString(), Claimed.DisplayName.ToString());
    Test.TestTrue(TEXT("A discovered but inactive selection shows condition and effect independently"),
        StatusSummary->GetText().ToString().Contains(TEXT("발견 상태  발견")) &&
        StatusSummary->GetText().ToString().Contains(TEXT("배치 조건  미충족")) &&
        StatusSummary->GetText().ToString().Contains(TEXT("현재 효과  미적용")));
    Test.TestTrue(TEXT("Selecting a detail does not hide the list"), Browse->IsVisible() && Detail->IsVisible());

    Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::Undiscovered);
    Test.TestEqual(TEXT("The undiscovered recipe is selectable"), Screen->GetSelectedRuleId(), Hidden.RuleId);
    Test.TestFalse(TEXT("Undiscovered entries cannot open 3D preview"), Open3D->GetIsEnabled());
    Test.TestTrue(TEXT("Undiscovered status is not mistaken for an inactive discovered recipe"),
        StatusSummary->GetText().ToString().Contains(TEXT("발견 상태  미발견")));

    Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::Active);
    Test.TestEqual(TEXT("Only the currently applied recipe is active"), Screen->GetVisibleEntryCount(), 1);
    Test.TestEqual(TEXT("Filtering retains the matching selection"), Screen->GetSelectedRuleId(), Active.RuleId);
    Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::RewardReady);
    Test.TestEqual(TEXT("Claimed and undiscovered recipes are not claimable"), Screen->GetVisibleEntryCount(), 1);
    const auto* RewardItem = List->GetListItems().IsEmpty() ? nullptr : Cast<UFF_FacilityCodexItem>(List->GetListItems()[0]);
    if (!Test.TestNotNull(TEXT("Reward filter uses the real item model"), RewardItem)) return false;
    Test.TestTrue(TEXT("Reward amount and claimed state are not conflated with discovery or current effect"),
        RewardItem->Entry.RuleId == Active.RuleId && RewardItem->Entry.DiscoveryRewardGold == 100 &&
        !RewardItem->Entry.bDiscoveryRewardClaimed && RewardItem->Entry.bDiscovered && RewardItem->Entry.bEffectApplied);

    Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::All);
    Screen->SetSearchText(FText::FromString(TEXT("보상 완료")));
    Test.TestEqual(TEXT("Search narrows the visible recipes"), Screen->GetVisibleEntryCount(), 1);
    Test.TestEqual(TEXT("Detail follows the remaining selected recipe"), Screen->GetSelectedRuleId(), Claimed.RuleId);
    Test.TestEqual(TEXT("Detail title follows selection"), Title->GetText().ToString(), Claimed.DisplayName.ToString());
    Screen->SetSearchText(FText::FromString(TEXT("없는 조합")));
    Test.TestEqual(TEXT("An empty search has no visible items"), Screen->GetVisibleEntryCount(), 0);
    Test.TestTrue(TEXT("An empty search clears stale selection"), Screen->GetSelectedRuleId().IsNone());
    Test.TestTrue(TEXT("Empty results keep browsing controls visible"), Browse->IsVisible());
    Test.TestFalse(TEXT("Empty results hide stale detail"), Detail->IsVisible());
    Screen->SetSearchText(FText::GetEmpty());
    Test.TestEqual(TEXT("Clearing search restores every entry"), Screen->GetVisibleEntryCount(), 3);
    Screen->ReleaseSlateResources(true);
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexParallelDataTest,
    "FestaFiesta.FacilityCodex.UI.ParallelSelectionAndFilterData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexParallelDataTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    return RunParallelDataTest(*this, FFBuildUIAssetPaths::FacilityCodexScreenClass);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexParallelDraftDataTest,
    "FestaFiesta.FacilityCodex.UI.ParallelDraftSelectionAndFilterData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexParallelDraftDataTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    return RunParallelDataTest(*this,
        TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Drafts/WBP_FF_FacilityParallelDraft.WBP_FF_FacilityParallelDraft_C"));
}
#endif
