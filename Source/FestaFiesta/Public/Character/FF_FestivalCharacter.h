// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FF_FestivalCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
struct FInputActionValue;

// 플레이어가 직접 조종하는 축제 방문객.
//
// 관찰 카메라(AFF_OverviewCameraPawn)와 역할이 다르다. 저쪽은 축제 전체를 내려다보며
// 배치와 흐름을 보는 용도고, 이쪽은 방문객 눈높이에서 걸어다니는 체험용이다.
// 둘 사이 전환은 AFF_FestivalPlayerController 가 담당한다.
//
// 이 캐릭터는 Crowd 시뮬레이션 대상이 아니다. NPC 군중은 FUnitData 배열로 일괄 처리되지만
// 플레이어는 CharacterMovement 로 직접 움직인다. 회피 계산에 끼어들지 않는다는 뜻이라
// 군중을 밀고 지나갈 수 있는데, 그건 Crowd 쪽에서 플레이어를 장애물로 인식하게 만들어야
// 해결된다. 지금 범위 밖이다.
//
// 입력은 캐릭터 자신이 받는다. 컨트롤러가 아니라 여기에 두는 이유는, 빙의가 옮겨오면
// Enhanced Input 이 자동으로 이 폰의 바인딩을 살리고 나갈 때 죽이기 때문이다.
// 컨트롤러에 두면 관찰 카메라 조작과 같은 키를 두고 직접 교통정리를 해야 한다.
UCLASS(Blueprintable)
class FESTAFIESTA_API AFF_FestivalCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFF_FestivalCharacter();

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Character")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Character")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	// 지금 달리는 중인지. 애니메이션 블루프린트가 읽는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Character")
	bool IsSprinting() const { return bSprinting; }

	// ── 입력 자산 ────────────────────────────────────────────────────────────
	// 비어 있으면 그 동작만 조용히 빠진다. 하나 안 넣었다고 캐릭터가 죽으면 곤란하다.

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputMappingContext> CharacterMappingContext;

	// 관찰 카메라 컨텍스트보다 우선순위를 높게 둔다. 같은 키가 겹쳐도 이쪽이 이긴다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	int32 CharacterMappingPriority = 10;

	// Axis2D. X 는 좌우, Y 는 전후.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> MoveAction;

	// Axis2D. 마우스 이동으로 시점 회전.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> LookAction;

	// Digital.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> JumpAction;

	// Digital. 누르는 동안 달린다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> SprintAction;

	// ── 이동 ─────────────────────────────────────────────────────────────────

	// 걷기 속도. 축제 방문객은 관람하며 다니므로 표준 600 보다 느리게 잡는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Movement", meta = (ClampMin = "50.0"))
	float WalkSpeed = 380.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Movement", meta = (ClampMin = "50.0"))
	float SprintSpeed = 700.f;

	// 마우스 감도 배수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Input", meta = (ClampMin = "0.05"))
	float LookSensitivity = 1.f;

protected:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);
	void HandleSprintStarted(const FInputActionValue& Value);
	void HandleSprintCompleted(const FInputActionValue& Value);

	void AddMappingContext();
	void RemoveMappingContext();
	void ApplyMovementSpeed();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

private:
	bool bSprinting = false;
};
