// FF_BuildMultiSelectionWidget.h
// 역할: 건축 중 선택한 여러 시설을 가로 목록과 드래그 사각형으로 보여 주는 화면을 선언한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "FF_BuildMultiSelectionWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UFF_BuildCatalogSubsystem;
class UFF_BuildModeSubsystem;
class AFFZSFacilityActor;

/** 시설을 보여 주고 해제·종료 요청을 전달한다. 실제 선택 목록은 BuildMode가 관리한다. */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Multi Selection", DisableNativeTick))
class FESTAFIESTA_API UFF_BuildMultiSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_BuildMultiSelectionWidget(const FObjectInitializer& ObjectInitializer);

	/** BuildMode의 현재 선택으로 목록을 다시 만든다. 위젯을 만든 직후에도 호출할 수 있다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Multi Selection")
	void RefreshSelection();

	/** 화면 픽셀 좌표로 드래그 사각형을 표시한다. 이 함수 자체는 시설을 선택하지 않는다. */
	void SetMarquee(bool bVisible, FVector2D StartViewportPx, FVector2D EndViewportPx);

	/** 패널 위 클릭이 뒤쪽 월드 시설의 선택으로도 처리되지 않도록 마우스 위치를 검사한다. */
	bool IsPointerOverStrip() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_MultiSelection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_MultiSelectionCount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> Row_SelectedFacilities;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_InspectSelection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ClearSelection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ExitMultiSelection;

private:
	void BuildNativeTree();
	void ResolveBindings();
	void AddFacilityCard(const AFFZSFacilityActor& Facility, int32 DisplayIndex);
	void AddEmptyHint();

	UFUNCTION() void HandleSelectionChanged();
	UFUNCTION() void HandleCatalogChanged();
	UFUNCTION() void HandleInspectClicked();
	UFUNCTION() void HandleClearClicked();
	UFUNCTION() void HandleExitClicked();

	TWeakObjectPtr<UFF_BuildModeSubsystem> BuildMode;
	TWeakObjectPtr<UFF_BuildCatalogSubsystem> Catalog;
	bool bMarqueeVisible = false;
	FVector2D MarqueeStartViewportPx = FVector2D::ZeroVector;
	FVector2D MarqueeEndViewportPx = FVector2D::ZeroVector;
};
