#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "FF_PrepBuildCategoryChoice.generated.h"

/** Saved catalog tabs expose their preview and identity in the Widget Blueprint Designer. */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Catalog Category Choice"))
class FESTAFIESTA_API UFF_PrepBuildCategoryChoice : public UFF_PrepChoiceButton
{
	GENERATED_BODY()

public:
	/** -1 is All; other values are EFFZSFacilityCategory values. Runtime minor choices set their own ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "분류 ID (-1: 전체)"))
	int32 AuthoredChoiceId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "탭 이름"))
	FText AuthoredLabel = NSLOCTEXT("FestaFiestaPrepBuild", "FilterAll", "전체");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "탭 아이콘"))
	FSlateBrush AuthoredIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "디자이너에서 선택 표시"))
	bool bPreviewSelected = false;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
};
