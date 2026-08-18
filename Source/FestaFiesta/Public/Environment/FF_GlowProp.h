// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FF_GlowProp.generated.h"

// This class does not need to be modified.
UINTERFACE(BlueprintType)
class UFF_GlowProp : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class FESTAFIESTA_API IFF_GlowProp
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GlowProp")

    void ActivateGlow();
    // ActivateGlow : 불 켜줘라고 부를 함수 

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GlowProp")
    void DeactivateGlow();
    // DeactivateGlow : 불 꺼줘라고 부를 함수
};
