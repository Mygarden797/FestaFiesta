// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "UI/Widgets/FF_HudTabButtonBase.h"
#include "FF_PrepHudTabButton.generated.h"

/**
 * 준비 단계 하단 바의 탭 버튼 하나.
 *
	 * 공통 선택 처리는 베이스가 담당한다. 피날레 구매 기회는 금색·오늘 배지로 추가 표시한다.
 *
 * WBP 는 `WBP_FF_PrepHudTabButton` 하나만 만들고 바에 6개를 놓는다.
 * **탭을 런타임에 생성하지 않는다** — 디자이너에서 안 보여 위치·크기를 손으로 못 만진다
 * (2026-08-13 라이브 HUD 에서 실제로 겪었다. docs/memory/wbp-codegen-workflow.md).
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep HUD Tab Button"))
class FESTAFIESTA_API UFF_PrepHudTabButton : public UFF_HudTabButtonBase
{
	GENERATED_BODY()

public:
	void Configure(EFF_PrepHudTab InTab, const FText& InLabel, const FSlateBrush& InIconBrush)
	{
		ConfigureTab(static_cast<int32>(InTab), DisplayLabelOverride.IsEmpty() ? InLabel : DisplayLabelOverride,
			DesignerIcon.GetResourceObject() ? DesignerIcon : InIconBrush);
	}

	EFF_PrepHudTab GetTab() const { return static_cast<EFF_PrepHudTab>(GetTabId()); }

	/** 구매 가능 여부는 부모 바가 ViewModel에서 읽어 전달한다. */
	void SetFinaleAttention(bool bEnabled);

	/** 완료 이벤트의 현재 상태로만 해제하며 탭 열람으로는 해제하지 않는다. */
	void SetRequiredAttention(bool bEnabled, const FText& Hint = FText::GetEmpty());
	bool HasRequiredAttention() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Required")
	bool bPreviewRequiredAttention = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Required", meta = (ClampMin = "16.0", ClampMax = "40.0"))
	float RequiredBadgeDiameter = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Required")
	FLinearColor RequiredBadgeColor = FLinearColor::FromSRGBColor(FColor(0xE9, 0x91, 0x18));

	/** 부모 HUD Designer에서 탭별 문구를 바꾼다. 비워 두면 공통 번역 문구를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Text")
	FText DisplayLabelOverride;

	/** 부모 HUD Designer에서 탭마다 다른 아이콘을 지정한다. 크기는 자식 Image_Icon에서 조절한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Appearance")
	FSlateBrush DesignerIcon;

	/** Designer에서 효과를 확인할 때만 사용한다. 런타임은 실제 구매 가능 상태를 따른다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Finale")
	bool bPreviewFinaleAttention = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Finale")
	FLinearColor FinaleFillColor = FLinearColor::FromSRGBColor(FColor(0xE8, 0xC4, 0x70));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Finale")
	FLinearColor FinaleOutlineColor = FLinearColor::FromSRGBColor(FColor(0xB2, 0x85, 0x30));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Finale")
	FLinearColor FinaleContentColor = FLinearColor::FromSRGBColor(FColor(0x55, 0x3B, 0x16));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Finale", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float FinaleSheenOpacity = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Finale", meta = (ClampMin = "2.0"))
	float FinaleSheenPeriod = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Finale")
	bool bAnimateFinaleSheen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Finale")
	FText FinaleBadgeText = NSLOCTEXT("FF_PrepHud", "FinaleTodayBadge", "오늘");

protected:
	virtual void NativePreConstruct() override;
	virtual void RefreshVisualState() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	bool HasFinaleAttention() const;
	bool bFinaleAttention = false;
	bool bRequiredAttention = false;
	float FinaleSheenElapsed = 0.f;
};
