// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_StaffTypes.generated.h"

// 스태프 한 명의 현재 상태. Idle → Moving → Working → Returning → Idle
UENUM(BlueprintType)
enum class EFF_StaffState : uint8
{
	Idle,
	Moving,
	Working,
	Returning
};

DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnStaffArrived, int32 /*StaffId*/);
