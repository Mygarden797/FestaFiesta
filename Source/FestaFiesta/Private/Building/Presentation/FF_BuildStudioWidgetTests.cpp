#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildStudioWidgetBindings,
	"FestaFiesta.UI.PrepBuild.StudioBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildStudioWidgetBindings::RunTest(const FString& Parameters)
{
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelBuild>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild.WBP_FF_PrepPanelBuild_C")));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Actual studio WBP"), Class) || !TestNotNull(TEXT("Editor fixture world"), World)) { return false; }
	const bool PreviousInitialization = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_PrepPanelBuild> Panel(CreateWidget<UFF_PrepPanelBuild>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = PreviousInitialization;
	if (!TestNotNull(TEXT("Instantiated studio"), Panel.Get())) { return false; }
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	for (const FName Name : { FName(TEXT("Panel_Catalog")), FName(TEXT("Panel_PlacementCompact")),
		FName(TEXT("TileView_Facilities")), FName(TEXT("HorizontalBox_Subcategories")),
		FName(TEXT("Button_InspectFacilities")), FName(TEXT("Text_CategoryBreadcrumb")),
		FName(TEXT("HorizontalBox_Categories")), FName(TEXT("Button_FavoritesOnly")),
		FName(TEXT("Text_FavoritesOnly")),
		FName(TEXT("Button_ReturnToCatalog")), FName(TEXT("Button_ExitBuildMode")),
		FName(TEXT("Text_PlacementGuide")) })
	{
		UWidget* Widget = Panel->GetWidgetFromName(Name);
		const FObjectPropertyBase* Binding = FindFProperty<FObjectPropertyBase>(Class, Name);
		TestNotNull(*FString::Printf(TEXT("Saved widget %s"), *Name.ToString()), Widget);
		if (TestNotNull(*FString::Printf(TEXT("Native binding %s"), *Name.ToString()), Binding))
		{
			TestEqual(*FString::Printf(TEXT("Live binding resolves %s"), *Name.ToString()),
				Binding->GetObjectPropertyValue_InContainer(Panel.Get()), static_cast<UObject*>(Widget));
		}
		if (Widget)
		{
			TestEqual(*FString::Printf(TEXT("%s is owned by the current tree"), *Name.ToString()),
				Widget->GetOuter(), static_cast<UObject*>(Panel->WidgetTree));
		}
	}
	const auto* MinorHost = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_Subcategories")));
	TestTrue(TEXT("The second-level filter is populated in the real widget"), MinorHost && MinorHost->GetChildrenCount() > 0);
	const auto* Categories = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_Categories")));
	TestTrue(TEXT("The authored horizontal row contains All and all ten facility categories"),
		Categories && Categories->GetChildrenCount() == 11);
	const auto* Favorites = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_FavoritesOnly")));
	TestTrue(TEXT("Favorites-only has a separate bound control"), Favorites && Favorites->OnClicked.IsBound());
	return true;
}
#endif
