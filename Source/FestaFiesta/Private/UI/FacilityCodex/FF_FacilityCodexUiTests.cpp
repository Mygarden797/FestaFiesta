#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "Building/Codex/FF_FacilityCodexModel.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/StackBox.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
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
            TEXT("/Game/FestaFiesta/UI/FacilityCodex/WBP_FF_FacilityCodex%s.WBP_FF_FacilityCodex%s_C"), Name, Name)).TryLoadClass<UUserWidget>());
    };
    auto* ScreenClass = LoadClass(TEXT("Screen"));
    auto* CardClass = LoadClass(TEXT("Card"));
    auto* LauncherClass = LoadClass(TEXT("Launcher"));
    auto* InspectorClass = Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(TEXT(
        "/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel.WBP_FF_FacilityInfoPanel_C")).TryLoadClass<UUserWidget>());
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
    if (!TestNotNull(TEXT("Virtualized tile view"), Tile) || !TestNotNull(TEXT("Search input"), Search) ||
        !TestNotNull(TEXT("Close button"), Close) || !TestNotNull(TEXT("Detail heading"), Detail)) return false;
    for (const FName Name : { TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"), TEXT("Button_ClearFacility") })
        if (!TestNotNull(*Name.ToString(), Cast<UButton>(Screen->GetWidgetFromName(Name)))) return false;

    auto Definition = [](const TCHAR* Suffix) {
        return LoadObject<UFFZSFacilityDefinition>(nullptr, *FString::Printf(TEXT(
            "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/%s"), Suffix));
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
        Entry.DisplayName = FText::Format(FText::FromString(TEXT("{0} + {1}")), Entry.TargetName, Entry.PartnerName);
        Entry.TargetThumbnail = Target->CatalogThumbnail;
        Entry.PartnerThumbnail = Partner->CatalogThumbnail;
        Entry.TargetThumbnail.LoadSynchronous();
        Entry.PartnerThumbnail.LoadSynchronous();
        Entry.MaxDistanceMeters = 15.f;
        Entry.ConfiguredBonusPercent = 10.f;
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
    Screen->SetFacilityFilter(Seating->Spec.FacilityDefinitionId);
    TestEqual(TEXT("Supporting facility matches its recipe"), Screen->GetVisibleEntryCount(), 1);
    Screen->SetFacilityFilter(NAME_None);
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_Undiscovered")))->OnClicked.Broadcast();
    TestEqual(TEXT("Actual undiscovered button filters"), Screen->GetVisibleEntryCount(), 1);
    TestEqual(TEXT("Detail follows filtered selection"), Screen->GetSelectedRuleId(), FName(TEXT("IceCreamShade")));
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_All")))->OnClicked.Broadcast();
    Search->SetText(FText::FromString(TEXT("no_matching_recipe")));
    Search->OnTextChanged.Broadcast(Search->GetText());
    TestEqual(TEXT("Search input filters recipes"), Screen->GetVisibleEntryCount(), 0);
    TestTrue(TEXT("Empty results clear detail selection"), Screen->GetSelectedRuleId().IsNone());
    TestTrue(TEXT("Empty-state copy is visible"), Screen->GetWidgetFromName(TEXT("Text_Empty"))->IsVisible());
    Screen->SetSearchText(FText::GetEmpty());
    TestEqual(TEXT("Programmatic search refreshes results"), Screen->GetVisibleEntryCount(), 2);
    TestTrue(TEXT("Programmatic search updates input text"), Search->GetText().IsEmpty());
    Tile->SetSelectedItem(Tile->GetListItems()[0]);
    TestEqual(TEXT("Tile selection drives recipe detail"), Screen->GetSelectedRuleId(), FName(TEXT("BurgerPicnic")));
    const FSlateFontInfo AuthoredFont = Detail->GetFont();
    const float AuthoredEntryWidth = Tile->GetEntryWidth();
    const float AuthoredEntryHeight = Tile->GetEntryHeight();
    auto* Main = Cast<UStackBox>(Screen->GetWidgetFromName(TEXT("Stack_Main")));
    auto* DetailBounds = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Size_Detail")));
    if (!TestNotNull(TEXT("Migrated responsive main panel"), Main) ||
        !TestNotNull(TEXT("Authored detail bounds"), DetailBounds)) return false;
    const float AuthoredDetailWidth = DetailBounds->GetWidthOverride();
    Screen->SetPreviewEntries(Entries);
    TestTrue(TEXT("Data refresh preserves Designer typography"), AuthoredFont == Detail->GetFont());

    auto Capture = [&](int32 Width, int32 Height, const TCHAR* Filename) {
        if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
        FWidgetRenderer Renderer(false, true);
        FVector2D Size(Width, Height);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
        if (!Target) return false;
        auto Window = SNew(SVirtualWindow).Size(Size);
        Window->SetContent(Slate);
        FHittestGrid Grid;
        for (int32 Frame = 0; Frame < 8; ++Frame)
        {
            Screen->ForceLayoutPrepass();
            Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
        }
        const FSlateRect CloseRect = Close->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Close button stays inside viewport"), CloseRect.Left >= 0 && CloseRect.Top >= 0 &&
            CloseRect.Right <= Width && CloseRect.Bottom <= Height && CloseRect.GetSize().GetMin() > 0);
        TestTrue(TEXT("Visible recipes generate cards"), Screen->GetVisibleEntryCount() == 0 || Tile->GetDisplayedEntryWidgets().Num() > 0);
        const TArray<UUserWidget*>& Cards = Tile->GetDisplayedEntryWidgets();
        TestEqual(TEXT("Responsive layout preserves authored TileView width"), Tile->GetEntryWidth(), AuthoredEntryWidth);
        TestEqual(TEXT("Responsive layout preserves authored TileView height"), Tile->GetEntryHeight(), AuthoredEntryHeight);
        TestTrue(TEXT("Responsive layout preserves Designer typography"), Detail->GetFont() == AuthoredFont);
        if (Width == 1280)
        {
            TestEqual(TEXT("720p places detail below readable cards"), Main->GetOrientation(), Orient_Vertical);
            TestTrue(TEXT("Compact detail uses its own scrolling surface"),
                DetailBounds->GetPaintSpaceGeometry().GetLayoutBoundingRect().Top >=
                Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect().Bottom - 1.f);
            const FSlateRect ListBounds = Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            for (int32 Index = 0; Index < FMath::Min(6, Tile->GetListItems().Num()); ++Index)
            {
                UUserWidget* Card = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[Index]);
                if (!TestNotNull(TEXT("720p first row is generated"), Card)) continue;
                const FSlateRect CardBounds = Card->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("720p shows the complete first card row including its status"),
                    CardBounds.Top >= ListBounds.Top - 1.f && CardBounds.Bottom <= ListBounds.Bottom + 1.f);
            }
        }
        else if (Width == 1920)
        {
            TestEqual(TEXT("1080p restores detail alongside cards"), Main->GetOrientation(), Orient_Horizontal);
            TestTrue(TEXT("Wide layout restores the saved detail width"),
                DetailBounds->IsWidthOverride() && FMath::IsNearlyEqual(DetailBounds->GetWidthOverride(), AuthoredDetailWidth));
        }
        if (Tile->GetListItems().Num() >= 7)
        {
            auto* FirstCard = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[0]);
            auto* SeventhCard = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[6]);
            if (TestNotNull(TEXT("First virtualized row exists"), FirstCard))
            {
                const FSlateRect First = FirstCard->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                for (int32 Index = 1; Index < 6; ++Index)
                {
                    auto* Card = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[Index]);
                    if (!TestNotNull(TEXT("Each of six columns is generated"), Card)) continue;
                    const FSlateRect Rect = Card->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("First six recipes share one row"), FMath::IsNearlyEqual(Rect.Top, First.Top, 1.f));
                    TestTrue(TEXT("Each column advances horizontally"), Rect.Left > First.Left);
                    TestTrue(TEXT("Sixth column remains inside the list"), Rect.Right <=
                        Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect().Right + 1.f);
                }
                if (Width == 1920) TestNotNull(TEXT("Wide grid generates another visible row"), SeventhCard);
                if (SeventhCard)
                {
                    const FSlateRect Seventh = SeventhCard->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("Seventh recipe begins the next row"), Seventh.Top > First.Top &&
                        FMath::IsNearlyEqual(Seventh.Left, First.Left, 1.f));
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
    TestTrue(TEXT("1080p visual preview"), Capture(1920, 1080, TEXT("FacilityCodex1080.png")));
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
            auto* UnknownImage = CastChecked<UImage>(UnknownCard->GetWidgetFromName(TEXT("Image_Target")));
            const FSlateColor DiscoveredColor = FoundBonus->GetColorAndOpacity();
            const FSlateColor UnknownColor = UnknownBonus->GetColorAndOpacity();
            const FLinearColor Gray = UnknownColor.GetSpecifiedColor();
            TestTrue(TEXT("Undiscovered bonus uses gray rather than the discovered accent"),
                FMath::IsNearlyEqual(Gray.R, Gray.G) && FMath::IsNearlyEqual(Gray.G, Gray.B) &&
                UnknownColor != DiscoveredColor);
            TestTrue(TEXT("Undiscovered card can still be selected"), UnknownCard->GetIsEnabled());
            TestFalse(TEXT("Only undiscovered artwork is dimmed"), UnknownImage->GetIsEnabled());
            Tile->SetSelectedItem(Tile->GetListItems()[1]);
            TestEqual(TEXT("Gray card reveals its recipe detail"), Screen->GetSelectedRuleId(), FName(TEXT("IceCreamShade")));
            const FSlateBrush UnknownBrush = UnknownBody->Background;
            Entries[1].bDiscovered = true;
            Screen->SetPreviewEntries(Entries);
            TestTrue(TEXT("Discovery restores the authored accent on the existing card"),
                UnknownBonus->GetColorAndOpacity() == DiscoveredColor);
            TestTrue(TEXT("Discovery restores thumbnail presentation"), UnknownImage->GetIsEnabled());
            Entries[1].bDiscovered = false;
            Screen->SetPreviewEntries(Entries);
            TestTrue(TEXT("A reused unknown card returns to its gray palette"),
                UnknownBonus->GetColorAndOpacity() == UnknownColor && UnknownBody->Background == UnknownBrush);
        }
    }
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
    TestTrue(TEXT("Six-column wide grid visual preview"), Capture(1920, 1080, TEXT("FacilityCodexGrid1080.png")));
    if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
        TestTrue(TEXT("Large table creates only viewport cards"), Tile->GetDisplayedEntryWidgets().Num() < 120);
    Screen->SetPreviewEntries({});
    TestEqual(TEXT("Empty table releases all items"), Screen->GetVisibleEntryCount(), 0);
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
        Screen->SetFacilityFilter(NAME_None);
        Screen->SetSearchText(FText::GetEmpty());
        Screen->SetPreviewEntries(ProductionEntries);
        TestEqual(TEXT("Every enabled usable production recipe reaches the visible list"),
            Screen->GetVisibleEntryCount(), EnabledUsableCount);
        auto* TargetName = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_TargetName")));
        auto* PartnerName = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_PartnerName")));
        auto* TargetImage = Cast<UImage>(Screen->GetWidgetFromName(TEXT("Image_Target")));
        auto* PartnerImage = Cast<UImage>(Screen->GetWidgetFromName(TEXT("Image_Partner")));
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
                        TargetImage->GetBrush().GetResourceObject() == Entry.TargetThumbnail.Get());
                if (Entry.PartnerThumbnail.IsValid())
                    TestTrue(Context + TEXT(" displays its partner PDA thumbnail"),
                        PartnerImage->GetBrush().GetResourceObject() == Entry.PartnerThumbnail.Get());
            }
        }
        if (!Tile->GetListItems().IsEmpty()) Tile->SetSelectedItem(Tile->GetListItems()[0]);
        Tile->ScrollToTop();
        AddInfo(FString::Printf(TEXT("ProductionCodex1080.png / ProductionCodex720.png: %d actual enabled usable "
            "recipes from %s, real PDA names/thumbnails, empty-placement fixture (undiscovered/unapplied)."),
            EnabledUsableCount, *ProductionTable->GetPathName()));
        TestTrue(TEXT("Production data empty-placement fixture at 1080p"), Capture(1920, 1080, TEXT("ProductionCodex1080.png")));
        TestTrue(TEXT("Production data empty-placement fixture at 720p"), Capture(1280, 720, TEXT("ProductionCodex720.png")));
    }
    int32 CloseCount = 0;
    Screen->OnCloseRequested.AddLambda([&CloseCount]() { ++CloseCount; });
    Close->OnClicked.Broadcast();
    TestEqual(TEXT("Actual close button dispatches once"), CloseCount, 1);
    Screen->OnCloseRequested.Clear();
    return true;
}
#endif
