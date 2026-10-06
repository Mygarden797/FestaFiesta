// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Incident/FF_IncidentTypes.h"
#include "FF_IncidentMarkerButton.generated.h"

class UBorder;
class UImage;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FFF_OnIncidentMarkerButtonClicked, FFF_IncidentHandle);

// 사건 아이콘 하나. 포커스를 가져가지 않도록 UButton 대신 마우스 입력을 직접 받고, 자기 핸들을 붙여 알린다.
// 스태프가 출동 중이면 아이콘을 팔레트 Teal 고리로 감싸고 툴팁에 진행 단계를 적는다. 고리라는 모양이 생기므로 색만으로 상태를 가르지 않는다(AT-011).
UCLASS(meta = (DisplayName = "FF Incident Marker Button"))
class FESTAFIESTA_API UFF_IncidentMarkerButton : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHandle(FFF_IncidentHandle InHandle) { Handle = InHandle; }
	FFF_IncidentHandle GetHandle() const { return Handle; }

	void SetIcon(UTexture2D* Icon, const FLinearColor& FallbackColor, const FVector2D& IconSize);
	void SetResponseStage(EFF_IncidentResponseStage InStage);
	EFF_IncidentResponseStage GetResponseStage() const { return ResponseStage; }
	bool IsResponseRingVisible() const;

	FFF_OnIncidentMarkerButtonClicked OnMarkerClicked;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	void EnsureTree();
	void ApplyTint();

	UPROPERTY(Transient)
	TObjectPtr<UImage> Image;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Ring;

	FFF_IncidentHandle Handle;
	EFF_IncidentResponseStage ResponseStage = EFF_IncidentResponseStage::None;
	FLinearColor BaseTint = FLinearColor::White;
	bool bHovered = false;
};
