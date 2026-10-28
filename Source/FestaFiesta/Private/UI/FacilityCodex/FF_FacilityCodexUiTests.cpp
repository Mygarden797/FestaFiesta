// FF_FacilityCodexUiTests.cpp
// 역할: 시설 도감의 동작과 저장된 화면 연결을 자동 검증한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
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
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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
    auto* InspectorClass = Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(FFBuildUIAssetPaths::FacilityInfoPanelClass).TryLoadClass<UUserWidget>());
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
    for (const FName Name : { TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"), TEXT("Button_Active") })
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
    if (Screen->GetWidgetFromName(TEXT("Column_CollectionCategories")))
    {
        for (const TCHAR* Name : {TEXT("Text_Empty"), TEXT("Border_RewardFeedback"), TEXT("Size_Detail"), TEXT("Size_Showcase")})
        {
            const auto* Template = ScreenClass->GetWidgetTreeArchetype()->FindWidget(Name);
            TestTrue(FString::Printf(TEXT("Saved Designer list hides conditional widget %s"), Name),
                Template && Template->bHiddenInDesigner);
        }
        // The Pearl collection has separate list and detail pages. Exercise the
        // current saved widget through that navigation instead of requiring the
        // retired floating inspector to paint while the list page is active.
        auto* Browse = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_Browse")));
        auto* ExitCodex = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_ExitCodex")));
        auto* DetailPage = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_Detail")));
        auto* Showcase = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_Showcase")));
        auto* Amount = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailRewardAmount")));
        auto* PreviewName = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_PreviewFacilityName")));
        auto* PreviewTarget = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_PreviewTarget")));
        auto* PreviewPartner = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_PreviewPartner")));
        auto* Claim = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_DetailClaimReward")));
        auto* Claimed = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Border_DetailRewardClaimed")));
        auto* Locked = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Border_DetailRewardLocked")));
        auto* RewardBounds = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_DetailRewardState")));
        auto* ClaimLabel = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailClaimReward")));
        auto* RewardHeading = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailRewardLabel")));
        auto* RewardChevron = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailRewardChevron")));
        auto* ClaimedLabel = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailRewardClaimed")));
        auto* LockedLabel = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailRewardLocked")));
        auto* DetailGold = Cast<UImage>(Screen->GetWidgetFromName(TEXT("Image_DetailRewardGold")));
        auto* CardGold = Cast<UImage>(CardClass->GetWidgetTreeArchetype()->FindWidget(TEXT("Image_CardRewardGold")));
        if (!TestNotNull(TEXT("Top-right codex back button"), ExitCodex) ||
            !TestNotNull(TEXT("Pearl collection browse"), Browse) ||
            !TestNotNull(TEXT("Separate detail page"), DetailPage) ||
            !TestNotNull(TEXT("Separate showcase"), Showcase) ||
            !TestNotNull(TEXT("Reward amount"), Amount) ||
            !TestNotNull(TEXT("Preview name"), PreviewName) ||
            !TestNotNull(TEXT("Preview target"), PreviewTarget) ||
            !TestNotNull(TEXT("Preview partner"), PreviewPartner) ||
            !TestNotNull(TEXT("Detail claim action"), Claim) ||
            !TestNotNull(TEXT("Claimed badge"), Claimed) ||
            !TestNotNull(TEXT("Locked badge"), Locked) ||
            !TestNotNull(TEXT("Reward state bounds"), RewardBounds) ||
            !TestNotNull(TEXT("Detail claim label"), ClaimLabel) ||
            !TestNotNull(TEXT("Discovery reward heading"), RewardHeading) ||
            !TestNotNull(TEXT("Reward row chevron"), RewardChevron) ||
            !TestNotNull(TEXT("Claimed reward receipt"), ClaimedLabel) ||
            !TestNotNull(TEXT("Locked reward explanation"), LockedLabel) ||
            !TestNotNull(TEXT("Detail Gold icon"), DetailGold) ||
            !TestNotNull(TEXT("Card Gold icon"), CardGold)) return false;
        TestTrue(TEXT("Pearl detail reward states fill their parent and reserve a 72-high row"),
            !RewardBounds->IsWidthOverride() && FMath::IsNearlyEqual(RewardBounds->GetHeightOverride(), 72.f));
        TestTrue(TEXT("Discovery reward heading, amount, and chevron have a clear hierarchy"),
            RewardHeading->GetFont().Size == 18 && ClaimLabel->GetFont().Size == 18 &&
            RewardChevron->GetFont().Size == 27);
        auto* ClaimIcon = Cast<UImage>(Screen->GetWidgetFromName(TEXT("Image_DetailClaimReward")));
        if (!TestNotNull(TEXT("The full-width claim row retains its Gold icon"), ClaimIcon)) return false;
        TestTrue(TEXT("The detail claim Gold icon persists its readable 32 by 32 size"),
            FMath::IsNearlyEqual(ClaimIcon->GetBrush().ImageSize.X, 32.f) &&
            FMath::IsNearlyEqual(ClaimIcon->GetBrush().ImageSize.Y, 32.f));
        for (const TCHAR* Name : {TEXT("Image_DetailRewardClaimed"), TEXT("Image_DetailRewardLocked")})
        {
            auto* StateIcon = Cast<UImage>(Screen->GetWidgetFromName(Name));
            if (!TestNotNull(Name, StateIcon)) return false;
            TestTrue(TEXT("Receipt and locked rows use the same 32 by 32 Gold badge"),
                FMath::IsNearlyEqual(StateIcon->GetBrush().ImageSize.X, 32.f) &&
                FMath::IsNearlyEqual(StateIcon->GetBrush().ImageSize.Y, 32.f));
        }
        TestFalse(TEXT("Collection opens on its list page"), Screen->IsCombinationDetailOpen());
        TestEqual(TEXT("Collection persists the requested six fixed columns"), CastChecked<UFF_FacilityCodexTileView>(Tile)->ColumnCount, 6);
        TestNotNull(TEXT("Reward track uses a saved Fill container without relying on native Tick"),
            Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Scroll_ReadableMilestones"))));
        TestTrue(TEXT("List shows the recipe grid and hides the separate detail"),
            Browse->IsVisible() && !DetailPage->IsVisible() && !Showcase->IsVisible());

        Screen->OpenCombinationDetail(Entries[0].RuleId);
        TestTrue(TEXT("Card activation opens the separate detail"), Screen->IsCombinationDetailOpen());
        TestTrue(TEXT("Detail replaces the list rather than covering it"),
            !Browse->IsVisible() && DetailPage->IsVisible() && Showcase->IsVisible());
        TestTrue(TEXT("Discovered recipe shows its facility and exact Gold reward"),
            PreviewName->GetText().EqualTo(Entries[0].TargetName) &&
            Amount->GetText().ToString().Contains(TEXT("450")) &&
            Claim->IsVisible() && Claim->GetIsEnabled() && !Claimed->IsVisible() && !Locked->IsVisible());
        TestTrue(TEXT("The discovery reward row names its action and exact payment"),
            RewardHeading->GetText().ToString() == TEXT("발견 보상") &&
            ClaimLabel->GetText().ToString() == TEXT("450 골드") &&
            RewardChevron->GetText().ToString() == TEXT("›"));
        TestTrue(TEXT("The saved Gold artwork remains compact after WBP migration"),
            DetailGold->GetBrush().ImageSize.X <= 30.f && CardGold->GetBrush().ImageSize.X <= 22.f);
        PreviewPartner->OnClicked.Broadcast();
        TestTrue(TEXT("Partner preview follows the selected recipe"), PreviewName->GetText().EqualTo(Entries[0].PartnerName));
        PreviewTarget->OnClicked.Broadcast();
        TestTrue(TEXT("Target preview can be restored"), PreviewName->GetText().EqualTo(Entries[0].TargetName));
        Claim->OnClicked.Broadcast();
        TestFalse(TEXT("Preview claim cannot mark a reward paid"),
            CastChecked<UFF_FacilityCodexItem>(Tile->GetSelectedItem())->Entry.bDiscoveryRewardClaimed);

        Screen->OpenCombinationDetail(Entries[1].RuleId);
        TestTrue(TEXT("Unknown recipe shows its own locked reward and target"),
            PreviewName->GetText().EqualTo(Entries[1].TargetName) &&
            Amount->GetText().ToString().Contains(FText::AsNumber(1250).ToString()) &&
            Locked->IsVisible() && !Claim->IsVisible() && !Claimed->IsVisible() &&
            LockedLabel->GetText().ToString() == TEXT("1,250 골드 · 미발견"));
        auto Changed = Entries;
        Changed[1].DiscoveryRewardGold = 0;
        Screen->SetPreviewEntries(Changed);
        Screen->OpenCombinationDetail(Changed[1].RuleId);
        TestTrue(TEXT("A zero reward clears the earlier amount and all reward actions"),
            Amount->GetText().ToString().Contains(TEXT("보상 없음")) &&
            !Amount->GetText().ToString().Contains(FText::AsNumber(1250).ToString()) &&
            !RewardBounds->IsVisible());
        Changed = Entries;
        Changed[0].bDiscoveryRewardClaimed = true;
        Screen->SetPreviewEntries(Changed);
        Screen->OpenCombinationDetail(Changed[0].RuleId);
        TestTrue(TEXT("A claimed reward keeps its amount but removes the action"),
            Amount->GetText().ToString().Contains(TEXT("450")) &&
            Claimed->IsVisible() && !Claim->IsVisible() && !Claim->GetIsEnabled() &&
            ClaimedLabel->GetText().ToString() == TEXT("450 골드 · 수령 완료"));

        Close->OnClicked.Broadcast();
        TestFalse(TEXT("Back returns to the collection list"), Screen->IsCombinationDetailOpen());
        int32 ExitRequests = 0;
        const FDelegateHandle ExitObserver = Screen->OnCloseRequested.AddLambda([&ExitRequests]() { ++ExitRequests; });
        const auto* BackLabel = Cast<UTextBlock>(ExitCodex->GetContent());
        TestTrue(TEXT("The former X advertises back navigation on the saved widget"),
            BackLabel && BackLabel->GetText().ToString() == TEXT("‹") &&
            ExitCodex->GetToolTipText().ToString() == TEXT("뒤로가기 · Esc"));
        Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::Discovered);
        Screen->OpenCombinationDetail(Entries[0].RuleId);
        ExitCodex->OnClicked.Broadcast();
        TestFalse(TEXT("The top-right back button returns from detail to the list"), Screen->IsCombinationDetailOpen());
        TestEqual(TEXT("Detail back keeps the codex modal open"), ExitRequests, 0);
        TestEqual(TEXT("Detail back retains the selected recipe"), Screen->GetSelectedRuleId(), Entries[0].RuleId);
        TestEqual(TEXT("Detail back preserves the discovery filter"), Screen->GetVisibleEntryCount(), 1);
        TestTrue(TEXT("Detail back restores the recipe list"), Browse->IsVisible() && !DetailPage->IsVisible());
        ExitCodex->OnClicked.Broadcast();
        TestEqual(TEXT("Back on the list requests returning to the previous screen once"), ExitRequests, 1);
        Screen->OnCloseRequested.Remove(ExitObserver);
        Screen->SetPreviewEntries(Entries);
        Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::RewardReady);
        TestEqual(TEXT("Reward filter lists only the discovered unpaid recipe"), Screen->GetVisibleEntryCount(), 1);
        Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::All);
        TestEqual(TEXT("All filter restores both recipes"), Screen->GetVisibleEntryCount(), 2);

        auto* CollectionSearchHint = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_SearchHint")));
        if (!TestNotNull(TEXT("Collection retains its full-contrast authored search hint"), CollectionSearchHint)) return false;
        TestTrue(TEXT("Only the compact authored hint paints in an empty collection search"),
            Search->GetHintText().IsEmpty() &&
            CollectionSearchHint->GetText().ToString() == TEXT("시설·조합 검색") &&
            CollectionSearchHint->GetVisibility() == ESlateVisibility::HitTestInvisible);
        Search->SetText(FText::FromString(TEXT("no_matching_recipe")));
        Search->OnTextChanged.Broadcast(Search->GetText());
        TestEqual(TEXT("A collection search hides the authored hint while typing"),
            CollectionSearchHint->GetVisibility(), ESlateVisibility::Collapsed);
        TestEqual(TEXT("Collection search input filters the recipes"), Screen->GetVisibleEntryCount(), 0);
        Screen->SetSearchText(FText::GetEmpty());
        TestEqual(TEXT("Clearing collection search restores the recipes"), Screen->GetVisibleEntryCount(), 2);
        TestTrue(TEXT("Clearing collection search restores exactly one passive hint"),
            Search->GetHintText().IsEmpty() &&
            CollectionSearchHint->GetVisibility() == ESlateVisibility::HitTestInvisible);

        if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
        {
            // This current-layout branch returns before the legacy fixture's
            // shader warmup. Prepare its real materials before judging contrast
            // or exporting images, including when this test runs on its own.
            TSet<UMaterialInterface*> Materials;
            const auto GatherBrush = [&Materials](const FSlateBrush& Brush)
            {
                if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) Materials.Add(Material);
            };
            const auto GatherMaterials = [&Materials, &GatherBrush](UWidget* Widget)
            {
                if (const auto* Border = Cast<UBorder>(Widget)) GatherBrush(Border->Background);
                if (const auto* Button = Cast<UButton>(Widget))
                {
                    GatherBrush(Button->GetStyle().Normal); GatherBrush(Button->GetStyle().Hovered);
                    GatherBrush(Button->GetStyle().Pressed); GatherBrush(Button->GetStyle().Disabled);
                }
                if (const auto* Image = Cast<UImage>(Widget)) GatherBrush(Image->GetBrush());
                if (const auto* Image = Cast<UFF_FacilityCodexImage>(Widget))
                    if (Image->DiscoveryMaterial) Materials.Add(Image->DiscoveryMaterial);
            };
            Screen->WidgetTree->ForEachWidget(GatherMaterials);
            CardClass->GetWidgetTreeArchetype()->ForEachWidget(GatherMaterials);
            for (UMaterialInterface* Material : Materials) Material->EnsureIsComplete();
            if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
            FlushRenderingCommands();
            const auto SaveCollectionPreview = [this](UTextureRenderTarget2D& Target, const FIntPoint Resolution, const TCHAR* Page)
            {
                TArray<FColor> Pixels;
                FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
                if (!Target.GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) return false;
                TArray64<uint8> Png;
                FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
                const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityCodex");
                IFileManager::Get().MakeDirectory(*Directory, true);
                return TestTrue(TEXT("Current collection preview saved"), FFileHelper::SaveArrayToFile(Png,
                    *(Directory / FString::Printf(TEXT("ReadableCollection_%s_%dx%d.png"), Page, Resolution.X, Resolution.Y))));
            };
            for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1024, 768), FIntPoint(720, 960), FIntPoint(1920, 1080)})
            {
                FWidgetRenderer Renderer(false, true);
                const FVector2D Size(Resolution.X, Resolution.Y);
                const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
                TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
                if (!TestNotNull(TEXT("Pearl collection render target"), Target.Get())) return false;
                auto Window = SNew(SVirtualWindow).Size(Size);
                Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
                FHittestGrid Grid;
                for (int32 Frame = 0; Frame < 8; ++Frame)
                {
                    Screen->ForceLayoutPrepass();
                    Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
                }
                const FSlateRect Viewport(0, 0, Resolution.X, Resolution.Y);
                const FSlateRect ScreenRect = Screen->GetWidgetFromName(TEXT("Size_Dialog"))->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Authored reference dialog stays inset and preserves its composition at every resolution"),
                    ScreenRect.Left > 0.f && ScreenRect.Top > 0.f &&
                    ScreenRect.Right < Resolution.X && ScreenRect.Bottom < Resolution.Y &&
                    FMath::IsNearlyEqual(ScreenRect.GetSize().X / ScreenRect.GetSize().Y, 1584.f / 864.f, .002f));
                const FSlateRect BrowseRect = Browse->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect TileRect = Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("The collection list and cards remain inside the viewport"),
                    BrowseRect.Left >= -1.f && BrowseRect.Top >= -1.f &&
                    BrowseRect.Right <= Viewport.Right + 1.f && BrowseRect.Bottom <= Viewport.Bottom + 1.f &&
                    TileRect.Left >= BrowseRect.Left - 1.f && TileRect.Right <= BrowseRect.Right + 1.f);
                TestTrue(TEXT("The separate detail stays hidden while browsing"), !DetailPage->IsVisible());
                const FSlateRect FilterRect = Screen->GetWidgetFromName(TEXT("Horizontal_Toolbar"))->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect CloseRect = Close->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Collection filters occupy their own row below the title and above the list"),
                    FilterRect.Top >= CloseRect.Bottom - 1.f && FilterRect.Bottom <= BrowseRect.Top + 1.f);
                const FSlateRect ExitRect = ExitCodex->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                AddInfo(FString::Printf(TEXT("Codex bounds %dx%d DPI=%.9f: Back=(%.3f,%.3f)-(%.3f,%.3f), size=(%.9f,%.9f); titleRight=%.3f filterTop=%.3f browseRight=%.3f"),
                    Resolution.X, Resolution.Y, Dpi, ExitRect.Left, ExitRect.Top, ExitRect.Right, ExitRect.Bottom,
                    ExitRect.GetSize().X, ExitRect.GetSize().Y, CloseRect.Right, FilterRect.Top, BrowseRect.Right));
                TestTrue(TEXT("Back remains visible at the top-right above the filters at every resolution"),
                    ExitCodex->IsVisible() && ExitRect.GetSize().GetMin() >= 31.95f &&
                    ExitRect.Right < ScreenRect.Right && ExitRect.Left > ScreenRect.Left &&
                    ExitRect.Top >= 0.f && ExitRect.Bottom < FilterRect.Top && ExitRect.Left > CloseRect.Right);
                // Click the enlarged edge, not merely its old visual center. This
                // catches a minimum-size decoration that does not enlarge hit testing.
                const FVector2D ExitPoint(ExitRect.Left + 2.f, (ExitRect.Top + ExitRect.Bottom) * .5f);
                TArray<FWidgetAndPointer> ExitBubble = Grid.GetBubblePath(ExitPoint, 0.f, false);
                FWidgetPath ExitPath(MakeArrayView(ExitBubble));
                int32 PointerCloseRequests = 0;
                const FDelegateHandle PointerCloseObserver = Screen->OnCloseRequested.AddLambda(
                    [&PointerCloseRequests]() { ++PointerCloseRequests; });
                if (TestTrue(TEXT("The outer edge of the minimum 32-pixel close target is on the real hit path"),
                    ExitPath.IsValid() && ExitPath.ContainsWidget(ExitCodex->GetCachedWidget().Get())))
                {
                    const TSet<FKey> Pressed{EKeys::LeftMouseButton}, Released;
                    const FPointerEvent Move(0, ExitPoint, ExitPoint, Released, EKeys::Invalid, 0.f, FModifierKeysState());
                    const FPointerEvent Down(0, ExitPoint, ExitPoint, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                    const FPointerEvent Up(0, ExitPoint, ExitPoint, Released, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                    FSlateApplication::Get().RoutePointerMoveEvent(ExitPath, Move, false);
                    FSlateApplication::Get().RoutePointerDownEvent(ExitPath, Down);
                    FSlateApplication::Get().RoutePointerUpEvent(ExitPath, Up);
                }
                Screen->OnCloseRequested.Remove(PointerCloseObserver);
                TestEqual(TEXT("The physical close target requests one close at every tested resolution"), PointerCloseRequests, 1);
                const auto* ProgressRow = Screen->GetWidgetFromName(TEXT("Row_CollectionProgress"));
                const auto* RewardScroll = Screen->GetWidgetFromName(TEXT("Scroll_ReadableMilestones"));
                const auto* RewardTrack = Screen->GetWidgetFromName(TEXT("Size_ReadableMilestoneTrack"));
                if (!TestNotNull(TEXT("Collection reward row"), ProgressRow) ||
                    !TestNotNull(TEXT("Collection reward viewport"), RewardScroll) ||
                    !TestNotNull(TEXT("Collection reward track"), RewardTrack)) return false;
                const FSlateRect RewardViewportRect = RewardScroll->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect RewardTrackRect = RewardTrack->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Header reward track fills its own viewport without entering the filter row"),
                    RewardViewportRect.Left >= ScreenRect.Left && RewardViewportRect.Right <= ScreenRect.Right &&
                    RewardTrackRect.Right >= RewardViewportRect.Right - 1.f &&
                    RewardViewportRect.Bottom <= FilterRect.Top + 1.f && RewardViewportRect.GetSize().X > 0.f);
                const FSlateRect SearchRect = Search->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Collection search fits to the right of the filters, including portrait windows"),
                    SearchRect.Left >= FilterRect.Right + 1.f && SearchRect.Right <= ScreenRect.Right &&
                    Search->GetDesiredSize().X <= Search->GetCachedGeometry().GetLocalSize().X + 1.f &&
                    Search->GetClipping() == EWidgetClipping::ClipToBounds);
                const FSlateRect SearchHintRect = CollectionSearchHint->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("The complete authored search hint stays inside the search field at every resolution"),
                    SearchHintRect.Left >= SearchRect.Left - 1.f && SearchHintRect.Right <= SearchRect.Right + 1.f &&
                    SearchHintRect.Top >= SearchRect.Top - 1.f && SearchHintRect.Bottom <= SearchRect.Bottom + 1.f &&
                    CollectionSearchHint->GetDesiredSize().X <= CollectionSearchHint->GetCachedGeometry().GetLocalSize().X + 1.f &&
                    CollectionSearchHint->GetClipping() == EWidgetClipping::ClipToBounds);
                SaveCollectionPreview(*Target, Resolution, TEXT("List"));
                auto* FirstCard = Cast<UFF_FacilityCodexCard>(Tile->GetEntryWidgetFromItem(Tile->GetListItems()[0]));
                if (!TestNotNull(TEXT("Visible first Pearl recipe card"), FirstCard)) return false;
                auto* Name = CastChecked<UTextBlock>(FirstCard->GetWidgetFromName(TEXT("Text_Name")));
                auto* NameBounds = CastChecked<USizeBox>(FirstCard->GetWidgetFromName(TEXT("Size_Name")));
                auto* State = CastChecked<UBorder>(FirstCard->GetWidgetFromName(TEXT("Border_State")));
                const FSlateRect CardRect = FirstCard->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect NameRect = Name->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect StateRect = State->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                AddInfo(FString::Printf(TEXT("Codex card %dx%d: card=(%.3f,%.3f)-(%.3f,%.3f), title=(%.3f,%.3f)-(%.3f,%.3f), reward=(%.3f,%.3f)-(%.3f,%.3f), rewardVisibility=%d"),
                    Resolution.X, Resolution.Y, CardRect.Left, CardRect.Top, CardRect.Right, CardRect.Bottom,
                    NameRect.Left, NameRect.Top, NameRect.Right, NameRect.Bottom,
                    StateRect.Left, StateRect.Top, StateRect.Right, StateRect.Bottom, int32(State->GetVisibility())));
                auto* Bonus = CastChecked<UTextBlock>(FirstCard->GetWidgetFromName(TEXT("Text_Bonus")));
                TestFalse(TEXT("Card fee percentage remains one line inside the auto-sized reward strip"), Bonus->GetAutoWrapText());
                TestTrue(TEXT("Pearl card title has enough height to show every wrapped line"),
                    Name->GetDesiredSize().Y <= NameBounds->GetCachedGeometry().GetLocalSize().Y + 1.f);
                TestTrue(TEXT("Pearl card title and reward stay inside its card"),
                    NameRect.Bottom <= CardRect.Bottom + 1.f && StateRect.Bottom <= CardRect.Bottom + 1.f);
                auto* GoldIcon = CastChecked<UImage>(FirstCard->GetWidgetFromName(TEXT("Image_CardRewardGold")));
                const FSlateRect GoldRect = GoldIcon->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Card Gold icon paints at badge size rather than native texture size"),
                    GoldRect.GetSize().X <= 24.f * Dpi && GoldRect.GetSize().Y <= 24.f * Dpi);
                TestTrue(TEXT("Card states show the exact reward and eligibility"),
                    CastChecked<UTextBlock>(FirstCard->GetWidgetFromName(TEXT("Text_State")))
                        ->GetText().ToString().Contains(TEXT("450")) &&
                    CastChecked<UTextBlock>(FirstCard->GetWidgetFromName(TEXT("Text_State")))
                        ->GetText().ToString().Contains(TEXT("수령 가능")));
                TestNotNull(TEXT("Card Gold badge uses its saved texture"),
                    Cast<UTexture2D>(GoldIcon->GetBrush().GetResourceObject()));
                const UBorder* CardSurface = CastChecked<UBorder>(FirstCard->GetWidgetFromName(TEXT("Border_Card")));
                TestTrue(TEXT("Pearl card retains its opaque authored rounded surface"),
                    CardSurface->Background.DrawAs == ESlateBrushDrawType::RoundedBox &&
                    CardSurface->Background.TintColor.GetSpecifiedColor().A * CardSurface->GetBrushColor().A >= .95f);
                if (Resolution == FIntPoint(1280, 720))
                {
                    auto* CardClaim = CastChecked<UButton>(FirstCard->GetWidgetFromName(TEXT("Button_ClaimReward")));
                    auto* Item = CastChecked<UFF_FacilityCodexItem>(Tile->GetListItems()[0]);
                    const auto PreviousClaim = Item->OnClaimRewardRequested;
                    int32 ClaimRequests = 0;
                    Item->OnClaimRewardRequested.BindLambda([&](FName RuleId)
                    {
                        ++ClaimRequests;
                        TestEqual(TEXT("Card reward action routes its own recipe"), RuleId, Entries[0].RuleId);
                    });
                    const FGeometry& ButtonGeometry = CardClaim->GetPaintSpaceGeometry();
                    const FVector2D Point = ButtonGeometry.LocalToAbsolute(ButtonGeometry.GetLocalSize() * .5f);
                    TArray<FWidgetAndPointer> Bubble = Grid.GetBubblePath(Point, 0.f, false);
                    FWidgetPath Path(MakeArrayView(Bubble));
                    if (TestTrue(TEXT("The card reward is on the real Slate hit path"),
                        Path.IsValid() && Path.ContainsWidget(CardClaim->GetCachedWidget().Get())))
                    {
                        const TSet<FKey> Pressed{EKeys::LeftMouseButton}, Released;
                        FSlateApplication& App = FSlateApplication::Get();
                        const FPointerEvent Move(0, Point, Point, Released, EKeys::Invalid, 0.f, FModifierKeysState());
                        const FPointerEvent Down(0, Point, Point, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                        const FPointerEvent Up(0, Point, Point, Released, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                        App.RoutePointerMoveEvent(Path, Move, false);
                        TestTrue(TEXT("Card reward consumes pointer press"), App.RoutePointerDownEvent(Path, Down).IsEventHandled());
                        TestTrue(TEXT("Card reward consumes pointer release"), App.RoutePointerUpEvent(Path, Up).IsEventHandled());
                    }
                    Item->OnClaimRewardRequested = PreviousClaim;
                    TestEqual(TEXT("A pointer claim routes exactly once"), ClaimRequests, 1);
                    TestFalse(TEXT("A card claim does not also open the detail page"), Screen->IsCombinationDetailOpen());
                    const FFF_FacilityCodexEntry Original = Item->Entry;
                    auto* CompactState = CastChecked<UTextBlock>(FirstCard->GetWidgetFromName(TEXT("Text_State")));
                    auto* CardClaimedBadge = CastChecked<UBorder>(FirstCard->GetWidgetFromName(TEXT("Border_RewardClaimed")));
                    auto* CardLockedBadge = CastChecked<UBorder>(FirstCard->GetWidgetFromName(TEXT("Border_RewardLocked")));
                    auto* CardBonus = CastChecked<UTextBlock>(FirstCard->GetWidgetFromName(TEXT("Text_Bonus")));
                    UWidget* CardRewardStrip = FirstCard->GetWidgetFromName(TEXT("Scale_RewardActions"));
                    if (!TestNotNull(TEXT("Card reward strip"), CardRewardStrip)) return false;
                    Item->Entry.bDiscoveryRewardClaimed = true;
                    FirstCard->RefreshPresentation();
                    TestTrue(TEXT("Claimed card has only its amount and receipt on the bottom line"),
                        !CardClaim->IsVisible() && !CardClaimedBadge->IsVisible() &&
                        CardRewardStrip->IsVisible() && CardBonus->IsVisible() &&
                        CardBonus->GetText().ToString().Contains(TEXT("%")) &&
                        CompactState->GetText().ToString().Contains(TEXT("수령 완료")) &&
                        CompactState->GetText().ToString().Contains(TEXT("450")));
                    Item->Entry.bDiscovered = false;
                    Item->Entry.bDiscoveryRewardClaimed = false;
                    FirstCard->RefreshPresentation();
                    TestTrue(TEXT("Undiscovered card has only its amount and prerequisite on the bottom line"),
                        !CardClaim->IsVisible() && !CardLockedBadge->IsVisible() &&
                        CardRewardStrip->IsVisible() && CardBonus->IsVisible() &&
                        CardBonus->GetText().ToString().Contains(TEXT("%")) &&
                        CompactState->GetText().ToString().Contains(TEXT("미발견")) &&
                        CompactState->GetText().ToString().Contains(TEXT("450")));
                    Item->Entry = Original;
                    FirstCard->RefreshPresentation();
                    TestTrue(TEXT("Pooled card restores its claim action after reward-state transitions"),
                        CardClaim->IsVisible() && CardClaim->GetIsEnabled() && CardRewardStrip->IsVisible());
                }
            }
            Screen->OpenCombinationDetail(Entries[0].RuleId);
            for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
            {
                FWidgetRenderer Renderer(false, true);
                const FVector2D Size(Resolution.X, Resolution.Y);
                const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
                TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
                if (!TestNotNull(TEXT("Pearl detail render target"), Target.Get())) return false;
                auto Window = SNew(SVirtualWindow).Size(Size);
                Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
                FHittestGrid Grid;
                for (int32 Frame = 0; Frame < 8; ++Frame)
                {
                    Screen->ForceLayoutPrepass();
                    Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
                }
                const FSlateRect DetailRect = DetailPage->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect ExitRect = ExitCodex->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect NextRect = Screen->GetWidgetFromName(TEXT("Button_CollectionNext"))->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Detail navigation leaves a clear gap before the back button"),
                    ExitCodex->IsVisible() && NextRect.Right + 16.f * Dpi <= ExitRect.Left && ExitRect.Right <= Resolution.X);
                const FSlateRect ShowcaseRect = Showcase->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect RewardPanelRect = Screen->GetWidgetFromName(TEXT("Border_ShowcaseRewardInfo"))
                    ->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect ClaimRect = Claim->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect ClaimLabelRect = ClaimLabel->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect ChevronRect = RewardChevron->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Separate detail and showcase fit the viewport"),
                    Screen->IsCombinationDetailOpen() && !Browse->IsVisible() &&
                    DetailRect.Left >= -1.f && DetailRect.Top >= -1.f &&
                    DetailRect.Right <= Resolution.X + 1.f && DetailRect.Bottom <= Resolution.Y + 1.f &&
                    ShowcaseRect.Left >= -1.f && ShowcaseRect.Top >= -1.f &&
                    ShowcaseRect.Right <= Resolution.X + 1.f && ShowcaseRect.Bottom <= Resolution.Y + 1.f);
                SaveCollectionPreview(*Target, Resolution, TEXT("Detail"));
                TestTrue(TEXT("The exact Gold amount and chevron paint inside the claim row"),
                    ClaimLabelRect.GetSize().GetMin() > 0.f && ChevronRect.GetSize().GetMin() > 0.f &&
                    ClaimLabelRect.Left >= ClaimRect.Left - 1.f && ClaimLabelRect.Right <= ChevronRect.Left + .5f &&
                    ChevronRect.Right <= ClaimRect.Right + 1.f);
                TestTrue(TEXT("The Pearl detail claim fills its reward surface at 72 logical pixels high"),
                    ClaimRect.GetSize().X >= RewardPanelRect.GetSize().X - 32.f * Dpi &&
                    ClaimRect.GetSize().Y / Dpi >= 71.f && ClaimRect.GetSize().Y / Dpi <= 73.f);
            }
            Close->OnClicked.Broadcast();
            TArray<FFF_FacilityCodexEntry> GridEntries;
            for (int32 Index = 0; Index < 12; ++Index)
            {
                auto& Entry = GridEntries.Add_GetRef(Entries[Index % Entries.Num()]);
                Entry.RuleId = FName(*FString::Printf(TEXT("FixedSix_%02d"), Index));
            }
            Screen->SetPreviewEntries(GridEntries);
            for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
            {
                FWidgetRenderer Renderer(false, true);
                const FVector2D Size(Resolution.X, Resolution.Y);
                const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
                TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
                if (!TestNotNull(TEXT("Six-column render target"), Target.Get())) return false;
                auto Window = SNew(SVirtualWindow).Size(Size);
                Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
                FHittestGrid Grid;
                for (int32 Frame = 0; Frame < 8; ++Frame)
                {
                    Screen->ForceLayoutPrepass();
                    Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
                }
                const auto Rect = [Tile](int32 Index)
                {
                    UUserWidget* Card = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[Index]);
                    return Card ? Card->GetPaintSpaceGeometry().GetLayoutBoundingRect() : FSlateRect();
                };
                const FSlateRect First = Rect(0), Sixth = Rect(5), Seventh = Rect(6);
                const FSlateRect GridBounds = Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                AddInfo(FString::Printf(TEXT("Six-column bounds %dx%d: first=%s sixth=%s seventh=%s grid=%s"),
                    Resolution.X, Resolution.Y, *First.ToString(), *Sixth.ToString(), *Seventh.ToString(), *GridBounds.ToString()));
                TestTrue(TEXT("Exactly six real cards occupy each row and the seventh begins the next row"),
                    First.GetSize().GetMin() > 0.f && Sixth.GetSize().GetMin() > 0.f && Seventh.GetSize().GetMin() > 0.f &&
                    FMath::Abs(First.Top - Sixth.Top) <= 1.f && Sixth.Left > First.Right &&
                    Seventh.Top >= First.Bottom - 1.f && FMath::Abs(Seventh.Left - First.Left) <= 1.f &&
                    First.Left >= GridBounds.Left - 1.f && Sixth.Right <= GridBounds.Right + 1.f);
                SaveCollectionPreview(*Target, Resolution, TEXT("SixColumns"));
            }
        }
        return true;
    }
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
    TestTrue(TEXT("The selected recipe's reward fills a 72-high row"),
        !DetailRewardBounds->IsWidthOverride() &&
        FMath::IsNearlyEqual(DetailRewardBounds->GetHeightOverride(), 72.f));

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
    TestTrue(TEXT("Detail typography uses the project Pretendard font"),
        AuthoredFont.FontObject && AuthoredFont.FontObject->GetPathName() ==
            TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard"));
    auto* CodexCanvas = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("Canvas_CodexLayout")));
    auto* CodexDock = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Border_CodexDock")));
    if (!TestNotNull(TEXT("Authored floating codex canvas"), CodexCanvas) ||
        !TestNotNull(TEXT("Authored lower catalog dock"), CodexDock)) return false;
    TestTrue(TEXT("Existing list and detail widgets remain in separate canvas slots"),
        Main->GetChildrenCount() == 1 && Main->GetChildAt(0) == BrowseBounds &&
        Cast<UCanvasPanelSlot>(DetailBounds->Slot) != nullptr);
    int32 BlurLayers = 0;
    Screen->WidgetTree->ForEachWidget([&](UWidget* Widget) {
        if (const auto* Blur = Cast<UBackgroundBlur>(Widget))
        {
            ++BlurLayers;
            TestTrue(TEXT("The glass backdrop has actual blur and a non-opaque low-quality fallback"),
                Blur->GetBlurStrength() > 0.f && Blur->GetLowQualityFallbackBrush().TintColor.GetSpecifiedColor().A < 1.f);
        }
    });
    TestEqual(TEXT("The floating codex uses one subtle shared backdrop blur"), BlurLayers, 1);
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
        const FSlateRect DockRect = CodexDock->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        if (Width >= 1280)
            TestTrue(TEXT("Search keeps a usable input inside the lower catalog dock"),
                Search->GetPaintSpaceGeometry().GetLayoutBoundingRect().GetSize().X > DockRect.GetSize().X * .20f);
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
        TestTrue(TEXT("The lower dock leaves the world visible above it"),
            DockRect.Top > Height * .25f && DockRect.Bottom <= Height + 1.f);
        TestTrue(TEXT("Collection cards stay inside the floating dock"),
            GridRect.Left >= DockRect.Left - 1.f && GridRect.Right <= DockRect.Right + 1.f &&
            GridRect.Top >= DockRect.Top - 1.f && GridRect.Bottom <= DockRect.Bottom + 1.f);
        const float LogicalWidth = Width / Dpi;
        const bool bWideShowcase = LogicalWidth >= 1100.f;
        if (bWideShowcase)
        {
            TestTrue(TEXT("Wide layout places a separate detail card beside the lower dock"),
                InspectorRect.Left >= DockRect.Right - 1.f && InspectorRect.Top < DockRect.Top &&
                InspectorRect.Right <= Width + 1.f);
        }
        else
        {
            TestTrue(TEXT("Narrow layout keeps the scrollable detail above the full-width dock"),
                InspectorRect.Bottom <= DockRect.Top + 1.f && InspectorRect.Top >= 0.f &&
                DockRect.GetSize().X >= Width * .85f);
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
            const FSlateRect TabsRect = Screen->GetWidgetFromName(TEXT("Horizontal_DetailTabs"))->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect RewardPanelRect = Screen->GetWidgetFromName(TEXT("Size_ShowcaseRewardInfo"))->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("The detail scroll viewport stays inside its reserved region"),
                ScrollRect.Left >= DetailRect.Left - 1.f && ScrollRect.Right <= DetailRect.Right + 1.f &&
                ScrollRect.Top >= DetailRect.Top - 1.f && ScrollRect.Bottom <= DetailRect.Bottom + 1.f);
            TestTrue(TEXT("Preview tabs remain visible above the detail scroll"),
                TabsRect.GetSize().Y >= 20.f * Dpi && TabsRect.Bottom <= ScrollRect.Top + 1.f &&
                ScrollRect.GetSize().Y >= 24.f * Dpi);
            bool bRewardInsideScroll = false;
            for (UWidget* Parent = Screen->GetWidgetFromName(TEXT("Size_ShowcaseRewardInfo"))->GetParent();
                Parent; Parent = Parent->GetParent())
                bRewardInsideScroll |= Parent == DetailScroll;
            TestTrue(TEXT("The reward row follows the explanation inside the detail scroll"), bRewardInsideScroll);
            TestTrue(TEXT("The reward row keeps the scroll viewport's horizontal bounds"),
                RewardPanelRect.Left >= ScrollRect.Left - 1.f &&
                RewardPanelRect.Right <= ScrollRect.Right + 1.f);
            bool bPagesInsideScroll = false;
            for (UWidget* Parent = DetailPages->GetParent(); Parent; Parent = Parent->GetParent())
                bPagesInsideScroll |= Parent == DetailScroll;
            TestTrue(TEXT("Long showcase detail pages are reachable through the vertical scroll view"), bPagesInsideScroll);
            const FSlateRect ActivePageRect = DetailPages->GetActiveWidget()->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(FString::Printf(TEXT("%s: detail pages fit the scroll viewport without horizontal overflow"), Filename),
                ActivePageRect.Left >= ScrollRect.Left - 1.f && ActivePageRect.Right <= ScrollRect.Right + 1.f);
            const TArray<FName> DetailNames = DetailPages->GetActiveWidgetIndex() == 0
                ? TArray<FName>{TEXT("Text_Bonus")}
                : DetailPages->GetActiveWidgetIndex() == 1
                ? TArray<FName>{TEXT("Text_Distance"), TEXT("Text_Condition"), TEXT("Text_Discovery")}
                : TArray<FName>{TEXT("Text_Applied")};
            int32 VisibleDetailTexts = 0;
            for (FName Name : DetailNames)
            {
                UWidget* DetailText = Screen->GetWidgetFromName(Name);
                if (!TestNotNull(TEXT("Detail page text binding is intact"), DetailText)) continue;
                bool bVisible = DetailText->IsVisible();
                for (UWidget* Parent = DetailText->GetParent(); Parent && bVisible; Parent = Parent->GetParent())
                    bVisible = Parent->IsVisible();
                if (!bVisible) continue;
                const FSlateRect Rect = DetailText->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                // A scroll box can cull nonempty text below the current viewport;
                // Slate leaves its cached paint geometry at the zero rect then.
                if (Rect.GetSize().X <= .5f && Rect.GetSize().Y <= .5f) continue;
                ++VisibleDetailTexts;
                TestTrue(FString::Printf(TEXT("Selected detail %s fits scroll width: text [%.1f, %.1f], scroll [%.1f, %.1f]"),
                    *Name.ToString(), Rect.Left, Rect.Right, ScrollRect.Left, ScrollRect.Right),
                    Rect.Left >= ScrollRect.Left - 1.f && Rect.Right <= ScrollRect.Right + 1.f);
                if (Name == FName(TEXT("Text_Bonus")))
                    TestTrue(FString::Printf(TEXT("Selected combination bonus is readable without scrolling: text bottom %.1f, scroll bottom %.1f"),
                        Rect.Bottom, ScrollRect.Bottom), Rect.Top >= ScrollRect.Top - 1.f &&
                        Rect.Bottom <= ScrollRect.Bottom + 1.f);
            }
            TestTrue(TEXT("Every selected detail page exposes readable text"), VisibleDetailTexts > 0);
            for (FName Name : {FName(TEXT("Text_TargetName")), FName(TEXT("Text_PartnerName"))})
            {
                const FSlateRect Rect = Screen->GetWidgetFromName(Name)->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Facility catalog links stay visible above the detail scroll"),
                    Rect.Left >= DetailRect.Left - 1.f && Rect.Right <= DetailRect.Right + 1.f &&
                    Rect.Top >= DetailRect.Top - 1.f && Rect.Bottom <= ScrollRect.Top + 1.f);
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
                    const FSlateRect RewardBoundsRect = DetailRewardBounds->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("Selected-detail reward states fill the same 72-high row"),
                        FMath::IsNearlyEqual(RewardRect.GetSize().X, RewardBoundsRect.GetSize().X, 1.f) &&
                        FMath::IsNearlyEqual(RewardRect.GetSize().Y / Dpi, 72.f, 1.f));
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
                UWidget* RewardStrip = Card->GetWidgetFromName(TEXT("Scale_RewardActions"));
                auto* CompactState = CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_State")));
                TestEqual(TEXT("Only unclaimed discovered rewards show their action"), RewardButton->IsVisible(), bRewardVisible);
                TestFalse(TEXT("Claimed and locked card badges never repeat the bottom reward state"),
                    ClaimedBadge->IsVisible() || LockedBadge->IsVisible());
                TestEqual(TEXT("Only a payable reward reserves the action region"),
                    RewardRegion->IsVisible(), bRewardVisible);
                if (TestNotNull(TEXT("Saved card fee and reward strip"), RewardStrip))
                    TestTrue(TEXT("Every populated card keeps its fee percentage visible"),
                        RewardStrip->IsVisible() && Bonus->IsVisible() &&
                        Bonus->GetText().ToString().Contains(TEXT("%")));
                TestEqual(TEXT("A claimed reward cannot be clicked again"), RewardButton->GetIsEnabled(),
                    bRewardVisible);
                if (bLocked)
                {
                    TestTrue(TEXT("Undiscovered reward amount and prerequisite remain on the bottom state line"),
                        CompactState->GetText().ToString().Contains(FText::AsNumber(Item->Entry.DiscoveryRewardGold).ToString()) &&
                        CompactState->GetText().ToString().Contains(TEXT("미발견")) &&
                        LockedBadge->GetToolTipText().IsEmpty());
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
                    TestTrue(TEXT("Claimed reward amount and receipt remain on the bottom state line"),
                        (Item->Entry.DiscoveryRewardGold <= 0 ||
                            CompactState->GetText().ToString().Contains(FText::AsNumber(Item->Entry.DiscoveryRewardGold).ToString())) &&
                        CompactState->GetText().ToString().Contains(TEXT("수령 완료")) &&
                        ClaimedBadge->GetToolTipText().IsEmpty());
                }
            }
            TestTrue(TEXT("The complete card information stays inside its surface"), StateRect.Bottom <= CardBounds.Bottom + 1.f);
        }
        AddInfo(FString::Printf(TEXT("Codex viewport %dx%d uses project DPI %.3f and %s floating detail."),
            Width, Height, Dpi, bWideShowcase ? TEXT("side") : TEXT("upper")));
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
            auto* UnknownState = CastChecked<UTextBlock>(UnknownCard->GetWidgetFromName(TEXT("Text_State")));
            auto* FoundState = CastChecked<UTextBlock>(FoundCard->GetWidgetFromName(TEXT("Text_State")));
            TestTrue(TEXT("Unknown card keeps its reward in the bottom line without an extra badge"),
                !UnknownLocked->IsVisible() && !UnknownReward->IsVisible() && !UnknownReward->GetIsEnabled() &&
                !UnknownCard->GetWidgetFromName(TEXT("Size_RewardState"))->IsVisible() &&
                UnknownState->GetText().ToString().Contains(TEXT("미발견")) &&
                UnknownState->GetText().ToString().Contains(FText::AsNumber(Entries[1].DiscoveryRewardGold).ToString()));
            FoundItem->Entry.bDiscoveryRewardClaimed = true;
            CastChecked<UFF_FacilityCodexCard>(FoundCard)->RefreshPresentation();
            FoundReward->OnClicked.Broadcast();
            TestEqual(TEXT("Claimed card rejects repeat clicks before reaching the economy"), ClaimRequests, 1);
            TestFalse(TEXT("Received action is disabled"), FoundReward->GetIsEnabled());
            TestFalse(TEXT("Received gift action disappears"), FoundReward->IsVisible());
            auto* CompletedBadge = CastChecked<UBorder>(FoundCard->GetWidgetFromName(TEXT("Border_RewardClaimed")));
            TestTrue(TEXT("Claimed card keeps one bottom receipt without a duplicate badge"),
                !CompletedBadge->IsVisible() && !FoundCard->GetWidgetFromName(TEXT("Size_RewardState"))->IsVisible() &&
                FoundState->GetText().ToString().Contains(TEXT("수령 완료")) &&
                FoundState->GetText().ToString().Contains(FText::AsNumber(Entries[0].DiscoveryRewardGold).ToString()));
            TestTrue(TEXT("Claimed reward visual preview"), Capture(1280, 720, TEXT("FacilityCodexClaimed720.png")));
            FoundItem->Entry.DiscoveryRewardGold = 0;
            CastChecked<UFF_FacilityCodexCard>(FoundCard)->RefreshPresentation();
            TestTrue(TEXT("A changed zero reward amount keeps claim history on the bottom line"),
                !CompletedBadge->IsVisible() && FoundState->GetText().ToString().Contains(TEXT("수령 완료")));
            // Restore screen-owned delegates before local assertion captures leave scope.
            Screen->SetPreviewEntries(Entries);
            TestTrue(TEXT("Pooled card clears the previous receipt and restores its ready action"),
                !CompletedBadge->IsVisible() && FoundState->GetText().ToString().Contains(TEXT("수령 가능")));
            TestTrue(TEXT("Pooled card restores an available claim"), FoundReward->IsVisible() && FoundReward->GetIsEnabled());
            UnknownItem->Entry.bDiscovered = true;
            CastChecked<UFF_FacilityCodexCard>(UnknownCard)->RefreshPresentation();
            TestTrue(TEXT("Discovery replaces a pooled locked hint with its claim action"),
                !UnknownLocked->IsVisible() && UnknownLocked->GetToolTipText().IsEmpty() &&
                UnknownReward->IsVisible() && UnknownReward->GetIsEnabled() &&
                UnknownCard->GetWidgetFromName(TEXT("Size_RewardState"))->IsVisible());
            UnknownItem->Entry.bDiscoveryRewardClaimed = true;
            CastChecked<UFF_FacilityCodexCard>(UnknownCard)->RefreshPresentation();
            TestTrue(TEXT("Claiming removes the ready action while retaining a single bottom receipt"),
                !UnknownCompleted->IsVisible() && !UnknownLocked->IsVisible() && !UnknownReward->IsVisible() &&
                UnknownState->GetText().ToString().Contains(TEXT("수령 완료")) &&
                !UnknownCard->GetWidgetFromName(TEXT("Size_RewardState"))->IsVisible());
            UnknownItem->Entry.bDiscovered = false;
            UnknownItem->Entry.bDiscoveryRewardClaimed = false;
            CastChecked<UFF_FacilityCodexCard>(UnknownCard)->RefreshPresentation();
            TestTrue(TEXT("Reusing a completed card restores only the unknown bottom state"),
                !UnknownLocked->IsVisible() && !UnknownCompleted->IsVisible() &&
                UnknownCompleted->GetToolTipText().IsEmpty() && !UnknownReward->IsVisible() &&
                UnknownState->GetText().ToString().Contains(TEXT("미발견")));
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
            auto* RewardRegion = CastChecked<USizeBox>(TransitionCard->GetWidgetFromName(TEXT("Size_RewardState")));
            auto* CompactState = CastChecked<UTextBlock>(TransitionCard->GetWidgetFromName(TEXT("Text_State")));
            const FVector2D LockedDetailSize = DetailLocked->GetPaintSpaceGeometry().GetLayoutBoundingRect().GetSize();
            TestTrue(TEXT("Pre-discovery card shows one bottom reward line without a duplicate locked badge"),
                !LockedBadge->IsVisible() && !ReadyButton->IsVisible() && !RewardRegion->IsVisible() &&
                CompactState->GetText().ToString().Contains(TEXT("미발견")) &&
                CompactState->GetText().ToString().Contains(FText::AsNumber(Entries[0].DiscoveryRewardGold).ToString()));

            TransitionItem->Entry = Entries[0];
            TransitionCard->RefreshPresentation();
            Screen->SetPreviewEntries(Entries);
            if (!TestTrue(TEXT("Ready reward transition preview"), Capture(Viewport.X, Viewport.Y, *BeforeFilename))) continue;
            const FSlateRect ReadyRect = ReadyButton->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FVector2D ReadyDetailSize = DetailReward->GetPaintSpaceGeometry().GetLayoutBoundingRect().GetSize();
            const FSlateRect CardRect = TransitionCard->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Discovery shows a readable claim button inside its card"),
                ReadyButton->IsVisible() && ReadyButton->GetIsEnabled() && RewardRegion->IsVisible() &&
                ReadyRect.GetSize().GetMin() > 0.f && ReadyRect.Left >= CardRect.Left - 1.f &&
                ReadyRect.Right <= CardRect.Right + 1.f && ReadyRect.Bottom <= CardRect.Bottom + 1.f &&
                CompactState->GetText().ToString().Contains(TEXT("수령 가능")));
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
            const FVector2D CompletedDetailSize = DetailClaimed->GetPaintSpaceGeometry().GetLayoutBoundingRect().GetSize();
            const FSlateRect StateRect = CompactState->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Claiming hides the upper badge and keeps the receipt inside the card"),
                !CompletedBadge->IsVisible() && !ReadyButton->IsVisible() && !RewardRegion->IsVisible() &&
                CompactState->GetText().ToString().Contains(TEXT("수령 완료")) &&
                StateRect.Left >= CardRect.Left - 1.f && StateRect.Right <= CardRect.Right + 1.f &&
                StateRect.Bottom <= CardRect.Bottom + 1.f);
            TestTrue(FString::Printf(TEXT("Selected-detail claim replaces its button with an identically sized receipt at %dp"), Viewport.Y),
                DetailClaimed->IsVisible() && ReadyDetailSize.Equals(CompletedDetailSize, .25f));
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
