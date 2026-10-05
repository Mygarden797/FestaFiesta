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
class UHorizontalBox;
class UPanelWidget;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFF_OnIncidentDispatchRequested,
	FFF_IncidentHandle, Handle, int32, OptionIndex, int32, StaffCount);

/**
 * 사건 아이콘을 눌렀을 때 뜨는 상세 패널. WBP 없이 C++ 만으로 트리를 만든다.
 *
 * 허브의 FFF_IncidentView 만 읽어 제목·본문과 첫 번째 유효 선택지의 비용·작업량·보상을 보여 주고,
 * 작업 인원을 MinStaff 이상, 지금 출동 가능한 스태프 수 이하에서 고르게 하고 예상 작업 시간(작업량 ÷ 인원)을 보여 준다.
 * 가능 인원은 StaffFacility 가 알려 주고, 출동·복귀로 바뀌면(OnStaffingChanged) 다시 계산한다.
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

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	FVector2D CardOffset = FVector2D(-24.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float CardWidth = 320.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	FLinearColor CardColor = FLinearColor(0.04f, 0.05f, 0.07f, 0.92f);

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
	UTextBlock* MakeText(UPanelWidget& Parent, FName Name, int32 FontSize);
	UButton* MakeButton(UPanelWidget& Parent, FName Name, const FText& Label, UTextBlock*& OutLabel);

	UFF_IncidentSubsystem* GetIncidentSubsystem() const;
	void BindIncidents();
	void UnbindIncidents();

	bool Refresh();
	void RefreshStaff();
	int32 GetDispatchableStaff() const;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Card;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Body;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Box_Option;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_OptionLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_StaffRange;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_WorkAmount;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Cost;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Reward;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Row_Staff;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_Minus;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_Plus;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_StaffCount;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_Send;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_SendLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(Transient)
	TObjectPtr<UFF_IncidentSubsystem> CachedIncident;

	UPROPERTY(Transient)
	TObjectPtr<UFF_StaffFacilitySubsystem> CachedStaffing;

	FFF_IncidentHandle CurrentHandle;
	FFF_IncidentOptionView CurrentOption;
	int32 SelectedStaff = 0;
};
