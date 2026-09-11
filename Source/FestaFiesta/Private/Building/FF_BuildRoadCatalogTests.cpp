#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"
#include "UObject/StrongObjectPtr.h"

namespace FFBuildRoadCatalogTest
{
    const TCHAR* Names[] = { TEXT("Sand"), TEXT("Asphalt"), TEXT("Wood"),
        TEXT("Stone"), TEXT("Herringbone"), TEXT("Concrete") };

    // Load through the same asynchronous discovery and validation path as the player's menu.
    class FVerifyCatalog final : public IAutomationLatentCommand
    {
    public:
        explicit FVerifyCatalog(FAutomationTestBase* InTest)
            : Test(InTest), GameInstance(NewObject<UGameInstance>()),
              Catalog(NewObject<UFF_BuildCatalogSubsystem>(GameInstance.Get())), StartedAt(FPlatformTime::Seconds())
        {
            Catalog->RefreshCatalog();
        }

        virtual bool Update() override
        {
            if (Catalog->GetLoadState() == EFF_BuildCatalogLoadState::Loading && FPlatformTime::Seconds() - StartedAt < 30.0)
            {
                return false;
            }
            if (!Test->TestTrue(*FString::Printf(TEXT("Catalog becomes ready: %s"), *Catalog->GetLastError().ToString()), Catalog->IsCatalogReady()))
            {
                Catalog->Deinitialize();
                return true;
            }
            const UFF_FestivalSettings* Settings = UFF_FestivalSettings::Get();
            const UFF_FestivalDefinition* Festival = Settings ? Settings->DefaultFestival.LoadSynchronous() : nullptr;
            const UDataTable* CostTable = Festival ? Festival->FacilityCostTable.Get() : nullptr;
            const TArray<FFF_BuildCatalogCategoryGroup> Groups = Catalog->GetCategoryGroups();
            const FFF_BuildCatalogCategoryGroup* RoadGroup = Groups.FindByPredicate(
                [](const FFF_BuildCatalogCategoryGroup& Group) { return Group.Category == EFFZSFacilityCategory::Road; });
            Test->TestNotNull(TEXT("Build menu exposes the road category"), RoadGroup);
            Test->TestNotNull(TEXT("Active festival supplies road prices"), CostTable);

            for (const TCHAR* Name : Names)
            {
                const FName Id(*FString::Printf(TEXT("Road_Surface_%s"), Name));
                const FPrimaryAssetId PrimaryId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Id);
                Test->TestEqual(TEXT("Road is packaged as a production primary asset"),
                    UAssetManager::Get().GetPrimaryAssetRules(PrimaryId).CookRule, EPrimaryAssetCookRule::AlwaysCook);
                FFF_BuildCatalogEntry Entry;
                if (!Test->TestTrue(*FString::Printf(TEXT("%s is selectable in the catalog"), Name), Catalog->FindEntry(Id, Entry))) continue;
                Test->TestEqual(TEXT("Road category survives validation"), Entry.Category, EFFZSFacilityCategory::Road);
                Test->TestEqual(TEXT("Roads share the surface group"), Entry.MinorToken, FName(TEXT("Surface")));
                FFF_PrepBuildFilter Filter;
                Filter.bFilterByCategory = true;
                Filter.Category = EFFZSFacilityCategory::Road;
                Filter.SearchText = TEXT("길");
                Test->TestTrue(TEXT("Prep HUD road filter and Korean search expose the road"),
                    FFF_PrepBuildPresentationRules::IsEntryVisible(Entry, Filter));
                Test->TestFalse(TEXT("Road card has its own thumbnail"), Entry.Thumbnail.IsNull());
                FFFZSFacilitySpec Spec;
                if (!Test->TestTrue(TEXT("Selection supplies an installable spec"), Catalog->TryGetDefinitionSpec(Id, Spec))) continue;
                const UFFZSFacilityDefinition* Definition = Catalog->FindDefinition(Id);
                if (!Test->TestNotNull(TEXT("Selection retains its authored definition"), Definition)) continue;
                // Size and layering permission belong to the authored PDA. Catalog projection
                // must preserve them rather than silently restore the original import defaults.
                TArray<FText> SpecErrors;
                Test->TestTrue(TEXT("Authored road specification remains valid"), Spec.IsValid(SpecErrors));
                Test->TestTrue(TEXT("Road has a positive authored footprint"),
                    Spec.FootprintSizeCells.X > 0 && Spec.FootprintSizeCells.Y > 0);
                Test->TestEqual(TEXT("Catalog preserves the authored road dimensions"),
                    Spec.FootprintSizeCells, Definition->Spec.FootprintSizeCells);
                Test->TestEqual(TEXT("Roads tile without added clearance"), Spec.ClearanceCells, 0);
                Test->TestEqual(TEXT("Catalog preserves each road's upper-facility permission"),
                    Spec.bAllowFacilityPlacementOnTop, Definition->Spec.bAllowFacilityPlacementOnTop);
                Test->TestEqual(TEXT("Catalog preserves the authored road surface height"),
                    Spec.PlacementObstacleHeightCm, Definition->Spec.PlacementObstacleHeightCm);
                Test->TestEqual(TEXT("Road creates no visitor service"), Spec.QueueRules.ServiceCapacity, 0);
                Test->TestEqual(TEXT("Road creates no waiting queue"), Spec.QueueRules.MaxWaitingAgents, 0);
                Test->TestFalse(TEXT("Road mesh is linked"), Spec.VisualMesh.IsNull());
                Test->TestFalse(TEXT("Road upgrades are disabled"), Spec.bUsesFacilityLevels);
                int32 Cost = 0;
                FName MatchedRow;
                FString Failure;
                const FName ExpectedRow(*FString::Printf(TEXT("Cost.Road.Surface.%s"), Name));
                Test->TestTrue(TEXT("Production economy resolves road cost"),
                    FFF_FacilityCostResolver::FindInstallCost(Spec, CostTable, Cost, MatchedRow, Failure));
                Test->TestEqual(TEXT("Road cost uses its exact row"), MatchedRow, ExpectedRow);
                Test->TestTrue(TEXT("Road installation cost is positive"), Cost > 0);
                if (Festival)
                {
                    Test->TestTrue(TEXT("Card price resolves from the installation table"), FFBuildCatalogModel::ApplyFacilityCosts(
                        Entry, Catalog->FindDefinition(Id), CostTable, Festival->FacilityDemolitionRefundRate, Failure));
                    Test->TestEqual(TEXT("Card price agrees with installation cost"), Entry.InstallCost, Cost);
                }
                if (RoadGroup)
                {
                    const FFF_BuildCatalogMinorGroup* Surface = RoadGroup->MinorGroups.FindByPredicate(
                        [](const FFF_BuildCatalogMinorGroup& Group) { return Group.MinorToken == TEXT("Surface"); });
                    if (Test->TestNotNull(TEXT("Road menu exposes the surface section"), Surface))
                    {
                        Test->TestEqual(TEXT("Road appears exactly once"), Surface->Items.FilterByPredicate(
                            [&Id](const FFF_BuildCatalogEntry& Item) { return Item.FacilityDefinitionId == Id; }).Num(), 1);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildRoadCatalogTest,
    "FestaFiesta.Building.Catalog.ProductionRoadAssetsAndEconomy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildRoadCatalogTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Production AssetManager is initialized"), UAssetManager::GetIfInitialized())) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FFBuildRoadCatalogTest::FVerifyCatalog(this));
    return true;
}

#endif
