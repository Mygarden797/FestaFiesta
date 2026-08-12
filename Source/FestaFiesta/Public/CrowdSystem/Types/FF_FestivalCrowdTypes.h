// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_FestivalCrowdTypes.generated.h"



enum class EUnitAnimationState : uint8
{
	Idle,
	Walk,
	Run
};



USTRUCT(BlueprintType)
struct FUnitData
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FVector Position = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FRotator Rotation = FRotator::ZeroRotator;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FVector Velocity = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FVector TargetLocation = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	float Speed = 150.f;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	bool bIsActive = true;
	
	//----Group---//
	FName ArchetypeID;
	
	FName CommandType;
	
	int32 PartyID;
	
	int32 StableAgentID;
	
	//---회피 관련 변수들 ---//
	FVector Force = FVector::ZeroVector;
	
	float MaxSpeed = 150.f;
	
	float Radius = 34.f;
	
	//Animation
	
	
};

USTRUCT(BlueprintType)
struct FCrowdGroupTypes
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Crowd|Group")
	int32 GroupID = 0;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Crowd|Group")
	FName GroupName = NAME_None;
};

