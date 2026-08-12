// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FF_FestivalGameMode.generated.h"

// 관찰 시점 게임 모드.
//
// 하는 일은 카메라 Pawn 과 PlayerController 를 이어주는 것뿐이다.
// 규칙, 승패, 스폰 흐름은 여기 넣지 않는다. 그건 정해지면 각자 주인이 생길 것이고,
// 지금 미리 자리를 잡아두면 나중에 그 사람이 이 파일을 고쳐야 한다.
//
// 레벨의 축제장 범위를 알고 있으면 InitialFocusLocation 과 InitialArmLength 를
// 지정해 첫 구도를 잡아줄 수 있다. 안 지정하면 Pawn 이 자기 위치를 기준으로 시작한다.
UCLASS()
class FESTAFIESTA_API AFF_FestivalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFF_FestivalGameMode();

	virtual void BeginPlay() override;

	// 축제장 중심. 보통 메인 게이트 앞이나 중앙 광장이다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera")
	FVector InitialFocusLocation = FVector::ZeroVector;

	// 축제장 전체가 한 화면에 들어오는 거리.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera", meta = (ClampMin = "100.0"))
	float InitialArmLength = 12000.f;

	// true 면 BeginPlay 에서 위 두 값으로 카메라 구도를 잡는다.
	// 레벨에 배치한 Pawn 의 위치를 그대로 쓰고 싶으면 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera")
	bool bApplyInitialFraming = true;
};
