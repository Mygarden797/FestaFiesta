// FF_BuildStudioWidgetTests.cpp
// 역할: 건설 카탈로그 화면의 필수 연결 위젯을 검사한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace FFBuildStudioWidgetTest
{
	void CollectCategoryChoices(UWidget* Root, TArray<UFF_PrepBuildCategoryChoice*>& Choices)
	{
		if (auto* Choice = Cast<UFF_PrepBuildCategoryChoice>(Root))
		{
			Choices.Add(Choice);
			return;
		}
		if (auto* Panel = Cast<UPanelWidget>(Root))
			for (UWidget* Child : Panel->GetAllChildren()) CollectCategoryChoices(Child, Choices);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildStudioWidgetBindings,
	"FestaFiesta.UI.PrepBuild.StudioBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildStudioWidgetBindings::RunTest(const FString& Parameters)
{
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelBuild>(nullptr,
		FFBuildUIAssetPaths::BuildPanelClass));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Actual studio WBP"), Class) || !TestNotNull(TEXT("Editor fixture world"), World)) { return false; }
	const bool PreviousInitialization = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_PrepPanelBuild> Panel(CreateWidget<UFF_PrepPanelBuild>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = PreviousInitialization;
	if (!TestNotNull(TEXT("Instantiated studio"), Panel.Get())) { return false; }
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	for (const FName Name : { FName(TEXT("Panel_Catalog")),
		FName(TEXT("TileView_Facilities")), FName(TEXT("HorizontalBox_Subcategories")),
		FName(TEXT("HorizontalBox_Categories")), FName(TEXT("Button_FavoritesOnly")),
		FName(TEXT("Text_FavoritesOnly")), FName(TEXT("Search_Facilities")) })
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
	auto* Categories = Cast<UHorizontalBox>(Panel->GetWidgetFromName(TEXT("HorizontalBox_Categories")));
	TArray<UFF_PrepBuildCategoryChoice*> Choices;
	FFBuildStudioWidgetTest::CollectCategoryChoices(Categories, Choices);
	TestTrue(TEXT("The authored category rail contains All and all ten facility categories"),
		Categories && Choices.Num() == 11);
	for (UFF_PrepBuildCategoryChoice* Choice : Choices)
	{
		TestTrue(TEXT("Every nested saved category retains its live selection action"), Choice->OnHudTabButtonClicked.IsBound());
		const auto* Label = Cast<UTextBlock>(Choice->GetWidgetFromName(TEXT("Text_Label")));
		TestTrue(TEXT("Major categories are icon-only while retaining a name tooltip"),
			Choice->bIconOnly && Label && Label->GetVisibility() == ESlateVisibility::Collapsed &&
			!Choice->GetToolTipText().IsEmpty());
	}
	if (MinorHost && MinorHost->GetChildrenCount() > 0)
	{
		const auto* Minor = Cast<UFF_PrepBuildCategoryChoice>(MinorHost->GetChildAt(0));
		const auto* Label = Minor ? Cast<UTextBlock>(Minor->GetWidgetFromName(TEXT("Text_Label"))) : nullptr;
		TestTrue(TEXT("Icon-only major settings do not hide dynamically populated minor labels"),
			Minor && !Minor->bIconOnly && Label && Label->IsVisible() && !Label->GetText().IsEmpty());
	}
	const auto* Favorites = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_FavoritesOnly")));
	TestTrue(TEXT("Favorites-only has a separate bound control"), Favorites && Favorites->OnClicked.IsBound());
	return true;
}
#endif
