// Uses the saved catalog and tile WBP classes, including their real pooled Slate rows.
// Synthetic data is confined to this fixture; no catalog, economy or building command runs.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Slate/WidgetRenderer.h"
#include "TextureCompiler.h"
#include "TextureResource.h"
#include "Types/PaintArgs.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationModel.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

namespace FFPrepBuildVirtualizedSlateTest
{
    constexpr int32 ItemCount = 144;
    constexpr const TCHAR* PanelPath =
        TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild.WBP_FF_PrepPanelBuild_C");
    constexpr const TCHAR* TilePath =
        TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile.WBP_FF_PrepBuildTile_C");
    constexpr const TCHAR* HudPath =
        TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepHudBar.WBP_FF_PrepHudBar_C");

    class FPlayerlessInitialization
    {
    public:
        explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
            : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
        {
            Class.bCanCallInitializedWithoutPlayerContext = true;
        }
        ~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
    private:
        UWidgetBlueprintGeneratedClass& Class;
        bool bPrevious;
    };

    struct FMeasurements
    {
        FVector2D CardSize = FVector2D::ZeroVector;
        FVector2D ThumbnailSize = FVector2D::ZeroVector;
        TMap<UUserWidget*, UObject*> LastItemByRow;
        TSet<UObject*> SeenItems;
        int32 ReboundRows = 0;
        int32 MaxRealizedRows = 0;
    };

    class FCatalogFixture
    {
    public:
        ~FCatalogFixture()
        {
#if !UE_BUILD_SHIPPING
            if (Hud && UFF_PrepHudBar::ActiveBar.Get() == Hud.Get()) { UFF_PrepHudBar::ActiveBar = PreviousActiveBar; }
#endif
        }

        bool Initialize(FAutomationTestBase& Test, const FVector2D InViewportSize, const bool bUseGameplayHud = false)
        {
            ViewportSize = InViewportSize;
            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            UWidgetBlueprintGeneratedClass* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(
                LoadClass<UFF_PrepPanelBuild>(nullptr, PanelPath));
            UWidgetBlueprintGeneratedClass* TileClass = Cast<UWidgetBlueprintGeneratedClass>(
                LoadClass<UFF_PrepBuildTileWidget>(nullptr, TilePath));
            if (!Test.TestNotNull(TEXT("Playerless editor world"), World) ||
                !Test.TestNotNull(TEXT("Actual catalog WBP"), PanelClass) ||
                !Test.TestNotNull(TEXT("Actual pooled tile WBP"), TileClass))
            {
                return false;
            }
            const FFloatProperty* HeightProperty = FindFProperty<FFloatProperty>(PanelClass, TEXT("CatalogHeight"));
            if (!Test.TestNotNull(TEXT("Catalog height remains editable in WBP Class Defaults"), HeightProperty)) { return false; }
            const float PanelHeight = HeightProperty->GetPropertyValue_InContainer(PanelClass->GetDefaultObject());
            if (!Test.TestTrue(TEXT("Authored catalog height fits the fixture viewport"), PanelHeight > 0 && PanelHeight < ViewportSize.Y)) { return false; }
            FPlayerlessInitialization PanelInitialization(*PanelClass);
            TileInitialization = MakeUnique<FPlayerlessInitialization>(*TileClass);
            Panel.Reset(CreateWidget<UFF_PrepPanelBuild>(World, PanelClass));
            if (!Test.TestNotNull(TEXT("Catalog instance"), Panel.Get()))
            {
                return false;
            }
            // NativeConstruct runs without opening a gameplay session. Only presentation
            // visibility/labels and the list's public item API are supplied by this fixture.
            const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
            SetVisibility(TEXT("Panel_Catalog"), ESlateVisibility::SelfHitTestInvisible);
            SetVisibility(TEXT("Panel_PlacementCompact"), ESlateVisibility::Collapsed);
            SetVisibility(TEXT("Box_Header"), ESlateVisibility::SelfHitTestInvisible);
            SetText(TEXT("Text_Gold"), TEXT("보유 7,500 Gold"));
            // The playerless world has no catalog subsystem. Keep the saved WBP's current
            // ready-state hint instead of its missing-session error or an old fixture string.
            const UWidgetTree* AuthoredTree = PanelClass->GetWidgetTreeArchetype();
            const UTextBlock* AuthoredStatus = AuthoredTree
                ? Cast<UTextBlock>(AuthoredTree->FindWidget(TEXT("Text_Status"))) : nullptr;
            if (UTextBlock* Status = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Status"))); Status && AuthoredStatus)
            {
                Status->SetText(AuthoredStatus->GetText());
            }
            SetText(TEXT("Text_ItemCount"), TEXT("시설 144 / 144"));
            List = Cast<UFF_PrepBuildTileView>(Panel->GetWidgetFromName(TEXT("TileView_Facilities")));
            if (!Test.TestNotNull(TEXT("Actual virtualized catalog list"), List) ||
                !Test.TestTrue(TEXT("The list retains the production tile WBP"), List->GetEntryWidgetClass() == TileClass))
            {
                return false;
            }
            // Portrait/landscape/large textures expose matching native image size and pool
            // release bugs. These diagnostic thumbnails deliberately differ in dimensions.
            const FIntPoint Sizes[] = { FIntPoint(32, 32), FIntPoint(1024, 256), FIntPoint(256, 2048) };
            const FColor Colors[] = { FColor(22, 133, 125), FColor(228, 164, 71), FColor(90, 136, 170) };
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Sizes); ++Index)
            {
                TArray<FColor> Pixels;
                Pixels.SetNumUninitialized(Sizes[Index].X * Sizes[Index].Y);
                for (int32 Y = 0; Y < Sizes[Index].Y; ++Y)
                {
                    for (int32 X = 0; X < Sizes[Index].X; ++X)
                    {
                        const bool bBuilding = X > Sizes[Index].X / 5 && X < Sizes[Index].X * 4 / 5 &&
                            Y > Sizes[Index].Y / 4 && Y < Sizes[Index].Y * 4 / 5;
                        Pixels[Y * Sizes[Index].X + X] = bBuilding ? Colors[Index] : FColor(229, 240, 234);
                    }
                }
                Textures.Emplace(UTexture2D::CreateTransient(Sizes[Index].X, Sizes[Index].Y,
                    PF_B8G8R8A8, NAME_None, TConstArrayView64<uint8>(
                        reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor))));
                if (!Test.TestNotNull(TEXT("Mixed-resolution thumbnail fixture"), Textures.Last().Get()))
                {
                    return false;
                }
            }
            TArray<UObject*> ListItems;
            for (int32 Index = 0; Index < ItemCount; ++Index)
            {
                FFF_BuildCatalogEntry Entry;
                Entry.FacilityDefinitionId = FName(*FString::Printf(TEXT("Booth_FoodStall_Fixture%03d"), Index));
                Entry.DisplayName = FText::FromString(FString::Printf(TEXT("%s %03d"), Index % 3 == 0
                    ? TEXT("해변에서 즐기는 아주 긴 이름의 푸드트럭") : TEXT("바다향기 푸드트럭"), Index));
                Entry.Category = EFFZSFacilityCategory::Booth;
                Entry.MinorToken = TEXT("FoodStall");
                Entry.InstallCost = 300 + Index * 50;
                Entry.bHasResolvedInstallCost = true;
                Entry.MaxInstancesPerWorld = 3;
                Entry.CurrentInstanceCount = Index % 4;
                Entry.bBuildLimitReached = Entry.CurrentInstanceCount >= Entry.MaxInstancesPerWorld;
                Entry.Thumbnail = Textures[Index % Textures.Num()].Get();
                UFF_PrepBuildTileData* Item = NewObject<UFF_PrepBuildTileData>(Panel.Get());
                Item->Initialize(Entry, 7500);
                ListItems.Add(Item);
            }
            List->SetListItems(ListItems);
            List->SetIsEnabled(true);
            Window = SNew(SVirtualWindow).Size(ViewportSize);
            Window->SetContent(SNew(SOverlay)
                + SOverlay::Slot().VAlign(VAlign_Bottom)
                [ SNew(SBox).HeightOverride(PanelHeight)[PanelSlate] ]);
            return !bUseGameplayHud || AttachGameplayHud(Test, World);
        }

        bool AttachGameplayHud(FAutomationTestBase& Test, UWorld* World)
        {
            auto* HudClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepHudBar>(nullptr, HudPath));
            if (!Test.TestNotNull(TEXT("The teammate HUD uses its actual saved WBP"), HudClass)) { return false; }
            FPlayerlessInitialization HudInitialization(*HudClass);
            Hud.Reset(CreateWidget<UFF_PrepHudBar>(World, HudClass));
            if (!Test.TestNotNull(TEXT("Playerless teammate HUD instance"), Hud.Get())) { return false; }
#if !UE_BUILD_SHIPPING
            PreviousActiveBar = UFF_PrepHudBar::ActiveBar;
#endif
            const TSharedRef<SWidget> HudSlate = Hud->TakeWidget();
            auto* Host = Cast<UPanelWidget>(Hud->GetWidgetFromName(TEXT("Panel_PanelHost")));
            const auto* PanelsProperty = FindFProperty<FMapProperty>(HudClass, TEXT("PanelWidgets"));
            const auto* HeightProperty = FindFProperty<FFloatProperty>(Panel->GetClass(), TEXT("CatalogHeight"));
            const auto* OffsetProperty = FindFProperty<FFloatProperty>(Panel->GetClass(), TEXT("CatalogOffsetY"));
            if (!Test.TestNotNull(TEXT("Actual teammate panel host"), Host) ||
                !Test.TestNotNull(TEXT("Actual HUD cached panels"), PanelsProperty) ||
                !Test.TestNotNull(TEXT("Authored catalog height"), HeightProperty) ||
                !Test.TestNotNull(TEXT("Authored catalog offset above the teammate tabs"), OffsetProperty)) { return false; }
            // Use the real public tab selection without starting a world build session.
            // A detached placeholder absorbs only the panel-open callback, then the real
            // build WBP replaces it in the same transient cache and authored HUD host.
            UClass* PlaceholderClass = LoadClass<UFF_PrepPanelBase>(nullptr,
                TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPlaceholderPanel.WBP_FF_PrepPlaceholderPanel_C"));
            if (!Test.TestNotNull(TEXT("Isolated panel-open placeholder class"), PlaceholderClass)) { return false; }
            auto* Placeholder = CreateWidget<UFF_PrepPanelBase>(World, PlaceholderClass);
            if (!Test.TestNotNull(TEXT("Isolated panel-open placeholder"), Placeholder)) { return false; }
            auto& CachedPanels = *PanelsProperty->ContainerPtrToValuePtr<TMap<EFF_PrepHudTab, TObjectPtr<UFF_PrepPanelBase>>>(Hud.Get());
            CachedPanels.Add(EFF_PrepHudTab::Build, Placeholder);
            Hud->SelectTab(EFF_PrepHudTab::Build);
            Test.TestTrue(TEXT("Actual teammate Build selection opens the catalog host"), Hud->GetSelection().IsSelected(EFF_PrepHudTab::Build));
            CachedPanels.Add(EFF_PrepHudTab::Build, Panel.Get());
            Panel->InitializePanel(Hud.Get(), EFF_PrepHudTab::Build, FText::FromString(TEXT("배치")));
            auto* HostSlot = Cast<UOverlaySlot>(Host->AddChild(Panel.Get()));
            if (!Test.TestNotNull(TEXT("Production HUD overlay slot"), HostSlot)) { return false; }
            HostSlot->SetHorizontalAlignment(HAlign_Fill);
            HostSlot->SetVerticalAlignment(VAlign_Fill);
            Host->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            FFF_PrepPanelLayout Layout = Hud->GetOpenPanelLayout();
            Test.TestTrue(TEXT("The real Build tab remains bottom docked"), Layout.Dock == EFF_PrepPanelDock::Bottom);
            Layout.Size.Y = HeightProperty->GetPropertyValue_InContainer(Panel.Get());
            Layout.Offset.Y = OffsetProperty->GetPropertyValue_InContainer(Panel.Get());
            Hud->SetOpenPanelLayout(Layout);
            DpiScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(
                FIntPoint(FMath::RoundToInt(ViewportSize.X), FMath::RoundToInt(ViewportSize.Y)));
            Window->SetContent(SNew(SDPIScaler).DPIScale(DpiScale)[HudSlate]);
            Test.TestTrue(TEXT("Fixture cannot access player preferences or a festival save"),
                Panel->GetGameInstance() == nullptr && Hud->GetGameInstance() == nullptr);
            Test.AddInfo(FString::Printf(TEXT("Gameplay HUD fixture: %.0fx%.0f, project DPI %.4f, logical viewport %.1fx%.1f, catalog %.0f high / offset %.0f"),
                ViewportSize.X, ViewportSize.Y, DpiScale, ViewportSize.X / DpiScale, ViewportSize.Y / DpiScale, Layout.Size.Y, Layout.Offset.Y));
            return true;
        }

        bool CheckFavoriteInputIsolation(FAutomationTestBase& Test, const FName FacilityId = NAME_None)
        {
            // No GameInstance means the production star handler cannot read or save personal preferences.
            if (!Test.TestTrue(TEXT("Pointer fixture is isolated from the user's GameInstance/preferences"),
                Panel->GetGameInstance() == nullptr)) { return false; }
            UButton* Favorite = nullptr;
            const FSlateRect ListRect = List->GetCachedGeometry().GetLayoutBoundingRect();
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                const auto* Item = Cast<UFF_PrepBuildTileData>(List->GetListObjectFromEntry(*Row));
                if (!FacilityId.IsNone() && (!Item || Item->GetFacilityDefinitionId() != FacilityId)) { continue; }
                auto* Candidate = Cast<UButton>(Row->GetWidgetFromName(TEXT("Button_Favorite")));
                if (!Candidate) { continue; }
                const FSlateRect Rect = Candidate->GetCachedGeometry().GetLayoutBoundingRect();
                if (Rect.Top >= ListRect.Top && Rect.Bottom <= ListRect.Bottom && Rect.GetSize().X > 10)
                {
                    Favorite = Candidate;
                    break;
                }
            }
            if (!Test.TestNotNull(TEXT("A visible production card has its independent favorite button"), Favorite)) { return false; }
            const FGeometry& Geometry = Favorite->GetCachedGeometry();
            const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
            TArray<FWidgetAndPointer> Bubble = HitGrid.GetBubblePath(Point, 0.0f, false);
            FWidgetPath Path(MakeArrayView(Bubble));
            if (!Test.TestTrue(TEXT("The actual painted star is hit-testable"),
                Path.IsValid() && Path.ContainsWidget(Favorite->GetCachedWidget().Get()))) { return false; }
            Test.TestTrue(TEXT("The star retains its native action binding"), Favorite->OnClicked.IsBound());
            int32 PlacementActivations = 0;
            const FDelegateHandle ClickHandle = List->OnItemClicked().AddLambda(
                [&PlacementActivations](UObject*) { ++PlacementActivations; });
            FSlateApplication& App = FSlateApplication::Get();
            const TSharedPtr<SWidget> PreviousFocus = App.GetUserFocusedWidget(0);
            const TSet<FKey> Pressed = { EKeys::LeftMouseButton };
            const TSet<FKey> Released;
            const FPointerEvent Down(0, Point, Point, Pressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
            const FPointerEvent Up(0, Point, Point, Released, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
            Test.TestTrue(TEXT("Star consumes pointer-down before its TileView row"), App.RoutePointerDownEvent(Path, Down).IsEventHandled());
            Test.TestTrue(TEXT("The actual star entered its pressed state"), Favorite->IsPressed());
            Test.TestTrue(TEXT("Star consumes pointer-up before its TileView row"), App.RoutePointerUpEvent(Path, Up).IsEventHandled());
            Test.TestFalse(TEXT("The star releases its pressed state"), Favorite->IsPressed());
            List->OnItemClicked().Remove(ClickHandle);
            if (PreviousFocus) { App.SetUserFocus(0, PreviousFocus, EFocusCause::SetDirectly); }
            Test.TestEqual(TEXT("A full favorite click emits no facility placement activation"), PlacementActivations, 0);
            return true;
        }

        bool UseSourceCatalogEntries(FAutomationTestBase& Test)
        {
            // Discover the same complete Primary Asset set as the production catalog.
            // A curated list injected directly into TileView previously left PricedEntries
            // empty, so the screenshot never created the overflowing real subcategory row.
            TArray<FPrimaryAssetId> AssetIds;
            UAssetManager& Assets = UAssetManager::Get();
            Assets.GetPrimaryAssetIdList(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), AssetIds);
            AssetIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
            const UFF_FestivalDefinition* Festival = LoadObject<UFF_FestivalDefinition>(nullptr,
                TEXT("/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan.DA_Festival_Busan"));
            if (!Test.TestNotNull(TEXT("Production festival definition owns the catalog costs"), Festival)) { return false; }
            const UDataTable* Costs = Festival->FacilityCostTable.Get();
            if (!Test.TestNotNull(TEXT("Production facility price table"), Costs)) { return false; }
            TArray<UTexture*> SourceTextures;
            TSet<FName> DefinitionIds;
            TSet<EFFZSFacilityCategory> Categories;
            SourceEntries.Reset();
            for (const FPrimaryAssetId& AssetId : AssetIds)
            {
                auto* Definition = Cast<UFFZSFacilityDefinition>(Assets.GetPrimaryAssetPath(AssetId).TryLoad());
                if (!Test.TestNotNull(*FString::Printf(TEXT("Registered source facility %s"), *AssetId.ToString()), Definition)) { return false; }
                if (!Definition->bShowInBuildCatalog) { continue; }
                const FString DefinitionName = Definition->Spec.FacilityDefinitionId.ToString();
                TArray<FText> DefinitionErrors;
                FFF_FacilityNameParts Parts;
                FString ParseFailure;
                if (!Test.TestTrue(*FString::Printf(TEXT("Active source definition is valid: %s"), *DefinitionName),
                    Definition->IsDefinitionValid(DefinitionErrors) && UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
                        Definition->Spec.Category, Definition->Spec.FacilityDefinitionId, Parts, ParseFailure))) { return false; }
                if (!Test.TestFalse(TEXT("Active source IDs are unique"), DefinitionIds.Contains(Definition->Spec.FacilityDefinitionId))) { return false; }
                DefinitionIds.Add(Definition->Spec.FacilityDefinitionId);
                Categories.Add(Definition->Spec.Category);
                Definitions.Emplace(Definition);
                UTexture2D* Texture = Definition->CatalogThumbnail.LoadSynchronous();
                if (!Test.TestNotNull(*FString::Printf(TEXT("Real facility thumbnail %s"), *DefinitionName), Texture)) { return false; }
                Textures.Emplace(Texture);
                SourceTextures.Add(Texture);
                FFF_BuildCatalogEntry Entry;
                Entry.PrimaryAssetId = Definition->GetPrimaryAssetId();
                Entry.FacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
                Entry.DisplayName = Definition->Spec.DisplayName;
                Entry.Category = Definition->Spec.Category;
                Entry.MinorToken = Parts.MinorToken;
                Entry.UniqueNameToken = Parts.UniqueToken;
                Entry.Definition = Definition;
                Entry.Thumbnail = Texture;
                Entry.MaxInstancesPerWorld = Definition->Spec.MaxInstancesPerWorld;
                FString CostFailure;
                if (!Test.TestTrue(*FString::Printf(TEXT("Source facility price resolves: %s"), *DefinitionName),
                    FFBuildCatalogModel::ApplyFacilityCosts(Entry, Definition, Costs,
                        Festival->FacilityDemolitionRefundRate, CostFailure)))
                {
                    Test.AddError(FString::Printf(TEXT("%s: %s"), *DefinitionName, *CostFailure));
                    return false;
                }
                SourceEntries.Add(MoveTemp(Entry));
            }
            if (!Test.TestTrue(TEXT("Full fixture covers all 45 existing active facilities, including future additions"), SourceEntries.Num() >= 45) ||
                !Test.TestEqual(TEXT("Full source fixture contains every production major category"), Categories.Num(), 10)) { return false; }
            FFBuildCatalogModel::SortEntries(SourceEntries);
            DetachedGame.Reset(NewObject<UGameInstance>());
            Catalog.Reset(NewObject<UFF_BuildCatalogSubsystem>(DetachedGame.Get()));
            Economy.Reset(NewObject<UFF_BuildCatalogEconomyReadModel>(DetachedGame.Get()));
            // None of these detached subsystems is initialized or registered with a world.
            // Their read snapshots exercise actual filtering without touching game saves.
            const auto* EntriesProperty = FindFProperty<FArrayProperty>(Catalog->GetClass(), TEXT("Entries"));
            const auto* StateProperty = FindFProperty<FEnumProperty>(Catalog->GetClass(), TEXT("LoadState"));
            const auto* PricedProperty = FindFProperty<FArrayProperty>(Panel->GetClass(), TEXT("PricedEntries"));
            auto* CatalogProperty = FindFProperty<FObjectPropertyBase>(Panel->GetClass(), TEXT("CatalogSubsystem"));
            auto* EconomyProperty = FindFProperty<FObjectPropertyBase>(Panel->GetClass(), TEXT("EconomyReadModel"));
            UFunction* GoldHandler = Economy->FindFunction(TEXT("HandleProgressionGoldChanged"));
            if (!Test.TestNotNull(TEXT("Reflected catalog snapshot"), EntriesProperty) ||
                !Test.TestNotNull(TEXT("Reflected catalog ready state"), StateProperty) ||
                !Test.TestNotNull(TEXT("Panel's real priced source entries"), PricedProperty) ||
                !Test.TestNotNull(TEXT("Panel catalog input"), CatalogProperty) ||
                !Test.TestNotNull(TEXT("Panel economy input"), EconomyProperty) ||
                !Test.TestNotNull(TEXT("Detached economy snapshot event"), GoldHandler)) { return false; }
            *EntriesProperty->ContainerPtrToValuePtr<TArray<FFF_BuildCatalogEntry>>(Catalog.Get()) = SourceEntries;
            StateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                StateProperty->ContainerPtrToValuePtr<void>(Catalog.Get()), static_cast<uint64>(EFF_BuildCatalogLoadState::Ready));
            *PricedProperty->ContainerPtrToValuePtr<TArray<FFF_BuildCatalogEntry>>(Panel.Get()) = SourceEntries;
            CatalogProperty->SetObjectPropertyValue_InContainer(Panel.Get(), Catalog.Get());
            EconomyProperty->SetObjectPropertyValue_InContainer(Panel.Get(), Economy.Get());
            struct { int32 NewGold = 7500; } GoldParameters;
            Economy->ProcessEvent(GoldHandler, &GoldParameters);
            UFunction* AllHandler = Panel->FindFunction(TEXT("HandleFilterAllClicked"));
            if (!Test.TestNotNull(TEXT("Production category filter event"), AllHandler)) { return false; }
            Panel->ProcessEvent(AllHandler, nullptr);
            Test.AddInfo(FString::Printf(TEXT("Complete source catalog: %d active facilities across %d categories; actual Busan installation prices; transient 7500 Gold"),
                SourceEntries.Num(), Categories.Num()));
            // LoadSynchronous resolves the UObject before editor texture compilation and
            // streaming finish. Wait for the selected source thumbnails before GPU capture.
            FTextureCompilingManager::Get().FinishCompilation(SourceTextures);
            for (UTexture* Texture : SourceTextures)
            {
                Texture->SetForceMipLevelsToBeResident(30.0f);
                Texture->WaitForStreaming();
            }
            return true;
        }

        bool CheckFacilityNavigation(FAutomationTestBase& Test)
        {
            const FName PartnerId(TEXT("Rest_Seating_Seating"));
            auto* Search = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("Search_Facilities")));
            auto* Favorites = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_FavoritesOnly")));
            const auto* StateProperty = FindFProperty<FEnumProperty>(Catalog->GetClass(), TEXT("LoadState"));
            const auto* PageSize = FindFProperty<FIntProperty>(Panel->GetClass(), TEXT("CardsPerPage"));
            UFunction* AllHandler = Panel->FindFunction(TEXT("HandleFilterAllClicked"));
            UFunction* PerformanceHandler = Panel->FindFunction(TEXT("HandleFilterPerformanceClicked"));
            UFunction* ModeHandler = Panel->FindFunction(TEXT("HandleBuildModeStateChanged"));
            if (!Test.TestNotNull(TEXT("Actual catalog search"), Search) ||
                !Test.TestNotNull(TEXT("Actual favorites filter"), Favorites) ||
                !Test.TestNotNull(TEXT("Catalog loading state"), StateProperty) ||
                !Test.TestNotNull(TEXT("Designer page capacity"), PageSize) ||
                !Test.TestNotNull(TEXT("All category action"), AllHandler) ||
                !Test.TestNotNull(TEXT("Performance category action"), PerformanceHandler) ||
                !Test.TestNotNull(TEXT("Build mode event"), ModeHandler)) { return false; }
            Search->SetText(FText::FromString(TEXT("NoSuchFacility")));
            Favorites->OnClicked.Broadcast();
            StateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                StateProperty->ContainerPtrToValuePtr<void>(Catalog.Get()), static_cast<uint64>(EFF_BuildCatalogLoadState::Loading));
            Panel->RequestFacilityFocus(PartnerId);
            Test.TestTrue(TEXT("A jump during loading does not expose stale cards"), Panel->GetCatalogPageItems().IsEmpty());
            struct { EFF_BuildModeState Previous = EFF_BuildModeState::CatalogOpen; EFF_BuildModeState Next = EFF_BuildModeState::BuildMode; } ModeParameters;
            Panel->ProcessEvent(ModeHandler, &ModeParameters);
            StateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                StateProperty->ContainerPtrToValuePtr<void>(Catalog.Get()), static_cast<uint64>(EFF_BuildCatalogLoadState::Ready));
            Panel->ProcessEvent(AllHandler, nullptr);
            Test.TestTrue(TEXT("Leaving the catalog cancels a pending jump; late readiness keeps the old search"),
                Panel->GetCatalogPageItems().IsEmpty() && !Search->GetText().IsEmpty());
            StateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                StateProperty->ContainerPtrToValuePtr<void>(Catalog.Get()), static_cast<uint64>(EFF_BuildCatalogLoadState::Loading));
            Panel->RequestFacilityFocus(PartnerId);
            StateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                StateProperty->ContainerPtrToValuePtr<void>(Catalog.Get()), static_cast<uint64>(EFF_BuildCatalogLoadState::Ready));
            // Reconcile after readiness must honor the retained request, not the old filters.
            Panel->ProcessEvent(AllHandler, nullptr);
            auto* Target = Cast<UFF_PrepBuildTileData>(List->GetSelectedItem());
            if (!Test.TestTrue(TEXT("Deferred navigation clears search/favorites and selects the actual Seating card"),
                Target && Target->GetFacilityDefinitionId() == PartnerId &&
                Panel->GetCatalogPageItems().Contains(Target) && Search->GetText().IsEmpty())) { return false; }
            Panel->ProcessEvent(PerformanceHandler, nullptr);
            const TArray<UObject*> ManualItems = Panel->GetCatalogPageItems();
            Test.TestTrue(TEXT("Later player category choice is not forced back to the consumed jump"),
                !ManualItems.IsEmpty() && !ManualItems.Contains(Target));

            const int32 OriginalPageSize = PageSize->GetPropertyValue_InContainer(Panel.Get());
            PageSize->SetPropertyValue_InContainer(Panel.Get(), 1);
            Panel->RequestFacilityFocus(TEXT("Booth_FoodStall_Taco"));
            auto* LaterTile = Cast<UFF_PrepBuildTileData>(List->GetSelectedItem());
            Test.TestTrue(TEXT("A target beyond page one is visible and selected on its own page"),
                Panel->GetCatalogPageIndex() > 0 && LaterTile &&
                LaterTile->GetFacilityDefinitionId() == FName(TEXT("Booth_FoodStall_Taco")) &&
                Panel->GetCatalogPageItems().Contains(LaterTile));
            PageSize->SetPropertyValue_InContainer(Panel.Get(), OriginalPageSize);
            Panel->RequestFacilityFocus(PartnerId);
            Paint();
            Test.TestEqual(TEXT("Returning to a different partner restores the appropriate page"), Panel->GetCatalogPageIndex(), 0);
            bool bFoundRow = false;
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                const auto* Item = Cast<UFF_PrepBuildTileData>(List->GetListObjectFromEntry(*Row));
                bFoundRow |= Item && Item->GetFacilityDefinitionId() == PartnerId;
            }
            Test.TestTrue(TEXT("Target card is actually generated by the saved WBP"), bFoundRow);
            return CheckFavoriteInputIsolation(Test, PartnerId);
        }

        bool UseSynergyCatalogPreview(FAutomationTestBase& Test)
        {
            const FName PartnerId(TEXT("Rest_Seating_Seating"));
            const FFF_BuildCatalogEntry* Source = SourceEntries.FindByPredicate([](const FFF_BuildCatalogEntry& Entry)
            {
                return Entry.FacilityDefinitionId == FName(TEXT("Booth_FoodStall_Burger"));
            });
            const auto* ModelProperty = FindFProperty<FObjectPropertyBase>(Panel->GetClass(), TEXT("PresentationModel"));
            auto* Model = ModelProperty ? Cast<UFF_PrepBuildPresentationModel>(
                ModelProperty->GetObjectPropertyValue_InContainer(Panel.Get())) : nullptr;
            if (!Test.TestNotNull(TEXT("Actual Burger catalog entry supplies the selected facility name"), Source) ||
                !Test.TestNotNull(TEXT("Actual panel presentation model"), Model)) { return false; }
            Model->SetSynergyHighlightIds({ PartnerId });
            Model->Reconcile(EFF_BuildCatalogLoadState::Ready, SourceEntries,
                FFF_PrepBuildFilter(), 7500, true, FText::GetEmpty());
            const TArray<UObject*> PageItems = Panel->GetCatalogPageItems();
            const auto* FirstItem = PageItems.IsEmpty() ? nullptr : Cast<UFF_PrepBuildTileData>(PageItems[0]);
            if (!Test.TestTrue(TEXT("The actual Seating partner sorts to the first six-card page"),
                FirstItem && FirstItem->GetFacilityDefinitionId() == PartnerId && FirstItem->IsSynergyHighlighted() &&
                PageItems.Num() == 6)) { return false; }
            // The detached fixture has no world selection subsystem; feed its real pooled
            // model page directly while retaining the saved WBP, prices and thumbnails.
            List->SetListItems(PageItems);
            List->ScrollToTop();
            List->RegenerateAllEntries();
            auto* Status = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Status")));
            if (!Test.TestNotNull(TEXT("The actual status widget explains the selected facility's recommendation"), Status)) { return false; }
            Status->SetText(FText::Format(FText::FromString(TEXT("{0} 조합 1/1종 강조 · 배치 시 거리 조건 확인")), Source->DisplayName));
            return true;
        }

        bool CheckSynergyCatalogPreview(FAutomationTestBase& Test)
        {
            UFF_PrepBuildTileWidget* RecommendedRow = nullptr;
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                const auto* Item = Cast<UFF_PrepBuildTileData>(List->GetListObjectFromEntry(*Row));
                if (Item && Item->GetFacilityDefinitionId() == FName(TEXT("Rest_Seating_Seating")))
                {
                    RecommendedRow = Cast<UFF_PrepBuildTileWidget>(Row);
                    break;
                }
            }
            if (!Test.TestNotNull(TEXT("The recommended Seating card is actually rendered"), RecommendedRow)) { return false; }
            auto* Highlight = Cast<UBorder>(RecommendedRow->GetWidgetFromName(TEXT("Border_SynergyHighlight")));
            auto* Badge = Cast<UTextBlock>(RecommendedRow->GetWidgetFromName(TEXT("Text_SynergyBadge")));
            auto* Card = Cast<UBorder>(RecommendedRow->GetWidgetFromName(TEXT("Border_Card")));
            auto* Favorite = Cast<UButton>(RecommendedRow->GetWidgetFromName(TEXT("Button_Favorite")));
            if (!Test.TestNotNull(TEXT("Saved WBP recommendation outline"), Highlight) ||
                !Test.TestNotNull(TEXT("Saved WBP recommendation badge"), Badge) ||
                !Test.TestNotNull(TEXT("Recommended card frame"), Card) ||
                !Test.TestNotNull(TEXT("Recommended card retains its favorite button"), Favorite)) { return false; }
            Test.TestTrue(TEXT("Recommended data reaches the actual pooled widget"), RecommendedRow->IsSynergyHighlighted());
            Test.TestTrue(TEXT("Recommendation outline and readable badge are visible without intercepting input"),
                Highlight->GetVisibility() == ESlateVisibility::HitTestInvisible &&
                Badge->GetVisibility() == ESlateVisibility::HitTestInvisible && !Badge->GetText().IsEmpty());
            const FSlateRect CardRect = Card->GetCachedGeometry().GetLayoutBoundingRect();
            for (const UWidget* Decoration : { static_cast<UWidget*>(Highlight), static_cast<UWidget*>(Badge) })
            {
                const FSlateRect Rect = Decoration->GetCachedGeometry().GetLayoutBoundingRect();
                Test.TestTrue(TEXT("Recommendation outline and badge fit within the actual card at 1280x720"),
                    Rect.GetSize().X > 1.0f && Rect.GetSize().Y > 1.0f &&
                    Rect.Left >= CardRect.Left - 1.5f && Rect.Right <= CardRect.Right + 1.5f &&
                    Rect.Top >= CardRect.Top - 1.5f && Rect.Bottom <= CardRect.Bottom + 1.5f);
            }
            const FGeometry& FavoriteGeometry = Favorite->GetCachedGeometry();
            const FVector2D Point = FavoriteGeometry.LocalToAbsolute(FavoriteGeometry.GetLocalSize() * 0.5);
            TArray<FWidgetAndPointer> Bubble = HitGrid.GetBubblePath(Point, 0.0f, false);
            FWidgetPath Path(MakeArrayView(Bubble));
            return Test.TestTrue(TEXT("Recommendation decorations preserve favorite hit-testing on the highlighted card"),
                Path.IsValid() && Path.ContainsWidget(Favorite->GetCachedWidget().Get()));
        }

        bool UseRequiredCatalogPreview(FAutomationTestBase& Test, const bool bInstalled)
        {
            const auto* Festival = LoadObject<UFF_FestivalDefinition>(nullptr,
                TEXT("/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan.DA_Festival_Busan"));
            if (!Test.TestNotNull(TEXT("Required preview uses the actual festival definition"), Festival)) { return false; }
            FFFZSReadModel WorldSnapshot;
            const FFF_RequiredFacilitiesStatus Before = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Festival, WorldSnapshot);
            const FFF_RequiredFacilityRow* Requirement = Before.Facilities.FindByPredicate([](const FFF_RequiredFacilityRow& Row)
            {
                return Row.bDefinitionValid && Row.Category == EFFZSFacilityCategory::Performance;
            });
            if (!Test.TestNotNull(TEXT("The actual festival authors a required performance facility"), Requirement)) { return false; }
            RequiredPreviewFacilityId = Requirement->FacilityDefinitionId;
            FFF_BuildCatalogEntry* Source = SourceEntries.FindByPredicate([this](const FFF_BuildCatalogEntry& Entry)
            {
                return Entry.FacilityDefinitionId == RequiredPreviewFacilityId;
            });
            if (!Test.TestTrue(TEXT("The festival requirement resolves to the actual one-per-world SunsetMainStage"),
                Source && Source->UniqueNameToken == TEXT("SunsetMainStage") &&
                Source->MaxInstancesPerWorld == 1 && Requirement->MinimumCount == 1)) { return false; }
            if (bInstalled)
            {
                // Only the world read snapshot is synthetic. No actor, asset or save is changed.
                FFFZSFacilityReadModel& Installed = WorldSnapshot.Facilities.AddDefaulted_GetRef();
                Installed.FacilityDefinitionId = RequiredPreviewFacilityId;
                Installed.Category = EFFZSFacilityCategory::Performance;
                Installed.State = EFFZSFacilityState::Validated;
            }
            const FFF_RequiredFacilitiesStatus Status = bInstalled
                ? UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Festival, WorldSnapshot) : Before;
            FFBuildCatalogModel::ApplyRequiredFacilities(SourceEntries, Status);
            Source->CurrentInstanceCount = WorldSnapshot.Facilities.Num();
            Source->bBuildLimitReached = Source->CurrentInstanceCount >= Source->MaxInstancesPerWorld;

            UFunction* PerformanceHandler = Panel->FindFunction(TEXT("HandleFilterPerformanceClicked"));
            const auto* ModelProperty = FindFProperty<FObjectPropertyBase>(Panel->GetClass(), TEXT("PresentationModel"));
            auto* Model = ModelProperty ? Cast<UFF_PrepBuildPresentationModel>(
                ModelProperty->GetObjectPropertyValue_InContainer(Panel.Get())) : nullptr;
            if (!Test.TestNotNull(TEXT("Production performance category action"), PerformanceHandler) ||
                !Test.TestNotNull(TEXT("Actual panel presentation model"), Model)) { return false; }
            Panel->ProcessEvent(PerformanceHandler, nullptr);
            FFF_PrepBuildFilter Filter;
            Filter.bFilterByCategory = true;
            Filter.Category = EFFZSFacilityCategory::Performance;
            // The playerless panel has no required-facilities subsystem. Supply the same
            // authoritative projection directly to its real model after normal filtering.
            Model->Reconcile(EFF_BuildCatalogLoadState::Ready, SourceEntries, Filter, 7500, true, FText::GetEmpty());
            UFF_PrepBuildTileData* Tile = Model->FindTile(RequiredPreviewFacilityId);
            if (!Test.TestNotNull(TEXT("Required facility remains in the production tile pool"), Tile)) { return false; }
            if (RequiredPreviewTile.IsValid())
            {
                Test.TestTrue(TEXT("Installation updates the same pooled tile object"), RequiredPreviewTile.Get() == Tile);
            }
            RequiredPreviewTile = Tile;
            Test.TestTrue(TEXT("Required status uses the authoritative before/after result"),
                Tile->IsRequiredFacility() && Tile->IsRequiredFacilitySatisfied() == bInstalled &&
                Tile->GetRequiredFacilityCurrentCount() == (bInstalled ? 1 : 0));
            List->SetListItems(Panel->GetCatalogPageItems());
            List->ScrollToTop();
            List->RegenerateAllEntries();
            return true;
        }

        bool CheckRequiredCatalogPreview(FAutomationTestBase& Test, const bool bInstalled)
        {
            UFF_PrepBuildTileWidget* RequiredRow = nullptr;
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                const auto* Item = Cast<UFF_PrepBuildTileData>(List->GetListObjectFromEntry(*Row));
                if (Item && Item->GetFacilityDefinitionId() == RequiredPreviewFacilityId)
                {
                    RequiredRow = Cast<UFF_PrepBuildTileWidget>(Row);
                    break;
                }
            }
            if (!Test.TestNotNull(TEXT("The actual required main stage card is painted"), RequiredRow)) { return false; }
            auto* State = Cast<UTextBlock>(RequiredRow->GetWidgetFromName(TEXT("Text_State")));
            auto* Card = Cast<UBorder>(RequiredRow->GetWidgetFromName(TEXT("Border_Card")));
            if (!Test.TestNotNull(TEXT("The existing status slot shows installation count"), State) ||
                !Test.TestNotNull(TEXT("Required facility card frame"), Card)) { return false; }
            Test.TestEqual(TEXT("The actual saved status slot changes from 0/1 to 1/1"),
                State->GetText().ToString(), FString(bInstalled ? TEXT("1/1") : TEXT("0/1")));
            Test.TestTrue(TEXT("The real row receives required/satisfied state"),
                RequiredRow->IsRequiredFacility() && RequiredRow->IsRequiredFacilitySatisfied() == bInstalled);
            const FSlateRect CardRect = Card->GetCachedGeometry().GetLayoutBoundingRect();
            for (const bool bSatisfiedDecoration : { false, true })
            {
                auto* Border = Cast<UBorder>(RequiredRow->GetWidgetFromName(bSatisfiedDecoration
                    ? TEXT("Border_RequiredSatisfied") : TEXT("Border_RequiredMissing")));
                auto* Badge = Cast<UTextBlock>(RequiredRow->GetWidgetFromName(bSatisfiedDecoration
                    ? TEXT("Text_RequiredSatisfied") : TEXT("Text_RequiredMissing")));
                if (!Test.TestNotNull(TEXT("Required status border is authored in the saved WBP"), Border) ||
                    !Test.TestNotNull(TEXT("Required status label is authored in the saved WBP"), Badge)) { return false; }
                const bool bActive = bSatisfiedDecoration == bInstalled;
                const ESlateVisibility Expected = bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
                Test.TestTrue(TEXT("Only the matching required badge is visible and neither badge blocks input"),
                    Border->GetVisibility() == Expected && Badge->GetVisibility() == Expected);
                if (!bActive) { continue; }
                Test.TestEqual(TEXT("Required badge retains the authored readable label"), Badge->GetText().ToString(),
                    FString(bInstalled ? TEXT("필수 충족") : TEXT("필수 설치")));
                for (const UWidget* Decoration : { static_cast<UWidget*>(Border), static_cast<UWidget*>(Badge) })
                {
                    const FSlateRect Rect = Decoration->GetCachedGeometry().GetLayoutBoundingRect();
                    Test.TestTrue(TEXT("Required badge fits inside the actual card at 1280x720"),
                        Rect.GetSize().X > 1.0f && Rect.GetSize().Y > 1.0f &&
                        Rect.Left >= CardRect.Left - 1.5f && Rect.Right <= CardRect.Right + 1.5f &&
                        Rect.Top >= CardRect.Top - 1.5f && Rect.Bottom <= CardRect.Bottom + 1.5f);
                }
            }
            return CheckFavoriteInputIsolation(Test, RequiredPreviewFacilityId);
        }

        void Paint()
        {
            // This is Slate's real slow paint path. SWidget::Paint ticks STableViewBase,
            // which generates/releases rows; it also works with NullRHI without GPU work.
            constexpr float Delta = 1.0f / 60.0f;
            for (int32 Pass = 0; Pass < 4; ++Pass)
            {
                Window->SlatePrepass(1.0f);
                HitGrid.SetHittestArea(FVector2D::ZeroVector, ViewportSize);
                HitGrid.Clear();
                FSlateWindowElementList Elements(Window);
                const FGeometry Geometry = FGeometry::MakeRoot(ViewportSize, FSlateLayoutTransform(1.0f));
                const FPaintArgs Args(nullptr, HitGrid, FVector2D::ZeroVector,
                    FPlatformTime::Seconds() + (++FrameNumber * Delta), Delta);
                Window->Paint(Args, Geometry, Geometry.GetLayoutBoundingRect(), Elements, 0, FWidgetStyle(), true);
            }
        }

        bool CheckFilterLayout(FAutomationTestBase& Test, const FString& State)
        {
            auto* Favorites = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_FavoritesOnly")));
            auto* Label = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_FavoritesOnly")));
            auto* Minors = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_Subcategories")));
            auto* Majors = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_Categories")));
            if (!Test.TestNotNull(TEXT("Independent favorite filter"), Favorites) ||
                !Test.TestNotNull(TEXT("Favorite filter readable label"), Label) ||
                !Test.TestNotNull(TEXT("Real generated subcategory row"), Minors) ||
                !Test.TestNotNull(TEXT("Real generated category row"), Majors)) { return false; }
            const FSlateRect FavoriteRect = Favorites->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect LabelRect = Label->GetCachedGeometry().GetLayoutBoundingRect();
            Test.TestTrue(*FString::Printf(TEXT("%s: favorites has a visible independent hit area inside the viewport"), *State),
                Favorites->IsVisible() && Favorites->GetIsEnabled() && FavoriteRect.GetSize().X >= 100.0 * DpiScale &&
                FavoriteRect.GetSize().Y >= 20.0 * DpiScale && FavoriteRect.Left >= 0 && FavoriteRect.Right <= ViewportSize.X &&
                FavoriteRect.Top >= 0 && FavoriteRect.Bottom <= ViewportSize.Y);
            Test.TestTrue(*FString::Printf(TEXT("%s: the complete favorite label fits its button"), *State),
                LabelRect.Left >= FavoriteRect.Left - 1 && LabelRect.Right <= FavoriteRect.Right + 1 &&
                LabelRect.Top >= FavoriteRect.Top - 1 && LabelRect.Bottom <= FavoriteRect.Bottom + 1);
            for (UHorizontalBox* Choices : { Majors, Minors })
            {
                for (UWidget* Choice : Choices->GetAllChildren())
                {
                    const FSlateRect Rect = Choice->GetCachedGeometry().GetLayoutBoundingRect();
                    const bool bOverlap = Rect.Left < FavoriteRect.Right - 1 && Rect.Right > FavoriteRect.Left + 1 &&
                        Rect.Top < FavoriteRect.Bottom - 1 && Rect.Bottom > FavoriteRect.Top + 1;
                    Test.TestFalse(*FString::Printf(TEXT("%s: %s never overlaps the favorite filter"), *State, *Choice->GetName()), bOverlap);
                }
            }
            // Geometry alone misses widgets that paint or hit-test above the button. Probe
            // its left, middle and right through the actual clipped Slate hit-test grid.
            for (const double Fraction : { 0.15, 0.5, 0.85 })
            {
                const FVector2D Point(FMath::Lerp(FavoriteRect.Left, FavoriteRect.Right, Fraction),
                    (FavoriteRect.Top + FavoriteRect.Bottom) * 0.5);
                TArray<FWidgetAndPointer> Bubble = HitGrid.GetBubblePath(Point, 0.0f, false);
                FWidgetPath Path(MakeArrayView(Bubble));
                Test.TestTrue(*FString::Printf(TEXT("%s: favorite filter remains clickable at %.0f percent of its width"), *State, Fraction * 100),
                    Path.IsValid() && Path.ContainsWidget(Favorites->GetCachedWidget().Get()));
            }
            if (Hud)
            {
                const UWidget* Host = Hud->GetWidgetFromName(TEXT("Panel_PanelHost"));
                const auto* CanvasSlot = Host ? Cast<UCanvasPanelSlot>(Host->Slot) : nullptr;
                if (!Test.TestNotNull(TEXT("Teammate HUD retains its canvas host contract"), CanvasSlot)) { return false; }
                const FSlateRect HostRect = Host->GetCachedGeometry().GetLayoutBoundingRect();
                const FMargin Offsets = CanvasSlot->GetOffsets();
                Test.TestTrue(*FString::Printf(TEXT("%s: project DPI scales the real HUD catalog width, height and bottom offset"), *State),
                    FMath::IsNearlyEqual(HostRect.Left, static_cast<float>(Offsets.Left * DpiScale), 1.5f) &&
                    FMath::IsNearlyEqual(HostRect.Right, static_cast<float>(ViewportSize.X - Offsets.Right * DpiScale), 1.5f) &&
                    FMath::IsNearlyEqual(HostRect.Bottom, static_cast<float>(ViewportSize.Y + Offsets.Top * DpiScale), 1.5f) &&
                    FMath::IsNearlyEqual(HostRect.GetSize().Y, Offsets.Bottom * DpiScale, 1.5f));
                int32 VisibleTabs = 0;
                for (const TCHAR* TabName : { TEXT("Tab_Build"), TEXT("Tab_Program"), TEXT("Tab_Staff"),
                    TEXT("Tab_Promotion"), TEXT("Tab_Finale"), TEXT("Tab_Check") })
                {
                    const UWidget* Tab = Hud->GetWidgetFromName(TabName);
                    if (!Test.TestNotNull(TEXT("Teammate tab is preserved in the real WBP"), Tab)) { return false; }
                    if (!Tab->IsVisible()) { continue; }
                    ++VisibleTabs;
                    const FSlateRect TabRect = Tab->GetCachedGeometry().GetLayoutBoundingRect();
                    Test.TestTrue(*FString::Printf(TEXT("%s: catalog stays above teammate %s"), *State, TabName),
                        TabRect.GetSize().Y > 1 && HostRect.Bottom <= TabRect.Top + 1.5);
                }
                Test.TestTrue(TEXT("The integrated fixture actually paints the teammate navigation"), VisibleTabs >= 4);
            }
            return true;
        }

        bool CheckSixCardPage(FAutomationTestBase& Test, const FString& State)
        {
            const TArray<UObject*> Items = Panel->GetCatalogPageItems();
            Test.TestEqual(*FString::Printf(TEXT("%s: TileView contains the public current page only"), *State), List->GetListItems().Num(), Items.Num());
            Test.TestTrue(*FString::Printf(TEXT("%s: page contains at most six facilities"), *State), Items.Num() > 0 && Items.Num() <= 6);
            const FSlateRect ListRect = List->GetCachedGeometry().GetLayoutBoundingRect();
            TArray<FSlateRect> Rects;
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                if (const UWidget* Card = Row->GetWidgetFromName(TEXT("Border_Card")))
                {
                    const FSlateRect Rect = Card->GetCachedGeometry().GetLayoutBoundingRect();
                    if (Rect.GetSize().X <= 1 || Rect.GetSize().Y <= 1) { continue; }
                    Rects.Add(Rect);
                    Test.TestTrue(*FString::Printf(TEXT("%s: every page card fits completely inside the list"), *State),
                        Rect.Left >= ListRect.Left - 1.5 && Rect.Right <= ListRect.Right + 1.5 &&
                        Rect.Top >= ListRect.Top - 1.5 && Rect.Bottom <= ListRect.Bottom + 1.5);
                }
            }
            Test.TestEqual(*FString::Printf(TEXT("%s: every page item is painted"), *State), Rects.Num(), Items.Num());
            Rects.Sort([](const FSlateRect& A, const FSlateRect& B) { return A.Left < B.Left; });
            if (Rects.Num() == 6)
            {
                Test.TestTrue(*FString::Printf(TEXT("%s: the six responsive cards occupy the available catalog width"), *State),
                    Rects.Last().Right - Rects[0].Left >= ListRect.GetSize().X * 0.94);
            }
            for (int32 Index = 1; Index < Rects.Num(); ++Index)
            {
                Test.TestTrue(*FString::Printf(TEXT("%s: six cards form one evenly sized nonoverlapping row"), *State),
                    Rects[Index - 1].Right <= Rects[Index].Left + 1.5 &&
                    FMath::IsNearlyEqual(Rects[0].Top, Rects[Index].Top, 1.5f) &&
                    FMath::IsNearlyEqual(Rects[0].GetSize().X, Rects[Index].GetSize().X, 1.5f));
            }
            return true;
        }

        bool CheckAllCategoriesAndFirstAid(FAutomationTestBase& Test, const bool bCapture)
        {
            auto* Majors = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_Categories")));
            auto* Minors = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_Subcategories")));
            if (!Test.TestNotNull(TEXT("Full real category list"), Majors) ||
                !Test.TestNotNull(TEXT("Full real minor choices"), Minors)) { return false; }
            Test.TestEqual(TEXT("All and all ten categories are present"), Majors->GetChildrenCount(), 11);
            Test.TestEqual(TEXT("All facilities does not create a cross-category strip of every minor token"), Minors->GetChildrenCount(), 1);
            CheckFilterLayout(Test, TEXT("All facilities"));
            CheckSixCardPage(Test, TEXT("All facilities / page 1"));
            Test.TestEqual(TEXT("Full source catalog exposes every six-item page"), Panel->GetCatalogPageCount(), FMath::DivideAndRoundUp(SourceEntries.Num(), 6));
            if (bCapture) { Capture(Test, TEXT("AllCategories")); }
            TSet<FName> PagedIds;
            for (int32 Page = 0; Page < Panel->GetCatalogPageCount(); ++Page)
            {
                Panel->SetCatalogPage(Page);
                Paint();
                CheckSixCardPage(Test, FString::Printf(TEXT("All facilities / page %d"), Page + 1));
                CheckFilterLayout(Test, FString::Printf(TEXT("All facilities / page %d"), Page + 1));
                for (UObject* Object : Panel->GetCatalogPageItems())
                {
                    const auto* Item = Cast<UFF_PrepBuildTileData>(Object);
                    if (!Test.TestNotNull(TEXT("A page item is a real facility"), Item)) { return false; }
                    Test.TestFalse(TEXT("Facility IDs are never duplicated across catalog pages"), PagedIds.Contains(Item->GetFacilityDefinitionId()));
                    PagedIds.Add(Item->GetFacilityDefinitionId());
                }
            }
            Test.TestEqual(TEXT("Page navigation reaches the complete source catalog"), PagedIds.Num(), SourceEntries.Num());
            for (int32 Index = 1; Index < Majors->GetChildrenCount(); ++Index)
            {
                auto* Choice = Cast<UFF_PrepChoiceButton>(Majors->GetChildAt(Index));
                if (!Test.TestNotNull(TEXT("Real category action"), Choice)) { return false; }
                Choice->OnHudTabButtonClicked.ExecuteIfBound(Choice->GetChoiceId());
                Paint();
                const auto Category = static_cast<EFFZSFacilityCategory>(Choice->GetChoiceId());
                TSet<FName> ExpectedMinors;
                for (const auto& Entry : SourceEntries) { if (Entry.Category == Category) { ExpectedMinors.Add(Entry.MinorToken); } }
                Test.TestEqual(TEXT("Selecting a category resets to its first page"), Panel->GetCatalogPageIndex(), 0);
                Test.TestEqual(TEXT("Every real subcategory of the selected category is present"), Minors->GetChildrenCount(), ExpectedMinors.Num() + 1);
                CheckFilterLayout(Test, FString::Printf(TEXT("Major category %d"), Choice->GetChoiceId()));
            }
            UFF_PrepChoiceButton* Safety = nullptr;
            for (UWidget* Child : Majors->GetAllChildren())
            {
                auto* Choice = Cast<UFF_PrepChoiceButton>(Child);
                if (Choice && Choice->GetChoiceId() == static_cast<int32>(EFFZSFacilityCategory::Safety)) { Safety = Choice; }
            }
            if (!Test.TestNotNull(TEXT("Safety category exists in the complete source fixture"), Safety)) { return false; }
            Safety->OnHudTabButtonClicked.ExecuteIfBound(Safety->GetChoiceId());
            Paint();
            UFF_PrepChoiceButton* FirstAid = nullptr;
            const FString FirstAidLabel = FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(TEXT("FirstAid")).ToString();
            for (UWidget* Child : Minors->GetAllChildren())
            {
                auto* Choice = Cast<UFF_PrepChoiceButton>(Child);
                const auto* Label = Choice ? Cast<UTextBlock>(Choice->GetWidgetFromName(TEXT("Text_Label"))) : nullptr;
                if (Label && Label->GetText().ToString() == FirstAidLabel) { FirstAid = Choice; }
            }
            if (!Test.TestNotNull(TEXT("The real First Aid subcategory is selectable"), FirstAid)) { return false; }
            FirstAid->OnHudTabButtonClicked.ExecuteIfBound(FirstAid->GetChoiceId());
            Paint();
            CheckFilterLayout(Test, TEXT("Safety / First Aid selected"));
            CheckSixCardPage(Test, TEXT("Safety / First Aid selected"));
            for (UObject* Object : Panel->GetCatalogPageItems())
            {
                const auto* Item = Cast<UFF_PrepBuildTileData>(Object);
                Test.TestTrue(TEXT("First Aid action actually filters the real catalog"), Item &&
                    Item->GetEntry().Category == EFFZSFacilityCategory::Safety && Item->GetEntry().MinorToken == TEXT("FirstAid"));
            }
            if (bCapture) { Capture(Test, TEXT("FirstAid")); }
            auto* Favorites = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_FavoritesOnly")));
            if (!Test.TestTrue(TEXT("Favorite-only action is bound"), Favorites && Favorites->OnClicked.IsBound())) { return false; }
            // The filter changes only this panel. No favorites storage subsystem is attached.
            Favorites->OnClicked.Broadcast();
            Paint();
            CheckFilterLayout(Test, TEXT("First Aid / favorites only / no saved favorites"));
            Test.TestEqual(TEXT("Empty favorites has no hidden stale page items"), Panel->GetCatalogPageItems().Num(), 0);
            Favorites->OnClicked.Broadcast();
            Paint();
            CheckFilterLayout(Test, TEXT("First Aid / favorite filter restored"));
            CheckSixCardPage(Test, TEXT("First Aid / favorite filter restored"));
            return true;
        }

        void CheckRows(FAutomationTestBase& Test, FMeasurements& Measurements, const FString& Step,
            const bool bRequireWholeCard = false)
        {
            const TArray<UUserWidget*>& Rows = List->GetDisplayedEntryWidgets();
            Test.TestTrue(*FString::Printf(TEXT("%s: viewport realizes some rows"), *Step), Rows.Num() > 0);
            Test.TestTrue(*FString::Printf(TEXT("%s: off-screen rows remain virtualized"), *Step), Rows.Num() < ItemCount / 2);
            Measurements.MaxRealizedRows = FMath::Max(Measurements.MaxRealizedRows, Rows.Num());
            const FSlateRect ListRect = List->GetCachedGeometry().GetLayoutBoundingRect();
            int32 VisibleCount = 0;
            int32 FullyVisibleCount = 0;
            for (UUserWidget* Row : Rows)
            {
                UFF_PrepBuildTileData* Item = Cast<UFF_PrepBuildTileData>(List->GetListObjectFromEntry(*Row));
                UTextBlock* Name = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Name")));
                UImage* Thumbnail = Cast<UImage>(Row->GetWidgetFromName(TEXT("Image_Thumbnail")));
                USizeBox* ThumbnailFrame = Cast<USizeBox>(Row->GetWidgetFromName(TEXT("SizeBox_Thumbnail")));
                UBorder* Card = Cast<UBorder>(Row->GetWidgetFromName(TEXT("Border_Card")));
                if (!Test.TestNotNull(TEXT("Realized row has its current list item"), Item) ||
                    !Test.TestNotNull(TEXT("Realized row name"), Name) ||
                    !Test.TestNotNull(TEXT("Realized row thumbnail"), Thumbnail) ||
                    !Test.TestNotNull(TEXT("Realized row fixed thumbnail frame"), ThumbnailFrame) ||
                    !Test.TestNotNull(TEXT("Realized row card"), Card))
                {
                    continue;
                }
                if (UObject* const* PreviousItem = Measurements.LastItemByRow.Find(Row))
                {
                    Measurements.ReboundRows += *PreviousItem != Item ? 1 : 0;
                }
                Measurements.LastItemByRow.Add(Row, Item);
                Measurements.SeenItems.Add(Item);
                const FString Label = FString::Printf(TEXT("%s / %s"), *Step, *Item->GetFacilityDefinitionId().ToString());
                Test.TestFalse(*FString::Printf(TEXT("%s: recycled name never becomes blank"), *Label), Name->GetText().IsEmpty());
                Test.TestEqual(*FString::Printf(TEXT("%s: name belongs to the new item"), *Label),
                    Name->GetText().ToString(), Item->GetDisplayName().ToString());
                Test.TestEqual(*FString::Printf(TEXT("%s: thumbnail belongs to the new item"), *Label),
                    Thumbnail->GetBrush().GetResourceObject(), static_cast<UObject*>(Item->GetThumbnail().Get()));
                Test.TestTrue(*FString::Printf(TEXT("%s: recycled thumbnail stays visible"), *Label),
                    Thumbnail->GetVisibility() != ESlateVisibility::Collapsed &&
                    Thumbnail->GetVisibility() != ESlateVisibility::Hidden && Thumbnail->GetColorAndOpacity().A > 0.0f);
                const UTexture2D* Texture = Item->GetThumbnail().Get();
                const FVector2D ImageDesiredSize = Thumbnail->GetDesiredSize();
                const double AuthoredThumbnailHeight = ThumbnailFrame->IsHeightOverride()
                    ? ThumbnailFrame->GetHeightOverride() : List->GetEntryHeight();
                Test.TestTrue(*FString::Printf(TEXT("%s: even clipped rows keep the current image's aspect and bounded desired size"), *Label),
                    Texture && ImageDesiredSize.X > 0.0 && ImageDesiredSize.Y > 0.0 &&
                    ImageDesiredSize.X <= List->GetEntryWidth() + 1.0 &&
                    ImageDesiredSize.Y <= AuthoredThumbnailHeight + 1.0 &&
                    FMath::IsNearlyEqual(ImageDesiredSize.X / ImageDesiredSize.Y,
                        static_cast<double>(Texture->GetSizeX()) / Texture->GetSizeY(), 0.01));

                const FSlateRect CardRect = Card->GetCachedGeometry().GetLayoutBoundingRect();
                if (CardRect.Right <= ListRect.Left || CardRect.Left >= ListRect.Right ||
                    CardRect.Bottom <= ListRect.Top || CardRect.Top >= ListRect.Bottom)
                {
                    continue; // The list may prefetch an additional clipped row.
                }
                ++VisibleCount;
                const bool bFullyVisible = CardRect.Top >= ListRect.Top - 1.0 &&
                    CardRect.Bottom <= ListRect.Bottom + 1.0;
                FullyVisibleCount += bFullyVisible ? 1 : 0;
                // Slate culls descendants outside the list clip. A partially visible card
                // may therefore carry an earlier paint geometry for its clipped image/name.
                // Data and desired-size checks above still cover every pooled row; inspect
                // painted child geometry only where the entire card was actually painted.
                if (!bFullyVisible) { continue; }
                const FVector2D CardSize = Card->GetCachedGeometry().GetLocalSize();
                const FVector2D ThumbnailSize = ThumbnailFrame->GetCachedGeometry().GetLocalSize();
                if (Measurements.CardSize.IsNearlyZero())
                {
                    Measurements.CardSize = CardSize;
                    Measurements.ThumbnailSize = ThumbnailSize;
                }
                const auto* CardFrame = Cast<USizeBox>(Row->GetWidgetFromName(TEXT("SizeBox_Card")));
                Test.TestTrue(*FString::Printf(TEXT("%s: card respects its authored height and allocated column width"), *Label),
                    CardFrame && CardSize.X > 0.0 && CardSize.X <= List->GetEntryWidth() + 1.0 &&
                    (!CardFrame->IsWidthOverride() || FMath::IsNearlyEqual(CardSize.X, static_cast<double>(CardFrame->GetWidthOverride()), 1.0)) &&
                    (!CardFrame->IsHeightOverride() || FMath::IsNearlyEqual(CardSize.Y, static_cast<double>(CardFrame->GetHeightOverride()), 1.0)));
                Test.TestTrue(*FString::Printf(TEXT("%s: card dimensions survive reuse"), *Label),
                    CardSize.Equals(Measurements.CardSize, 1.0));
                Test.TestTrue(*FString::Printf(TEXT("%s: thumbnail frame survives mixed native resolutions"), *Label),
                    ThumbnailSize.Equals(Measurements.ThumbnailSize, 1.0) &&
                    (!ThumbnailFrame->IsWidthOverride() || FMath::IsNearlyEqual(ThumbnailSize.X, static_cast<double>(ThumbnailFrame->GetWidthOverride()), 1.0)) &&
                    (!ThumbnailFrame->IsHeightOverride() || FMath::IsNearlyEqual(ThumbnailSize.Y, static_cast<double>(ThumbnailFrame->GetHeightOverride()), 1.0)));
                const FSlateRect ImageRect = Thumbnail->GetCachedGeometry().GetLayoutBoundingRect();
                const FSlateRect FrameRect = ThumbnailFrame->GetCachedGeometry().GetLayoutBoundingRect();
                const double ImageWidth = ImageRect.Right - ImageRect.Left;
                const double ImageHeight = ImageRect.Bottom - ImageRect.Top;
                Test.TestTrue(*FString::Printf(TEXT("%s: image stays inside its stable frame"), *Label),
                    ImageWidth > 1.0 && ImageHeight > 1.0 && ImageRect.Left >= FrameRect.Left - 1.0 &&
                    ImageRect.Right <= FrameRect.Right + 1.0 && ImageRect.Top >= FrameRect.Top - 1.0 &&
                    ImageRect.Bottom <= FrameRect.Bottom + 1.0);
                Test.TestTrue(*FString::Printf(TEXT("%s: image preserves source aspect ratio"), *Label),
                    Texture && ImageHeight > 0.0 && FMath::IsNearlyEqual(ImageWidth / ImageHeight,
                        static_cast<double>(Texture->GetSizeX()) / Texture->GetSizeY(), 0.01));
                const FSlateRect NameRect = Name->GetCachedGeometry().GetLayoutBoundingRect();
                Test.TestTrue(*FString::Printf(TEXT("%s: name keeps visible space inside its card"), *Label),
                    NameRect.Right > NameRect.Left && NameRect.Bottom - NameRect.Top >= Name->GetFont().Size * DpiScale * 0.7 &&
                    NameRect.Top >= CardRect.Top - 1.0 && NameRect.Bottom <= CardRect.Bottom + 1.0);
            }
            Test.TestTrue(*FString::Printf(TEXT("%s: at least one card is inside the visible list"), *Step), VisibleCount > 0);
            if (bRequireWholeCard)
            {
                Test.TestTrue(*FString::Printf(TEXT("%s: the dock leaves room for a complete card and its name"), *Step), FullyVisibleCount > 0);
            }
        }

        bool Capture(FAutomationTestBase& Test, const FString& Suffix)
        {
            // One encoding step: Slate outputs linear colors, the explicit sRGB target
            // converts them. Enabling both shader gamma and sRGB would wash out the palette.
            FWidgetRenderer Renderer(false, true);
            TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(ViewportSize, TF_Bilinear, true));
            if (!Test.TestNotNull(TEXT("Catalog screenshot target"), Target.Get()))
            {
                return false;
            }
            Renderer.DrawWindow(Target.Get(), HitGrid, Window.ToSharedRef(), 1.0f, ViewportSize, 1.0f / 60.0f);
            TArray<FColor> Pixels;
            const int32 Width = FMath::RoundToInt(ViewportSize.X);
            const int32 Height = FMath::RoundToInt(ViewportSize.Y);
            FReadSurfaceDataFlags ReadFlags;
            ReadFlags.SetLinearToGamma(false); // The sRGB target already encoded the UI palette.
            if (!Test.TestTrue(TEXT("Actual catalog Slate pixels can be read"),
                Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == Width * Height))
            {
                return false;
            }
            int32 VisiblePixels = 0;
            for (const FColor& Pixel : Pixels)
            {
                VisiblePixels += Pixel.A > 0 && (Pixel.R > 25 || Pixel.G > 25 || Pixel.B > 25) ? 1 : 0;
            }
            Test.TestTrue(TEXT("Catalog image contains visible cards and labels"), VisiblePixels > Width * 100);
            if (const auto* Mark = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("Block_BuildMark00"))))
            {
                const FSlateRect MarkRect = Mark->GetCachedGeometry().GetLayoutBoundingRect();
                const int32 SampleX = FMath::Clamp(FMath::RoundToInt((MarkRect.Left + MarkRect.Right) * 0.5f), 0, Width - 1);
                const int32 SampleY = FMath::Clamp(FMath::RoundToInt((MarkRect.Top + MarkRect.Bottom) * 0.5f), 0, Height - 1);
                const FColor Actual = Pixels[SampleY * Width + SampleX];
                const FColor Expected = (Mark->Background.TintColor.GetSpecifiedColor() * Mark->GetBrushColor()).ToFColor(true);
                Test.TestTrue(TEXT("PNG preserves the production teal without a second gamma conversion"),
                    FMath::Abs(static_cast<int32>(Actual.R) - Expected.R) <= 3 &&
                    FMath::Abs(static_cast<int32>(Actual.G) - Expected.G) <= 3 &&
                    FMath::Abs(static_cast<int32>(Actual.B) - Expected.B) <= 3);
            }
            const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BuildableGrid"));
            IFileManager::Get().MakeDirectory(*Directory, true);
            const FString Path = FPaths::Combine(Directory,
                FString::Printf(TEXT("BuildCatalog_%dx%d_%s.png"), Width, Height, *Suffix));
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
            const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
            Test.TestTrue(TEXT("Catalog PNG exists"), bSaved && IFileManager::Get().FileSize(*Path) > 0);
            Test.AddInfo(FString::Printf(TEXT("Actual catalog with real facility data (7500 Gold fixture): %s"), *FPaths::ConvertRelativePathToFull(Path)));
            return bSaved;
        }

        void ShowFirstPage() { Panel->SetCatalogPage(0); }
        void ShowLastPage() { Panel->SetCatalogPage(Panel->GetCatalogPageCount() - 1); }

        UFF_PrepBuildTileView* List = nullptr;

    private:
        void SetVisibility(const FName Name, const ESlateVisibility Visibility)
        {
            if (UWidget* Widget = Panel->GetWidgetFromName(Name)) { Widget->SetVisibility(Visibility); }
        }
        void SetText(const FName Name, const TCHAR* Text)
        {
            if (UTextBlock* Label = Cast<UTextBlock>(Panel->GetWidgetFromName(Name))) { Label->SetText(FText::FromString(Text)); }
        }
        TUniquePtr<FPlayerlessInitialization> TileInitialization;
        TStrongObjectPtr<UFF_PrepPanelBuild> Panel;
        TStrongObjectPtr<UFF_PrepHudBar> Hud;
        TStrongObjectPtr<UGameInstance> DetachedGame;
        TStrongObjectPtr<UFF_BuildCatalogSubsystem> Catalog;
        TStrongObjectPtr<UFF_BuildCatalogEconomyReadModel> Economy;
        TArray<FFF_BuildCatalogEntry> SourceEntries;
        FName RequiredPreviewFacilityId = NAME_None;
        TWeakObjectPtr<UFF_PrepBuildTileData> RequiredPreviewTile;
#if !UE_BUILD_SHIPPING
        TWeakObjectPtr<UFF_PrepHudBar> PreviousActiveBar;
#endif
        TArray<TStrongObjectPtr<UTexture2D>> Textures;
        TArray<TStrongObjectPtr<UFFZSFacilityDefinition>> Definitions;
        FHittestGrid HitGrid;
        TSharedPtr<SVirtualWindow> Window;
        FVector2D ViewportSize = FVector2D::ZeroVector;
        float DpiScale = 1.0f;
        int32 FrameNumber = 0;
    };

    bool RunCatalogFixture(FAutomationTestBase& Test, const bool bCapture)
    {
        if (!Test.TestTrue(TEXT("Slate application is initialized"), FSlateApplication::IsInitialized())) { return false; }
        TArray<FIntPoint> Resolutions = { FIntPoint(1280, 720), FIntPoint(1920, 1080) };
        if (bCapture)
        {
            Resolutions.Add(FIntPoint(1742, 738));
            // The isolated HUD intentionally has no gameplay ViewModel. Its expected
            // diagnostic is not a rendering failure; real catalog/economy snapshots follow.
            Test.AddExpectedError(TEXT("ViewModel 을 찾지 못했다"), EAutomationExpectedErrorFlags::Contains, Resolutions.Num());
        }
        for (const FIntPoint Resolution : Resolutions)
        {
            FCatalogFixture Fixture;
            if (!Fixture.Initialize(Test, FVector2D(Resolution.X, Resolution.Y), bCapture)) { return false; }
            FMeasurements Measurements;
            if (bCapture)
            {
                if (!Fixture.UseSourceCatalogEntries(Test)) { return false; }
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Source catalog top"), true);
                Fixture.Capture(Test, TEXT("Top"));
                Fixture.ShowLastPage();
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Source catalog bottom"), true);
                Fixture.Capture(Test, TEXT("Bottom"));
                Fixture.ShowFirstPage();
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Source catalog returned"), true);
                Fixture.Capture(Test, TEXT("Returned"));
                if (!Fixture.CheckAllCategoriesAndFirstAid(Test, true)) { return false; }
                Fixture.List->ClearListItems();
                continue;
            }
            Fixture.Paint();
            Fixture.CheckRows(Test, Measurements, TEXT("Initial top"), true);
            if (!Fixture.CheckFavoriteInputIsolation(Test)) { return false; }
            for (int32 Round = 0; Round < 3; ++Round)
            {
                for (const float Offset : { 24.0f, 72.0f, 112.0f })
                {
                    Fixture.List->SetScrollOffset(Offset);
                    Fixture.Paint();
                    Fixture.CheckRows(Test, Measurements, FString::Printf(TEXT("Round %d offset %.0f"), Round, Offset));
                }
                Fixture.List->ScrollToBottom();
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Bottom"), true);
                Test.TestTrue(TEXT("Scrolling reaches the final catalog item"),
                    Measurements.SeenItems.Contains(Fixture.List->GetListItems().Last()));
                Fixture.List->RegenerateAllEntries();
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Regenerated while scrolled"), true);
                Fixture.List->ScrollToTop();
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Returned to top"), true);
            }
            Test.TestTrue(TEXT("Different item objects actually reuse the same pooled widgets"), Measurements.ReboundRows > 0);
            Test.TestTrue(TEXT("The scenario inspects more data than one visible screen"),
                Measurements.SeenItems.Num() > Measurements.MaxRealizedRows * 2);
            Test.TestEqual(TEXT("Scrolling never drops data items"), Fixture.List->GetListItems().Num(), ItemCount);
            Test.AddInfo(FString::Printf(TEXT("%dx%d: %d distinct items, max %d realized rows, %d recycled row bindings; card %.0fx%.0f, image frame %.0fx%.0f"),
                Resolution.X, Resolution.Y, Measurements.SeenItems.Num(), Measurements.MaxRealizedRows, Measurements.ReboundRows,
                Measurements.CardSize.X, Measurements.CardSize.Y, Measurements.ThumbnailSize.X, Measurements.ThumbnailSize.Y));
            const TArray<UObject*> PreservedItems = Fixture.List->GetListItems();
            Fixture.List->ClearListItems();
            Fixture.Paint();
            Test.TestEqual(TEXT("Empty catalog releases every realized row"), Fixture.List->GetDisplayedEntryWidgets().Num(), 0);
            Fixture.List->SetListItems(PreservedItems);
            Fixture.Paint();
            Fixture.CheckRows(Test, Measurements, TEXT("Refilled after empty results"), true);
            Fixture.List->ClearListItems();
        }
        return true;
    }

    bool RunSynergyCatalogFixture(FAutomationTestBase& Test)
    {
        if (!Test.TestTrue(TEXT("Slate application is initialized"), FSlateApplication::IsInitialized())) { return false; }
        Test.AddExpectedError(TEXT("ViewModel 을 찾지 못했다"), EAutomationExpectedErrorFlags::Contains, 1);
        FCatalogFixture Fixture;
        if (!Fixture.Initialize(Test, FVector2D(1280, 720), true) ||
            !Fixture.UseSourceCatalogEntries(Test) || !Fixture.UseSynergyCatalogPreview(Test)) { return false; }
        Fixture.Paint();
        FMeasurements Measurements;
        Fixture.CheckRows(Test, Measurements, TEXT("Source catalog synergy recommendation"), true);
        if (!Fixture.CheckSixCardPage(Test, TEXT("Source catalog synergy recommendation")) ||
            !Fixture.CheckSynergyCatalogPreview(Test) || !Fixture.CheckFavoriteInputIsolation(Test)) { return false; }
        const bool bCaptured = Fixture.Capture(Test, TEXT("Synergy"));
        Fixture.List->ClearListItems();
        return bCaptured;
    }

    bool RunFacilityNavigationFixture(FAutomationTestBase& Test)
    {
        Test.AddExpectedError(TEXT("ViewModel 을 찾지 못했다"), EAutomationExpectedErrorFlags::Contains, 1);
        FCatalogFixture Fixture;
        if (!Fixture.Initialize(Test, FVector2D(1280, 720), true) ||
            !Fixture.UseSourceCatalogEntries(Test) || !Fixture.CheckFacilityNavigation(Test)) { return false; }
        return Fixture.Capture(Test, TEXT("PartnerNavigation"));
    }

    bool RunRequiredCatalogFixture(FAutomationTestBase& Test)
    {
        if (!Test.TestTrue(TEXT("Slate application is initialized"), FSlateApplication::IsInitialized())) { return false; }
        Test.AddExpectedError(TEXT("ViewModel 을 찾지 못했다"), EAutomationExpectedErrorFlags::Contains, 1);
        FCatalogFixture Fixture;
        if (!Fixture.Initialize(Test, FVector2D(1280, 720), true) || !Fixture.UseSourceCatalogEntries(Test)) { return false; }
        FMeasurements Measurements;
        for (const bool bInstalled : { false, true })
        {
            if (!Fixture.UseRequiredCatalogPreview(Test, bInstalled)) { return false; }
            Fixture.Paint();
            Fixture.CheckRows(Test, Measurements, bInstalled ? TEXT("Required stage installed") : TEXT("Required stage missing"), true);
            if (!Fixture.CheckRequiredCatalogPreview(Test, bInstalled) ||
                !Fixture.Capture(Test, bInstalled ? TEXT("RequiredSatisfied") : TEXT("RequiredMissing"))) { return false; }
        }
        Fixture.List->ClearListItems();
        return true;
    }

    bool RunFullCatalogLayoutFixture(FAutomationTestBase& Test)
    {
        if (!Test.TestTrue(TEXT("Slate application is initialized"), FSlateApplication::IsInitialized())) { return false; }
        Test.AddExpectedError(TEXT("ViewModel 을 찾지 못했다"), EAutomationExpectedErrorFlags::Contains, 3);
        for (const FIntPoint Resolution : { FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(1742, 738) })
        {
            FCatalogFixture Fixture;
            if (!Fixture.Initialize(Test, FVector2D(Resolution.X, Resolution.Y), true) ||
                !Fixture.UseSourceCatalogEntries(Test)) { return false; }
            Fixture.Paint();
            if (!Fixture.CheckAllCategoriesAndFirstAid(Test, false)) { return false; }
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildFullCatalogHudLayoutTest,
    "FestaFiesta.UI.PrepBuild.FullCatalogHudLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildFullCatalogHudLayoutTest::RunTest(const FString& Parameters)
{
    return FFPrepBuildVirtualizedSlateTest::RunFullCatalogLayoutFixture(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildVirtualizedScrollLifecycleTest,
    "FestaFiesta.UI.PrepBuild.VirtualizedScrollLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildVirtualizedScrollLifecycleTest::RunTest(const FString& Parameters)
{
    return FFPrepBuildVirtualizedSlateTest::RunCatalogFixture(*this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildCatalogRenderedPreviewTest,
    "FestaFiesta.UI.PrepBuild.RenderedPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildCatalogRenderedPreviewTest::RunTest(const FString& Parameters)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddInfo(TEXT("SKIPPED: PNG capture requires a real RHI; use -RenderOffscreen. VirtualizedScrollLifecycle runs with NullRHI."));
        return true;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("FFSynergyNavigationPreview")))
    {
        return FFPrepBuildVirtualizedSlateTest::RunFacilityNavigationFixture(*this);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("FFSynergyCatalogPreview")))
    {
        return FFPrepBuildVirtualizedSlateTest::RunSynergyCatalogFixture(*this);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("FFRequiredCatalogPreview")))
    {
        return FFPrepBuildVirtualizedSlateTest::RunRequiredCatalogFixture(*this);
    }
    return FFPrepBuildVirtualizedSlateTest::RunCatalogFixture(*this, true);
}

#endif
