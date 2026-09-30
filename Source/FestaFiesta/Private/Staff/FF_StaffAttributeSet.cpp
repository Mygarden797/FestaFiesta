// Fill out your copyright notice in the Description page of Project Settings.

#include "Staff/FF_StaffAttributeSet.h"


UFF_StaffAttributeSet::UFF_StaffAttributeSet()
{
}

void UFF_StaffAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
}
