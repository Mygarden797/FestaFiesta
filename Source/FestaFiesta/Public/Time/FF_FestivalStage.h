// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_FestivalStage.generated.h"

UENUM(BlueprintType)
enum class EFF_FestivalStage : uint8 //축제 단계를 나타내는 이름표
{
    Setup             UMETA(DisplayName = "셋업"),
    PromotionPreview  UMETA(DisplayName = "홍보 프리뷰"),
    SunsetTransition  UMETA(DisplayName = "노을 전환"),
    MorningCheck      UMETA(DisplayName = "모닝체크"),
    Live              UMETA(DisplayName = "라이브 운영"),
    Closing           UMETA(DisplayName = "마감"),
    Report            UMETA(DisplayName = "리포트"),
    Finale            UMETA(DisplayName = "피날레")
};