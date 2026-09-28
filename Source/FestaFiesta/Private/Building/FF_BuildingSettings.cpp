// FF_BuildingSettings.cpp
// 역할: 기존 화면·처리 속도를 기본값으로 유지하고 잘못된 설정으로 무한 작업이나 멈춤이 생기지 않게 한다.
#include "Building/FF_BuildingSettings.h"

#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Presentation/FF_BuildModeGuide.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "Building/UI/FF_BuildCatalogScreen.h"

UFF_BuildingSettings::UFF_BuildingSettings()
{
	CatalogScreenClass = TSoftClassPtr<UFF_BuildCatalogScreen>(FSoftObjectPath(FFBuildUIAssetPaths::DefaultCatalogScreenClassPath));
	PlacementFailureFeedbackClass = TSoftClassPtr<UFF_BuildPlacementFailureFeedback>(FSoftObjectPath(FFBuildUIAssetPaths::DefaultPlacementFailureFeedbackClassPath));
	BuildModeGuideClass = TSoftClassPtr<UFF_BuildModeGuide>(FSoftObjectPath(FFBuildUIAssetPaths::DefaultBuildModeGuideClassPath));
}

double UFF_BuildingSettings::GetWorkBudgetSeconds() const
{
	const float SafeMilliseconds = FMath::IsFinite(WorkBudgetMilliseconds)
		? FMath::Clamp(WorkBudgetMilliseconds, .1f, 16.f) : 2.f;
	return static_cast<double>(SafeMilliseconds) / 1000.;
}

int32 UFF_BuildingSettings::GetDragPlacementsPerFrame() const
{
	return FMath::Clamp(DragPlacementsPerFrame, 1, 64);
}

int32 UFF_BuildingSettings::GetNearbyCandidatesPerFrame() const
{
	return FMath::Clamp(NearbyCandidatesPerFrame, 1, 64);
}

float UFF_BuildingSettings::GetPreviewRefreshIntervalSeconds() const
{
	return FMath::IsFinite(PreviewRefreshIntervalSeconds)
		? FMath::Clamp(PreviewRefreshIntervalSeconds, .01f, 1.f) : .08f;
}
