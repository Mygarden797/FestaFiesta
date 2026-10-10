#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogModel.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildQueueLayoutShortcutTest,
    "FestaFiesta.Building.BuildMode.QueueLayout.TopRowAndInputGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_BuildQueueLayoutShortcutTest::RunTest(const FString& Parameters)
{
    const FKey Keys[] = {EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero};
    const EFFZSQueueLayout Layouts[] = {EFFZSQueueLayout::Authored, EFFZSQueueLayout::Left,
        EFFZSQueueLayout::Right, EFFZSQueueLayout::UShape};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        EFFZSQueueLayout Layout = EFFZSQueueLayout::Authored;
        TestTrue(TEXT("Top-row key resolves without modifiers"),
            FFBuildModeModel::TryResolveQueueLayoutShortcut(Keys[Index], false, false, false, Layout));
        TestEqual(TEXT("Number maps to requested layout"), Layout, Layouts[Index]);
        TestFalse(TEXT("Typing into an editable field keeps number input"),
            FFBuildModeModel::TryResolveQueueLayoutShortcut(Keys[Index], true, false, false, Layout));
        TestFalse(TEXT("Control, Shift, Alt or Command modifiers suppress queue shortcuts"),
            FFBuildModeModel::TryResolveQueueLayoutShortcut(Keys[Index], false, true, false, Layout));
        TestFalse(TEXT("Paused input cannot change queues"),
            FFBuildModeModel::TryResolveQueueLayoutShortcut(Keys[Index], false, false, true, Layout));
    }
    EFFZSQueueLayout Layout;
    for (const FKey Key : {EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine, EKeys::NumPadZero,
        EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Z, EKeys::Y})
        TestFalse(TEXT("Previous digits, numeric keypad and undo/redo keys are not queue layout shortcuts"),
            FFBuildModeModel::TryResolveQueueLayoutShortcut(Key, false, false, false, Layout));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildQueueLayoutDraftTest,
    "FestaFiesta.Building.BuildMode.QueueLayout.DraftsCancelAndInstallPayload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_BuildQueueLayoutDraftTest::RunTest(const FString& Parameters)
{
    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Nav(Settings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
    TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    UGameInstance* Game = World->GetGameInstance();
    UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
    UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    UFF_BuildCatalogSubsystem* Catalog = Game->GetSubsystem<UFF_BuildCatalogSubsystem>();
    UFF_FestivalProgressionSubsystem* Economy = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
    if (!TestNotNull(TEXT("Build"), Build) || !TestNotNull(TEXT("Zone authority"), Zones) ||
        !TestNotNull(TEXT("Catalog"), Catalog) || !TestNotNull(TEXT("Economy"), Economy)) return false;
    FFBuildCatalogModel::BeginNextRequest(Catalog->RequestGeneration);
    if (Catalog->ActiveLoadHandle.IsValid()) Catalog->ActiveLoadHandle->CancelHandle();
    Catalog->ActiveLoadHandle.Reset();
    TStrongObjectPtr<UFFZSFacilityDefinition> Definition(NewObject<UFFZSFacilityDefinition>());
    Definition->Spec = FFZSTest::MakeValidFacilitySpec();
    Definition->bEnableDragPlacement = true;
    Catalog->Entries.Reset();
    Catalog->EntryIndexByDefinitionId.Reset();
    FFF_BuildCatalogEntry& CatalogEntry = Catalog->Entries.AddDefaulted_GetRef();
    CatalogEntry.Definition = Definition.Get();
    Catalog->EntryIndexByDefinitionId.Add(Definition->Spec.FacilityDefinitionId, 0);

    TStrongObjectPtr<UFF_FestivalDefinition> Festival(NewObject<UFF_FestivalDefinition>(Game));
    TStrongObjectPtr<UDataTable> Costs(NewObject<UDataTable>(Game));
    Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
    FFF_FacilityCostRow Cost; Cost.InstallCost = 100;
    Costs->AddRow(Definition->Spec.InstallCostKey, Cost);
    Festival->FestivalId = TEXT("QueueLayoutDraftTest");
    Festival->FacilityCostTable = Costs.Get();
    Festival->InitialGold = 10000;
    Economy->StartFestival(Festival.Get());
    Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!FFProjectZoneTest::RegisterZone(*this, Zone, Zones)) return false;
    ON_SCOPE_EXIT
    {
        Build->EndDragPlacement();
        Build->DestroyPlacementPreview();
        Build->State = EFF_BuildModeState::Closed;
        Build->bInputSuspendedForPauseMenu = false;
    };
    Build->State = EFF_BuildModeState::Placing;
    Build->SelectedSpec = Definition->Spec;
    Build->SelectedFacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
    Build->CurrentZoneId = Zone->ZoneId;
    Build->CurrentPlacement.OriginCell = FIntPoint(1000, 1000);
    Build->SpawnPlacementPreview();
    if (!TestNotNull(TEXT("Placement preview"), Build->PlacementPreview.Get())) return false;
    // Old assets can still carry service defaults before their next normalization.
    // Category capability must keep these objects out of queue editing regardless.
    for (const EFFZSFacilityCategory Category : {EFFZSFacilityCategory::Lighting,
        EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile})
    {
        Build->SelectedSpec.Category = Category;
        TestFalse(TEXT("Non-service placement never exposes queue layout controls"), Build->CanChooseQueueLayout());
        TestFalse(TEXT("Non-service placement rejects queue shape shortcuts"),
            Build->SetSelectedFacilityQueueLayout(EFFZSQueueLayout::Left));
        TestTrue(TEXT("Non-service placement has no queue shortcut hint"), Build->GetQueueLayoutKeyHint().IsEmpty());
    }
    Build->SelectedSpec = Definition->Spec;
    TestTrue(TEXT("Placing supports queue layout shortcuts"), Build->CanChooseQueueLayout());
    TestTrue(TEXT("A placing choice updates the draft"), Build->SetSelectedFacilityQueueLayout(EFFZSQueueLayout::Left));
    Build->RebuildPreviewCommand();
    const FFFZSCommand Install = Build->PlacementPreview->PreviewCommand;
    TestEqual(TEXT("Install payload carries the draft choice"), Install.FacilitySpec.QueueLayout, EFFZSQueueLayout::Left);
    TestTrue(TEXT("Install payload retains authored geometry"), Install.FacilitySpec.QueueSplinePointsCm == Definition->Spec.QueueSplinePointsCm);
    TestTrue(TEXT("Ghost geometry uses the chosen shape"), Build->SelectedSpec.QueueSplinePointsCm != Definition->Spec.QueueSplinePointsCm);
    TestEqual(TEXT("Draft alone purchases no facilities"), Zones->GetPlacedFacilityCount(), 0);
    TestTrue(TEXT("Chosen install payload commits successfully"), Zones->ExecuteCommand(Install).bSucceeded);
    AFFZSFacilityActor* Facility = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed facility"), Facility)) return false;
    TestEqual(TEXT("Installed actor uses the preview choice"), Facility->GetQueueLayout(), EFFZSQueueLayout::Left);

    Definition->Spec.QueueRules.MaxWaitingAgents = 3;
    TestFalse(TEXT("Unsupported U draft is rejected with no mutation"), Build->SetSelectedFacilityQueueLayout(EFFZSQueueLayout::UShape));
    TestEqual(TEXT("Rejected draft preserves last valid choice"), Build->GetEditingQueueLayout(), EFFZSQueueLayout::Left);
    Definition->Spec.QueueRules.MaxWaitingAgents = 12;
    Build->bInputSuspendedForPauseMenu = true;
    TestFalse(TEXT("Paused direct action also rejects edits"), Build->SetSelectedFacilityQueueLayout(EFFZSQueueLayout::Right));
    Build->bInputSuspendedForPauseMenu = false;
    Build->bHasPlacementTarget = true;
    Build->BeginDragPlacement(true);
    TestTrue(TEXT("A paint stroke can use the chosen shape"), Build->DragPlacementSession.IsValid());
    TestTrue(TEXT("Changing shape stops pending paint targets"), Build->SetSelectedFacilityQueueLayout(EFFZSQueueLayout::Right));
    TestFalse(TEXT("Old stroke cannot continue using the new footprint"), Build->DragPlacementSession.IsValid());

    Build->State = EFF_BuildModeState::Moving;
    Build->MovingFacility = Facility;
    Build->MovingFacilityInstanceId = Facility->GetFacilityInstanceId();
    Build->MoveTransactionId = FGuid::NewGuid();
    Build->MoveOriginZoneId = Facility->GetZoneId();
    Build->MoveOriginPlacement = Facility->GetGridPlacement();
    Build->SelectedSpec = Facility->GetFacilitySpec();
    Build->PendingQueueLayout = Facility->GetQueueLayout();
    Build->bHasPlacementTarget = false;
    const int64 RevisionBeforeDraft = Zones->GetWorldRevision();
    TestTrue(TEXT("Moving supports queue draft changes"), Build->SetSelectedFacilityQueueLayout(EFFZSQueueLayout::UShape));
    Build->RebuildPreviewCommand();
    TestTrue(TEXT("Move payload explicitly opts into atomic queue change"), Build->PlacementPreview->PreviewCommand.bChangeQueueLayout);
    TestEqual(TEXT("Draft leaves original queue intact"), Facility->GetQueueLayout(), EFFZSQueueLayout::Left);
    TestEqual(TEXT("Draft leaves authority revision intact"), Zones->GetWorldRevision(), RevisionBeforeDraft);
    Build->CancelMovingToBuildMode(TEXT(""));
    TestEqual(TEXT("Cancel leaves original queue intact"), Facility->GetQueueLayout(), EFFZSQueueLayout::Left);
    TestEqual(TEXT("Cancel returns to installed-facility mode"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Cancel clears the draft"), Build->PendingQueueLayout, EFFZSQueueLayout::Authored);
    return true;
}

#endif
