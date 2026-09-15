// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_TutorialMarker.generated.h"

class UBillboardComponent;
class USceneComponent;

// 튜토리얼이 가리킬 세계의 한 지점.
//
// ─── 왜 좌표를 데이터에 안 적는가 ───────────────────────────────────────────
// CSV 에 좌표를 적으면 맵을 조금만 옮겨도 전부 어긋난다. 그리고 어긋났다는 걸
// 알아채기 어렵다. 레벨에 놓으면 눈으로 보면서 맞출 수 있고, 맵을 고칠 때
// 같이 움직인다.
//
// ─── 이름으로만 부른다 ──────────────────────────────────────────────────────
// 대사에서는 이름만 적는다. UI 를 가리킬 때와 같은 컬럼을 쓰므로,
// 쓰는 쪽은 이게 화면 위인지 세계 안인지 신경 쓰지 않아도 된다.
UCLASS(meta = (DisplayName = "FF Tutorial Marker"))
class FESTAFIESTA_API AFF_TutorialMarker : public AActor
{
	GENERATED_BODY()

public:
	AFF_TutorialMarker();

	// 대사에서 이 지점을 부를 이름.
	//
	// 비워두면 액터 이름을 쓴다. 이름을 안 정해도 동작은 하게 두되,
	// 액터 이름은 복사하면 뒤에 숫자가 붙으므로 직접 적는 편이 낫다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial")
	FName AnchorId;

	// 강조 반경. 화면에 그려질 원의 크기를 정한다.
	//
	// 시설 하나를 가리킬 때와 구역 전체를 가리킬 때가 다르다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "10.0"))
	float RadiusCm = 300.f;

	// 이 마커를 부를 실제 이름. AnchorId 가 비어 있으면 액터 이름을 쓴다.
	FName ResolveAnchorId() const
	{
		return AnchorId.IsNone() ? FName(*GetName()) : AnchorId;
	}

private:
	UPROPERTY(VisibleAnywhere, Category = "Tutorial")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	// 에디터에서 눈에 보이게. 게임에는 나오지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "Tutorial")
	TObjectPtr<UBillboardComponent> Icon;
#endif
};
