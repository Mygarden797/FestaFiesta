// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_ConfirmDialog.generated.h"

class UFF_CommonButtonBase;
class UFF_StatCard;
class UTextBlock;
class UVerticalBox;

/** 확인창에 띄우는 수치 한 줄. `비용 8,000` · `구매 후 잔액 2,600` 같은 것. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ConfirmDialogRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confirm")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confirm")
	FText Value;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnConfirmDialogResolved);

/**
 * **되돌릴 수 없는 행동 전용** 확인창.
 *
 * 🔴 **아직 실사용처가 없다** (2026-08-27 확인). 타이틀의 `게임 종료` 는 이 창을 거치지 않고
 * `QuitGame` 을 바로 부른다 — 예전 주석의 `지금 쓰는 곳 — 게임 종료` 는 낡은 기록이었다.
 * **첫 사용처는 피날레 구매**다 (축제당 1회, 되돌릴 수 없음).
 * 되돌릴 수 있는 행동에는 쓰지 않는다. 확인창이 흔해지면 아무도 읽지 않게 된다.
 *
 * === 기본 포커스는 항상 `취소` 다 ===
 * `NativeGetDesiredFocusTarget()` 을 오버라이드해 **코드로** 강제한다.
 * 베이스의 `InitialFocusTarget` 바인딩에 맡기면 WBP 에서 위젯 이름을 그렇게 지어야 하고,
 * 누가 이름을 바꾸면 조용히 `확인` 에 포커스가 간다. 되돌릴 수 없는 행동에서 그건 사고다.
 *
 * === 자기를 닫는다 — `UFF_PanelFrame` 과 반대다 ===
 * 준비 단계 패널은 **한 번에 하나만 열리는 규칙을 `UFF_PrepHudBar` 가 소유**하므로 스스로 닫지 않는다.
 * 확인창은 결과가 하나뿐이고 "지금 어느 확인창이 열려 있나" 를 추적하는 주인이 없다.
 * 그래서 결과를 알린 뒤 자기를 비활성화한다. 호출부가 닫는 것을 잊어 창이 남는 쪽이 더 나쁘다.
 *
 * === 수치 행은 `UFF_StatCard` 를 재사용한다 ===
 * 라벨 + 수치는 이미 그 위젯의 모양이다. 확인창 전용 행 클래스를 새로 만들지 않는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Confirm Dialog"))
class FESTAFIESTA_API UFF_ConfirmDialog : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	/**
	 * 내용을 채운다. **`PushPanel` 로 인스턴스를 받은 직후에 부른다.**
	 *
	 * 인스턴스를 얻는 창구가 `PushPanel` 의 반환값뿐이라 물리적으로 Push 이후가 된다.
	 * 활성화가 먼저 돌아도 안전하다 — `NativeGetDesiredFocusTarget()` 은 `Button_Cancel`
	 * 포인터를 직접 보므로 내용이 아직 비어 있어도 포커스는 취소에 간다.
	 *
	 * @param InTitle        무엇을 확인하는지. `게임을 종료할까요?`
	 * @param InBody         되돌릴 수 없다는 사실과 그 결과. 비우지 않는다
	 * @param InRows         수치 표. 비어 있으면 표 영역을 숨긴다
	 * @param InConfirmLabel 확인 버튼 문구. 비우면 `확인`
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void Configure(
		const FText& InTitle,
		const FText& InBody,
		const TArray<FFF_ConfirmDialogRow>& InRows,
		const FText& InConfirmLabel);

	/** 확인을 눌렀을 때. 실제 행동은 구독하는 쪽이 한다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnConfirmDialogResolved OnConfirmed;

	/** 취소했을 때. 뒤로 가기·ESC 로 닫아도 이쪽이다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnConfirmDialogResolved OnCancelled;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** **항상 `취소` 다.** 되돌릴 수 없는 행동에서 기본 포커스가 `확인` 이면 사고가 난다. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION()
	void HandleButtonClicked(UFF_CommonButtonBase* Button);

	// --- 위젯 바인딩 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Body;

	/** 수치 행이 들어가는 곳. 행이 없으면 이 컨테이너를 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UVerticalBox> VerticalBox_Rows;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Cancel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Confirm;

	/** 수치 행에 쓸 위젯. 보통 `WBP_FF_StatCard` 다. 비어 있으면 표를 그리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI")
	TSubclassOf<UFF_StatCard> RowWidgetClass;

private:
	/**
	 * 한 번 띄운 동안 두 번 눌리는 것을 막는다. 되돌릴 수 없는 행동이 두 번 실행되면 안 된다.
	 *
	 * **수명은 창 한 번이다.** 풀에서 재사용되는 인스턴스라 `NativeConstruct` 가 매번 리셋한다.
	 */
	bool bResolved = false;
};
