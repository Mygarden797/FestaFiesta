// FF_FFZSFacilityDefinition.h

// 시설 사양을 Primary Data Asset으로 제작·검증하는 데이터 전용 경계다.
// 흐름: 에디터에서 Spec을 편집·검증하고 설치 시 시설 액터가 값을 복사해 저장 독립성을 확보한다.
// 불변식: 유효한 ID의 Primary Asset Type은 FFZSFacilityDefinition이며 런타임 저장은 로드된 에셋에 의존하지 않는다.
// 실패/성능: 잘못된 사양은 설치 전에 거부되고 비주얼은 소프트 참조라 에셋 로드를 강제하지 않는다.
 
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSFacilityDefinition.generated.h"

//시설 담당자가 Content Browser에서 만드는 데이터 전용 Primary Data Asset.
// 런타임 액터는 Spec을 값으로 복사하므로 저장 게임이 이 에셋의 로드 상태에 의존하지 않는다.
 
UCLASS(BlueprintType)
class FF_ZONESYSTEM_API UFFZSFacilityDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FF Zone System")
    FFFZSFacilitySpec Spec;
    // ID·크기·대기열·앵커·외부 키·소프트 메시를 포함하는 설치 시점 복사 원본.

    UFUNCTION(BlueprintPure, Category = "FF Zone System")
    bool IsDefinitionValid(TArray<FText>& OutErrors) const;
    // Spec을 변경하지 않고 모든 검증 오류를 OutErrors 뒤에 추가한다. 오류가 없으면 true다. 
    
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    // 유효한 정의 ID면 FFZSFacilityDefinition:FacilityDefinitionId를 반환하고, 없으면 부모 규칙을 사용.

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
    // Content Browser 데이터 검증에 Spec 오류를 전달한다. 런타임 빌드에는 포함되지 않는다. 
#endif
};
