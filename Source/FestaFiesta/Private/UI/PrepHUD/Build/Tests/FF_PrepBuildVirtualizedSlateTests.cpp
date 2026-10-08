// FF_PrepBuildVirtualizedSlateTests.cpp
// 역할: 준비 단계 건설 화면의 동작과 저장된 화면 연결을 자동 검증한다.

// Uses the saved catalog and tile WBP classes, including their real pooled Slate rows.
// Synthetic data is confined to this fixture; no catalog, economy or building command runs.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
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
#include "FF_FFZSFacilityDefinition.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/SObjectTableRow.h"
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
        FFBuildUIAssetPaths::BuildPanelClass;
    constexpr const TCHAR* TilePath =
        FFBuildUIAssetPaths::PrepBuildTileClass;
    constexpr const TCHAR* HudPath =
        FFBuildUIAssetPaths::PrepHudBarClass;

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
        UFF_PrepPanelBuild* GetPanel() const { return Panel.Get(); }
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
                Entry.CompatibleProgramCount = Index % 3 == 0 ? 1 : 0;
                Entry.CompatibleProgramNames = Entry.CompatibleProgramCount > 0
                    ? FText::FromString(TEXT("가상화 검증 프로그램")) : FText::GetEmpty();
                Entry.bRequiredForSetup = Index % 4 < 2;
                Entry.RequiredMinimumCount = Entry.bRequiredForSetup ? 1 : 0;
                Entry.RequiredInstalledCount = Entry.bRequiredForSetup ? FMath::Min(Entry.CurrentInstanceCount, 1) : 0;
                Entry.bRequiredSetupSatisfied = Entry.bRequiredForSetup && Entry.RequiredInstalledCount >= 1;
                UFF_PrepBuildTileData* Item = NewObject<UFF_PrepBuildTileData>(Panel.Get());
                Item->Initialize(Entry, 7500);
                Item->SetSynergyHighlighted(Index % 5 == 0);
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
                if (Rect.Left >= ListRect.Left && Rect.Right <= ListRect.Right &&
                    Rect.Top >= ListRect.Top && Rect.Bottom <= ListRect.Bottom && Rect.GetSize().X > 10)
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
                Path.IsValid() && Path.ContainsWidget(Favorite->GetCachedWidget().Get())))
            {
                Test.AddInfo(FString::Printf(TEXT("Favorite point %.1f,%.1f, hit path %s, visibility %d"),
                    Point.X, Point.Y, *Path.ToString(), int32(Favorite->GetVisibility())));
                return false;
            }
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

        bool CheckCatalogDragInput(FAutomationTestBase& Test)
        {
            Panel->SetCatalogPage(0);
            Paint();
            auto* Ranking = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_RevenueRanking")));
            auto* RankingLabel = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_RevenueRanking")));
            auto* Launcher = Cast<UUserWidget>(Panel->GetWidgetFromName(TEXT("Widget_FacilityCodexCatalogLauncher")));
            auto* Codex = Launcher ? Cast<UButton>(Launcher->GetWidgetFromName(TEXT("Button_OpenCodex"))) : nullptr;
            auto* CodexLabel = Codex ? Cast<UTextBlock>(Codex->GetContent()) : nullptr;
            if (!Test.TestNotNull(TEXT("Catalog ranking action"), Ranking) || !Test.TestNotNull(TEXT("Catalog ranking label"), RankingLabel) ||
                !Test.TestNotNull(TEXT("Nested codex action"), Codex) || !Test.TestNotNull(TEXT("Nested codex label"), CodexLabel)) return false;
            Test.TestTrue(TEXT("The pinned combination codex tab is large enough to click"),
                Codex->GetCachedGeometry().GetLocalSize().X >= 100.f &&
                Codex->GetCachedGeometry().GetLocalSize().Y >= 25.f);
            Test.TestTrue(TEXT("The codex tab is distinct from the header actions"),
                Launcher->GetParent() == Panel->GetWidgetFromName(TEXT("Row_BuildCodexCategory")) &&
                CodexLabel->GetText().ToString().Contains(TEXT("조합 도감")) &&
                RankingLabel->GetFont() == CodexLabel->GetFont());
            Test.TestNull(TEXT("The redundant selection confirmation strip is deleted from the saved catalog"),
                Panel->GetWidgetFromName(TEXT("Border_AuroraSelection")));
            Test.TestEqual(TEXT("Continuous catalog has one logical page"), Panel->GetCatalogPageCount(), 1);
            Test.TestEqual(TEXT("The complete filtered catalog remains in the virtualized list"),
                List->GetListItems().Num(), Panel->GetCatalogPageItems().Num());
            const UWidget* Dock = Panel->GetWidgetFromName(TEXT("Border_PanelRoot"));
            if (!Test.TestNotNull(TEXT("Compact catalog has its authored dock"), Dock)) return false;
            const FSlateRect DockRect = Dock->GetCachedGeometry().GetLayoutBoundingRect();
            Test.TestTrue(TEXT("The shallow full-width ribbon preserves the world view above it"),
                DockRect.Top >= ViewportSize.Y * 0.5 && DockRect.Left >= 0 && DockRect.Right <= ViewportSize.X + 1 &&
                DockRect.GetSize().X >= ViewportSize.X * 0.8);
            const FGeometry& ListGeometry = List->GetCachedGeometry();
            auto* Card = Cast<UFF_PrepBuildTileWidget>(List->GetDisplayedEntryWidgets().IsEmpty()
                ? nullptr : List->GetDisplayedEntryWidgets()[0]);
            if (!Test.TestNotNull(TEXT("Gesture begins on an actual card, not an empty card gap"), Card)) return false;
            const FVector2D Point = Card->GetCachedGeometry().LocalToAbsolute(FVector2D(90, 65));
            TArray<FWidgetAndPointer> Bubble = HitGrid.GetBubblePath(Point, 0.0f, false);
            FWidgetPath Path(MakeArrayView(Bubble));
            if (!Test.TestTrue(TEXT("The visible catalog drag surface is hit-testable"),
                Path.IsValid() && Path.ContainsWidget(Panel->GetCachedWidget().Get()))) return false;
            FSlateApplication& App = FSlateApplication::Get();
            const TSharedPtr<SWidget> PreviousFocus = App.GetUserFocusedWidget(0);
            const TSet<FKey> Pressed = { EKeys::LeftMouseButton };
            const TSet<FKey> Released;
            const auto Gesture = [&](const FVector2D LocalDelta)
            {
                const FVector2D End = ListGeometry.LocalToAbsolute(
                    ListGeometry.AbsoluteToLocal(Point) + LocalDelta);
                const FPointerEvent Down(0, Point, Point, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                const FPointerEvent Move(0, End, Point, Pressed, EKeys::Invalid, 0.f, FModifierKeysState());
                const FPointerEvent Up(0, End, End, Released, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                Test.TestTrue(TEXT("Catalog captures drag on pointer-down"), App.RoutePointerDownEvent(Path, Down).IsEventHandled());
                Test.TestTrue(TEXT("Press alone does not select a card"), List->GetSelectedItem() == nullptr);
                App.RoutePointerMoveEvent(Path, Move, false);
                App.RoutePointerUpEvent(Path, Up);
            };
            const TArray<UObject*> CatalogItems = Panel->GetCatalogPageItems();
            Gesture(FVector2D(-90, 0));
            const float OffsetAfterLeftDrag = List->GetScrollOffset();
            Test.TestTrue(TEXT("Left drag advances the continuous ribbon immediately"), OffsetAfterLeftDrag > 0.f);
            Test.TestTrue(TEXT("Drag leaves no accidentally selected candidate"), Panel->GetSelectedCatalogFacilityId().IsNone());
            Paint();
            Bubble = HitGrid.GetBubblePath(Point, 0.0f, false);
            Path = FWidgetPath(MakeArrayView(Bubble));
            const float OffsetBeforeRightDrag = List->GetScrollOffset();
            Gesture(FVector2D(90, 0));
            Test.TestTrue(TEXT("Right drag moves the ribbon toward its beginning"),
                List->GetScrollOffset() < OffsetBeforeRightDrag);
            Paint();
            Bubble = HitGrid.GetBubblePath(Point, 0.0f, false);
            Path = FWidgetPath(MakeArrayView(Bubble));
            const float OffsetBeforeShortDrag = List->GetScrollOffset();
            Gesture(FVector2D(-20, 0));
            Test.TestTrue(TEXT("A short drag moves continuously instead of turning a page"),
                List->GetScrollOffset() > OffsetBeforeShortDrag);
            Test.TestTrue(TEXT("Movement beyond click tolerance does not select a card"), List->GetSelectedItem() == nullptr);
            const float OffsetBeforeVerticalDrag = List->GetScrollOffset();
            Gesture(FVector2D(0, -20));
            Test.TestTrue(TEXT("Vertical movement does not scroll a horizontal ribbon"),
                FMath::IsNearlyEqual(List->GetScrollOffset(), OffsetBeforeVerticalDrag, .02f));
            Test.TestTrue(TEXT("Short vertical movement beyond click tolerance cancels selection"), List->GetSelectedItem() == nullptr);
            Test.TestTrue(TEXT("Scrolling does not replace or duplicate the catalog data"),
                Panel->GetCatalogPageItems() == CatalogItems && Panel->GetCatalogPageIndex() == 0);
            List->ScrollToTop();
            Paint();
            if (!CheckFavoriteInputIsolation(Test)) return false;
            Test.TestTrue(TEXT("Starring a card does not activate a placement candidate"), Panel->GetSelectedCatalogFacilityId().IsNone());
            // Positive control: the identical path without movement reaches card activation
            // on release. The detached mode rejects world preview creation safely.
            Bubble = HitGrid.GetBubblePath(Point, 0.f, false);
            Path = FWidgetPath(MakeArrayView(Bubble));
            Gesture(FVector2D::ZeroVector);
            Test.TestFalse(TEXT("A released click activates a candidate on the same tested surface"), Panel->GetSelectedCatalogFacilityId().IsNone());
            if (PreviousFocus) App.SetUserFocus(0, PreviousFocus, EFocusCause::SetDirectly);
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
                FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, Festival->ProgramCatalog);
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
            UFunction* AllHandler = Panel->FindFunction(TEXT("HandleFilterAllClicked"));
            UFunction* PerformanceHandler = Panel->FindFunction(TEXT("HandleFilterPerformanceClicked"));
            UFunction* ModeHandler = Panel->FindFunction(TEXT("HandleBuildModeStateChanged"));
            if (!Test.TestNotNull(TEXT("Actual catalog search"), Search) ||
                !Test.TestNotNull(TEXT("Actual favorites filter"), Favorites) ||
                !Test.TestNotNull(TEXT("Catalog loading state"), StateProperty) ||
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
            Test.TestTrue(TEXT("The codex destination has its own identity state"), Target->IsCodexFocused());
            Panel->ProcessEvent(PerformanceHandler, nullptr);
            const TArray<UObject*> ManualItems = Panel->GetCatalogPageItems();
            Test.TestTrue(TEXT("Later player category choice is not forced back to the consumed jump"),
                !ManualItems.IsEmpty() && !ManualItems.Contains(Target));
            Test.TestTrue(TEXT("Filtering the destination away retains its codex identity"), Target->IsCodexFocused());
            Panel->ProcessEvent(AllHandler, nullptr);
            Paint();
            Test.TestTrue(TEXT("Returning to all categories retains the destination accent"), Target->IsCodexFocused());
            for (UObject* Object : Panel->GetCatalogPageItems())
            {
                auto* Other = Cast<UFF_PrepBuildTileData>(Object);
                if (Other && Other != Target)
                {
                    List->SetSelectedItem(Other);
                    Test.TestFalse(TEXT("Choosing a different facility clears the old codex destination"), Target->IsCodexFocused());
                    break;
                }
            }

            Panel->RequestFacilityFocus(TEXT("Booth_FoodStall_Taco"));
            auto* LaterTile = Cast<UFF_PrepBuildTileData>(List->GetSelectedItem());
            Test.TestTrue(TEXT("A target anywhere in the continuous ribbon is selected and scrolled into view"),
                Panel->GetCatalogPageIndex() == 0 && LaterTile &&
                LaterTile->GetFacilityDefinitionId() == FName(TEXT("Booth_FoodStall_Taco")) &&
                Panel->GetCatalogPageItems().Contains(LaterTile));
            Panel->RequestFacilityFocus(PartnerId);
            Paint();
            Test.TestFalse(TEXT("A second codex jump clears the previous destination"), LaterTile && LaterTile->IsCodexFocused());
            Test.TestEqual(TEXT("Returning to another facility retains the continuous ribbon"), Panel->GetCatalogPageIndex(), 0);
            bool bFoundRow = false;
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                const auto* Item = Cast<UFF_PrepBuildTileData>(List->GetListObjectFromEntry(*Row));
                bFoundRow |= Item && Item->GetFacilityDefinitionId() == PartnerId;
            }
            Test.TestTrue(TEXT("Target card is actually generated by the saved WBP"), bFoundRow);
            if (!CheckCodexFocusPresentation(Test, PartnerId)) { return false; }
            Panel->RequestFacilityFocus(NAME_None);
            Test.TestFalse(TEXT("Ordinary catalog open cancels a prior codex accent"), Target->IsCodexFocused());
            Panel->RequestFacilityFocus(PartnerId);
            Panel->ProcessEvent(ModeHandler, &ModeParameters);
            Test.TestFalse(TEXT("Leaving the catalog clears the destination accent"), Target->IsCodexFocused());
            Panel->RequestFacilityFocus(TEXT("Missing_Codex_Destination"));
            Test.TestFalse(TEXT("An invalid destination cannot resurrect the previous accent"), Target->IsCodexFocused());
            Panel->RequestFacilityFocus(PartnerId);
            Paint();
            if (!CheckCodexFocusPresentation(Test, PartnerId)) { return false; }
            return CheckFavoriteInputIsolation(Test, PartnerId);
        }

        bool CheckCodexFocusPresentation(FAutomationTestBase& Test, const FName Destination)
        {
            int32 FocusedRows = 0;
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                auto* Tile = Cast<UFF_PrepBuildTileWidget>(Row);
                auto* Item = Cast<UFF_PrepBuildTileData>(List->GetListObjectFromEntry(*Row));
                auto* Rim = Cast<UImage>(Row->GetWidgetFromName(TEXT("Image_CodexFocusRim")));
                auto* Badge = Cast<UBorder>(Row->GetWidgetFromName(TEXT("Border_CodexFocusBadge")));
                auto* Label = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_CodexFocusBadge")));
                if (!Test.TestNotNull(TEXT("Saved destination rim"), Rim) ||
                    !Test.TestNotNull(TEXT("Saved destination badge"), Badge) ||
                    !Test.TestNotNull(TEXT("Saved destination caption"), Label) || !Tile || !Item) return false;
                const bool bDestination = Item->GetFacilityDefinitionId() == Destination;
                const auto Visibility = bDestination ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
                Test.TestTrue(TEXT("Only the requested facility shows the independent non-interactive accent"),
                    Tile->IsCodexFocused() == bDestination && Rim->GetVisibility() == Visibility && Badge->GetVisibility() == Visibility);
                Test.TestTrue(TEXT("The destination frame uses an actual UI material"),
                    Cast<UMaterialInterface>(Rim->GetBrush().GetResourceObject()) != nullptr);
                if (!bDestination) continue;
                ++FocusedRows;
                Test.TestEqual(TEXT("Codex destination has a readable origin caption"), Label->GetText().ToString(), FString(TEXT("도감에서 찾은 시설")));
                auto* Favorite = Cast<UButton>(Row->GetWidgetFromName(TEXT("Button_Favorite")));
                const FSlateRect BadgeBounds = Badge->GetCachedGeometry().GetLayoutBoundingRect();
                const FSlateRect CardBounds = Row->GetCachedGeometry().GetLayoutBoundingRect();
                Test.TestTrue(TEXT("Destination badge fits inside the existing card"),
                    BadgeBounds.Left >= CardBounds.Left && BadgeBounds.Top >= CardBounds.Top &&
                    BadgeBounds.Right <= CardBounds.Right + 1 && BadgeBounds.Bottom <= CardBounds.Bottom);
                if (Favorite)
                {
                    const FSlateRect FavoriteBounds = Favorite->GetCachedGeometry().GetLayoutBoundingRect();
                    Test.TestTrue(TEXT("Destination caption leaves the favorite action unobstructed"),
                        BadgeBounds.Right <= FavoriteBounds.Left || BadgeBounds.Bottom <= FavoriteBounds.Top);
                }
            }
            return Test.TestEqual(TEXT("Exactly one generated card marks the codex destination"), FocusedRows, 1);
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
            if (!Test.TestTrue(TEXT("The actual Seating partner sorts first in the continuous ribbon"),
                FirstItem && FirstItem->GetFacilityDefinitionId() == PartnerId && FirstItem->IsSynergyHighlighted() &&
                PageItems.Num() == Model->GetVisibleTiles().Num())) { return false; }
            // The detached fixture has no world selection subsystem; feed its complete
            // filtered ribbon directly while retaining the saved WBP, prices and thumbnails.
            List->SetListItems(PageItems);
            List->ScrollToTop();
            List->RegenerateAllEntries();
            Test.TestNull(TEXT("The retired hidden footer is removed; recommendation remains on the real card badge"),
                Panel->GetWidgetFromName(TEXT("Border_CatalogFooter")));
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
            // authoritative projection to its real model and header after normal filtering.
            Model->Reconcile(EFF_BuildCatalogLoadState::Ready, SourceEntries, Filter, 7500, true, FText::GetEmpty());
            Panel->RefreshRequiredFacilityGuide(Status);
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
            // The hidden old header was removed from the Designer tree. The live
            // required/program state remains represented by each actual card below.
            Test.TestNull(TEXT("Retired title/gold header is removed"), Panel->GetWidgetFromName(TEXT("Box_Header")));
            Test.TestNull(TEXT("Retired header legend is removed"), Panel->GetWidgetFromName(TEXT("Border_CatalogRequiredGuide")));
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
            Test.TestEqual(TEXT("Required state explains installation without a fraction"),
                State->GetText().ToString(), FString(bInstalled ? TEXT("설치 완료") : TEXT("1개 설치")));
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
                    FString(TEXT("필수 · 1개 설치")));
                for (const UWidget* Decoration : { static_cast<UWidget*>(Border), static_cast<UWidget*>(Badge) })
                {
                    const FSlateRect Rect = Decoration->GetCachedGeometry().GetLayoutBoundingRect();
                    Test.TestTrue(TEXT("Required badge fits inside the actual card at the current viewport"),
                        Rect.GetSize().X > 1.0f && Rect.GetSize().Y > 1.0f &&
                        Rect.Left >= CardRect.Left - 1.5f && Rect.Right <= CardRect.Right + 1.5f &&
                        Rect.Top >= CardRect.Top - 1.5f && Rect.Bottom <= CardRect.Bottom + 1.5f);
                }
            }
            return CheckFavoriteInputIsolation(Test, RequiredPreviewFacilityId);
        }

        bool UseProgramCatalogPreview(FAutomationTestBase& Test)
        {
            const FFF_BuildCatalogEntry* Source = SourceEntries.FindByPredicate([](const FFF_BuildCatalogEntry& Entry)
            {
                return Entry.Category == EFFZSFacilityCategory::Booth && Entry.CompatibleProgramCount > 0;
            });
            const auto* ModelProperty = FindFProperty<FObjectPropertyBase>(Panel->GetClass(), TEXT("PresentationModel"));
            auto* Model = ModelProperty ? Cast<UFF_PrepBuildPresentationModel>(
                ModelProperty->GetObjectPropertyValue_InContainer(Panel.Get())) : nullptr;
            UFunction* BoothHandler = Panel->FindFunction(TEXT("HandleFilterBoothClicked"));
            if (!Test.TestNotNull(TEXT("The actual festival exposes a program-compatible booth"), Source) ||
                !Test.TestNotNull(TEXT("Actual panel model for program preview"), Model) ||
                !Test.TestNotNull(TEXT("Production booth category action"), BoothHandler)) { return false; }
            ProgramPreviewFacilityId = Source->FacilityDefinitionId;
            Panel->ProcessEvent(BoothHandler, nullptr);
            FFF_PrepBuildFilter Filter;
            Filter.bFilterByCategory = true;
            Filter.Category = EFFZSFacilityCategory::Booth;
            // Retain the complete category; a valid program host may begin outside the
            // visible ribbon as the production catalog grows.
            Model->Reconcile(EFF_BuildCatalogLoadState::Ready, SourceEntries, Filter, 7500, true, FText::GetEmpty());
            const int32 TargetIndex = Model->GetVisibleTiles().IndexOfByPredicate([this](const TObjectPtr<UFF_PrepBuildTileData>& Tile)
            {
                return Tile && Tile->GetFacilityDefinitionId() == ProgramPreviewFacilityId;
            });
            if (!Test.TestTrue(TEXT("Actual program host remains in the filtered booth catalog"), TargetIndex != INDEX_NONE)) { return false; }
            const TArray<UObject*> PageItems = Panel->GetCatalogPageItems();
            if (!Test.TestTrue(TEXT("The full booth ribbon retains the program host"),
                PageItems.Contains(Model->FindTile(ProgramPreviewFacilityId)))) { return false; }
            List->SetListItems(PageItems);
            List->RequestNavigateToItem(Model->FindTile(ProgramPreviewFacilityId));
            List->RegenerateAllEntries();
            return true;
        }

        bool CheckProgramCatalogPreview(FAutomationTestBase& Test)
        {
            UFF_PrepBuildTileWidget* ProgramRow = nullptr;
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                const auto* Item = Cast<UFF_PrepBuildTileData>(List->GetListObjectFromEntry(*Row));
                if (Item && Item->GetFacilityDefinitionId() == ProgramPreviewFacilityId)
                {
                    ProgramRow = Cast<UFF_PrepBuildTileWidget>(Row);
                    Test.TestFalse(TEXT("Actual compatible program names reach the card data"), Item->GetEntry().CompatibleProgramNames.IsEmpty());
                    Test.TestTrue(TEXT("Actual compatible program names are explained in the card tooltip"),
                        ProgramRow && ProgramRow->GetToolTipText().ToString().Contains(Item->GetEntry().CompatibleProgramNames.ToString()));
                    break;
                }
            }
            if (!Test.TestNotNull(TEXT("A facility from the actual program catalog is painted"), ProgramRow)) { return false; }
            auto* Badge = Cast<UTextBlock>(ProgramRow->GetWidgetFromName(TEXT("Text_ProgramBadge")));
            if (!Test.TestNotNull(TEXT("Program badge caption is authored in the saved WBP"), Badge)) { return false; }
            Test.TestTrue(TEXT("Actual compatible program state is visible on the pooled card"), ProgramRow->HasCompatiblePrograms());
            Test.TestEqual(TEXT("Program badge retains its authored concise caption"), Badge->GetText().ToString(), FString(TEXT("프로그램")));
            return CheckFavoriteInputIsolation(Test, ProgramPreviewFacilityId);
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

        bool CheckCatalogRowPreview(FAutomationTestBase& Test, const FString& State)
        {
            const TArray<UObject*> Items = Panel->GetCatalogPageItems();
            Test.TestEqual(*FString::Printf(TEXT("%s: the ribbon retains every filtered facility"), *State),
                List->GetListItems().Num(), Items.Num());
            Test.TestEqual(*FString::Printf(TEXT("%s: the ribbon uses one logical page"), *State),
                Panel->GetCatalogPageCount(), Items.IsEmpty() ? 0 : 1);
            const FSlateRect ListRect = List->GetCachedGeometry().GetLayoutBoundingRect();
            TArray<FSlateRect> VisibleRects;
            int32 FullyVisibleCount = 0;
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                if (const UWidget* Card = Row->GetWidgetFromName(TEXT("Border_Card")))
                {
                    const FSlateRect Rect = Card->GetCachedGeometry().GetLayoutBoundingRect();
                    if (Rect.GetSize().X <= 1 || Rect.GetSize().Y <= 1) { continue; }
                    const int32 ItemIndex = Items.IndexOfByKey(List->GetListObjectFromEntry(*Row));
                    if (!Test.TestTrue(TEXT("Every realized card belongs to the current filtered catalog"), ItemIndex != INDEX_NONE)) return false;
                    if (Rect.Right <= ListRect.Left || Rect.Left >= ListRect.Right) continue;
                    VisibleRects.Add(Rect);
                    const bool bFullyVisible = Rect.Left >= ListRect.Left - 1.5 && Rect.Right <= ListRect.Right + 1.5;
                    FullyVisibleCount += bFullyVisible ? 1 : 0;
                    Test.TestTrue(*FString::Printf(TEXT("%s: the horizontal card row fits vertically in the ribbon"), *State),
                        Rect.Top >= ListRect.Top - 1.5 && Rect.Bottom <= ListRect.Bottom + 1.5);
                    if (!bFullyVisible)
                    {
                        const float OutsideX = Rect.Left < ListRect.Left
                            ? FMath::Max(Rect.Left + 1.f, ListRect.Left - 3.f * DpiScale)
                            : FMath::Min(Rect.Right - 1.f, ListRect.Right + 3.f * DpiScale);
                        const FVector2D ClippedPoint(OutsideX, (Rect.Top + Rect.Bottom) * .5f);
                        TArray<FWidgetAndPointer> Bubble = HitGrid.GetBubblePath(ClippedPoint, 0.f, false);
                        FWidgetPath Path(MakeArrayView(Bubble));
                        Test.TestFalse(*FString::Printf(TEXT("%s: clipped edge cards do not intercept input outside the ribbon"), *State),
                            Path.IsValid() && Path.ContainsWidget(Row->GetCachedWidget().Get()));
                    }
                }
            }
            if (!Items.IsEmpty())
            {
                Test.TestTrue(*FString::Printf(TEXT("%s: at least one complete card is visible"), *State), FullyVisibleCount > 0);
                const int32 VisibleCapacity = FMath::CeilToInt(ListRect.GetSize().X /
                    FMath::Max(1.f, List->GetEntryWidth())) + 2;
                Test.TestTrue(*FString::Printf(TEXT("%s: the list virtualizes facilities outside the ribbon"), *State),
                    VisibleRects.Num() <= Items.Num() &&
                    (Items.Num() <= VisibleCapacity * 2 || List->GetDisplayedEntryWidgets().Num() < Items.Num()));
            }
            VisibleRects.Sort([](const FSlateRect& A, const FSlateRect& B) { return A.Left < B.Left; });
            for (int32 Index = 1; Index < VisibleRects.Num(); ++Index)
            {
                Test.TestTrue(*FString::Printf(TEXT("%s: horizontal cards are evenly sized and do not overlap"), *State),
                    VisibleRects[Index - 1].Right <= VisibleRects[Index].Left + 1.5 &&
                    FMath::IsNearlyEqual(VisibleRects[Index - 1].Top, VisibleRects[Index].Top, 1.5f) &&
                    FMath::IsNearlyEqual(VisibleRects[Index - 1].GetSize().X, VisibleRects[Index].GetSize().X, 1.5f));
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
            TSet<UObject*> PictogramResources;
            for (UWidget* Child : Majors->GetAllChildren())
            {
                auto* Choice = Cast<UFF_PrepChoiceButton>(Child);
                auto* Icon = Choice ? Cast<UImage>(Choice->GetWidgetFromName(TEXT("Image_Icon"))) : nullptr;
                auto* Button = Choice ? Cast<UButton>(Choice->GetWidgetFromName(TEXT("Button_Root"))) : nullptr;
                auto* Caption = Choice ? Cast<UTextBlock>(Choice->GetWidgetFromName(TEXT("Text_Label"))) : nullptr;
                if (!Test.TestNotNull(TEXT("Every existing category keeps an icon"), Icon) ||
                    !Test.TestNotNull(TEXT("Every existing category keeps its keyboard button"), Button) ||
                    !Test.TestNotNull(TEXT("Pictograms retain their readable Korean captions"), Caption)) return false;
                UObject* Resource = Icon->GetBrush().GetResourceObject();
                Test.TestTrue(TEXT("Category pictograms use the saved SDF material instead of unrelated status artwork"),
                    Cast<UMaterialInterface>(Resource) && Resource->GetName().StartsWith(TEXT("MI_UI_CategoryClarity_")));
                Test.TestFalse(TEXT("Every category has a distinct schematic symbol"), PictogramResources.Contains(Resource));
                PictogramResources.Add(Resource);
                Test.TestTrue(TEXT("Each schematic icon occupies the authored compact 22px square"),
                    Icon->GetCachedGeometry().GetLocalSize().Equals(FVector2D(22, 22), .5));
                Test.TestTrue(TEXT("Category buttons preserve native keyboard focusability"),
                    Button->GetIsFocusable() && Button->TakeWidget()->SupportsKeyboardFocus());
                const FSlateRect IconRect = Icon->GetCachedGeometry().GetLayoutBoundingRect();
                const FSlateRect CaptionRect = Caption->GetCachedGeometry().GetLayoutBoundingRect();
                const FSlateRect ButtonRect = Button->GetCachedGeometry().GetLayoutBoundingRect();
                Test.TestTrue(TEXT("Category pictogram and caption remain side by side inside their button"),
                    !Caption->GetText().IsEmpty() && IconRect.Right < CaptionRect.Left &&
                    IconRect.Top >= ButtonRect.Top - 1.5f && IconRect.Bottom <= ButtonRect.Bottom + 1.5f &&
                    CaptionRect.Right <= ButtonRect.Right + 1.5f);
            }
            Test.TestEqual(TEXT("All facilities does not create a cross-category strip of every minor token"), Minors->GetChildrenCount(), 1);
            CheckFilterLayout(Test, TEXT("All facilities"));
            CheckCatalogRowPreview(Test, TEXT("All facilities / ribbon start"));
            Test.TestEqual(TEXT("Full source catalog uses one continuous ribbon"), Panel->GetCatalogPageCount(), 1);
            if (bCapture) { Capture(Test, TEXT("AllCategories")); }
            const TArray<UObject*> CatalogItems = Panel->GetCatalogPageItems();
            Test.TestEqual(TEXT("The unfiltered ribbon retains the full source catalog"), CatalogItems.Num(), SourceEntries.Num());
            TSet<FName> CatalogIds;
            for (UObject* Object : CatalogItems)
            {
                const auto* Item = Cast<UFF_PrepBuildTileData>(Object);
                if (!Test.TestNotNull(TEXT("A ribbon item is a real facility"), Item)) return false;
                Test.TestFalse(TEXT("Facility IDs never repeat in the continuous ribbon"), CatalogIds.Contains(Item->GetFacilityDefinitionId()));
                CatalogIds.Add(Item->GetFacilityDefinitionId());
            }
            Test.TestEqual(TEXT("The continuous ribbon covers every source facility"), CatalogIds.Num(), SourceEntries.Num());
            List->SetScrollOffset(List->GetNumItems());
            Paint();
            CheckCatalogRowPreview(Test, TEXT("All facilities / ribbon end"));
            Test.TestTrue(TEXT("Scrolling reaches the final source facility"),
                !CatalogItems.IsEmpty() && List->GetDisplayedEntryWidgets().ContainsByPredicate([this, &CatalogItems](UUserWidget* Row)
                {
                    return List->GetListObjectFromEntry(*Row) == CatalogItems.Last();
                }));
            List->ScrollToTop();
            Paint();
            for (int32 Index = 1; Index < Majors->GetChildrenCount(); ++Index)
            {
                auto* Choice = Cast<UFF_PrepChoiceButton>(Majors->GetChildAt(Index));
                if (!Test.TestNotNull(TEXT("Real category action"), Choice)) { return false; }
                Choice->OnHudTabButtonClicked.ExecuteIfBound(Choice->GetChoiceId());
                Paint();
                const auto Category = static_cast<EFFZSFacilityCategory>(Choice->GetChoiceId());
                Test.TestTrue(TEXT("Activating a pictogram preserves the current category selection"), Choice->IsSelected());
                auto* SelectedIcon = Cast<UImage>(Choice->GetWidgetFromName(TEXT("Image_Icon")));
                auto* SelectedLabel = Cast<UTextBlock>(Choice->GetWidgetFromName(TEXT("Text_Label")));
                Test.TestTrue(TEXT("Selected category icon follows the caption's readable selected tint"),
                    SelectedIcon && SelectedLabel && SelectedIcon->GetColorAndOpacity().Equals(
                        SelectedLabel->GetColorAndOpacity().GetSpecifiedColor(), .001f));
                TSet<FName> ExpectedMinors;
                for (const auto& Entry : SourceEntries) { if (Entry.Category == Category) { ExpectedMinors.Add(Entry.MinorToken); } }
                Test.TestEqual(TEXT("Selecting a category resets the ribbon's logical index"), Panel->GetCatalogPageIndex(), 0);
                Test.TestEqual(TEXT("Every real subcategory of the selected category is present"), Minors->GetChildrenCount(), ExpectedMinors.Num() + 1);
                CheckFilterLayout(Test, FString::Printf(TEXT("Major category %d"), Choice->GetChoiceId()));
                CheckCatalogRowPreview(Test, FString::Printf(TEXT("Major category %d"), Choice->GetChoiceId()));
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
            CheckCatalogRowPreview(Test, TEXT("Safety / First Aid selected"));
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
            CheckCatalogRowPreview(Test, TEXT("First Aid / favorite filter restored"));
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
                auto* Tile = Cast<UFF_PrepBuildTileWidget>(Row);
                auto* ProgramBorder = Cast<UBorder>(Row->GetWidgetFromName(TEXT("Border_ProgramBadge")));
                auto* ProgramLabel = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_ProgramBadge")));
                auto* ProgramBackdrop = Cast<UBorder>(Row->GetWidgetFromName(TEXT("Border_ThumbnailBackdrop")));
                if (!Test.TestNotNull(TEXT("Realized row has its current list item"), Item) ||
                    !Test.TestNotNull(TEXT("Realized row name"), Name) ||
                    !Test.TestNotNull(TEXT("Realized row thumbnail"), Thumbnail) ||
                    !Test.TestNotNull(TEXT("Realized row fixed thumbnail frame"), ThumbnailFrame) ||
                    !Test.TestNotNull(TEXT("Realized row card"), Card) ||
                    !Test.TestNotNull(TEXT("Realized native tile"), Tile) ||
                    !Test.TestNotNull(TEXT("Realized program badge border"), ProgramBorder) ||
                    !Test.TestNotNull(TEXT("Realized program badge caption"), ProgramLabel))
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
                const bool bHasPrograms = Item->GetEntry().CompatibleProgramCount > 0;
                const ESlateVisibility ProgramVisibility = bHasPrograms && !Item->IsRequiredFacility()
                    ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
                Test.TestTrue(*FString::Printf(TEXT("%s: pooled program badge belongs to the current facility and cannot intercept input"), *Label),
                    Tile->HasCompatiblePrograms() == bHasPrograms && ProgramBorder->GetVisibility() == ProgramVisibility &&
                    ProgramLabel->GetVisibility() == ProgramVisibility);
                // The saved style can use a material or a rounded Slate brush. Verify
                // the authored state contract rather than a retired material filename.
                const UWidgetTree* TileTree = CastChecked<UWidgetBlueprintGeneratedClass>(Row->GetClass())->GetWidgetTreeArchetype();
                const auto* AuthoredBackdrop = TileTree ? Cast<UBorder>(TileTree->FindWidget(TEXT("Border_ThumbnailBackdrop"))) : nullptr;
                FSlateBrush ExpectedBackdrop = AuthoredBackdrop ? AuthoredBackdrop->Background : FSlateBrush();
                const bool bMissingRequired = Item->IsRequiredFacility() && !Item->IsRequiredFacilitySatisfied();
                if (bMissingRequired || bHasPrograms)
                {
                    const auto* BrushProperty = FindFProperty<FStructProperty>(Row->GetClass(),
                        bMissingRequired ? TEXT("RequiredThumbnailBrush") : TEXT("ProgramThumbnailBrush"));
                    const auto* TintProperty = FindFProperty<FStructProperty>(Row->GetClass(),
                        bMissingRequired ? TEXT("RequiredCardTint") : TEXT("ProgramCardTint"));
                    if (BrushProperty && TintProperty)
                    {
                        const FSlateBrush& Accent = *BrushProperty->ContainerPtrToValuePtr<FSlateBrush>(Row);
                        if (Accent.GetResourceObject()) ExpectedBackdrop = Accent;
                        else ExpectedBackdrop.TintColor = *TintProperty->ContainerPtrToValuePtr<FLinearColor>(Row);
                    }
                    else Test.AddError(Label + TEXT(": authored facility accent properties are missing"));
                }
                Test.TestTrue(*FString::Printf(TEXT("%s: recycled thumbnail surface matches the current facility and authored style"), *Label),
                    ProgramBackdrop && AuthoredBackdrop && ProgramBackdrop->Background == ExpectedBackdrop);
                Test.TestTrue(*FString::Printf(TEXT("%s: pooled required and synergy flags belong to the current facility"), *Label),
                    Tile->IsRequiredFacility() == Item->IsRequiredFacility() &&
                    Tile->IsRequiredFacilitySatisfied() == Item->IsRequiredFacilitySatisfied() &&
                    Tile->IsSynergyHighlighted() == Item->IsSynergyHighlighted());
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
                    continue; // The list may prefetch an additional clipped edge card.
                }
                ++VisibleCount;
                const bool bFullyVisible = CardRect.Left >= ListRect.Left - 1.0 &&
                    CardRect.Right <= ListRect.Right + 1.0 &&
                    CardRect.Top >= ListRect.Top - 1.0 && CardRect.Bottom <= ListRect.Bottom + 1.0;
                FullyVisibleCount += bFullyVisible ? 1 : 0;
                // Slate culls descendants outside the list clip. A partially visible edge card
                // may therefore carry an earlier paint geometry for its clipped image/name.
                // Data and desired-size checks above still cover every pooled row; inspect
                // painted child geometry only where the entire card was actually painted.
                if (!bFullyVisible) { continue; }
                TArray<const UWidget*> VisibleBadges;
                for (const TCHAR* BadgeName : { TEXT("Text_RequiredMissing"), TEXT("Text_RequiredSatisfied"),
                    TEXT("Text_ProgramBadge"), TEXT("Text_SynergyBadge") })
                {
                    const UWidget* Badge = Row->GetWidgetFromName(BadgeName);
                    if (!Badge || !Badge->IsVisible()) { continue; }
                    const FSlateRect BadgeRect = Badge->GetCachedGeometry().GetLayoutBoundingRect();
                    Test.TestTrue(*FString::Printf(TEXT("%s: %s fits inside the card"), *Label, BadgeName),
                        BadgeRect.GetSize().X > 1 && BadgeRect.GetSize().Y > 1 &&
                        BadgeRect.Left >= CardRect.Left - 1.5 && BadgeRect.Right <= CardRect.Right + 1.5 &&
                        BadgeRect.Top >= CardRect.Top - 1.5 && BadgeRect.Bottom <= CardRect.Bottom + 1.5);
                    if (FString(BadgeName) != TEXT("Text_SynergyBadge"))
                    {
                        const FSlateRect ThumbnailRect = ThumbnailFrame->GetCachedGeometry().GetLayoutBoundingRect();
                        Test.TestTrue(*FString::Printf(TEXT("%s: %s shares the bottom badge row without covering the thumbnail center"), *Label, BadgeName),
                            BadgeRect.Top >= ThumbnailRect.Top + ThumbnailRect.GetSize().Y * 0.6f &&
                            BadgeRect.Bottom <= ThumbnailRect.Bottom + 1.5f);
                    }
                    for (const UWidget* PreviousBadge : VisibleBadges)
                    {
                        const FSlateRect PreviousRect = PreviousBadge->GetCachedGeometry().GetLayoutBoundingRect();
                        const bool bOverlap = BadgeRect.Left < PreviousRect.Right - 1 && BadgeRect.Right > PreviousRect.Left + 1 &&
                            BadgeRect.Top < PreviousRect.Bottom - 1 && BadgeRect.Bottom > PreviousRect.Top + 1;
                        Test.TestFalse(*FString::Printf(TEXT("%s: %s and %s remain independently readable"),
                            *Label, BadgeName, *PreviousBadge->GetName()), bOverlap);
                    }
                    VisibleBadges.Add(Badge);
                }
                const FVector2D CardSize = Card->GetCachedGeometry().GetLocalSize();
                const FVector2D ThumbnailSize = ThumbnailFrame->GetCachedGeometry().GetLocalSize();
                if (Measurements.CardSize.IsNearlyZero())
                {
                    Measurements.CardSize = CardSize;
                    Measurements.ThumbnailSize = ThumbnailSize;
                }
                const auto* CardFrame = Cast<USizeBox>(Row->GetWidgetFromName(TEXT("SizeBox_Card")));
                Test.TestTrue(*FString::Printf(TEXT("%s: card respects its authored height and allocated column width (painted %.1fx%.1f, frame width %.1f enabled=%d height %.1f enabled=%d, list entry %.1fx%.1f)"),
                    *Label, CardSize.X, CardSize.Y,
                    CardFrame ? CardFrame->GetWidthOverride() : -1.f, CardFrame && CardFrame->IsWidthOverride(),
                    CardFrame ? CardFrame->GetHeightOverride() : -1.f, CardFrame && CardFrame->IsHeightOverride(),
                    List->GetEntryWidth(), List->GetEntryHeight()),
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
                const bool bNameFits = Name->GetDesiredSize().Y <= Name->GetCachedGeometry().GetLocalSize().Y + 1.0f &&
                    Name->GetDesiredSize().X <= Name->GetCachedGeometry().GetLocalSize().X + 1.0f;
                Test.TestTrue(*FString::Printf(TEXT("%s: name fits or uses authored ellipsis with the complete name in its tooltip"), *Label),
                    bNameFits || (Name->GetTextOverflowPolicy() == ETextOverflowPolicy::Ellipsis &&
                        Tile->GetToolTipText().ToString().Contains(Item->GetDisplayName().ToString())));
                Test.TestTrue(*FString::Printf(TEXT("%s: name keeps visible space inside its card"), *Label),
                    NameRect.Right > NameRect.Left && NameRect.Bottom - NameRect.Top >= Name->GetFont().Size * DpiScale * 0.7 &&
                    NameRect.Top >= CardRect.Top - 1.0 && NameRect.Bottom <= CardRect.Bottom + 1.0);
                if (const auto* Cost = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Cost"))))
                {
                    const FSlateRect CostRect = Cost->GetCachedGeometry().GetLayoutBoundingRect();
                    Test.TestTrue(*FString::Printf(TEXT("%s: installation price stays below the name and inside the card (card LTRB %.1f,%.1f,%.1f,%.1f; name bottom %.1f; cost LTRB %.1f,%.1f,%.1f,%.1f)"),
                        *Label, CardRect.Left, CardRect.Top, CardRect.Right, CardRect.Bottom, NameRect.Bottom,
                        CostRect.Left, CostRect.Top, CostRect.Right, CostRect.Bottom),
                        CostRect.Top >= NameRect.Bottom - 1.0 && CostRect.Bottom <= CardRect.Bottom + 1.0 &&
                        CostRect.Left >= CardRect.Left - 1.0 && CostRect.Right <= CardRect.Right + 1.0);
                }
            }
            Test.TestTrue(*FString::Printf(TEXT("%s: at least one card is inside the visible list"), *Step), VisibleCount > 0);
            if (bRequireWholeCard)
            {
                Test.TestTrue(*FString::Printf(TEXT("%s: the ribbon leaves room for a complete card and its name"), *Step), FullyVisibleCount > 0);
            }
        }

        bool Capture(FAutomationTestBase& Test, const FString& Suffix)
        {
            // A first offscreen draw can request an as-yet uncompiled material shader
            // map. Finishing only already queued jobs left the first 720p destination
            // capture without its rim/badge while the later 1080p capture was correct.
            // Warm the actual panel and pooled-row brushes before either resolution.
            TSet<UMaterialInterface*> Materials;
            auto GatherBrush = [&Materials](const FSlateBrush& Brush)
            {
                if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) { Materials.Add(Material); }
            };
            auto GatherWidget = [&](UWidget* Widget)
            {
                if (const auto* Border = Cast<UBorder>(Widget)) { GatherBrush(Border->Background); }
                if (const auto* Image = Cast<UImage>(Widget)) { GatherBrush(Image->GetBrush()); }
                if (const auto* Button = Cast<UButton>(Widget))
                {
                    GatherBrush(Button->GetStyle().Normal);
                    GatherBrush(Button->GetStyle().Hovered);
                    GatherBrush(Button->GetStyle().Pressed);
                    GatherBrush(Button->GetStyle().Disabled);
                }
            };
            if (Panel->WidgetTree) { Panel->WidgetTree->ForEachWidget(GatherWidget); }
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                if (Row && Row->WidgetTree) { Row->WidgetTree->ForEachWidget(GatherWidget); }
            }
            for (UMaterialInterface* Material : Materials) { Material->EnsureIsComplete(); }
            if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); }
            FlushRenderingCommands();
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
            for (UUserWidget* Row : List->GetDisplayedEntryWidgets())
            {
                const auto* Tile = Cast<UFF_PrepBuildTileWidget>(Row);
                const auto* Badge = Cast<UBorder>(Row->GetWidgetFromName(TEXT("Border_CodexFocusBadge")));
                if (!Tile || !Tile->IsCodexFocused() || !Badge || !Badge->IsVisible()) { continue; }
                const FSlateRect Rect = Badge->GetCachedGeometry().GetLayoutBoundingRect();
                // Sample the center area, excluding rounded edges. The cream glyphs
                // occupy part of it, but a real dark teal fill must cover at least 25%.
                // With a missing shader only pale card pixels and cream text remain.
                const int32 MinX = FMath::Clamp(FMath::CeilToInt(Rect.Left + Rect.GetSize().X * 0.15f), 0, Width - 1);
                const int32 MaxX = FMath::Clamp(FMath::FloorToInt(Rect.Right - Rect.GetSize().X * 0.15f), 0, Width - 1);
                const int32 MinY = FMath::Clamp(FMath::CeilToInt(Rect.Top + Rect.GetSize().Y * 0.2f), 0, Height - 1);
                const int32 MaxY = FMath::Clamp(FMath::FloorToInt(Rect.Bottom - Rect.GetSize().Y * 0.2f), 0, Height - 1);
                int32 DarkTealPixels = 0;
                int32 SampleCount = 0;
                for (int32 Y = MinY; Y <= MaxY; ++Y)
                {
                    for (int32 X = MinX; X <= MaxX; ++X)
                    {
                        const FColor Color = Pixels[Y * Width + X];
                        ++SampleCount;
                        DarkTealPixels += Color.A >= 240 && Color.R < 100 && Color.G > 20 &&
                            Color.G < 145 && Color.B < 145 && Color.G > Color.R + 8 ? 1 : 0;
                    }
                }
                Test.TestTrue(*FString::Printf(TEXT("%dx%d codex destination badge renders its dark teal background (%d/%d pixels)"),
                    Width, Height, DarkTealPixels, SampleCount),
                    SampleCount > 0 && DarkTealPixels > SampleCount / 4);
            }
            int32 VisiblePixels = 0;
            for (const FColor& Pixel : Pixels)
            {
                VisiblePixels += Pixel.A > 0 && (Pixel.R > 25 || Pixel.G > 25 || Pixel.B > 25) ? 1 : 0;
            }
            // The dock has a capped width and follows project DPI; wider game viewports
            // must not increase the required painted area of the same sparse catalog.
            const UWidget* Dock = Panel->GetWidgetFromName(TEXT("Border_PanelRoot"));
            if (!Test.TestNotNull(TEXT("Rendered catalog has its authored dock"), Dock)) { return false; }
            const float PaintedWidth = Dock->GetCachedGeometry().GetLayoutBoundingRect().GetSize().X;
            Test.TestTrue(TEXT("Catalog image contains visible cards and labels"),
                PaintedWidth > 0.f && VisiblePixels > PaintedWidth * 100.f * DpiScale);
            const UWidget* MarkSurface = Panel->GetWidgetFromName(TEXT("Border_BuildMark"));
            if (const auto* Mark = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("Block_BuildMark00")));
                Mark && MarkSurface && MarkSurface->IsVisible())
            {
                const FSlateRect MarkRect = Mark->GetCachedGeometry().GetLayoutBoundingRect();
                const int32 SampleX = FMath::Clamp(FMath::RoundToInt((MarkRect.Left + MarkRect.Right) * 0.5f), 0, Width - 1);
                const int32 SampleY = FMath::Clamp(FMath::RoundToInt((MarkRect.Top + MarkRect.Bottom) * 0.5f), 0, Height - 1);
                const FColor Actual = Pixels[SampleY * Width + SampleX];
                if (Cast<UMaterialInterface>(Mark->Background.GetResourceObject()))
                {
                    // The authored Aurora surface varies over its UVs; a white brush tint is
                    // not its rendered color. Verify the saved green material is visible.
                    Test.TestTrue(*FString::Printf(TEXT("Catalog iridescent material is opaque and green (RGBA %d, %d, %d, %d)"),
                        Actual.R, Actual.G, Actual.B, Actual.A),
                        Actual.A >= 240 && Actual.G > 25 && Actual.G > Actual.B + 2 && Actual.G >= Actual.R);
                }
                else
                {
                    const FColor Expected = (Mark->Background.TintColor.GetSpecifiedColor() * Mark->GetBrushColor()).ToFColor(true);
                    Test.TestTrue(TEXT("PNG preserves the authored solid palette without a second gamma conversion"),
                        FMath::Abs(static_cast<int32>(Actual.R) - Expected.R) <= 3 &&
                        FMath::Abs(static_cast<int32>(Actual.G) - Expected.G) <= 3 &&
                        FMath::Abs(static_cast<int32>(Actual.B) - Expected.B) <= 3);
                }
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

        void ShowFirstPage() { List->ScrollToTop(); }
        void ShowLastPage() { List->SetScrollOffset(List->GetNumItems()); }

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
        FName ProgramPreviewFacilityId = NAME_None;
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
                Fixture.List->SetScrollOffset(Fixture.List->GetNumItems());
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
        if (!Fixture.CheckCatalogRowPreview(Test, TEXT("Source catalog synergy recommendation")) ||
            !Fixture.CheckSynergyCatalogPreview(Test) || !Fixture.CheckFavoriteInputIsolation(Test)) { return false; }
        const bool bCaptured = Fixture.Capture(Test, TEXT("Synergy"));
        Fixture.List->ClearListItems();
        return bCaptured;
    }

    bool RunFacilityNavigationFixture(FAutomationTestBase& Test)
    {
        Test.AddExpectedError(TEXT("ViewModel 을 찾지 못했다"), EAutomationExpectedErrorFlags::Contains, 2);
        for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
        {
            FCatalogFixture Fixture;
            if (!Fixture.Initialize(Test, FVector2D(Resolution.X, Resolution.Y), true) ||
                !Fixture.UseSourceCatalogEntries(Test) || !Fixture.CheckFacilityNavigation(Test) ||
                !Fixture.Capture(Test, TEXT("PartnerNavigation"))) { return false; }
        }
        return true;
    }

    bool RunRequiredCatalogFixture(FAutomationTestBase& Test)
    {
        if (!Test.TestTrue(TEXT("Slate application is initialized"), FSlateApplication::IsInitialized())) { return false; }
        Test.AddExpectedError(TEXT("ViewModel 을 찾지 못했다"), EAutomationExpectedErrorFlags::Contains, 2);
        for (const FIntPoint Resolution : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
        {
            FCatalogFixture Fixture;
            if (!Fixture.Initialize(Test, FVector2D(Resolution.X, Resolution.Y), true) ||
                !Fixture.UseSourceCatalogEntries(Test)) { return false; }
            FMeasurements Measurements;
            for (int32 Step = 0; Step < 3; ++Step)
            {
                const bool bInstalled = Step == 1;
                const TCHAR* State = Step == 0 ? TEXT("Required stage missing")
                    : bInstalled ? TEXT("Required stage installed") : TEXT("Required stage removed");
                const TCHAR* CaptureSuffix = Step == 0 ? TEXT("RequiredMissing")
                    : bInstalled ? TEXT("RequiredSatisfied") : TEXT("RequiredRemoved");
                if (!Fixture.UseRequiredCatalogPreview(Test, bInstalled)) { return false; }
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, State, true);
                if (!Fixture.CheckRequiredCatalogPreview(Test, bInstalled) ||
                    !Fixture.Capture(Test, CaptureSuffix)) { return false; }
            }
            if (!Fixture.UseProgramCatalogPreview(Test)) { return false; }
            Fixture.Paint();
            Fixture.CheckRows(Test, Measurements, TEXT("Actual program-compatible booths"), true);
            if (!Fixture.CheckProgramCatalogPreview(Test) || !Fixture.Capture(Test, TEXT("ProgramFacilities"))) { return false; }
            Fixture.List->ClearListItems();
        }
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
            if (!Fixture.CheckAllCategoriesAndFirstAid(Test, !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))) { return false; }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ClarityCatalogDragTest,
    "FestaFiesta.UI.PrepBuild.ClarityCatalogDrag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_ClarityCatalogDragTest::RunTest(const FString& Parameters)
{
    if (!TestTrue(TEXT("Slate is initialized"), FSlateApplication::IsInitialized())) return false;
    AddExpectedError(TEXT("ViewModel 을 찾지 못했다"), EAutomationExpectedErrorFlags::Contains, 2);
    for (const FIntPoint Resolution : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
    {
        FFPrepBuildVirtualizedSlateTest::FCatalogFixture Fixture;
        if (!Fixture.Initialize(*this, FVector2D(Resolution.X, Resolution.Y), true) ||
            !Fixture.UseSourceCatalogEntries(*this)) return false;
        TStrongObjectPtr<UWorld> InputWorld(NewObject<UWorld>());
        TStrongObjectPtr<UFF_BuildModeSubsystem> InputMode(NewObject<UFF_BuildModeSubsystem>(InputWorld.Get()));
        InputMode->State = EFF_BuildModeState::CatalogOpen;
        Fixture.GetPanel()->BuildModeSubsystem = InputMode.Get();
        // Missing authoritative gold deliberately rejects the positive click control;
        // no test can place in or charge the user's editor world.
        Fixture.GetPanel()->EconomyReadModel = nullptr;
        if (!Fixture.CheckCatalogDragInput(*this)) return false;
        TestEqual(TEXT("Drag, favorite and rejected click never enter world placement"),
            InputMode->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
        TestTrue(TEXT("Input fixture never creates a selected world spec"), InputMode->GetSelectedFacilityDefinitionId().IsNone());
        Fixture.GetPanel()->BuildModeSubsystem = nullptr;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildVirtualizedScrollLifecycleTest,
    "FestaFiesta.UI.PrepBuild.VirtualizedScrollLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildVirtualizedScrollLifecycleTest::RunTest(const FString& Parameters)
{
    return FFPrepBuildVirtualizedSlateTest::RunCatalogFixture(*this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildReleasedRowPointerLifecycleTest,
    "FestaFiesta.UI.PrepBuild.ReleasedRowPointerLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildReleasedRowPointerLifecycleTest::RunTest(const FString& Parameters)
{
    if (!TestTrue(TEXT("Slate is initialized"), FSlateApplication::IsInitialized())) return false;
    FFPrepBuildVirtualizedSlateTest::FCatalogFixture Fixture;
    if (!Fixture.Initialize(*this, FVector2D(1280, 720))) return false;
    // This row-lifecycle fixture supplies synthetic list items without a gameplay
    // PresentationModel. Its panel would reject those IDs and clear every selection.
    // Keep the real list/row selection pipeline, with only that unrelated handler removed.
    Fixture.List->OnItemSelectionChanged().RemoveAll(Fixture.GetPanel());
    // These two synthetic entries have neither required nor program accents. Keep only
    // one realized row so that the subsequent item must reuse the same pooled widget.
    TStrongObjectPtr<UObject> FirstItem(Fixture.List->GetListItems()[2]);
    TStrongObjectPtr<UObject> NextItem(Fixture.List->GetListItems()[10]);
    Fixture.List->SetListItems(TArray<UObject*>{ FirstItem.Get() });
    Fixture.Paint();
    auto* Entry = Fixture.List->GetEntryWidgetFromItem<UFF_PrepBuildTileWidget>(FirstItem.Get());
    if (!TestNotNull(TEXT("Actual saved tile entry is realized"), Entry)) return false;
    auto* Card = Cast<UBorder>(Entry->GetWidgetFromName(TEXT("Border_Card")));
    auto* CodexRim = Cast<UImage>(Entry->GetWidgetFromName(TEXT("Image_CodexFocusRim")));
    auto* CodexBadge = Cast<UBorder>(Entry->GetWidgetFromName(TEXT("Border_CodexFocusBadge")));
    if (!TestNotNull(TEXT("Pooled tile retains authored codex rim"), CodexRim) ||
        !TestNotNull(TEXT("Pooled tile retains authored codex badge"), CodexBadge)) return false;
    CastChecked<UFF_PrepBuildTileData>(FirstItem.Get())->SetCodexFocused(true);
    Entry->RefreshItemPresentation();
    TestTrue(TEXT("First item has a codex accent independent of its selection"),
        Entry->IsCodexFocused() && CodexRim->GetVisibility() == ESlateVisibility::HitTestInvisible &&
        CodexBadge->GetVisibility() == ESlateVisibility::HitTestInvisible);
    const TSharedPtr<SObjectTableRow<UObject*>> SlateRow =
        StaticCastSharedPtr<SObjectTableRow<UObject*>>(Entry->GetCachedWidget());
    const TSharedPtr<STableViewBase> SlateList =
        StaticCastSharedPtr<STableViewBase>(Fixture.List->GetCachedWidget());
    if (!TestNotNull(TEXT("Saved tile card brush"), Card) ||
        !TestTrue(TEXT("Production Slate row and owner exist"), SlateRow.IsValid() && SlateList.IsValid())) return false;
    const FGeometry Geometry = Entry->GetCachedGeometry();
    const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
    const FPointerEvent Pointer(0, Point, Point, TSet<FKey>(), EKeys::Invalid, 0.f, FModifierKeysState());
    const FLinearColor NormalTint = Card->Background.TintColor.GetSpecifiedColor();
    Fixture.List->SetItemSelection(FirstItem.Get(), true);
    if (!TestTrue(TEXT("The real list has selected the first fixture item"),
        Fixture.List->GetSelectedItem() == FirstItem.Get())) return false;
    // Deliver the real engine selection detector normally invoked by its active timer;
    // the isolated paint fixture does not advance the application's global timers.
    SlateRow->DetectItemSelectionChanged(FPlatformTime::Seconds(), 0.f);
    const FLinearColor SelectedTint = Card->Background.TintColor.GetSpecifiedColor();
    TestFalse(TEXT("Selection notification changes the visible card tint"), SelectedTint.Equals(NormalTint));
    SlateRow->OnMouseEnter(Geometry, Pointer);
    Entry->RefreshItemPresentation();
    SlateRow->OnMouseLeave(Pointer);
    TestTrue(TEXT("Hover and presentation refresh preserve selection"),
        Card->Background.TintColor.GetSpecifiedColor().Equals(SelectedTint));
    SlateRow->InitializeRow();
    TestTrue(TEXT("Reinitializing the same mapped item preserves its selection tint"),
        Card->Background.TintColor.GetSpecifiedColor().Equals(SelectedTint));
    Fixture.List->ClearSelection();
    TestTrue(TEXT("The real list clears the fixture selection"), Fixture.List->GetSelectedItem() == nullptr);
    SlateRow->DetectItemSelectionChanged(FPlatformTime::Seconds(), 0.f);
    TestTrue(TEXT("Deselection notification restores the normal card tint"),
        Card->Background.TintColor.GetSpecifiedColor().Equals(NormalTint));
    SlateRow->OnMouseEnter(Geometry, Pointer);
    TestFalse(TEXT("An attached unselected card still responds to hover"),
        Card->Background.TintColor.GetSpecifiedColor().Equals(NormalTint));
    Fixture.List->SetItemSelection(FirstItem.Get(), true);
    if (!TestTrue(TEXT("The entry is selected before release"),
        Fixture.List->GetSelectedItem() == FirstItem.Get())) return false;
    SlateRow->DetectItemSelectionChanged(FPlatformTime::Seconds(), 0.f);

    // A pointer path can outlive the page it hovered. Retain its actual SObjectTableRow
    // while the list removes its mapping and completes refresh, exactly the window in
    // which IsListItemSelected used to assert in NativeOnMouseLeave after a catalog drag.
    Fixture.List->ClearListItems();
    Fixture.Paint();
    if (!TestEqual(TEXT("The cleared catalog releases its only entry"), Fixture.List->GetDisplayedEntryWidgets().Num(), 0) ||
        !TestFalse(TEXT("The detached row's list has completed its refresh"), SlateList->IsPendingRefresh()) ||
        !TestTrue(TEXT("The retained Slate row still wraps the released widget"),
            SlateRow->GetUserWidget() == Entry && IObjectTableRow::ObjectRowFromUserWidget(Entry).IsValid()) ||
        !TestTrue(TEXT("The released widget no longer has a list item mapping"),
            Fixture.List->GetListObjectFromEntry(*Entry) == nullptr && Entry->GetPresentedItem() == nullptr)) return false;
    SlateRow->OnMouseLeave(Pointer);
    SlateRow->OnMouseEnter(Geometry, Pointer);
    Entry->RefreshItemPresentation();
    TestTrue(TEXT("Late pointer events and presentation refresh leave the released row neutral"),
        Card->Background.TintColor.GetSpecifiedColor().Equals(NormalTint));
    TestTrue(TEXT("Released rows clear their codex rim and origin caption"),
        !Entry->IsCodexFocused() && CodexRim->GetVisibility() == ESlateVisibility::Collapsed &&
        CodexBadge->GetVisibility() == ESlateVisibility::Collapsed);
    SlateRow->OnMouseLeave(Pointer);

    Fixture.List->SetListItems(TArray<UObject*>{ NextItem.Get() });
    Fixture.Paint();
    if (!TestTrue(TEXT("A different item reuses the retained pooled widget"),
        Fixture.List->GetEntryWidgetFromItem<UFF_PrepBuildTileWidget>(NextItem.Get()) == Entry)) return false;
    TestTrue(TEXT("The reused entry binds the new item without stale selection or hover"),
        Entry->GetPresentedItem() == NextItem.Get() && Card->Background.TintColor.GetSpecifiedColor().Equals(NormalTint));
    TestTrue(TEXT("A replacement facility never inherits the previous codex origin"),
        !Entry->IsCodexFocused() && CodexRim->GetVisibility() == ESlateVisibility::Collapsed &&
        CodexBadge->GetVisibility() == ESlateVisibility::Collapsed);
    Fixture.List->SetItemSelection(NextItem.Get(), true);
    if (!TestTrue(TEXT("The real list selects the replacement item"),
        Fixture.List->GetSelectedItem() == NextItem.Get())) return false;
    SlateRow->DetectItemSelectionChanged(FPlatformTime::Seconds(), 0.f);
    TestTrue(TEXT("The reused row still receives selection changes"),
        Card->Background.TintColor.GetSpecifiedColor().Equals(SelectedTint));
    Fixture.List->RegenerateAllEntries();
    Fixture.Paint();
    auto* Regenerated = Fixture.List->GetEntryWidgetFromItem<UFF_PrepBuildTileWidget>(NextItem.Get());
    if (!TestNotNull(TEXT("Selected item is realized again after regeneration"), Regenerated)) return false;
    StaticCastSharedPtr<SObjectTableRow<UObject*>>(Regenerated->GetCachedWidget())->DetectItemSelectionChanged(
        FPlatformTime::Seconds(), 0.f);
    auto* RegeneratedCard = Cast<UBorder>(Regenerated->GetWidgetFromName(TEXT("Border_Card")));
    TestTrue(TEXT("Regenerated selected item regains its selection through the engine callback"),
        RegeneratedCard && RegeneratedCard->Background.TintColor.GetSpecifiedColor().Equals(SelectedTint));
    Fixture.List->ClearListItems();
    Fixture.Paint();
    return true;
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
