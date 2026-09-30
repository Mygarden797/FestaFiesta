// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_StaffBase.generated.h"

class AFF_StaffActor;

// 레벨에 놓는 스태프 본부. 스태프가 스폰되고 대기하며 복귀하는 위치
UCLASS()
class FESTAFIESTA_API AFF_StaffBase : public AActor
{
	GENERATED_BODY()

public:
	AFF_StaffBase();

	FVector GetSlotLocation(int32 SlotIndex) const;

	TSubclassOf<AFF_StaffActor> GetStaffActorClass() const { return StaffActorClass; }
	float GetBaseMoveSpeed() const { return BaseMoveSpeed; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Staff")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(EditAnywhere, Category = "Staff")
	TSubclassOf<AFF_StaffActor> StaffActorClass;

	UPROPERTY(EditAnywhere, Category = "Staff", meta = (ClampMin = "0.0"))
	float SlotSpacing = 150.f;

	UPROPERTY(EditAnywhere, Category = "Staff", meta = (ClampMin = "1"))
	int32 SlotsPerRow = 5;

	UPROPERTY(EditAnywhere, Category = "Staff", meta = (ClampMin = "0.0"))
	float BaseMoveSpeed = 300.f;
};
