// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "FF_StaffAttributeSet.generated.h"

// 스태프 능력치(GAS). Subsystem 은 MoveSpeed 만 읽고 값의 출처(레벨·버프)는 모른다
UCLASS()
class FESTAFIESTA_API UFF_StaffAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UFF_StaffAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS_BASIC(UFF_StaffAttributeSet, MoveSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	FGameplayAttributeData WorkPower;
	ATTRIBUTE_ACCESSORS_BASIC(UFF_StaffAttributeSet, WorkPower)
};
