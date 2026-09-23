// FF_FFZSAudienceLayout.h
// 역할: 관객이 설 자리를 행과 열로 만들고 관람 영역에 맞는 위치와 수용 인원을 계산한다.
#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSAudienceLayout.generated.h"

/** Facility-owned geometry only. No tickets, NPC ownership, clock, or per-seat actors. */
USTRUCT(BlueprintType)
struct FF_ZONESYSTEM_API FFFZSAudienceLayout
{
    GENERATED_BODY()

    static constexpr int32 MaxSlots = 8192;
    static constexpr int32 MaxDimension = 512;
    // Compressed rectangles only. Legacy/debug cell materialization keeps its separate cap.
    static constexpr float MaxAreaLimitM2 = 50000.f;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Enable Audience Area", ToolTip = "관람 공간을 사용합니다. 켜면 공간도 시설 배치 점유에 포함됩니다. NPC 이동과 공연 시간 연결은 별도입니다."))
    bool bEnabled = false;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "First Seat Local Position (cm)", ToolTip = "무대 중심 기준 첫 줄 왼쪽 관람 자리의 발 위치입니다. X는 오른쪽, Y는 관객 쪽(+Y), Z는 높이입니다.", EditCondition = "bEnabled", Units = "cm"))
    FVector FirstSlotLocalCm = FVector::ZeroVector;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Columns", ToolTip = "한 줄의 인원입니다. 확정하면 입력한 행·열을 유지하며 자리·입퇴장·부족한 면적을 자동 보정합니다. 전체 검증과 보정 이유를 표시하며, 해결할 수 없는 오류는 요청 인원을 보관해 수정 후 다시 적용할 수 있습니다.", ClampMin = "1", ClampMax = "512", EditCondition = "bEnabled"))
    int32 Columns = 12;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Rows", ToolTip = "무대 앞에서 뒤쪽(+Y)으로 늘어나는 줄 수입니다. 확정하면 요청 인원을 유지하도록 자동 보정·전체 검증합니다. 부족한 관람 면적은 필요한 최소값으로 늘리고 이유를 표시합니다. 현재 정원 지원 상한은 8192명입니다.", ClampMin = "1", ClampMax = "512", EditCondition = "bEnabled"))
    int32 Rows = 10;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Audience Area Limit (m²)", ToolTip = "관람 공간 전용 허용 면적입니다. 0은 기존 공통 예산, 양수는 별도 관람 한도입니다. 인원 변경에 부족한 값은 모든 사용 레벨에 필요한 최소 m²로 자동 확대하고 이유를 표시합니다. 더 큰 여유값은 직접 지정할 수 있습니다. 무대·일반 Queue 한도와 실제 맵 검사는 유지되며 최대 50,000m²입니다.", ClampMin = "0", ClampMax = "50000", EditCondition = "bEnabled"))
    float AreaLimitM2 = 0.f;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Column Spacing (cm)", ToolTip = "좌우 자리 중심 사이 간격입니다. NPC 반경과 도착 허용 오차를 포함해 겹치지 않는 간격이어야 합니다.", Units = "cm", EditCondition = "bEnabled"))
    float ColumnSpacingCm = 120.f;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Row Spacing (cm)", ToolTip = "앞뒤 자리 중심 사이 간격입니다. 값을 늘리면 필요한 관람 공간도 커집니다.", Units = "cm", EditCondition = "bEnabled"))
    float RowSpacingCm = 120.f;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Additional Center Aisle (cm)", ToolTip = "가운데 두 열 사이에 기존 자리 간격 외에 추가로 확보하는 폭입니다. 0이면 추가 통로가 없습니다.", Units = "cm", EditCondition = "bEnabled"))
    float CenterAisleWidthCm = 200.f;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Perimeter Margin (cm)", ToolTip = "가장자리 자리 중심부터 예약 경계까지의 폭입니다. 입퇴장 위치는 이 가장자리 통로에 놓습니다.", Units = "cm", EditCondition = "bEnabled"))
    float PerimeterMarginCm = 180.f;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "NPC Radius (cm)", ToolTip = "관객 한 명의 보행 충돌 반경입니다. 현재 Crowd 기본 반경은 34cm입니다. 다른 크기의 NPC는 팀과 맞춰 주세요.", Units = "cm", EditCondition = "bEnabled"))
    float AgentRadiusCm = 34.f;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Arrival Tolerance (cm)", ToolTip = "자리 중심에서 이 거리 안에 들어오면 도착으로 판단하도록 NPC 팀에 제공하는 값입니다.", Units = "cm", EditCondition = "bEnabled"))
    float ArrivalToleranceCm = 25.f;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Audience Entry Local Position (cm)", ToolTip = "관람객 입구입니다. 일반 Queue의 Entry와 별개이며 관람 영역 가장자리 통로 안에 놓습니다.", Units = "cm", EditCondition = "bEnabled"))
    FVector EntryLocalCm = FVector::ZeroVector;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Audience Exit Local Position (cm)", ToolTip = "관람객 출구입니다. 입구와 떨어진 가장자리 통로에 놓아 동선이 한곳에 몰리지 않게 합니다.", Units = "cm", EditCondition = "bEnabled"))
    FVector ExitLocalCm = FVector::ZeroVector;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Audience", meta = (DisplayName = "Stage Focus Local Position (cm)", ToolTip = "관객이 바라볼 무대 위 기준점입니다. 보행 목적지가 아니므로 NavMesh 위일 필요는 없습니다.", Units = "cm", EditCondition = "bEnabled"))
    FVector FocusLocalCm = FVector::ZeroVector;

    int32 GetCapacity() const;
    bool IsValid(TArray<FText>& OutErrors) const;
    bool TryGetSlotLocal(int32 SlotIndex, FVector& OutPosition) const;
    bool TryGetLocalBounds(FBox2D& OutBounds) const;
    // Explicit authoring action. Preserves front-row Y/Z and focus, centres X, fits rear portals.
    bool TryCenterAndArrangePortals();
    void ApplySizeMultiplier(float Multiplier);
};
