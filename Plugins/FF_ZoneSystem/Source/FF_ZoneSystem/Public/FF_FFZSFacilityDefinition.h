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

class UTexture2D;

//시설 담당자가 Content Browser에서 만드는 데이터 전용 Primary Data Asset.
// 런타임 액터는 Spec을 값으로 복사하므로 저장 게임이 이 에셋의 로드 상태에 의존하지 않는다.
 
UCLASS(BlueprintType)
class FF_ZONESYSTEM_API UFFZSFacilityDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** AssetManager, catalog, validators, and adapters share this one type token. */
    static const FPrimaryAssetType& GetFacilityDefinitionAssetType();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FF Zone System", meta = (DisplayName = "Facility Specification", ToolTip = "이 시설의 ID·건물 크기·대기열·앵커·시각 메시를 정의하는 프로덕션 단일 원천입니다. 설치 시 이 값을 검증한 복사본이 월드 인스턴스와 저장 데이터에 들어갑니다.", AssetBundles = "FacilityVisual"))
    FFFZSFacilitySpec Spec;
    // ID·크기·대기열·앵커·외부 키·소프트 메시를 포함하는 설치 시점 복사 원본.

    /** 건설 카탈로그 카드에만 사용하는 선택적 이미지. 설치 사양·SaveGame 데이터에는 포함하지 않는다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FF Zone System|Presentation", meta = (DisplayName = "Catalog Thumbnail", ToolTip = "건설 카탈로그 카드에 표시할 선택적 이미지입니다. 건물 크기·대기열·배치 판정에는 영향을 주지 않습니다.", AssetBundles = "BuildCatalogUI"))
    TSoftObjectPtr<UTexture2D> CatalogThumbnail;

#if WITH_EDITORONLY_DATA
    /** Explicit write-ownership for the procedural mesh import/material tools; never affects runtime size. */
    UPROPERTY(EditAnywhere, Category = "FF Zone System|Authoring", AdvancedDisplay, meta = (DisplayName = "Managed by Procedural Mesh Tools", ToolTip = "켜면 프로젝트의 절차형 FBX 임포트·재질 도구가 이 시설의 VisualMesh를 수정할 권한을 가집니다. 크기 값에는 영향을 주지 않으며 소유권 메타데이터가 일치할 때만 사용하십시오."))
    bool bProcedurallyManagedVisualMesh = false;
#endif

    UFUNCTION(BlueprintPure, Category = "FF Zone System", meta = (DisplayName = "Is Facility Definition Valid", ToolTip = "이 Data Asset의 시설 사양을 변경하지 않고 검사합니다. ID·건물 크기·대기열·앵커·안전 예산 오류를 오류 목록으로 반환합니다."))
    bool IsDefinitionValid(UPARAM(DisplayName = "Errors") TArray<FText>& OutErrors) const;

    /** True only when Spec exactly matches the loaded AssetManager Definition with the same ID. */
    static bool DoesSpecMatchLoadedDefinition(const FFFZSFacilitySpec& Spec);
    // Spec을 변경하지 않고 모든 검증 오류를 OutErrors 뒤에 추가한다. 오류가 없으면 true다. 
    
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    // 유효한 정의 ID면 FFZSFacilityDefinition:FacilityDefinitionId를 반환하고, 없으면 부모 규칙을 사용.

#if WITH_EDITOR
    virtual void PostInitProperties() override;
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
    // Content Browser 데이터 검증에 Spec 오류를 전달한다. 런타임 빌드에는 포함되지 않는다. 
#endif
};
