// FF_FacilityCodexCatalogUiTests.cpp
// 역할: 시설 도감의 동작과 저장된 화면 연결을 자동 검증한다.

// Verifies the additive codex button in the saved production facility catalog.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
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

	bool IsWithin(const UWidget* Widget, const UWidget* Container)
	{
		for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
			if (Current == Container) return true;
		return false;
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
	auto* PanelClass = LoadWbp(FFBuildUIAssetPaths::BuildPanelClass);
	auto* LauncherClass = LoadWbp(FFBuildUIAssetPaths::FacilityCodexCatalogLauncherClass);
	auto* TileClass = LoadWbp(FFBuildUIAssetPaths::PrepBuildTileClass);
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
	for (const FName Name : { TEXT("Panel_Catalog") })
	{
		if (UWidget* Widget = Panel->GetWidgetFromName(Name)) Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (UWidget* Compact = Panel->GetWidgetFromName(TEXT("Panel_PlacementCompact"))) Compact->SetVisibility(ESlateVisibility::Collapsed);

	auto* CodexRow = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("Row_BuildCodexCategory")));
	auto* CategoryScroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("ScrollBox_Categories")));
	auto* CategorySize = Cast<USizeBox>(Panel->GetWidgetFromName(TEXT("Size_CategoryRow")));
	auto* Search = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("Search_Facilities")));
	auto* Launcher = Cast<UFF_FacilityCodexLauncher>(Panel->GetWidgetFromName(TEXT("Widget_FacilityCodexCatalogLauncher")));
	auto* Ranking = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_RevenueRanking")));
	auto* SubcategoryHeader = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_SubcategoryHeader")));
	auto* SearchBounds = Cast<USizeBox>(Panel->GetWidgetFromName(TEXT("Size_PearlSearch")));
	auto* RankingBounds = Cast<USizeBox>(Panel->GetWidgetFromName(TEXT("Size_PearlRevenueRanking")));
	auto* Tiles = Cast<UFF_PrepBuildTileView>(Panel->GetWidgetFromName(TEXT("TileView_Facilities")));
	if (!TestNotNull(TEXT("Pinned bottom codex tab row"), CodexRow) ||
		!TestNotNull(TEXT("Category filters retain their scrollbox"), CategoryScroll) ||
		!TestNotNull(TEXT("Category row retains its designer sizebox"), CategorySize) ||
		!TestNotNull(TEXT("Existing search field"), Search) ||
		!TestNotNull(TEXT("Added catalog codex launcher"), Launcher) ||
		!TestNotNull(TEXT("Added revenue ranking action"), Ranking) ||
		!TestNotNull(TEXT("Pearl search and ranking row"), SubcategoryHeader) ||
		!TestNotNull(TEXT("Search bounds"), SearchBounds) ||
		!TestNotNull(TEXT("Ranking bounds"), RankingBounds) ||
		!TestNotNull(TEXT("Actual facility tile view"), Tiles)) return false;
	const auto* DesignerLayout = FindFProperty<FBoolProperty>(PanelClass, TEXT("bUseDesignerCatalogLayout"));
	TestTrue(TEXT("Catalog geometry and action styling use the saved Designer layout"),
		DesignerLayout && DesignerLayout->GetPropertyValue_InContainer(Panel.Get()));
	for (const TCHAR* Retired : {TEXT("Box_Header"), TEXT("HorizontalBox_CatalogHeader"), TEXT("Text_Gold"),
		TEXT("Border_AuroraSelection"), TEXT("Border_CatalogFooter"), TEXT("HorizontalBox_Tools"),
		TEXT("Size_CategoryDivider"), TEXT("Panel_PlacementCompact"), TEXT("Text_CategoryBreadcrumb")})
		TestNull(*FString::Printf(TEXT("Retired catalog presentation is removed: %s"), Retired), Panel->GetWidgetFromName(Retired));
	TestTrue(TEXT("Additive entry is the saved compact launcher class"), Launcher->GetClass() == LauncherClass);
	TestTrue(TEXT("Search, ranking and codex share the action row below the category filters"),
		Search->GetParent() == SearchBounds && SearchBounds->GetParent() == SubcategoryHeader &&
		// The revenue introduction badge adds a noninteractive overlay inside its bounds.
		IsWithin(Ranking, RankingBounds) && RankingBounds->GetParent() == SubcategoryHeader &&
		Launcher->GetParent() == CodexRow &&
		CategoryScroll->GetParent() == Panel->GetWidgetFromName(TEXT("Size_BuildCodexCategoryViewport")) &&
		CodexRow->GetParent() == SubcategoryHeader &&
		Panel->GetWidgetFromName(TEXT("Size_BuildCodexCategoryViewport"))->GetParent() == CategorySize);
	TestEqual(TEXT("Revenue ranking immediately follows search"),
		SubcategoryHeader->GetChildIndex(RankingBounds), SubcategoryHeader->GetChildIndex(SearchBounds) + 1);
	auto* Button = Cast<UButton>(Launcher->GetWidgetFromName(TEXT("Button_OpenCodex")));
	auto* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
	if (!TestNotNull(TEXT("Compact launcher button"), Button) || !TestNotNull(TEXT("Authored button label"), Label)) return false;
	TestTrue(TEXT("Catalog codex button is usable"), Button->GetIsEnabled() && Button->IsVisible());
	TestTrue(TEXT("Pinned button identifies the combination codex"),
		Label->GetText().ToString().Contains(TEXT("조합")) && Label->GetText().ToString().Contains(TEXT("도감")));
	TestFalse(TEXT("Compact button label stays on one line"), Label->GetAutoWrapText());
	const UWidgetTree* AuthoredTree = PanelClass->GetWidgetTreeArchetype();
	if (auto* Status = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Status"))))
	{
		if (const auto* Authored = AuthoredTree ? Cast<UTextBlock>(AuthoredTree->FindWidget(TEXT("Text_Status"))) : nullptr)
			Status->SetText(Authored->GetText());
	}

	// Real source thumbnails make the surrounding palette visible; entries are transient view data.
	const TCHAR* Definitions[] = {
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_FoodStall_Burger.PDA_FFZS_Booth_FoodStall_Burger"),
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_FoodStall_IceCream.PDA_FFZS_Booth_FoodStall_IceCream"),
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Rest/PDA_FFZS_Rest_Seating_Seating.PDA_FFZS_Rest_Seating_Seating"),
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Rest/PDA_FFZS_Rest_Shade_Shade.PDA_FFZS_Rest_Shade_Shade") };
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
				const FSlateRect RankingRect = Ranking->GetPaintSpaceGeometry().GetLayoutBoundingRect();
				const FSlateRect LabelRect = Label->GetPaintSpaceGeometry().GetLayoutBoundingRect();
				TestTrue(TEXT("Search, codex and ranking actions stay inside the viewport"),
					Contains(Viewport, SearchRect) && Contains(Viewport, ButtonRect) && Contains(Viewport, RankingRect) &&
					RankingRect.GetSize().X >= 60.f * Dpi && RankingRect.GetSize().Y >= 20.f * Dpi);
				TestTrue(TEXT("Search remains wide enough to type a facility name"), SearchRect.GetSize().X >= 100.f * Dpi);
				TestTrue(TEXT("Codex button remains a visible click target"), ButtonRect.GetSize().X >= 50.f * Dpi && ButtonRect.GetSize().Y >= 20.f * Dpi);
				TestFalse(TEXT("Codex button does not overlap search"), Overlaps(SearchRect, ButtonRect));
				TestFalse(TEXT("Codex button does not overlap ranking"), Overlaps(ButtonRect, RankingRect));
				TestFalse(TEXT("Search does not overlap ranking"), Overlaps(SearchRect, RankingRect));
				TestTrue(TEXT("Codex action follows ranking on the search row"),
					ButtonRect.Left >= RankingRect.Right - 1.f &&
					FMath::IsNearlyEqual(ButtonRect.GetCenter().Y, SearchRect.GetCenter().Y, 1.f));
				TestTrue(TEXT("Codex action remains inside its authored action row"),
					Contains(SubcategoryHeader->GetPaintSpaceGeometry().GetLayoutBoundingRect(), ButtonRect));
				TestTrue(TEXT("Complete button label stays inside its button"), Contains(ButtonRect, LabelRect));
				for (UTextBlock* ActionLabel : { Label, Cast<UTextBlock>(Ranking->GetContent()) })
				{
					if (!TestNotNull(TEXT("Header action retains its readable text label"), ActionLabel)) continue;
					const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(ActionLabel->GetText(), ActionLabel->GetFont());
					const FVector2D Allotted = ActionLabel->GetCachedGeometry().GetLocalSize();
					TestTrue(*FString::Printf(TEXT("%s label has space for every glyph: allotted %.2fx%.2f, measured %.2fx%.2f"),
						*ActionLabel->GetName(), Allotted.X, Allotted.Y, TextSize.X, TextSize.Y),
						Allotted.X + 1.f >= TextSize.X && Allotted.Y + 1.f >= TextSize.Y);
				}
			};
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
			// Repeated paints must preserve both adjacent actions and their labels.
			Paint();
			CheckBounds();
		}
	}
	Panel->HandlePanelClosed();
	return true;
}

#endif
