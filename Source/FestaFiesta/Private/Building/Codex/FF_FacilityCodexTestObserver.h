// 역할: 도감 변경 알림 횟수를 세어 자동화 검사에서 알림 누락이나 중복을 확인한다.
#pragma once

#include "CoreMinimal.h"
#include "FF_FacilityCodexTestObserver.generated.h"

/** 도감의 생성·갱신·종료 과정에서 공개 알림을 받는 검사 전용 관찰자다. */
UCLASS()
class UFF_FacilityCodexTestObserver final : public UObject
{
	GENERATED_BODY()
public:
	int32 NotificationCount = 0;

	UFUNCTION()
	void HandleCodexChanged() { ++NotificationCount; }
};
