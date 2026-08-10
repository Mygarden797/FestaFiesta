// FF_BuildCatalogThumbnailTests.cpp

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetBundleData.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/UI/FF_BuildFacilityCard.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace
{
	UTexture2D* MakeTransientThumbnail(const TCHAR* Name)
	{
		return UTexture2D::CreateTransient(
			1,
			1,
			PF_B8G8R8A8,
			FName(Name));
	}

	TSoftObjectPtr<UTexture2D>* FindDefaultThumbnailValue(
		FAutomationTestBase& Test,
		UFF_BuildFacilityCard& Card)
	{
		FProperty* DefaultThumbnailProperty = FindFProperty<FProperty>(
			UFF_BuildFacilityCard::StaticClass(),
			TEXT("DefaultThumbnail"));
		Test.TestNotNull(
			TEXT("Facility card exposes its default thumbnail setting"),
			DefaultThumbnailProperty);
		if (!DefaultThumbnailProperty)
		{
			return nullptr;
		}

		TSoftObjectPtr<UTexture2D>* DefaultValue =
			DefaultThumbnailProperty->ContainerPtrToValuePtr<TSoftObjectPtr<UTexture2D>>(&Card);
		Test.TestNotNull(
			TEXT("The facility card default thumbnail value is addressable"),
			DefaultValue);
		return DefaultValue;
	}

	bool BindThumbnailTestWidgets(
		FAutomationTestBase& Test,
		UFF_BuildFacilityCard& Card,
		UImage& ThumbnailImage,
		UTexture2D& DefaultThumbnail)
	{
		FObjectPropertyBase* ImageProperty = FindFProperty<FObjectPropertyBase>(
			UFF_BuildFacilityCard::StaticClass(),
			TEXT("Image_Thumbnail"));

		Test.TestNotNull(TEXT("Facility card exposes its thumbnail image binding"), ImageProperty);
		if (!ImageProperty)
		{
			return false;
		}

		ImageProperty->SetObjectPropertyValue_InContainer(&Card, &ThumbnailImage);
		TSoftObjectPtr<UTexture2D>* DefaultValue = FindDefaultThumbnailValue(Test, Card);
		if (!DefaultValue)
		{
			return false;
		}

		*DefaultValue = TSoftObjectPtr<UTexture2D>(&DefaultThumbnail);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogThumbnailDefinitionContractTest,
	"FestaFiesta.Building.Catalog.ThumbnailDefinitionContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogThumbnailDefinitionContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSoftObjectProperty* CatalogThumbnailProperty = FindFProperty<FSoftObjectProperty>(
		UFFZSFacilityDefinition::StaticClass(),
		TEXT("CatalogThumbnail"));
	TestNotNull(
		TEXT("Facility definitions expose CatalogThumbnail as a soft object reference"),
		CatalogThumbnailProperty);
	if (!CatalogThumbnailProperty)
	{
		return false;
	}

	TestEqual(
		TEXT("CatalogThumbnail accepts Texture2D assets"),
		CatalogThumbnailProperty->PropertyClass.Get(),
		UTexture2D::StaticClass());
	TestEqual(
		TEXT("CatalogThumbnail belongs directly to the facility definition asset"),
		CatalogThumbnailProperty->GetOwnerClass(),
		UFFZSFacilityDefinition::StaticClass());
	TestTrue(
		TEXT("CatalogThumbnail is editable in facility definition assets"),
		CatalogThumbnailProperty->HasAnyPropertyFlags(CPF_Edit));
	TestTrue(
		TEXT("CatalogThumbnail is readable from Blueprint"),
		CatalogThumbnailProperty->HasAllPropertyFlags(
			CPF_BlueprintVisible | CPF_BlueprintReadOnly));
	TestFalse(
		TEXT("CatalogThumbnail is presentation metadata, not SaveGame state"),
		CatalogThumbnailProperty->HasAnyPropertyFlags(CPF_SaveGame));
	// Property metadata scanning is an editor-only Asset Manager facility. Runtime builds verify the reflected
	// field contract above, while editor automation additionally proves the named bundle contains the soft path.
#if WITH_EDITOR
	TestEqual(
		TEXT("CatalogThumbnail participates in the build-catalog UI asset bundle"),
		CatalogThumbnailProperty->GetMetaData(TEXT("AssetBundles")),
		FString(TEXT("BuildCatalogUI")));

	UFFZSFacilityDefinition* Definition = NewObject<UFFZSFacilityDefinition>();
	UTexture2D* BundledThumbnail = MakeTransientThumbnail(TEXT("CatalogBundledThumbnail"));
	TestNotNull(TEXT("A transient facility definition can be created"), Definition);
	TestNotNull(TEXT("A transient bundled thumbnail can be created"), BundledThumbnail);
	if (Definition && BundledThumbnail)
	{
		Definition->CatalogThumbnail = TSoftObjectPtr<UTexture2D>(BundledThumbnail);

		FAssetBundleData BundleData;
		UAssetManager::Get().InitializeAssetBundlesFromMetadata(Definition, BundleData);
		FAssetBundleEntry* BundleEntry = BundleData.FindEntry(TEXT("BuildCatalogUI"));
		TestNotNull(
			TEXT("Asset Manager metadata scanning emits the build-catalog UI bundle"),
			BundleEntry);
		if (BundleEntry)
		{
			const FTopLevelAssetPath ThumbnailAssetPath =
				Definition->CatalogThumbnail.ToSoftObjectPath().GetAssetPath();
			TestTrue(
				TEXT("The generated bundle contains the configured catalog thumbnail"),
				BundleEntry->AssetPaths.Contains(ThumbnailAssetPath));
		}
	}
#endif

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogThumbnailSessionLifetimeTest,
	"FestaFiesta.Building.Catalog.ThumbnailSessionLifetime",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogThumbnailSessionLifetimeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_BuildCatalogSubsystem* Catalog = NewObject<UFF_BuildCatalogSubsystem>();
	TestNotNull(TEXT("A transient build catalog subsystem can be created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	TestEqual(
		TEXT("No thumbnail bundle is requested before a build session"),
		Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);

	Catalog->AcquireThumbnailPreload();
	TestEqual(
		TEXT("A session waits for the definition catalog before preloading thumbnails"),
		Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog);

	Catalog->AcquireThumbnailPreload();
	Catalog->ReleaseThumbnailPreload();
	TestEqual(
		TEXT("Releasing one of two consumers keeps the thumbnail session alive"),
		Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog);

	Catalog->ReleaseThumbnailPreload();
	TestEqual(
		TEXT("Releasing the final consumer returns the thumbnail session to idle"),
		Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildFacilityCardThumbnailSelectionTest,
	"FestaFiesta.Building.UI.FacilityCardThumbnailSelection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFacilityCardThumbnailSelectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_BuildFacilityCard* Card = NewObject<UFF_BuildFacilityCard>();
	UImage* ThumbnailImage = NewObject<UImage>(Card);
	UTexture2D* ExplicitThumbnail = MakeTransientThumbnail(TEXT("CatalogExplicitThumbnail"));
	UTexture2D* DefaultThumbnail = MakeTransientThumbnail(TEXT("CatalogDefaultThumbnail"));
	const TSoftObjectPtr<UTexture2D> UnresolvedThumbnail(FSoftObjectPath(TEXT(
		"/Game/FestaFiesta/Automation/UnresolvedCatalogThumbnail.UnresolvedCatalogThumbnail")));

	TestNotNull(TEXT("A native facility card can be created for the unit test"), Card);
	TestNotNull(TEXT("A thumbnail image can be created for the unit test"), ThumbnailImage);
	TestNotNull(TEXT("The explicit transient thumbnail can be created"), ExplicitThumbnail);
	TestNotNull(TEXT("The default transient thumbnail can be created"), DefaultThumbnail);
	if (!Card || !ThumbnailImage || !ExplicitThumbnail || !DefaultThumbnail)
	{
		return false;
	}

	if (!BindThumbnailTestWidgets(*this, *Card, *ThumbnailImage, *DefaultThumbnail))
	{
		return false;
	}

	TestEqual(
		TEXT("A loaded entry-specific thumbnail wins over a loaded fallback"),
		UFF_BuildFacilityCard::ResolveLoadedThumbnail(
			TSoftObjectPtr<UTexture2D>(ExplicitThumbnail),
			TSoftObjectPtr<UTexture2D>(DefaultThumbnail)),
		ExplicitThumbnail);
	TestEqual(
		TEXT("An unresolved entry-specific thumbnail uses the loaded fallback without loading"),
		UFF_BuildFacilityCard::ResolveLoadedThumbnail(
			UnresolvedThumbnail,
			TSoftObjectPtr<UTexture2D>(DefaultThumbnail)),
		DefaultThumbnail);
	TestNull(
		TEXT("No thumbnail is returned when neither reference is loaded"),
		UFF_BuildFacilityCard::ResolveLoadedThumbnail(
			UnresolvedThumbnail,
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT(
				"/Game/FestaFiesta/Automation/UnresolvedCatalogFallback.UnresolvedCatalogFallback")))));

	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Booth_FoodStall_ThumbnailContract");
	Entry.Thumbnail = TSoftObjectPtr<UTexture2D>(ExplicitThumbnail);
	Card->Configure(Entry);
	TestEqual(
		TEXT("An entry-specific thumbnail takes priority over the card default"),
		ThumbnailImage->GetBrush().GetResourceObject(),
		static_cast<UObject*>(ExplicitThumbnail));

	Entry.Thumbnail.Reset();
	Card->Configure(Entry);
	TestEqual(
		TEXT("A null entry thumbnail falls back to the card default"),
		ThumbnailImage->GetBrush().GetResourceObject(),
		static_cast<UObject*>(DefaultThumbnail));

	Entry.Thumbnail = UnresolvedThumbnail;
	Card->Configure(Entry);
	TestEqual(
		TEXT("A non-null but unloaded entry thumbnail still uses the loaded card default"),
		ThumbnailImage->GetBrush().GetResourceObject(),
		static_cast<UObject*>(DefaultThumbnail));

	TSoftObjectPtr<UTexture2D>* DefaultValue = FindDefaultThumbnailValue(*this, *Card);
	if (DefaultValue)
	{
		DefaultValue->Reset();
		Card->RefreshThumbnail();
		TestNull(
			TEXT("Refreshing with no loaded thumbnail clears the previously displayed brush"),
			ThumbnailImage->GetBrush().GetResourceObject());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
