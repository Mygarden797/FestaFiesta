// FFZADistrict.h
// 역할: 구역(District) — Zone과 별개로 "이 땅에는 이런 시설만"이라는 정책을 덧씌우는 레이어.
//
// 예: 해변 산책로 구역에는 Booth/PhotoSpot만, 주차장 구역에는 Toilet/Info만.
// Zone(NPC 소속·혼잡도용)을 쪼개지 않고도 건축 규칙을 세밀하게 줄 수 있다.
// 현업 도시 건설 게임의 "District 정책"과 같은 개념이다.
//
// 동작 원리: 시설 영역과 겹치는 모든 구역이 그 시설 종류를 허용해야 설치된다.
#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FFZAPolygonAreaActor.h"
#include "FFZADistrict.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "FF District (Build Policy)"))
class FF_ZONEAUTHORING_API AFFZADistrict : public AFFZAPolygonAreaActor
{
    GENERATED_BODY()

public:
    AFFZADistrict();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District", meta = (
        ToolTip = "켜면 모든 시설 종류를 허용합니다(색 구분/이름 표시용 구역)."))
    bool bAllowAllFacilityCategories = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District", meta = (
        EditCondition = "!bAllowAllFacilityCategories",
        ToolTip = "이 구역에 지을 수 있는 시설 종류입니다."))
    TArray<EFFZSFacilityCategory> AllowedFacilityCategories;

    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|District")
    bool IsCategoryAllowed(EFFZSFacilityCategory Category) const;

    virtual bool ValidateAreaData(TArray<FText>& OutErrors) const override;

protected:
    virtual void RegisterWithSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem) override;
    virtual void UnregisterFromSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem) override;
};
