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

    // The same editor owns team capacity and physical lane size. All fields are
    // committed together, including a queue-only edit at the original location.
    Build->State = EFF_BuildModeState::Placing;
    Build->SelectedFacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
    Build->SelectedSpec = Definition->Spec;
    Build->CurrentZoneId = Zone->ZoneId;
    Build->CurrentPlacement.OriginCell = FIntPoint(6000, 6000);
    Build->bHasPlacementTarget = true;
    Build->SpawnPlacementPreview();
    if (!TestNotNull(TEXT("Queue settings preview"), Build->PlacementPreview.Get())) return false;
    TestTrue(TEXT("A valid team/spacing/width draft is accepted atomically"),
        Build->SetSelectedFacilityQueueSettings(8, 150.f, 600.f));
    const FFFZSQueueSettings Custom = Build->GetEditingQueueSettings();
    TestTrue(TEXT("Applying a draft opts into the common queue generator"), Custom.bEnabled);
    TestFalse(TEXT("An invalid width rejects the complete edit"),
        Build->SetSelectedFacilityQueueSettings(20, 200.f, 10.f));
    TestTrue(TEXT("Invalid settings preserve all previous fields"), Build->GetEditingQueueSettings() == Custom);
    TestTrue(TEXT("Changing shape preserves custom capacity and dimensions"),
        Build->SetSelectedFacilityQueueLayout(EFFZSQueueLayout::Right));
    TestTrue(TEXT("Shape shortcuts retain the entire user configuration"), Build->GetEditingQueueSettings() == Custom);
    Build->RebuildPreviewCommand();
    const FFFZSCommand CustomInstall = Build->PlacementPreview->PreviewCommand;
    TestTrue(TEXT("Install payload carries the complete draft"), CustomInstall.FacilitySpec.QueueSettings == Custom);
    TestEqual(TEXT("Effective preview exposes the selected team capacity"), Build->SelectedSpec.QueueRules.MaxWaitingAgents, 8);
    TestEqual(TEXT("Effective preview exposes the selected slot spacing"), Build->SelectedSpec.QueueRules.SlotSpacingCm, 150.f);
    if (!TestTrue(TEXT("Custom queue facility installs"), Zones->ExecuteCommand(CustomInstall).bSucceeded)) return false;
    AFFZSFacilityActor* CustomFacility = Zones->FindFacility(CustomInstall.FacilityInstanceId);
    if (!TestNotNull(TEXT("Custom facility"), CustomFacility)) return false;
    TestTrue(TEXT("Installed base retains the user configuration"), CustomFacility->GetBaseFacilitySpec().QueueSettings == Custom);
    TestEqual(TEXT("Real queue component receives the new capacity"), CustomFacility->GetEffectiveQueueRules().MaxWaitingAgents, 8);

    Build->State = EFF_BuildModeState::Moving;
    Build->MovingFacility = CustomFacility;
    Build->MovingFacilityInstanceId = CustomFacility->GetFacilityInstanceId();
    Build->MoveTransactionId = FGuid::NewGuid();
    Build->MoveOriginZoneId = CustomFacility->GetZoneId();
    Build->MoveOriginPlacement = CustomFacility->GetGridPlacement();
    Build->CurrentZoneId = Build->MoveOriginZoneId;
    Build->CurrentPlacement = Build->MoveOriginPlacement;
    Build->SelectedSpec = CustomFacility->GetFacilitySpec();
    Build->PendingQueueLayout = CustomFacility->GetQueueLayout();
    Build->PendingQueueSettings = Custom;
    Build->bHasPlacementTarget = true;
    TestTrue(TEXT("Installed queue accepts a size/capacity-only draft"),
        Build->SetSelectedFacilityQueueSettings(6, 140.f, 500.f));
    Build->RefreshCurrentPlacementValidation();
    TestTrue(TEXT("Move payload explicitly opts into settings changes"),
        Build->PlacementPreview->PreviewCommand.bChangeQueueSettings);
    TestEqual(TEXT("Draft leaves installed queue capacity untouched"), CustomFacility->GetEffectiveQueueRules().MaxWaitingAgents, 8);
    TestTrue(TEXT("Same-position confirmation commits changed queue settings"), Build->CommitPlacement());
    CustomFacility = Zones->FindFacility(CustomInstall.FacilityInstanceId);
    if (!TestNotNull(TEXT("Reconfigured facility"), CustomFacility)) return false;
    TestEqual(TEXT("Confirmed queue receives the new team capacity"), CustomFacility->GetEffectiveQueueRules().MaxWaitingAgents, 6);
    TestEqual(TEXT("Queue edit does not move the facility"), CustomFacility->GetGridPlacement().OriginCell, CustomInstall.Placement.OriginCell);
    TestTrue(TEXT("Queue settings are one undoable edit"), Build->UndoBuildEdit());
    CustomFacility = Zones->FindFacility(CustomInstall.FacilityInstanceId);
    if (!TestNotNull(TEXT("Facility after settings undo"), CustomFacility)) return false;
    TestTrue(TEXT("Undo restores original capacity and dimensions"), CustomFacility->GetBaseFacilitySpec().QueueSettings == Custom);
    TestTrue(TEXT("Queue settings can be redone"), Build->RedoBuildEdit());

    TestTrue(TEXT("Existing queue facilities can be unified in one operation"), Build->ResetAllFacilityQueuesToDefaults());
    FFFZSQueueSettings Defaults;
    Defaults.bEnabled = true;
    for (const FGuid& Id : {Install.FacilityInstanceId, CustomInstall.FacilityInstanceId})
    {
        const AFFZSFacilityActor* Unified = Zones->FindFacility(Id);
        if (TestNotNull(TEXT("Unified facility remains installed"), Unified))
        {
            TestTrue(TEXT("Every queue uses the same common default settings"), Unified->GetBaseFacilitySpec().QueueSettings == Defaults);
            TestEqual(TEXT("Every queue uses the common default shape"), Unified->GetQueueLayout(), EFFZSQueueLayout::Authored);
        }
    }
    TestTrue(TEXT("One undo restores the settings of every unified facility"), Build->UndoBuildEdit());
    CustomFacility = Zones->FindFacility(CustomInstall.FacilityInstanceId);
    if (!TestNotNull(TEXT("Facility after bulk undo"), CustomFacility)) return false;
    TestEqual(TEXT("Bulk undo restores the independently edited capacity"), CustomFacility->GetEffectiveQueueRules().MaxWaitingAgents, 6);

    UFFZSFacilityDefinition* MainStage = LoadObject<UFFZSFacilityDefinition>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage.PDA_FFZS_Performance_MainStage_SunsetMainStage"));
    if (!TestNotNull(TEXT("Main stage definition for audience-adjacent draft"), MainStage)) return false;
    Catalog->EntryIndexByDefinitionId.Add(MainStage->Spec.FacilityDefinitionId, Catalog->Entries.Num());
    Catalog->Entries.AddDefaulted_GetRef().Definition = MainStage;
    Build->State = EFF_BuildModeState::Placing;
    Build->SelectedFacilityDefinitionId = MainStage->Spec.FacilityDefinitionId;
    Build->SelectedSpec = MainStage->Spec;
    Build->PendingQueueLayout = EFFZSQueueLayout::Authored;
    Build->PendingQueueSettings = MainStage->Spec.QueueSettings;
    Build->CurrentZoneId = Zone->ZoneId;
    Build->bHasPlacementTarget = false;
    Build->SpawnPlacementPreview();
    const FFFZSAudienceLayout MainStageAudience = Build->SelectedSpec.Audience;
    TestFalse(TEXT("The production main stage cannot recreate a retired service queue"),
        Build->SetSelectedFacilityQueueSettings(6, 115.f, 300.f));
    Build->RebuildPreviewCommand();
    TestTrue(TEXT("A main stage command rebuild retains no FIFO path or reservation"),
        Build->SelectedSpec.QueueSplinePointsCm.IsEmpty() && Build->SelectedSpec.QueueReservedRects.IsEmpty());
    TestTrue(TEXT("Queue controls preserve every main stage audience seat and portal"),
        FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&MainStageAudience, &Build->SelectedSpec.Audience, 0));

    UFFZSFacilityDefinition* AudienceStage = NewObject<UFFZSFacilityDefinition>(GetTransientPackage());
    AudienceStage->Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Performance);
    AudienceStage->Spec.FacilityDefinitionId = TEXT("Performance_SubStage_AudienceQueueDraftTest");
    AudienceStage->Spec.Audience.bEnabled = true;
    AudienceStage->Spec.Audience.FirstSlotLocalCm = FVector(0, 700, 0);
    if (!TestTrue(TEXT("The separate performance fixture has valid audience portals"),
        AudienceStage->Spec.Audience.TryCenterAndArrangePortals())) return false;
    Catalog->EntryIndexByDefinitionId.Add(AudienceStage->Spec.FacilityDefinitionId, Catalog->Entries.Num());
    Catalog->Entries.AddDefaulted_GetRef().Definition = AudienceStage;
    Build->SelectedFacilityDefinitionId = AudienceStage->Spec.FacilityDefinitionId;
    Build->SelectedSpec = AudienceStage->Spec;
    Build->PendingQueueSettings = AudienceStage->Spec.QueueSettings;
    TestTrue(TEXT("Other performance facilities retain editable queues beside their audience"),
        Build->SetSelectedFacilityQueueSettings(6, 115.f, 300.f));
    const TArray<FVector> AudienceStageDraft = Build->SelectedSpec.QueueSplinePointsCm;
    Build->RebuildPreviewCommand();
    TestTrue(TEXT("A command rebuild preserves an audience-shifted performance queue draft"),
        AudienceStageDraft == Build->SelectedSpec.QueueSplinePointsCm);
    return true;
}

#endif
