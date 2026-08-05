// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_BaseNPC.generated.h"

UCLASS()
class FESTAFIESTA_API AFF_BaseNPC : public AActor
{
	GENERATED_BODY()

public:

	AFF_BaseNPC();
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
	virtual void MoveToTargetLocation(FVector TargetLocation,float DeltaTime);
	

protected:

	

public:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC|Component")
	TObjectPtr<USceneComponent> SceneComponent;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="NPC|Component")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
	
	
	
};
