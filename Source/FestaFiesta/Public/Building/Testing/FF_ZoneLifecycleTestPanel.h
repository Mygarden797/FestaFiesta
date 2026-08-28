// FF_ZoneLifecycleTestPanel.h

// Test_Main에서 Festival 단계와 시설 개장·폐쇄 권위 API를 관찰·수동 실행하는 개발 패널을 선언한다.
// 흐름: WBP 버튼 바인딩 -> 단계/Zone 이벤트 구독 -> 배치 명령 실행 -> 상태 요약 갱신.
// 불변식: 제품 입력 모드는 바꾸지 않고 실제 Stage/Zone 공개 API만 호출한다.
// 실패/수명: 선택적 서브시스템이나 BindWidget이 없으면 동작을 건너뛰고 Destruct에서 구독을 해제한다.

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

