#include "Zone/FF_ZoneSystemPlaytestOverviewPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

// Enhanced Input 자산에 의존하지 않고 PlayerController의 원시 키 상태를 읽는다.
// 따라서 팀원이 만든 테스트 맵에도 별도 Input Mapping Context 없이 바로 사용할 수 있다.
namespace
{
    constexpr float MinimumCameraDistance = 1400.0f;
    constexpr float MaximumCameraDistance = 30000.0f;
    constexpr float DefaultPitchDegrees = -58.0f;
    constexpr float DefaultYawDegrees = -45.0f;
}

AFF_ZoneSystemPlaytestOverviewPawn::AFF_ZoneSystemPlaytestOverviewPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // 충돌 형상이 없는 루트를 사용해 시설과 지형이 카메라 이동을 방해하지 않게 한다.
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("OverviewRoot"));
    SetRootComponent(SceneRoot);

    // SpringArm 충돌 검사는 끈다. 시설 위를 지나갈 때 카메라가 갑자기 당겨지는 현상을 방지한다.
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("OverviewCameraBoom"));
    CameraBoom->SetupAttachment(SceneRoot);
    CameraBoom->TargetArmLength = InitialCameraDistance;
    CameraBoom->SetRelativeRotation(FRotator(DefaultPitchDegrees, DefaultYawDegrees, 0.0f));
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 12.0f;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 14.0f;

    OverviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));
    OverviewCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    OverviewCamera->FieldOfView = 60.0f;
    OverviewCamera->bUsePawnControlRotation = false;

    FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("OverviewMovement"));
    FloatingMovement->Acceleration = 16000.0f;
    FloatingMovement->Deceleration = 20000.0f;
    FloatingMovement->TurningBoost = 8.0f;
}

void AFF_ZoneSystemPlaytestOverviewPawn::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController || bUiInteractionMode)
    {
        return;
    }

    const FRotator CameraRotation = CameraBoom->GetRelativeRotation();
    const FRotator YawRotation(0.0f, CameraRotation.Yaw, 0.0f);
    const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    FVector MoveDirection = FVector::ZeroVector;
    MoveDirection += Forward * ((PlayerController->IsInputKeyDown(EKeys::W) ||
        PlayerController->IsInputKeyDown(EKeys::Up)) ? 1.0f : 0.0f);
    MoveDirection -= Forward * ((PlayerController->IsInputKeyDown(EKeys::S) ||
        PlayerController->IsInputKeyDown(EKeys::Down)) ? 1.0f : 0.0f);
    MoveDirection += Right * ((PlayerController->IsInputKeyDown(EKeys::D) ||
        PlayerController->IsInputKeyDown(EKeys::Right)) ? 1.0f : 0.0f);
    MoveDirection -= Right * ((PlayerController->IsInputKeyDown(EKeys::A) ||
        PlayerController->IsInputKeyDown(EKeys::Left)) ? 1.0f : 0.0f);

    // 줌 아웃 상태에서는 넓은 맵을 빠르게 훑을 수 있도록 이동 속도를 카메라 거리에 비례시킨다.
    const bool bFastPan = PlayerController->IsInputKeyDown(EKeys::LeftShift) ||
        PlayerController->IsInputKeyDown(EKeys::RightShift);
    const float DistanceScaledSpeed = FMath::Clamp(CameraBoom->TargetArmLength * 0.55f, 1000.0f, 8500.0f);
    FloatingMovement->MaxSpeed = DistanceScaledSpeed * (bFastPan ? 2.0f : 1.0f);
    if (!MoveDirection.IsNearlyZero())
    {
        AddMovementInput(MoveDirection.GetSafeNormal());
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollUp))
    {
        AdjustZoom(0.84f);
    }
    if (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollDown))
    {
        AdjustZoom(1.19f);
    }

    // 오른쪽 버튼을 누른 동안에만 회전시켜 좌클릭 시설 설치와 입력이 충돌하지 않게 한다.
    if (PlayerController->IsInputKeyDown(EKeys::RightMouseButton))
    {
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        PlayerController->GetInputMouseDelta(MouseX, MouseY);

        FRotator NewRotation = CameraRotation;
        NewRotation.Yaw += MouseX * 0.22f;
        NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch + MouseY * 0.16f, -82.0f, -32.0f);
        NewRotation.Roll = 0.0f;
        CameraBoom->SetRelativeRotation(NewRotation);
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::Home))
    {
        ResetOverview();
    }
}

void AFF_ZoneSystemPlaytestOverviewPawn::InitializeOverview(
    const FVector& FocusLocation,
    const float SuggestedCameraDistance)
{
    InitialFocusLocation = FocusLocation;
    InitialCameraDistance = FMath::Clamp(SuggestedCameraDistance, MinimumCameraDistance, MaximumCameraDistance);
    ResetOverview();
}

void AFF_ZoneSystemPlaytestOverviewPawn::SetUiInteractionMode(const bool bEnabled)
{
    bUiInteractionMode = bEnabled;
    if (bEnabled && FloatingMovement)
    {
        FloatingMovement->StopMovementImmediately();
    }
}

bool AFF_ZoneSystemPlaytestOverviewPawn::HasActiveOverviewCamera() const
{
    return OverviewCamera && OverviewCamera->IsActive() && CameraBoom &&
        CameraBoom->GetRelativeRotation().Pitch <= -30.0f &&
        CameraBoom->TargetArmLength >= MinimumCameraDistance;
}

void AFF_ZoneSystemPlaytestOverviewPawn::ResetOverview()
{
    SetActorLocation(InitialFocusLocation, false, nullptr, ETeleportType::TeleportPhysics);
    CameraBoom->TargetArmLength = InitialCameraDistance;
    CameraBoom->SetRelativeRotation(FRotator(DefaultPitchDegrees, DefaultYawDegrees, 0.0f));
    if (FloatingMovement)
    {
        FloatingMovement->StopMovementImmediately();
    }
}

void AFF_ZoneSystemPlaytestOverviewPawn::AdjustZoom(const float ZoomMultiplier)
{
    CameraBoom->TargetArmLength = FMath::Clamp(
        CameraBoom->TargetArmLength * ZoomMultiplier,
        MinimumCameraDistance,
        MaximumCameraDistance);
}

