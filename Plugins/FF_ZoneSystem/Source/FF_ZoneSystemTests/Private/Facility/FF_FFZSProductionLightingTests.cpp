// FF_FFZSProductionLightingTests.cpp
// 역할: 프로젝트에 배포되는 시설 조명 에셋과 런타임 적용 결과를 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetBundleData.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/LightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FF_FFZSAudienceLibrary.h"
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
    // Keep the nine authored stage fixtures and the two audience washes. Check
    // identities too, so an unrelated replacement cannot mask a missing lamp.
    const TArray<FName> ExpectedLightNames = {
        TEXT("Overhead_00_Spot"), TEXT("Overhead_02_Spot"), TEXT("Overhead_04_Spot"),
        TEXT("Overhead_06_Spot"), TEXT("Overhead_10_Spot"),
        TEXT("DeckSpot_Spot"), TEXT("DeckSpot_Spot_001"),
        TEXT("RunwaySpot_Spot"), TEXT("RunwaySpot_Spot_001"),
        TEXT("AudienceWash_Left_Spot"), TEXT("AudienceWash_Right_Spot")
    };
    TestEqual(TEXT("All eleven authored runtime lamps exist"), Lights.Num(), ExpectedLightNames.Num());
    for (const FName ExpectedName : ExpectedLightNames)
    {
        TestTrue(*FString::Printf(TEXT("Authored runtime lamp exists: %s"), *ExpectedName.ToString()),
            Lights.ContainsByPredicate([ExpectedName](const ULightComponent* Light)
                { return Light && Light->GetFName() == ExpectedName; }));
    }
    if (Lights.IsEmpty()) return false;
    TestTrue(TEXT("Rig uses final fitted mesh transform"), Rig->GetActorTransform().Equals(Facility->VisualMesh->GetComponentTransform(), 0.01));

    bool bReportedCoverageDiagnostics = false;
    const auto CheckAudienceCoverage = [&](const TCHAR* Phase)
    {
        FFFZSAudienceSnapshot Audience;
        FText AudienceError;
        // Use the actual installed audience layout and actor transform. The rig
        // inherits the fitted mesh transform, which is a different coordinate space.
        if (!TestTrue(*FString::Printf(TEXT("%s: installed audience layout resolves"), Phase),
            UFFZSAudienceLibrary::BuildSnapshot(Facility->GetFacilitySpec().Audience,
                Facility->GetActorTransform(), Audience, AudienceError))) return false;
        TestEqual(*FString::Printf(TEXT("%s: production audience has 500 places"), Phase), Audience.Capacity, 500);
        TArray<const USpotLightComponent*> AudienceLights;
        TInlineComponentArray<USpotLightComponent*> Spots(Rig);
        for (const USpotLightComponent* Spot : Spots)
        {
            if (Spot->GetFName() == TEXT("AudienceWash_Left_Spot")
                || Spot->GetFName() == TEXT("AudienceWash_Right_Spot")) AudienceLights.Add(Spot);
        }
        if (!TestEqual(*FString::Printf(TEXT("%s: both audience washes are spotlights"), Phase),
            AudienceLights.Num(), 2)) return false;
        int32 UncoveredSamples = 0;
        FString FirstUncovered;
        for (int32 SlotIndex = 0; SlotIndex < Audience.Capacity; ++SlotIndex)
        {
            FTransform Slot;
            if (!TestTrue(*FString::Printf(TEXT("%s: audience slot %d resolves"), Phase, SlotIndex),
                UFFZSAudienceLibrary::GetAudienceSlotTransform(Audience, SlotIndex, Slot))) return false;
            for (const double HeightCm : { 120.0, 170.0 })
            {
                const FVector Sample = Slot.GetLocation() + FVector::UpVector * HeightCm;
                const bool bCovered = AudienceLights.ContainsByPredicate([&](const USpotLightComponent* Spot)
                {
                    const FVector ToSample = Sample - Spot->GetComponentLocation();
                    // Require the full-strength cone and a 15% range margin, not
                    // merely a point just inside the effectively dark outer edge.
                    return Spot->bAffectsWorld && Spot->IsVisible() && Spot->Intensity > 0.f
                        && ToSample.SizeSquared() < FMath::Square(Spot->AttenuationRadius * 0.85)
                        && FVector::DotProduct(Spot->GetForwardVector(), ToSample.GetSafeNormal())
                            >= FMath::Cos(FMath::DegreesToRadians(Spot->InnerConeAngle));
                });
                if (!bCovered)
                {
                    ++UncoveredSamples;
                    if (FirstUncovered.IsEmpty()) FirstUncovered = FString::Printf(
                        TEXT("slot %d, height %.0f cm, world %s"), SlotIndex, HeightCm, *Sample.ToString());
                    if (!bReportedCoverageDiagnostics)
                    {
                        bReportedCoverageDiagnostics = true;
                        AddInfo(FString::Printf(TEXT("%s audience coverage diagnostics: facility world=%s; mesh relative=%s; rig world=%s; first missed sample=%s"),
                            Phase, *Facility->GetActorTransform().ToString(),
                            *Facility->VisualMesh->GetRelativeTransform().ToString(),
                            *Rig->GetActorTransform().ToString(), *Sample.ToString()));
                        for (const USpotLightComponent* Spot : AudienceLights)
                        {
                            const FVector ToSample = Sample - Spot->GetComponentLocation();
                            const double Dot = FVector::DotProduct(Spot->GetForwardVector(), ToSample.GetSafeNormal());
                            AddInfo(FString::Printf(TEXT("Audience lamp %s: parent=%s; relative=%s; world=%s; forward=%s; inner=%.3f deg; outer=%.3f deg; radius=%.3f cm; intensity=%.3f; visible=%d; affectsWorld=%d; first sample angle=%.3f deg; distance=%.3f cm"),
                                *Spot->GetName(), *GetNameSafe(Spot->GetAttachParent()),
                                *Spot->GetRelativeTransform().ToString(), *Spot->GetComponentTransform().ToString(),
                                *Spot->GetForwardVector().ToString(), Spot->InnerConeAngle, Spot->OuterConeAngle,
                                Spot->AttenuationRadius, Spot->Intensity, Spot->IsVisible(), Spot->bAffectsWorld,
                                FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0))), ToSample.Size()));
                        }
                    }
                }
            }
        }
        return TestEqual(*FString::Printf(TEXT("%s: every torso/head is inside a lit inner cone with range margin; first miss: %s"),
            Phase, *FirstUncovered), UncoveredSamples, 0);
    };
    CheckAudienceCoverage(TEXT("Installed"));

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
    if (!TestTrue(*FString::Printf(TEXT("Production stage moves and rotates: %s"), *MoveResult.Message.ToString()),
        MoveResult.bSucceeded)) return false;
    TestTrue(TEXT("Moving the stage retains its controlled lighting rig"), Facility->GetLightingRig() == Rig);
    CheckAudienceCoverage(TEXT("Moved and rotated"));
    TInlineComponentArray<UStaticMeshComponent*> PreviewMeshes(Rig);
    for (const UStaticMeshComponent* Preview : PreviewMeshes)
    {
        TestTrue(TEXT("Authoring preview is editor-only"), Preview->IsEditorOnly());
        TestTrue(TEXT("Authoring preview hidden during gameplay"), Preview->bHiddenInGame);
    }
    TMap<FName, float> InstalledIntensities;
    TMap<FName, FLinearColor> InstalledColors;
    for (const ULightComponent* Light : Lights)
    {
        InstalledIntensities.Add(Light->GetFName(), Light->Intensity);
        InstalledColors.Add(Light->GetFName(), Light->GetLightColor());
        TestTrue(*FString::Printf(TEXT("Installed lamp illuminates: %s"), *Light->GetName()),
            Light->Intensity > 0.f && Light->IsVisible());
    }
    FFFZSFacilityLightingState State = Facility->GetLightingState();
    State.BrightnessMultiplier = Spec.LightingSettings.BrightnessMultiplier * 0.5f;
    FText Reason;
    TestTrue(TEXT("World API adjusts selected facility"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    for (const ULightComponent* Light : Lights)
    {
        TestTrue(*FString::Printf(TEXT("Lamp becomes half as bright: %s"), *Light->GetName()),
            FMath::IsNearlyEqual(Light->Intensity, InstalledIntensities.FindChecked(Light->GetFName()) * 0.5f));
    }
    State.bEnabled = false;
    TestTrue(TEXT("World API turns lights off"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    for (const ULightComponent* Light : Lights)
    {
        TestEqual(*FString::Printf(TEXT("Lamp intensity off: %s"), *Light->GetName()), Light->Intensity, 0.f);
        TestFalse(*FString::Printf(TEXT("Lamp visibility off: %s"), *Light->GetName()), Light->IsVisible());
    }
    TestTrue(TEXT("Reset supported"), Subsystem->ResetFacilityLighting(Install.FacilityInstanceId, Reason));
    for (const ULightComponent* Light : Lights)
    {
        TestTrue(*FString::Printf(TEXT("Reset restores installed brightness: %s"), *Light->GetName()),
            Light->IsVisible() && FMath::IsNearlyEqual(Light->Intensity, InstalledIntensities.FindChecked(Light->GetFName())));
    }
    State.bEnabled = true;
    State.bOverrideColor = true;
    State.Color = FLinearColor(1.f, 0.25f, 0.06f);
    TestTrue(TEXT("Color and brightness override applies"), Subsystem->SetFacilityLighting(Install.FacilityInstanceId, State, Reason));
    for (const ULightComponent* Light : Lights)
    {
        TestTrue(*FString::Printf(TEXT("Color override reaches lamp: %s"), *Light->GetName()),
            Light->GetLightColor().Equals(State.Color, 0.01f));
    }
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
    TInlineComponentArray<ULightComponent*> RestoredLights(Rig);
    TestEqual(TEXT("Restored rig retains every lamp"), RestoredLights.Num(), InstalledIntensities.Num());
    for (const ULightComponent* Light : RestoredLights)
    {
        const float* InitialIntensity = InstalledIntensities.Find(Light->GetFName());
        if (!TestNotNull(*FString::Printf(TEXT("Restored lamp retains authored identity: %s"), *Light->GetName()),
            InitialIntensity)) continue;
        TestTrue(*FString::Printf(TEXT("Restored lamp has saved brightness: %s"), *Light->GetName()),
            Light->IsVisible() && FMath::IsNearlyEqual(Light->Intensity, *InitialIntensity * 0.5f));
        TestTrue(*FString::Printf(TEXT("Restored lamp has saved color: %s"), *Light->GetName()),
            Light->GetLightColor().Equals(State.Color, 0.01f));
    }
    CheckAudienceCoverage(TEXT("Restored"));
    TestTrue(TEXT("Restored production lighting can reset"), Subsystem->ResetFacilityLighting(Install.FacilityInstanceId, Reason));
    for (const ULightComponent* Light : RestoredLights)
    {
        const FLinearColor* InitialColor = InstalledColors.Find(Light->GetFName());
        if (InitialColor) TestTrue(*FString::Printf(TEXT("Reset restores authored lamp color: %s"), *Light->GetName()),
            Light->GetLightColor().Equals(*InitialColor, 0.01f));
    }
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
