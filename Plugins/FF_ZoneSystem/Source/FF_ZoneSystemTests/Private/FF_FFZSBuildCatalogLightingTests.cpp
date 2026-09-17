#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetBundleData.h"
#include "Components/ChildActorComponent.h"
#include "Components/LightComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationEditorCommon.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityLightingRig.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSSaveGameTestObject.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

namespace FFZSBuildLightingTest
{
    struct FAssetCase
    {
        const TCHAR* Name;
        const TCHAR* DefinitionId;
    };

    const FAssetCase Assets[] = {
        { TEXT("CurtainArch"), TEXT("Lighting_Decorative_CurtainArch") },
        { TEXT("Festoon"), TEXT("Lighting_Decorative_Festoon") },
        { TEXT("GoldenTree"), TEXT("Lighting_Decorative_GoldenTree") },
        { TEXT("RattanPendant"), TEXT("Lighting_Pendant_RattanPendant") },
        { TEXT("StarTree"), TEXT("Lighting_Decorative_StarTree") },
        { TEXT("FestivalStreetlamp"), TEXT("Lighting_Streetlamp_FestivalStreetlamp") }
    };

    struct FInstalledLight
    {
        float Intensity;
        FLinearColor Color;
        FVector MeshLocalPosition;
    };
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FFFZSBuildCatalogLightingTest,
    "FF_ZoneSystem.Lighting.BuildCatalogAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FFFZSBuildCatalogLightingTest::GetTests(TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    for (const FFZSBuildLightingTest::FAssetCase& Asset : FFZSBuildLightingTest::Assets)
    {
        OutBeautifiedNames.Add(Asset.Name);
        OutTestCommands.Add(Asset.Name);
    }
}

bool FFFZSBuildCatalogLightingTest::RunTest(const FString& Parameters)
{
    const FFZSBuildLightingTest::FAssetCase* Asset = nullptr;
    for (const FFZSBuildLightingTest::FAssetCase& Candidate : FFZSBuildLightingTest::Assets)
    {
        if (Parameters == Candidate.Name)
        {
            Asset = &Candidate;
            break;
        }
    }
    if (!TestNotNull(TEXT("Known authored lighting asset"), Asset)) return false;

    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, true);
    TGuardValue<bool> RequireNavigation(Settings->bFailWhenNavigationSystemIsUnavailable, true);
    TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
    TGuardValue<bool> ZoneRestriction(Settings->bRestrictZoneIdsToProjectPresets, false);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Test world"), World)) return false;

    const FString AssetName = FString(TEXT("PDA_FFZS_")) + Asset->DefinitionId;
    const FSoftObjectPath DefinitionPath(FString::Printf(
        TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Lighting/%s.%s"), *AssetName, *AssetName));

    // Inspect serialized metadata before UObject loading can rebuild missing bundles in memory.
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanFilesSynchronous({FPackageName::LongPackageNameToFilename(
        DefinitionPath.GetLongPackageName(), FPackageName::GetAssetPackageExtension())}, true);
    const FAssetData DiskAsset = Registry.GetAssetByObjectPath(DefinitionPath, true);
    if (!TestTrue(TEXT("Saved catalog definition exists"), DiskAsset.IsValid())
        || !TestTrue(TEXT("Definition serializes asset bundles"), DiskAsset.TaggedAssetBundles.IsValid())) return false;
    const FAssetBundleEntry* LightingBundle = DiskAsset.TaggedAssetBundles->Bundles.FindByPredicate(
        [](const FAssetBundleEntry& Entry) { return Entry.BundleName == TEXT("FacilityLighting"); });
    const FAssetBundleEntry* VisualBundle = DiskAsset.TaggedAssetBundles->Bundles.FindByPredicate(
        [](const FAssetBundleEntry& Entry) { return Entry.BundleName == TEXT("FacilityVisual"); });
    if (!TestNotNull(TEXT("Saved FacilityLighting bundle"), LightingBundle)
        || !TestNotNull(TEXT("Saved FacilityVisual bundle"), VisualBundle)) return false;

    UFFZSFacilityDefinition* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr, *DefinitionPath.ToString());
    if (!TestNotNull(TEXT("Authored definition loads"), Definition)) return false;
    const FFFZSFacilitySpec Spec = Definition->Spec;
    TestEqual(TEXT("Stable facility ID"), Spec.FacilityDefinitionId, FName(Asset->DefinitionId));
    TestEqual(TEXT("Definition is in lighting category"), Spec.Category, EFFZSFacilityCategory::Lighting);
    TestTrue(TEXT("Definition is offered in build catalog"), Definition->bShowInBuildCatalog);
    TestFalse(TEXT("Catalog has a display name"), Spec.DisplayName.IsEmpty());
    TestTrue(TEXT("Installation enables authored lights"), Spec.LightingSettings.bEnabled);
    TestTrue(TEXT("Installation has positive brightness"), Spec.LightingSettings.BrightnessMultiplier > 0.f);
    TestTrue(TEXT("Cook and async-load bundle includes rig generated class"),
        LightingBundle->AssetPaths.Contains(Spec.LightingSettings.LightingRigClass.ToSoftObjectPath().GetAssetPath()));
    TestTrue(TEXT("Cook and async-load bundle includes visual mesh"),
        VisualBundle->AssetPaths.Contains(Spec.VisualMesh.ToSoftObjectPath().GetAssetPath()));
    TestTrue(TEXT("Each definition uses its dedicated editable rig"),
        Spec.LightingSettings.LightingRigClass.ToString().EndsWith(
            FString::Printf(TEXT(".BP_FF_%s_Lighting_C"), Asset->Name)));
    UClass* RigClass = Spec.LightingSettings.LightingRigClass.LoadSynchronous();
    UStaticMesh* Mesh = Spec.VisualMesh.LoadSynchronous();
    if (!TestNotNull(TEXT("Lighting rig resolves"), RigClass)
        || !TestNotNull(TEXT("Original authored mesh resolves"), Mesh)) return false;
    TestTrue(TEXT("Rig supports shared facility lighting controls"), RigClass->IsChildOf(AFFZSFacilityLightingRig::StaticClass()));
    const FString ExpectedMeshName = Parameters == TEXT("FestivalStreetlamp")
        ? FString::Printf(TEXT("SM_%s"), Asset->Name)
        : FString::Printf(TEXT("SM_FF_%s"), Asset->Name);
    TestEqual(TEXT("Definition uses requested authored mesh"), Mesh->GetName(), ExpectedMeshName);
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Production definition validates"), Definition->IsDefinitionValid(Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Zone subsystem"), Subsystem) || !TestNotNull(TEXT("Test zone"), Zone)) return false;
    Zone->ZoneId = Spec.AllowedZoneIds.IsEmpty() ? FName(TEXT("GZ_LightingTest")) : Spec.AllowedZoneIds[0];
    Zone->BoundarySpline->ClearSplinePoints(false);
    for (const FVector& Point : { FVector(-100000, -100000, 0), FVector(100000, -100000, 0),
        FVector(100000, 100000, 0), FVector(-100000, 100000, 0) })
    {
        const int32 Index = Zone->BoundarySpline->GetNumberOfSplinePoints();
        Zone->BoundarySpline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    Zone->BoundarySpline->SetClosedLoop(true, true);
    if (!TestTrue(TEXT("Installation zone registers"), Subsystem->RegisterZone(Zone))) return false;
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(10000, 10000);
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
        Definition, Zone->ZoneId, Placement, Subsystem->GetWorldRevision());
    const FFFZSCommandResult InstallResult = Subsystem->ExecuteCommand(Install);
    if (!TestTrue(*FString::Printf(TEXT("Player install succeeds: %s"), *InstallResult.Message.ToString()),
        InstallResult.bSucceeded)) return false;
    AFFZSFacilityActor* Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    AFFZSFacilityLightingRig* Rig = Facility ? Facility->GetLightingRig() : nullptr;
    if (!TestNotNull(TEXT("Installation creates controllable rig"), Rig)) return false;
    TestTrue(TEXT("Installed visual is the selected asset"), Facility->VisualMesh->GetStaticMesh() == Mesh);
    TestTrue(TEXT("Rig attaches to installed visual"), Facility->FacilityLightingComponent->GetAttachParent() == Facility->VisualMesh);
    TestTrue(TEXT("Rig follows final fitted mesh transform"),
        Rig->GetActorTransform().Equals(Facility->VisualMesh->GetComponentTransform(), 0.01));

    TInlineComponentArray<ULightComponent*> Lights(Rig);
    if (!TestTrue(TEXT("Real light components exist"), !Lights.IsEmpty())) return false;
    TestEqual(TEXT("Every lamp participates in facility controls"), Rig->GetAuthoredLightCount(), Lights.Num());
    TInlineComponentArray<UStaticMeshComponent*> PreviewMeshes(Rig);
    for (const UStaticMeshComponent* Preview : PreviewMeshes)
    {
        TestTrue(TEXT("Rig preview meshes are editor-only"), Preview->IsEditorOnly());
        TestTrue(TEXT("Rig previews do not duplicate the gameplay mesh"), Preview->bHiddenInGame);
    }
    TMap<FName, FFZSBuildLightingTest::FInstalledLight> InstalledLights;
    bool bIlluminates = false;
    for (const ULightComponent* Light : Lights)
    {
        InstalledLights.Add(Light->GetFName(), {Light->Intensity, Light->GetLightColor(),
            Facility->VisualMesh->GetComponentTransform().InverseTransformPosition(Light->GetComponentLocation())});
        bIlluminates |= Light->Intensity > 0.f && Light->IsVisible();
    }
    TestTrue(TEXT("Installed asset actually illuminates"), bIlluminates);

    FText Reason;
    FFFZSFacilityLightingState State = Facility->GetLightingState();
    State.BrightnessMultiplier = Spec.LightingSettings.BrightnessMultiplier * 0.5f;
    State.bOverrideColor = true;
    State.Color = FLinearColor(0.04f, 0.3f, 1.f);
    if (!TestTrue(TEXT("Selected facility accepts brightness and color"),
        Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason))) return false;
    TestTrue(TEXT("Repeating a slider value succeeds"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    for (const ULightComponent* Light : Lights)
    {
        const FFZSBuildLightingTest::FInstalledLight& Initial = InstalledLights.FindChecked(Light->GetFName());
        TestTrue(TEXT("Brightness changes every lamp without compounding"), FMath::IsNearlyEqual(Light->Intensity, Initial.Intensity * 0.5f));
        TestTrue(TEXT("Color control reaches every lamp"), Light->GetLightColor().Equals(State.Color, 0.01f));
    }

    FFFZSCommand Move;
    Move.CommandId = FGuid::NewGuid();
    Move.TransactionId = FGuid::NewGuid();
    Move.Type = EFFZSCommandType::Move;
    Move.FacilityInstanceId = Install.FacilityInstanceId;
    Move.ZoneId = Zone->ZoneId;
    Move.Placement.OriginCell = FIntPoint(12000, 14000);
    Move.Placement.YawCentidegrees = 3700;
    Move.ExpectedWorldRevision = Subsystem->GetWorldRevision();
    const FFFZSCommandResult MoveResult = Subsystem->ExecuteCommand(Move);
    if (!TestTrue(*FString::Printf(TEXT("Move and rotate installed light: %s"), *MoveResult.Message.ToString()),
        MoveResult.bSucceeded)) return false;
    TestTrue(TEXT("Move reuses the single controlled rig"), Facility->GetLightingRig() == Rig);
    for (const ULightComponent* Light : Lights)
    {
        const FFZSBuildLightingTest::FInstalledLight& Initial = InstalledLights.FindChecked(Light->GetFName());
        TestTrue(TEXT("Moved lamp remains aligned with its fixture"), Light->GetComponentLocation().Equals(
            Facility->VisualMesh->GetComponentTransform().TransformPosition(Initial.MeshLocalPosition), 0.01));
        TestTrue(TEXT("Move preserves brightness override"), FMath::IsNearlyEqual(Light->Intensity, Initial.Intensity * 0.5f));
    }

    FFFZSCommand Open;
    Open.CommandId = FGuid::NewGuid();
    Open.TransactionId = FGuid::NewGuid();
    Open.Type = EFFZSCommandType::Open;
    Open.FacilityInstanceId = Install.FacilityInstanceId;
    Open.ExpectedWorldRevision = Subsystem->GetWorldRevision();
    if (!TestTrue(TEXT("Lighting facility supports the normal opening lifecycle"),
        Subsystem->ExecuteCommand(Open).bSucceeded)) return false;
    TestEqual(TEXT("Opened lighting facility is active"), Facility->GetFacilityState(), EFFZSFacilityState::Active);
    TestEqual(TEXT("Active lighting does not open a customer queue"),
        Facility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Closed);

    State.bEnabled = false;
    TestTrue(TEXT("Player can switch the asset off"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    for (const ULightComponent* Light : Lights)
    {
        TestEqual(TEXT("Off extinguishes every lamp"), Light->Intensity, 0.f);
        TestFalse(TEXT("Off hides every light component"), Light->IsVisible());
    }
    UFFZSSaveGameTestObject* Save = NewObject<UFFZSSaveGameTestObject>();
    Save->Snapshot = Subsystem->CaptureWorldSnapshot();
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Authored lighting state writes through SaveGame archive"),
        UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    UFFZSSaveGameTestObject* Loaded = Cast<UFFZSSaveGameTestObject>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Lighting save archive reads"), Loaded)) return false;
    if (!TestEqual(TEXT("Save contains one installed lighting asset"), Loaded->Snapshot.Facilities.Num(), 1)) return false;
    const FFFZSFacilityInstanceSnapshot& Saved = Loaded->Snapshot.Facilities[0];
    TestTrue(TEXT("Instance override flag persists"), Saved.bHasLightingOverride);
    TestFalse(TEXT("Off switch persists"), Saved.LightingState.bEnabled);
    TestTrue(TEXT("Color override flag persists"), Saved.LightingState.bOverrideColor);
    TestTrue(TEXT("Color persists"), Saved.LightingState.Color.Equals(State.Color));
    TestEqual(TEXT("Brightness persists"), Saved.LightingState.BrightnessMultiplier, State.BrightnessMultiplier);
    TestTrue(TEXT("Rig reference persists"), Saved.FacilitySpec.LightingSettings.LightingRigClass == Spec.LightingSettings.LightingRigClass);
    TestTrue(TEXT("Moved and rotated placement persists"), Saved.Placement == Move.Placement);
    TestEqual(TEXT("Active lighting lifecycle persists"), Saved.State, EFFZSFacilityState::Active);
    Errors.Reset();
    if (!TestTrue(TEXT("Saved installed lighting restores through world API"), Subsystem->RestoreWorldSnapshot(Loaded->Snapshot, Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    Rig = Facility ? Facility->GetLightingRig() : nullptr;
    if (!TestNotNull(TEXT("Restore creates the authored controllable rig"), Rig)) return false;
    TInlineComponentArray<ULightComponent*> RestoredLights(Rig);
    TestEqual(TEXT("Restore preserves every lamp"), RestoredLights.Num(), InstalledLights.Num());
    TestEqual(TEXT("Restored lighting retains active lifecycle"), Facility->GetFacilityState(), EFFZSFacilityState::Active);
    TestEqual(TEXT("Restored active lighting still has no customer queue"),
        Facility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("Restored facility retains off state"), Facility->GetLightingState().bEnabled);
    for (const ULightComponent* Light : RestoredLights) TestEqual(TEXT("Restored off lights remain dark"), Light->Intensity, 0.f);
    State.bEnabled = true;
    TestTrue(TEXT("Restored asset can be switched on"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    for (const ULightComponent* Light : RestoredLights)
    {
        const FFZSBuildLightingTest::FInstalledLight* Initial = InstalledLights.Find(Light->GetFName());
        if (!TestNotNull(TEXT("Restored lamp keeps authored identity"), Initial)) continue;
        TestTrue(TEXT("Restored lamp applies saved brightness"), FMath::IsNearlyEqual(Light->Intensity, Initial->Intensity * 0.5f));
        TestTrue(TEXT("Restored lamp applies saved color"), Light->GetLightColor().Equals(State.Color, 0.01f));
        TestTrue(TEXT("Restored lamp remains fitted to moved mesh"), Light->GetComponentLocation().Equals(
            Facility->VisualMesh->GetComponentTransform().TransformPosition(Initial->MeshLocalPosition), 0.01));
    }
    TestTrue(TEXT("Player can reset restored asset to its authored settings"),
        Subsystem->ResetFacilityLighting(Install.FacilityInstanceId, Reason));
    TestFalse(TEXT("Reset clears persisted instance override"), Facility->CaptureInstanceSnapshot().bHasLightingOverride);
    for (const ULightComponent* Light : RestoredLights)
    {
        const FFZSBuildLightingTest::FInstalledLight* Initial = InstalledLights.Find(Light->GetFName());
        if (!Initial) continue;
        TestTrue(TEXT("Reset restores authored lamp intensity"), FMath::IsNearlyEqual(Light->Intensity, Initial->Intensity));
        TestTrue(TEXT("Reset restores authored individual lamp color"), Light->GetLightColor().Equals(Initial->Color, 0.01f));
    }

    FFFZSCommand Close;
    Close.CommandId = FGuid::NewGuid();
    Close.TransactionId = FGuid::NewGuid();
    Close.Type = EFFZSCommandType::Close;
    Close.FacilityInstanceId = Install.FacilityInstanceId;
    Close.ExpectedWorldRevision = Subsystem->GetWorldRevision();
    TestTrue(TEXT("Lighting closes without waiting for nonexistent customers"), Subsystem->ExecuteCommand(Close).bSucceeded);

    FFFZSCommand Remove;
    Remove.CommandId = FGuid::NewGuid();
    Remove.TransactionId = FGuid::NewGuid();
    Remove.Type = EFFZSCommandType::Remove;
    Remove.FacilityInstanceId = Install.FacilityInstanceId;
    Remove.ExpectedWorldRevision = Subsystem->GetWorldRevision();
    TestTrue(TEXT("Player removes installed lighting asset"), Subsystem->ExecuteCommand(Remove).bSucceeded);
    TestNull(TEXT("Removed asset is no longer selectable"), Subsystem->FindFacility(Install.FacilityInstanceId));
    TestTrue(TEXT("Removal destroys the lighting rig"), !IsValid(Rig) || Rig->IsActorBeingDestroyed());
    TestFalse(TEXT("Stale selection cannot change removed lights"),
        Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    return true;
}

#endif
