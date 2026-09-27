// FF_BuildLightingCatalogTests.cpp
// 역할: BuildLightingCatalog 기능의 동작 계약을 자동 검사한다 (검사 경로: FestaFiesta.Building.Catalog.ProductionLightingAssetsAndEconomy).

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace FFBuildLightingCatalogTest
{
    struct FExpectedAsset
    {
        const TCHAR* Name;
        const TCHAR* Minor;
    };

    const FExpectedAsset ExpectedAssets[] = {
        { TEXT("CurtainArch"), TEXT("Decorative") },
        { TEXT("Festoon"), TEXT("Decorative") },
        { TEXT("GoldenTree"), TEXT("Decorative") },
        { TEXT("RattanPendant"), TEXT("Pendant") },
        { TEXT("StarTree"), TEXT("Decorative") },
        { TEXT("FestivalStreetlamp"), TEXT("Streetlamp") }
    };

    // Exercise the public asynchronous discovery path, including the actual catalog validation
    // and naming filters. Strong references keep the isolated catalog alive between editor ticks.
    class FVerifyCatalog final : public IAutomationLatentCommand
    {
    public:
        explicit FVerifyCatalog(FAutomationTestBase* InTest)
            : Test(InTest)
            , GameInstance(NewObject<UGameInstance>())
            , Catalog(NewObject<UFF_BuildCatalogSubsystem>(GameInstance.Get()))
            , StartedAt(FPlatformTime::Seconds())
        {
            Catalog->RefreshCatalog();
        }

        virtual bool Update() override
        {
            if (Catalog->GetLoadState() == EFF_BuildCatalogLoadState::Loading
                && FPlatformTime::Seconds() - StartedAt < 30.0)
            {
                return false;
            }
            if (!Test->TestTrue(*FString::Printf(TEXT("Production catalog becomes ready: %s"),
                *Catalog->GetLastError().ToString()), Catalog->IsCatalogReady()))
            {
                Catalog->Deinitialize();
                return true;
            }

            const UFF_FestivalSettings* Settings = UFF_FestivalSettings::Get();
            const UFF_FestivalDefinition* Festival = Settings ? Settings->DefaultFestival.LoadSynchronous() : nullptr;
            const UDataTable* CostTable = Festival ? Festival->FacilityCostTable.Get() : nullptr;
            if (!Test->TestNotNull(TEXT("Default Festival supplies live facility prices"), CostTable))
            {
                Catalog->Deinitialize();
                return true;
            }
            Test->TestEqual(TEXT("Lighting uses the same canonical cost table as the economy bridge"),
                CostTable->GetPathName(), FString(TEXT("/Game/FestaFiesta/Data/Economy/DT_FacilityCosts.DT_FacilityCosts")));

            UAssetManager& AssetManager = UAssetManager::Get();
            const TArray<FFF_BuildCatalogCategoryGroup> Groups = Catalog->GetCategoryGroups();
            const FFF_BuildCatalogCategoryGroup* LightingGroup = Groups.FindByPredicate(
                [](const FFF_BuildCatalogCategoryGroup& Group) { return Group.Category == EFFZSFacilityCategory::Lighting; });
            Test->TestNotNull(TEXT("Build menu exposes the lighting category"), LightingGroup);

            for (const FExpectedAsset& Expected : ExpectedAssets)
            {
                const FName DefinitionId(*FString::Printf(TEXT("Lighting_%s_%s"), Expected.Minor, Expected.Name));
                const FPrimaryAssetId PrimaryId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), DefinitionId);
                const FString AssetName = FString(TEXT("PDA_FFZS_")) + DefinitionId.ToString();
                const FSoftObjectPath ExpectedPath(FString::Printf(
                    TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Lighting/%s.%s"), *AssetName, *AssetName));
                Test->TestEqual(*FString::Printf(TEXT("%s is discovered as a production primary asset"), Expected.Name),
                    AssetManager.GetPrimaryAssetPath(PrimaryId), ExpectedPath);
                Test->TestEqual(*FString::Printf(TEXT("%s is included in packaged builds"), Expected.Name),
                    AssetManager.GetPrimaryAssetRules(PrimaryId).CookRule, EPrimaryAssetCookRule::AlwaysCook);

                FFF_BuildCatalogEntry Entry;
                if (!Test->TestTrue(*FString::Printf(TEXT("%s survives production catalog validation"), Expected.Name),
                    Catalog->FindEntry(DefinitionId, Entry))) continue;
                Test->TestEqual(TEXT("Catalog preserves the lighting category"), Entry.Category, EFFZSFacilityCategory::Lighting);
                Test->TestEqual(TEXT("Catalog preserves the expected minor group"), Entry.MinorToken, FName(Expected.Minor));
                Test->TestEqual(TEXT("Catalog preserves the selected asset name"), Entry.UniqueNameToken, FString(Expected.Name));
                Test->TestEqual(TEXT("Catalog preserves primary asset identity"), Entry.PrimaryAssetId, PrimaryId);
                Test->TestEqual(TEXT("Catalog selection resolves the production definition"), Entry.Definition.ToSoftObjectPath(), ExpectedPath);
                UFFZSFacilityDefinition* Definition = Catalog->FindDefinition(DefinitionId);
                if (!Test->TestNotNull(TEXT("Catalog keeps installable definition loaded"), Definition)) continue;
                FFFZSFacilitySpec InstallSpec;
                Test->TestTrue(TEXT("Player selection can retrieve an installation specification"),
                    Catalog->TryGetDefinitionSpec(DefinitionId, InstallSpec));
                Test->TestEqual(TEXT("Installation specification retains lighting category"), InstallSpec.Category, EFFZSFacilityCategory::Lighting);
                Test->TestTrue(TEXT("Installation specification retains its controllable rig"),
                    InstallSpec.LightingSettings.LightingRigClass == Definition->Spec.LightingSettings.LightingRigClass
                    && !InstallSpec.LightingSettings.LightingRigClass.IsNull());

                const FName ExpectedCostKey(*FString::Printf(TEXT("Cost.Lighting.%s.%s"), Expected.Minor, Expected.Name));
                const FFF_FacilityCostRow* ExactRow = CostTable->FindRow<FFF_FacilityCostRow>(
                    ExpectedCostKey, TEXT("LightingCatalogEconomyContract"), false);
                if (!Test->TestNotNull(*FString::Printf(TEXT("%s has an exact authored economy row"), Expected.Name), ExactRow)) continue;
                Test->TestTrue(TEXT("Authored price is accepted by construction economy"),
                    FFF_FacilityCostResolver::IsValidInstallCost(ExactRow->InstallCost));
                FName ResolvedKey;
                FString Failure;
                Test->TestTrue(TEXT("Lighting cost convention resolves"),
                    FFF_FacilityCostResolver::ResolveCostKey(InstallSpec, ResolvedKey, Failure));
                Test->TestEqual(TEXT("Convention selects the facility-specific row"), ResolvedKey, ExpectedCostKey);
                int32 InstallCost = 0;
                FName MatchedRow;
                Test->TestTrue(TEXT("Production economy resolver accepts the lighting definition"),
                    FFF_FacilityCostResolver::FindInstallCost(InstallSpec, CostTable, InstallCost, MatchedRow, Failure));
                Test->TestEqual(TEXT("Economy matches exact row without a fallback"), MatchedRow, ExpectedCostKey);
                Test->TestEqual(TEXT("Economy uses the authored lighting installation cost"), InstallCost, ExactRow->InstallCost);
                Test->TestTrue(TEXT("Catalog card can display the same live price"), FFBuildCatalogModel::ApplyFacilityCosts(
                    Entry, Definition, CostTable, Festival->FacilityDemolitionRefundRate, Failure));
                Test->TestTrue(TEXT("Install and recovery prices resolve for the catalog card"),
                    Entry.bHasResolvedInstallCost && Entry.bHasResolvedRecoveryCost);
                Test->TestEqual(TEXT("Catalog price agrees with actual construction cost"), Entry.InstallCost, InstallCost);
                Test->TestEqual(TEXT("Catalog records the exact economy row"), Entry.MatchedInstallCostRow, ExpectedCostKey);
                Test->TestEqual(TEXT("Catalog refund agrees with the active Festival rule"), Entry.RecoveryCost,
                    FFF_FacilityCostResolver::CalculateDemolitionRefund(InstallCost, Festival->FacilityDemolitionRefundRate));

                if (LightingGroup)
                {
                    const FFF_BuildCatalogMinorGroup* MinorGroup = LightingGroup->MinorGroups.FindByPredicate(
                        [&Expected](const FFF_BuildCatalogMinorGroup& Group) { return Group.MinorToken == FName(Expected.Minor); });
                    if (Test->TestNotNull(TEXT("Lighting menu contains the authored minor category"), MinorGroup))
                    {
                        Test->TestTrue(TEXT("Lighting category contains the selectable asset exactly once"),
                            MinorGroup->Items.FilterByPredicate([&DefinitionId](const FFF_BuildCatalogEntry& Item)
                                { return Item.FacilityDefinitionId == DefinitionId; }).Num() == 1);
                    }
                }
            }

            Catalog->Deinitialize();
            return true;
        }

    private:
        FAutomationTestBase* Test;
        TStrongObjectPtr<UGameInstance> GameInstance;
        TStrongObjectPtr<UFF_BuildCatalogSubsystem> Catalog;
        double StartedAt;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildLightingCatalogTest,
    "FestaFiesta.Building.Catalog.ProductionLightingAssetsAndEconomy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildLightingCatalogTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Production AssetManager is initialized"), UAssetManager::GetIfInitialized())) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FFBuildLightingCatalogTest::FVerifyCatalog(this));
    return true;
}

#endif
