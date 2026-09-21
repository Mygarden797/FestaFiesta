#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "Building/Codex/FF_FacilityCodexModel.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/StackBox.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Editor.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Layout/SDPIScaler.h"

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
    for (const FName Name : { TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"), TEXT("Button_Active"), TEXT("Button_ClearFacility") })
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
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("Button_Active")))->OnClicked.Broadcast();
    TestEqual(TEXT("Applied filter shows only recipes with a live nonzero effect"), Screen->GetVisibleEntryCount(), 1);
    TestEqual(TEXT("Applied filter selects the actual applied recipe"), Screen->GetSelectedRuleId(), FName(TEXT("BurgerPicnic")));
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
    auto CheckSelectedTab = [&](const TCHAR* Name)
    {
        auto* Button = CastChecked<UButton>(Screen->GetWidgetFromName(FName(Name)));
        TestTrue(TEXT("Selected codex tab stays enabled so Slate does not gray out its contents"), Button->GetIsEnabled());
        TestTrue(TEXT("Selected codex tab uses its saved selected brush"), Button->GetStyle().Normal == Button->GetStyle().Disabled);
        TestTrue(TEXT("Selected codex label remains bright on the selected surface"),
            CastChecked<UTextBlock>(Button->GetContent())->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::White));
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
    if (!TestNotNull(TEXT("Centered medium-sized codex frame"), Dialog) ||
        !TestNotNull(TEXT("Direct detail tabs"), DetailPages)) return false;
    TestEqual(TEXT("Production codex retains exactly six columns"), CastChecked<UFF_FacilityCodexTileView>(Tile)->ColumnCount, 6);
    for (const FName Name : {TEXT("Button_DetailRecipe"), TEXT("Button_DetailPlacement"), TEXT("Button_DetailEffect")})
        if (!TestNotNull(TEXT("Direct detail navigation button"), Cast<UButton>(Screen->GetWidgetFromName(Name)))) return false;
    Screen->SetPreviewEntries(Entries);
    TestTrue(TEXT("Data refresh preserves Designer typography"), AuthoredFont == Detail->GetFont());

    // Standalone runs cannot rely on BuildTools.RenderedPreview warming the shared
    // Clarity materials first. Finish their UI shaders before taking any screenshots.
    if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        TSet<UMaterialInterface*> Materials;
        auto GatherMaterial = [&Materials](const FSlateBrush& Brush)
        {
            if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) Materials.Add(Material);
        };
        Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
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
        });
        for (UMaterialInterface* Material : Materials) Material->EnsureIsComplete();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        FlushRenderingCommands();
    }

    auto Capture = [&](int32 Width, int32 Height, const TCHAR* Filename) {
        if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
        FWidgetRenderer Renderer(false, true);
        FVector2D Size(Width, Height);
        const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
        if (!Target) return false;
        auto Window = SNew(SVirtualWindow).Size(Size);
        Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
        FHittestGrid Grid;
        for (int32 Frame = 0; Frame < 8; ++Frame)
        {
            Screen->ForceLayoutPrepass();
            Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
        }
        const FSlateRect CloseRect = Close->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Close button stays inside viewport"), CloseRect.Left >= 0 && CloseRect.Top >= 0 &&
            CloseRect.Right <= Width && CloseRect.Bottom <= Height && CloseRect.GetSize().GetMin() > 0);
        TArray<FSlateRect> ActionRects;
        for (const TCHAR* Name : {TEXT("Button_Close"), TEXT("Button_All"), TEXT("Button_Discovered"),
            TEXT("Button_Undiscovered"), TEXT("Button_Active"), TEXT("Button_ClearFacility"),
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
        TestEqual(TEXT("Responsive layout preserves authored TileView width"), Tile->GetEntryWidth(), AuthoredEntryWidth);
        TestEqual(TEXT("Responsive layout preserves authored TileView height"), Tile->GetEntryHeight(), AuthoredEntryHeight);
        TestTrue(TEXT("Responsive layout preserves Designer typography"), Detail->GetFont() == AuthoredFont);
        TestEqual(TEXT("Details have a reserved region below the six-column grid"), Main->GetOrientation(), Orient_Vertical);
        TestTrue(TEXT("Detail and grid never overlap"),
            DetailBounds->GetPaintSpaceGeometry().GetLayoutBoundingRect().Top >=
            Tile->GetPaintSpaceGeometry().GetLayoutBoundingRect().Bottom - 1.f);
        const FSlateRect DialogRect = Dialog->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Codex leaves visible world around a medium-sized frame"),
            DialogRect.GetSize().X <= Width * .91f && DialogRect.GetSize().Y <= Height * .79f);
        TestTrue(TEXT("Codex frame is centered in the viewport"),
            FMath::IsNearlyEqual((DialogRect.Left + DialogRect.Right) * .5f, Width * .5f, 2.f) &&
            FMath::IsNearlyEqual((DialogRect.Top + DialogRect.Bottom) * .5f, Height * .5f, 2.f));
        const FSlateRect DetailRect = DetailBounds->GetPaintSpaceGeometry().GetLayoutBoundingRect();
        if (Screen->GetVisibleEntryCount() > 0)
        {
            const FSlateRect ActivePageRect = DetailPages->GetActiveWidget()->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(FString::Printf(TEXT("%s: each displayed detail page stays inside its reserved region"), Filename),
                ActivePageRect.Top >= DetailRect.Top - 1.f && ActivePageRect.Bottom <= DetailRect.Bottom + 1.f);
            const TArray<FName> DetailNames = DetailPages->GetActiveWidgetIndex() == 0
                ? TArray<FName>{TEXT("Text_TargetName"), TEXT("Text_PartnerName"), TEXT("Text_Bonus")}
                : DetailPages->GetActiveWidgetIndex() == 1
                ? TArray<FName>{TEXT("Text_Distance"), TEXT("Text_Condition"), TEXT("Text_Discovery")}
                : TArray<FName>{TEXT("Text_Applied")};
            for (FName Name : DetailNames)
            {
                const FSlateRect Rect = Screen->GetWidgetFromName(Name)->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(TEXT("Complete selected detail content remains visible without scrolling"),
                    Rect.Left >= DetailRect.Left - 1.f && Rect.Right <= DetailRect.Right + 1.f &&
                    Rect.Top >= DetailRect.Top - 1.f && Rect.Bottom <= DetailRect.Bottom + 1.f);
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
        for (int32 Index = 0; Index < FMath::Min(6, Tile->GetListItems().Num()); ++Index)
        {
            UUserWidget* Card = Tile->GetEntryWidgetFromItem(Tile->GetListItems()[Index]);
            if (!TestNotNull(TEXT("First card row is generated at project DPI"), Card)) continue;
            TestTrue(TEXT("Cards do not cover neighbouring recipes with repeated-name tooltips"),Card->GetToolTipText().IsEmpty() && Card->GetToolTip() == nullptr);
            const FSlateRect CardBounds = Card->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Project-DPI viewport shows the complete first card row including its status"),
                CardBounds.Top >= ListBounds.Top - 1.f && CardBounds.Bottom <= ListBounds.Bottom + 1.f);
            auto* NameText = CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_Name")));
            UWidget* NameBounds = Card->GetWidgetFromName(TEXT("Size_Name"));
            UWidget* Images = Card->GetWidgetFromName(TEXT("Horizontal_RecipeImages"));
            const FSlateRect NameRect = NameText->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect NameRegion = NameBounds->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect ImagesRect = Images->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Complete wrapped title glyphs fit their reserved height"),
                NameText->GetDesiredSize().Y <= NameText->GetPaintSpaceGeometry().GetLocalSize().Y + 1.f);
            TestTrue(TEXT("Wrapped card titles never extend into the ingredient images"),
                NameRect.Bottom <= NameRegion.Bottom + 1.f && NameRegion.Bottom <= ImagesRect.Top + 1.f);
            if (NameText->GetText().ToString().Contains(TEXT("아이스크림 가게 +")))
                TestTrue(TEXT("The long fixture title actually exercises a complete second line"),
                    NameText->GetDesiredSize().Y > 30.f && NameText->GetDesiredSize().Y <= NameRegion.GetSize().Y / Dpi + 1.f);
            auto* Bonus = CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_Bonus")));
            UWidget* State = Card->GetWidgetFromName(TEXT("Border_State"));
            const FSlateRect BonusRect = Bonus->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect StateRect = State->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Bonus text fits one full-width line, including the percent suffix"),
                Bonus->GetDesiredSize().X <= Bonus->GetPaintSpaceGeometry().GetLocalSize().X + 1.f && BonusRect.GetSize().Y / Dpi < 30.f);
            TestTrue(TEXT("A discovery badge cannot squeeze or overlap the bonus"), StateRect.Top >= BonusRect.Bottom - 1.f);
            TestTrue(TEXT("The complete card information stays inside its surface"), StateRect.Bottom <= CardBounds.Bottom + 1.f);
        }
        AddInfo(FString::Printf(TEXT("Codex viewport %dx%d uses project DPI %.3f and %s detail."),
            Width, Height, Dpi, Main->GetOrientation() == Orient_Horizontal ? TEXT("side") : TEXT("stacked")));
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
    TestTrue(TEXT("Narrow project-DPI responsive preview"), Capture(1024, 768, TEXT("FacilityCodex1024x768.png")));
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
            auto* UnknownImage = CastChecked<UImage>(UnknownCard->GetWidgetFromName(TEXT("Image_Target")));
            const FSlateColor DiscoveredColor = FoundBonus->GetColorAndOpacity();
            const FSlateColor UnknownColor = UnknownBonus->GetColorAndOpacity();
            const FLinearColor UnknownInk = UnknownColor.GetSpecifiedColor();
            TestTrue(TEXT("Undiscovered bonus remains readable in the green palette"),
                UnknownInk.G >= UnknownInk.R && UnknownInk.G >= UnknownInk.B &&
                UnknownInk.GetLuminance() < .5f);
            TestTrue(TEXT("Undiscovered card can still be selected"), UnknownCard->GetIsEnabled());
            TestFalse(TEXT("Only undiscovered artwork is dimmed"), UnknownImage->GetIsEnabled());
            Tile->SetSelectedItem(Tile->GetListItems()[1]);
            TestEqual(TEXT("Undiscovered card reveals its recipe detail"), Screen->GetSelectedRuleId(), FName(TEXT("IceCreamShade")));
            const FSlateBrush UnknownBrush = UnknownBody->Background;
            Entries[1].bDiscovered = true;
            Screen->SetPreviewEntries(Entries);
            TestTrue(TEXT("Discovery restores the authored accent on the existing card"),
                UnknownBonus->GetColorAndOpacity() == DiscoveredColor);
            TestTrue(TEXT("Discovery restores thumbnail presentation"), UnknownImage->GetIsEnabled());
            Entries[1].bDiscovered = false;
            Screen->SetPreviewEntries(Entries);
            TestTrue(TEXT("A reused unknown card returns to its authored undiscovered palette"),
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
