// Opt-in visual fixture: run this filter with -RenderOffscreen instead of -NullRHI.
// Uses the shipped grid renderer and real visibility collision/ground; it exports a review
// image without a machine-dependent golden screenshot comparison or production map edits.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "FF_FFZSBuildableGridComponent.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "RHIGlobals.h"

namespace
{
    class FScopedRenderedGridSettings
    {
    public:
        FScopedRenderedGridSettings()
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bGround(Settings->bEnableGroundChecks)
            , bCollision(Settings->bEnableWorldCollisionChecks)
            , bRestrictZones(Settings->bRestrictZoneIdsToProjectPresets)
        {
            Settings->bEnableGroundChecks = true;
            Settings->bEnableWorldCollisionChecks = true;
            Settings->bRestrictZoneIdsToProjectPresets = false;
        }
        ~FScopedRenderedGridSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bRestrictZoneIdsToProjectPresets = bRestrictZones;
        }
    private:
        UFFZSDeveloperSettings* Settings;
        bool bGround;
        bool bCollision;
        bool bRestrictZones;
    };

    UStaticMeshComponent* AddRenderedGridBox(UWorld& World, UStaticMesh& Cube,
        UMaterialInterface& Material, const FVector& Center, const FVector& Size,
        const FLinearColor& Color, const bool bCollision)
    {
        AActor* Actor = World.SpawnActor<AActor>();
        if (!Actor)
        {
            return nullptr;
        }
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
        Actor->SetRootComponent(Mesh);
        Actor->AddInstanceComponent(Mesh);
        Mesh->SetStaticMesh(&Cube);
        Mesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        Mesh->SetCollisionResponseToChannel(ECC_Visibility, bCollision ? ECR_Block : ECR_Ignore);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCanEverAffectNavigation(false);
        UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(&Material, Mesh);
        Tint->SetVectorParameterValue(TEXT("Color"), Color);
        Tint->SetVectorParameterValue(TEXT("BaseColor"), Color);
        Tint->SetVectorParameterValue(TEXT("Tint"), Color);
        Mesh->SetMaterial(0, Tint);
        Mesh->RegisterComponent();
        Actor->SetActorLocation(Center);
        Actor->SetActorScale3D(Size / 100.0);
        return Mesh;
    }

    AFFZSProtectedArea* AddRenderedGridProtection(UWorld& World, const FName ZoneId,
        const FName Id, const FVector& HalfExtent)
    {
        AFFZSProtectedArea* Area = World.SpawnActor<AFFZSProtectedArea>();
        if (Area)
        {
            Area->ProtectedAreaId = Id;
            Area->LinkedZoneId = ZoneId;
            Area->AreaType = EFFZSProtectedAreaType::NoBuild;
            Area->AreaBounds->SetBoxExtent(HalfExtent);
            Area->SetActorLocation(FVector(1000.0, 1000.0, 100.0));
        }
        return Area;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildableGridRenderedPreviewTest,
    "FF_ZoneSystem.Integration.BuildableGrid.RenderedPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridRenderedPreviewTest::RunTest(const FString& Parameters)
{
    if (GUsingNullRHI)
    {
        AddInfo(TEXT("SKIPPED: Buildable-grid PNG capture needs a real RHI. Run this filter with -RenderOffscreen."));
        return true;
    }
    FScopedRenderedGridSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Isolated editor fixture world"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* System = World->GetSubsystem<UFFZSWorldSubsystem>();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("World subsystem"), System) || !TestNotNull(TEXT("Fixture cube"), Cube) ||
        !TestNotNull(TEXT("Fixture material"), Material) || !TestNotNull(TEXT("Fixture zone"), Zone))
    {
        return false;
    }
    Zone->ApplyAcceptedZonePreset(TEXT("GridRenderedPreview"));
    const FVector Corners[] = {
        FVector(0.0, 0.0, 0.0), FVector(2000.0, 0.0, 0.0),
        FVector(2000.0, 2000.0, 0.0), FVector(0.0, 2000.0, 0.0)
    };
    Zone->BoundarySpline->ClearSplinePoints(false);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
    {
        Zone->BoundarySpline->AddSplinePoint(Corners[Index], ESplineCoordinateSpace::Local, false);
        Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    Zone->BoundarySpline->SetClosedLoop(true, true);
    Zone->BoundarySpline->SetVisibility(false);
    if (!TestTrue(TEXT("Fixture zone registers"), System->RegisterZone(Zone)))
    {
        return false;
    }

    const FLinearColor LandColor(0.10f, 0.17f, 0.15f);
    const FLinearColor RoadColor(0.055f, 0.075f, 0.085f);
    TestNotNull(TEXT("Physical level ground"), AddRenderedGridBox(*World, *Cube, *Material,
        FVector(1000.0, 1000.0, -30.0), FVector(2000.0, 2000.0, 60.0), LandColor, true));
    TestNotNull(TEXT("Road across parcel"), AddRenderedGridBox(*World, *Cube, *Material,
        FVector(1000.0, 1000.0, 0.5), FVector(220.0, 2000.0, 1.0), RoadColor, false));
    TestNotNull(TEXT("Crossing road"), AddRenderedGridBox(*World, *Cube, *Material,
        FVector(1000.0, 1000.0, 0.6), FVector(2000.0, 220.0, 1.0), RoadColor, false));
    TestNotNull(TEXT("Physical building obstacle"), AddRenderedGridBox(*World, *Cube, *Material,
        FVector(450.0, 1450.0, 150.0), FVector(350.0, 350.0, 300.0), FLinearColor(0.38f, 0.48f, 0.46f), true));
    // A lighter roof makes the physical obstruction readable in the exported angled view.
    TestNotNull(TEXT("Building roof trim"), AddRenderedGridBox(*World, *Cube, *Material,
        FVector(450.0, 1450.0, 305.0), FVector(375.0, 375.0, 10.0), FLinearColor(0.60f, 0.66f, 0.59f), false));
    AFFZSProtectedArea* RoadA = AddRenderedGridProtection(*World, Zone->ZoneId,
        TEXT("GridRenderProtectedA"), FVector(110.0, 1000.0, 250.0));
    AFFZSProtectedArea* RoadB = AddRenderedGridProtection(*World, Zone->ZoneId,
        TEXT("GridRenderProtectedB"), FVector(1000.0, 110.0, 250.0));
    if (!TestNotNull(TEXT("First protected road"), RoadA) || !TestNotNull(TEXT("Second protected road"), RoadB) ||
        !TestTrue(TEXT("First protection registers"), System->RegisterProtectedArea(RoadA)) ||
        !TestTrue(TEXT("Second protection registers"), System->RegisterProtectedArea(RoadB)))
    {
        return false;
    }

    AActor* GridOwner = World->SpawnActor<AActor>();
    AActor* CameraOwner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Grid owner"), GridOwner) || !TestNotNull(TEXT("Capture owner"), CameraOwner))
    {
        return false;
    }
    UFFZSBuildableGridComponent* Grid = NewObject<UFFZSBuildableGridComponent>(GridOwner, NAME_None, RF_Transient);
    GridOwner->SetRootComponent(Grid);
    GridOwner->AddInstanceComponent(Grid);
    Grid->RegisterComponent();
    FFFZSCommand Overview;
    Overview.ZoneId = Zone->ZoneId;
    // Repeated public refresh calls also drain implementations that amortize terrain probes.
    // Bounded work, no sleep, and no installation commands are necessary for the fixture.
    for (int32 Pass = 0; Pass < 128; ++Pass)
    {
        Grid->RefreshGrid(*System, Overview, FVector2D(1000.0), FVector2D(1100.0), 100,
            FVector(1000.0, 1000.0, 0.0), FVector::UpVector);
    }
    if (!TestTrue(TEXT("Actual renderer produced buildable grid lines"), !Grid->BatchedLines.IsEmpty()))
    {
        return false;
    }

    constexpr int32 Width = 1200;
    constexpr int32 Height = 1000;
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(CameraOwner, NAME_None, RF_Transient);
    Target->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
    Target->ClearColor = FLinearColor(0.012f, 0.024f, 0.029f);
    Target->InitAutoFormat(Width, Height);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(CameraOwner, NAME_None, RF_Transient);
    CameraOwner->SetRootComponent(Capture);
    CameraOwner->AddInstanceComponent(Capture);
    Capture->TextureTarget = Target;
    Capture->ProjectionType = ECameraProjectionMode::Orthographic;
    Capture->OrthoWidth = 2800.0f;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->bUseRayTracingIfEnabled = false;
    ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>();
    if (Light)
    {
        Light->SetActorRotation(FRotator(-60.0, 25.0, 0.0));
        Light->GetLightComponent()->SetIntensity(30.0f);
        Light->GetLightComponent()->SetCastShadows(false);
    }
    Capture->ShowFlags.SetLighting(true);
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetVolumetricFog(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetBloom(false);
    Capture->ShowFlags.SetDepthOfField(false);
    Capture->ShowFlags.SetAntiAliasing(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
    Capture->PostProcessSettings.AutoExposureBias = 0.0f;
    Capture->RegisterComponent();
    const FVector CameraLocation(1000.0, -2200.0, 4600.0);
    CameraOwner->SetActorLocation(CameraLocation);
    CameraOwner->SetActorRotation((FVector(1000.0, 1000.0, 0.0) - CameraLocation).Rotation());
    World->SendAllEndOfFrameUpdates();
    FlushRenderingCommands();
    // Let transient mesh/material scene resources settle before reading the review image.
    for (int32 Frame = 0; Frame < 3; ++Frame)
    {
        Capture->CaptureScene();
        FlushRenderingCommands();
    }

    TArray<FColor> Pixels;
    FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
    if (!TestTrue(TEXT("Real RHI returns a complete captured image"),
        Resource && Resource->ReadPixels(Pixels) && Pixels.Num() == Width * Height))
    {
        return false;
    }
    int32 TealPixels = 0;
    uint8 MinLuminance = 255;
    uint8 MaxLuminance = 0;
    for (const FColor& Pixel : Pixels)
    {
        const uint8 Luminance = static_cast<uint8>((static_cast<int32>(Pixel.R) + Pixel.G + Pixel.B) / 3);
        MinLuminance = FMath::Min(MinLuminance, Luminance);
        MaxLuminance = FMath::Max(MaxLuminance, Luminance);
        TealPixels += Pixel.G > 100 && Pixel.G > Pixel.R * 1.3 && Pixel.B > Pixel.R * 1.1 ? 1 : 0;
    }
    TestTrue(TEXT("Capture contains scene contrast rather than a blank clear color"), MaxLuminance - MinLuminance > 35);
    TestTrue(TEXT("Teal grid pixels are present in the RHI image"), TealPixels > 100);
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BuildableGrid"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, TEXT("BuildableGrid.png"));
    const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
    TestTrue(TEXT("Review PNG is written to disk"), bSaved && IFileManager::Get().FileSize(*Path) > 0);
    AddInfo(FString::Printf(TEXT("Buildable grid RHI preview: %s (teal pixels: %d, lines: %d)"),
        *FPaths::ConvertRelativePathToFull(Path), TealPixels, Grid->BatchedLines.Num()));
    Capture->TextureTarget = nullptr;
    return bSaved;
}

#endif
