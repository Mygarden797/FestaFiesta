#pragma once

/**
 * @file FF_FFZSPhase0Bridge.h
 * @brief Phase 0 병렬 개발을 위한 비권위 월드 변경 스텁 계약이다.
 *
 * 이 API는 노션 일정의 "항상 성공 + 고정 슬롯 3개" 통합 계약만 제공한다.
 * 실제 월드 상태, 비용, 정책, Revision은 변경하지 않으며 운영 코드는
 * UFFZSWorldSubsystem::ExecuteCommand를 사용해야 한다.
 */

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSPhase0Bridge.generated.h"

/** Phase 0 스텁의 고정 응답. 성공은 요청 수락만 뜻하며 월드 적용 성공을 뜻하지 않는다. */
USTRUCT(BlueprintType)
struct FF_ZONESYSTEM_API FFFZSPhase0WorldChangeResponse
{
    GENERATED_BODY()

    /** Phase 0에서는 입력과 무관하게 true다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Phase 0")
    bool bSucceeded = false;

    /** 스텁은 권위 월드를 변경하지 않으므로 항상 false다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Phase 0")
    bool bWorldStateChanged = false;

    /**
     * 60 m x 40 m 코어 Zone의 좌하단을 원점으로 한 로컬 cm 좌표다.
     * Phase 0 고정 순서: A(10 m, 20 m), B(30 m, 20 m), C(50 m, 20 m).
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Phase 0")
    TArray<FVector> SlotCoordinatesCm;

    /** 호출자가 스텁과 실제 적용 결과를 혼동하지 않도록 하는 진단 문구다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Phase 0")
    FText Message;
};

/** Phase 0 동안 하위 모듈의 병렬 개발을 막지 않는 개발 전용 façade다. */
UCLASS()
class FF_ZONESYSTEM_API UFFZSPhase0Bridge : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 입력을 검증하거나 실행하지 않고 성공 응답과 고정 슬롯 3개를 반환한다.
     * @warning 비용·Actor·점유·Policy·WorldRevision을 전혀 변경하지 않는다.
     *          실제 통합은 UFFZSWorldSubsystem::ExecuteCommand로 교체해야 한다.
     */
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Phase 0",
        meta = (DevelopmentOnly, DisplayName = "Request World Change (Phase 0 Stub)"))
    static FFFZSPhase0WorldChangeResponse RequestWorldChangePhase0(const FFFZSCommand& Request);

    /** 팀 회의에서 좌표를 바꿀 때 단일 구현 지점을 제공하는 결정적 값 복사본이다. */
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Phase 0")
    static TArray<FVector> GetPhase0SlotCoordinatesCm();
};
