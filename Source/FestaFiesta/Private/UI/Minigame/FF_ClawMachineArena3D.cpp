// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_ClawMachineArena3D.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor Teal(0.035f, 0.68f, 0.72f, 1.0f);
	const FLinearColor DeepTeal(0.015f, 0.25f, 0.30f, 1.0f);
	const FLinearColor Coral(1.0f, 0.28f, 0.24f, 1.0f);
	const FLinearColor Cream(0.96f, 0.91f, 0.76f, 1.0f);
	const FLinearColor Gold(1.35f, 0.72f, 0.10f, 1.0f);
	const FLinearColor DarkMetal(0.055f, 0.075f, 0.085f, 1.0f);
	const FLinearColor BrushedSteel(0.36f, 0.58f, 0.62f, 1.0f);
	constexpr float ClawTopZ = 570.0f;
	constexpr float ClawBottomZ = -52.0f;
	constexpr float GravityCmPerSecondSquared = 980.0f;
	constexpr float MechanicalSubstepSeconds = 1.0f / 120.0f;
	constexpr int32 MaxMechanicalSubsteps = 8;

	void StepDampedSpring(
		FVector& Position,
		FVector& Velocity,
		const FVector& Target,
		const float FrequencyHz,
		const float DampingRatio,
		const float DeltaSeconds)
	{
		const float Omega = 2.0f * UE_PI * FMath::Max(0.01f, FrequencyHz);
		const FVector Acceleration = (Target - Position) * (Omega * Omega)
			- Velocity * (2.0f * FMath::Max(0.0f, DampingRatio) * Omega);
		Velocity += Acceleration * DeltaSeconds;
		Position += Velocity * DeltaSeconds;
	}

	void StepDampedSpring(
		float& Position,
		float& Velocity,
		const float Target,
		const float FrequencyHz,
		const float DampingRatio,
		const float DeltaSeconds)
	{
		const float Omega = 2.0f * UE_PI * FMath::Max(0.01f, FrequencyHz);
		const float Acceleration = (Target - Position) * (Omega * Omega)
			- Velocity * (2.0f * FMath::Max(0.0f, DampingRatio) * Omega);
		Velocity += Acceleration * DeltaSeconds;
		Position += Velocity * DeltaSeconds;
	}
}

AFF_ClawMachineArena3D::AFF_ClawMachineArena3D()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/FestaFiesta/UI/Minigame/UIUX_v2/M_MG_TargetShooting_Arena3D."
			"M_MG_TargetShooting_Arena3D"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BackdropMaterialFinder(
		TEXT("/Game/FestaFiesta/UI/Minigame/ClawMachine/M_MG_ClawMachine_Backdrop."
			"M_MG_ClawMachine_Backdrop"));
	CubeMesh = CubeFinder.Object;
	SphereMesh = SphereFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	ConeMesh = ConeFinder.Object;
	ArenaMaterial = MaterialFinder.Object;
	BackdropMaterial = BackdropMaterialFinder.Object;

	// Product-scale cabinet. Engine basic cubes are 100 cm, so every scale below is an
	// explicit finished dimension / 100 rather than a half-extent. This keeps the shell,
	// chamber and controls mechanically connected from every camera angle.
	CabinetTealParts.Add(CreatePrimitive(TEXT("Base"), CubeMesh,
		FVector(0.0f, 0.0f, -355.0f), FRotator::ZeroRotator, FVector(7.0f, 7.0f, 2.7f)));
	CabinetTealParts.Add(CreatePrimitive(TEXT("Plinth"), CubeMesh,
		FVector(0.0f, 0.0f, -505.0f), FRotator::ZeroRotator, FVector(7.7f, 7.7f, 0.30f)));
	CabinetTealParts.Add(CreatePrimitive(TEXT("TopCanopy"), CubeMesh,
		FVector(0.0f, 0.0f, 690.0f), FRotator::ZeroRotator, FVector(7.2f, 7.2f, 0.85f)));
	CabinetCreamParts.Add(CreatePrimitive(TEXT("TopCap"), CubeMesh,
		FVector(0.0f, 0.0f, 741.0f), FRotator::ZeroRotator, FVector(7.45f, 7.45f, 0.18f), false));
	CabinetCoralParts.Add(CreatePrimitive(TEXT("FrontApron"), CubeMesh,
		FVector(-357.0f, 0.0f, -350.0f), FRotator::ZeroRotator, FVector(0.30f, 6.25f, 1.55f)));
	CabinetCoralParts.Add(CreatePrimitive(TEXT("ControlDeck"), CubeMesh,
		FVector(-392.0f, 0.0f, -170.0f), FRotator::ZeroRotator, FVector(1.05f, 7.10f, 0.48f)));
	CabinetCreamParts.Add(CreatePrimitive(TEXT("PrizeFloor"), CubeMesh,
		FVector(10.0f, 0.0f, -202.0f), FRotator::ZeroRotator, FVector(6.15f, 6.15f, 0.18f)));
	CabinetTealParts.Add(CreatePrimitive(TEXT("StudioFloor"), CubeMesh,
		FVector(0.0f, 0.0f, -526.0f), FRotator::ZeroRotator, FVector(11.0f, 11.0f, 0.08f), false));

	for (const float FrontBack : { -1.0f, 1.0f })
	{
		for (const float Side : { -1.0f, 1.0f })
		{
			CabinetTealParts.Add(CreatePrimitive(
				*FString::Printf(TEXT("ChamberPost_%d_%d"),
					FrontBack < 0.0f ? 0 : 1, Side < 0.0f ? 0 : 1),
				CubeMesh, FVector(FrontBack * 315.0f, Side * 315.0f, 225.0f),
				FRotator::ZeroRotator, FVector(0.46f, 0.46f, 8.10f)));
		}
	}
	for (const float Side : { -1.0f, 1.0f })
	{
		CabinetTealParts.Add(CreatePrimitive(
			*FString::Printf(TEXT("SideTopRail_%d"), Side < 0.0f ? 0 : 1),
			CubeMesh, FVector(0.0f, Side * 315.0f, 623.0f), FRotator::ZeroRotator,
			FVector(6.30f, 0.46f, 0.46f)));
	}
	CabinetTealParts.Add(CreatePrimitive(TEXT("FrontTopRail"), CubeMesh,
		FVector(-315.0f, 0.0f, 623.0f), FRotator::ZeroRotator, FVector(0.46f, 6.30f, 0.46f)));

	BackdropPanel = CreatePrimitive(TEXT("InteriorBackdrop"), CubeMesh,
		FVector(329.0f, 0.0f, 220.0f), FRotator::ZeroRotator, FVector(0.16f, 6.05f, 7.25f), false);
	if (BackdropPanel && BackdropMaterial)
	{
		BackdropPanel->SetMaterial(0, BackdropMaterial);
	}

	// Marquee, prize chute and controls make the machine readable even if all optional art is absent.
	CabinetCoralParts.Add(CreatePrimitive(TEXT("Marquee"), CubeMesh,
		FVector(-369.0f, 0.0f, 690.0f), FRotator::ZeroRotator, FVector(0.88f, 6.05f, 0.82f), false));
	CabinetGoldParts.Add(CreatePrimitive(TEXT("MarqueeLight"), CubeMesh,
		FVector(-416.0f, 0.0f, 691.0f), FRotator::ZeroRotator, FVector(0.08f, 5.45f, 0.18f), false));
	CabinetTealParts.Add(CreatePrimitive(TEXT("PrizeChuteOpening"), CubeMesh,
		FVector(-382.0f, -185.0f, -365.0f), FRotator::ZeroRotator, FVector(0.12f, 2.15f, 1.05f), false));
	CabinetCreamParts.Add(CreatePrimitive(TEXT("PrizeChuteTop"), CubeMesh,
		FVector(-397.0f, -185.0f, -303.0f), FRotator::ZeroRotator, FVector(0.12f, 2.45f, 0.12f), false));
	CabinetCreamParts.Add(CreatePrimitive(TEXT("PrizeChuteBottom"), CubeMesh,
		FVector(-397.0f, -185.0f, -427.0f), FRotator::ZeroRotator, FVector(0.12f, 2.45f, 0.12f), false));
	CabinetCreamParts.Add(CreatePrimitive(TEXT("PrizeChuteLeft"), CubeMesh,
		FVector(-397.0f, -424.0f, -365.0f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 1.35f), false));
	CabinetCreamParts.Add(CreatePrimitive(TEXT("PrizeChuteRight"), CubeMesh,
		FVector(-397.0f, 54.0f, -365.0f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 1.35f), false));
	CabinetGoldParts.Add(CreatePrimitive(TEXT("CoinSlot"), CubeMesh,
		FVector(-397.0f, 225.0f, -350.0f), FRotator::ZeroRotator, FVector(0.12f, 0.15f, 0.42f), false));
	CabinetGoldParts.Add(CreatePrimitive(TEXT("ActionButton"), CylinderMesh,
		FVector(-405.0f, 225.0f, -134.0f), FRotator::ZeroRotator, FVector(0.34f, 0.34f, 0.18f), false));
	JoystickStem = CreatePrimitive(TEXT("Joystick"), CylinderMesh,
		FVector(-405.0f, -175.0f, -112.0f), FRotator::ZeroRotator, FVector(0.14f, 0.14f, 0.56f), false);
	JoystickBall = CreatePrimitive(TEXT("JoystickBall"), SphereMesh,
		FVector(-405.0f, -175.0f, -66.0f), FRotator::ZeroRotator, FVector(0.28f), false);
	CabinetGoldParts.Add(JoystickStem);
	CabinetCoralParts.Add(JoystickBall);

	Carriage = CreatePrimitive(TEXT("ClawCarriage"), CubeMesh,
		FVector(10.0f, 0.0f, ClawTopZ), FRotator::ZeroRotator, FVector(0.86f, 0.76f, 0.22f));
	Cable = CreatePrimitive(TEXT("ClawCable"), CylinderMesh,
		FVector(10.0f, 0.0f, 430.0f), FRotator::ZeroRotator, FVector(0.070f, 0.070f, 1.4f), false);
	ClawHub = CreatePrimitive(TEXT("ClawHub"), CylinderMesh,
		FVector(10.0f, 0.0f, 300.0f), FRotator::ZeroRotator, FVector(0.40f, 0.40f, 0.34f));
	for (int32 TineIndex = 0; TineIndex < 3; ++TineIndex)
	{
		ClawTines.Add(CreatePrimitive(
			*FString::Printf(TEXT("ClawTine_%d"), TineIndex),
			CylinderMesh, FVector(10.0f, 0.0f, 250.0f), FRotator::ZeroRotator,
			FVector(0.09f, 0.09f, 0.72f)));
		ClawTips.Add(CreatePrimitive(
			*FString::Printf(TEXT("ClawTip_%d"), TineIndex),
			ConeMesh, FVector(10.0f, 0.0f, 205.0f), FRotator::ZeroRotator,
			FVector(0.17f, 0.17f, 0.24f)));
	}
}

FVector AFF_ClawMachineArena3D::NormalizedToPrizeFloor(const FVector2D& PositionNormalized)
{
	return FVector(
		FMath::Lerp(255.0, -245.0, FMath::Clamp(PositionNormalized.Y, 0.0, 1.0)),
		FMath::Lerp(-270.0, 270.0, FMath::Clamp(PositionNormalized.X, 0.0, 1.0)),
		-150.0f);
}

UStaticMeshComponent* AFF_ClawMachineArena3D::CreatePrimitive(
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

UStaticMeshComponent* AFF_ClawMachineArena3D::CreateRuntimePrimitive(
	const FName Name,
	UStaticMesh* Mesh,
	const FVector& Scale)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, Name);
	if (!Component)
	{
		return nullptr;
	}
	AddInstanceComponent(Component);
	Component->SetupAttachment(SceneRoot);
	Component->SetStaticMesh(Mesh);
	if (ArenaMaterial)
	{
		Component->SetMaterial(0, ArenaMaterial);
	}
	Component->SetRelativeScale3D(Scale);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(true);
	Component->bReceivesDecals = false;
	Component->RegisterComponent();
	return Component;
}

UMaterialInstanceDynamic* AFF_ClawMachineArena3D::ApplyColor(
	UStaticMeshComponent* Mesh,
	const FLinearColor& Color,
	const float Roughness,
	const float Metallic)
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
		Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		// UViewport has only a compact preview-light rig. A restrained material fill
		// prevents unlit cabinet faces from crushing to pure black while preserving
		// directional shading and the authored palette.
		const float MaterialFill = Metallic >= 0.60f ? 0.10f : 0.16f;
		Material->SetScalarParameterValue(
			TEXT("EmissiveStrength"),
			Color.GetMax() > 1.0f
				? FMath::Clamp(Color.GetMax() - 0.45f, MaterialFill, 1.5f)
				: MaterialFill);
	}
	return Material;
}

void AFF_ClawMachineArena3D::ConfigureMachine_Implementation(
	const UFF_ClawMachineDefinition* Definition,
	const FFF_ClawMachineSnapshot& InitialSnapshot)
{
	ConfiguredDefinition = Definition;
	bPhysicalStateInitialized = false;
	bHasTargetSnapshot = false;
	GantryVelocity = FVector::ZeroVector;
	HubVerticalVelocity = 0.0f;
	CableSwingRadians = FVector2D::ZeroVector;
	CableSwingVelocity = FVector2D::ZeroVector;
	HeldPrizeVelocity = FVector::ZeroVector;
	ResolvedRules = Definition ? Definition->Rules : FFF_ClawMachineRules();
	if (Definition)
	{
		FString FailureReason;
		if (!Definition->BuildResolvedPrizePool(ResolvedRules.PrizePool, &FailureReason))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ClawMachine arena is using the inline fallback catalog: %s"),
				*FailureReason);
			ResolvedRules.PrizePool = Definition->Rules.PrizePool;
		}
	}
	if (!bBaseMaterialsInitialized)
	{
		// Blueprint children may replace the two art materials without rebuilding code.
		for (UStaticMeshComponent* Part : CabinetTealParts) { if (Part && ArenaMaterial) { Part->SetMaterial(0, ArenaMaterial); } }
		for (UStaticMeshComponent* Part : CabinetCoralParts) { if (Part && ArenaMaterial) { Part->SetMaterial(0, ArenaMaterial); } }
		for (UStaticMeshComponent* Part : CabinetCreamParts) { if (Part && ArenaMaterial) { Part->SetMaterial(0, ArenaMaterial); } }
		for (UStaticMeshComponent* Part : CabinetGoldParts) { if (Part && ArenaMaterial) { Part->SetMaterial(0, ArenaMaterial); } }
		for (UStaticMeshComponent* Part : { Carriage.Get(), Cable.Get(), ClawHub.Get() })
		{
			if (Part && ArenaMaterial) { Part->SetMaterial(0, ArenaMaterial); }
		}
		for (UStaticMeshComponent* Tine : ClawTines) { if (Tine && ArenaMaterial) { Tine->SetMaterial(0, ArenaMaterial); } }
		for (UStaticMeshComponent* Tip : ClawTips) { if (Tip && ArenaMaterial) { Tip->SetMaterial(0, ArenaMaterial); } }
		if (BackdropPanel)
		{
			BackdropPanel->SetMaterial(0, BackdropMaterial ? BackdropMaterial.Get() : ArenaMaterial.Get());
		}
		for (UStaticMeshComponent* Part : CabinetTealParts) { ApplyColor(Part, Teal, 0.42f, 0.08f); }
		for (UStaticMeshComponent* Part : CabinetCoralParts) { ApplyColor(Part, Coral, 0.40f, 0.04f); }
		for (UStaticMeshComponent* Part : CabinetCreamParts) { ApplyColor(Part, Cream, 0.72f, 0.01f); }
		for (UStaticMeshComponent* Part : CabinetGoldParts) { ApplyColor(Part, Gold, 0.28f, 0.22f); }
		if (BackdropPanel && !BackdropMaterial) { ApplyColor(BackdropPanel, Cream, 0.82f, 0.0f); }
		ApplyColor(Carriage, DeepTeal, 0.34f, 0.25f);
		ApplyColor(Cable, DarkMetal, 0.24f, 0.68f);
		ApplyColor(ClawHub, Gold, 0.24f, 0.62f);
		for (UStaticMeshComponent* Tine : ClawTines) { ApplyColor(Tine, BrushedSteel, 0.22f, 0.72f); }
		for (UStaticMeshComponent* Tip : ClawTips) { ApplyColor(Tip, Gold, 0.22f, 0.68f); }
		bBaseMaterialsInitialized = true;
	}
	BuildPrizeComponents(Definition, InitialSnapshot);
	ApplyMachineSnapshot_Implementation(InitialSnapshot);
}

void AFF_ClawMachineArena3D::ApplyMachineSnapshot_Implementation(
	const FFF_ClawMachineSnapshot& Snapshot)
{
	UpdateClaw(Snapshot);
	UpdatePrizes(Snapshot);
}

void AFF_ClawMachineArena3D::SetOperatorInput(FVector2D InputAxis)
{
	TargetOperatorInput = InputAxis.GetClampedToMaxSize(1.0f);
}

void AFF_ClawMachineArena3D::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateOperatorControls(DeltaSeconds);
	if (!bHasTargetSnapshot)
	{
		return;
	}
	SimulateMechanicalPresentation(DeltaSeconds);
	ApplySimulatedClaw(TargetSnapshot);
	UpdatePrizes(TargetSnapshot);
}

void AFF_ClawMachineArena3D::UpdateOperatorControls(const float DeltaSeconds)
{
	if (!JoystickStem || !JoystickBall)
	{
		return;
	}
	const float SafeDeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);
	SmoothedOperatorInput = FMath::Vector2DInterpTo(
		SmoothedOperatorInput,
		TargetOperatorInput,
		SafeDeltaSeconds,
		15.0f);

	// The stem is solved from a pivot and direction instead of rotating in place,
	// keeping the ball mechanically connected for every diagonal input.
	const FVector Pivot(-405.0f, -175.0f, -145.0f);
	const FVector Direction = FVector(
		SmoothedOperatorInput.Y * 0.24f,
		SmoothedOperatorInput.X * 0.24f,
		1.0f).GetSafeNormal();
	JoystickStem->SetRelativeLocation(Pivot + Direction * 28.0f);
	JoystickStem->SetRelativeRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
	JoystickBall->SetRelativeLocation(Pivot + Direction * 79.0f);
}

void AFF_ClawMachineArena3D::BuildPrizeComponents(
	const UFF_ClawMachineDefinition* Definition,
	const FFF_ClawMachineSnapshot& Snapshot)
{
	if (!Definition || PrizeBodies.Num() == Snapshot.Prizes.Num())
	{
		return;
	}

	for (UStaticMeshComponent* Body : PrizeBodies) { if (Body) { Body->DestroyComponent(); } }
	for (UStaticMeshComponent* Accent : PrizeAccents) { if (Accent) { Accent->DestroyComponent(); } }
	PrizeBodies.Reset();
	PrizeAccents.Reset();

	for (const FFF_ClawMachinePrizeSnapshot& Prize : Snapshot.Prizes)
	{
		const FFF_ClawMachinePrizeTuning* Tuning = ResolvedRules.FindPrize(Prize.PrizeId);
		UStaticMesh* AuthoredMesh = Tuning ? Tuning->PrizeMesh.LoadSynchronous() : nullptr;
		UStaticMesh* BodyMesh = AuthoredMesh ? AuthoredMesh : SphereMesh.Get();
		const FVector BodyScale = AuthoredMesh
			? (Tuning ? Tuning->VisualScale : FVector(1.0f))
			: (Tuning ? Tuning->VisualScale * 0.66f : FVector(0.66f));
		UStaticMeshComponent* Body = CreateRuntimePrimitive(
			*FString::Printf(TEXT("PrizeBody_%d"), Prize.PrizeSerial),
			BodyMesh,
			BodyScale);
		UStaticMeshComponent* Accent = CreateRuntimePrimitive(
			*FString::Printf(TEXT("PrizeAccent_%d"), Prize.PrizeSerial),
			AuthoredMesh ? nullptr : SphereMesh,
			FVector(0.31f));
		if (Body && Tuning)
		{
			if (UMaterialInterface* AuthoredMaterial = Tuning->PrizeMaterial.LoadSynchronous())
			{
				Body->SetMaterial(0, AuthoredMaterial);
			}
			else
			{
				ApplyColor(Body, Tuning->FallbackColor, 0.88f, 0.0f);
			}
		}
		if (Accent)
		{
			Accent->SetVisibility(AuthoredMesh == nullptr, true);
			ApplyColor(Accent, Tuning ? Tuning->FallbackColor * 0.88f : Teal, 0.92f, 0.0f);
		}
		PrizeBodies.Add(Body);
		PrizeAccents.Add(Accent);
	}
}

void AFF_ClawMachineArena3D::UpdateClaw(const FFF_ClawMachineSnapshot& Snapshot)
{
	TargetSnapshot = Snapshot;
	bHasTargetSnapshot = true;
	const FVector FloorPoint = NormalizedToPrizeFloor(Snapshot.ClawPositionNormalized);
	const FVector TargetGantry(FloorPoint.X, FloorPoint.Y, ClawTopZ);
	const float TargetHubZ = CalculateTargetHubZ(Snapshot);
	if (!bPhysicalStateInitialized)
	{
		SimulatedGantryLocation = TargetGantry;
		GantryVelocity = FVector::ZeroVector;
		SimulatedHubZ = TargetHubZ;
		HubVerticalVelocity = 0.0f;
		CableSwingRadians = FVector2D::ZeroVector;
		CableSwingVelocity = FVector2D::ZeroVector;
		SimulatedHubLocation = FVector(FloorPoint.X, FloorPoint.Y, TargetHubZ);
		SimulatedHeldPrizeLocation = SimulatedHubLocation + FVector(0.0f, 0.0f, -118.0f);
		HeldPrizeVelocity = FVector::ZeroVector;
		bPhysicalStateInitialized = true;
		ApplySimulatedClaw(Snapshot);
	}
}

float AFF_ClawMachineArena3D::CalculateTargetHubZ(const FFF_ClawMachineSnapshot& Snapshot)
{
	float VerticalAlpha = 0.0f;
	switch (Snapshot.Phase)
	{
	case EFF_ClawMachinePhase::Dropping: VerticalAlpha = Snapshot.PhaseAlpha; break;
	case EFF_ClawMachinePhase::Closing: VerticalAlpha = 1.0f; break;
	case EFF_ClawMachinePhase::Lifting: VerticalAlpha = 1.0f - Snapshot.PhaseAlpha; break;
	default: break;
	}
	return FMath::Lerp(ClawTopZ - 145.0f, ClawBottomZ, VerticalAlpha);
}

void AFF_ClawMachineArena3D::SimulateMechanicalPresentation(const float DeltaSeconds)
{
	if (!bPhysicalStateInitialized || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return;
	}
	const float SafeDeltaSeconds = FMath::Min(DeltaSeconds, 0.05f);
	const int32 StepCount = FMath::Clamp(
		FMath::CeilToInt(SafeDeltaSeconds / MechanicalSubstepSeconds),
		1,
		MaxMechanicalSubsteps);
	const float StepSeconds = SafeDeltaSeconds / static_cast<float>(StepCount);
	const FVector TargetFloor = NormalizedToPrizeFloor(TargetSnapshot.ClawPositionNormalized);
	const FVector TargetGantry(TargetFloor.X, TargetFloor.Y, ClawTopZ);
	const float TargetHubZ = CalculateTargetHubZ(TargetSnapshot);
	const float PendulumLength = FMath::Max(80.0f, ResolvedRules.PendulumLengthCm);
	const float PendulumOmegaSquared = GravityCmPerSecondSquared / PendulumLength;
	const float MaxSwingRadians = FMath::DegreesToRadians(
		FMath::Max(1.0f, ResolvedRules.MaxPendulumSwingDegrees));

	for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
	{
		const FVector PreviousGantryVelocity = GantryVelocity;
		StepDampedSpring(
			SimulatedGantryLocation,
			GantryVelocity,
			TargetGantry,
			ResolvedRules.GantryResponseFrequencyHz,
			ResolvedRules.GantryDampingRatio,
			StepSeconds);
		StepDampedSpring(
			SimulatedHubZ,
			HubVerticalVelocity,
			TargetHubZ,
			ResolvedRules.GantryResponseFrequencyHz * 1.15f,
			ResolvedRules.GantryDampingRatio,
			StepSeconds);

		const FVector GantryAcceleration = ((GantryVelocity - PreviousGantryVelocity)
			/ FMath::Max(StepSeconds, UE_SMALL_NUMBER)).GetClampedToMaxSize(2200.0f);
		const FVector2D HorizontalAcceleration(GantryAcceleration.X, GantryAcceleration.Y);
		const FVector2D SwingAcceleration =
			-CableSwingRadians * PendulumOmegaSquared
			-CableSwingVelocity * FMath::Max(0.1f, ResolvedRules.PendulumDampingPerSecond)
			-HorizontalAcceleration / PendulumLength;
		CableSwingVelocity += SwingAcceleration * StepSeconds;
		CableSwingRadians += CableSwingVelocity * StepSeconds;
		CableSwingRadians.X = FMath::Clamp(CableSwingRadians.X, -MaxSwingRadians, MaxSwingRadians);
		CableSwingRadians.Y = FMath::Clamp(CableSwingRadians.Y, -MaxSwingRadians, MaxSwingRadians);

		const float ActualCableLength = FMath::Max(8.0f, ClawTopZ - SimulatedHubZ);
		SimulatedHubLocation = FVector(
			SimulatedGantryLocation.X + FMath::Sin(CableSwingRadians.X) * ActualCableLength,
			SimulatedGantryLocation.Y + FMath::Sin(CableSwingRadians.Y) * ActualCableLength,
			SimulatedHubZ);
		const FVector TargetHeldPrize = SimulatedHubLocation + FVector(0.0f, 0.0f, -118.0f);
		if (TargetSnapshot.bCarryingPrize)
		{
			if (!bWasCarryingPrize)
			{
				SimulatedHeldPrizeLocation = TargetHeldPrize;
				HeldPrizeVelocity = FVector::ZeroVector;
			}
			StepDampedSpring(
				SimulatedHeldPrizeLocation,
				HeldPrizeVelocity,
				TargetHeldPrize,
				ResolvedRules.HeldPrizeResponseFrequencyHz,
				0.72f,
				StepSeconds);
		}
		bWasCarryingPrize = TargetSnapshot.bCarryingPrize;
	}
}

void AFF_ClawMachineArena3D::ApplySimulatedClaw(const FFF_ClawMachineSnapshot& Snapshot)
{
	const FVector CableVector = SimulatedHubLocation - SimulatedGantryLocation;
	const float CableLength = FMath::Max(8.0f, CableVector.Size());

	Carriage->SetRelativeLocation(SimulatedGantryLocation);
	Cable->SetRelativeLocation((SimulatedGantryLocation + SimulatedHubLocation) * 0.5f);
	Cable->SetRelativeRotation(FRotationMatrix::MakeFromZ(CableVector.GetSafeNormal()).Rotator());
	Cable->SetRelativeScale3D(FVector(0.07f, 0.07f, CableLength / 100.0f));
	ClawHub->SetRelativeLocation(SimulatedHubLocation);

	const float CloseAlpha =
		(Snapshot.Phase == EFF_ClawMachinePhase::Aiming
			|| Snapshot.Phase == EFF_ClawMachinePhase::Dropping)
		? 0.0f
		: (Snapshot.Phase == EFF_ClawMachinePhase::Closing ? Snapshot.PhaseAlpha : 1.0f);
	for (int32 Index = 0; Index < ClawTines.Num(); ++Index)
	{
		const float AngleRadians = 2.0f * UE_PI * static_cast<float>(Index) / 3.0f;
		const FVector RadialDirection(
			FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
		const float Radius = FMath::Lerp(58.0f, 28.0f, CloseAlpha);
		const float OutwardLean = FMath::Lerp(0.58f, 0.20f, CloseAlpha);
		const FVector TineDirection = FVector(
			RadialDirection.X * OutwardLean,
			RadialDirection.Y * OutwardLean,
			-1.0f).GetSafeNormal();
		const FRotator TineRotation = FRotationMatrix::MakeFromZ(TineDirection).Rotator();
		UStaticMeshComponent* Tine = ClawTines[Index];
		Tine->SetRelativeLocation(
			SimulatedHubLocation + RadialDirection * Radius + FVector(0.0f, 0.0f, -50.0f));
		Tine->SetRelativeRotation(TineRotation);
		if (UStaticMeshComponent* Tip = ClawTips.IsValidIndex(Index) ? ClawTips[Index].Get() : nullptr)
		{
			Tip->SetRelativeLocation(
				SimulatedHubLocation + RadialDirection * (Radius + 25.0f) + FVector(0.0f, 0.0f, -98.0f));
			Tip->SetRelativeRotation(TineRotation);
		}
	}
}

void AFF_ClawMachineArena3D::UpdatePrizes(const FFF_ClawMachineSnapshot& Snapshot)
{
	for (int32 Index = 0; Index < Snapshot.Prizes.Num() && Index < PrizeBodies.Num(); ++Index)
	{
		const FFF_ClawMachinePrizeSnapshot& Prize = Snapshot.Prizes[Index];
		UStaticMeshComponent* Body = PrizeBodies[Index];
		UStaticMeshComponent* Accent = PrizeAccents.IsValidIndex(Index) ? PrizeAccents[Index].Get() : nullptr;
		if (!Body)
		{
			continue;
		}
		const bool bHeld = Snapshot.bCarryingPrize && Snapshot.HeldPrizeSerial == Prize.PrizeSerial;
		const bool bVisible = Prize.bAvailable || bHeld;
		Body->SetVisibility(bVisible, true);
		if (Accent) { Accent->SetVisibility(bVisible && Accent->GetStaticMesh() != nullptr, true); }
		if (!bVisible)
		{
			continue;
		}

		FVector Location = NormalizedToPrizeFloor(Prize.PositionNormalized);
		if (bHeld)
		{
			Location = SimulatedHeldPrizeLocation;
		}
		Body->SetRelativeLocation(Location);
		Body->SetRelativeRotation(bHeld
			? FRotator(
				-FMath::RadiansToDegrees(CableSwingRadians.Y),
				Prize.RotationDegrees,
				FMath::RadiansToDegrees(CableSwingRadians.X))
			: FRotator(0.0f, Prize.RotationDegrees, 0.0f));
		if (Accent)
		{
			Accent->SetRelativeLocation(Location + FVector(-24.0f, 0.0f, 28.0f));
			Accent->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
}
