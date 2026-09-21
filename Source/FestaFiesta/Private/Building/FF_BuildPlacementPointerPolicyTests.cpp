#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FF_BuildPlacementPointerPolicy.h"

#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "Building/UI/FF_BuildCatalogScreen.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Rendering/DrawElements.h"
#include "Slate/SObjectWidget.h"
#include "Types/PaintArgs.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace FFBuildPlacementPointerTest
{
	FWidgetPath PaintAndHit(const TSharedRef<SWidget>& Content)
	{
		const FVector2D Size(512.0, 512.0);
		const TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(Size);
		Window->SetContent(Content);
		Window->SlatePrepass(1.0f);
		FHittestGrid HitGrid;
		HitGrid.SetHittestArea(FVector2D::ZeroVector, Size);
		FSlateWindowElementList Elements(Window);
		const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform(1.0f));
		const FPaintArgs Args(nullptr, HitGrid, FVector2D::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
		Window->Paint(Args, Geometry, Geometry.GetLayoutBoundingRect(), Elements, 0, FWidgetStyle(), true);
		TArray<FWidgetAndPointer> HitPath = HitGrid.GetBubblePath(Size * 0.5, 0.0f, false);
		return FWidgetPath(MakeArrayView(HitPath));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildPlacementPointerPolicyTest,
	"FestaFiesta.Building.BuildMode.DragPlacement.PointerUiPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementPointerPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFBuildPlacementPointer;
	using namespace FFBuildPlacementPointerTest;
	if (!TestTrue(TEXT("Slate is available for the real hit-test fixture"), FSlateApplication::IsInitialized()))
	{
		return false;
	}
	TestFalse(TEXT("An absent path does not veto a controller-owned pointer"), IsBlockedByUiPath(FWidgetPath()));

	// A passive UUserWidget still appears as SObjectWidget on its child's hit path.
	// No gameplay UI is opened and no native construction callbacks are invoked.
	TStrongObjectPtr<UFF_BuildPlacementFailureFeedback> Passive(NewObject<UFF_BuildPlacementFailureFeedback>());
	const TSharedRef<SObjectWidget> PassiveWrapper = SNew(SObjectWidget, Passive.Get())
		[SNew(SOverlay) + SOverlay::Slot()[SNew(SBorder)]];
	const FWidgetPath PassivePath = PaintAndHit(PassiveWrapper);
	TestTrue(TEXT("Passive full-screen HUD content has an actual painted hit path"), PassivePath.IsValid());
	TestTrue(TEXT("Passive UMG wrapper participates in the path"), PassivePath.ContainsWidget(&PassiveWrapper.Get()));
	TestFalse(TEXT("Passive UMG, overlay and border wrappers allow held placement"), IsBlockedByUiPath(PassivePath));

	const TSharedRef<SButton> Button = SNew(SButton)[SNew(STextBlock).Text(FText::FromString(TEXT("HUD button")))];
	const FWidgetPath ButtonPath = PaintAndHit(SNew(SOverlay) + SOverlay::Slot()[Button]);
	TestTrue(TEXT("The painted button is present even when its label is the hit leaf"),
		ButtonPath.IsValid() && ButtonPath.ContainsWidget(&Button.Get()));
	TestTrue(TEXT("A hovered HUD button blocks held placement through its full path"), IsBlockedByUiPath(ButtonPath));

	UClass* PanelClass = LoadClass<UFF_PrepPanelBase>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild.WBP_FF_PrepPanelBuild_C"));
	if (!TestNotNull(TEXT("The production Prep panel class exists"), PanelClass)) { return false; }
	TStrongObjectPtr<UFF_PrepPanelBase> Panel(NewObject<UFF_PrepPanelBase>(GetTransientPackage(), PanelClass));
	const TSharedRef<SObjectWidget> PanelWrapper = SNew(SObjectWidget, Panel.Get())[SNew(SBorder)];
	const FWidgetPath PanelPath = PaintAndHit(PanelWrapper);
	TestTrue(TEXT("An actual Prep panel's empty surface participates in the painted path"),
		PanelPath.IsValid() && PanelPath.ContainsWidget(&PanelWrapper.Get()));
	TestTrue(TEXT("The Prep panel's custom pointer handler blocks placement over its empty surface"),
		IsBlockedByUiPath(PanelPath));

	TStrongObjectPtr<UFF_BuildCatalogScreen> Catalog(NewObject<UFF_BuildCatalogScreen>());
	const TSharedRef<SObjectWidget> CatalogWrapper = SNew(SObjectWidget, Catalog.Get())[SNew(SBorder)];
	const FWidgetPath CatalogPath = PaintAndHit(CatalogWrapper);
	TestTrue(TEXT("A standalone catalog's empty surface participates in the painted path"),
		CatalogPath.IsValid() && CatalogPath.ContainsWidget(&CatalogWrapper.Get()));
	TestTrue(TEXT("The standalone catalog blocks world inspection and placement over its empty surface"),
		IsBlockedByUiPath(CatalogPath));
	return true;
}

#endif
