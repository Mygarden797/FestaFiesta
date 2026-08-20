// FF_BuildCatalogGoldTests.cpp

// Festival 골드 변경이 카탈로그 읽기 모델과 화면 표시까지 전달되는 계약을 검증한다.
// 흐름: 테스트 GameInstance/진행 상태 구성 -> 카탈로그 화면 생성 -> 골드 변경 전후 텍스트 확인.
// 불변식: 저장된 WBP 유무와 무관하게 디자이너 경로와 네이티브 폴백은 같은 골드를 보여야 한다.
// 실패/수명: 자동화용 클래스 override와 Root 객체는 테스트 범위를 벗어나기 전에 원상 복구한다.

#include "Building/UI/FF_BuildCatalogScreen.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Components/HorizontalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_FestivalSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
	// Unity 빌드는 여러 cpp의 익명 네임스페이스를 합치므로 보조 함수 이름을 파일 고유하게 유지한다.
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

	// 이 테스트는 **Read Model 이 잔액을 그대로 비추는지**만 본다. StartFestival 이 Day1
	// 임대료를 징수하므로(economy_spec.md §5 S6) 임대료를 0 으로 두지 않으면 12,345 가 안 나온다.
	TestDefinition->DailyVenueRentGold = 0;

	Progression->StartFestival(TestDefinition);
	TestEqual(TEXT("Building Read Model synchronously mirrors Progression initial Gold"),
		EconomyReadModel->GetGold(), 12'345);

	const UFFZSFacilityDefinition* FoodTruckDefinition =
		LoadObject<UFFZSFacilityDefinition>(
			nullptr,
			TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/"
				"PDA_FFZS_Booth_FoodStall_DadaPochaFoodTruck."
				"PDA_FFZS_Booth_FoodStall_DadaPochaFoodTruck"));
	TestNotNull(TEXT("The production FoodTruck definition loads for catalog pricing"),
		FoodTruckDefinition);
	if (!FoodTruckDefinition)
	{
		return false;
	}

	FFF_BuildCatalogEntry PricedEntry;
	PricedEntry.FacilityDefinitionId = FoodTruckDefinition->Spec.FacilityDefinitionId;
	FString PricingFailureReason;
	TestTrue(TEXT("Building Read Model projects the active Festival's facility pricing"),
		EconomyReadModel->TryProjectFacilityCosts(
			PricedEntry, FoodTruckDefinition, PricingFailureReason));
	TestEqual(TEXT("Building Read Model projects the live FoodTruck install cost"),
		PricedEntry.InstallCost, 400);
	TestEqual(TEXT("Building Read Model projects the configured demolition recovery cost"),
		PricedEntry.RecoveryCost, 200);
	TestTrue(TEXT("A zero-capable recovery cost has an independent resolved flag"),
		PricedEntry.bHasResolvedRecoveryCost);

	TestDefinition->FacilityDemolitionRefundRate = 0.25f;
	TestTrue(TEXT("Reprojection reads the active Festival's latest recovery rate"),
		EconomyReadModel->TryProjectFacilityCosts(
			PricedEntry, FoodTruckDefinition, PricingFailureReason));
	TestEqual(TEXT("A changed 25 percent rate projects a 100 Gold recovery cost"),
		PricedEntry.RecoveryCost, 100);

	UDataTable* OriginalCostTable = TestDefinition->FacilityCostTable;
	TestDefinition->FacilityCostTable = nullptr;
	TestFalse(TEXT("Pricing fails closed when the active Festival has no cost table"),
		EconomyReadModel->TryProjectFacilityCosts(
			PricedEntry, FoodTruckDefinition, PricingFailureReason));
	TestFalse(TEXT("A failed reprojection clears the install-cost resolved flag"),
		PricedEntry.bHasResolvedInstallCost);
	TestFalse(TEXT("A failed reprojection clears the recovery-cost resolved flag"),
		PricedEntry.bHasResolvedRecoveryCost);
	TestDefinition->FacilityCostTable = OriginalCostTable;
	TestDefinition->FacilityDemolitionRefundRate = 0.5f;

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

	// 자동화 Game 월드는 LocalPlayer를 만들지 않는다. 다른 테스트나 PIE가 클래스를 재사용하기 전에
	// 이 범위 한정 override를 원래 값으로 복구한다.
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

	TestTrue(TEXT("A production Gold spend succeeds"), Progression->TrySpend(345, EFF_SpendReason::FacilityInstall));
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
