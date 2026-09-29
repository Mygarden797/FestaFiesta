// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_RiverRespawnPlane.generated.h"

class AFF_FestivalCharacter;
class UBoxComponent;

// 수면 높이 기준으로 강에 빠졌는지 판정
// 강에 빠진 방문객 캐릭터를 감지해 안전한 땅으로 되돌리게 하는 액터.
//
// 반응하는 것은 플레이어가 조종 중인 AFF_FestivalCharacter 뿐이다.
// 관찰 카메라 폰과 Crowd 의 NPC 는 대상이 아니다.
UCLASS(meta = (DisplayName = "FF River Respawn Plane"))
class FESTAFIESTA_API AFF_RiverRespawnPlane : public AActor
{
	GENERATED_BODY()

public:
	AFF_RiverRespawnPlane();

	virtual void BeginPlay() override;

	// 발이 액터 Z 보다 이만큼 더 내려가야 빠진 것으로 본다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Respawn", meta = (ClampMin = "0.0"))
	float TriggerDepthCm = 10.f;

	// 판정 주기.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Respawn", meta = (ClampMin = "0.02"))
	float CheckIntervalSeconds = 0.1f;

private:
	void CheckCharacter();
	bool IsInsideRegion(const FVector& WorldLocation) const;

	FTimerHandle CheckTimer;

	// 적용 구역을 눈으로 보이게 하는 박스. 콜리전은 없다.
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Respawn")
	TObjectPtr<UBoxComponent> Region;
};
