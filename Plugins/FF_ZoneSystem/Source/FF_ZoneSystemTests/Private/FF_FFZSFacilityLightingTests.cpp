// FF_FFZSFacilityLightingTests.cpp
// 역할: 시설에 조명 리그가 생성되고 운영 상태에 맞게 적용되는지 검증한다.
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
#include "Materials/Material.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceDynamic.h"
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
    TestTrue(TEXT("Install preserves authored lamp temperature"), Rig->TestLight->bUseTemperature);
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
    TestFalse(TEXT("Custom color is not tinted by authored temperature"), Rig->TestLight->bUseTemperature);
    State.bOverrideColor = false;
    Actor->ApplyFacilityLightingOverride(State, true);
    TestTrue(TEXT("Authored per-light color returns"), Rig->TestLight->GetLightColor().Equals(AuthoredColor, 0.01f));
    TestTrue(TEXT("Original color restores authored temperature mode"), Rig->TestLight->bUseTemperature);
    TestEqual(TEXT("Authored warm temperature remains unchanged"), Rig->TestLight->Temperature, 2700.0f);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSFacilityLightingEmissiveTest,
    "FF_ZoneSystem.Lighting.EmissiveIsolationAndMaterialReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityLightingEmissiveTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("World"), World) || !TestNotNull(TEXT("Cube"), Cube)) return false;
    UMaterial* Source = NewObject<UMaterial>();
    UMaterialExpressionScalarParameter* BrightnessExpression = nullptr;
    UMaterialExpressionScalarParameter* OverrideExpression = nullptr;
    for (const FName Name : { FName(TEXT("FF_LightingBrightness")), FName(TEXT("FF_LightingUseColorOverride")) })
    {
        UMaterialExpressionScalarParameter* Scalar = NewObject<UMaterialExpressionScalarParameter>(Source);
        Scalar->ParameterName = Name;
        Scalar->DefaultValue = Name == TEXT("FF_LightingBrightness") ? 1.0f : 0.0f;
        Source->GetExpressionCollection().AddExpression(Scalar);
        if (Name == TEXT("FF_LightingBrightness")) BrightnessExpression = Scalar;
        else OverrideExpression = Scalar;
    }
    UMaterialExpressionVectorParameter* Color = NewObject<UMaterialExpressionVectorParameter>(Source);
    Color->ParameterName = TEXT("FF_LightingColor");
    Color->DefaultValue = FLinearColor::White;
    Source->GetExpressionCollection().AddExpression(Color);
    UMaterialExpressionLinearInterpolate* Tint = NewObject<UMaterialExpressionLinearInterpolate>(Source);
    Tint->ConstA = 1.0f;
    Tint->B.Connect(0, Color);
    Tint->Alpha.Connect(0, OverrideExpression);
    Source->GetExpressionCollection().AddExpression(Tint);
    UMaterialExpressionMultiply* Emission = NewObject<UMaterialExpressionMultiply>(Source);
    Emission->A.Connect(0, Tint);
    Emission->B.Connect(0, BrightnessExpression);
    Source->GetExpressionCollection().AddExpression(Emission);
    Source->GetEditorOnlyData()->EmissiveColor.Connect(0, Emission);
    Source->UpdateCachedExpressionData();

    AFFZSLightingTestRig* RigA = World->SpawnActor<AFFZSLightingTestRig>();
    AFFZSLightingTestRig* RigB = World->SpawnActor<AFFZSLightingTestRig>();
    if (!TestNotNull(TEXT("First rig"), RigA) || !TestNotNull(TEXT("Second rig"), RigB)) return false;
    UStaticMeshComponent* MeshA = NewObject<UStaticMeshComponent>(RigA);
    UStaticMeshComponent* MeshB = NewObject<UStaticMeshComponent>(RigB);
    MeshA->SetStaticMesh(Cube);
    MeshB->SetStaticMesh(Cube);
    MeshA->SetMaterial(0, Source);
    MeshB->SetMaterial(0, Source);

    auto ReadScalar = [](const UMaterialInterface* Material, const FName Name)
    {
        float Value = -1.0f;
        if (Material) Material->GetScalarParameterValue(FMaterialParameterInfo(Name), Value);
        return Value;
    };
    FFFZSFacilityLightingState State;
    State.BrightnessMultiplier = 2.5f;
    State.bOverrideColor = true;
    State.Color = FLinearColor(0.2f, 0.5f, 0.8f);
    RigA->ApplyLightingState(State, true, MeshA);
    UMaterialInstanceDynamic* MaterialA = Cast<UMaterialInstanceDynamic>(MeshA->GetMaterial(0));
    if (!TestNotNull(TEXT("Opt-in material gets an instance"), MaterialA)) return false;
    TestEqual(TEXT("Emissive uses selected brightness"), ReadScalar(MaterialA, TEXT("FF_LightingBrightness")), 2.5f);
    TestEqual(TEXT("Override tint enabled"), ReadScalar(MaterialA, TEXT("FF_LightingUseColorOverride")), 1.0f);
    FLinearColor AppliedColor;
    TestTrue(TEXT("Color parameter exists"), MaterialA->GetVectorParameterValue(
        FMaterialParameterInfo(TEXT("FF_LightingColor")), AppliedColor));
    TestTrue(TEXT("Emissive tint matches real light color"), AppliedColor.Equals(State.Color));
    RigA->ApplyLightingState(State, true, MeshA);
    TestTrue(TEXT("Repeated edits reuse material instance"), MeshA->GetMaterial(0) == MaterialA);
    TestEqual(TEXT("Repeated brightness does not compound"), ReadScalar(MaterialA, TEXT("FF_LightingBrightness")), 2.5f);
    TestEqual(TEXT("Source asset keeps authored appearance"), ReadScalar(Source, TEXT("FF_LightingBrightness")), 1.0f);
    TestTrue(TEXT("Other facility still references original"), MeshB->GetMaterial(0) == Source);

    State.bEnabled = false;
    RigA->ApplyLightingState(State, true, MeshA);
    TestEqual(TEXT("Off extinguishes bulb material too"), ReadScalar(MaterialA, TEXT("FF_LightingBrightness")), 0.0f);
    State.bEnabled = true;
    RigA->ApplyLightingState(State, false, MeshA);
    TestEqual(TEXT("Hidden staging suppresses emissive"), ReadScalar(MaterialA, TEXT("FF_LightingBrightness")), 0.0f);
    State = FFFZSFacilityLightingState();
    RigA->ApplyLightingState(State, true, MeshA);
    TestEqual(TEXT("Reset restores authored brightness"), ReadScalar(MaterialA, TEXT("FF_LightingBrightness")), 1.0f);
    TestEqual(TEXT("Reset restores original colors"), ReadScalar(MaterialA, TEXT("FF_LightingUseColorOverride")), 0.0f);

    // A shared external dynamic material must also be isolated and retain a valid non-MID parent.
    MeshB->SetMaterial(0, MaterialA);
    State.BrightnessMultiplier = 4.0f;
    RigB->ApplyLightingState(State, true, MeshB);
    UMaterialInstanceDynamic* MaterialB = Cast<UMaterialInstanceDynamic>(MeshB->GetMaterial(0));
    if (!TestNotNull(TEXT("Shared MID is copied"), MaterialB)) return false;
    TestTrue(TEXT("Facilities own distinct dynamic instances"), MaterialB != MaterialA);
    TestTrue(TEXT("Copied MID has original renderable parent"), MaterialB->Parent == Source);
    TestEqual(TEXT("Editing copied MID leaves first fixture unchanged"), ReadScalar(MaterialA, TEXT("FF_LightingBrightness")), 1.0f);

    MeshB->SetMaterial(0, Source);
    RigB->ApplyLightingState(State, true, MeshB);
    TestEqual(TEXT("Replaced mesh material receives current state"), ReadScalar(MeshB->GetMaterial(0), TEXT("FF_LightingBrightness")), 4.0f);
    UMaterialInterface* Uncontrolled = Cube->GetMaterial(0);
    MeshB->SetMaterial(0, Uncontrolled);
    RigB->ApplyLightingState(State, true, MeshB);
    TestTrue(TEXT("Ordinary materials remain untouched"), MeshB->GetMaterial(0) == Uncontrolled);
    RigA->Destroy();
    RigB->Destroy();
    return true;
}

#endif
