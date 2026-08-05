// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_SpawnActor.generated.h"



UCLASS()
class FESTAFIESTA_API AFF_SpawnActor : public AActor
{
	GENERATED_BODY()

public:

	AFF_SpawnActor();

protected:
	
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "FF|Components")
	TObjectPtr<UInstancedStaticMeshComponent> ISMComponent;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "FF|Crowd")
	int32 UnitCount = 500;


};
