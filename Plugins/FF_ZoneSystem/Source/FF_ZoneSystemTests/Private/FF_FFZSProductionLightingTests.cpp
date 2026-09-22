#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetBundleData.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/LightComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityLightingRig.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSProductionLightingTest,
    "FF_ZoneSystem.Lighting.ProductionMainStageInstallation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSProductionLightingTest::RunTest(const FString& Parameters)
{
    // UE5.7 moves AssetBundleData out of ordinary tags into TaggedAssetBundles.
    // Force disk metadata refresh before loading the UObject so PostLoad cannot repair a missing saved bundle.
    const FSoftObjectPath DefinitionPath(TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage.PDA_FFZS_Performance_MainStage_SunsetMainStage"));
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanFilesSynchronous({FPackageName::LongPackageNameToFilename(
        DefinitionPath.GetLongPackageName(), FPackageName::GetAssetPackageExtension())}, true);
    const FAssetData DiskAsset = Registry.GetAssetByObjectPath(DefinitionPath, true);
    if (!TestTrue(TEXT("Saved PDA registry entry exists"), DiskAsset.IsValid())
        || !TestTrue(TEXT("Saved PDA has typed asset bundles"), DiskAsset.TaggedAssetBundles.IsValid())) return false;
    const FAssetBundleEntry* LightingBundle = DiskAsset.TaggedAssetBundles->Bundles.FindByPredicate(
        [](const FAssetBundleEntry& Entry) { return Entry.BundleName == TEXT("FacilityLighting"); });
    if (!TestNotNull(TEXT("FacilityLighting bundle is serialized on disk"), LightingBundle)) return false;

    UFFZSFacilityDefinition* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage.PDA_FFZS_Performance_MainStage_SunsetMainStage"));
    if (!TestNotNull(TEXT("Production MainStage PDA exists"), Definition)) return false;
    const FFFZSFacilitySpec Spec = Definition->Spec;
    TestTrue(TEXT("Saved lighting bundle contains the exact generated rig class for cooking and async loading"),
        LightingBundle->AssetPaths.Contains(Spec.LightingSettings.LightingRigClass.ToSoftObjectPath().GetAssetPath()));
    if (!TestNotNull(TEXT("PDA rig class resolves"), Spec.LightingSettings.LightingRigClass.LoadSynchronous())
        || !TestNotNull(TEXT("PDA mesh resolves"), Spec.VisualMesh.LoadSynchronous())) return false;
    TestTrue(TEXT("PDA specifies dedicated editable lighting BP"), Spec.LightingSettings.LightingRigClass.ToString().Contains(TEXT("BP_MainStage_Grand_Lighting")));
    TestTrue(TEXT("PDA automatically enables lights"), Spec.LightingSettings.bEnabled);

    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
    TGuardValue<bool> ZoneRestriction(Settings->bRestrictZoneIdsToProjectPresets, false);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
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
    if (!TestTrue(TEXT("Test zone registers"), Subsystem && Subsystem->RegisterZone(Zone))) return false;
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(10000, 10000);
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, Placement, Subsystem->GetWorldRevision());
    const FFFZSCommandResult Result = Subsystem->ExecuteCommand(Install);
    if (!TestTrue(*FString::Printf(TEXT("Production install: %s"), *Result.Message.ToString()), Result.bSucceeded)) return false;
    AFFZSFacilityActor* Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    AFFZSFacilityLightingRig* Rig = Facility ? Facility->GetLightingRig() : nullptr;
    if (!TestNotNull(TEXT("Production install creates lighting rig"), Rig)) return false;
    TInlineComponentArray<ULightComponent*> Lights(Rig);
    // The artist's lighting revision 67483cfb removed Overhead_08_Spot from the
    // dedicated runtime rig. The original preview Blueprint still has ten lamps.
    // Check the authored identities as well as the count so another missing lamp
    // cannot be masked by an unrelated replacement.
    const TArray<FName> ExpectedLightNames = {
        TEXT("Overhead_00_Spot"), TEXT("Overhead_02_Spot"), TEXT("Overhead_04_Spot"),
        TEXT("Overhead_06_Spot"), TEXT("Overhead_10_Spot"),
        TEXT("DeckSpot_Spot"), TEXT("DeckSpot_Spot_001"),
        TEXT("RunwaySpot_Spot"), TEXT("RunwaySpot_Spot_001")
    };
    TestEqual(TEXT("All nine authored runtime lamps exist"), Lights.Num(), ExpectedLightNames.Num());
    for (const FName ExpectedName : ExpectedLightNames)
    {
        TestTrue(*FString::Printf(TEXT("Authored runtime lamp exists: %s"), *ExpectedName.ToString()),
            Lights.ContainsByPredicate([ExpectedName](const ULightComponent* Light)
                { return Light && Light->GetFName() == ExpectedName; }));
    }
    if (Lights.IsEmpty()) return false;
    TestTrue(TEXT("Rig uses final fitted mesh transform"), Rig->GetActorTransform().Equals(Facility->VisualMesh->GetComponentTransform(), 0.01));
    TInlineComponentArray<UStaticMeshComponent*> PreviewMeshes(Rig);
    for (const UStaticMeshComponent* Preview : PreviewMeshes)
    {
        TestTrue(TEXT("Authoring preview is editor-only"), Preview->IsEditorOnly());
        TestTrue(TEXT("Authoring preview hidden during gameplay"), Preview->bHiddenInGame);
    }
    const float InstalledIntensity = Lights[0]->Intensity;
    TestTrue(TEXT("Install actually illuminates"), InstalledIntensity > 0.f && Lights[0]->IsVisible());
    FFFZSFacilityLightingState State = Facility->GetLightingState();
    State.BrightnessMultiplier = Spec.LightingSettings.BrightnessMultiplier * 0.5f;
    FText Reason;
    TestTrue(TEXT("World API adjusts selected facility"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    TestTrue(TEXT("Actual light becomes half as bright"), FMath::IsNearlyEqual(Lights[0]->Intensity, InstalledIntensity * 0.5f));
    State.bEnabled = false;
    TestTrue(TEXT("World API turns lights off"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    TestEqual(TEXT("Actual lamp off"), Lights[0]->Intensity, 0.f);
    TestTrue(TEXT("Reset supported"), Subsystem->ResetFacilityLighting(Install.FacilityInstanceId, Reason));
    TestTrue(TEXT("Reset restores exact installed PDA brightness"), FMath::IsNearlyEqual(Lights[0]->Intensity, InstalledIntensity));
    State.bEnabled = true;
    State.bOverrideColor = true;
    State.Color = FLinearColor(1.f, 0.25f, 0.06f);
    TestTrue(TEXT("Color and brightness override applies"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    const FFFZSWorldSnapshot Snapshot = Subsystem->CaptureWorldSnapshot();
    TArray<FText> Errors;
    if (!TestTrue(TEXT("World snapshot restores lighting override"), Subsystem->RestoreWorldSnapshot(Snapshot, Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    Rig = Facility ? Facility->GetLightingRig() : nullptr;
    if (!TestNotNull(TEXT("Restored production rig"), Rig)) return false;
    TestTrue(TEXT("Restored override color"), Facility->GetLightingState().Color.Equals(State.Color));
    TestEqual(TEXT("Restored override brightness"), Facility->GetLightingState().BrightnessMultiplier, State.BrightnessMultiplier);
    FFFZSCommand Remove;
    Remove.CommandId = FGuid::NewGuid();
    Remove.TransactionId = FGuid::NewGuid();
    Remove.Type = EFFZSCommandType::Remove;
    Remove.FacilityInstanceId = Install.FacilityInstanceId;
    Remove.ExpectedWorldRevision = Subsystem->GetWorldRevision();
    TestTrue(TEXT("Production facility removal succeeds"), Subsystem->ExecuteCommand(Remove).bSucceeded);
    TestNull(TEXT("Removal unregisters facility"), Subsystem->FindFacility(Install.FacilityInstanceId));
    TestTrue(TEXT("Removal destroys owned lighting actor"), !IsValid(Rig) || Rig->IsActorBeingDestroyed());
    TestFalse(TEXT("Removed facility cannot be illuminated by stale UI"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    return true;
}

#endif
