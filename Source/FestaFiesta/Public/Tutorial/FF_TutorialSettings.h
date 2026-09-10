// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Tutorial/FF_DialogueBoxWidget.h"
#include "Tutorial/FF_TutorialOverlayWidget.h"
#include "FF_TutorialSettings.generated.h"

class UFF_DialogueScriptData;

// 튜토리얼의 프로젝트 기본값.
// Project Settings > Game > FestaFiesta Tutorial 에서 지정한다.
//
// 레벨마다 다른 대사로 시작할 수 있어야 해서 GameMode 쪽 지정이 우선한다.
// 여기 값은 그게 비어 있을 때 쓴다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Tutorial"))
class FESTAFIESTA_API UFF_TutorialSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	static const UFF_TutorialSettings* Get();

	// 어느 레벨에서든 시작할 대사. 보통 비워둔다.
	//
	// 레벨마다 다른 대사가 필요하므로 GameMode 지정이 우선한다.
	// 여기에 넣으면 모든 레벨에서 재생되므로, 전체에 공통으로 나와야 하는
	// 안내가 아니라면 GameMode 쪽에 지정하는 편이 맞다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (DisplayName = "Default Start Script"))
	TSoftObjectPtr<UFF_DialogueScriptData> DefaultStartScript;

	// 시작까지 기다릴 시간(초).
	//
	// 레벨이 뜨자마자 대사가 나오면 화면이 아직 정리되기 전이라 어수선하다.
	// 0 이면 즉시 시작한다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (ClampMin = "0.0"))
	float StartDelaySeconds = 0.5f;

	// 대사창 위젯. 지정하면 대사가 시작될 때 자동으로 화면에 띄운다.
	//
	// 직접 띄우고 싶으면 비워두면 된다. 그때는 만들어둔 위젯이 알아서 구독한다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (DisplayName = "Dialogue Box Widget Class"))
	TSoftClassPtr<UFF_DialogueBoxWidget> DialogueBoxWidgetClass;

	// 강조 오버레이 위젯. 비워두면 강조 표시가 나오지 않는다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (DisplayName = "Overlay Widget Class"))
	TSoftClassPtr<UFF_TutorialOverlayWidget> OverlayWidgetClass;

	// 대사창과 오버레이의 화면 순서. 큰 값이 위에 온다.
	//
	// 대사창이 오버레이보다 위여야 어두운 덮개에 가려지지 않는다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial")
	int32 OverlayZOrder = 100;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial")
	int32 DialogueBoxZOrder = 110;

	// 카메라를 이 거리 이상 움직이면 이동한 것으로 본다.
	//
	// 너무 작으면 손이 미끄러진 것도 조작으로 잡히고, 너무 크면 한참 움직여야
	// 다음으로 넘어간다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (ClampMin = "0.0"))
	float CameraMoveThresholdCm = 1500.f;

	// 줌 배율이 이만큼 달라지면 줌한 것으로 본다. 0.3 이면 30% 변화.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (ClampMin = "0.01"))
	float CameraZoomThresholdRatio = 0.3f;
};
