// FF_BuildMinorSection.h

// 하나의 소분류 제목과 해당 시설 카드들을 묶는 디자이너 소유 UMG 섹션이다.
// 화면은 이 위젯을 소분류마다 동적으로 생성한 뒤 AddCard로 카드를 전달하며, 이 클래스는
//  카드의 선택 로직을 알지 않고 오직 WBP 컨테이너 계층에 배치하는 역할만 담당한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_BuildMinorSection.generated.h"

class UCommonTextBlock;
class UWidget;
class UWrapBox;

UCLASS(Blueprintable, meta = (DisplayName = "FF Build Minor Section"))
class FESTAFIESTA_API UFF_BuildMinorSection : public UUserWidget
{
	GENERATED_BODY()

public:
	// 저장된 제목과 현재 연결된 제목 위젯을 함께 갱신한다. 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI")
	void SetSectionTitle(const FText& InTitle);

	// 유효한 카드만 WrapBox에 추가한다. 자식 소유권은 UMG 패널이 가진다. 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI")
	void AddCard(UWidget* Card);

	// 섹션을 재사용할 때 기존 패널 자식을 제거한다. 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI")
	void ClearCards();

protected:
	// 디자이너 미리보기와 런타임 생성 직전에 직렬화된 제목을 BindWidget에 투영한다. 
	virtual void NativePreConstruct() override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (AllowPrivateAccess = "true"))
	FText SectionTitle;

	// WBP_FF_BuildMinorSection이 반드시 같은 이름과 호환 타입으로 제공해야 하는 BindWidget 계약이다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_MinorTitle;

	// 화면이 만든 시설 카드가 들어갈 WrapBox의 이름 기반 BindWidget 계약이다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWrapBox> Box_Cards;
};
