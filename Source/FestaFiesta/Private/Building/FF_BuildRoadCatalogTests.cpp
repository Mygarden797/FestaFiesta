// FF_BuildRoadCatalogTests.cpp
// 역할: BuildRoadCatalog 기능의 동작 계약을 자동 검사한다 (검사 경로: FestaFiesta.Building.Catalog.ProductionRoadAssetsAndEconomy).

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
    // Required baseline prevents an empty/incomplete catalog from passing. Additional
    // authored surfaces are discovered below and receive the same checks automatically.
    // Wood and Stone moved from roads to tiles; the remaining four stay roads only.
    const TCHAR* RoadNames[] = { TEXT("Sand"), TEXT("Asphalt"), TEXT("Herringbone"), TEXT("Concrete") };
    const TCHAR* TileNames[] = { TEXT("Wood"), TEXT("Stone") };

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
            Test->TestNotNull(TEXT("Active festival supplies road and tile prices"), CostTable);

            for (const EFFZSFacilityCategory Category : { EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile })
            {
                const bool bIsTile = Category == EFFZSFacilityCategory::Tile;
                const TCHAR* MajorToken = bIsTile ? TEXT("Tile") : TEXT("Road");
                const FFF_BuildCatalogCategoryGroup* CategoryGroup = Groups.FindByPredicate(
                    [Category](const FFF_BuildCatalogCategoryGroup& Group) { return Group.Category == Category; });
                Test->TestNotNull(*FString::Printf(TEXT("Build menu exposes %s separately"), MajorToken), CategoryGroup);
                TArray<FName> SurfaceIds;
                for (const FFF_BuildCatalogEntry& Entry : Catalog->GetEntriesView())
                {
                    if (Entry.Category == Category && Entry.MinorToken == TEXT("Surface"))
                    {
                        SurfaceIds.Add(Entry.FacilityDefinitionId);
                    }
                }
                for (const TCHAR* Name : bIsTile ? MakeArrayView(TileNames) : MakeArrayView(RoadNames))
                {
                    const FName RequiredId(*FString::Printf(TEXT("%s_Surface_%s"), MajorToken, Name));
                    Test->TestTrue(*FString::Printf(TEXT("Baseline surface %s remains selectable"), *RequiredId.ToString()),
                        SurfaceIds.Contains(RequiredId));
                }
                Test->AddInfo(FString::Printf(TEXT("Checking all %d catalog %s surfaces"), SurfaceIds.Num(), MajorToken));
                for (const FName Id : SurfaceIds)
                {
                    const FPrimaryAssetId PrimaryId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Id);
                    Test->TestEqual(TEXT("Surface is packaged as a production primary asset"),
                        UAssetManager::Get().GetPrimaryAssetRules(PrimaryId).CookRule, EPrimaryAssetCookRule::AlwaysCook);
                    FFF_BuildCatalogEntry Entry;
                    if (!Test->TestTrue(*FString::Printf(TEXT("%s is selectable in the catalog"), *Id.ToString()), Catalog->FindEntry(Id, Entry))) continue;
                    Test->TestEqual(TEXT("Surface category survives validation"), Entry.Category, Category);
                    Test->TestEqual(TEXT("Roads and tiles share the surface minor token"), Entry.MinorToken, FName(TEXT("Surface")));
                    FFF_PrepBuildFilter Filter;
                    Filter.bFilterByCategory = true;
                    Filter.Category = Category;
                    Test->TestFalse(TEXT("Surface has an authored display name"), Entry.DisplayName.IsEmptyOrWhitespace());
                    Filter.SearchText = Entry.DisplayName.ToString();
                    Test->TestTrue(TEXT("Prep HUD category and authored-name search expose the surface"),
                        FFF_PrepBuildPresentationRules::IsEntryVisible(Entry, Filter));
                    Filter.Category = bIsTile ? EFFZSFacilityCategory::Road : EFFZSFacilityCategory::Tile;
                    Filter.SearchText.Reset();
                    Test->TestFalse(TEXT("Road and tile category filters remain distinct"),
                        FFF_PrepBuildPresentationRules::IsEntryVisible(Entry, Filter));
                    Test->TestFalse(TEXT("Surface card has its own thumbnail"), Entry.Thumbnail.IsNull());
                    FFFZSFacilitySpec Spec;
                    if (!Test->TestTrue(TEXT("Selection supplies an installable spec"), Catalog->TryGetDefinitionSpec(Id, Spec))) continue;
                    const UFFZSFacilityDefinition* Definition = Catalog->FindDefinition(Id);
                    if (!Test->TestNotNull(TEXT("Selection retains its authored definition"), Definition)) continue;
                    // Keep authored dimensions while deriving stacking permission from the category.
                    TArray<FText> SpecErrors;
                    Test->TestTrue(TEXT("Authored surface specification remains valid"), Spec.IsValid(SpecErrors));
                    Test->TestTrue(TEXT("Surface has a positive authored footprint"),
                        Spec.FootprintSizeCells.X > 0 && Spec.FootprintSizeCells.Y > 0);
                    Test->TestEqual(TEXT("Catalog preserves the authored surface dimensions"),
                        Spec.FootprintSizeCells, Definition->Spec.FootprintSizeCells);
                    Test->TestEqual(TEXT("Surfaces connect without added clearance"), Spec.ClearanceCells, 0);
                    Test->TestTrue(TEXT("Road and tile are both ground surfaces"), Spec.IsGroundSurface());
                    Test->TestEqual(TEXT("Only tiles allow facilities on top"), Spec.AllowsFacilityPlacementOnTop(), bIsTile);
                    Test->TestEqual(TEXT("Catalog preserves the authored surface height"),
                        Spec.PlacementObstacleHeightCm, Definition->Spec.PlacementObstacleHeightCm);
                    Test->TestEqual(TEXT("Surface creates no visitor service"), Spec.QueueRules.ServiceCapacity, 0);
                    Test->TestEqual(TEXT("Surface creates no waiting queue"), Spec.QueueRules.MaxWaitingAgents, 0);
                    Test->TestFalse(TEXT("Surface mesh is linked"), Spec.VisualMesh.IsNull());
                    Test->TestFalse(TEXT("Surface upgrades are disabled"), Spec.bUsesFacilityLevels);
                    Test->TestTrue(TEXT("Both surface categories support drag placement"), Definition->bEnableDragPlacement);
                    int32 Cost = 0;
                    FName MatchedRow;
                    FString Failure;
                    const FName ExpectedRow = Spec.InstallCostKey.IsNone()
                        ? FName(*FString::Printf(TEXT("Cost.%s.Surface.%s"), MajorToken, *Entry.UniqueNameToken))
                        : Spec.InstallCostKey;
                    Test->TestTrue(TEXT("Production economy resolves surface cost"),
                        FFF_FacilityCostResolver::FindInstallCost(Spec, CostTable, Cost, MatchedRow, Failure));
                    Test->TestEqual(TEXT("Surface cost honors its authored key or exact category row"), MatchedRow, ExpectedRow);
                    Test->TestTrue(TEXT("Surface installation cost is valid"), FFF_FacilityCostResolver::IsValidInstallCost(Cost));
                    if (Festival)
                    {
                        Test->TestTrue(TEXT("Card price resolves from the installation table"), FFBuildCatalogModel::ApplyFacilityCosts(
                            Entry, Catalog->FindDefinition(Id), CostTable, Festival->FacilityDemolitionRefundRate, Failure));
                        Test->TestEqual(TEXT("Card price agrees with installation cost"), Entry.InstallCost, Cost);
                    }
                    if (CategoryGroup)
                    {
                        const FFF_BuildCatalogMinorGroup* Surface = CategoryGroup->MinorGroups.FindByPredicate(
                            [](const FFF_BuildCatalogMinorGroup& Group) { return Group.MinorToken == TEXT("Surface"); });
                        if (Test->TestNotNull(TEXT("Each surface category exposes its surface section"), Surface))
                        {
                            Test->TestEqual(TEXT("Surface appears exactly once"), Surface->Items.FilterByPredicate(
                                [&Id](const FFF_BuildCatalogEntry& Item) { return Item.FacilityDefinitionId == Id; }).Num(), 1);
                        }
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
