// Checks Designer edits on a transient instance; never saves or modifies a WBP asset.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildDesignerAppearanceTest,
	"FestaFiesta.UI.PrepBuild.DesignerAppearanceSurvivesRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildDesignerAppearanceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepBuildTileWidget>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile.WBP_FF_PrepBuildTile_C")));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Actual Designer tile class"), Class) || !TestNotNull(TEXT("Editor world"), World)) { return false; }
	const bool Previous = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_PrepBuildTileWidget> Tile(CreateWidget<UFF_PrepBuildTileWidget>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = Previous;
	if (!TestNotNull(TEXT("Transient tile instance"), Tile.Get())) { return false; }
	auto* Size = Cast<USizeBox>(Tile->GetWidgetFromName(TEXT("SizeBox_Card")));
	auto* Card = Cast<UBorder>(Tile->GetWidgetFromName(TEXT("Border_Card")));
	auto* Name = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_Name")));
	auto* Cost = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_Cost")));
	auto* ThumbnailFrame = Cast<USizeBox>(Tile->GetWidgetFromName(TEXT("SizeBox_Thumbnail")));
	auto* Thumbnail = Cast<UImage>(Tile->GetWidgetFromName(TEXT("Image_Thumbnail")));
	const FObjectPropertyBase* ItemProperty = FindFProperty<FObjectPropertyBase>(Class, TEXT("Item"));
	if (!TestNotNull(TEXT("Designer card SizeBox"), Size) || !TestNotNull(TEXT("Designer card border"), Card) ||
		!TestNotNull(TEXT("Designer name"), Name) || !TestNotNull(TEXT("Designer cost"), Cost) ||
		!TestNotNull(TEXT("Transient view-model slot"), ItemProperty) ||
		!TestNotNull(TEXT("Designer thumbnail frame"), ThumbnailFrame) ||
		!TestNotNull(TEXT("Designer thumbnail image"), Thumbnail)) { return false; }
	// These are instance-only edits made before PreConstruct captures the authored appearance.
	Size->SetWidthOverride(196.f);
	Size->SetHeightOverride(188.f);
	FSlateBrush AuthoredBrush = Card->Background;
	AuthoredBrush.TintColor = FLinearColor(0.35f, 0.22f, 0.09f, 1.f);
	Card->SetBrush(AuthoredBrush);
	FSlateFontInfo Font = Name->GetFont();
	Font.Size = 19;
	Name->SetFont(Font);
	const FSlateColor AuthoredCost(FLinearColor(0.18f, 0.12f, 0.62f, 1.f));
	Cost->SetColorAndOpacity(AuthoredCost);
	ThumbnailFrame->SetWidthOverride(176.f);
	ThumbnailFrame->SetHeightOverride(88.f);
	TStrongObjectPtr<UTexture2D> WideTexture(UTexture2D::CreateTransient(400, 200));
	TStrongObjectPtr<UTexture2D> PortraitTexture(UTexture2D::CreateTransient(200, 400));
	if (!TestNotNull(TEXT("Wide texture fixture"), WideTexture.Get()) ||
		!TestNotNull(TEXT("Portrait texture fixture"), PortraitTexture.Get())) { return false; }
	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Booth_CraftExperience_DesignerFixture");
	Entry.DisplayName = FText::FromString(TEXT("디자이너 시설"));
	Entry.Category = EFFZSFacilityCategory::Booth;
	Entry.MinorToken = TEXT("CraftExperience");
	Entry.InstallCost = 300;
	Entry.bHasResolvedInstallCost = true;
	Entry.Thumbnail = WideTexture.Get();
	UFF_PrepBuildTileData* Data = NewObject<UFF_PrepBuildTileData>(Tile.Get());
	Data->Initialize(Entry, 1000, true);
	ItemProperty->SetObjectPropertyValue_InContainer(Tile.Get(), Data);
	const TSharedRef<SWidget> Slate = Tile->TakeWidget();
	for (int32 Pass = 0; Pass < 3; ++Pass) { Tile->RefreshItemPresentation(); }
	Slate->SlatePrepass(1.0f);
	TestTrue(TEXT("Native Slate construction preserves the Designer card dimensions"),
		Slate->GetDesiredSize().Equals(FVector2D(196, 188), 0.5));
	TestTrue(TEXT("Runtime refresh preserves the authored normal card brush"),
		Card->Background.TintColor == AuthoredBrush.TintColor);
	TestTrue(TEXT("Runtime refresh preserves the authored normal cost color"), Cost->GetColorAndOpacity() == AuthoredCost);
	TestEqual(TEXT("Runtime refresh preserves the authored name font size"), Name->GetFont().Size, Font.Size);
	TestEqual(TEXT("Designer styling still receives current item data"), Name->GetText().ToString(), Entry.DisplayName.ToString());
	TestTrue(TEXT("Runtime thumbnail fitting uses the Designer's enlarged 176 x 88 frame"),
		Thumbnail->GetDesiredSize().Equals(FVector2D(176, 88), 0.5));
	// Reusing a row for an image with another aspect must read the current WBP frame,
	// while leaving the frame's authoring values untouched.
	ThumbnailFrame->SetWidthOverride(160.f);
	ThumbnailFrame->SetHeightOverride(100.f);
	Entry.Thumbnail = PortraitTexture.Get();
	Data->Initialize(Entry, 1000, true);
	for (int32 Pass = 0; Pass < 3; ++Pass) { Tile->RefreshItemPresentation(); }
	Slate->SlatePrepass(1.0f);
	TestTrue(TEXT("Recycled portrait uses the edited frame and preserves texture aspect"),
		Thumbnail->GetDesiredSize().Equals(FVector2D(50, 100), 0.5));
	TestEqual(TEXT("Data refresh preserves the Designer's thumbnail width"), ThumbnailFrame->GetWidthOverride(), 160.f);
	TestEqual(TEXT("Data refresh preserves the Designer's thumbnail height"), ThumbnailFrame->GetHeightOverride(), 100.f);
	Data->RefreshGold(0, true);
	Tile->RefreshItemPresentation();
	TestFalse(TEXT("An unavailable item can show its distinct error state"), Cost->GetColorAndOpacity() == AuthoredCost);
	Data->RefreshGold(1000, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Returning to available restores the Designer color"), Cost->GetColorAndOpacity() == AuthoredCost);
	return true;
}

#endif
