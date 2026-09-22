// FF_FFZSAudienceLibrary.h
// 역할: 시설의 관람 범위와 관객 수용량을 게임 코드와 블루프린트에 제공한다.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSAudienceLibrary.generated.h"

/** Immutable read contract. Cache once when gathering starts; refresh after facility changes. */
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
    /** Does not find actors by name or synchronously load the PDA. Inactive facilities fail closed. */
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Audience", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Facility Audience Snapshot"))
    static bool GetFacilityAudienceSnapshot(const UObject* WorldContextObject, FGuid FacilityInstanceId,
        FFFZSAudienceSnapshot& OutSnapshot, FText& OutError);

    /** O(1). Slot indices are front-to-back, left-to-right, and count people, not parties.
     * Returns authored foot positions, not NavMesh-projected positions. Never claims a slot. */
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Audience", meta = (DisplayName = "Get Audience Slot Transform"))
    static bool GetAudienceSlotTransform(const FFFZSAudienceSnapshot& Snapshot, int32 SlotIndex,
        FTransform& OutTransform);

    /** Explicit inspection only: at most 64 slots per call. Checks entry->slot and slot->exit.
     * Call every batch at setup/QA or before gathering, never from every NPC's Tick.
     * A successful batch does not certify untested slots or other zones' routes to the entry. */
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Audience", meta = (WorldContext = "WorldContextObject", DisplayName = "Check Audience Navigation Batch"))
    static bool CheckAudienceNavigationBatch(const UObject* WorldContextObject,
        const FFFZSAudienceSnapshot& Snapshot, int32 FirstSlotIndex, int32 SlotCount,
        TArray<int32>& OutFailedSlots, FText& OutError);

    static bool BuildSnapshot(const FFFZSAudienceLayout& Layout, const FTransform& Transform,
        FFFZSAudienceSnapshot& OutSnapshot, FText& OutError);
};
