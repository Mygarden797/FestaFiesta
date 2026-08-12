// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Types/FF_FestivalCrowdTypes.h"
#include "FF_BaseCrowd.generated.h"

class UFF_CrowdAnimInstance;
class UFF_CrowdMovementComponent;
class UFF_CrowdVisualComponent;
class UStaticMeshComponent;


//BaseCrowd 비용을 아끼기 위해서 모든 이동 정보는 Subsystem에서 담당하고, Actor는 오직 후보정이 끝난 값만 받아서, 
//Tick 당 이동하기 위한 함수만 처리한다.



UCLASS()
class FESTAFIESTA_API AFF_BaseCrowd : public APawn
{
	GENERATED_BODY()

public:

	AFF_BaseCrowd();
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	//--- Agent와의 연결/연결 해제를 위한 함수 ---/
	virtual void BindToAgent(int32 NewAgentIndex, FVector SpawnLocation, FRotator SpawnRotation);
	virtual void ReleaseFromAgent();
	// --- 후 보정이 끝난 이동 값을 그대로 적용하기 위한 함수 ---/
	virtual void PushPresentation(FVector NewPosition, FRotator NewRotator, float NewSpeed);

	/**
	 * 시뮬레이션이 밀어 넣은 속도를 반환한다.
	 *
	 * APawn 기본 구현은 물리 시뮬레이션이나 무브먼트 컴포넌트가 없으면 항상 0을 반환한다.
	 * 이 군중은 둘 다 없이 위치를 직접 밀어 넣으므로 기본 구현으로는 늘 0이 나오고,
	 * GetVelocity()로 속도를 구하는 AnimBP나 외부 코드가 전부 정지 상태로 오해한다.
	 */
	virtual FVector GetVelocity() const override { return CachedVelocity; }
	
protected:



public:
	/**
	 * 빈 루트. 트랜스폼을 받는 지점이다.
	 *
	 * 메시를 루트로 두지 않는 이유: 루트 컴포넌트의 상대 트랜스폼은 곧 액터 트랜스폼이고,
	 * 그 값은 매 프레임 PushPresentation의 SetActorLocationAndRotation이 덮어쓴다.
	 * 그래서 메시가 루트면 BP에서 메시 방향/높이를 보정할 자리가 없다.
	 *
	 * 루트를 따로 두면 SkeletalMeshComponent의 상대 회전이 살아남는다.
	 * ACharacter가 Mesh를 CapsuleComponent 밑에 두고 Rotation Z = -90을 주는 것과 같은 구조다.
	 */
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Component")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// BP에서 상대 회전/위치를 자유롭게 줄 수 있다. 메시 정면이 +X가 아니면 여기서 Yaw를 보정한다
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Component")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Crowd|Animation")
	TObjectPtr<UFF_CrowdAnimInstance> CrowdAnimInstance;

	/**
	 * SkeletalMesh가 없을 때만 켜지는 대체 표시용 원기둥.
	 *
	 * 콘솔(FF.Crowd.Spawn)로 띄우면 어떤 BP를 쓸지 알 방법이 없어 메시 없는 AFF_BaseCrowd가 스폰된다.
	 * 그러면 시뮬레이션은 도는데 화면에는 아무것도 안 보여서, 빌드해 본 사람이 동작 여부를 확인할 수 없다.
	 * 이 원기둥이 그 경우의 눈이다. 메시가 설정된 BP를 쓰면 BeginPlay에서 꺼진다.
	 */
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Component")
	TObjectPtr<UStaticMeshComponent> ProxyMeshComponent;

	/**
	 * 액터 전체에 더할 보정 Yaw, 도. 기본 0.
	 *
	 * **메시 방향 보정은 BP에서 SkeletalMeshComponent의 상대 회전으로 하는 것이 정석이다.**
	 * 이 값은 그 대신이 아니라, BP 없이 콘솔로만 확인할 때(FF.Crowd.MeshYaw) 쓰는 런타임 손잡이다.
	 *
	 * 둘 다 걸면 더해진다. BP 쪽에 값을 넣었으면 여기는 0으로 두는 편이 헷갈리지 않는다.
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Crowd|Visual")
	float MeshYawOffset = 0.f;

private:
	int32 AgentIndex = INDEX_NONE;

	// GetVelocity()가 돌려줄 값. PushPresentation에서 갱신한다
	FVector CachedVelocity = FVector::ZeroVector;
	
	
};
