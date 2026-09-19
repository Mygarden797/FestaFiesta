// Project content contracts. Identity and prices come from the authored data;
// localized display names must never become persistent facility identifiers.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFFacilityCatalogActiveDataTest,
    "FestaFiesta.Data.FacilityDefinitions.ActiveCatalogIdentityAndPrices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFFacilityCatalogActiveDataTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UAssetManager& Assets = UAssetManager::Get();
    const UFF_FestivalSettings* Settings = UFF_FestivalSettings::Get();
    const UFF_FestivalDefinition* Festival = Settings ? Settings->DefaultFestival.LoadSynchronous() : nullptr;
    const UDataTable* Costs = Festival ? Festival->FacilityCostTable.Get() : nullptr;
    const bool bUsableCosts = Costs && Costs->GetRowStruct() == FFF_FacilityCostRow::StaticStruct();
    if (!Festival)
    {
        AddError(FString::Printf(TEXT("Default festival could not be loaded: %s"),
            Settings ? *Settings->DefaultFestival.ToSoftObjectPath().ToString() : TEXT("<missing settings>")));
    }
    else if (!bUsableCosts)
    {
        AddError(FString::Printf(TEXT("%s: FacilityCostTable must reference a %s table; found %s."),
            *Festival->GetPathName(), *FFF_FacilityCostRow::StaticStruct()->GetName(), *GetPathNameSafe(Costs)));
    }

    // Enumerate the registry by asset path, not the PrimaryAssetId map. Duplicate IDs can
    // hide a definition in that map, which is precisely one of the errors this test detects.
    FARFilter Filter;
    Filter.PackagePaths.Add(TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions"));
    Filter.ClassPaths.Add(UFFZSFacilityDefinition::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> Definitions;
    Assets.GetAssetRegistry().GetAssets(Filter, Definitions);
    Definitions.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
    });
    TestTrue(TEXT("Facility authoring root contains definition assets"), !Definitions.IsEmpty());

    TMap<FName, FString> FirstPathById;
    int32 ActiveCount = 0;
    for (const FAssetData& Asset : Definitions)
    {
        const FSoftObjectPath ObjectPath = Asset.GetSoftObjectPath();
        const FString Path = ObjectPath.ToString();
        const UFFZSFacilityDefinition* Definition = Cast<UFFZSFacilityDefinition>(Asset.GetAsset());
        if (!Definition)
        {
            AddError(FString::Printf(TEXT("%s: facility definition could not be loaded."), *Path));
            continue;
        }
        if (!Definition->bShowInBuildCatalog) continue;
        ++ActiveCount;
        const FFFZSFacilitySpec& Spec = Definition->Spec;
        const FName Id = Spec.FacilityDefinitionId;
        FFF_FacilityNameParts Parts;
        FString Failure;
        if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(Spec.Category, Id, Parts, Failure))
        {
            AddError(FString::Printf(TEXT("%s: invalid stable FacilityDefinitionId '%s': %s. "
                "Use DisplayName for player-facing text."), *Path, *Id.ToString(),
                Failure.IsEmpty() ? TEXT("ID is empty") : *Failure));
        }
        if (const FString* FirstPath = FirstPathById.Find(Id))
        {
            AddError(FString::Printf(TEXT("%s: duplicate FacilityDefinitionId '%s'; first asset: %s."),
                *Path, *Id.ToString(), **FirstPath));
        }
        else FirstPathById.Add(Id, Path);

        const FPrimaryAssetId Expected(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Id);
        const FPrimaryAssetId Actual = Definition->GetPrimaryAssetId();
        const FPrimaryAssetId Registered = Assets.GetPrimaryAssetIdForPath(ObjectPath);
        if (!Expected.IsValid() || Actual != Expected || Registered != Expected ||
            Assets.GetPrimaryAssetPath(Expected) != ObjectPath)
        {
            AddError(FString::Printf(TEXT("%s: PrimaryAssetId mismatch. Expected=%s, object=%s, registry=%s, mapped path=%s."),
                *Path, *Expected.ToString(), *Actual.ToString(), *Registered.ToString(),
                *Assets.GetPrimaryAssetPath(Expected).ToString()));
        }

        // Reuse the real resolver, including authored overrides and supported legacy/default
        // rows. Do not demand an invented per-facility row or a fixed catalog price.
        FName RequestedKey;
        Failure.Reset();
        if (!FFF_FacilityCostResolver::ResolveCostKey(Spec, RequestedKey, Failure))
        {
            AddError(FString::Printf(TEXT("%s: installation price key could not be derived: %s."), *Path, *Failure));
        }
        if (bUsableCosts)
        {
            int32 Cost = 0;
            FName MatchedRow;
            Failure.Reset();
            if (!FFF_FacilityCostResolver::FindInstallCost(Spec, Costs, Cost, MatchedRow, Failure))
            {
                AddError(FString::Printf(TEXT("%s: installation price could not be resolved from %s: %s."),
                    *Path, *Costs->GetPathName(), *Failure));
            }
            else if (Cost <= 0 || MatchedRow.IsNone())
            {
                AddError(FString::Printf(TEXT("%s: resolved installation price must be positive; table=%s, row=%s, Gold=%d."),
                    *Path, *Costs->GetPathName(), *MatchedRow.ToString(), Cost));
            }
        }
    }
    TestTrue(TEXT("At least one facility is active in the build catalog"), ActiveCount > 0);
    AddInfo(FString::Printf(TEXT("Checked %d active definitions out of %d registry assets without modifying content."),
        ActiveCount, Definitions.Num()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFFacilityDisplayNameIdentityTest,
    "FestaFiesta.Data.FacilityDefinitions.DisplayNamePreservesIdentityAndPrice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFFacilityDisplayNameIdentityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TStrongObjectPtr<UFFZSFacilityDefinition> Definition(NewObject<UFFZSFacilityDefinition>(GetTransientPackage()));
    TStrongObjectPtr<UDataTable> Costs(NewObject<UDataTable>(GetTransientPackage()));
    Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
    const FName StableId(TEXT("Info_Ticketing_DisplayNameFixture"));
    const FName ExactKey(TEXT("Cost.Info.Ticketing.DisplayNameFixture"));
    Definition->Spec.FacilityDefinitionId = StableId;
    Definition->Spec.Category = EFFZSFacilityCategory::Info;
    Definition->Spec.InstallCostKey = NAME_None;
    FFF_FacilityCostRow Row;
    Row.InstallCost = 173;
    Costs->AddRow(ExactKey, Row);
    Row.InstallCost = 19;
    Costs->AddRow(TEXT("Cost.Info.Default"), Row);
    const FPrimaryAssetId StablePrimaryId = Definition->GetPrimaryAssetId();

    // The distinct default price also detects accidental fallback after a display rename.
    for (const TCHAR* Label : { TEXT("Ticket"), TEXT("티켓 안내소"), TEXT("새 이름 / 안내 데스크"),
        TEXT("Booth_FoodStall_DisplayOnly") })
    {
        Definition->Spec.DisplayName = FText::FromString(Label);
        const FString Context = FString::Printf(TEXT("DisplayName '%s'"), Label);
        FName Key, MatchedRow;
        FString Failure;
        int32 Cost = 0;
        TestEqual(*(Context + TEXT(" preserves the stable ID")), Definition->Spec.FacilityDefinitionId, StableId);
        TestTrue(*(Context + TEXT(" preserves PrimaryAssetId")), Definition->GetPrimaryAssetId() == StablePrimaryId);
        TestTrue(*(Context + TEXT(" resolves its identity-based price key")),
            FFF_FacilityCostResolver::ResolveCostKey(Definition->Spec, Key, Failure));
        TestEqual(*(Context + TEXT(" keeps the exact price key")), Key, ExactKey);
        TestTrue(*(Context + TEXT(" resolves the authored price")),
            FFF_FacilityCostResolver::FindInstallCost(Definition->Spec, Costs.Get(), Cost, MatchedRow, Failure));
        TestEqual(*(Context + TEXT(" keeps the exact matched row instead of default fallback")), MatchedRow, ExactKey);
        TestEqual(*(Context + TEXT(" keeps the authored Gold amount")), Cost, 173);
    }

    // Reproduce the regression on a transient copy: a localized ID must be rejected even
    // when its DisplayName looks canonical, rather than silently charging the default row.
    Definition->Spec.FacilityDefinitionId = TEXT("티켓 안내소");
    Definition->Spec.DisplayName = FText::FromName(StableId);
    FFF_FacilityNameParts Parts;
    FString Failure;
    FName Key, MatchedRow;
    int32 Cost = 0;
    TestFalse(TEXT("Localized display text is not a stable facility identifier"),
        UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(Definition->Spec.Category,
            Definition->Spec.FacilityDefinitionId, Parts, Failure));
    TestFalse(TEXT("A canonical-looking DisplayName cannot repair a corrupted price identity"),
        FFF_FacilityCostResolver::ResolveCostKey(Definition->Spec, Key, Failure));
    TestFalse(TEXT("Invalid identity cannot silently use the category default price"),
        FFF_FacilityCostResolver::FindInstallCost(Definition->Spec, Costs.Get(), Cost, MatchedRow, Failure));
    return !HasAnyErrors();
}

#endif
