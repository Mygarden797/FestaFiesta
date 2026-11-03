// FF_BuildCatalogGoldTests.cpp

// Festival 골드 변경이 카탈로그 읽기 모델과 화면 표시까지 전달되는 계약을 검증한다.
// 흐름: 테스트 GameInstance/진행 상태 구성 -> 카탈로그 화면 생성 -> 골드 변경 전후 텍스트 확인.
// 불변식: 남아 있는 네이티브 안전 화면은 실제 골드를 표시하고 현재 PrepBuild 디자인은 바꾸지 않는다.
// 실패/수명: 자동화용 클래스 override와 Root 객체는 테스트 범위를 벗어나기 전에 원상 복구한다.

#include "Building/UI/FF_BuildCatalogScreen.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_FestivalSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"

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

	Progression->StartFestival(TestDefinition);
	TestEqual(TEXT("Building Read Model synchronously mirrors Progression initial Gold"),
		EconomyReadModel->GetGold(), 12'345);

	const UFFZSFacilityDefinition* FoodTruckDefinition =
		LoadObject<UFFZSFacilityDefinition>(
			nullptr,
			TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Booth/"
				"PDA_FFZS_Booth_FoodStall_Taco."
				"PDA_FFZS_Booth_FoodStall_Taco"));
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
	// 🔴 설치비를 리터럴로 박지 않는다. 예전에는 400·200 이 박혀 있어서
	// DT_FacilityCosts 의 FoodTruck 값이 400 → 700 으로 바뀌자(2026-08-24) 두 줄이 같이 깨졌다.
	// 이 테스트가 보려는 것은 금액 자체가 아니라 **Read Model 이 살아 있는 표를 그대로 비추는가**다.
	const int32 LiveInstallCost = PricedEntry.InstallCost;
	TestTrue(TEXT("Building Read Model projects a valid live FoodTruck install cost"),
		PricedEntry.HasValidInstallCost());
	TestEqual(TEXT("Building Read Model projects the configured demolition recovery cost"),
		PricedEntry.RecoveryCost, FMath::RoundToInt(LiveInstallCost * 0.5f));
	TestTrue(TEXT("A zero-capable recovery cost has an independent resolved flag"),
		PricedEntry.bHasResolvedRecoveryCost);

	TestDefinition->FacilityDemolitionRefundRate = 0.25f;
	TestTrue(TEXT("Reprojection reads the active Festival's latest recovery rate"),
		EconomyReadModel->TryProjectFacilityCosts(
			PricedEntry, FoodTruckDefinition, PricingFailureReason));
	TestEqual(TEXT("A changed 25 percent rate projects a quarter of the install cost"),
		PricedEntry.RecoveryCost, FMath::RoundToInt(LiveInstallCost * 0.25f));

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

	// The saved full-screen catalog is intentionally retired. Keep the native
	// fallback's economy/lifetime coverage without recreating its old gold header
	// in the production PrepBuild panel.
	UWidgetBlueprintGeneratedClass* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(
		LoadObject<UClass>(nullptr, FFBuildUIAssetPaths::BuildPanelClass));
	TestNotNull(TEXT("The current production PrepBuild WBP loads"), PanelClass);
	if (!PanelClass)
	{
		return false;
	}
	const UWidgetTree* ArchetypeTree = PanelClass->GetWidgetTreeArchetype();
	if (!TestNotNull(TEXT("The current production panel retains its authored tree"), ArchetypeTree)) return false;
	TArray<UWidget*> ArchetypeWidgets;
	ArchetypeTree->GetAllWidgets(ArchetypeWidgets);
	const int32 ArchetypeWidgetCount = ArchetypeWidgets.Num();
	const bool bPackageWasDirty = PanelClass->GetOutermost()->IsDirty();
	TestNull(TEXT("The retired full-screen Gold display is absent from the production panel"),
		ArchetypeTree->FindWidget(TEXT("TXT_Gold")));
	UFF_BuildCatalogScreen* Screen = CreateWidget<UFF_BuildCatalogScreen>(World, UFF_BuildCatalogScreen::StaticClass());
	TestNotNull(TEXT("Native fallback catalog can be created in the isolated Game world"), Screen);
	if (!Screen)
	{
		return false;
	}

	TSharedPtr<SWidget> ScreenSlateWidget = Screen->TakeWidget();
	TestFalse(TEXT("The compatibility catalog uses its native Slate fallback"), Screen->IsUsingDesignerWidgetTree());
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
	ArchetypeWidgets.Reset();
	ArchetypeTree->GetAllWidgets(ArchetypeWidgets);
	TestEqual(TEXT("Native fallback preserves the active panel's authored widget count"),
		ArchetypeWidgets.Num(), ArchetypeWidgetCount);
	TestNull(TEXT("Native fallback never adds its Gold panel to the production WBP"),
		ArchetypeTree->FindWidget(TEXT("Gold_Size")));
	TestEqual(TEXT("Native fallback preserves the production WBP package dirty state"),
		PanelClass->GetOutermost()->IsDirty(), bPackageWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
