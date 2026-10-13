// 역할: 국가 지도 입력 검사에서 실제 동적 요청 델리게이트의 호출 횟수를 관찰한다.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FF_AtlasCountryInputTestObserver.generated.h"

UCLASS()
class UFF_AtlasCountryInputTestObserver final : public UObject
{
	GENERATED_BODY()

public:
	int32 StartRequests = 0;
	int32 BackRequests = 0;
	FName LastFestivalId;

	UFUNCTION()
	void HandleStart(FName FestivalId)
	{
		++StartRequests;
		LastFestivalId = FestivalId;
	}

	UFUNCTION()
	void HandleBack() { ++BackRequests; }
};
