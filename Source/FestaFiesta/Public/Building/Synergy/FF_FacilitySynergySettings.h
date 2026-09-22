// FF_FacilitySynergySettings.h
// 역할: 조합 데이터표와 시설당 보너스 상한을 에디터에서 설정한다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_FacilitySynergySettings.generated.h"

class UDataTable;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="FestaFiesta 시설 조합"))
class FESTAFIESTA_API UFF_FacilitySynergySettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UFF_FacilitySynergySettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	UFUNCTION() static TArray<FString> GetFacilityIdOptions();
	UPROPERTY(EditAnywhere, Config, Category="조합", meta=(DisplayName="시설 조합 데이터표", RequiredAssetDataTags="RowStructure=/Script/FestaFiesta.FF_FacilitySynergyRow", ToolTip="FF_FacilitySynergyRow 형식의 데이터표를 선택합니다. 플레이 시작 시 한 번 읽습니다."))
	TSoftObjectPtr<UDataTable> SynergyTable;
	UPROPERTY(EditAnywhere, Config, Category="조합", meta=(DisplayName="시설당 추가 수익 상한 (%)", ClampMin="0", ClampMax="1000", ToolTip="한 시설에 여러 조합이 성립해도 이 비율을 넘지 않습니다. 50이면 최대 +50%입니다. 0이면 효과를 끕니다."))
	float MaxTotalBonusPercent = 50.f;
};
