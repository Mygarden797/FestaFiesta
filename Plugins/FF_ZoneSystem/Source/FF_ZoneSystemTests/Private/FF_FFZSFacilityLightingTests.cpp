#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/ChildActorComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSLightingTestRig.h"
#include "FF_FFZSSaveGameTestObject.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSFacilityLightingLifecycleTest,
    "FF_ZoneSystem.Lighting.LifecycleScaleAndSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityLightingLifecycleTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("World"), World) || !TestNotNull(TEXT("Mesh"), Mesh)) return false;
    FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
    Spec.VisualMesh = Mesh;
    Spec.VisualSizingMode = EFFZSVisualSizingMode::SourceMeshSize;
    Spec.VisualRotation = FRotator(0, 90, 0);
    Spec.VisualOffsetCm = FVector(4, -7, 12);
    Spec.LightingSettings.LightingRigClass = AFFZSLightingTestRig::StaticClass();
    Spec.LightingSettings.BrightnessMultiplier = 1.5f;
    AFFZSFacilityActor* Actor = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestTrue(TEXT("Initialize lit facility"), Actor && Actor->InitializeFacility(
        FGuid::NewGuid(), Spec, TEXT("GZ_DadaPocha"), FFFZSGridPlacement(), FTransform::Identity))) return false;
    AFFZSLightingTestRig* Rig = Cast<AFFZSLightingTestRig>(Actor->GetLightingRig());
    if (!TestNotNull(TEXT("Configured child rig is created"), Rig)) return false;
    TestTrue(TEXT("Lighting child attaches to mesh"), Actor->FacilityLightingComponent->GetAttachParent() == Actor->VisualMesh);
    TestEqual(TEXT("Install automatically uses PDA brightness"), Rig->TestLight->Intensity, 1500.f);
    const FLinearColor AuthoredColor = Rig->TestLight->GetLightColor();
    TestTrue(TEXT("Light follows original mesh-space position and rotation"), Rig->TestLight->GetComponentLocation().Equals(
        Actor->VisualMesh->GetComponentTransform().TransformPosition(FVector(17, -31, 93)), 0.01));

    FFFZSFacilityLightingState State = Actor->GetLightingState();
    State.BrightnessMultiplier = 2.f;
    State.bOverrideColor = true;
    State.Color = FLinearColor(1.f, 0.1f, 0.02f);
    Actor->ApplyFacilityLightingOverride(State, true);
    Actor->ApplyFacilityLightingOverride(State, true);
    TestEqual(TEXT("Repeated multiplier never compounds"), Rig->TestLight->Intensity, 2000.f);
    TestTrue(TEXT("Linear color survives light FColor encoding"), Rig->TestLight->GetLightColor().Equals(State.Color, 0.01f));
    State.bOverrideColor = false;
    Actor->ApplyFacilityLightingOverride(State, true);
    TestTrue(TEXT("Authored per-light color returns"), Rig->TestLight->GetLightColor().Equals(AuthoredColor, 0.01f));

    Actor->SetActorHiddenInGame(true);
    TestEqual(TEXT("Hidden restore staging has no illumination"), Rig->TestLight->Intensity, 0.f);
    Actor->SetActorHiddenInGame(false);
    TestEqual(TEXT("Publishing staging restores brightness"), Rig->TestLight->Intensity, 2000.f);
    TestTrue(TEXT("Size upgrade succeeds"), Actor->ApplyUpgradeState(2, 1.f, 2.f));
    TestTrue(TEXT("Upgrade reuses one rig"), Actor->GetLightingRig() == Rig);
    TestEqual(TEXT("Range scales from authored radius"), Rig->TestLight->AttenuationRadius, 800.f);
    TestTrue(TEXT("Scaled light remains on matching fixture"), Rig->TestLight->GetComponentLocation().Equals(
        Actor->VisualMesh->GetComponentTransform().TransformPosition(FVector(17, -31, 93)), 0.01));
    TestTrue(TEXT("Move succeeds"), Actor->ApplyPlacement(TEXT("GZ_DadaPocha"), FFFZSGridPlacement(),
        FTransform(FRotator(0, 38, 0), FVector(500, 250, 0))));
    TestTrue(TEXT("Move reuses one rig"), Actor->GetLightingRig() == Rig);
    TestEqual(TEXT("Move does not double attenuation"), Rig->TestLight->AttenuationRadius, 800.f);

    UFFZSSaveGameTestObject* Save = NewObject<UFFZSSaveGameTestObject>();
    Save->Snapshot.Facilities.Add(Actor->CaptureInstanceSnapshot());
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Snapshot SaveGame archive writes"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    UFFZSSaveGameTestObject* Loaded = Cast<UFFZSSaveGameTestObject>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Snapshot archive reads"), Loaded)) return false;
    const FFFZSFacilityInstanceSnapshot& Saved = Loaded->Snapshot.Facilities[0];
    TestTrue(TEXT("Override flag persists"), Saved.bHasLightingOverride);
    TestEqual(TEXT("Instance brightness persists"), Saved.LightingState.BrightnessMultiplier, 2.f);
    TestEqual(TEXT("PDA baseline persists independently"), Saved.FacilitySpec.LightingSettings.BrightnessMultiplier, 1.5f);
    TestTrue(TEXT("Rig class soft path persists"), Saved.FacilitySpec.LightingSettings.LightingRigClass == Spec.LightingSettings.LightingRigClass);
    AFFZSFacilityActor* Restored = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestTrue(TEXT("Initialize restored actor"), Restored && Restored->InitializeFacility(Saved.FacilityInstanceId,
        Saved.FacilitySpec, Saved.ZoneId, Saved.Placement, Saved.WorldTransform, Saved.State, Saved.UpgradeLevel,
        Saved.UpgradeServiceDurationMultiplier, Saved.UpgradeSizeMultiplier, Saved.UpgradeRevenueMultiplier))) return false;
    Restored->ApplyFacilityLightingOverride(Saved.LightingState, Saved.bHasLightingOverride);
    AFFZSLightingTestRig* RestoredRig = Cast<AFFZSLightingTestRig>(Restored->GetLightingRig());
    if (!TestNotNull(TEXT("Restored rig"), RestoredRig)) return false;
    TestEqual(TEXT("Restored light has instance brightness"), RestoredRig->TestLight->Intensity, 2000.f);
    Restored->ApplyFacilityLightingOverride(FFFZSFacilityLightingState(), false);
    TestEqual(TEXT("Legacy absent override returns captured PDA defaults"), RestoredRig->TestLight->Intensity, 1500.f);

    const uint32 OldGeneration = Actor->LightingLoadGeneration;
    Actor->MarkRemoved();
    Actor->ApplyLoadedFacilityLighting(OldGeneration);
    TestNull(TEXT("Removal destroys rig; late async callback cannot respawn"), Actor->GetLightingRig());
    Restored->Destroy();
    Actor->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSFacilityLightingValidationTest,
    "FF_ZoneSystem.Lighting.ValidationAndCommandFingerprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityLightingValidationTest::RunTest(const FString& Parameters)
{
    FFFZSFacilityLightingState State;
    TestTrue(TEXT("Default light state valid"), State.IsValid());
    State.BrightnessMultiplier = -0.1f;
    TestFalse(TEXT("Negative brightness rejected"), State.IsValid());
    State.BrightnessMultiplier = 5.01f;
    TestFalse(TEXT("Excessive brightness rejected"), State.IsValid());
    State.BrightnessMultiplier = 2.f;
    State.Color.R = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite color rejected"), State.IsValid());
    FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(FFZSTest::MakeValidFacilitySpec(),
        TEXT("GZ_DadaPocha"), FFFZSGridPlacement(), 0);
    const FString DefaultFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    TestFalse(TEXT("Legacy default fingerprint has no lighting extension"), DefaultFingerprint.Contains(TEXT("spec.lighting.")));
    Command.FacilitySpec.LightingSettings.LightingRigClass = AFFZSLightingTestRig::StaticClass();
    const FString RigFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    TestNotEqual(TEXT("Rig reference participates in canonical command identity"), RigFingerprint, DefaultFingerprint);
    Command.FacilitySpec.LightingSettings.BrightnessMultiplier = 2.f;
    TestNotEqual(TEXT("PDA brightness participates in command identity"), UFFZSGridLibrary::BuildCommandFingerprint(Command), RigFingerprint);
    return true;
}

#endif
