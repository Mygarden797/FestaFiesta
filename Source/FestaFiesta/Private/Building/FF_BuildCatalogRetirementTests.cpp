// Retiring a catalog item must preserve its saved specification and asset identity.
#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetBundleData.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogRetiredDefinitionTest,
	"FestaFiesta.Building.Catalog.RetirementPreservesIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogRetiredDefinitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!TestNotNull(TEXT("The editor AssetManager is initialized"), AssetManager))
	{
		return false;
	}

	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UFF_BuildCatalogSubsystem> Catalog(
		NewObject<UFF_BuildCatalogSubsystem>(GameInstance.Get()));
	TStrongObjectPtr<UFFZSFacilityDefinition> Current(NewObject<UFFZSFacilityDefinition>());
	TStrongObjectPtr<UFFZSFacilityDefinition> Retired(NewObject<UFFZSFacilityDefinition>());
	Current->Spec = FFZSTest::MakeValidFacilitySpec();
	Current->Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_CurrentTest");
	Retired->Spec = Current->Spec;
	// An obsolete classification reproduces the historical catalog warning.
	Retired->Spec.FacilityDefinitionId = TEXT("Booth_FoodTruck_RetiredTest");
	const FFFZSFacilitySpec SavedSpec = Retired->Spec;
	const FPrimaryAssetId SavedId = Retired->GetPrimaryAssetId();
	TestTrue(TEXT("New definitions are visible without authoring changes"),
		Current->bShowInBuildCatalog && Retired->bShowInBuildCatalog);
	Retired->bShowInBuildCatalog = false;

	// A private dynamic type avoids changing the registered production definitions.
	const FPrimaryAssetType TestType(*FString::Printf(
		TEXT("FFCatalogRetirementTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	ON_SCOPE_EXIT { AssetManager->RemovePrimaryAssetType(TestType); };
	const FPrimaryAssetId CurrentId(TestType, TEXT("Current"));
	const FPrimaryAssetId RetiredId(TestType, TEXT("Retired"));
	const bool bCurrentRegistered = AssetManager->AddDynamicAsset(
		CurrentId, FSoftObjectPath(Current.Get()), FAssetBundleData());
	const bool bRetiredRegistered = AssetManager->AddDynamicAsset(
		RetiredId, FSoftObjectPath(Retired.Get()), FAssetBundleData());
	if (!TestTrue(TEXT("Transient definitions register for the test"),
		bCurrentRegistered && bRetiredRegistered))
	{
		return false;
	}

	Catalog->RequestedAssetIds = { RetiredId, CurrentId };
	Catalog->RebuildCatalogFromLoadedAssets();
	TestEqual(TEXT("Only the current item appears in the catalog"),
		Catalog->GetEntriesView().Num(), 1);
	FFF_BuildCatalogEntry Entry;
	TestTrue(TEXT("The default visible definition remains selectable"),
		Catalog->FindEntry(Current->Spec.FacilityDefinitionId, Entry));
	TestFalse(TEXT("The retired definition is absent from new selection"),
		Catalog->FindEntry(SavedSpec.FacilityDefinitionId, Entry));
	TestEqual(TEXT("Intentional retirement does not count as a failed asset"),
		Catalog->GetSkippedAssetCount(), 0);
	TestTrue(TEXT("Retirement leaves every saved spec field unchanged"),
		FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&SavedSpec, &Retired->Spec, 0));
	TestEqual(TEXT("The permanent Primary Asset ID is unchanged"),
		Retired->GetPrimaryAssetId(), SavedId);
	TestTrue(TEXT("The retired asset remains available through AssetManager"),
		AssetManager->GetPrimaryAssetObject(RetiredId) == Retired.Get());
	return true;
}

#endif
