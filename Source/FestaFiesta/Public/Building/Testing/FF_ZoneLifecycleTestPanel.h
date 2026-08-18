// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStage.h"
#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"
#include "FF_ZoneLifecycleTestPanel.generated.h"

class UButton;
class UCommonTextBlock;

/**
 * Test_Main에서 시설 자동 개장/폐쇄를 관찰하고 같은 권위 API를 수동 실행하는 개발용 패널입니다.
 * 레이아웃은 /Game/FestaFiesta/Building/Debug의 파생 WBP가 소유하며 입력 모드는 변경하지 않습니다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Zone Lifecycle Test Panel"))
class FESTAFIESTA_API UFF_ZoneLifecycleTestPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Testing")
	void RefreshStatus();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleSetupClicked();

	UFUNCTION()
	void HandleMorningCheckClicked();

	UFUNCTION()
	void HandleLiveClicked();

	UFUNCTION()
	void HandleClosingClicked();

	UFUNCTION()
	void HandleOpenAllClicked();

	UFUNCTION()
	void HandleCloseAllClicked();

	UFUNCTION()
	void HandleEmergencyCloseAllClicked();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	UFUNCTION()
	void HandleZoneWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleLifecycleBatchCompleted(const FFF_ZoneFacilityLifecycleBatchResult& Result);

	void RequestStage(EFF_FestivalStage Stage);
	void ApplyBatchResult(const FFF_ZoneFacilityLifecycleBatchResult& Result);
	void SetLastResult(const FText& Text);

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_Stage;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_PhaseRevision;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_FacilityCounts;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_QueueCounts;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_LastResult;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Setup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_MorningCheck;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Live;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Closing;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_OpenAll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_CloseAll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_EmergencyCloseAll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Refresh;
};

