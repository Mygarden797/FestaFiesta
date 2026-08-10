// FF_BuildTopButton.h

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_CommonButtonBase.h"
#include "FF_BuildTopButton.generated.h"

class UImage;

/**
 * 건설 카탈로그의 건축/스킨/닫기 액션에만 사용하는 투명 버튼이다.
 *
 * 대분류 버튼 WBP와 클래스를 분리해 상단 배경을 투명하게 바꾸더라도 부스·화장실 등
 * 대분류 타일의 Surface에는 영향을 주지 않는다. 선택 상태는 배경 면 대신 하단 표시선으로 남긴다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Top Button"))
class FESTAFIESTA_API UFF_BuildTopButton : public UFF_CommonButtonBase
{
	GENERATED_BODY()

public:
	UFF_BuildTopButton();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;

	/** 투명 배경에서도 현재 Build/Skin 탭을 식별할 수 있게 하는 하단 3px 표시선. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Building|UI")
	TObjectPtr<UImage> Image_SelectedIndicator;

private:
	void RefreshSelectedIndicator();
};
