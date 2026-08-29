// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_TargetShootingArena3D.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float ArenaPlaneWidth = 3080.0f;
	constexpr float ArenaPlaneHeight = 1580.0f;
	constexpr float TargetMeshRadius = 50.0f;
	constexpr float TargetMeshDepth = 100.0f;
	constexpr float ProjectileOriginX = -810.0f;
	constexpr float TargetImpactX = 420.0f;

	const FLinearColor DeepNavy(0.003f, 0.012f, 0.026f, 1.0f);
	const FLinearColor NavyPanel(0.010f, 0.034f, 0.060f, 1.0f);
	const FLinearColor Gunmetal(0.045f, 0.075f, 0.095f, 1.0f);
	const FLinearColor Cyan(0.02f, 0.92f, 1.45f, 1.0f);
	const FLinearColor Amber(1.55f, 0.48f, 0.035f, 1.0f);
	const FLinearColor WaterBlue(0.03f, 0.50f, 0.84f, 1.0f);
	const FRotator CylinderFacesCamera(90.0f, 0.0f, 0.0f);
}

AFF_TargetShootingArena3D::AFF_TargetShootingArena3D()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ArenaMaterialFinder(
		TEXT("/Game/FestaFiesta/UI/Minigame/UIUX_v2/M_MG_TargetShooting_Arena3D."
			 "M_MG_TargetShooting_Arena3D"));
	UStaticMesh* Cube = CubeFinder.Object;
	UStaticMesh* Cylinder = CylinderFinder.Object;
	UStaticMesh* Sphere = SphereFinder.Object;
	ArenaMaterial = ArenaMaterialFinder.Object;

	// A deep gallery box gives the player real perspective, parallax cues and shadows.
	NavyEnvironment.Add(CreatePrimitive(
		TEXT("RearWall"), Cube, FVector(585.0f, 0.0f, 190.0f),
		FRotator::ZeroRotator, FVector(0.34f, 18.0f, 10.1f)));
	NavyEnvironment.Add(CreatePrimitive(
		TEXT("Floor"), Cube, FVector(-180.0f, 0.0f, -755.0f),
		FRotator::ZeroRotator, FVector(15.0f, 18.0f, 0.24f)));
	NavyEnvironment.Add(CreatePrimitive(
		TEXT("Ceiling"), Cube, FVector(-180.0f, 0.0f, 1140.0f),
		FRotator::ZeroRotator, FVector(15.0f, 18.0f, 0.20f)));
	NavyEnvironment.Add(CreatePrimitive(
		TEXT("LeftWall"), Cube, FVector(-180.0f, -1775.0f, 190.0f),
		FRotator::ZeroRotator, FVector(15.0f, 0.25f, 9.5f)));
	NavyEnvironment.Add(CreatePrimitive(
		TEXT("RightWall"), Cube, FVector(-180.0f, 1775.0f, 190.0f),
		FRotator::ZeroRotator, FVector(15.0f, 0.25f, 9.5f)));

	for (int32 PanelIndex = -3; PanelIndex <= 3; ++PanelIndex)
	{
		SteelEnvironment.Add(CreatePrimitive(
			*FString::Printf(TEXT("RearPanel_%d"), PanelIndex + 3),
			Cube,
			FVector(548.0f, PanelIndex * 420.0f, 175.0f),
			FRotator::ZeroRotator,
			FVector(0.12f, 1.88f, 8.4f)));
	}
	for (int32 BayIndex = -2; BayIndex <= 2; ++BayIndex)
	{
		SteelEnvironment.Add(CreatePrimitive(
			*FString::Printf(TEXT("BayColumn_%d"), BayIndex + 2),
			Cube,
			FVector(495.0f, BayIndex * 710.0f, 185.0f),
			FRotator::ZeroRotator,
			FVector(0.22f, 0.18f, 9.0f)));
	}

	CyanLights.Add(CreatePrimitive(
		TEXT("RearLightTop"), Cube, FVector(515.0f, 0.0f, 985.0f),
		FRotator::ZeroRotator, FVector(0.08f, 16.0f, 0.035f), false));
	CyanLights.Add(CreatePrimitive(
		TEXT("RearLightBottom"), Cube, FVector(515.0f, 0.0f, -600.0f),
		FRotator::ZeroRotator, FVector(0.08f, 16.0f, 0.035f), false));
	CyanLights.Add(CreatePrimitive(
		TEXT("FloorGuideLeft"), Cube, FVector(-200.0f, -570.0f, -719.0f),
		FRotator::ZeroRotator, FVector(14.0f, 0.035f, 0.025f), false));
	CyanLights.Add(CreatePrimitive(
		TEXT("FloorGuideRight"), Cube, FVector(-200.0f, 570.0f, -719.0f),
		FRotator::ZeroRotator, FVector(14.0f, 0.035f, 0.025f), false));
	AmberLights.Add(CreatePrimitive(
		TEXT("FloorCenterGuide"), Cube, FVector(-160.0f, 0.0f, -716.0f),
		FRotator::ZeroRotator, FVector(14.0f, 0.022f, 0.028f), false));

	TargetArm = CreatePrimitive(
		TEXT("TargetServoArm"), Cube, FVector(505.0f, 0.0f, 190.0f),
		FRotator::ZeroRotator, FVector(0.52f, 0.13f, 0.13f));
	TargetCarrier = CreatePrimitive(
		TEXT("TargetCarrier"), Cube, FVector(500.0f, 0.0f, 190.0f),
		FRotator::ZeroRotator, FVector(0.28f, 2.6f, 3.2f));
	TargetCarrierInset = CreatePrimitive(
		TEXT("TargetCarrierInset"), Cube, FVector(477.0f, 0.0f, 190.0f),
		FRotator::ZeroRotator, FVector(0.10f, 2.35f, 2.92f));
	TargetBacking = CreatePrimitive(
		TEXT("TargetBacking"), Cylinder, FVector(463.0f, 0.0f, 190.0f),
		CylinderFacesCamera, FVector(1.0f, 1.0f, 0.15f));
	TargetOuter = CreatePrimitive(
		TEXT("TargetOuter"), Cylinder, FVector(450.0f, 0.0f, 190.0f),
		CylinderFacesCamera, FVector(1.0f, 1.0f, 0.07f));
	TargetPrecision = CreatePrimitive(
		TEXT("TargetPrecision"), Cylinder, FVector(441.0f, 0.0f, 190.0f),
		CylinderFacesCamera, FVector(0.58f, 0.58f, 0.06f));
	TargetBullseye = CreatePrimitive(
		TEXT("TargetBullseye"), Cylinder, FVector(432.0f, 0.0f, 190.0f),
		CylinderFacesCamera, FVector(0.26f, 0.26f, 0.055f));
	for (int32 BoltIndex = 0; BoltIndex < 4; ++BoltIndex)
	{
		TargetBolts.Add(CreatePrimitive(
			*FString::Printf(TEXT("TargetBolt_%d"), BoltIndex),
			Sphere,
			FVector(429.0f, 0.0f, 190.0f),
			FRotator::ZeroRotator,
			FVector(0.11f)));
	}

	ProjectileGlow = CreatePrimitive(
		TEXT("ProjectileGlow"), Sphere, FVector::ZeroVector,
		FRotator::ZeroRotator, FVector(0.30f), false);
	ProjectileCore = CreatePrimitive(
		TEXT("ProjectileCore"), Sphere, FVector::ZeroVector,
		FRotator::ZeroRotator, FVector(0.17f), false);
	ProjectileTrail = CreatePrimitive(
		TEXT("ProjectileTrail"), Cylinder, FVector::ZeroVector,
		FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.8f), false);

	// Stylized but genuinely three-dimensional first-person water blaster.
	WeaponBody.Add(CreatePrimitive(
		TEXT("WeaponReceiver"), Cube, FVector(-800.0f, 105.0f, -120.0f),
		FRotator(0.0f, -4.0f, 0.0f), FVector(1.35f, 0.62f, 0.52f)));
	WeaponBody.Add(CreatePrimitive(
		TEXT("WeaponTank"), Sphere, FVector(-842.0f, 95.0f, -63.0f),
		FRotator::ZeroRotator, FVector(0.72f, 0.60f, 0.72f)));
	WeaponBody.Add(CreatePrimitive(
		TEXT("WeaponBarrel"), Cylinder, FVector(-674.0f, 88.0f, -82.0f),
		CylinderFacesCamera, FVector(0.40f, 0.40f, 1.55f)));
	WeaponBody.Add(CreatePrimitive(
		TEXT("WeaponNozzle"), Cylinder, FVector(-551.0f, 78.0f, -70.0f),
		CylinderFacesCamera, FVector(0.54f, 0.54f, 0.36f)));
	WeaponBody.Add(CreatePrimitive(
		TEXT("WeaponGrip"), Cube, FVector(-826.0f, 105.0f, -220.0f),
		FRotator(0.0f, 0.0f, -16.0f), FVector(0.40f, 0.48f, 0.95f)));
	MuzzleFlash = CreatePrimitive(
		TEXT("MuzzleFlash3D"), Sphere, FVector(-515.0f, 76.0f, -65.0f),
		FRotator::ZeroRotator, FVector(0.35f), false);

	SetTargetPartsVisible(false);
	ProjectileCore->SetVisibility(false, true);
	ProjectileGlow->SetVisibility(false, true);
	ProjectileTrail->SetVisibility(false, true);
	MuzzleFlash->SetVisibility(false, true);
}

FVector AFF_TargetShootingArena3D::NormalizedToTargetPlane(
	const FVector2D& NormalizedPoint,
	const float DepthX)
{
	return FVector(
		DepthX,
		(FMath::Clamp(NormalizedPoint.X, 0.0, 1.0) - 0.5) * ArenaPlaneWidth,
		190.0 + (0.5 - FMath::Clamp(NormalizedPoint.Y, 0.0, 1.0)) * ArenaPlaneHeight);
}

void AFF_TargetShootingArena3D::SetEnvironmentVisible(const bool bVisible)
{
	for (const TArray<TObjectPtr<UStaticMeshComponent>>* Group : {
		&NavyEnvironment, &SteelEnvironment, &CyanLights, &AmberLights })
	{
		for (UStaticMeshComponent* Mesh : *Group)
		{
			if (Mesh)
			{
				Mesh->SetVisibility(bVisible, true);
			}
		}
	}
}

UStaticMeshComponent* AFF_TargetShootingArena3D::CreatePrimitive(
	const FName Name,
	UStaticMesh* Mesh,
	const FVector& Location,
	const FRotator& Rotation,
	const FVector& Scale,
	const bool bCastShadow)
{
	UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Component->SetupAttachment(SceneRoot);
	Component->SetStaticMesh(Mesh);
	if (ArenaMaterial)
	{
		Component->SetMaterial(0, ArenaMaterial);
	}
	Component->SetRelativeLocation(Location);
	Component->SetRelativeRotation(Rotation);
	Component->SetRelativeScale3D(Scale);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(bCastShadow);
	Component->bReceivesDecals = false;
	return Component;
}

UMaterialInstanceDynamic* AFF_TargetShootingArena3D::ApplyColor(
	UStaticMeshComponent* Mesh,
	const FLinearColor& Color)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!Material)
	{
		Material = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetScalarParameterValue(TEXT("Roughness"), 0.34f);
		Material->SetScalarParameterValue(TEXT("Metallic"), 0.18f);
		Material->SetScalarParameterValue(
			TEXT("EmissiveStrength"),
			Color.GetMax() > 1.0f ? FMath::Clamp(Color.GetMax() - 0.72f, 0.0f, 1.35f) : 0.0f);
	}
	return Material;
}

void AFF_TargetShootingArena3D::SetTargetPartTransform(
	UStaticMeshComponent* Mesh,
	const FVector& Center,
	const float HorizontalRadius,
	const float VerticalRadius,
	const float RadiusRatio,
	const float DepthX)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->SetRelativeLocation(FVector(DepthX, Center.Y, Center.Z));
	Mesh->SetRelativeRotation(CylinderFacesCamera);
	Mesh->SetRelativeScale3D(FVector(
		FMath::Max(1.0f, VerticalRadius * RadiusRatio) / TargetMeshRadius,
		FMath::Max(1.0f, HorizontalRadius * RadiusRatio) / TargetMeshRadius,
		8.0f / TargetMeshDepth));
}

void AFF_TargetShootingArena3D::SetTargetPartsVisible(const bool bVisible)
{
	for (UStaticMeshComponent* Mesh : {
		TargetArm.Get(), TargetCarrier.Get(), TargetCarrierInset.Get(),
		TargetBacking.Get(), TargetOuter.Get(),
		TargetPrecision.Get(), TargetBullseye.Get() })
	{
		if (Mesh)
		{
			Mesh->SetVisibility(bVisible, true);
		}
	}
	for (UStaticMeshComponent* Bolt : TargetBolts)
	{
		if (Bolt)
		{
			Bolt->SetVisibility(bVisible, true);
		}
	}
}

void AFF_TargetShootingArena3D::SetTargetPresentation_Implementation(
	const bool bVisible,
	const FVector2D& CenterNormalized,
	const FVector2D& RadiusNormalized,
	const float PrecisionRatio,
	const float BullseyeRatio,
	const FLinearColor& OuterColor,
	const FLinearColor& PrecisionColor,
	const FLinearColor& BullseyeColor,
	const float HighlightStrength)
{
	if (!bMaterialsInitialized)
	{
		for (UStaticMeshComponent* Mesh : NavyEnvironment)
		{
			if (UMaterialInstanceDynamic* Material = ApplyColor(Mesh, DeepNavy))
			{
				Material->SetScalarParameterValue(TEXT("Roughness"), 0.88f);
				Material->SetScalarParameterValue(TEXT("Metallic"), 0.02f);
			}
		}
		for (UStaticMeshComponent* Mesh : SteelEnvironment)
		{
			if (UMaterialInstanceDynamic* Material = ApplyColor(Mesh, NavyPanel))
			{
				Material->SetScalarParameterValue(TEXT("Roughness"), 0.72f);
				Material->SetScalarParameterValue(TEXT("Metallic"), 0.10f);
			}
		}
		for (UStaticMeshComponent* Mesh : CyanLights)
		{
			ApplyColor(Mesh, Cyan);
		}
		for (UStaticMeshComponent* Mesh : AmberLights)
		{
			ApplyColor(Mesh, Amber);
		}
		ApplyColor(TargetArm, Gunmetal);
		ApplyColor(TargetCarrier, Gunmetal);
		ApplyColor(TargetCarrierInset, DeepNavy);
		ApplyColor(TargetBacking, FLinearColor(0.006f, 0.009f, 0.014f, 1.0f));
		for (UStaticMeshComponent* Bolt : TargetBolts)
		{
			ApplyColor(Bolt, Cyan);
		}
		for (int32 Index = 0; Index < WeaponBody.Num(); ++Index)
		{
			ApplyColor(WeaponBody[Index], Index == 1 ? WaterBlue : Gunmetal);
		}
		ApplyColor(MuzzleFlash, Cyan);
		bMaterialsInitialized = true;
	}

	SetTargetPartsVisible(bVisible);
	if (!bVisible)
	{
		return;
	}

	const FVector Center = NormalizedToTargetPlane(CenterNormalized);
	const float HorizontalRadius = FMath::Max(8.0, RadiusNormalized.X * ArenaPlaneWidth);
	const float VerticalRadius = FMath::Max(8.0, RadiusNormalized.Y * ArenaPlaneHeight);
	const float SafePrecisionRatio = FMath::Clamp(PrecisionRatio, 0.12f, 0.88f);
	const float SafeBullseyeRatio = FMath::Clamp(BullseyeRatio, 0.06f, SafePrecisionRatio);

	TargetArm->SetRelativeLocation(FVector(520.0f, Center.Y, Center.Z));
	TargetArm->SetRelativeScale3D(FVector(
		0.58f,
		FMath::Max(0.16f, HorizontalRadius / 620.0f),
		FMath::Max(0.16f, VerticalRadius / 620.0f)));
	TargetCarrier->SetRelativeLocation(FVector(500.0f, Center.Y, Center.Z));
	TargetCarrier->SetRelativeScale3D(FVector(
		0.28f,
		HorizontalRadius * 1.44f / TargetMeshRadius,
		VerticalRadius * 1.34f / TargetMeshRadius));
	TargetCarrierInset->SetRelativeLocation(FVector(477.0f, Center.Y, Center.Z));
	TargetCarrierInset->SetRelativeScale3D(FVector(
		0.10f,
		HorizontalRadius * 1.27f / TargetMeshRadius,
		VerticalRadius * 1.18f / TargetMeshRadius));
	SetTargetPartTransform(TargetBacking, Center, HorizontalRadius, VerticalRadius, 1.18f, 463.0f);
	SetTargetPartTransform(TargetOuter, Center, HorizontalRadius, VerticalRadius, 1.0f, 450.0f);
	SetTargetPartTransform(TargetPrecision, Center, HorizontalRadius, VerticalRadius, SafePrecisionRatio, 441.0f);
	SetTargetPartTransform(TargetBullseye, Center, HorizontalRadius, VerticalRadius, SafeBullseyeRatio, 432.0f);

	const float Highlight = FMath::Clamp(HighlightStrength, 0.0f, 0.75f);
	ApplyColor(TargetOuter, OuterColor * (0.64f + Highlight * 0.42f));
	ApplyColor(TargetPrecision, PrecisionColor * (0.48f + Highlight * 0.32f));
	ApplyColor(TargetBullseye, BullseyeColor * (0.72f + Highlight * 0.30f));
	const FVector2D BoltSigns[] = {
		FVector2D(-1.0, -1.0), FVector2D(1.0, -1.0),
		FVector2D(-1.0, 1.0), FVector2D(1.0, 1.0)
	};
	for (int32 Index = 0; Index < TargetBolts.Num() && Index < UE_ARRAY_COUNT(BoltSigns); ++Index)
	{
		TargetBolts[Index]->SetRelativeLocation(FVector(
			426.0f,
			Center.Y + BoltSigns[Index].X * HorizontalRadius * 0.77f,
			Center.Z + BoltSigns[Index].Y * VerticalRadius * 0.77f));
		const float BoltScale = FMath::Clamp(
			FMath::Min(HorizontalRadius, VerticalRadius) / 470.0f,
			0.08f,
			0.22f);
		TargetBolts[Index]->SetRelativeScale3D(FVector(BoltScale));
	}
}

void AFF_TargetShootingArena3D::SetProjectilePresentation_Implementation(
	const bool bVisible,
	const FVector2D& PositionNormalized,
	const FVector2D& AimNormalized,
	const float TravelAlpha,
	const FLinearColor& CoreColor,
	const FLinearColor& GlowColor)
{
	for (UStaticMeshComponent* Mesh : {
		ProjectileCore.Get(), ProjectileGlow.Get(), ProjectileTrail.Get() })
	{
		if (Mesh)
		{
			Mesh->SetVisibility(bVisible, true);
		}
	}
	if (!bVisible)
	{
		return;
	}

	const float SafeTravelAlpha = FMath::Clamp(TravelAlpha, 0.0f, 1.0f);
	FVector ProjectileLocation = NormalizedToTargetPlane(
		PositionNormalized,
		FMath::Lerp(ProjectileOriginX, TargetImpactX, SafeTravelAlpha));
	const FVector AimLocation = NormalizedToTargetPlane(AimNormalized, TargetImpactX);
	const FVector Direction = (AimLocation - ProjectileLocation).GetSafeNormal();
	const float Pulse = 1.0f + 0.14f * FMath::Sin(SafeTravelAlpha * 8.0f * UE_PI);

	ProjectileCore->SetRelativeLocation(ProjectileLocation);
	ProjectileCore->SetRelativeScale3D(FVector(0.18f * Pulse));
	ProjectileGlow->SetRelativeLocation(ProjectileLocation);
	ProjectileGlow->SetRelativeScale3D(FVector(0.34f * Pulse));
	ApplyColor(ProjectileCore, CoreColor * 1.45f);
	FLinearColor SoftGlow = GlowColor * 1.65f;
	SoftGlow.A = 0.55f;
	ApplyColor(ProjectileGlow, SoftGlow);

	const float TrailLength = FMath::Lerp(55.0f, 145.0f, SafeTravelAlpha);
	ProjectileTrail->SetRelativeLocation(ProjectileLocation - Direction * TrailLength * 0.5f);
	ProjectileTrail->SetRelativeRotation(
		FQuat::FindBetweenNormals(FVector::UpVector, Direction).Rotator());
	ProjectileTrail->SetRelativeScale3D(FVector(0.065f, 0.065f, TrailLength / 100.0f));
	ApplyColor(ProjectileTrail, GlowColor * 1.25f);
}

void AFF_TargetShootingArena3D::SetWeaponPresentation_Implementation(
	const bool bVisible,
	const float RecoilAlpha,
	const float MuzzleFlashAlpha)
{
	const FVector RecoilOffset(
		-FMath::Clamp(RecoilAlpha, 0.0f, 1.0f) * 42.0f,
		0.0f,
		-FMath::Clamp(RecoilAlpha, 0.0f, 1.0f) * 12.0f);
	for (UStaticMeshComponent* Mesh : WeaponBody)
	{
		if (Mesh)
		{
			Mesh->SetVisibility(bVisible, true);
			Mesh->SetRelativeLocation(Mesh->GetRelativeLocation() - WeaponRestOffset + RecoilOffset);
		}
	}
	WeaponRestOffset = RecoilOffset;
	if (MuzzleFlash)
	{
		const float FlashAlpha = FMath::Clamp(MuzzleFlashAlpha, 0.0f, 1.0f);
		MuzzleFlash->SetVisibility(bVisible && FlashAlpha > 0.0f, true);
		MuzzleFlash->SetRelativeLocation(
			FVector(-515.0f, 76.0f, -65.0f) + RecoilOffset);
		MuzzleFlash->SetRelativeScale3D(FVector(FMath::Lerp(0.18f, 0.62f, FlashAlpha)));
	}
}
