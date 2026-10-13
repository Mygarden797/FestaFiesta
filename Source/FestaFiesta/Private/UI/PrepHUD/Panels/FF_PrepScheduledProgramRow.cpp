#include "UI/PrepHUD/Panels/FF_PrepScheduledProgramRow.h"

#include "Components/TextBlock.h"

void UFF_PrepScheduledProgramRow::ConfigureProgram(
	const FText& ProgramName, const FText& FacilityName, const FText& TimeRange)
{
	CachedProgramName = ProgramName;
	CachedFacilityName = FacilityName;
	CachedTimeRange = TimeRange;
	bHasConfiguredProgram = true;
	ApplyPresentation();
}

void UFF_PrepScheduledProgramRow::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyPresentation();
}

void UFF_PrepScheduledProgramRow::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyPresentation();
}

void UFF_PrepScheduledProgramRow::ApplyPresentation()
{
	// 데이터가 들어오기 전에는 디자이너 미리보기 값을 보존한다.
	if (!bHasConfiguredProgram) return;

	if (Text_ProgramName) Text_ProgramName->SetText(CachedProgramName);
	if (Text_FacilityName) Text_FacilityName->SetText(CachedFacilityName);
	if (Text_TimeRange)
	{
		Text_TimeRange->SetText(CachedTimeRange);
		Text_TimeRange->SetVisibility(CachedTimeRange.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
