// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FinaleLaunchSite.generated.h"

class USceneComponent;

// 불꽃이 올라가는 지점.
//
// 레벨에 놓기만 하면 된다. 연출 서브시스템이 알아서 찾는다.
// 여러 개 놓으면 순서대로 돌아가며 쏘므로, 축제장 양끝에 하나씩 두면
// 좌우에서 번갈아 터진다.
//
// 아직 레벨이 없어서 하나도 없을 수 있다. 그때는 원점 위에서 터지고 경고만 남긴다.
UCLASS(Blueprintable)
class FESTAFIESTA_API AFF_FinaleLaunchSite : public AActor
{
	GENERATED_BODY()

public:
	AFF_FinaleLaunchSite();

	// 이 지점에서 쏠 때 반경을 개별로 조정하고 싶을 때 쓴다.
	// 0 이하면 등급 설정값을 그대로 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale", meta = (ClampMin = "0.0"))
	float SpreadRadiusOverride = 0.f;

	// 여러 지점이 있을 때 도는 순서. 작은 쪽이 먼저다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale")
	int32 Order = 0;

	// 레벨에 놓인 발사 지점을 Order 순으로 모은다.
	static void CollectAll(const UWorld* World, TArray<AFF_FinaleLaunchSite*>& OutSites);

protected:
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Finale")
	TObjectPtr<USceneComponent> SceneRoot;
};
