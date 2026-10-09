// FFZAPlot.h
// 역할: 구획(Plot) — "아직 잠겨 있어 지을 수 없는 땅"을 표시하는 레이어.
//
// 현업 건축 시뮬레이션의 "토지 구매/확장" 방식이다. 처음엔 잠겨 있고(회색으로 표시),
// 게임 진행(돈 지불, 퀘스트 보상 등)으로 UnlockPlot을 호출하면 그 땅에 지을 수 있게 된다.
//
// 동작 원리: Zone 안이라도, 잠긴 구획과 시설 영역이 조금이라도(면적 > 0) 겹치면 설치를 거부한다.
// 거부는 FF_ZoneSystem의 공식 확장 지점(External Policy)으로 처리하므로 기존 코드를 고치지 않는다.
// 잠금/해제 상태는 UFFZAZoneAuthoringSubsystem이 보관한다(저장 시스템은 그 함수를 호출하면 된다).
#pragma once

#include "CoreMinimal.h"
#include "FFZAPolygonAreaActor.h"
#include "FFZAPlot.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "FF Plot (Locked Land)"))
class FF_ZONEAUTHORING_API AFFZAPlot : public AFFZAPolygonAreaActor
{
    GENERATED_BODY()

public:
    AFFZAPlot();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plot", meta = (
        ToolTip = "켜면 게임 시작부터 해제된 상태입니다(지을 수 있음). 끄면 잠긴 채 시작합니다."))
    bool bStartsUnlocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plot", meta = (ClampMin = "0",
        ToolTip = "해제 비용(기획 데이터). 실제 돈 차감은 경제 시스템이 UnlockPlot 호출 전에 처리합니다."))
    int32 UnlockCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plot", meta = (
        ToolTip = "잠긴 구획 오버레이 색입니다."))
    FLinearColor LockedColor = FLinearColor(0.25f, 0.25f, 0.28f, 1.0f);

    // 현재 해제 여부(서브시스템 상태 기준).
    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|Plot")
    bool IsUnlocked() const;

    virtual FLinearColor GetOverlayColor() const override;
    virtual bool ShouldDrawOverlay() const override;

protected:
    virtual void RegisterWithSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem) override;
    virtual void UnregisterFromSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem) override;
};
