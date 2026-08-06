// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_UIRootLayout.h"

#include "Widgets/CommonActivatableWidgetContainer.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "UI/FF_UILog.h"

UFF_ActivatableScreenBase* UFF_UIRootLayout::PushScreen(TSubclassOf<UFF_ActivatableScreenBase> ScreenClass)
{
	return PushToStack(ScreenStack, ScreenClass, TEXT("ScreenStack"));
}

UFF_ActivatableScreenBase* UFF_UIRootLayout::PushPanel(TSubclassOf<UFF_ActivatableScreenBase> PanelClass)
{
	return PushToStack(PanelStack, PanelClass, TEXT("PanelStack"));
}

bool UFF_UIRootLayout::PopScreen()
{
	return PopFromStack(ScreenStack, TEXT("ScreenStack"));
}

bool UFF_UIRootLayout::PopPanel()
{
	return PopFromStack(PanelStack, TEXT("PanelStack"));
}

UFF_ActivatableScreenBase* UFF_UIRootLayout::PushToStack(
	UCommonActivatableWidgetStack* TargetStack,
	TSubclassOf<UFF_ActivatableScreenBase> WidgetClass,
	const TCHAR* StackLabel)
{
	// 가드마다 이유를 남긴다. 조용히 nullptr을 돌려주면 화면이 안 뜨는 원인을 찾을 수 없다.
	if (!TargetStack)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] %s 바인딩이 없다. WBP에 같은 이름의 CommonActivatableWidgetStack이 있는지 확인할 것."),
			*GetName(), StackLabel);
		return nullptr;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] %s에 밀어 넣을 위젯 클래스가 지정되지 않았다."), *GetName(), StackLabel);
		return nullptr;
	}

	UFF_ActivatableScreenBase* Pushed = TargetStack->AddWidget<UFF_ActivatableScreenBase>(WidgetClass);
	if (!Pushed)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] %s에 %s 생성 실패."), *GetName(), StackLabel, *WidgetClass->GetName());
		return nullptr;
	}

	UE_LOG(LogFestaFiestaUI, Verbose,
		TEXT("[%s] %s ← %s"), *GetName(), StackLabel, *WidgetClass->GetName());

	return Pushed;
}

bool UFF_UIRootLayout::PopFromStack(UCommonActivatableWidgetStack* TargetStack, const TCHAR* StackLabel)
{
	if (!TargetStack)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] %s 바인딩이 없어 닫을 수 없다."), *GetName(), StackLabel);
		return false;
	}

	UCommonActivatableWidget* ActiveWidget = TargetStack->GetActiveWidget();
	if (!ActiveWidget)
	{
		// 닫을 것이 없는 것은 오류가 아니다(뒤로 가기 중복 입력 등).
		UE_LOG(LogFestaFiestaUI, Verbose,
			TEXT("[%s] %s가 비어 있어 닫을 것이 없다."), *GetName(), StackLabel);
		return false;
	}

	TargetStack->RemoveWidget(*ActiveWidget);
	return true;
}
