// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Incident/FF_IncidentTypes.h"
#include "UI/LiveHUD/Incident/FF_IncidentMarkerRules.h"
#include "FF_IncidentMarkerButton.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FFF_OnIncidentMarkerButtonClicked, FFF_IncidentHandle);

// 원판 + 기호 + 링 하나. 포커스를 가져가지 않도록 UButton 대신 마우스를 직접 받고, 원판 밖 클릭은 월드로 흘려 보낸다.
UCLASS(meta = (DisplayName = "FF Incident Marker Button"))
class FESTAFIESTA_API UFF_IncidentMarkerButton : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHandle(FFF_IncidentHandle InHandle) { Handle = InHandle; }
	FFF_IncidentHandle GetHandle() const { return Handle; }

	void SetMarkerSize(const FVector2D& Size);
	void SetRingMaterial(UMaterialInterface* Material);
	void ApplyLook(const FFF_IncidentMarkerLook& InLook, UTexture2D* GlyphTexture);

	const FFF_IncidentMarkerLook& GetLook() const { return Look; }
	bool HasRingMaterial() const { return RingMID != nullptr; }
	float GetRingOuterRadius() const;

	static constexpr float DiscRadiusUV = 0.45f;
	static constexpr float RingOuterRadiusUV = 0.425f;
	static constexpr float BlinkRingBoostUV = 0.025f;

	FFF_OnIncidentMarkerButtonClicked OnMarkerClicked;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	void EnsureTree();
	void ApplyRingParameters();
	void ApplyTransform();
	bool IsInsideDisc(const FGeometry& InGeometry, const FVector2D& ScreenPosition) const;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Disc;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Glyph;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Ring;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMID;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentGlyph;

	FFF_IncidentHandle Handle;
	FFF_IncidentMarkerLook Look;
	float MotionTime = 0.f;
	bool bHovered = false;
};
