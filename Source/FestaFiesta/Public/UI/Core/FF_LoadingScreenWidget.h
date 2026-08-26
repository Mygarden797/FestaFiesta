// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_LoadingScreenWidget.generated.h"

class UBorder;
class UCircularThrobber;
class UTextBlock;

/**
 * 축제 레벨을 여는 동안 MoviePlayer가 유지하는 로딩 화면.
 *
 * 일반 화면 스택에 올리지 않는다. `OpenLevel`은 게임 스레드를 막으므로 UMG를 먼저 AddToViewport해도
 * 다음 프레임이 그려지지 않는다. `UFF_UIFlowSubsystem`이 PreLoadMap에서 `TakeWidget()` 결과를
 * MoviePlayer에 넘긴다. 이 클래스는 표시만 하고 레벨 전환을 직접 요청하지 않는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Loading Screen Widget"))
class FESTAFIESTA_API UFF_LoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_LoadingScreenWidget(const FObjectInitializer& ObjectInitializer);

	/** 이번에 여는 축제 이름을 넣고 전체 표시를 즉시 동기화한다. */
	void SetLoadingContext(const FText& InFestivalName);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	/** 화면 전체 배경. 색은 프론트엔드 팔레트의 Void를 쓴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Loading")
	TObjectPtr<UBorder> Border_Background;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Loading")
	TObjectPtr<UTextBlock> Text_FestivalName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Loading")
	TObjectPtr<UTextBlock> Text_Loading;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Loading")
	TObjectPtr<UTextBlock> Text_Tip;

	/** 애니메이션은 Slate가 그린다. 게임 Tick에 의존하는 BP 애니메이션은 쓰지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Loading")
	TObjectPtr<UCircularThrobber> Throbber_Loading;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Loading|Content")
	FText LoadingMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Loading|Content")
	FText TipMessage;

private:
	void RefreshView();

	FText FestivalName;
};
