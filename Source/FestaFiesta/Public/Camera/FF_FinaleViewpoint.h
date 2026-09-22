// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FinaleViewpoint.generated.h"

class UCameraComponent;
class USceneComponent;

// 피날레 연출을 보여주는 고정 시점.
//
// ─── 왜 관찰 카메라를 쓰지 않는가 ───────────────────────────────────────────
// 관찰 카메라는 피치가 -35 ~ -72 도로 항상 아래를 내려다본다. 배치와 동선을 읽는
// 시점이라 그게 맞는데, 불꽃은 상공에서 터지므로 그 각도로는 화면 위쪽이 잘린다.
//
// 관찰 카메라의 피치 범위를 위로 여는 방법도 있지만, 그러면 평상시 조작으로도
// 하늘을 볼 수 있게 되어 배치 게임 시점이 무너진다. 최소 지상고 보정이 각도를
// 세우는 방향으로만 동작하는 것과도 충돌한다.
//
// 그래서 연출 전용 시점을 따로 둔다. 연출이 끝나면 관찰 카메라로 돌아가는데,
// 그쪽은 아무도 건드리지 않았으므로 떠날 때 구도가 그대로 남아 있다.
//
// ─── 배치 방법 ──────────────────────────────────────────────────────────────
// 레벨에 놓고 원하는 구도로 회전시키면 된다. 발사 지점을 지정하면 그쪽을 자동으로
// 바라보므로, 대충 놓고 대상만 연결해도 구도가 잡힌다.
//
// 레벨이 아직 없어서 위치는 나중에 정한다. 지금은 어디에 놓아도 동작하게 해둔다.
UCLASS(Blueprintable)
class FESTAFIESTA_API AFF_FinaleViewpoint : public AActor
{
	GENERATED_BODY()

public:
	AFF_FinaleViewpoint();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Finale")
	UCameraComponent* GetViewCamera() const { return ViewCamera; }

	// 이 시점이 바라볼 지점. 보통 발사대다.
	// 비어 있으면 배치한 회전을 그대로 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Finale")
	void SetLookTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Finale")
	AActor* GetLookTarget() const { return LookTarget.Get(); }

	// 대상을 바라보는 회전을 계산한다. 대상이 없으면 현재 회전을 그대로 돌려준다.
	//
	// 불꽃은 발사 지점보다 한참 위에서 터지므로 대상의 발밑을 겨누면 화면 아래를 본다.
	// LookAtHeightOffset 만큼 올려서 겨눈다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Finale")
	FRotator ResolveLookRotation() const;

	// 레벨에 놓인 시점을 찾는다. 없으면 nullptr.
	// 여러 개면 Priority 가 가장 높은 것을 고른다.
	static AFF_FinaleViewpoint* FindBest(const UWorld* World);

	/* 순회 연출에 쓸 시점들을 TourOrder 순으로 모은다.

	   FindBest 는 한 자리만 고르는 함수다. 낮은 등급은 그것으로 충분하지만
	   최고 등급은 여러 자리를 돌아야 해서 목록이 필요하다.
	   같은 액터를 두 용도로 쓰되 고르는 기준만 나눈다.
	*/
	static void CollectTour(const UWorld* World, TArray<AFF_FinaleViewpoint*>& OutViewpoints);

	// 바라볼 대상. 레벨에서 지정한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "FestaFiesta|Finale")
	TObjectPtr<AActor> LookTargetActor;

	// 대상 위치에서 이만큼 위를 겨눈다. 불꽃이 터지는 높이에 맞춘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale")
	float LookAtHeightOffset = 20000.f;

	// true 면 매 프레임 대상을 다시 겨눈다. 발사대가 움직이거나 여러 곳에서
	// 순차로 터질 때 쓴다. 고정 연출이면 꺼두는 편이 화면이 안정적이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale")
	bool bTrackTargetEveryFrame = false;

	// 순회 연출에 이 시점을 넣을지.
	//
	// 고정 연출용으로만 두고 순회에서는 빼고 싶은 자리가 있다. 예를 들어
	// 전체를 넓게 잡는 시점은 처음과 끝에만 쓰고 중간에는 안 쓰는 편이 낫다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale|Tour")
	bool bIncludeInTour = true;

	// 순회 순서. 작은 쪽부터 간다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale|Tour")
	int32 TourOrder = 0;

	// 이 자리에 머무는 시간(초).
	//
	// 자리마다 다르게 두는 이유는 볼거리가 다르기 때문이다. 발사대 바로 옆은
	// 짧게 스치고, 전체가 보이는 자리는 오래 둬야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale|Tour", meta = (ClampMin = "0.5"))
	float TourHoldSeconds = 6.f;

	// 이 자리로 넘어올 때 걸리는 시간(초).
	//
	// 0 이면 즉시 잘린다. 컷 전환을 원하면 0 으로 두고, 흐르듯 이어가려면 값을 준다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale|Tour", meta = (ClampMin = "0.0"))
	float TourBlendSeconds = 2.f;

	// 시점이 여럿일 때 고르는 기준. 큰 쪽이 이긴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Finale")
	int32 Priority = 0;

protected:
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Finale")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Finale")
	TObjectPtr<UCameraComponent> ViewCamera;

private:
	TWeakObjectPtr<AActor> LookTarget;
};
