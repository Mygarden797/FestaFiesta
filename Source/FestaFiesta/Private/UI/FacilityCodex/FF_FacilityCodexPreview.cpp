#include "UI/FacilityCodex/FF_FacilityCodexPreview.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StreamableManager.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "SceneView.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
	constexpr double ShowcaseRadius = 100.0;
	constexpr float MinimumZoom = .65f;
	constexpr float MaximumZoom = 1.65f;
}

UFF_FacilityCodexPreview::UFF_FacilityCodexPreview(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTextureCube> NeutralSky(TEXT(
		"/Engine/EngineResources/GrayLightTextureCube.GrayLightTextureCube"));
	PreviewSkyCubemap = NeutralSky.Object;
	// UViewport's Slate surface disables blending. Use the project's off-white stage,
	// rather than promising transparency that varies with the renderer's alpha settings.
	SetBackgroundColor(FLinearColor::FromSRGBColor(FColor(238, 242, 239, 255)));
	ShowFlags.DisableAdvancedFeatures();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UFF_FacilityCodexPreview::ShowMesh(TSoftObjectPtr<UStaticMesh> Mesh)
{
	if (RequestedMesh == Mesh && (IsPreviewReady() || LoadHandle.IsValid())) return;
	CancelLoad();
	ClearActor();
	RequestedMesh = Mesh;
	ResetView();
	LoadRequestedMesh();
}

void UFF_FacilityCodexPreview::ResetPreview()
{
	CancelLoad();
	RequestedMesh.Reset();
	ClearActor();
	ResetView();
}

void UFF_FacilityCodexPreview::CancelLoad()
{
	++RequestSerial;
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
}

void UFF_FacilityCodexPreview::LoadRequestedMesh()
{
	UWorld* PreviewWorld = GetViewportWorld();
	if (IsDesignTime() || RequestedMesh.IsNull() || !PreviewWorld ||
		PreviewWorld->WorldType != EWorldType::GamePreview) return;

	const uint64 Serial = RequestSerial;
	if (UStaticMesh* Mesh = RequestedMesh.Get())
	{
		InstallMesh(Mesh, Serial);
		return;
	}

	const TSoftObjectPtr<UStaticMesh> Mesh = RequestedMesh;
	const TWeakObjectPtr<UFF_FacilityCodexPreview> WeakPreview(this);
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Mesh.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakPreview, Mesh, Serial]()
		{
			UFF_FacilityCodexPreview* Preview = WeakPreview.Get();
			if (!Preview || Preview->RequestSerial != Serial) return;
			Preview->InstallMesh(Mesh.Get(), Serial);
			// InstallMesh retains the mesh before the streamable handle releases its reference.
			if (Preview->RequestSerial == Serial) Preview->LoadHandle.Reset();
		}));
}

void UFF_FacilityCodexPreview::InstallMesh(UStaticMesh* Mesh, uint64 Serial)
{
	UWorld* PreviewWorld = GetViewportWorld();
	if (Serial != RequestSerial || !Mesh || Mesh->GetNumLODs() == 0 || !PreviewWorld ||
		PreviewWorld->WorldType != EWorldType::GamePreview) return;

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const double Radius = Bounds.BoxExtent.Size();
	if (Bounds.Origin.ContainsNaN() || Bounds.BoxExtent.ContainsNaN() ||
		!FMath::IsFinite(Radius) || Radius <= 0.0) return;
	const double Scale = ShowcaseRadius / Radius;
	const FVector CenterOffset = Bounds.Origin * Scale;
	if (!FMath::IsFinite(Scale) || CenterOffset.ContainsNaN()) return;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags = RF_Transient | RF_DuplicateTransient | RF_TextExportTransient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Actor = PreviewWorld->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!Actor) return;

	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);
	Actor->PrimaryActorTick.bCanEverTick = false;
	UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->bUseDefaultCollision = false;
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetSimulatePhysics(false);
	Component->SetComponentTickEnabled(false);
	Component->PrimaryComponentTick.bCanEverTick = false;

	PreviewActor = Actor;
	LoadedMesh = Mesh;
	MeshCenter = Bounds.Origin;
	MeshScale = Scale;
	ApplyView();
	SetReady(true);
}

void UFF_FacilityCodexPreview::ClearActor()
{
	if (AStaticMeshActor* Actor = PreviewActor.Get()) Actor->Destroy();
	PreviewActor.Reset();
	LoadedMesh = nullptr;
	MeshCenter = FVector::ZeroVector;
	MeshScale = 1.0;
	SetReady(false);
}

bool UFF_FacilityCodexPreview::IsPreviewReady() const
{
	return bPreviewReady && IsValid(LoadedMesh) && PreviewActor.IsValid() && GetViewportWorld();
}

void UFF_FacilityCodexPreview::SetReady(bool bReady)
{
	if (bPreviewReady == bReady) return;
	bPreviewReady = bReady;
	OnPreviewReadyChanged.Broadcast();
}

void UFF_FacilityCodexPreview::RotatePreview(float YawDelta)
{
	if (!FMath::IsFinite(YawDelta)) return;
	PreviewYaw = FRotator::NormalizeAxis(PreviewYaw + FRotator::NormalizeAxis(YawDelta));
	ApplyView();
}

void UFF_FacilityCodexPreview::SetZoom(float Delta)
{
	if (!FMath::IsFinite(Delta)) return;
	PreviewZoom = FMath::Clamp(PreviewZoom + Delta, MinimumZoom, MaximumZoom);
	ApplyView();
}

void UFF_FacilityCodexPreview::ResetView()
{
	PreviewYaw = 0.f;
	PreviewZoom = 1.f;
	ApplyView();
}

void UFF_FacilityCodexPreview::RefreshViewForSize()
{
	UpdatePreviewLighting();
	const FVector2D Size = GetCachedGeometry().GetLocalSize();
	if (Size.ContainsNaN() || Size.X <= 1.f || Size.Y <= 1.f || Size.Equals(LastViewportSize, .1)) return;
	LastViewportSize = Size;
	ApplyView();
}

void UFF_FacilityCodexPreview::ApplyView()
{
	if (AStaticMeshActor* Actor = PreviewActor.Get())
	{
		const FQuat Rotation = FRotator(0.f, PreviewYaw, 0.f).Quaternion();
		// Rotate around the mesh bounds, including assets whose authored pivot is far away.
		Actor->SetActorTransform(FTransform(Rotation,
			Rotation.RotateVector(-MeshCenter * MeshScale), FVector(MeshScale)),
			false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Match FUMGViewportClient's projection, including its default 4:3 authored aspect
	// and LocalPlayer's axis constraint. A wide MaintainYFOV stage keeps its vertical
	// field of view; assuming a fixed horizontal FOV makes the model shrink on resize.
	const FVector2D Size = LastViewportSize.X > 1.f && LastViewportSize.Y > 1.f
		? LastViewportSize : FVector2D(640.f, 320.f);
	const FIntRect ViewRect(0, 0, FMath::Max(1, FMath::RoundToInt(Size.X)), FMath::Max(1, FMath::RoundToInt(Size.Y)));
	FMinimalViewInfo ViewInfo;
	FSceneViewProjectionData ProjectionData;
	ProjectionData.SetViewRectangle(ViewRect);
	FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(ViewInfo,
		GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint, ViewRect, ProjectionData);
	const double NarrowAxisScale = FMath::Max(
		FMath::Abs(ProjectionData.ProjectionMatrix.M[0][0]), FMath::Abs(ProjectionData.ProjectionMatrix.M[1][1]));
	const double HalfAngle = FMath::Atan(1.0 / FMath::Max(NarrowAxisScale, UE_DOUBLE_SMALL_NUMBER));
	const double FitDistance = ShowcaseRadius * 1.18 / FMath::Sin(HalfAngle);
	const double Distance = FMath::Max(FitDistance / PreviewZoom, ShowcaseRadius * 1.4);
	const FRotator CameraRotation(-22.f, -35.f, 0.f);
	SetViewRotation(CameraRotation);
	SetViewLocation(-CameraRotation.Vector() * Distance);
}

void UFF_FacilityCodexPreview::ConfigurePreviewLighting()
{
	UWorld* PreviewWorld = GetViewportWorld();
	if (!PreviewWorld) return;
	// These components belong to UViewport's FPreviewScene and have no actor owner.
	// Find them once per rebuild, filtered by their registered world; never touch game lights.
	for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
		if (It->GetWorld() == PreviewWorld)
			It->SetIntensity(1.0f);
	for (TObjectIterator<USkyLightComponent> It; It; ++It)
	{
		if (It->GetWorld() != PreviewWorld) continue;
		PreviewSkyLight = *It;
		It->CubemapResolution = 32;
		It->SetCubemap(PreviewSkyCubemap);
		It->SetIntensity(.6f);
		bSkyCapturePending = PreviewSkyCubemap != nullptr;
		break;
	}
	UpdatePreviewLighting();
}

void UFF_FacilityCodexPreview::UpdatePreviewLighting()
{
	if (!bSkyCapturePending || !PreviewSkyLight.IsValid() || !FApp::CanEverRender()) return;
#if WITH_EDITOR
	// UViewport does not tick FPreviewScene::UpdateCaptureContents. Defer our one-time
	// cubemap processing until an asynchronously compiled editor texture is available.
	if (PreviewSkyCubemap && PreviewSkyCubemap->IsDefaultTexture()) return;
#endif
	USkyLightComponent::UpdateSkyCaptureContents(GetViewportWorld());
	bSkyCapturePending = PreviewSkyLight->GetProcessedSkyTexture() == nullptr;
}

TSharedRef<SWidget> UFF_FacilityCodexPreview::RebuildWidget()
{
	// A reconstruction gets its own load serial and world. A callback from the old
	// Slate surface must never repopulate a closed or newly selected preview.
	CancelLoad();
	ClearActor();
	LastViewportSize = FVector2D::ZeroVector;
	PreviewSkyLight.Reset();
	bSkyCapturePending = false;
	const TSharedRef<SWidget> Widget = Super::RebuildWidget();
	if (!IsDesignTime() && GetViewportWorld())
	{
		SetEnableAdvancedFeatures(false);
		SetShowFlag(TEXT("MotionBlur"), false);
		SetShowFlag(TEXT("Bloom"), false);
		SetShowFlag(TEXT("EyeAdaptation"), false);
		SetShowFlag(TEXT("Fog"), false);
		SetShowFlag(TEXT("Atmosphere"), false);
		SetShowFlag(TEXT("TemporalAA"), false);
		// A cubemap is required: the stock preview skylight otherwise has no render proxy.
		ConfigurePreviewLighting();
		ApplyView();
		LoadRequestedMesh();
	}
	return Widget;
}

void UFF_FacilityCodexPreview::ReleaseSlateResources(bool bReleaseChildren)
{
	CancelLoad();
	ClearActor();
	LastViewportSize = FVector2D::ZeroVector;
	PreviewSkyLight.Reset();
	bSkyCapturePending = false;
	Super::ReleaseSlateResources(bReleaseChildren);
}
