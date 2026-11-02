// FF_FFZSBuildPresentationSettings.cpp
// 역할: 격자·예약 범위·배치 고스트의 색, 선 두께, 재질을 Project Settings에서 조정하게 한다.
#include "FF_FFZSBuildPresentationSettings.h"
#include "Materials/MaterialInterface.h"

UFFZSBuildPresentationSettings::UFFZSBuildPresentationSettings()
{
    CategoryName = TEXT("Game");
    SectionName = TEXT("FFBuildPresentation");

    // 기존 프로젝트의 기본 외형을 보존하는 초기값이다. 다른 프로젝트는 설정에서 교체한다.
    // 개별 액터마다 경로를 복제하지 않고, 공통 설정의 소프트 참조 한 곳만 유지한다.
    DefaultPlacementFeedbackMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
        TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity/M_BuildPlacementFeedback.M_BuildPlacementFeedback")));
}
