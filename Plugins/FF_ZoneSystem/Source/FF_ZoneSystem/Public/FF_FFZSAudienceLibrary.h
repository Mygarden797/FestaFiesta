// FF_FFZSAudienceLibrary.h
// 역할: 시설의 관람 범위와 관객 수용량을 게임 코드와 블루프린트에 제공한다.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_FFZSAudienceLayout.h"
#include "FF_FFZSAudienceLibrary.generated.h"

/** 관객 모집 시작 시 보관하는 읽기 전용 사본이다. 시설이 변경되면 새 사본을 받아야 한다. */
USTRUCT(BlueprintType)
struct FF_ZONESYSTEM_API FFFZSAudienceSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Audience")
    FGuid FacilityInstanceId;
    UPROPERTY(BlueprintReadOnly, Category = "Audience")
    int64 WorldRevision = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Audience")
    FTransform FacilityTransform = FTransform::Identity;
    UPROPERTY(BlueprintReadOnly, Category = "Audience")
    FFFZSAudienceLayout Layout;
    UPROPERTY(BlueprintReadOnly, Category = "Audience")
    int32 Capacity = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Audience")
    FVector EntryWorldCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Audience")
    FVector ExitWorldCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category = "Audience")
    FVector FocusWorldCm = FVector::ZeroVector;
};

UCLASS()
class FF_ZONESYSTEM_API UFFZSAudienceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 등록된 시설의 정보를 조회한다. 이름 검색·에셋 즉시 로딩은 하지 않으며 비활성 시설은 실패한다. */
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Audience", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Facility Audience Snapshot"))
    static bool GetFacilityAudienceSnapshot(const UObject* WorldContextObject, FGuid FacilityInstanceId,
        FFFZSAudienceSnapshot& OutSnapshot, FText& OutError);

    /** 앞에서 뒤, 왼쪽에서 오른쪽 순서인 한 사람의 자리 좌표를 즉시 계산한다.
     * 실제 자리 예약이나 이동 경로 보정은 호출한 쪽이 별도로 처리한다. */
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Audience", meta = (DisplayName = "Get Audience Slot Transform"))
    static bool GetAudienceSlotTransform(const FFFZSAudienceSnapshot& Snapshot, int32 SlotIndex,
        FTransform& OutTransform);

    /** 한 번에 최대 64자리의 입장·퇴장 경로를 검사한다. 준비 단계나 관객 모집 전에 호출한다.
     * 매 프레임 모든 NPC에서 실행하지 않는다. 성공해도 이번에 검사하지 않은 경로는 보장하지 않는다. */
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Audience", meta = (WorldContext = "WorldContextObject", DisplayName = "Check Audience Navigation Batch"))
    static bool CheckAudienceNavigationBatch(const UObject* WorldContextObject,
        const FFFZSAudienceSnapshot& Snapshot, int32 FirstSlotIndex, int32 SlotCount,
        TArray<int32>& OutFailedSlots, FText& OutError);

    static bool BuildSnapshot(const FFFZSAudienceLayout& Layout, const FTransform& Transform,
        FFFZSAudienceSnapshot& OutSnapshot, FText& OutError);
};
