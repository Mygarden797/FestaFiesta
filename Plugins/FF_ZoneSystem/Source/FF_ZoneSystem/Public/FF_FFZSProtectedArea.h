// FF_FFZSProtectedArea.h

//설치 금지 영역과 일반·긴급·접근성 동선의 배치 충돌 경계를 명시하는 액터다.
//흐름: BeginPlay에 월드 서브시스템에 등록되고 배치 검증이 시설 월드 박스와의 교차를 질의한다.
// 불변식: 0 폭은 유형별 Project Settings 값을 상속하며 명시적 양수는 해당 액터의 고정 오버라이드다.
// 실패/성능: 이 규칙은 게임 배치용 프로토타입이지 법적 안전 인증이 아니며 교차 검사는 등록 영역 수에 비례한다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSProtectedArea.generated.h"

class UBoxComponent;

//명시적 설치 금지·보호 동선 볼륨.
// 폭은 게임플레이 프로토타입 규칙이며 법규 검토나 현장 안전 인증을 대신하지 않는다.
 
UCLASS(Blueprintable)
class FF_ZONESYSTEM_API AFFZSProtectedArea : public AActor
{
    GENERATED_BODY()

public:
    AFFZSProtectedArea();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Protected Area")
    TObjectPtr<UBoxComponent> AreaBounds;
    // 배치 충돌에 사용하는 월드 정렬 박스를 제공하는 루트 볼륨이다. 스케일·회전 제작 규칙은 검증 대상이다. 

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Protected Area")
    FName ProtectedAreaId = NAME_None;
    // 로그·검증에서 영역을 식별하는 비어 있지 않은 안정 ID다. 

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Protected Area")
    FName LinkedZoneId = NAME_None;
    // 이 영역이 적용되는 Gameplay Zone ID. 비어 있으면 데이터 검증이 실패한다. 
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Protected Area")
    EFFZSProtectedAreaType AreaType = EFFZSProtectedAreaType::NoBuild;
    // 금지 영역 또는 보호할 동선·관람 영역의 의미다.
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Protected Area", meta = (ClampMin = "0.0", Units = "cm"))
    float RequiredClearWidthCm = 0.0f;
    // 양수는 액터 고정 오버라이드이고 0은 AreaType별 현재 Project Settings 폭을 동적으로 상속한다. 

    UFUNCTION(BlueprintPure, Category = "Protected Area|Rules")
    float GetEffectiveRequiredClearWidthCm() const;
    // 명시적 폭 또는 유형별 현재 설정값을 반환한다. 읽기 전용이며 속성을 변경하지 않는다. 
    
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Protected Area|Rules")
    void ApplyDefaultWidthForType();
    // 현재 유형별 설정값을 RequiredClearWidthCm에 복사해 이 액터의 폭으로 고정한다. 이후 설정 변경을 상속하지 않는다. 

    UFUNCTION(BlueprintPure, Category = "Protected Area")
    bool IntersectsWorldBox(const FBox& WorldBox) const;
    // 주어진 시설 월드 박스가 보호 영역의 유효 폭을 포함한 경계와 겹치는지 반환한다. 
    
    UFUNCTION(BlueprintPure, Category = "Protected Area|Validation")
    bool ValidateProtectedAreaData(TArray<FText>& OutErrors) const;
    // ID·존·Transform·폭의 모든 제작 오류를 OutErrors에 누적하고 오류가 없을 때 true를 반환한다. 
    
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Protected Area|Presets")
    void ApplyAccessibleFireworksViewingPreset();
    // 접근 가능한 불꽃 관람 구역 프로토타입 값을 편집기에서 적용한다. 적용 뒤 현장 조건을 다시 검토해야 한다. 

protected:
    virtual void BeginPlay() override;
    // 지원 월드에서 권한 서브시스템에 자신을 등록한다. 
    
    
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // 파괴·레벨 전환 시 등록을 해제해 약한 참조와 검증 범위를 정리한다. 
};
