#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_PrepScheduledProgramRow.generated.h"

class UTextBlock;

/** 오늘 편성 완료한 프로그램 한 건. 실행과 선택 기능 없이 현재 편성 정보만 표시한다. */
UCLASS(Abstract, Blueprintable)
class FESTAFIESTA_API UFF_PrepScheduledProgramRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void ConfigureProgram(const FText& ProgramName, const FText& FacilityName, const FText& TimeRange);
	virtual void SynchronizeProperties() override;

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ProgramName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FacilityName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_TimeRange;

private:
	void ApplyPresentation();

	UPROPERTY(Transient) FText CachedProgramName;
	UPROPERTY(Transient) FText CachedFacilityName;
	UPROPERTY(Transient) FText CachedTimeRange;
	bool bHasConfiguredProgram = false;
};
