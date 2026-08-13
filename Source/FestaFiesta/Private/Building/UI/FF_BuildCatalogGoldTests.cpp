// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/UI/FF_BuildCatalogScreen.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Components/HorizontalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_FestivalSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
	// Keep this helper name unique because Unity builds merge anonymous namespaces from .cpp files.
	class FScopedBuildCatalogPlayerlessWidgetInitialization
	{
	public:
		explicit FScopedBuildCatalogPlayerlessWidgetInitialization(UWidgetBlueprintGeneratedClass& InClass)
			: WidgetClass(InClass)
			, bPreviousValue(InClass.bCanCallInitializedWithoutPlayerContext)
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = true;
		}

		~FScopedBuildCatalogPlayerlessWidgetInitialization()
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = bPreviousValue;
		}

	private:
		UWidgetBlueprintGeneratedClass& WidgetClass;
		bool bPreviousValue;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogCurrentGoldTest,
	"FestaFiesta.Building.UI.CurrentGoldTracksProgressionLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogCurrentGoldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	UFF_BuildCatalogEconomyReadModel* EconomyReadModel = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogEconomyReadModel>()
		: nullptr;
	TestNotNull(TEXT("Game test world provides FestivalProgression"), Progression);
	TestNotNull(TEXT("Game test world provides the Building economy Read Model"), EconomyReadModel);
	if (!World || !GameInstance || !Progression || !EconomyReadModel)
	{
		return false;
	}

	const UFF_FestivalSettings* Settings = UFF_FestivalSettings::Get();
	const UFF_FestivalDefinition* SourceDefinition = Settings
		? Settings->DefaultFestival.LoadSynchronous()
		: nullptr;
	UFF_FestivalDefinition* TestDefinition = SourceDefinition
		? DuplicateObject<UFF_FestivalDefinition>(SourceDefinition, GameInstance)
		: nullptr;
	TestNotNull(TEXT("The production Festival definition can seed an isolated economy"), TestDefinition);
	if (!TestDefinition)
	{
		return false;
	}

	TestDefinition->FestivalId = TEXT("BuildCatalogGoldAutomationFestival");
	TestDefinition->InitialGold = 12'345;
	Progression->StartFestival(TestDefinition);
	TestEqual(TEXT("Building Read Model synchronously mirrors Progression initial Gold"),
		EconomyReadModel->GetGold(), 12'345);

	const TCHAR* ScreenClassPath =
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCatalogScreen.WBP_FF_BuildCatalogScreen_C");
	UWidgetBlueprintGeneratedClass* ScreenClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, ScreenClassPath));
	TestNotNull(TEXT("Build catalog WBP generated class loads"), ScreenClass);
	if (!ScreenClass)
	{
		return false;
	}

	// 기존 팀 WBP에는 기능 노드를 저장하지 않습니다. 인스턴스에만 추가되는지와 패키지 dirty
	// 상태가 바뀌지 않는지를 함께 고정해 런타임 확장이 에셋을 오염시키는 회귀를 막습니다.
	const UWidgetTree* ArchetypeTree = ScreenClass->GetWidgetTreeArchetype();
	UHorizontalBox* ArchetypeTopBar = ArchetypeTree
		? Cast<UHorizontalBox>(ArchetypeTree->FindWidget(TEXT("Top_Bar")))
		: nullptr;
	TestNotNull(TEXT("Production catalog WBP exposes its existing Top_Bar archetype"), ArchetypeTopBar);
	TestNull(TEXT("Production catalog WBP is not modified with a persisted Gold panel"),
		ArchetypeTree ? ArchetypeTree->FindWidget(TEXT("Gold_Size")) : nullptr);
	TestNull(TEXT("Production catalog WBP is not modified with a persisted Gold text"),
		ArchetypeTree ? ArchetypeTree->FindWidget(TEXT("TXT_Gold")) : nullptr);
	const int32 ArchetypeTopBarChildCount = ArchetypeTopBar
		? ArchetypeTopBar->GetChildrenCount()
		: INDEX_NONE;
	const bool bPackageWasDirty = ScreenClass->GetOutermost()->IsDirty();

	// Automation Game worlds do not create a LocalPlayer. This scoped override is restored before
	// the class can be reused by another test or PIE session.
	FScopedBuildCatalogPlayerlessWidgetInitialization PlayerlessInitializationGuard(*ScreenClass);
	UFF_BuildCatalogScreen* Screen = CreateWidget<UFF_BuildCatalogScreen>(World, ScreenClass);
	TestNotNull(TEXT("Build catalog screen can be created in the isolated Game world"), Screen);
	if (!Screen)
	{
		return false;
	}

	TSharedPtr<SWidget> ScreenSlateWidget = Screen->TakeWidget();
	TestNotNull(TEXT("Runtime screen injects Gold_Size into its own WidgetTree"),
		Screen->GetWidgetFromName(TEXT("Gold_Size")));
	const FText InitialExpected = FText::Format(
		FText::FromString(TEXT("{0} Gold")), FText::AsNumber(12'345));
	TestTrue(TEXT("Top-right Gold display uses the actual current balance"),
		Screen->GetDisplayedGoldText().EqualTo(InitialExpected));
	TestTrue(TEXT("Screen subscribes to the Building Read Model through its Gold handler"),
		EconomyReadModel->OnGoldChanged.IsAlreadyBound(
			Screen, &UFF_BuildCatalogScreen::HandleGoldChanged));

	TestTrue(TEXT("A production Gold spend succeeds"), Progression->TrySpend(345));
	TestEqual(TEXT("Building Read Model receives the Progression Gold change"),
		EconomyReadModel->GetGold(), 12'000);
	const FText SpentExpected = FText::Format(
		FText::FromString(TEXT("{0} Gold")), FText::AsNumber(12'000));
	TestTrue(TEXT("Open catalog updates its Gold text synchronously after spending"),
		Screen->GetDisplayedGoldText().EqualTo(SpentExpected));

	ScreenSlateWidget.Reset();
	TestFalse(TEXT("NativeDestruct removes the screen's Read Model subscription"),
		EconomyReadModel->OnGoldChanged.IsAlreadyBound(
			Screen, &UFF_BuildCatalogScreen::HandleGoldChanged));
	TestEqual(TEXT("Runtime extension preserves the WBP Top_Bar archetype child count"),
		ArchetypeTopBar ? ArchetypeTopBar->GetChildrenCount() : INDEX_NONE,
		ArchetypeTopBarChildCount);
	TestNull(TEXT("Runtime extension never adds Gold_Size to the WBP archetype"),
		ArchetypeTree ? ArchetypeTree->FindWidget(TEXT("Gold_Size")) : nullptr);
	TestEqual(TEXT("Runtime extension preserves the production WBP package dirty state"),
		ScreenClass->GetOutermost()->IsDirty(), bPackageWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
