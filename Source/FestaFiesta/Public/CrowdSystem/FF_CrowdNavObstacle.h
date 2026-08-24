// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_CrowdNavObstacle.generated.h"

class UBoxComponent;

/**
 * 시설 하나의 바닥면만큼 NavMesh를 도려내는 액터.
 *
 * ── 왜 필요한가 ────────────────────────────────────────────────────────────────
 * AFFZSFacilityActor 의 컴포넌트는 전부 SetCanEverAffectNavigation(false) 다.
 * 그래서 NavMesh 는 부스 아래가 뚫린 채로 구워지고, FindPathSync 는 부스를 관통하는
 * 2점 직선을 정상 경로로 돌려준다. 경로추종을 켜도 웨이포인트가 목적지 하나뿐이라
 * 직선 조향과 같아지고, 군중은 벽 반발에만 기대 부스에 붙어 미끄러진다.
 *
 * ── 왜 시설 액터를 고치지 않고 별도 액터인가 ────────────────────────────────────
 * ZoneSystem 은 다른 파트가 소유한 플러그인이다. 남의 파일을 고치는 대신, 이미 공개된
 * 읽기 경로(UFFZSWorldSubsystem::BuildReadModel + FindFacility)만 써서 Crowd 쪽에서
 * 구멍을 만든다. 벽 반발용 선분(FFF_CrowdWallSegment)과 **같은 소스, 같은 타이밍**으로
 * 갱신되므로 둘이 어긋날 수 없다는 이점도 있다.
 *
 * ── 왜 ANavModifierVolume 이 아닌가 ────────────────────────────────────────────
 * AVolume 은 브러시(UModel) 기반이라 런타임 스폰에 적합하지 않다. UBoxComponent 에
 * bDynamicObstacle 을 켜면 물리 콜리전 없이 area modifier 만 나가고
 * (UShapeComponent::GetNavigationData), 회전·크기도 그대로 반영된다.
 *
 * 소유·수명은 UFF_CrowdSimulationSubsystem 이 가진다. 직접 배치하지 말 것.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class FESTAFIESTA_API AFF_CrowdNavObstacle : public AActor
{
	GENERATED_BODY()

public:
	AFF_CrowdNavObstacle();

	void ConfigureForFacility(const FVector& CentreCm, float YawDegrees, const FVector& HalfExtentCm);

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> ObstacleBox;

};
