// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_SurfingSceneActor.h"

#include "Components/ChildActorComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/Scene.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Minigame/Surfing/FF_SurfingRules.h"
#include "UI/Minigame/FF_SurfingScreen.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr TCHAR SurfingScenePlaneMeshPath[] = TEXT("/Engine/BasicShapes/Plane.Plane");
	constexpr TCHAR SurfingSceneCubeMeshPath[] = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr TCHAR SurfingSceneSphereMeshPath[] = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	constexpr TCHAR SurfingSceneCylinderMeshPath[] = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	constexpr TCHAR SurfingSceneConeMeshPath[] = TEXT("/Engine/BasicShapes/Cone.Cone");
	constexpr TCHAR SurfingSceneBasicShapeMaterialPath[] =
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	constexpr TCHAR SurfingSceneProjectSeaMaterialPath[] =
		TEXT("/Game/Level/Demo_Matereal/Sea_Material/Mi_Sea_Inst.Mi_Sea_Inst");
	constexpr float CharacterCaptureRefreshIntervalSeconds = 0.5f;

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
	SceneCapture->ShowFlags.SetAtmosphere(false);
	SceneCapture->ShowFlags.SetFog(false);
	SceneCapture->ShowFlags.SetVolumetricFog(false);
	SceneCapture->ShowFlags.SetSkyLighting(false);
	SceneCapture->ShowFlags.SetReflectionEnvironment(false);
	SceneCapture->ShowFlags.SetScreenSpaceReflections(false);
	SceneCapture->ShowOnlyActors.Add(this);

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetCastShadows(true);
	KeyLight->SetLightingChannels(false, true, false);
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetCastShadows(false);
	FillLight->SetLightingChannels(false, true, false);

	OceanFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanFloor"));
	OceanFloor->SetupAttachment(SceneRoot);
	OceanSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanSurface"));
	OceanSurface->SetupAttachment(SceneRoot);
	SkyBackdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyBackdrop"));
	SkyBackdrop->SetupAttachment(SceneRoot);

	SurferRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SurferRoot"));
	SurferRoot->SetupAttachment(SceneRoot);
	SurfingCharacterHost = CreateDefaultSubobject<UChildActorComponent>(
		TEXT("SurfingCharacterHost"));
	SurfingCharacterHost->SetupAttachment(SurferRoot);

	for (int32 Index = 0; Index < 7; ++Index)
	{
		UStaticMeshComponent* WaveBand = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("WaveBand_%02d"), Index + 1));
		WaveBand->SetupAttachment(SceneRoot);
		WaveBands.Add(WaveBand);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		SurfingScenePlaneMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		SurfingSceneCubeMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		SurfingSceneSphereMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		SurfingSceneCylinderMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(
		SurfingSceneConeMeshPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(
		SurfingSceneBasicShapeMaterialPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SeaMaterialFinder(
		SurfingSceneProjectSeaMaterialPath);

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
		SkyBackdrop,
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
		MeshComponent->SetLightingChannels(false, true, false);
		MeshComponent->SetCastShadow(
			MeshComponent != OceanFloor
			&& MeshComponent != OceanSurface
			&& MeshComponent != SkyBackdrop
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
	CameraLateralFollowRatio = FMath::Clamp(
		VisualStyle.CameraLateralFollowRatio, 0.0f, 0.9f);
	PlayerPositionSmoothingSpeed = FMath::Max(0.01f, VisualStyle.PlayerPositionSmoothingSpeed);
	PlayerRotationSmoothingSpeed = FMath::Max(0.01f, VisualStyle.PlayerRotationSmoothingSpeed);
	bResetPresentationLagOnDirectionChange =
		VisualStyle.bResetPresentationLagOnDirectionChange;
	LastPresentedLateralDirection = 0.0f;
	bHasPresentedLateralDirection = false;
	CameraPositionSmoothingSpeed = FMath::Max(0.01f, VisualStyle.CameraPositionSmoothingSpeed);
	CameraRotationSmoothingSpeed = FMath::Max(0.01f, VisualStyle.CameraRotationSmoothingSpeed);
	CourseObjectSmoothingSpeed = FMath::Max(0.01f, VisualStyle.CourseObjectSmoothingSpeed);
	SurferBobAmplitudeCm = FMath::Max(0.0f, VisualStyle.SurferBobAmplitudeCm);
	TurnBankScale = FMath::Max(0.0f, VisualStyle.TurnBankScale);
	MaximumBankDegrees = FMath::Clamp(VisualStyle.MaximumBankDegrees, 0.0f, 45.0f);
	CharacterCaptureRefreshRemainingSeconds = CharacterCaptureRefreshIntervalSeconds;
	SceneCapture->bCaptureEveryFrame = true;
	SceneCapture->ShowOnlyActors.Reset();
	SceneCapture->ShowOnlyActors.Add(this);
	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->FOVAngle = FMath::Clamp(VisualStyle.Camera3DFov, 30.0f, 120.0f);
	SceneCapture->SetRelativeLocation(CameraFollowOffset);
	SceneCapture->SetRelativeRotation(
		(CameraLookAtOffset - CameraFollowOffset).Rotation());
	SceneCapture->MaxViewDistanceOverride = ArenaHalfForwardWorld * 8.0f;
	SceneCapture->PostProcessBlendWeight = 1.0f;
	SceneCapture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	SceneCapture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	SceneCapture->PostProcessSettings.bOverride_AutoExposureBias = true;
	SceneCapture->PostProcessSettings.AutoExposureBias = FMath::Clamp(
		VisualStyle.DaylightExposureBias, -2.0f, 4.0f);
	SceneCapture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	SceneCapture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	KeyLight->SetIntensity(FMath::Max(0.0f, VisualStyle.SceneKeyLightIntensity) / 10000.0f);
	KeyLight->SetLightColor(FLinearColor(1.0f, 0.94f, 0.82f));
	KeyLight->SetRelativeRotation(FRotator(-52.0f, -28.0f, 0.0f));
	FillLight->SetIntensity(FMath::Max(0.0f, VisualStyle.SceneFillLightIntensity));
	FillLight->SetAttenuationRadius(ArenaHalfForwardWorld * 2.4f);
	FillLight->SetLightColor(FLinearColor(0.55f, 0.84f, 1.0f));
	FillLight->SetRelativeLocation(FVector(1350.0, 1550.0, 1650.0));

	OceanMaterial = CreateColorMaterial(
		TEXT("MI_SurfingOceanFloor"), VisualStyle.OceanDeepColor);
	OceanSurfaceMaterial = CreateOceanSurfaceMaterial(
		VisualStyle.OceanColor, VisualStyle.FoamColor);
	SkyMaterial = CreateColorMaterial(TEXT("MI_SurfingDaylightSky"), VisualStyle.DaylightSkyColor);
	FoamMaterial = CreateColorMaterial(TEXT("MI_SurfingFoam"), VisualStyle.FoamColor);
	CharacterVisualStyle.BoardColor = VisualStyle.SurferColor;
	CharacterVisualStyle.BoostBoardColor = VisualStyle.BoostingSurferColor;
	CharacterVisualStyle.OutfitColor = VisualStyle.RiderSuitColor;
	CharacterVisualStyle.SkinColor = VisualStyle.RiderSkinColor;
	CharacterVisualStyle.WakeColor = VisualStyle.FoamColor;

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
	const bool bCharacterReady = SetSurfingCharacterClass(
		VisualStyle.SurfingCharacterClass);
	bSceneInitialized = bCharacterReady && IsSceneReady();
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
	CharacterCaptureActors.Reset();
	if (AFF_SurfingCharacterActor* Character = GetSurfingCharacterActor())
	{
		Character->ReleaseCharacter();
	}
	if (SurfingCharacterHost)
	{
		SurfingCharacterHost->DestroyChildActor();
		SurfingCharacterHost->SetChildActorClass(nullptr);
	}
	VisibleCourseObjectIdsLastFrame.Reset();
}

void AFF_SurfingSceneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseScene();
	Super::EndPlay(EndPlayReason);
}

bool AFF_SurfingSceneActor::SetSurfingCharacterClass(
	TSubclassOf<AFF_SurfingCharacterActor> InCharacterClass)
{
	if (!SurfingCharacterHost || !SceneCapture)
	{
		return false;
	}

	TSubclassOf<AFF_SurfingCharacterActor> CharacterClass = InCharacterClass;
	if (!CharacterClass
		|| CharacterClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
	{
		CharacterClass = AFF_SurfingPrimitiveCharacterActor::StaticClass();
	}

	const bool bWasCapturing = SceneCapture->bCaptureEveryFrame;
	SceneCapture->bCaptureEveryFrame = false;
	ClearCharacterCaptureActors();
	if (AFF_SurfingCharacterActor* PreviousCharacter = GetSurfingCharacterActor())
	{
		SceneCapture->ShowOnlyActors.Remove(PreviousCharacter);
		PreviousCharacter->ReleaseCharacter();
	}
	SurfingCharacterHost->DestroyChildActor();
	SurfingCharacterHost->SetChildActorClass(CharacterClass);
	if (!SurfingCharacterHost->GetChildActor() && SurfingCharacterHost->IsRegistered())
	{
		SurfingCharacterHost->CreateChildActor();
	}

	AFF_SurfingCharacterActor* Character = GetSurfingCharacterActor();
	bool bReady = IsValid(Character)
		&& Character->InitializeCharacter(CharacterVisualStyle);
	if (!bReady && CharacterClass != AFF_SurfingPrimitiveCharacterActor::StaticClass())
	{
		if (Character)
		{
			Character->ReleaseCharacter();
		}
		SurfingCharacterHost->DestroyChildActor();
		SurfingCharacterHost->SetChildActorClass(
			AFF_SurfingPrimitiveCharacterActor::StaticClass());
		if (!SurfingCharacterHost->GetChildActor() && SurfingCharacterHost->IsRegistered())
		{
			SurfingCharacterHost->CreateChildActor();
		}
		Character = GetSurfingCharacterActor();
		bReady = IsValid(Character)
			&& Character->InitializeCharacter(CharacterVisualStyle);
	}
	if (bReady)
	{
		RefreshCharacterCaptureActors();
		ApplyCharacterPresentationState(0.0f);
	}
	else if (bSceneInitialized)
	{
		bSceneInitialized = false;
		SetActorTickEnabled(false);
	}
	SceneCapture->bCaptureEveryFrame = bWasCapturing;
	if (bReady && SceneCapture->TextureTarget)
	{
		SceneCapture->CaptureSceneDeferred();
	}
	return bReady;
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
	const float LateralDirection = FMath::Clamp(
		static_cast<float>(Snapshot.MovementInput.X), -1.0f, 1.0f);
	const bool bDirectionChanged = FMath::Abs(LateralDirection) > UE_KINDA_SMALL_NUMBER
		&& bHasPresentedLateralDirection
		&& LateralDirection * LastPresentedLateralDirection < 0.0f;
	if (FMath::Abs(LateralDirection) > UE_KINDA_SMALL_NUMBER)
	{
		LastPresentedLateralDirection = LateralDirection;
		bHasPresentedLateralDirection = true;
	}
	if (!bHasPlayerTransform)
	{
		SurferRoot->SetRelativeLocation(PlayerTargetLocation);
		SurferRoot->SetRelativeRotation(FRotator(0.0f, PlayerTargetYaw, 0.0f));
		FVector CameraAnchor = PlayerTargetLocation;
		CameraAnchor.Y *= CameraLateralFollowRatio;
		SceneCapture->SetRelativeLocation(CameraAnchor + CameraFollowOffset);
		SceneCapture->SetRelativeRotation((
			CameraAnchor + CameraLookAtOffset
			- SceneCapture->GetRelativeLocation()).Rotation());
		bHasPlayerTransform = true;
	}
	else if (bDirectionChanged && bResetPresentationLagOnDirectionChange)
	{
		SurferRoot->SetRelativeLocation(PlayerTargetLocation);
		SurferRoot->SetRelativeRotation(FRotator(0.0f, PlayerTargetYaw, 0.0f));
	}

	bBoosting = Snapshot.bBoosting;
	bInvincible = Snapshot.bInvincible;

	if (Snapshot.LatestEventSerial > 0
		&& Snapshot.LatestEventSerial != PresentedEventSerial)
	{
		PresentedEventSerial = Snapshot.LatestEventSerial;
	}
	ApplyCharacterPresentationState(0.0f);

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
		const bool bWasVisible = VisibleCourseObjectIdsLastFrame.Contains(
			ObjectSnapshot.ObjectId);
		const bool bRecycledJump = !bNewObject
			&& FVector::DistSquared(ObjectRoot->GetRelativeLocation(), TargetLocation)
				> FMath::Square(ArenaHalfForwardWorld * 1.5f);
		if (bNewObject || !bWasVisible || bRecycledJump)
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
	VisibleCourseObjectIdsLastFrame = VisibleIds;
}

void AFF_SurfingSceneActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSceneInitialized || DeltaSeconds <= 0.0f)
	{
		return;
	}

	SceneSeconds += DeltaSeconds;
	CharacterCaptureRefreshRemainingSeconds -= DeltaSeconds;
	if (CharacterCaptureRefreshRemainingSeconds <= 0.0f)
	{
		RefreshCharacterCaptureActors();
		CharacterCaptureRefreshRemainingSeconds = CharacterCaptureRefreshIntervalSeconds;
	}

	UpdateWaterPresentation(SceneSeconds);

	const FVector BobbedPlayerTarget = PlayerTargetLocation + FVector(
		0.0,
		0.0,
		FMath::Sin(SceneSeconds * 3.4f) * SurferBobAmplitudeCm);
	const float PlayerPositionResponse = FFSurfingRules::CalculateExponentialResponse(
		PlayerPositionSmoothingSpeed, DeltaSeconds);
	SurferRoot->SetRelativeLocation(FMath::Lerp(
		SurferRoot->GetRelativeLocation(), BobbedPlayerTarget, PlayerPositionResponse));

	const FRotator CurrentRotation = SurferRoot->GetRelativeRotation();
	const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, PlayerTargetYaw);
	const float TargetBank = FMath::Clamp(
		-YawDelta * TurnBankScale, -MaximumBankDegrees, MaximumBankDegrees);
	const FRotator DesiredRotation(
		FMath::Sin(SceneSeconds * 2.1f) * 1.8f,
		PlayerTargetYaw,
		TargetBank);
	const float PlayerRotationResponse = FFSurfingRules::CalculateExponentialResponse(
		PlayerRotationSmoothingSpeed, DeltaSeconds);
	SurferRoot->SetRelativeRotation(FQuat::Slerp(
		CurrentRotation.Quaternion(),
		DesiredRotation.Quaternion(),
		PlayerRotationResponse).GetNormalized());
	FVector CameraAnchor = PlayerTargetLocation;
	CameraAnchor.Y *= CameraLateralFollowRatio;
	const FVector DesiredCameraLocation = CameraAnchor + CameraFollowOffset;
	const float CameraPositionResponse = FFSurfingRules::CalculateExponentialResponse(
		CameraPositionSmoothingSpeed, DeltaSeconds);
	SceneCapture->SetRelativeLocation(FMath::Lerp(
		SceneCapture->GetRelativeLocation(),
		DesiredCameraLocation,
		CameraPositionResponse));
	const FRotator DesiredCameraRotation = (
		CameraAnchor + CameraLookAtOffset
		- SceneCapture->GetRelativeLocation()).Rotation();
	const float CameraRotationResponse = FFSurfingRules::CalculateExponentialResponse(
		CameraRotationSmoothingSpeed, DeltaSeconds);
	SceneCapture->SetRelativeRotation(FQuat::Slerp(
		SceneCapture->GetRelativeRotation().Quaternion(),
		DesiredCameraRotation.Quaternion(),
		CameraRotationResponse).GetNormalized());
	ApplyCharacterPresentationState(TargetBank);
	if (AFF_SurfingCharacterActor* Character = GetSurfingCharacterActor())
	{
		Character->AdvancePresentation(DeltaSeconds);
	}
	const float CourseObjectResponse = FFSurfingRules::CalculateExponentialResponse(
		CourseObjectSmoothingSpeed, DeltaSeconds);
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
		ObjectRoot->SetRelativeLocation(FMath::Lerp(
			ObjectRoot->GetRelativeLocation(), BobbedTarget, CourseObjectResponse));

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
		&& IsValid(SurfingCharacterHost)
		&& IsValid(GetSurfingCharacterActor())
		&& GetSurfingCharacterActor()->IsCharacterReady()
		&& IsValid(PlaneMesh)
		&& IsValid(CubeMesh)
		&& IsValid(SphereMesh)
		&& IsValid(CylinderMesh)
		&& IsValid(ConeMesh)
		&& IsValid(BasicShapeMaterial)
		&& IsValid(OceanMaterial)
		&& IsValid(OceanSurfaceMaterial)
		&& ObjectMaterials.Num() == 6;
}

int32 AFF_SurfingSceneActor::GetVisibleCourseObjectCount() const
{
	return CourseObjectTargetLocations.Num();
}

int32 AFF_SurfingSceneActor::GetRiderMeshPartCount() const
{
	const AFF_SurfingCharacterActor* Character = GetSurfingCharacterActor();
	return Character ? Character->GetRiderMeshPartCount() : 0;
}

int32 AFF_SurfingSceneActor::GetCharacterRenderablePrimitiveCount() const
{
	const AFF_SurfingCharacterActor* Character = GetSurfingCharacterActor();
	return Character ? Character->GetRenderablePrimitiveCount() : 0;
}

AFF_SurfingCharacterActor* AFF_SurfingSceneActor::GetSurfingCharacterActor() const
{
	return SurfingCharacterHost
		? Cast<AFF_SurfingCharacterActor>(SurfingCharacterHost->GetChildActor())
		: nullptr;
}

void AFF_SurfingSceneActor::ClearCharacterCaptureActors()
{
	if (SceneCapture)
	{
		for (const TWeakObjectPtr<AActor>& CapturedActor : CharacterCaptureActors)
		{
			if (AActor* Actor = CapturedActor.Get())
			{
				SceneCapture->ShowOnlyActors.Remove(Actor);
			}
		}
	}
	CharacterCaptureActors.Reset();
}

void AFF_SurfingSceneActor::RefreshCharacterCaptureActors()
{
	AFF_SurfingCharacterActor* Character = GetSurfingCharacterActor();
	if (!SceneCapture || !IsValid(Character))
	{
		ClearCharacterCaptureActors();
		return;
	}

	TArray<AActor*> ReportedActors;
	Character->GetPresentationActors(ReportedActors);
	TArray<AActor*> PresentationActors;
	PresentationActors.Reserve(ReportedActors.Num());
	for (AActor* ReportedActor : ReportedActors)
	{
		if (IsValid(ReportedActor))
		{
			PresentationActors.AddUnique(ReportedActor);
		}
	}

	bool bCaptureSetChanged = PresentationActors.Num() != CharacterCaptureActors.Num();
	if (!bCaptureSetChanged)
	{
		for (AActor* PresentationActor : PresentationActors)
		{
			const bool bAlreadyCaptured = CharacterCaptureActors.ContainsByPredicate(
				[PresentationActor](const TWeakObjectPtr<AActor>& CapturedActor)
				{
					return CapturedActor.Get() == PresentationActor;
				});
			if (!bAlreadyCaptured)
			{
				bCaptureSetChanged = true;
				break;
			}
		}
	}
	if (!bCaptureSetChanged)
	{
		return;
	}

	ClearCharacterCaptureActors();
	for (AActor* PresentationActor : PresentationActors)
	{
		SceneCapture->ShowOnlyActors.AddUnique(PresentationActor);
		CharacterCaptureActors.Add(PresentationActor);
	}
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

UMaterialInstanceDynamic* AFF_SurfingSceneActor::CreateOceanSurfaceMaterial(
	const FLinearColor& OceanColor,
	const FLinearColor& FoamColor)
{
	// The project sea instance was authored for a level with skylight/exponential
	// fog and renders nearly black in this isolated capture. The bright local
	// material plus moving foam bands gives deterministic daylight in every map.
	UMaterialInterface* ParentMaterial = BasicShapeMaterial;
	if (!ParentMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
		ParentMaterial, this, TEXT("MI_SurfingDaylightOcean"));
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), OceanColor);
		Material->SetVectorParameterValue(TEXT("BaseColor"), OceanColor);
		Material->SetVectorParameterValue(TEXT("Tint"), OceanColor);
		Material->SetVectorParameterValue(TEXT("Color_Coast"), FoamColor);
	}
	return Material;
}

void AFF_SurfingSceneActor::ApplyCharacterPresentationState(const float BankDegrees)
{
	if (AFF_SurfingCharacterActor* Character = GetSurfingCharacterActor())
	{
		FFF_SurfingCharacterPresentationState State;
		State.bBoosting = bBoosting;
		State.bInvincible = bInvincible;
		State.BankDegrees = BankDegrees;
		State.EventSerial = PresentedEventSerial;
		Character->ApplyPresentationState(State);
	}
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
	Part->SetLightingChannels(false, true, false);
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
	SkyBackdrop->SetStaticMesh(CubeMesh);

	OceanFloor->SetMaterial(0, OceanMaterial);
	OceanSurface->SetMaterial(0, OceanSurfaceMaterial);
	SkyBackdrop->SetMaterial(0, SkyMaterial);

	const FVector OceanScale(
		ArenaHalfForwardWorld / 50.0f * 5.0f,
		ArenaHalfForwardWorld * ArenaHorizontalMetricScale / 50.0f * 2.25f,
		1.0f);
	OceanFloor->SetRelativeLocation(FVector(0.0, 0.0, -180.0));
	OceanFloor->SetRelativeScale3D(OceanScale * FVector(1.06f, 1.08f, 1.0f));
	OceanSurface->SetRelativeLocation(FVector::ZeroVector);
	OceanSurface->SetRelativeScale3D(OceanScale);
	SkyBackdrop->SetRelativeLocation(FVector(
		ArenaHalfForwardWorld * 4.45f,
		0.0f,
		ArenaHalfForwardWorld * 1.55f));
	SkyBackdrop->SetRelativeScale3D(FVector(
		0.08f,
		ArenaHalfForwardWorld * ArenaHorizontalMetricScale / 50.0f * 2.6f,
		ArenaHalfForwardWorld / 50.0f * 1.55f));

	for (int32 Index = 0; Index < WaveBands.Num(); ++Index)
	{
		UStaticMeshComponent* WaveBand = WaveBands[Index];
		WaveBand->SetStaticMesh(CubeMesh);
		WaveBand->SetMaterial(0, FoamMaterial);
		const float WidthScale = FMath::Lerp(
			3.0f,
			6.0f,
			static_cast<float>((Index * 37) % 100) / 100.0f);
		WaveBand->SetRelativeScale3D(FVector(0.11f, WidthScale, 0.010f));
	}
	UpdateWaterPresentation(SceneSeconds);

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
			MeshComponent->SetLightingChannels(false, true, false);
		}
	}
}

void AFF_SurfingSceneActor::UpdateWaterPresentation(const float PresentationSeconds)
{
	OceanSurface->SetRelativeRotation(FRotator(
		FMath::Sin(PresentationSeconds * 0.42f) * 0.12f,
		0.0f,
		FMath::Cos(PresentationSeconds * 0.31f) * 0.15f));

	const float WaveSpan = ArenaHalfForwardWorld * 2.35f;
	for (int32 Index = 0; Index < WaveBands.Num(); ++Index)
	{
		UStaticMeshComponent* WaveBand = WaveBands[Index];
		const float Wrapped = FMath::Fmod(
			PresentationSeconds * 165.0f
				+ static_cast<float>(Index) * WaveSpan / WaveBands.Num(),
			WaveSpan);
		const float X = ArenaHalfForwardWorld * 1.18f - Wrapped;
		const float Y = FMath::Sin(PresentationSeconds * 0.55f + Index * 1.37f)
			* ArenaHalfForwardWorld * ArenaHorizontalMetricScale * 0.18f;
		WaveBand->SetRelativeLocation(FVector(X, Y, 7.0f + (Index % 2) * 2.0f));
		WaveBand->SetRelativeRotation(FRotator(
			0.0f,
			FMath::Sin(Index * 1.9f) * 5.0f,
			0.0f));
	}
}
