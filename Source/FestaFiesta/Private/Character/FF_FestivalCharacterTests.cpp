// Copyright Epic Games, Inc. All Rights Reserved.

// 역할: 플레이어만 시설에 부딪히고 일반 캐릭터는 통과하는지 실제 이동으로 검증한다.

#include "Character/FF_FestivalCharacter.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FF_FFZSCollision.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalCharacterFacilityCollisionTest,
	"FestaFiesta.Character.FacilityCollision.PlayerCapsuleSweepAndBlueprintRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalCharacterFacilityCollisionTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Transient physics world"), World)
		|| !TestNotNull(TEXT("Engine cube with authored simple collision"), Cube))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFF_FestivalCharacter* Player = World->SpawnActor<AFF_FestivalCharacter>(
		AFF_FestivalCharacter::StaticClass(), FVector(-250.0, -150.0, 100.0),
		FRotator::ZeroRotator, SpawnParameters);
	ACharacter* OtherCharacter = World->SpawnActor<ACharacter>(
		ACharacter::StaticClass(), FVector(-250.0, 150.0, 100.0),
		FRotator::ZeroRotator, SpawnParameters);
	AActor* FacilityBodyOwner = World->SpawnActor<AActor>(
		AActor::StaticClass(), FVector(0.0, 0.0, 100.0),
		FRotator::ZeroRotator, SpawnParameters);
	if (!TestNotNull(TEXT("Actual festival player"), Player)
		|| !TestNotNull(TEXT("Unmodified ordinary character"), OtherCharacter)
		|| !TestNotNull(TEXT("Facility collision fixture"), FacilityBodyOwner))
	{
		return false;
	}

	// The plugin tests cover real facility lifecycle changes. This independent body
	// isolates the game's opt-in without modifying any facility asset or NPC class.
	UStaticMeshComponent* FacilityBody = NewObject<UStaticMeshComponent>(FacilityBodyOwner);
	FacilityBodyOwner->SetRootComponent(FacilityBody);
	FacilityBodyOwner->AddInstanceComponent(FacilityBody);
	FacilityBody->SetMobility(EComponentMobility::Movable);
	FacilityBody->SetStaticMesh(Cube);
	FacilityBody->SetCollisionObjectType(FFZSCollision::FacilityPlayerObjectChannel);
	FacilityBody->SetCollisionResponseToAllChannels(ECR_Ignore);
	FacilityBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	FacilityBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FacilityBody->SetGenerateOverlapEvents(false);
	FacilityBody->SetCanEverAffectNavigation(false);
	FacilityBody->RegisterComponent();
	FacilityBody->SetWorldLocation(FVector(0.0, 0.0, 100.0));
	FacilityBody->SetWorldScale3D(FVector(1.0, 8.0, 4.0));

	UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
	UCapsuleComponent* OtherCapsule = OtherCharacter->GetCapsuleComponent();
	TestEqual(TEXT("Player constructor opts into facility collision"),
		PlayerCapsule->GetCollisionResponseToChannel(FFZSCollision::FacilityPlayerObjectChannel), ECR_Block);
	TestEqual(TEXT("Ordinary characters retain the channel's default Ignore response"),
		OtherCapsule->GetCollisionResponseToChannel(FFZSCollision::FacilityPlayerObjectChannel), ECR_Ignore);
	TestEqual(TEXT("Player remains a Pawn for existing gameplay collision"),
		PlayerCapsule->GetCollisionObjectType(), ECC_Pawn);
	TestEqual(TEXT("Player still blocks static ground and walls"),
		PlayerCapsule->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Block);
	TestEqual(TEXT("Player still blocks existing dynamic obstacles"),
		PlayerCapsule->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);

	// Emulate an existing Blueprint's serialized component response overriding the
	// new C++ default. Route the normal actor BeginPlay after component initialization.
	World->InitializeActorsForPlay(FURL());
	TestFalse(TEXT("Fixture leaves player BeginPlay pending"), Player->HasActorBegunPlay());
	PlayerCapsule->SetCollisionResponseToChannel(FFZSCollision::FacilityPlayerObjectChannel, ECR_Ignore);
	Player->DispatchBeginPlay();
	TestTrue(TEXT("Player BeginPlay ran"), Player->HasActorBegunPlay());
	TestEqual(TEXT("BeginPlay repairs the serialized Blueprint Ignore response"),
		PlayerCapsule->GetCollisionResponseToChannel(FFZSCollision::FacilityPlayerObjectChannel), ECR_Block);
	TestEqual(TEXT("BeginPlay preserves Pawn object type"), PlayerCapsule->GetCollisionObjectType(), ECC_Pawn);
	TestEqual(TEXT("BeginPlay preserves static world blocking"),
		PlayerCapsule->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Block);
	TestEqual(TEXT("BeginPlay preserves dynamic world blocking"),
		PlayerCapsule->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Block);

	// A single long swept movement crosses the entire obstacle. Checking both the
	// hit component and final position catches configurations that only report a hit.
	FHitResult PlayerHit;
	PlayerCapsule->MoveComponent(FVector(500.0, 0.0, 0.0), PlayerCapsule->GetComponentQuat(), true, &PlayerHit);
	TestTrue(TEXT("Actual player capsule sweep is blocked"), PlayerHit.bBlockingHit);
	TestEqual(TEXT("The authored mesh collision produced the blocking hit"),
		PlayerHit.GetComponent(), static_cast<UPrimitiveComponent*>(FacilityBody));
	TestTrue(TEXT("Player stops outside the mesh instead of tunneling through it"),
		PlayerCapsule->GetComponentLocation().X <= -50.0 - PlayerCapsule->GetScaledCapsuleRadius() + 1.0);

	FHitResult OtherHit;
	OtherCapsule->MoveComponent(FVector(500.0, 0.0, 0.0), OtherCapsule->GetComponentQuat(), true, &OtherHit);
	TestFalse(TEXT("The same mesh does not block an ordinary character"), OtherHit.bBlockingHit);
	TestTrue(TEXT("Ordinary character completes the same swept distance"),
		FMath::IsNearlyEqual(OtherCapsule->GetComponentLocation().X, 250.0, 0.1));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
