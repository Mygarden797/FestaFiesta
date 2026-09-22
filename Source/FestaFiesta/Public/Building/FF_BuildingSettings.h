// FF_BuildingSettings.h
// 역할: 건설 화면과 처리 속도 설정을 에디터의 프로젝트 설정 한곳에서 관리한다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_BuildingSettings.generated.h"

class UFF_BuildCatalogScreen;
class UFF_BuildModeGuide;
class UFF_BuildPlacementFailureFeedback;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="FestaFiesta 건설"))
class FESTAFIESTA_API UFF_BuildingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFF_BuildingSettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 화면을 교체할 때 C++ 경로를 고치지 않고 프로젝트 설정에서 위젯 Blueprint를 선택한다.
	UPROPERTY(EditAnywhere, Config, Category="화면", meta=(DisplayName="기본 건설 카탈로그"))
	TSoftClassPtr<UFF_BuildCatalogScreen> CatalogScreenClass;
	UPROPERTY(EditAnywhere, Config, Category="화면", meta=(DisplayName="배치 실패 안내"))
	TSoftClassPtr<UFF_BuildPlacementFailureFeedback> PlacementFailureFeedbackClass;
	UPROPERTY(EditAnywhere, Config, Category="화면", meta=(DisplayName="건설 조작 안내"))
	TSoftClassPtr<UFF_BuildModeGuide> BuildModeGuideClass;

	// 설치/검증 명령 한 건은 중단하지 않는다. 아래 시간은 명령 사이에 확인하는 프레임 예산이다.
	UPROPERTY(EditAnywhere, Config, Category="처리 속도", meta=(DisplayName="프레임당 작업 시간 (ms)", ClampMin="0.1", ClampMax="16.0"))
	float WorkBudgetMilliseconds = 2.f;
	UPROPERTY(EditAnywhere, Config, Category="처리 속도", meta=(DisplayName="드래그 프레임당 최대 설치 수", ClampMin="1", ClampMax="64"))
	int32 DragPlacementsPerFrame = 8;
	UPROPERTY(EditAnywhere, Config, Category="처리 속도", meta=(DisplayName="빈자리 찾기 프레임당 최대 검사 수", ClampMin="1", ClampMax="64"))
	int32 NearbyCandidatesPerFrame = 4;
	UPROPERTY(EditAnywhere, Config, Category="처리 속도", meta=(DisplayName="배치 미리보기 갱신 간격 (초)", ClampMin="0.01", ClampMax="1.0"))
	float PreviewRefreshIntervalSeconds = .08f;

	// ini 직접 편집으로 에디터 제한을 우회한 값도 런타임에서 안전한 범위로 제한한다.
	double GetWorkBudgetSeconds() const;
	int32 GetDragPlacementsPerFrame() const;
	int32 GetNearbyCandidatesPerFrame() const;
	float GetPreviewRefreshIntervalSeconds() const;
};
