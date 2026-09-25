#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Incident/FF_IncidentCatalog.h"
#include "Incident/FF_IncidentSettings.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_IncidentCatalogLoadTest,
	"FestaFiesta.Incident.Catalog.ConfiguredDefinitionsLoad",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentCatalogLoadTest::RunTest(const FString& Parameters)
{
	const UFF_IncidentSettings* Settings = UFF_IncidentSettings::Get();
	if (!TestNotNull(TEXT("Incident settings"), Settings))
	{
		return false;
	}

	const FString ExpectedPath = TEXT("/Game/FestaFiesta/Data/Incident/FF_IncidentCatalog.FF_IncidentCatalog");
	TestEqual(TEXT("Configured catalog path"), Settings->Catalog.ToSoftObjectPath().ToString(), ExpectedPath);
	if (!TestFalse(TEXT("Configured catalog reference is not null"), Settings->Catalog.IsNull()))
	{
		return false;
	}

	// Exercise the subsystem's real loading path. A soft path may be valid even when Get() is null.
	UFF_IncidentSubsystem* Subsystem = NewObject<UFF_IncidentSubsystem>(GetTransientPackage());
	Subsystem->LoadDefinitions();
	if (!TestNotNull(TEXT("Catalog loaded from the configured soft reference"), Subsystem->LoadedCatalog.Get()))
	{
		return false;
	}

	int32 EnabledDefinitions = 0;
	for (const FFF_IncidentCatalogEntry& Entry : Subsystem->LoadedCatalog->Entries)
	{
		if (!Entry.bEnabled)
		{
			continue;
		}

		++EnabledDefinitions;
		const UFF_IncidentDefinition* Definition = Entry.Definition.LoadSynchronous();
		if (TestNotNull(TEXT("Enabled incident definition loads"), Definition))
		{
			TestTrue(TEXT("Loaded definition registered by incident tag"),
				Subsystem->CachedDef.Contains(Definition->IncidentTag));
		}
	}

	TestTrue(TEXT("Catalog contains enabled definitions"), EnabledDefinitions > 0);
	TestEqual(TEXT("Every enabled definition registered"), Subsystem->CachedDef.Num(), EnabledDefinitions);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
