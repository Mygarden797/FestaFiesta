// FF_FacilityAuroraBuildTests.cpp
// 역할: 카탈로그 선택이 기존 권위 검증을 거쳐 즉시 배치 미리보기를 시작하는지 확인한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationModel.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AuroraCatalogSelectionTest,
    "FestaFiesta.UI.PrepBuild.CatalogSelectionStartsPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_AuroraCatalogSelectionTest::RunTest(const FString& Parameters)
{
    UClass* PanelClass = LoadClass<UFF_PrepPanelBuild>(nullptr,
        FFBuildUIAssetPaths::BuildPanelClass);
    if (!TestNotNull(TEXT("Saved production catalog class"), PanelClass)) return false;
    // Real authority and economy live in an isolated world. No install/confirm call is made.
    auto* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    auto* Game = World->GetGameInstance();
    auto* Mode = World->GetSubsystem<UFF_BuildModeSubsystem>();
    auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    auto* Catalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
    auto* Progression = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
    auto* Economy = Game ? Game->GetSubsystem<UFF_BuildCatalogEconomyReadModel>() : nullptr;
    auto* Festival = LoadObject<UFF_FestivalDefinition>(nullptr,
        TEXT("/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan.DA_Festival_Busan"));
    auto* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_CraftExperience_SandArt.PDA_FFZS_Booth_CraftExperience_SandArt"));
    if (!TestNotNull(TEXT("Real build mode"), Mode) || !TestNotNull(TEXT("Real zones"), Zones) ||
        !TestNotNull(TEXT("Real catalog"), Catalog) || !TestNotNull(TEXT("Real economy"), Progression) ||
        !TestNotNull(TEXT("Catalog economy projection"), Economy) || !TestNotNull(TEXT("Source festival"), Festival) ||
        !TestNotNull(TEXT("Source facility"), Definition)) return false;
    FFBuildCatalogModel::BeginNextRequest(Catalog->RequestGeneration);
    if (Catalog->ActiveLoadHandle.IsValid() && !Catalog->ActiveLoadHandle->HasLoadCompleted())
        Catalog->ActiveLoadHandle->CancelHandle();
    Catalog->ActiveLoadHandle.Reset();
    Catalog->RequestedAssetIds.Reset();
    Catalog->Entries.Reset();
    Catalog->EntryIndexByDefinitionId.Reset();
    FFF_BuildCatalogEntry Entry;
    FString Failure;
    if (!TestTrue(TEXT("Production definition becomes a real catalog entry"),
        Catalog->TryMakeEntry(Definition->GetPrimaryAssetId(), *Definition, Entry, Failure))) return false;
    Catalog->EntryIndexByDefinitionId.Add(Entry.FacilityDefinitionId, Catalog->Entries.Add(Entry));
    Catalog->RebuildCategoryGroups();
    Catalog->FinishRefresh(EFF_BuildCatalogLoadState::Ready);
    Progression->StartFestival(Festival);
    Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
    Mode->State = EFF_BuildModeState::CatalogOpen;
    ON_SCOPE_EXIT
    {
        Mode->DestroyPlacementPreview();
        Mode->ResetPlacementSelection();
        Mode->DismissPlacementFailureFeedback(true);
        Mode->State = EFF_BuildModeState::Closed;
    };
    auto* Panel = NewObject<UFF_PrepPanelBuild>(World, PanelClass);
    Panel->BuildModeSubsystem = Mode;
    Panel->CatalogSubsystem = Catalog;
    Panel->EconomyReadModel = Economy;
    Panel->ZoneSystem = Zones;
    Panel->PresentationModel = NewObject<UFF_PrepBuildPresentationModel>(Panel);
    Panel->Button_PlaceSelected = NewObject<UButton>(Panel);
    Panel->Text_Status = NewObject<UTextBlock>(Panel);
    Panel->RefreshPricedEntries();
    Panel->ReconcilePresentation();
    auto* Tile = Panel->PresentationModel->FindTile(Entry.FacilityDefinitionId);
    if (!TestNotNull(TEXT("Priced real card"), Tile) || !TestTrue(TEXT("Fixture can afford its real card"), Tile->IsAffordable())) return false;
    const int32 GoldBefore = Progression->GetGold();
    const int64 RevisionBefore = Zones->GetWorldRevision();
    Panel->HandleTileSelectionChanged(Tile);
    TestEqual(TEXT("Keyboard navigation records focus only"), Panel->GetSelectedCatalogFacilityId(), Entry.FacilityDefinitionId);
    TestEqual(TEXT("Focus leaves the catalog open"), Mode->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
    TestNull(TEXT("Focus creates no preview"), Mode->PlacementPreview.Get());
    Panel->RequestFacilityFocus(Entry.FacilityDefinitionId);
    TestEqual(TEXT("Inspector focus does not activate placement"), Mode->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
    TestEqual(TEXT("Retired confirmation action remains collapsed"), Panel->Button_PlaceSelected->GetVisibility(), ESlateVisibility::Collapsed);
    Panel->HandleTileClicked(Tile);
    TestEqual(TEXT("A card click starts placement mode"), Mode->GetBuildModeState(), EFF_BuildModeState::Placing);
    TestNotNull(TEXT("A card click creates the actual world preview"), Mode->PlacementPreview.Get());
    TestEqual(TEXT("Selection retains the actual definition"), Panel->GetSelectedCatalogFacilityId(), Entry.FacilityDefinitionId);
    TestEqual(TEXT("Preview uses the clicked definition"), Mode->GetSelectedFacilityDefinitionId(), Entry.FacilityDefinitionId);
    TestEqual(TEXT("Clicking a card never charges gold"), Progression->GetGold(), GoldBefore);
    TestEqual(TEXT("Starting a preview never executes a world command"), Zones->GetWorldRevision(), RevisionBefore);
    TestEqual(TEXT("No facility is installed before world confirmation"), Zones->CountFacilitiesByDefinitionId(Entry.FacilityDefinitionId), 0);
    Mode->DestroyPlacementPreview();
    Mode->ResetPlacementSelection();
    Mode->State = EFF_BuildModeState::CatalogOpen;
    TestTrue(TEXT("Fixture spends its balance independently of catalog input"),
        Progression->TrySpend(GoldBefore, EFF_SpendReason::Promotion));
    Panel->HandleTileClicked(Tile);
    TestEqual(TEXT("An unaffordable card keeps the catalog open"), Mode->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
    TestNull(TEXT("An unaffordable card allocates no preview"), Mode->PlacementPreview.Get());
    TestFalse(TEXT("The card is revalidated against the latest balance"), Tile->IsAffordable());
    TestEqual(TEXT("The actual affordability reason is visible"), Panel->Text_Status->GetText().ToString(), Tile->GetStateText().ToString());
    TestEqual(TEXT("Rejected activation never mutates the world"), Zones->GetWorldRevision(), RevisionBefore);

    // Seed a pre-existing facility in this isolated world; count projection still
    // reads the real authority registry instead of a stale catalog snapshot.
    const FName FixtureZoneId(FFProjectZoneTest::ZoneId());
    auto* FixtureZone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Existing facility's real zone"), FixtureZone) ||
        !TestTrue(TEXT("Fixture zone uses a valid accepted preset"), FixtureZone->ApplyAcceptedZonePreset(FixtureZoneId)) ||
        !TestTrue(TEXT("Fixture zone registers with authority"), Zones->RegisterZone(FixtureZone))) return false;
    auto* Installed = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestNotNull(TEXT("Pre-existing facility fixture"), Installed)) return false;
    *FindFProperty<FStructProperty>(Installed->GetClass(), TEXT("FacilitySpec"))
        ->ContainerPtrToValuePtr<FFFZSFacilitySpec>(Installed) = Definition->Spec;
    *FindFProperty<FStructProperty>(Installed->GetClass(), TEXT("FacilityInstanceId"))
        ->ContainerPtrToValuePtr<FGuid>(Installed) = FGuid::NewGuid();
    FindFProperty<FNameProperty>(Installed->GetClass(), TEXT("ZoneId"))
        ->SetPropertyValue_InContainer(Installed, FixtureZoneId);
    if (!TestTrue(TEXT("Existing fixture registers with the real count authority"), Zones->RegisterFacility(Installed))) return false;
    Catalog->Entries[0].MaxInstancesPerWorld = 1;
    auto* FeedbackClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildPlacementFailureFeedback>(nullptr,
        FFBuildUIAssetPaths::PlacementFailureFeedbackClass));
    if (!TestNotNull(TEXT("Production failure toast class"), FeedbackClass)) return false;
    {
        const bool bPreviousPlayerless = FeedbackClass->bCanCallInitializedWithoutPlayerContext;
        FeedbackClass->bCanCallInitializedWithoutPlayerContext = true;
        Mode->PlacementFailureFeedback = CreateWidget<UFF_BuildPlacementFailureFeedback>(World, FeedbackClass);
        FeedbackClass->bCanCallInitializedWithoutPlayerContext = bPreviousPlayerless;
    }
    if (!TestNotNull(TEXT("Production toast fixture"), Mode->PlacementFailureFeedback.Get())) return false;
    const TSharedRef<SWidget> FeedbackSlate = Mode->PlacementFailureFeedback->TakeWidget();
    Panel->Text_Status = nullptr; // Production web ribbon has no visible legacy status footer.
    const int64 CappedRevision = Zones->GetWorldRevision();
    const int32 CappedGold = Progression->GetGold();
    Panel->HandleTileClicked(Tile);
    TestEqual(TEXT("Capacity rejection keeps the catalog open"), Mode->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
    TestNull(TEXT("Clicking a locked card never starts a placement preview"), Mode->PlacementPreview.Get());
    TestEqual(TEXT("Capacity wins over the simultaneously insufficient balance"),
        Tile->GetTileState(), EFF_PrepBuildTileState::BuildLimitReached);
    TestEqual(TEXT("Clicking a locked card displays the non-blocking toast"),
        Mode->PlacementFailureFeedback->GetVisibility(), ESlateVisibility::HitTestInvisible);
    TestEqual(TEXT("The toast's actual text widget receives the complete current reason"),
        Mode->PlacementFailureFeedback->GetDisplayedFailureReason().ToString(), Tile->GetStateText().ToString());
    TestTrue(TEXT("The visible reason includes capacity and how to unlock"),
        Mode->PlacementFailureFeedback->GetDisplayedFailureReason().ToString().Contains(TEXT("1/1")) &&
        Mode->PlacementFailureFeedback->GetDisplayedFailureReason().ToString().Contains(TEXT("철거")));
    TestEqual(TEXT("A locked click cannot spend gold"), Progression->GetGold(), CappedGold);
    TestEqual(TEXT("A locked click cannot mutate the world"), Zones->GetWorldRevision(), CappedRevision);
    Zones->UnregisterFacility(Installed);
    Panel->RefreshPricedEntries();
    Panel->ReconcilePresentation();
    TestEqual(TEXT("Removing the installed facility clears the cap on the same pooled tile"),
        Tile->GetTileState(), EFF_PrepBuildTileState::InsufficientFunds);
    Panel->PresentationModel->Reconcile(EFF_BuildCatalogLoadState::Ready, {}, {}, 7600, true, FText::GetEmpty());
    Panel->ApplyPresentation(false);
    TestTrue(TEXT("Filtering the candidate out clears its selection"), Panel->GetSelectedCatalogFacilityId().IsNone());
    TestFalse(TEXT("A hidden/stale card cannot be selected"), Panel->SelectCatalogFacility(Entry.FacilityDefinitionId));
    TestFalse(TEXT("A hidden/stale card cannot activate placement"), Panel->ActivateCatalogFacility(Entry.FacilityDefinitionId));
    return true;
}
#endif
