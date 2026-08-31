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
class UBorder;
class UCommonTextBlock;
class UFF_FestivalProgressionSubsystem;

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

	/** 표시 중인 카드 위에 포인터가 있어 raw 월드 클릭을 막아야 하는지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Testing")
	bool IsPointerOverInteractivePanel() const;

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

	/** Debug rewind: changes only the Stage timeline; irreversible economy state is preserved. */
	UFUNCTION()
	void HandlePreviousDayClicked();

	/** Debug fast-forward: uses the real Report -> AdvanceToNextDay authority path. */
	UFUNCTION()
	void HandleNextDayClicked();

	UFUNCTION()
	void HandleRemoveGoldClicked();

	UFUNCTION()
	void HandleAddGoldClicked();

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
	void HandleGoldChanged(int32 NewGold);

	UFUNCTION()
	void HandleZoneWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleLifecycleBatchCompleted(const FFF_ZoneFacilityLifecycleBatchResult& Result);

	void RequestStage(EFF_FestivalStage Stage);
	void ApplyBatchResult(const FFF_ZoneFacilityLifecycleBatchResult& Result);
	void SetLastResult(const FText& Text);
	void RefreshGoldControls();
	UFF_FestivalProgressionSubsystem* GetProgressionSubsystem() const;
	int32 GetGoldAdjustmentStep() const;

	// Live rewind schedules one redundant empty AutomaticClose on the next Tick.
	// Suppress only that no-op result so the rewind outcome remains visible.
	bool bSuppressNextEmptyAutomaticCloseResult = false;

	// 새 LightTeal WBP의 상호작용 경계. 구 에셋도 로드할 수 있도록 refresh 전에는 선택 계약이다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_Panel;

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
	TObjectPtr<UCommonTextBlock> TXT_Gold;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_BTN_RemoveGold;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_BTN_AddGold;

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
	TObjectPtr<UButton> BTN_PreviousDay;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_NextDay;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_RemoveGold;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_AddGold;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_OpenAll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_CloseAll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_EmergencyCloseAll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Refresh;

	/** 한 번의 디버그 조작으로 증감할 Gold. 파생 WBP 기본값에서 조정할 수 있습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Building|Testing",
		meta = (ClampMin = "1", UIMin = "1", AllowPrivateAccess = "true"))
	int32 GoldAdjustmentAmount = 1000;
};

