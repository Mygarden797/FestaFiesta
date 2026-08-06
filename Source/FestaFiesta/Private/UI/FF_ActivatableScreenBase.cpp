// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_ActivatableScreenBase.h"

#include "UI/FF_UILog.h"

TOptional<FUIInputConfig> UFF_ActivatableScreenBase::GetDesiredInputConfig() const
{
	if (bIgnoreInputConfig)
	{
		// 빈 값을 돌려주면 CommonUI가 이 화면 때문에 입력 설정을 바꾸지 않는다.
		// 비모달 알림·토스트는 반드시 이 경로를 쓴다.
		return TOptional<FUIInputConfig>();
	}

	ECommonInputMode InputMode = ECommonInputMode::Menu;
	switch (ScreenInputMode)
	{
	case EFF_ScreenInputMode::Game:
		InputMode = ECommonInputMode::Game;
		break;
	case EFF_ScreenInputMode::All:
		InputMode = ECommonInputMode::All;
		break;
	case EFF_ScreenInputMode::Menu:
	default:
		InputMode = ECommonInputMode::Menu;
		break;
	}

	const EMouseCaptureMode CaptureMode =
		bShowMouseCursor ? EMouseCaptureMode::NoCapture : EMouseCaptureMode::CapturePermanently;

	return FUIInputConfig(InputMode, CaptureMode);
}

UWidget* UFF_ActivatableScreenBase::NativeGetDesiredFocusTarget() const
{
	if (InitialFocusTarget)
	{
		return InitialFocusTarget;
	}

	// 조용히 넘어가지 않고 이유를 남긴다.
	// 포커스 대상이 없으면 게임패드로 아무것도 선택할 수 없는 상태가 되는데,
	// 화면만 봐서는 원인을 알 수 없기 때문이다.
	UE_LOG(LogFestaFiestaUI, Verbose,
		TEXT("[%s] InitialFocusTarget이 없어 CommonUI 기본 포커스 규칙을 따른다."), *GetName());

	return Super::NativeGetDesiredFocusTarget();
}
