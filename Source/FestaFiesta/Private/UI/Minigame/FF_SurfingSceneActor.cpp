// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_SurfingSceneActor.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/Minigame/FF_SurfingScreen.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr TCHAR PlaneMeshPath[] = TEXT("/Engine/BasicShapes/Plane.Plane");
	constexpr TCHAR CubeMeshPath[] = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr TCHAR SphereMeshPath[] = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	constexpr TCHAR CylinderMeshPath[] = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	constexpr TCHAR ConeMeshPath[] = TEXT("/Engine/BasicShapes/Cone.Cone");
	constexpr TCHAR BasicShapeMaterialPath[] =
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	constexpr TCHAR ProjectSeaMaterialPath[] =
		TEXT("/Game/Level/Demo_Matereal/Sea_Material/Mi_Sea_Inst.Mi_Sea_Inst");

	int32 ObjectTypeIndex(const EFF_SurfingObjectType Type)
	{
		return FMath::Clamp(static_cast<int32>(Type), 0, 5);
	}

	bool IsPickupType(const EFF_SurfingObjectType Type)
	{
		return Type == EFF_SurfingObjectType::Coin
			|| Type == EFF_SurfingObjectType::Heart
			|| Type == EFF_SurfingObjectType::BoostPickup;
	}
}

AFF_SurfingSceneActor::AFF_SurfingSceneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(SceneRoot);
	SceneCapture->PrimitiveRenderMode =
		ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = true;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->bAlwaysPersistRenderingState = true;
	SceneCapture->bUseRayTracingIfEnabled = false;
	SceneCapture->ShowFlags.SetMotionBlur(false);
	SceneCapture->ShowFlags.SetDepthOfField(false);
	SceneCapture->ShowOnlyActors.Add(this);

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetCastShadows(true);
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetCastShadows(false);

	OceanFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanFloor"));
	OceanFloor->SetupAttachment(SceneRoot);
	OceanSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanSurface"));
	OceanSurface->SetupAttachment(SceneRoot);

	SurferRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SurferRoot"));
	SurferRoot->SetupAttachment(SceneRoot);
	Surfboard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surfboard"));
	Surfboard->SetupAttachment(SurferRoot);
	RiderRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RiderRoot"));
	RiderRoot->SetupAttachment(SurferRoot);
	RiderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RiderBody"));
	RiderBody->SetupAttachment(RiderRoot);
	RiderHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RiderHead"));
	RiderHead->SetupAttachment(RiderRoot);
	RiderLeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RiderLeftArm"));
	RiderLeftArm->SetupAttachment(RiderRoot);
	RiderRightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RiderRightArm"));
	RiderRightArm->SetupAttachment(RiderRoot);
	RiderLeftLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RiderLeftLeg"));
	RiderLeftLeg->SetupAttachment(RiderRoot);
	RiderRightLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RiderRightLeg"));
	RiderRightLeg->SetupAttachment(RiderRoot);
	WakeLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WakeLeft"));
	WakeLeft->SetupAttachment(SurferRoot);
	WakeRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WakeRight"));
	WakeRight->SetupAttachment(SurferRoot);

	for (int32 Index = 0; Index < 7; ++Index)
	{
		UStaticMeshComponent* WaveBand = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("WaveBand_%02d"), Index + 1));
		WaveBand->SetupAttachment(SceneRoot);
		WaveBands.Add(WaveBand);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(PlaneMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(CubeMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(SphereMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(CylinderMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(ConeMeshPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(
		BasicShapeMaterialPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SeaMaterialFinder(
		ProjectSeaMaterialPath);

	PlaneMesh = PlaneFinder.Object;
	CubeMesh = CubeFinder.Object;
	SphereMesh = SphereFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	ConeMesh = ConeFinder.Object;
	BasicShapeMaterial = BasicMaterialFinder.Object;
	ProjectSeaMaterial = SeaMaterialFinder.Object;

	TArray<UStaticMeshComponent*> FixedMeshes = {
		OceanFloor,
		OceanSurface,
		Surfboard,
		RiderBody,
		RiderHead,
		RiderLeftArm,
		RiderRightArm,
		RiderLeftLeg,
		RiderRightLeg,
		WakeLeft,
		WakeRight,
	};
	for (UStaticMeshComponent* WaveBand : WaveBands)
	{
		FixedMeshes.Add(WaveBand);
	}
	for (UStaticMeshComponent* MeshComponent : FixedMeshes)
	{
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetCanEverAffectNavigation(false);
		MeshComponent->SetVisibleInSceneCaptureOnly(true);
		MeshComponent->SetCastShadow(
			MeshComponent != OceanFloor
			&& MeshComponent != OceanSurface
			&& MeshComponent != WakeLeft
			&& MeshComponent != WakeRight
			&& !WaveBands.Contains(MeshComponent));
	}
}

void AFF_SurfingSceneActor::InitializeScene(
	const FFF_SurfingVisualStyle& VisualStyle,
	UTextureRenderTarget2D* RenderTarget)
{
	ArenaHalfForwardWorld = FMath::Max(100.0f, VisualStyle.ArenaWorldHalfForward);
	ArenaHorizontalMetricScale = 2.18f;
	CameraFollowOffset = VisualStyle.Camera3DLocation;
	CameraLookAtOffset = VisualStyle.Camera3DLookAt;
	SceneCapture->bCaptureEveryFrame = true;
	SceneCapture->ShowOnlyActors.Reset();
	SceneCapture->ShowOnlyActors.Add(this);
	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->FOVAngle = FMath::Clamp(VisualStyle.Camera3DFov, 30.0f, 120.0f);
	SceneCapture->SetRelativeLocation(CameraFollowOffset);
	SceneCapture->SetRelativeRotation(
		(CameraLookAtOffset - CameraFollowOffset).Rotation());
	SceneCapture->MaxViewDistanceOverride = ArenaHalfForwardWorld * 8.0f;
	KeyLight->SetIntensity(FMath::Max(0.0f, VisualStyle.SceneKeyLightIntensity));
	KeyLight->SetAttenuationRadius(ArenaHalfForwardWorld * 7.5f);
	KeyLight->SetLightColor(FLinearColor(1.0f, 0.84f, 0.68f));
	KeyLight->SetRelativeLocation(FVector(-350.0, -1250.0, 2600.0));
	FillLight->SetIntensity(FMath::Max(0.0f, VisualStyle.SceneFillLightIntensity));
	FillLight->SetAttenuationRadius(ArenaHalfForwardWorld * 8.5f);
	FillLight->SetLightColor(FLinearColor(0.34f, 0.72f, 1.0f));
	FillLight->SetRelativeLocation(FVector(1250.0, 1650.0, 1150.0));

	OceanMaterial = CreateColorMaterial(TEXT("MI_SurfingOcean"), VisualStyle.OceanColor);
	FoamMaterial = CreateColorMaterial(TEXT("MI_SurfingFoam"), VisualStyle.FoamColor);
	BoardMaterial = CreateColorMaterial(TEXT("MI_SurfingBoard"), VisualStyle.SurferColor);
	BoostBoardMaterial = CreateColorMaterial(
		TEXT("MI_SurfingBoostBoard"), VisualStyle.BoostingSurferColor);
	RiderMaterial = CreateColorMaterial(
		TEXT("MI_SurfingRider"), FLinearColor(0.95f, 0.12f, 0.035f, 1.0f));
	RiderSkinMaterial = CreateColorMaterial(
		TEXT("MI_SurfingRiderSkin"), FLinearColor(0.95f, 0.55f, 0.28f, 1.0f));

	ObjectMaterials.Reset();
	const FLinearColor ObjectColors[] = {
		VisualStyle.RockColor,
		VisualStyle.LogColor,
		VisualStyle.BuoyColor,
		VisualStyle.CoinColor,
		VisualStyle.HeartColor,
		VisualStyle.BoostPickupColor,
	};
	const TCHAR* ObjectMaterialNames[] = {
		TEXT("MI_SurfingRock"),
		TEXT("MI_SurfingLog"),
		TEXT("MI_SurfingBuoy"),
		TEXT("MI_SurfingCoin"),
		TEXT("MI_SurfingHeart"),
		TEXT("MI_SurfingBoostPickup"),
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ObjectColors); ++Index)
	{
		ObjectMaterials.Add(CreateColorMaterial(ObjectMaterialNames[Index], ObjectColors[Index]));
	}

	ConfigureFixedVisuals();
	bSceneInitialized = IsSceneReady();
	SetActorTickEnabled(bSceneInitialized);
	if (bSceneInitialized)
	{
		SceneCapture->CaptureSceneDeferred();
	}
}

void AFF_SurfingSceneActor::ReleaseScene()
{
	bSceneInitialized = false;
	SetActorTickEnabled(false);
	if (SceneCapture)
	{
		SceneCapture->bCaptureEveryFrame = false;
		SceneCapture->TextureTarget = nullptr;
		SceneCapture->ShowOnlyActors.Reset();
	}
}

void AFF_SurfingSceneActor::ApplySnapshot(const FFF_SurfingSnapshot& Snapshot)
{
	if (!bSceneInitialized)
	{
		return;
	}
	if (FMath::IsFinite(Snapshot.ArenaHorizontalMetricScale)
		&& Snapshot.ArenaHorizontalMetricScale > UE_SMALL_NUMBER
		&& !FMath::IsNearlyEqual(
			ArenaHorizontalMetricScale,
			Snapshot.ArenaHorizontalMetricScale))
	{
		ArenaHorizontalMetricScale = Snapshot.ArenaHorizontalMetricScale;
		ConfigureFixedVisuals();
	}

	PlayerTargetLocation = ArenaToSceneLocal(Snapshot.PlayerPositionNormalized, 76.0f);
	FVector2D Heading = Snapshot.CurrentHeading;
	if (Heading.IsNearlyZero())
	{
		Heading = FVector2D(0.0f, 1.0f);
	}
	Heading.Normalize();
	PlayerTargetYaw = FMath::RadiansToDegrees(FMath::Atan2(
		Heading.X * ArenaHorizontalMetricScale,
		Heading.Y));
	if (!bHasPlayerTransform)
	{
		SurferRoot->SetRelativeLocation(PlayerTargetLocation);
		SurferRoot->SetRelativeRotation(FRotator(0.0f, PlayerTargetYaw, 0.0f));
		SceneCapture->SetRelativeLocation(PlayerTargetLocation + CameraFollowOffset);
		SceneCapture->SetRelativeRotation((
			PlayerTargetLocation + CameraLookAtOffset
			- SceneCapture->GetRelativeLocation()).Rotation());
		bHasPlayerTransform = true;
	}

	bBoosting = Snapshot.bBoosting;
	bInvincible = Snapshot.bInvincible;
	Surfboard->SetMaterial(0, bBoosting ? BoostBoardMaterial : BoardMaterial);
	WakeLeft->SetVisibility(true);
	WakeRight->SetVisibility(true);

	if (Snapshot.LatestEventSerial > 0
		&& Snapshot.LatestEventSerial != PresentedEventSerial)
	{
		PresentedEventSerial = Snapshot.LatestEventSerial;
		EventPulseRemainingSeconds = 0.32f;
	}

	TSet<int32> VisibleIds;
	CourseObjectTargetLocations.Reset();
	for (const FFF_SurfingObjectSnapshot& ObjectSnapshot : Snapshot.VisibleObjects)
	{
		if (ObjectSnapshot.ObjectId < 0)
		{
			continue;
		}

		VisibleIds.Add(ObjectSnapshot.ObjectId);
		const float RadiusWorld = FMath::Clamp(
			ObjectSnapshot.CollisionRadiusNormalized
				* ArenaHalfForwardWorld
				* 1.45f,
			36.0f,
			150.0f);

		if (const EFF_SurfingObjectType* ExistingType =
			CourseObjectTypes.Find(ObjectSnapshot.ObjectId))
		{
			if (*ExistingType != ObjectSnapshot.Type)
			{
				DestroyCourseObject(ObjectSnapshot.ObjectId);
			}
		}

		USceneComponent* ObjectRoot = CourseObjectRoots.FindRef(ObjectSnapshot.ObjectId);
		const bool bNewObject = !IsValid(ObjectRoot);
		if (bNewObject)
		{
			ObjectRoot = CreateCourseObject(ObjectSnapshot, RadiusWorld);
		}
		if (!ObjectRoot)
		{
			continue;
		}

		const FVector TargetLocation = ArenaToSceneLocal(
			ObjectSnapshot.PositionNormalized,
			GetObjectHoverHeight(ObjectSnapshot.Type, RadiusWorld));
		CourseObjectTargetLocations.Add(ObjectSnapshot.ObjectId, TargetLocation);
		if (bNewObject)
		{
			ObjectRoot->SetRelativeLocation(TargetLocation);
		}
		SetCourseObjectVisible(*ObjectRoot, true);
	}

	for (const TPair<int32, TObjectPtr<USceneComponent>>& Pair : CourseObjectRoots)
	{
		if (IsValid(Pair.Value) && !VisibleIds.Contains(Pair.Key))
		{
			SetCourseObjectVisible(*Pair.Value, false);
		}
	}
	TArray<UStaticMeshComponent*> CaptureMeshes;
	GetComponents<UStaticMeshComponent>(CaptureMeshes);
	for (UStaticMeshComponent* MeshComponent : CaptureMeshes)
	{
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComponent->SetCanEverAffectNavigation(false);
			MeshComponent->SetVisibleInSceneCaptureOnly(true);
		}
	}
	SceneCapture->CaptureSceneDeferred();
}

void AFF_SurfingSceneActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSceneInitialized || DeltaSeconds <= 0.0f)
	{
		return;
	}

	SceneSeconds += DeltaSeconds;
	EventPulseRemainingSeconds = FMath::Max(0.0f, EventPulseRemainingSeconds - DeltaSeconds);

	OceanSurface->SetRelativeRotation(FRotator(
		FMath::Sin(SceneSeconds * 0.42f) * 0.35f,
		0.0f,
		FMath::Cos(SceneSeconds * 0.31f) * 0.45f));

	const float WaveSpan = ArenaHalfForwardWorld * 2.35f;
	for (int32 Index = 0; Index < WaveBands.Num(); ++Index)
	{
		UStaticMeshComponent* WaveBand = WaveBands[Index];
		const float Wrapped = FMath::Fmod(
			SceneSeconds * 165.0f
				+ static_cast<float>(Index) * WaveSpan / WaveBands.Num(),
			WaveSpan);
		const float X = ArenaHalfForwardWorld * 1.18f - Wrapped;
		const float Y = FMath::Sin(SceneSeconds * 0.55f + Index * 1.37f)
			* ArenaHalfForwardWorld * ArenaHorizontalMetricScale * 0.18f;
		WaveBand->SetRelativeLocation(FVector(X, Y, 7.0f + (Index % 2) * 2.0f));
		WaveBand->SetRelativeRotation(FRotator(0.0f, FMath::Sin(Index * 1.9f) * 5.0f, 0.0f));
	}

	const FVector BobbedPlayerTarget = PlayerTargetLocation + FVector(
		0.0,
		0.0,
		FMath::Sin(SceneSeconds * 3.4f) * 7.0f);
	SurferRoot->SetRelativeLocation(FMath::VInterpTo(
		SurferRoot->GetRelativeLocation(), BobbedPlayerTarget, DeltaSeconds, 10.0f));

	const FRotator CurrentRotation = SurferRoot->GetRelativeRotation();
	const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, PlayerTargetYaw);
	const float TargetBank = FMath::Clamp(-YawDelta * 0.32f, -18.0f, 18.0f);
	const FRotator DesiredRotation(
		FMath::Sin(SceneSeconds * 2.1f) * 1.8f,
		PlayerTargetYaw,
		TargetBank);
	SurferRoot->SetRelativeRotation(FMath::RInterpTo(
		CurrentRotation, DesiredRotation, DeltaSeconds, 8.0f));
	const FVector DesiredCameraLocation = PlayerTargetLocation + CameraFollowOffset;
	SceneCapture->SetRelativeLocation(FMath::VInterpTo(
		SceneCapture->GetRelativeLocation(),
		DesiredCameraLocation,
		DeltaSeconds,
		4.5f));
	const FRotator DesiredCameraRotation = (
		PlayerTargetLocation + CameraLookAtOffset
		- SceneCapture->GetRelativeLocation()).Rotation();
	SceneCapture->SetRelativeRotation(FMath::RInterpTo(
		SceneCapture->GetRelativeRotation(),
		DesiredCameraRotation,
		DeltaSeconds,
		5.5f));
	RiderRoot->SetRelativeRotation(FMath::RInterpTo(
		RiderRoot->GetRelativeRotation(),
		FRotator(
			bBoosting ? -14.0f : -5.0f,
			0.0f,
			TargetBank * -0.42f),
		DeltaSeconds,
		7.0f));
	const float LimbMotion = FMath::Sin(SceneSeconds * (bBoosting ? 8.0f : 4.0f))
		* (bBoosting ? 8.0f : 3.0f);
	RiderLeftArm->SetRelativeRotation(FRotator(18.0f + LimbMotion, 0.0f, -58.0f));
	RiderRightArm->SetRelativeRotation(FRotator(18.0f - LimbMotion, 0.0f, 58.0f));

	float SurferScale = bBoosting ? 1.08f : 1.0f;
	if (EventPulseRemainingSeconds > 0.0f)
	{
		SurferScale *= FMath::Lerp(1.0f, 1.14f, EventPulseRemainingSeconds / 0.32f);
	}
	SurferRoot->SetRelativeScale3D(FVector(SurferScale));
	if (bInvincible && !bBoosting)
	{
		Surfboard->SetMaterial(
			0,
			FMath::Fmod(SceneSeconds, 0.20f) < 0.10f ? BoostBoardMaterial : BoardMaterial);
	}
	else
	{
		Surfboard->SetMaterial(0, bBoosting ? BoostBoardMaterial : BoardMaterial);
	}

	const float WakeLength = bBoosting ? 2.35f : 1.45f;
	WakeLeft->SetRelativeScale3D(FVector(WakeLength, 0.065f, 0.025f));
	WakeRight->SetRelativeScale3D(FVector(WakeLength, 0.065f, 0.025f));
	WakeLeft->SetRelativeLocation(FVector(-70.0f * WakeLength, -31.0f, -16.0f));
	WakeRight->SetRelativeLocation(FVector(-70.0f * WakeLength, 31.0f, -16.0f));

	for (const TPair<int32, FVector>& Pair : CourseObjectTargetLocations)
	{
		USceneComponent* ObjectRoot = CourseObjectRoots.FindRef(Pair.Key);
		const EFF_SurfingObjectType* Type = CourseObjectTypes.Find(Pair.Key);
		if (!IsValid(ObjectRoot) || !Type)
		{
			continue;
		}

		const float Phase = static_cast<float>(Pair.Key % 97) * 0.31f;
		const float BobAmount = IsPickupType(*Type) ? 14.0f : 4.0f;
		const FVector BobbedTarget = Pair.Value + FVector(
			0.0,
			0.0,
			FMath::Sin(SceneSeconds * 2.4f + Phase) * BobAmount);
		ObjectRoot->SetRelativeLocation(FMath::VInterpTo(
			ObjectRoot->GetRelativeLocation(), BobbedTarget, DeltaSeconds, 12.0f));

		FRotator Rotation = ObjectRoot->GetRelativeRotation();
		if (IsPickupType(*Type))
		{
			Rotation.Yaw = FMath::Fmod(Rotation.Yaw + DeltaSeconds * 72.0f, 360.0f);
		}
		else if (*Type == EFF_SurfingObjectType::Log
			|| *Type == EFF_SurfingObjectType::Buoy)
		{
			Rotation.Roll = FMath::Sin(SceneSeconds * 1.8f + Phase) * 4.0f;
		}
		ObjectRoot->SetRelativeRotation(Rotation);
	}
}

bool AFF_SurfingSceneActor::IsSceneReady() const
{
	return IsValid(SceneRoot)
		&& IsValid(SceneCapture)
		&& IsValid(SceneCapture->TextureTarget)
		&& IsValid(OceanSurface)
		&& IsValid(SurferRoot)
		&& IsValid(Surfboard)
		&& IsValid(RiderRoot)
		&& IsValid(RiderBody)
		&& IsValid(RiderHead)
		&& IsValid(RiderLeftArm)
		&& IsValid(RiderRightArm)
		&& IsValid(RiderLeftLeg)
		&& IsValid(RiderRightLeg)
		&& IsValid(PlaneMesh)
		&& IsValid(CubeMesh)
		&& IsValid(SphereMesh)
		&& IsValid(CylinderMesh)
		&& IsValid(ConeMesh)
		&& IsValid(BasicShapeMaterial)
		&& IsValid(OceanMaterial)
		&& IsValid(BoardMaterial)
		&& ObjectMaterials.Num() == 6;
}

int32 AFF_SurfingSceneActor::GetVisibleCourseObjectCount() const
{
	return CourseObjectTargetLocations.Num();
}

int32 AFF_SurfingSceneActor::GetRiderMeshPartCount() const
{
	const UStaticMeshComponent* RiderParts[] = {
		RiderBody,
		RiderHead,
		RiderLeftArm,
		RiderRightArm,
		RiderLeftLeg,
		RiderRightLeg,
	};
	int32 Count = 0;
	for (const UStaticMeshComponent* Part : RiderParts)
	{
		Count += IsValid(Part) && IsValid(Part->GetStaticMesh()) ? 1 : 0;
	}
	return Count;
}

FVector AFF_SurfingSceneActor::GetPlayerSceneLocalLocation() const
{
	return IsValid(SurferRoot) ? SurferRoot->GetRelativeLocation() : FVector::ZeroVector;
}

FVector AFF_SurfingSceneActor::ArenaToSceneLocal(
	const FVector2D& PositionNormalized,
	const float Height) const
{
	return FVector(
		FMath::Clamp(PositionNormalized.Y, -1.0, 1.0) * ArenaHalfForwardWorld,
		FMath::Clamp(PositionNormalized.X, -1.0, 1.0)
			* ArenaHalfForwardWorld * ArenaHorizontalMetricScale,
		Height);
}

FVector2D AFF_SurfingSceneActor::SceneLocalToArena(const FVector& SceneLocalPosition) const
{
	return FVector2D(
		SceneLocalPosition.Y / FMath::Max(
			ArenaHalfForwardWorld * ArenaHorizontalMetricScale, 1.0f),
		SceneLocalPosition.X / FMath::Max(ArenaHalfForwardWorld, 1.0f));
}

bool AFF_SurfingSceneActor::DeprojectCapturePointToArena(
	const FVector2D& CaptureUv,
	FVector2D& OutArenaPosition) const
{
	OutArenaPosition = FVector2D::ZeroVector;
	if (!bSceneInitialized || !SceneCapture || !SceneCapture->TextureTarget)
	{
		return false;
	}

	const float AspectRatio = SceneCapture->TextureTarget->SizeY > 0
		? static_cast<float>(SceneCapture->TextureTarget->SizeX)
			/ static_cast<float>(SceneCapture->TextureTarget->SizeY)
		: 16.0f / 9.0f;
	const float TanHalfHorizontalFov = FMath::Tan(
		FMath::DegreesToRadians(SceneCapture->FOVAngle * 0.5f));
	const float ScreenX = FMath::Clamp(CaptureUv.X, 0.0, 1.0) * 2.0f - 1.0f;
	const float ScreenY = 1.0f - FMath::Clamp(CaptureUv.Y, 0.0, 1.0) * 2.0f;
	const FVector RayDirection = (
		SceneCapture->GetForwardVector()
		+ SceneCapture->GetRightVector() * ScreenX * TanHalfHorizontalFov
		+ SceneCapture->GetUpVector() * ScreenY
			* TanHalfHorizontalFov / FMath::Max(AspectRatio, 0.01f)).GetSafeNormal();
	if (RayDirection.Z >= -UE_SMALL_NUMBER)
	{
		return false;
	}

	const FVector RayOrigin = SceneCapture->GetComponentLocation();
	const float OceanWorldZ = GetActorLocation().Z;
	const double Distance = (OceanWorldZ - RayOrigin.Z) / RayDirection.Z;
	if (!FMath::IsFinite(Distance) || Distance <= 0.0)
	{
		return false;
	}

	const FVector HitWorld = RayOrigin + RayDirection * Distance;
	const FVector HitLocal = GetActorTransform().InverseTransformPosition(HitWorld);
	OutArenaPosition = SceneLocalToArena(HitLocal);
	return FMath::IsFinite(OutArenaPosition.X) && FMath::IsFinite(OutArenaPosition.Y);
}

UMaterialInstanceDynamic* AFF_SurfingSceneActor::CreateColorMaterial(
	const FName MaterialName,
	const FLinearColor& Color)
{
	if (!BasicShapeMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
		BasicShapeMaterial, this, MaterialName);
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Material->SetVectorParameterValue(TEXT("Tint"), Color);
	}
	return Material;
}

UStaticMeshComponent* AFF_SurfingSceneActor::CreateObjectPart(
	USceneComponent& Parent,
	const int32 ObjectId,
	const TCHAR* PartName,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FVector& RelativeLocation,
	const FRotator& RelativeRotation,
	const FVector& RelativeScale)
{
	if (!Mesh || !Material)
	{
		return nullptr;
	}

	const FName ComponentName = MakeUniqueObjectName(
		this,
		UStaticMeshComponent::StaticClass(),
		*FString::Printf(TEXT("SurfObject_%d_%s"), ObjectId, PartName));
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(
		this, UStaticMeshComponent::StaticClass(), ComponentName, RF_Transient);
	if (!Part)
	{
		return nullptr;
	}

	Part->SetMobility(EComponentMobility::Movable);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetVisibleInSceneCaptureOnly(true);
	Part->SetCastShadow(!IsPickupType(CourseObjectTypes.FindRef(ObjectId)));
	Part->SetStaticMesh(Mesh);
	Part->SetMaterial(0, Material);
	Part->SetupAttachment(&Parent);
	Part->SetRelativeLocation(RelativeLocation);
	Part->SetRelativeRotation(RelativeRotation);
	Part->SetRelativeScale3D(RelativeScale);
	AddInstanceComponent(Part);
	Part->RegisterComponent();
	return Part;
}

USceneComponent* AFF_SurfingSceneActor::CreateCourseObject(
	const FFF_SurfingObjectSnapshot& ObjectSnapshot,
	const float RadiusWorld)
{
	const FName RootName = MakeUniqueObjectName(
		this,
		USceneComponent::StaticClass(),
		*FString::Printf(TEXT("SurfObject_%d_Root"), ObjectSnapshot.ObjectId));
	USceneComponent* ObjectRoot = NewObject<USceneComponent>(
		this, USceneComponent::StaticClass(), RootName, RF_Transient);
	if (!ObjectRoot)
	{
		return nullptr;
	}

	ObjectRoot->SetMobility(EComponentMobility::Movable);
	ObjectRoot->SetupAttachment(SceneRoot);
	AddInstanceComponent(ObjectRoot);
	ObjectRoot->RegisterComponent();
	CourseObjectRoots.Add(ObjectSnapshot.ObjectId, ObjectRoot);
	CourseObjectTypes.Add(ObjectSnapshot.ObjectId, ObjectSnapshot.Type);

	UMaterialInterface* Material = GetObjectMaterial(ObjectSnapshot.Type);
	const float SphereScale = RadiusWorld / 50.0f;
	const float CylinderRadiusScale = RadiusWorld / 50.0f;
	switch (ObjectSnapshot.Type)
	{
	case EFF_SurfingObjectType::Log:
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("Log"), CylinderMesh, Material,
			FVector(0.0, 0.0, RadiusWorld * 0.12f), FRotator(0.0f, 90.0f, 0.0f),
			FVector(CylinderRadiusScale * 0.42f, CylinderRadiusScale * 0.42f,
				CylinderRadiusScale * 1.55f));
		break;
	case EFF_SurfingObjectType::Buoy:
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("Buoy"), ConeMesh, Material,
			FVector(0.0, 0.0, RadiusWorld * 0.68f), FRotator::ZeroRotator,
			FVector(SphereScale * 0.62f, SphereScale * 0.62f, SphereScale * 1.35f));
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("BuoyCap"), SphereMesh, Material,
			FVector(0.0, 0.0, RadiusWorld * 1.55f), FRotator::ZeroRotator,
			FVector(SphereScale * 0.22f));
		break;
	case EFF_SurfingObjectType::Coin:
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("Coin"), CylinderMesh, Material,
			FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f),
			FVector(CylinderRadiusScale, CylinderRadiusScale, CylinderRadiusScale * 0.18f));
		break;
	case EFF_SurfingObjectType::Heart:
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("HeartPoint"), ConeMesh, Material,
			FVector(0.0, 0.0, -RadiusWorld * 0.20f), FRotator(180.0f, 0.0f, 0.0f),
			FVector(SphereScale * 0.72f, SphereScale * 0.58f, SphereScale * 0.85f));
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("HeartLeft"), SphereMesh, Material,
			FVector(0.0, -RadiusWorld * 0.34f, RadiusWorld * 0.32f), FRotator::ZeroRotator,
			FVector(SphereScale * 0.56f));
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("HeartRight"), SphereMesh, Material,
			FVector(0.0, RadiusWorld * 0.34f, RadiusWorld * 0.32f), FRotator::ZeroRotator,
			FVector(SphereScale * 0.56f));
		break;
	case EFF_SurfingObjectType::BoostPickup:
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("BoostCore"), CylinderMesh, Material,
			FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f),
			FVector(CylinderRadiusScale * 0.62f, CylinderRadiusScale * 0.62f,
				CylinderRadiusScale * 0.74f));
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("BoostTip"), ConeMesh, Material,
			FVector(RadiusWorld * 0.82f, 0.0, 0.0), FRotator(0.0f, 90.0f, 0.0f),
			FVector(SphereScale * 0.52f, SphereScale * 0.52f, SphereScale * 0.72f));
		break;
	case EFF_SurfingObjectType::Rock:
	default:
		CreateObjectPart(
			*ObjectRoot, ObjectSnapshot.ObjectId, TEXT("Rock"), SphereMesh, Material,
			FVector(0.0, 0.0, RadiusWorld * 0.12f),
			FRotator(
				static_cast<float>((ObjectSnapshot.ObjectId * 17) % 25),
				static_cast<float>((ObjectSnapshot.ObjectId * 41) % 180),
				static_cast<float>((ObjectSnapshot.ObjectId * 23) % 20)),
			FVector(SphereScale * 1.08f, SphereScale * 0.88f, SphereScale * 0.72f));
		break;
	}

	return ObjectRoot;
}

void AFF_SurfingSceneActor::DestroyCourseObject(const int32 ObjectId)
{
	USceneComponent* ObjectRoot = CourseObjectRoots.FindRef(ObjectId);
	if (IsValid(ObjectRoot))
	{
		TArray<USceneComponent*> Descendants;
		ObjectRoot->GetChildrenComponents(true, Descendants);
		for (int32 Index = Descendants.Num() - 1; Index >= 0; --Index)
		{
			if (IsValid(Descendants[Index]))
			{
				Descendants[Index]->DestroyComponent();
			}
		}
		ObjectRoot->DestroyComponent();
	}
	CourseObjectRoots.Remove(ObjectId);
	CourseObjectTypes.Remove(ObjectId);
	CourseObjectTargetLocations.Remove(ObjectId);
}

void AFF_SurfingSceneActor::SetCourseObjectVisible(
	USceneComponent& ObjectRoot,
	const bool bVisible) const
{
	ObjectRoot.SetVisibility(bVisible, true);
	ObjectRoot.SetHiddenInGame(!bVisible, true);
}

UMaterialInterface* AFF_SurfingSceneActor::GetObjectMaterial(
	const EFF_SurfingObjectType Type) const
{
	const int32 Index = ObjectTypeIndex(Type);
	return ObjectMaterials.IsValidIndex(Index) ? ObjectMaterials[Index] : BasicShapeMaterial;
}

float AFF_SurfingSceneActor::GetObjectHoverHeight(
	const EFF_SurfingObjectType Type,
	const float RadiusWorld) const
{
	if (IsPickupType(Type))
	{
		return 112.0f + RadiusWorld * 0.35f;
	}
	return Type == EFF_SurfingObjectType::Buoy
		? 16.0f
		: 8.0f;
}

void AFF_SurfingSceneActor::ConfigureFixedVisuals()
{
	OceanFloor->SetStaticMesh(PlaneMesh);
	OceanSurface->SetStaticMesh(PlaneMesh);
	Surfboard->SetStaticMesh(CubeMesh);
	RiderBody->SetStaticMesh(CylinderMesh);
	RiderHead->SetStaticMesh(SphereMesh);
	RiderLeftArm->SetStaticMesh(CylinderMesh);
	RiderRightArm->SetStaticMesh(CylinderMesh);
	RiderLeftLeg->SetStaticMesh(CylinderMesh);
	RiderRightLeg->SetStaticMesh(CylinderMesh);
	WakeLeft->SetStaticMesh(CubeMesh);
	WakeRight->SetStaticMesh(CubeMesh);

	OceanFloor->SetMaterial(0, OceanMaterial);
	OceanSurface->SetMaterial(0, ProjectSeaMaterial ? ProjectSeaMaterial : OceanMaterial);
	Surfboard->SetMaterial(0, BoardMaterial);
	RiderBody->SetMaterial(0, RiderMaterial);
	RiderHead->SetMaterial(0, RiderSkinMaterial);
	RiderLeftArm->SetMaterial(0, RiderMaterial);
	RiderRightArm->SetMaterial(0, RiderMaterial);
	RiderLeftLeg->SetMaterial(0, RiderMaterial);
	RiderRightLeg->SetMaterial(0, RiderMaterial);
	WakeLeft->SetMaterial(0, FoamMaterial);
	WakeRight->SetMaterial(0, FoamMaterial);

	const FVector OceanScale(
		ArenaHalfForwardWorld / 50.0f * 1.35f,
		ArenaHalfForwardWorld * ArenaHorizontalMetricScale / 50.0f * 1.12f,
		1.0f);
	OceanFloor->SetRelativeLocation(FVector(0.0, 0.0, -48.0));
	OceanFloor->SetRelativeScale3D(OceanScale * FVector(1.06f, 1.08f, 1.0f));
	OceanSurface->SetRelativeLocation(FVector::ZeroVector);
	OceanSurface->SetRelativeScale3D(OceanScale);

	Surfboard->SetRelativeLocation(FVector::ZeroVector);
	Surfboard->SetRelativeScale3D(FVector(2.65f, 0.68f, 0.10f));
	RiderRoot->SetRelativeLocation(FVector::ZeroVector);
	RiderBody->SetRelativeLocation(FVector(-8.0, 0.0, 88.0));
	RiderBody->SetRelativeRotation(FRotator(0.0f, 8.0f, 0.0f));
	RiderBody->SetRelativeScale3D(FVector(0.29f, 0.29f, 0.78f));
	RiderHead->SetRelativeLocation(FVector(-4.0, 0.0, 148.0));
	RiderHead->SetRelativeScale3D(FVector(0.30f));
	RiderLeftArm->SetRelativeLocation(FVector(-4.0, -31.0, 106.0));
	RiderLeftArm->SetRelativeRotation(FRotator(18.0f, 0.0f, -58.0f));
	RiderLeftArm->SetRelativeScale3D(FVector(0.13f, 0.13f, 0.58f));
	RiderRightArm->SetRelativeLocation(FVector(-4.0, 31.0, 106.0));
	RiderRightArm->SetRelativeRotation(FRotator(18.0f, 0.0f, 58.0f));
	RiderRightArm->SetRelativeScale3D(FVector(0.13f, 0.13f, 0.58f));
	RiderLeftLeg->SetRelativeLocation(FVector(-39.0, -9.0, 42.0));
	RiderLeftLeg->SetRelativeRotation(FRotator(0.0f, -34.0f, -18.0f));
	RiderLeftLeg->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.58f));
	RiderRightLeg->SetRelativeLocation(FVector(38.0, 9.0, 42.0));
	RiderRightLeg->SetRelativeRotation(FRotator(0.0f, 34.0f, 18.0f));
	RiderRightLeg->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.58f));
	WakeLeft->SetRelativeRotation(FRotator(0.0f, -9.0f, 0.0f));
	WakeRight->SetRelativeRotation(FRotator(0.0f, 9.0f, 0.0f));

	for (int32 Index = 0; Index < WaveBands.Num(); ++Index)
	{
		UStaticMeshComponent* WaveBand = WaveBands[Index];
		WaveBand->SetStaticMesh(CubeMesh);
		WaveBand->SetMaterial(0, FoamMaterial);
		const float WidthScale = FMath::Lerp(
			4.5f,
			9.0f,
			static_cast<float>((Index * 37) % 100) / 100.0f);
		WaveBand->SetRelativeScale3D(FVector(0.16f, WidthScale, 0.018f));
	}

	SurferRoot->SetRelativeLocation(PlayerTargetLocation);
	TArray<UStaticMeshComponent*> CaptureMeshes;
	GetComponents<UStaticMeshComponent>(CaptureMeshes);
	for (UStaticMeshComponent* MeshComponent : CaptureMeshes)
	{
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComponent->SetCanEverAffectNavigation(false);
			MeshComponent->SetVisibleInSceneCaptureOnly(true);
		}
	}
}
