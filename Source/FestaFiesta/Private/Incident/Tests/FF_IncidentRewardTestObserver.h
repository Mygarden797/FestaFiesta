#pragma once

#include "CoreMinimal.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "FF_IncidentRewardTestObserver.generated.h"

// 지급 알림 중 재진입과 종료 시 저장 원장을 검사하는 관찰자.
UCLASS()
class UFF_IncidentRewardTestObserver final : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UFF_FestivalProgressionSubsystem> Progression;
	UPROPERTY() TObjectPtr<UFF_IncidentSubsystem> Incident;
	FFF_IncidentHandle CurrentHandle;
	FFF_IncidentClosedInfo LastInfo;
	TArray<uint8> SavedAtFinished;
	int32 FinishedCount = 0;
	bool bRunNestedRevenue = false;
	bool bHandleValidDuringPayment = true;

	UFUNCTION()
	void HandleGold(int32 Value)
	{
		if (bRunNestedRevenue && Progression && Incident)
		{
			bRunNestedRevenue = false;
			bHandleValidDuringPayment = Incident->IsHandleValid(CurrentHandle);
			// 다른 수입을 같은 콜스택에서 발생시켜도 사건의 지급 영수증에 섞이면 안 된다.
			Progression->AddRevenue(7, EFF_EarnReason::IncidentReward);
		}
	}

	UFUNCTION()
	void HandleFinished(const FFF_IncidentClosedInfo& Info)
	{
		LastInfo = Info;
		++FinishedCount;
		if (Progression) Progression->WriteSaveData(SavedAtFinished);
	}
};
