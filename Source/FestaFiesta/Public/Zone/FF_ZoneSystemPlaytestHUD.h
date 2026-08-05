#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Zone/FF_ZoneSystemPlaytestTypes.h"
#include "FF_ZoneSystemPlaytestHUD.generated.h"

class UButton;
class UTextBlock;

/**
 * WBP_ZoneSystemPlaytestHUD의 동작만 담당하는 네이티브 부모 클래스다.
 * 배치와 스타일은 Blueprint Designer가 소유하며, 선택적 위젯 이름을 유지하면
 * 상태 표시와 버튼 동작이 C++에 자동으로 연결된다.
 */
UCLASS(Blueprintable)
class FESTAFIESTA_API UFF_ZoneSystemPlaytestHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Blueprint가 현재 HUD 표시 상태를 읽을 때 사용한다. */
    UFUNCTION(BlueprintPure, Category = "Zone Playtest")
    const FFF_ZoneSystemPlaytestHudState& GetHudState() const { return HudState; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FFF_ZoneSystemPlaytestHudState HudState;

    /** 디자이너가 애니메이션이나 추가 표시 로직을 붙일 수 있도록 상태 갱신을 전달한다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Zone Playtest", meta = (DisplayName = "On HUD State Updated"))
    void ReceiveHudStateUpdated(const FFF_ZoneSystemPlaytestHudState& NewState);

private:
    // 아래 이름은 WBP에서 유지해야 자동 바인딩된다. Optional이므로 디자이너가 일부 항목을 생략해도 안전하다.
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Zone;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Preset;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Placement;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Validation;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Status;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Help;

    // 버튼은 이름별로 Zone 플레이테스트 subsystem의 명령 함수에 연결된다.
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_PresetPrevious;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_PresetNext;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Install;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Move;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Remove;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Open;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Close;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Emergency;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_QueueJoin;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_QueueAdvance;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_QueueBlock;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_SnapshotSave;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_SnapshotRestore;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Reset;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Debug;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Help;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_ReturnToGame;

    float RefreshElapsed = 1.0f;

    void RefreshDisplay();
    void ConfigureButton(UButton* Button) const;

    UFUNCTION() void HandlePresetPrevious();
    UFUNCTION() void HandlePresetNext();
    UFUNCTION() void HandleInstall();
    UFUNCTION() void HandleMove();
    UFUNCTION() void HandleRemove();
    UFUNCTION() void HandleOpen();
    UFUNCTION() void HandleClose();
    UFUNCTION() void HandleEmergency();
    UFUNCTION() void HandleQueueJoin();
    UFUNCTION() void HandleQueueAdvance();
    UFUNCTION() void HandleQueueBlock();
    UFUNCTION() void HandleSnapshotSave();
    UFUNCTION() void HandleSnapshotRestore();
    UFUNCTION() void HandleReset();
    UFUNCTION() void HandleDebug();
    UFUNCTION() void HandleHelp();
    UFUNCTION() void HandleReturnToGame();
};

