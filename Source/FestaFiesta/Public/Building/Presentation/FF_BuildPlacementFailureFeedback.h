// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "FF_FFZSTypes.h"
#include "FF_BuildPlacementFailureFeedback.generated.h"

class UCommonTextBlock;

/** 건축 명령 실패를 디자이너 편집형 상단 토스트로 표현하는 안정 UI 분류입니다. */
UENUM(BlueprintType)
enum class EFF_BuildPlacementFailureType : uint8
{
	InsufficientGold,
	PlacementRejected,
	ConfigurationError,
	WorldStateChanged,
	Unknown
};

/**
 * 건축 배치 실패를 알리는 비차단형 UMG 토스트의 네이티브 계약입니다.
 * 제목과 상세 사유만 C++이 제공하며 배경, 위치, 폰트와 애니메이션은 파생 WBP가 소유합니다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Placement Failure Feedback"))
class FESTAFIESTA_API UFF_BuildPlacementFailureFeedback : public UUserWidget
{
	GENERATED_BODY()

public:
	/** FFZS의 안정 실패 코드를 디자이너가 분기할 UI 타입으로 변환합니다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	static EFF_BuildPlacementFailureType ClassifyFailure(
		EFFZSPlacementFailure FailureReason);

	/** 실패 타입과 현재 작업을 기반으로 짧은 토스트 제목을 만듭니다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	static FText MakeFailureTitle(
		EFFZSPlacementFailure FailureReason,
		bool bIsMoving);

	/** 같은 인스턴스를 재사용하고 자동 숨김 시간을 처음부터 다시 시작합니다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI")
	void PresentFailure(
		EFF_BuildPlacementFailureType InFailureType,
		const FText& InTitle,
		const FText& InReason);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI")
	void DismissFeedback();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	EFF_BuildPlacementFailureType GetFailureType() const { return FailureType; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetFailureTitle() const { return FailureTitle; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetFailureReason() const { return FailureReason; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetDisplayedFailureTitle() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetDisplayedFailureReason() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

	/** 타입별 색상·아이콘과 등장 애니메이션을 WBP에서 확장하는 지점입니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Building|UI",
		meta = (DisplayName = "On Failure Presented"))
	void BP_OnFailurePresented(
		EFF_BuildPlacementFailureType InFailureType,
		const FText& InTitle,
		const FText& InReason);

	/** 숨김 직후 정리 사운드나 상태 초기화를 WBP에서 확장할 수 있습니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Building|UI",
		meta = (DisplayName = "On Failure Dismissed"))
	void BP_OnFailureDismissed();

private:
	void ApplyPresentation();
	void RestartAutoDismissTimer();
	void ClearAutoDismissTimer();

	UPROPERTY(Transient)
	EFF_BuildPlacementFailureType FailureType = EFF_BuildPlacementFailureType::Unknown;

	UPROPERTY(Transient)
	FText FailureTitle;

	UPROPERTY(Transient)
	FText FailureReason;

	bool bHasRuntimePresentation = false;
	FTimerHandle AutoDismissTimer;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_FailureTitle;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_FailureReason;

	/** 0이면 자동으로 숨기지 않습니다. 디자이너가 WBP Class Defaults에서 조정할 수 있습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Designer",
		meta = (ClampMin = "0.0", Units = "s", AllowPrivateAccess = "true"))
	float AutoDismissSeconds = 3.5f;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Designer")
	FText DesignerPreviewTitle = NSLOCTEXT(
		"FFBuildPlacementFailureFeedback", "DesignerPreviewTitle", "골드 부족");

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Designer")
	FText DesignerPreviewReason = NSLOCTEXT(
		"FFBuildPlacementFailureFeedback", "DesignerPreviewReason",
		"시설 설치비 400 Gold가 필요하지만 현재 잔액은 0 Gold입니다.");
};
