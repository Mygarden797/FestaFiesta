#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildCompactLightingBadgeTest,
	"FestaFiesta.UI.PrepBuild.CompactLightingBadge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildCompactLightingBadgeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepBuildTileWidget>(nullptr, FFBuildUIAssetPaths::PrepBuildTileClass));
	if (!TestNotNull(TEXT("Saved compact facility tile"), Class) || !TestNotNull(TEXT("Editor fixture world"), World)) return false;
	const bool bPrevious = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_PrepBuildTileWidget> Tile(CreateWidget<UFF_PrepBuildTileWidget>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = bPrevious;
	if (!TestNotNull(TEXT("Actual transient facility card"), Tile.Get())) return false;
	Tile->bThumbnailOnly = true;
	UWidget* Badge = Tile->GetWidgetFromName(TEXT("Badge_Lighting"));
	UWidget* Required = Tile->GetWidgetFromName(TEXT("Badge_RequiredIcon"));
	UWidget* Frame = Tile->GetWidgetFromName(TEXT("SizeBox_Thumbnail"));
	auto* PrimaryIcon = Cast<UImage>(Tile->GetWidgetFromName(TEXT("Image_ApprovedLighting")));
	auto* FallbackIcon = Cast<UImage>(Tile->GetWidgetFromName(TEXT("Image_A5Lighting")));
	const auto* ItemProperty = FindFProperty<FObjectPropertyBase>(Class, TEXT("Item"));
	if (!TestNotNull(TEXT("Authored lighting badge"), Badge) || !TestNotNull(TEXT("Authored required star"), Required) ||
		!TestNotNull(TEXT("Actual thumbnail frame"), Frame) || !TestNotNull(TEXT("Pooled item slot"), ItemProperty)) return false;
	TStrongObjectPtr<UFF_PrepBuildTileData> Data(NewObject<UFF_PrepBuildTileData>(Tile.Get()));
	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Lighting_CompactFixture");
	Entry.DisplayName = FText::FromString(TEXT("조명 표시 검사"));
	Entry.bHasResolvedInstallCost = true;
	Entry.InstallCost = 300;
	Entry.bSupportsLighting = true;
	const TSharedRef<SWidget> Slate = Tile->TakeWidget();
	const FVector2D Size(110, 115);
	FWidgetRenderer Renderer(false, true);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
	if (!TestNotNull(TEXT("Badge review render target"), Target.Get())) return false;
	const auto RefreshAndPaint = [&]()
	{
		Data->Initialize(Entry, 1000, true);
		ItemProperty->SetObjectPropertyValue_InContainer(Tile.Get(), Data.Get());
		Tile->RefreshItemPresentation();
		for (int32 Pass = 0; Pass < 3; ++Pass) Renderer.DrawWidget(Target.Get(), Slate, Size, 0.f, false);
		FlushRenderingCommands();
	};
	RefreshAndPaint();
	TestEqual(TEXT("A lighting-capable thumbnail retains its icon without captions"), Badge->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("The badge reuses an existing lighting image resource"),
		(PrimaryIcon && PrimaryIcon->GetBrush().GetResourceObject()) || (FallbackIcon && FallbackIcon->GetBrush().GetResourceObject()));
	const FSlateRect FrameRect = Frame->GetCachedGeometry().GetLayoutBoundingRect();
	const FSlateRect LightingOnly = Badge->GetCachedGeometry().GetLayoutBoundingRect();
	TestTrue(TEXT("A lighting-only marker starts in the thumbnail's upper-left corner"),
		LightingOnly.GetSize().X > 0 && LightingOnly.Left >= FrameRect.Left - 1 && LightingOnly.Left <= FrameRect.Left + 8 &&
		LightingOnly.Top >= FrameRect.Top - 1 && LightingOnly.Top <= FrameRect.Top + 8 && LightingOnly.Bottom <= FrameRect.Bottom + 1);
	Entry.bRequiredForSetup = true;
	Entry.RequiredMinimumCount = 1;
	RefreshAndPaint();
	const FSlateRect RequiredRect = Required->GetCachedGeometry().GetLayoutBoundingRect();
	const FSlateRect LightingWithStar = Badge->GetCachedGeometry().GetLayoutBoundingRect();
	TestTrue(TEXT("Required and lighting icons remain visible together"), Required->IsVisible() && Badge->IsVisible());
	TestTrue(TEXT("Lighting sits beside the required star without overlap"),
		LightingWithStar.Left >= RequiredRect.Right - .5f && FMath::Abs(LightingWithStar.Top - RequiredRect.Top) <= 1.f &&
		LightingWithStar.Right <= FrameRect.Right + 1.f);
	TestTrue(TEXT("The required star keeps the first upper-left position"), RequiredRect.Left <= FrameRect.Left + 8.f);
	Entry.bSupportsLighting = false;
	RefreshAndPaint();
	TestEqual(TEXT("Rebinding a non-lighting facility clears the previous lighting icon"), Badge->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Removing lighting never removes the required marker"), Required->IsVisible());
	Entry.bSupportsLighting = true;
	RefreshAndPaint();
	IUserListEntry::ReleaseEntry(*Tile.Get());
	TestEqual(TEXT("Releasing a pooled row removes its lighting marker"), Badge->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Releasing a pooled row removes its required marker"), Required->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

#endif
