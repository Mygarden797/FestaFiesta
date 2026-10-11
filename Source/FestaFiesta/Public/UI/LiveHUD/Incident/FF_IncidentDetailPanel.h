// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Incident/FF_IncidentTypes.h"
#include "FF_IncidentDetailPanel.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UFF_IncidentSubsystem;
class UFF_StaffFacilitySubsystem;
class UFont;
class UHorizontalBox;
class UPanelWidget;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFF_OnIncidentDispatchRequested,
	FFF_IncidentHandle, Handle, int32, OptionIndex, int32, StaffCount);

/**
 * 사건 아이콘을 눌렀을 때 뜨는 상세 패널. WBP 없이 C++ 만으로 트리를 만든다.
 *
 * 허브의 FFF_IncidentView 만 읽어 제목·본문과 첫 번째 유효 선택지의 작업량·보상을 보여 주고,
 * 작업 인원을 MinStaff 이상, 지금 출동 가능한 스태프 수 이하에서 고르게 하고 예상 작업 시간(작업량 ÷ 인원)을 보여 준다.
 * 가능 인원은 StaffFacility 가 알려 주고, 출동·복귀로 바뀌면(OnStaffingChanged) 다시 계산한다.
 * CostGold·CostStaff 는 현재 출동 경로에서 차감하지 않으므로 비용으로 표시하지 않는다.
 *
 * 출동 중(View.ResponseStage != None)이면 인원 고르기 줄을 숨기고 상태 줄(현장으로 가는 중 / 현장 작업 중 · 남은 시간)을 보여 주며
 * [보내기]를 끈다. 틱에서 다시 읽지 않는다 — 출동 담당(UFF_StaffDispatchSubsystem)이 단계가 바뀌거나 남은 시간이
 * 표시 단위(분)로 바뀔 때 OnIncidentUpdated 를 방송하고, 패널은 그 방송에만 다시 읽는다 (UI_API.md 원칙 3).
 * [보내기]가 꺼진 이유가 상태 줄로 드러나지 않으면(가용 인원 부족) 버튼 아래에 이유 한 줄을 적는다 (AT-011: 흐려짐만으로는 이유를 알 수 없다).
 *
 * 모양은 UFF_UIPalette(LightTeal)와 Pretendard 를 따른다. 패널 모서리 8px · 버튼 6px · 상태 칩 4px, 제목 20px,
 * 버튼은 44px 이상 크기 상자(Button_*_Size)로 감싼다. 기준은 FFIncidentUiStyle 주석 참고.
 *
 * [보내기]는 OnDispatchRequested 만 방송한다. 패널은 허브도 배치 관리자도 부르지 않는다.
 * 선택지가 없는 사건은 [확인]으로 바뀌고 OptionIndex=INDEX_NONE, StaffCount=0 으로 방송한다.
 *
 * 열린 사건이 Updated 되면 다시 읽고 고른 인원은 새 범위로 Clamp 한다. Closed 되면 스스로 닫힌다.
 * 닫혀 있을 때는 Collapsed, 열려 있을 때도 루트는 SelfHitTestInvisible 이라 카드 밖 월드 클릭을 막지 않는다.
 */
UCLASS(meta = (DisplayName = "FF Incident Detail Panel"))
class FESTAFIESTA_API UFF_IncidentDetailPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Incident")
	void OpenFor(FFF_IncidentHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Incident")
	void Close();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	bool IsOpen() const { return CurrentHandle.IsSet(); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	int32 GetSelectedStaff() const { return SelectedStaff; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Incident")
	FFF_OnIncidentDispatchRequested OnDispatchRequested;

	/** 상태 줄 문구. None 이면 빈 문구. 남은 시간은 축제 분을 올림해 적는다. */
	static FText BuildResponseStatusText(EFF_IncidentResponseStage Stage, float RemainingWorkMinutes);

	/** [보내기]가 꺼진 이유 중 상태 줄로 드러나지 않는 것. 막히지 않았거나 출동 중이면 빈 문구. */
	static FText BuildSendBlockedReason(EFF_IncidentResponseStage Stage, bool bHasOption, int32 Available, int32 MinStaff);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	FVector2D CardOffset = FVector2D(-24.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float CardWidth = 320.f;

private:
	UFUNCTION()
	void HandleIncidentUpdated(FFF_IncidentHandle Handle);

	UFUNCTION()
	void HandleIncidentClosed(FFF_IncidentHandle Handle);

	UFUNCTION()
	void HandleStaffingChanged();

	UFUNCTION()
	void HandleMinusClicked();

	UFUNCTION()
	void HandlePlusClicked();

	UFUNCTION()
	void HandleSendClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void BuildTree();
	UTextBlock* MakeText(UPanelWidget& Parent, FName Name, const TCHAR* Typeface, int32 FontSize, const FLinearColor& Color);
	UButton* MakeButton(UPanelWidget& Parent, FName Name, const FText& Label, bool bPrimary, UTextBlock*& OutLabel);
	void SetButtonEnabled(UButton* Button, UTextBlock* Label, bool bPrimary, bool bEnabled);

	UFF_IncidentSubsystem* GetIncidentSubsystem() const;
	void BindIncidents();
	void UnbindIncidents();

	bool Refresh();
	void RefreshStaff();
	int32 GetDispatchableStaff() const;
	bool IsResponding() const { return ResponseStage != EFF_IncidentResponseStage::None; }

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Card;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Body;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Border_Status;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Box_Option;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_OptionLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_StaffRange;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_WorkAmount;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Reward;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Row_Staff;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_Minus;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label_Minus;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_Plus;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label_Plus;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_StaffCount;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_Send;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_SendLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Reason;

	UPROPERTY(Transient)
	TObjectPtr<UFont> UiFont;

	UPROPERTY(Transient)
	TObjectPtr<UFF_IncidentSubsystem> CachedIncident;

	UPROPERTY(Transient)
	TObjectPtr<UFF_StaffFacilitySubsystem> CachedStaffing;

	FFF_IncidentHandle CurrentHandle;
	FFF_IncidentOptionView CurrentOption;
	int32 SelectedStaff = 0;
	EFF_IncidentResponseStage ResponseStage = EFF_IncidentResponseStage::None;
};
