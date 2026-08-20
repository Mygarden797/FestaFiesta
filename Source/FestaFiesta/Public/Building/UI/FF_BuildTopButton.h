// FF_BuildTopButton.h

// 건설 카탈로그 상단의 건축·스킨·닫기 액션에 사용하는 투명 버튼을 선언한다.
// 흐름: 공용 스타일 설정 -> PreConstruct/Construct -> 선택 변경 때 표시선 동기화.
// 불변식: 상태 브러시는 투명하고 선택 여부는 하단 표시선과 콘텐츠 색으로만 표현한다.
// 실패/수명: 선택 표시 BindWidget이 없으면 시각 갱신만 건너뛰고 버튼 입력은 유지한다.

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
