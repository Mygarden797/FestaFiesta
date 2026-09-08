// Uses the saved catalog and tile WBP classes, including their real pooled Slate rows.
// Synthetic data is confined to this fixture; no catalog, economy or building command runs.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
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
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

namespace FFPrepBuildVirtualizedSlateTest
{
    constexpr int32 ItemCount = 144;
    constexpr const TCHAR* PanelPath =
        TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild.WBP_FF_PrepPanelBuild_C");
    constexpr const TCHAR* TilePath =
        TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile.WBP_FF_PrepBuildTile_C");

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
        bool Initialize(FAutomationTestBase& Test, const FVector2D InViewportSize, const float PanelHeight)
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
            SetText(TEXT("Text_Status"), TEXT("시설을 선택해 배치하세요"));
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
            Window = SNew(SVirtualWindow).Size(ViewportSize);
            Window->SetContent(SNew(SOverlay)
                + SOverlay::Slot().VAlign(VAlign_Bottom)
                [ SNew(SBox).HeightOverride(PanelHeight)[PanelSlate] ]);
            return true;
        }

        bool UseSourceCatalogEntries(FAutomationTestBase& Test)
        {
            // The final visual preview uses real source definitions, names, thumbnails and
            // prices. Mixed-resolution diagnostic textures belong only to the scroll test.
            const TCHAR* DefinitionNames[] = {
                TEXT("Booth_FoodStall_DadaPochaFoodTruck"), TEXT("Booth_CraftExperience_SandArt"),
                TEXT("Booth_MarketGoods_FestivalGoods"), TEXT("Booth_Nightlife_EDMBar"),
                TEXT("Performance_MainStage_SunsetMainStage"), TEXT("Performance_SubStage_LocalArtist"),
                TEXT("Performance_ExperienceStage_DanceBattle"), TEXT("Performance_Busking_StreetStage"),
                TEXT("Info_GeneralInfo_MainInformation"), TEXT("Rest_Seating_MainRestArea"),
                TEXT("Safety_FirstAid_MedicalTent"), TEXT("PhotoSpot_Landmark_SunsetLandmark"),
                TEXT("Toilet_Standard_MainRestroom"), TEXT("Toilet_Accessible_WestGate")
            };
            UDataTable* Costs = LoadObject<UDataTable>(nullptr,
                TEXT("/Game/FestaFiesta/Data/Economy/DT_FacilityCosts.DT_FacilityCosts"));
            if (!Test.TestNotNull(TEXT("Production facility price table"), Costs)) { return false; }
            TArray<UObject*> Items;
            TArray<UTexture*> SourceTextures;
            for (const TCHAR* DefinitionName : DefinitionNames)
            {
                TArray<FString> Tokens;
                FString(DefinitionName).ParseIntoArray(Tokens, TEXT("_"));
                const FString Path = FString::Printf(
                    TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/%s/PDA_FFZS_%s.PDA_FFZS_%s"),
                    *Tokens[0], DefinitionName, DefinitionName);
                UFFZSFacilityDefinition* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr, *Path);
                if (!Test.TestNotNull(*FString::Printf(TEXT("Source facility %s"), DefinitionName), Definition)) { return false; }
                Definitions.Emplace(Definition);
                UTexture2D* Texture = Definition->CatalogThumbnail.LoadSynchronous();
                if (!Test.TestNotNull(*FString::Printf(TEXT("Real facility thumbnail %s"), DefinitionName), Texture)) { return false; }
                Textures.Emplace(Texture);
                SourceTextures.Add(Texture);
                FFF_BuildCatalogEntry Entry;
                Entry.PrimaryAssetId = Definition->GetPrimaryAssetId();
                Entry.FacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
                Entry.DisplayName = Definition->Spec.DisplayName;
                Entry.Category = Definition->Spec.Category;
                Entry.MinorToken = FName(*Tokens[1]);
                Entry.Definition = Definition;
                Entry.Thumbnail = Texture;
                Entry.MaxInstancesPerWorld = Definition->Spec.MaxInstancesPerWorld;
                FString CostFailure;
                FFBuildCatalogModel::ApplyFacilityCosts(Entry, Definition, Costs, 0.5f, CostFailure);
                UFF_PrepBuildTileData* Item = NewObject<UFF_PrepBuildTileData>(Panel.Get());
                Item->Initialize(Entry, 7500);
                Items.Add(Item);
            }
            List->SetListItems(Items);
            SetText(TEXT("Text_ItemCount"), TEXT("시설 14 / 14"));
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
                Test.TestTrue(*FString::Printf(TEXT("%s: even clipped rows keep the current image's aspect and bounded desired size"), *Label),
                    Texture && ImageDesiredSize.X > 0.0 && ImageDesiredSize.Y > 0.0 &&
                    ImageDesiredSize.X <= FFPrepBuildTileLayout::ThumbnailWidth + 1.0 &&
                    ImageDesiredSize.Y <= FFPrepBuildTileLayout::ThumbnailHeight + 1.0 &&
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
                Test.TestTrue(*FString::Printf(TEXT("%s: card has the fixed production dimensions"), *Label),
                    CardSize.Equals(FVector2D(FFPrepBuildTileLayout::CardWidth, FFPrepBuildTileLayout::CardHeight), 1.0));
                Test.TestTrue(*FString::Printf(TEXT("%s: card dimensions survive reuse"), *Label),
                    CardSize.Equals(Measurements.CardSize, 1.0));
                Test.TestTrue(*FString::Printf(TEXT("%s: thumbnail frame survives mixed native resolutions"), *Label),
                    ThumbnailSize.Equals(Measurements.ThumbnailSize, 1.0) && ThumbnailSize.Equals(FVector2D(
                        FFPrepBuildTileLayout::ThumbnailWidth, FFPrepBuildTileLayout::ThumbnailHeight), 1.0));
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
                    NameRect.Right > NameRect.Left && NameRect.Bottom - NameRect.Top >= 12.0 &&
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
            if (const UWidget* GoldBacking = Panel->GetWidgetFromName(TEXT("Border_Gold")))
            {
                const FSlateRect GoldRect = GoldBacking->GetCachedGeometry().GetLayoutBoundingRect();
                const int32 SampleX = FMath::Clamp(FMath::RoundToInt((GoldRect.Left + GoldRect.Right) * 0.5f), 0, Width - 1);
                const int32 SampleY = FMath::Clamp(FMath::RoundToInt(GoldRect.Top + 2.0f), 0, Height - 1);
                const FColor Actual = Pixels[SampleY * Width + SampleX];
                const FColor Expected = UFF_UIPalette::Get()->InGame.TealDeep.ToFColor(true);
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
        TArray<TStrongObjectPtr<UTexture2D>> Textures;
        TArray<TStrongObjectPtr<UFFZSFacilityDefinition>> Definitions;
        FHittestGrid HitGrid;
        TSharedPtr<SVirtualWindow> Window;
        FVector2D ViewportSize = FVector2D::ZeroVector;
        int32 FrameNumber = 0;
    };

    bool RunCatalogFixture(FAutomationTestBase& Test, const bool bCapture)
    {
        if (!Test.TestTrue(TEXT("Slate application is initialized"), FSlateApplication::IsInitialized())) { return false; }
        for (const FIntPoint Resolution : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
        {
            FCatalogFixture Fixture;
            if (!Fixture.Initialize(Test, FVector2D(Resolution.X, Resolution.Y), FMath::Min(320.0f, Resolution.Y * 0.4f))) { return false; }
            FMeasurements Measurements;
            if (bCapture)
            {
                if (!Fixture.UseSourceCatalogEntries(Test)) { return false; }
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Source catalog top"), true);
                Fixture.Capture(Test, TEXT("Top"));
                Fixture.List->ScrollToBottom();
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Source catalog bottom"), true);
                Fixture.Capture(Test, TEXT("Bottom"));
                Fixture.List->ScrollToTop();
                Fixture.Paint();
                Fixture.CheckRows(Test, Measurements, TEXT("Source catalog returned"), true);
                Fixture.Capture(Test, TEXT("Returned"));
                Fixture.List->ClearListItems();
                continue;
            }
            Fixture.Paint();
            Fixture.CheckRows(Test, Measurements, TEXT("Initial top"), true);
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
    return FFPrepBuildVirtualizedSlateTest::RunCatalogFixture(*this, true);
}

#endif
