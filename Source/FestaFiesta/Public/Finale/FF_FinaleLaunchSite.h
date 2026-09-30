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

	/* 이 지점 하나를 바지선 여러 척으로 쓴다.

	   실제 다대포 해상 불꽃쇼(제30회 부산바다축제 개막, 2026-08-07)는 바다 위 바지선 3척에서
	   넓게 쏘아 올렸다. 액터를 세 개 놓고 간격을 손으로 맞추는 대신, 하나를 바다 위에 두고
	   척수와 간격만 정하면 좌우로 나란히 펼친다.
	   배열 방향은 액터의 오른쪽(Y)이다. 액터 앞(X)이 해변을 보게 두면 해안선을 따라 늘어선다.
	   1 이면 예전처럼 액터 위치 한 곳이다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale|Barge", meta = (ClampMin = "1", ClampMax = "9", UIMin = "1", UIMax = "5"))
	int32 BargeCount = 1;

	// 바지선 사이 간격(cm). 실제 간격 자료가 없어 화면을 보고 맞추는 튜닝값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale|Barge", meta = (ClampMin = "0.0", EditCondition = "BargeCount > 1"))
	float BargeSpacingCm = 15000.f;

	// 이 지점이 쓰는 발사 위치들. 가운데를 기준으로 좌우 대칭이다.
	void GetLaunchPoints(TArray<FVector>& OutPoints) const;

	// 레벨에 놓인 발사 지점을 Order 순으로 모은다.
	static void CollectAll(const UWorld* World, TArray<AFF_FinaleLaunchSite*>& OutSites);

	// 모든 지점의 발사 위치를 Order, 바지선 순으로 펼쳐 모은다. 연출은 이 목록만 본다.
	static void CollectLaunchPoints(const UWorld* World, TArray<FVector>& OutPoints);

	// 가운데 기준 좌우 오프셋(cm). 3척·간격 100 이면 -100, 0, 100. 테스트가 이 계산을 직접 본다.
	static void ComputeBargeOffsets(int32 Count, float SpacingCm, TArray<float>& OutOffsets);

protected:
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Finale")
	TObjectPtr<USceneComponent> SceneRoot;
};
