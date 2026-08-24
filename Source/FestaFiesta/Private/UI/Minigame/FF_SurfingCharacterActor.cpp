// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_SurfingCharacterActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr TCHAR SurfingCharacterCubeMeshPath[] = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr TCHAR SurfingCharacterSphereMeshPath[] = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	constexpr TCHAR SurfingCharacterCylinderMeshPath[] = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	constexpr TCHAR SurfingCharacterBasicShapeMaterialPath[] =
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	constexpr float EventPulseDurationSeconds = 0.32f;

	FLinearColor SanitizedColor(
		const FLinearColor& Candidate,
		const FLinearColor& Fallback)
	{
		if (!FMath::IsFinite(Candidate.R)
			|| !FMath::IsFinite(Candidate.G)
			|| !FMath::IsFinite(Candidate.B)
			|| !FMath::IsFinite(Candidate.A))
		{
			return Fallback;
		}

		return FLinearColor(
			FMath::Clamp(Candidate.R, 0.0f, 16.0f),
			FMath::Clamp(Candidate.G, 0.0f, 16.0f),
			FMath::Clamp(Candidate.B, 0.0f, 16.0f),
			FMath::Clamp(Candidate.A, 0.0f, 1.0f));
	}

	bool IsRenderablePresentationPrimitive(const UPrimitiveComponent& PrimitiveComponent)
	{
		if (!PrimitiveComponent.IsRegistered()
			|| !PrimitiveComponent.IsVisible()
			|| PrimitiveComponent.bHiddenInGame
			|| (PrimitiveComponent.GetOwner() && PrimitiveComponent.GetOwner()->IsHidden()))
		{
			return false;
		}

		if (const UStaticMeshComponent* StaticMesh =
			Cast<UStaticMeshComponent>(&PrimitiveComponent))
		{
			return IsValid(StaticMesh->GetStaticMesh());
		}
		if (const USkeletalMeshComponent* SkeletalMesh =
			Cast<USkeletalMeshComponent>(&PrimitiveComponent))
		{
			return IsValid(SkeletalMesh->GetSkeletalMeshAsset());
		}
		return !PrimitiveComponent.IsA<UShapeComponent>()
			&& PrimitiveComponent.CanEverRender();
	}
}

AFF_SurfingCharacterActor::AFF_SurfingCharacterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);

	CharacterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CharacterRoot"));
	SetRootComponent(CharacterRoot);
}

void AFF_SurfingCharacterActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseCharacter();
	Super::EndPlay(EndPlayReason);
}

bool AFF_SurfingCharacterActor::InitializeCharacter(
	const FFF_SurfingCharacterVisualStyle& InVisualStyle)
{
	if (bInitializationAttempted)
	{
		ReleaseCharacter();
	}

	CharacterVisualStyle = InVisualStyle;
	CharacterVisualStyle.BoardColor = SanitizedColor(
		InVisualStyle.BoardColor,
		FFF_SurfingCharacterVisualStyle().BoardColor);
	CharacterVisualStyle.BoostBoardColor = SanitizedColor(
		InVisualStyle.BoostBoardColor,
		FFF_SurfingCharacterVisualStyle().BoostBoardColor);
	CharacterVisualStyle.OutfitColor = SanitizedColor(
		InVisualStyle.OutfitColor,
		FFF_SurfingCharacterVisualStyle().OutfitColor);
	CharacterVisualStyle.SkinColor = SanitizedColor(
		InVisualStyle.SkinColor,
		FFF_SurfingCharacterVisualStyle().SkinColor);
	CharacterVisualStyle.WakeColor = SanitizedColor(
		InVisualStyle.WakeColor,
		FFF_SurfingCharacterVisualStyle().WakeColor);

	PresentationState = FFF_SurfingCharacterPresentationState();
	PresentationSeconds = 0.0f;
	EventPulseRemainingSeconds = 0.0f;
	bInitializationAttempted = true;
	bCharacterReady = false;

	OnInitializeCharacter(CharacterVisualStyle);
	EnforcePrimitiveIsolation();
	OnPresentationStateChanged(PresentationState);
	EnforcePrimitiveIsolation();
	bCharacterReady = EvaluateCharacterReady();
	return IsCharacterReady();
}

void AFF_SurfingCharacterActor::ApplyPresentationState(
	const FFF_SurfingCharacterPresentationState& InPresentationState)
{
	FFF_SurfingCharacterPresentationState SanitizedState = InPresentationState;
	SanitizedState.BankDegrees = FMath::IsFinite(InPresentationState.BankDegrees)
		? FMath::Clamp(InPresentationState.BankDegrees, -45.0f, 45.0f)
		: 0.0f;
	SanitizedState.EventSerial = FMath::Max(0, InPresentationState.EventSerial);

	if (SanitizedState.EventSerial > 0
		&& SanitizedState.EventSerial != PresentationState.EventSerial)
	{
		EventPulseRemainingSeconds = EventPulseDurationSeconds;
	}
	PresentationState = SanitizedState;

	if (bInitializationAttempted)
	{
		OnPresentationStateChanged(PresentationState);
		EnforcePrimitiveIsolation();
	}
}

void AFF_SurfingCharacterActor::AdvancePresentation(const float DeltaSeconds)
{
	if (!IsCharacterReady() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Presentation is deliberately bounded so a hitch cannot fling a replacement rig.
	const float SafeDeltaSeconds = FMath::Min(DeltaSeconds, 0.25f);
	PresentationSeconds += SafeDeltaSeconds;
	EventPulseRemainingSeconds = FMath::Max(
		0.0f,
		EventPulseRemainingSeconds - SafeDeltaSeconds);
	OnAdvancePresentation(SafeDeltaSeconds, PresentationSeconds);
	EnforcePrimitiveIsolation();
}

void AFF_SurfingCharacterActor::ReleaseCharacter()
{
	if (!bInitializationAttempted || bReleaseInProgress)
	{
		return;
	}

	bReleaseInProgress = true;
	OnReleaseCharacter();
	EnforcePrimitiveIsolation();
	bCharacterReady = false;
	bInitializationAttempted = false;
	PresentationState = FFF_SurfingCharacterPresentationState();
	PresentationSeconds = 0.0f;
	EventPulseRemainingSeconds = 0.0f;
	bReleaseInProgress = false;
}

bool AFF_SurfingCharacterActor::HasRenderableCharacter() const
{
	return bInitializationAttempted && GetRenderablePrimitiveCount() > 0;
}

int32 AFF_SurfingCharacterActor::GetRenderablePrimitiveCount() const
{
	int32 RenderableCount = 0;
	TArray<AActor*> PresentationActors;
	GetPresentationActors(PresentationActors);
	for (const AActor* PresentationActor : PresentationActors)
	{
		if (!IsValid(PresentationActor))
		{
			continue;
		}
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		PresentationActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			RenderableCount += IsValid(PrimitiveComponent)
				&& IsRenderablePresentationPrimitive(*PrimitiveComponent) ? 1 : 0;
		}
	}
	return RenderableCount;
}

void AFF_SurfingCharacterActor::GetPresentationActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	OutActors.Add(const_cast<AFF_SurfingCharacterActor*>(this));
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (IsValid(AttachedActor))
		{
			OutActors.AddUnique(AttachedActor);
		}
	}
}

int32 AFF_SurfingCharacterActor::GetRiderMeshPartCount() const
{
	return bInitializationAttempted
		? FMath::Max(0, EvaluateRiderMeshPartCount())
		: 0;
}

float AFF_SurfingCharacterActor::GetEventPulseAlpha() const
{
	return FMath::Clamp(
		EventPulseRemainingSeconds / EventPulseDurationSeconds,
		0.0f,
		1.0f);
}

void AFF_SurfingCharacterActor::EnforcePrimitiveIsolation()
{
	TArray<AActor*> PresentationActors;
	GetPresentationActors(PresentationActors);
	for (AActor* PresentationActor : PresentationActors)
	{
		if (!IsValid(PresentationActor))
		{
			continue;
		}
		PresentationActor->SetActorEnableCollision(false);
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		PresentationActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!PrimitiveComponent)
			{
				continue;
			}
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComponent->SetGenerateOverlapEvents(false);
			PrimitiveComponent->SetCanEverAffectNavigation(false);
			PrimitiveComponent->SetVisibleInSceneCaptureOnly(true);
			PrimitiveComponent->SetLightingChannels(false, true, false);
		}
	}
}

void AFF_SurfingCharacterActor::OnInitializeCharacter_Implementation(
	const FFF_SurfingCharacterVisualStyle& InVisualStyle)
{
}

void AFF_SurfingCharacterActor::OnPresentationStateChanged_Implementation(
	const FFF_SurfingCharacterPresentationState& InPresentationState)
{
}

void AFF_SurfingCharacterActor::OnAdvancePresentation_Implementation(
	const float DeltaSeconds,
	const float InPresentationSeconds)
{
}

void AFF_SurfingCharacterActor::OnReleaseCharacter_Implementation()
{
}

bool AFF_SurfingCharacterActor::EvaluateCharacterReady_Implementation() const
{
	return GetRenderablePrimitiveCount() > 0;
}

int32 AFF_SurfingCharacterActor::EvaluateRiderMeshPartCount_Implementation() const
{
	return GetRenderablePrimitiveCount();
}

AFF_SurfingPrimitiveCharacterActor::AFF_SurfingPrimitiveCharacterActor()
{
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(CharacterRoot);
	Surfboard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surfboard"));
	Surfboard->SetupAttachment(VisualRoot);
	RiderRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RiderRoot"));
	RiderRoot->SetupAttachment(VisualRoot);
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
	WakeLeft->SetupAttachment(VisualRoot);
	WakeRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WakeRight"));
	WakeRight->SetupAttachment(VisualRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		SurfingCharacterCubeMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		SurfingCharacterSphereMeshPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		SurfingCharacterCylinderMeshPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(
		SurfingCharacterBasicShapeMaterialPath);
	CubeMesh = CubeFinder.Object;
	SphereMesh = SphereFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	BasicShapeMaterial = BasicMaterialFinder.Object;

	UStaticMeshComponent* ShadowMeshes[] = {
		Surfboard,
		RiderBody,
		RiderHead,
		RiderLeftArm,
		RiderRightArm,
		RiderLeftLeg,
		RiderRightLeg,
	};
	for (UStaticMeshComponent* MeshComponent : ShadowMeshes)
	{
		ConfigurePrimitive(*MeshComponent, true);
	}
	ConfigurePrimitive(*WakeLeft, false);
	ConfigurePrimitive(*WakeRight, false);
	ConfigureGeometry();
	EnforcePrimitiveIsolation();
}

void AFF_SurfingPrimitiveCharacterActor::OnInitializeCharacter_Implementation(
	const FFF_SurfingCharacterVisualStyle& InVisualStyle)
{
	BoardMaterial = CreateColorMaterial(TEXT("MI_SurfingCharacterBoard"), InVisualStyle.BoardColor);
	BoostBoardMaterial = CreateColorMaterial(
		TEXT("MI_SurfingCharacterBoostBoard"),
		InVisualStyle.BoostBoardColor);
	OutfitMaterial = CreateColorMaterial(
		TEXT("MI_SurfingCharacterOutfit"),
		InVisualStyle.OutfitColor);
	SkinMaterial = CreateColorMaterial(
		TEXT("MI_SurfingCharacterSkin"),
		InVisualStyle.SkinColor);
	WakeMaterial = CreateColorMaterial(
		TEXT("MI_SurfingCharacterWake"),
		InVisualStyle.WakeColor);

	ConfigureGeometry();
	Surfboard->SetMaterial(0, BoardMaterial);
	RiderBody->SetMaterial(0, OutfitMaterial);
	RiderHead->SetMaterial(0, SkinMaterial);
	RiderLeftArm->SetMaterial(0, OutfitMaterial);
	RiderRightArm->SetMaterial(0, OutfitMaterial);
	RiderLeftLeg->SetMaterial(0, OutfitMaterial);
	RiderRightLeg->SetMaterial(0, OutfitMaterial);
	WakeLeft->SetMaterial(0, WakeMaterial);
	WakeRight->SetMaterial(0, WakeMaterial);
	VisualRoot->SetRelativeScale3D(FVector::OneVector);
	RiderRoot->SetRelativeRotation(FRotator::ZeroRotator);
	WakeLeft->SetVisibility(true);
	WakeRight->SetVisibility(true);
}

void AFF_SurfingPrimitiveCharacterActor::OnPresentationStateChanged_Implementation(
	const FFF_SurfingCharacterPresentationState& InPresentationState)
{
	if (Surfboard)
	{
		Surfboard->SetMaterial(
			0,
			InPresentationState.bBoosting ? BoostBoardMaterial : BoardMaterial);
	}
}

void AFF_SurfingPrimitiveCharacterActor::OnAdvancePresentation_Implementation(
	const float DeltaSeconds,
	const float InPresentationSeconds)
{
	const FFF_SurfingCharacterPresentationState State = GetPresentationState();
	RiderRoot->SetRelativeRotation(FMath::RInterpTo(
		RiderRoot->GetRelativeRotation(),
		FRotator(
			State.bBoosting ? -14.0f : -5.0f,
			0.0f,
			State.BankDegrees * -0.42f),
		DeltaSeconds,
		7.0f));

	const float LimbMotion = FMath::Sin(
		InPresentationSeconds * (State.bBoosting ? 8.0f : 4.0f))
		* (State.bBoosting ? 8.0f : 3.0f);
	RiderLeftArm->SetRelativeRotation(FRotator(18.0f + LimbMotion, 0.0f, -58.0f));
	RiderRightArm->SetRelativeRotation(FRotator(18.0f - LimbMotion, 0.0f, 58.0f));

	float CharacterScale = State.bBoosting ? 1.08f : 1.0f;
	CharacterScale *= FMath::Lerp(1.0f, 1.14f, GetEventPulseAlpha());
	VisualRoot->SetRelativeScale3D(FVector(CharacterScale));

	if (State.bInvincible && !State.bBoosting)
	{
		Surfboard->SetMaterial(
			0,
			FMath::Fmod(InPresentationSeconds, 0.20f) < 0.10f
				? BoostBoardMaterial
				: BoardMaterial);
	}
	else
	{
		Surfboard->SetMaterial(0, State.bBoosting ? BoostBoardMaterial : BoardMaterial);
	}

	const float WakeLength = State.bBoosting ? 2.35f : 1.45f;
	WakeLeft->SetRelativeScale3D(FVector(WakeLength, 0.065f, 0.025f));
	WakeRight->SetRelativeScale3D(FVector(WakeLength, 0.065f, 0.025f));
	WakeLeft->SetRelativeLocation(FVector(-70.0f * WakeLength, -31.0f, -16.0f));
	WakeRight->SetRelativeLocation(FVector(-70.0f * WakeLength, 31.0f, -16.0f));
}

void AFF_SurfingPrimitiveCharacterActor::OnReleaseCharacter_Implementation()
{
	VisualRoot->SetRelativeScale3D(FVector::OneVector);
	RiderRoot->SetRelativeRotation(FRotator::ZeroRotator);
	Surfboard->SetMaterial(0, BoardMaterial);
}

bool AFF_SurfingPrimitiveCharacterActor::EvaluateCharacterReady_Implementation() const
{
	return IsValid(CharacterRoot)
		&& IsValid(VisualRoot)
		&& IsValid(Surfboard)
		&& IsValid(RiderRoot)
		&& IsValid(RiderBody)
		&& IsValid(RiderHead)
		&& IsValid(RiderLeftArm)
		&& IsValid(RiderRightArm)
		&& IsValid(RiderLeftLeg)
		&& IsValid(RiderRightLeg)
		&& IsValid(WakeLeft)
		&& IsValid(WakeRight)
		&& IsValid(CubeMesh)
		&& IsValid(SphereMesh)
		&& IsValid(CylinderMesh)
		&& IsValid(BasicShapeMaterial)
		&& IsValid(BoardMaterial)
		&& IsValid(BoostBoardMaterial)
		&& IsValid(OutfitMaterial)
		&& IsValid(SkinMaterial)
		&& IsValid(WakeMaterial)
		&& EvaluateRiderMeshPartCount_Implementation() == 6;
}

int32 AFF_SurfingPrimitiveCharacterActor::EvaluateRiderMeshPartCount_Implementation() const
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
	for (const UStaticMeshComponent* RiderPart : RiderParts)
	{
		Count += IsValid(RiderPart) && IsValid(RiderPart->GetStaticMesh()) ? 1 : 0;
	}
	return Count;
}

UMaterialInstanceDynamic* AFF_SurfingPrimitiveCharacterActor::CreateColorMaterial(
	const FName MaterialBaseName,
	const FLinearColor& Color)
{
	if (!BasicShapeMaterial)
	{
		return nullptr;
	}

	const FName UniqueMaterialName = MakeUniqueObjectName(
		this,
		UMaterialInstanceDynamic::StaticClass(),
		MaterialBaseName);
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
		BasicShapeMaterial,
		this,
		UniqueMaterialName);
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Material->SetVectorParameterValue(TEXT("Tint"), Color);
	}
	return Material;
}

void AFF_SurfingPrimitiveCharacterActor::ConfigureGeometry()
{
	Surfboard->SetStaticMesh(CubeMesh);
	RiderBody->SetStaticMesh(CylinderMesh);
	RiderHead->SetStaticMesh(SphereMesh);
	RiderLeftArm->SetStaticMesh(CylinderMesh);
	RiderRightArm->SetStaticMesh(CylinderMesh);
	RiderLeftLeg->SetStaticMesh(CylinderMesh);
	RiderRightLeg->SetStaticMesh(CylinderMesh);
	WakeLeft->SetStaticMesh(CubeMesh);
	WakeRight->SetStaticMesh(CubeMesh);

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

	const float WakeLength = 1.45f;
	WakeLeft->SetRelativeScale3D(FVector(WakeLength, 0.065f, 0.025f));
	WakeRight->SetRelativeScale3D(FVector(WakeLength, 0.065f, 0.025f));
	WakeLeft->SetRelativeLocation(FVector(-70.0f * WakeLength, -31.0f, -16.0f));
	WakeRight->SetRelativeLocation(FVector(-70.0f * WakeLength, 31.0f, -16.0f));
}

void AFF_SurfingPrimitiveCharacterActor::ConfigurePrimitive(
	UStaticMeshComponent& MeshComponent,
	const bool bCastShadow)
{
	MeshComponent.SetMobility(EComponentMobility::Movable);
	MeshComponent.SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent.SetGenerateOverlapEvents(false);
	MeshComponent.SetCanEverAffectNavigation(false);
	MeshComponent.SetVisibleInSceneCaptureOnly(true);
	MeshComponent.SetLightingChannels(false, true, false);
	MeshComponent.SetCastShadow(bCastShadow);
}
