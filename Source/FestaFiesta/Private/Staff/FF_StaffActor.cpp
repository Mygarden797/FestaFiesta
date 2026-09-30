// Fill out your copyright notice in the Description page of Project Settings.

#include "Staff/FF_StaffActor.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "Staff/FF_StaffAttributeSet.h"


AFF_StaffActor::AFF_StaffActor()
{
}

UAbilitySystemComponent* AFF_StaffActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AFF_StaffActor::SetStaffId(int32 InStaffId)
{
}

void AFF_StaffActor::InitFromLevel(int32 TrainingLevel)
{
}

float AFF_StaffActor::GetMoveSpeed() const
{
	return 0.f;
}

float AFF_StaffActor::GetWorkPower() const
{
	return 0.f;
}

bool AFF_StaffActor::ApplyEffect(TSubclassOf<UGameplayEffect> Effect)
{
	return false;
}

void AFF_StaffActor::BeginPlay()
{
	Super::BeginPlay();
}
