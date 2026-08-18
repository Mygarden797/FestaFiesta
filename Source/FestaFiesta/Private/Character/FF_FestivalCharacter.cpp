// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/FF_FestivalCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/FF_CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"

AFF_FestivalCharacter::AFF_FestivalCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	// 반지름 34cm 는 Crowd 에이전트와 같은 값이다. 연동 가이드가 NPC 두 명의 비겹침
	// 중심거리를 68cm 로 잡고 시설 간격을 그 기준으로 보정했으므로, 플레이어만 다른 몸집이면
	// 대기열 슬롯이나 통로에서 혼자 끼거나 혼자 통과하는 상황이 생긴다.

	// 컨트롤러 회전은 스프링암만 따라간다. 몸은 이동 방향을 본다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->MinAnalogWalkSpeed = 20.f;
		Movement->BrakingDecelerationWalking = 2000.f;
		Movement->JumpZVelocity = 420.f;
		Movement->AirControl = 0.2f;
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 320.f;
	CameraBoom->bUsePawnControlRotation = true;
	// 관찰 카메라와 달리 여기서는 충돌 검사를 켠다. 3인칭 시점이라 벽이 카메라와 몸 사이에
	// 끼면 화면이 막히는데, 부스가 빽빽한 축제장에서는 그 상황이 자주 나온다.
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->SocketOffset = FVector(0.f, 40.f, 60.f);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AFF_FestivalCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyMovementSpeed();
}

void AFF_FestivalCharacter::ApplyMovementSpeed()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
	}
}

void AFF_FestivalCharacter::AddMappingContext()
{
	if (!CharacterMappingContext)
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[캐릭터] CharacterMappingContext 가 비어 있어 조작이 동작하지 않는다. ")
			TEXT("캐릭터 블루프린트에서 IMC 와 InputAction 을 지정할 것."));
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		InputSubsystem->AddMappingContext(CharacterMappingContext, CharacterMappingPriority);
	}
}

void AFF_FestivalCharacter::RemoveMappingContext()
{
	if (!CharacterMappingContext)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		InputSubsystem->RemoveMappingContext(CharacterMappingContext);
	}
}

void AFF_FestivalCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// 빙의가 들어온 시점이다. 여기서 컨텍스트를 붙여야 캐릭터를 조종하는 동안에만
	// 이 키들이 살아 있다. 컨트롤러 BeginPlay 에서 붙이면 관찰 시점에서도 계속 켜져 있다.
	AddMappingContext();
}

void AFF_FestivalCharacter::UnPossessed()
{
	// 관찰 시점으로 돌아갈 때 캐릭터 키를 남겨두면 WASD 가 양쪽에 다 먹는다.
	RemoveMappingContext();

	// 달리던 중에 빙의가 빠지면 상태가 남는다.
	bSprinting = false;
	ApplyMovementSpeed();

	Super::UnPossessed();
}

void AFF_FestivalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[캐릭터] EnhancedInputComponent 가 아니다. 프로젝트 입력 설정을 확인할 것."));
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFF_FestivalCharacter::HandleMove);
	}
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFF_FestivalCharacter::HandleLook);
	}
	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AFF_FestivalCharacter::HandleJumpStarted);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFF_FestivalCharacter::HandleJumpCompleted);
	}
	if (SprintAction)
	{
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AFF_FestivalCharacter::HandleSprintStarted);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AFF_FestivalCharacter::HandleSprintCompleted);
	}
}

void AFF_FestivalCharacter::HandleMove(const FInputActionValue& Value)
{
	const AController* OwningController = GetController();
	if (!OwningController)
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();
	if (Input.IsNearlyZero())
	{
		return;
	}

	// 카메라가 보는 방향 기준으로 움직인다. Pitch 를 빼야 위를 보고 있을 때 땅으로 파고들지 않는다.
	const FRotator YawOnly(0.f, OwningController->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void AFF_FestivalCharacter::HandleLook(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X * LookSensitivity);
	AddControllerPitchInput(Input.Y * LookSensitivity);
}

void AFF_FestivalCharacter::HandleJumpStarted(const FInputActionValue& Value)
{
	Jump();
}

void AFF_FestivalCharacter::HandleJumpCompleted(const FInputActionValue& Value)
{
	StopJumping();
}

void AFF_FestivalCharacter::HandleSprintStarted(const FInputActionValue& Value)
{
	bSprinting = true;
	ApplyMovementSpeed();
}

void AFF_FestivalCharacter::HandleSprintCompleted(const FInputActionValue& Value)
{
	bSprinting = false;
	ApplyMovementSpeed();
}
