#include "FF_MapTerrainCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "FF_FFZSBuildableGridActor.h"
#include "FF_MapGridActor.h"
#include "EngineUtils.h"

void UFFMapTerrainCapture::Refresh(UWorld& World, const FBox& Bounds, const int64 Revision, const FVector& PlayerLocation)
{
    if (!Bounds.IsValid || PlayerLocation.ContainsNaN()) return;
    // 200 m capture padding keeps a moving 100 m circle sharp without photographing every frame.
    const FVector Center(FMath::GridSnap(PlayerLocation.X, 2500.0), FMath::GridSnap(PlayerLocation.Y, 2500.0), Bounds.GetCenter().Z);
    const FBox LocalBounds(FVector(Center.X - 10000.0, Center.Y - 10000.0, Bounds.Min.Z),
        FVector(Center.X + 10000.0, Center.Y + 10000.0, Bounds.Max.Z));
    const double Now = World.GetTimeSeconds();
    const bool bChanged = !CapturedBounds.IsValid || !CapturedBounds.Equals(LocalBounds, 0.01) ||
        (CapturedRevision != Revision && Now >= NextRevisionCaptureTime);
    if ((!bChanged && !bNeedsWarmupCapture) || Now < NextCaptureTime) return;
    const bool bFirstCapture = !Capture;
    if (!Capture)
    {
        Texture = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
        Texture->ClearColor = FLinearColor::Transparent;
        Texture->RenderTargetFormat = RTF_RGBA8;
        Texture->InitAutoFormat(512, 512);
        Capture = NewObject<USceneCaptureComponent2D>(&World, NAME_None, RF_Transient);
        Capture->bCaptureEveryFrame = false;
        Capture->bCaptureOnMovement = false;
        Capture->TextureTarget = Texture;
        Capture->ProjectionType = ECameraProjectionMode::Orthographic;
        Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->bAutoCalculateOrthoPlanes = true;
        Capture->ShowFlags.SetMotionBlur(false);
        Capture->ShowFlags.SetTemporalAA(false);
        Capture->ShowFlags.SetFog(false);
        // A readable map photograph uses material colors without scene exposure/shadows.
        Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
        Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
        Capture->ShowFlags.SetDynamicShadows(false);
        Capture->ShowFlags.SetEyeAdaptation(false);
        Capture->ShowFlags.SetBloom(false);
        Capture->RegisterComponentWithWorld(&World);
    }
    // +X north and +Y east. The WBP crops/recenters this padded image as the user moves.
    Capture->OrthoWidth = 20000.f;
    Capture->SetWorldLocationAndRotation(FVector(Center.X, Center.Y,
        Bounds.Max.Z + FMath::Max(10000.0, double(Capture->OrthoWidth))), FRotator(-90.0, 0.0, 0.0));
    Capture->HiddenActors.Reset();
    for (TActorIterator<AFFZSBuildableGridActor> It(&World); It; ++It) Capture->HideActorComponents(*It);
    for (TActorIterator<AFFMapGridActor> It(&World); It; ++It) Capture->HideActorComponents(*It);
    Capture->CaptureSceneDeferred();
    CapturedBounds = LocalBounds;
    CapturedRevision = Revision;
    // Movement capture at most 2 Hz, stationary construction changes at most 0.2 Hz.
    NextCaptureTime = Now + 0.5;
    NextRevisionCaptureTime = Now + 5.0;
    // One delayed photograph lets initial streaming settle; camera movement never recaptures.
    bNeedsWarmupCapture = bFirstCapture;
}

void UFFMapTerrainCapture::Shutdown()
{
    if (Capture) Capture->DestroyComponent();
    Capture = nullptr;
    Texture = nullptr;
}
