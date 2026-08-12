// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_FestivalGameMode.h"

#include "Camera/FF_CameraTypes.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"

AFF_FestivalGameMode::AFF_FestivalGameMode()
{
	DefaultPawnClass = AFF_OverviewCameraPawn::StaticClass();
	PlayerControllerClass = AFF_FestivalPlayerController::StaticClass();

	// 관찰 시점이라 HUD 나 스테이트 클래스를 따로 지정하지 않는다.
	// 필요해지면 그때 담당자가 정하면 된다.
}

void AFF_FestivalGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!bApplyInitialFraming)
	{
		return;
	}

	// 첫 플레이어의 카메라만 잡는다. 이 프로젝트는 싱글 관찰 시점이다.
	const AFF_FestivalPlayerController* Controller =
		Cast<AFF_FestivalPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
	if (!Controller)
	{
		return;
	}

	if (AFF_OverviewCameraPawn* CameraPawn = Controller->GetOverviewCameraPawn())
	{
		CameraPawn->InitializeOverview(InitialFocusLocation, InitialArmLength);
	}
	else
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[카메라] 관찰 카메라 Pawn 을 못 찾아 초기 구도를 적용하지 못했다. ")
			TEXT("GameMode 의 DefaultPawnClass 를 확인할 것."));
	}
}
