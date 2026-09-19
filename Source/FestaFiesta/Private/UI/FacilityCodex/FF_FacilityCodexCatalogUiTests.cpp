// Verifies the additive codex button in the saved production facility catalog.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "Slate/WidgetRenderer.h"
#include "TextureCompiler.h"
#include "TextureResource.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

namespace FFFacilityCodexCatalogTest
{
	struct FPlayerlessInitialization
	{
		explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
			: Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
		{ Class.bCanCallInitializedWithoutPlayerContext = true; }
		~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
		UWidgetBlueprintGeneratedClass& Class;
		bool bPrevious;
	};

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner, const float Tolerance = 1.f)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Top >= Outer.Top - Tolerance &&
			Inner.Right <= Outer.Right + Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	bool Overlaps(const FSlateRect& A, const FSlateRect& B)
	{
		return A.Left < B.Right - .5f && A.Right > B.Left + .5f &&
			A.Top < B.Bottom - .5f && A.Bottom > B.Top + .5f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexCatalogUiTest,
	"FestaFiesta.FacilityCodex.UI.CatalogHeader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexCatalogUiTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityCodexCatalogTest;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto LoadWbp = [](const TCHAR* Path)
	{
		return Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(Path).TryLoadClass<UUserWidget>());
	};
	auto* PanelClass = LoadWbp(TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild.WBP_FF_PrepPanelBuild_C"));
	auto* LauncherClass = LoadWbp(TEXT("/Game/FestaFiesta/UI/FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher.WBP_FF_FacilityCodexCatalogLauncher_C"));
	auto* TileClass = LoadWbp(TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile.WBP_FF_PrepBuildTile_C"));
	if (!TestNotNull(TEXT("Isolated editor world"), World) || !TestNotNull(TEXT("Saved facility catalog WBP"), PanelClass) ||
		!TestNotNull(TEXT("Saved compact codex launcher WBP"), LauncherClass) || !TestNotNull(TEXT("Saved facility tile WBP"), TileClass)) return false;
	FPlayerlessInitialization PanelContext(*PanelClass), LauncherContext(*LauncherClass), TileContext(*TileClass);
	TStrongObjectPtr<UFF_PrepPanelBuild> Panel(CreateWidget<UFF_PrepPanelBuild>(World, PanelClass));
	if (!TestNotNull(TEXT("Production catalog instance"), Panel.Get())) return false;
	TestTrue(TEXT("Fixture cannot access player saves or preferences"), Panel->GetGameInstance() == nullptr);
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	// Exercise the real open lifecycle. The playerless editor deliberately has no BuildMode;
	// its missing-owner close warning is expected, then only presentation state is supplied.
	AddExpectedMessage(TEXT("소유 바가 없어 패널을 닫을 수 없다"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1, false);
	Panel->HandlePanelOpened();
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	for (const FName Name : { TEXT("Panel_Catalog"), TEXT("Box_Header") })
	{
		if (UWidget* Widget = Panel->GetWidgetFromName(Name)) Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (UWidget* Compact = Panel->GetWidgetFromName(TEXT("Panel_PlacementCompact"))) Compact->SetVisibility(ESlateVisibility::Collapsed);

	auto* Header = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_CatalogHeader")));
	auto* Search = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("Search_Facilities")));
	auto* Gold = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Gold")));
	auto* Launcher = Cast<UFF_FacilityCodexLauncher>(Panel->GetWidgetFromName(TEXT("Widget_FacilityCodexCatalogLauncher")));
	auto* Ranking = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_RevenueRanking")));
	auto* Tiles = Cast<UFF_PrepBuildTileView>(Panel->GetWidgetFromName(TEXT("TileView_Facilities")));
	if (!TestNotNull(TEXT("Authored catalog header"), Header) || !TestNotNull(TEXT("Existing search field"), Search) ||
		!TestNotNull(TEXT("Existing gold balance"), Gold) || !TestNotNull(TEXT("Added catalog codex launcher"), Launcher) ||
		!TestNotNull(TEXT("Added revenue ranking action"), Ranking) ||
		!TestNotNull(TEXT("Actual facility tile view"), Tiles)) return false;
	TestTrue(TEXT("Additive entry is the saved compact launcher class"), Launcher->GetClass() == LauncherClass);
	TestTrue(TEXT("Search and launcher share the original header"), Search->GetParent() == Header && Launcher->GetParent() == Header);
	TestTrue(TEXT("Ranking shares the original catalog header"), Ranking->GetParent() == Header);
	TestEqual(TEXT("Revenue ranking immediately follows search"), Header->GetChildIndex(Ranking), Header->GetChildIndex(Search) + 1);
	TestEqual(TEXT("Codex follows the added revenue ranking action"), Header->GetChildIndex(Launcher), Header->GetChildIndex(Ranking) + 1);
	auto* Button = Cast<UButton>(Launcher->GetWidgetFromName(TEXT("Button_OpenCodex")));
	auto* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
	if (!TestNotNull(TEXT("Compact launcher button"), Button) || !TestNotNull(TEXT("Authored button label"), Label)) return false;
	TestTrue(TEXT("Catalog codex button is usable"), Button->GetIsEnabled() && Button->IsVisible());
	TestTrue(TEXT("Button label identifies the codex"), Label->GetText().ToString().Contains(TEXT("도감")));
	TestFalse(TEXT("Compact button label stays on one line"), Label->GetAutoWrapText());
	Gold->SetText(FText::FromString(TEXT("보유 7,500 Gold")));
	const UWidgetTree* AuthoredTree = PanelClass->GetWidgetTreeArchetype();
	if (auto* Status = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Status"))))
	{
		if (const auto* Authored = AuthoredTree ? Cast<UTextBlock>(AuthoredTree->FindWidget(TEXT("Text_Status"))) : nullptr)
			Status->SetText(Authored->GetText());
	}

	// Real source thumbnails make the surrounding palette visible; entries are transient view data.
	const TCHAR* Definitions[] = {
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_FoodStall_Burger.PDA_FFZS_Booth_FoodStall_Burger"),
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_FoodStall_IceCream.PDA_FFZS_Booth_FoodStall_IceCream"),
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Rest/PDA_FFZS_Rest_Seating_Seating.PDA_FFZS_Rest_Seating_Seating"),
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Rest/PDA_FFZS_Rest_Shade_Shade.PDA_FFZS_Rest_Shade_Shade") };
	TArray<UObject*> Items;
	TArray<UTexture*> Textures;
	for (const TCHAR* Path : Definitions)
	{
		const auto* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr, Path);
		if (!TestNotNull(TEXT("Existing facility source definition"), Definition)) return false;
		FFF_BuildCatalogEntry Entry;
		Entry.FacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
		Entry.DisplayName = Definition->Spec.DisplayName;
		Entry.Category = Definition->Spec.Category;
		Entry.InstallCost = 500;
		Entry.bHasResolvedInstallCost = true;
		Entry.Thumbnail = Definition->CatalogThumbnail;
		if (UTexture2D* Texture = Entry.Thumbnail.LoadSynchronous()) Textures.Add(Texture);
		auto* Item = NewObject<UFF_PrepBuildTileData>(Panel.Get());
		Item->Initialize(Entry, 7500);
		Items.Add(Item);
	}
	FTextureCompilingManager::Get().FinishCompilation(Textures);
	Tiles->SetListItems(Items);
	Tiles->SetIsEnabled(true);
	if (auto* Count = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_ItemCount")))) Count->SetText(FText::FromString(TEXT("시설 4 / 4")));
	const auto* HeightProperty = FindFProperty<FFloatProperty>(PanelClass, TEXT("CatalogHeight"));
	if (!TestNotNull(TEXT("Designer-owned catalog height"), HeightProperty)) return false;
	const float PanelHeight = HeightProperty->GetPropertyValue_InContainer(Panel.Get());

	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		for (const FIntPoint Resolution : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
		{
			const FVector2D Size(Resolution.X, Resolution.Y);
			const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
			auto Window = SNew(SVirtualWindow).Size(Size);
			Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)
				[ SNew(SOverlay) + SOverlay::Slot().VAlign(VAlign_Bottom).Padding(FMargin(12, 0, 12, 42))
					[ SNew(SBox).HeightOverride(PanelHeight)[Slate] ] ]);
			FWidgetRenderer Renderer(false, true);
			TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
			if (!TestNotNull(TEXT("Catalog render target"), Target.Get())) return false;
			FHittestGrid HitGrid;
			auto Paint = [&]()
			{
				for (int32 Frame = 0; Frame < 5; ++Frame)
				{
					Panel->ForceLayoutPrepass();
					Renderer.DrawWindow(Target.Get(), HitGrid, Window, 1.f, Size, .016f, false);
				}
			};
			auto CheckBounds = [&]()
			{
				const FSlateRect Viewport(0, 0, Resolution.X, Resolution.Y);
				const FSlateRect SearchRect = Search->GetPaintSpaceGeometry().GetLayoutBoundingRect();
				const FSlateRect ButtonRect = Button->GetPaintSpaceGeometry().GetLayoutBoundingRect();
				const FSlateRect GoldRect = Gold->GetPaintSpaceGeometry().GetLayoutBoundingRect();
				const FSlateRect LabelRect = Label->GetPaintSpaceGeometry().GetLayoutBoundingRect();
				TestTrue(TEXT("Search, codex button and gold stay inside the viewport"), Contains(Viewport, SearchRect) && Contains(Viewport, ButtonRect) && Contains(Viewport, GoldRect));
				TestTrue(TEXT("Search remains wide enough to type a facility name"), SearchRect.GetSize().X >= 100.f * Dpi);
				TestTrue(TEXT("Codex button remains a visible click target"), ButtonRect.GetSize().X >= 50.f * Dpi && ButtonRect.GetSize().Y >= 20.f * Dpi);
				TestFalse(TEXT("Codex button does not overlap search"), Overlaps(SearchRect, ButtonRect));
				TestFalse(TEXT("Codex button does not overlap gold"), Overlaps(ButtonRect, GoldRect));
				TestFalse(TEXT("Search does not overlap gold"), Overlaps(SearchRect, GoldRect));
				TestTrue(TEXT("Codex stays to the right of the search field"), ButtonRect.Left >= SearchRect.Right - 1.f);
				TestTrue(TEXT("Complete button label stays inside its button"), Contains(ButtonRect, LabelRect));
				const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label->GetText(), Label->GetFont());
				TestTrue(TEXT("Button label has enough unscaled space for all glyphs"), Label->GetCachedGeometry().GetLocalSize().X + 1.f >= TextSize.X && Label->GetCachedGeometry().GetLocalSize().Y + 1.f >= TextSize.Y);
			};
			Gold->SetText(FText::FromString(TEXT("보유 7,500 Gold")));
			Paint();
			CheckBounds();
			TArray<FColor> Pixels;
			FReadSurfaceDataFlags Flags;
			Flags.SetLinearToGamma(false);
			if (!TestTrue(TEXT("Catalog render pixels are readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) return false;
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityCodex");
			IFileManager::Get().MakeDirectory(*Directory, true);
			const FString Path = Directory / FString::Printf(TEXT("FacilityCodexCatalog%d.png"), Resolution.Y);
			TestTrue(TEXT("Catalog header screenshot saved"), FFileHelper::SaveArrayToFile(Png, *Path));
			AddInfo(FString::Printf(TEXT("Saved catalog header: %s (project DPI %.3f)"), *FPaths::ConvertRelativePathToFull(Path), Dpi));
			// A larger balance must shrink flexible search space without hiding the new button.
			Gold->SetText(FText::FromString(TEXT("보유 9,999,999 Gold")));
			Paint();
			CheckBounds();
		}
	}
	Panel->HandlePanelClosed();
	return true;
}

#endif
