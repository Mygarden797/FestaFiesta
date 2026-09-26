// FF_FFZSFacilityAnchorComponent.h

// 시설의 입구·서비스·출구 위치를 Type과 Index로 식별하는 수동 SceneComponent다.
// 흐름: 시설 액터가 사양의 앵커 정의를 런타임 컴포넌트로 만들고 조회자가 월드 변환을 얻는다.
// 불변식: 한 시설 안에서 Type과 Index 조합은 유일해야 하며 컴포넌트가 이동이나 내비게이션을 직접 수행하지 않는다.
// 실패/성능: 잘못된 조합은 사양 검증에서 거부되고, 컴포넌트는 Tick과 별도 할당 작업을 만들지 않는다.
// 생성·부착·좌표 갱신은 시설 액터가 담당하고 타입 연결은 UHT가 생성하므로 별도 .cpp는 필요 없다.
 

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSFacilityAnchorComponent.generated.h"

// 시설 사양의 논리 앵커를 월드 Transform으로 노출하는 수동 마커 컴포넌트.
// 시설 액터가 생성·소유하며 별도 Tick이나 권한 변경 기능이 없다.
 
UCLASS(ClassGroup = (FFZoneSystem), meta = (BlueprintSpawnableComponent))
class FF_ZONESYSTEM_API UFFZSFacilityAnchorComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor")
    EFFZSAnchorType AnchorType = EFFZSAnchorType::Entry;
    // Entry·Service·Exit 중 앵커의 역할.
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor", meta = (ClampMin = "0"))
    int32 AnchorIndex = 0;
    // 같은 AnchorType 안에서의 0 기반 안정 인덱스다. 사양 검증이 중복 조합을 거부.
};
