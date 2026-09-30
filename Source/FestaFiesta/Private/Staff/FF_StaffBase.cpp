// Fill out your copyright notice in the Description page of Project Settings.

#include "Staff/FF_StaffBase.h"

#include "Staff/FF_StaffActor.h"


AFF_StaffBase::AFF_StaffBase()
{
}

FVector AFF_StaffBase::GetSlotLocation(int32 SlotIndex) const
{
	return GetActorLocation();
}
