// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Title/FF_TitleScreen.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"

void UFF_TitleScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// AddUniqueDynamic만 쓴다. AddDynamic은 중복 구독을 막아주지 않는다.
	if (Btn_Play)
	{
		Btn_Play->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (Btn_Options)
	{
		Btn_Options->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (Btn_Quit)
	{
		Btn_Quit->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
}

void UFF_TitleScreen::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건. 조기 return으로 빠져나가면 아래 해제를 못 한다.
	if (Btn_Play)
	{
		Btn_Play->OnFFButtonClicked.RemoveDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (Btn_Options)
	{
		Btn_Options->OnFFButtonClicked.RemoveDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (Btn_Quit)
	{
		Btn_Quit->OnFFButtonClicked.RemoveDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}

	Super::NativeDestruct();
}

UWidget* UFF_TitleScreen::NativeGetDesiredFocusTarget() const
{
	if (Btn_Play)
	{
		return Btn_Play;
	}

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] Btn_Play가 없어 초기 포커스를 지정하지 못했다. 게임패드로 아무것도 선택할 수 없는 상태가 된다."),
		*GetName());

	return Super::NativeGetDesiredFocusTarget();
}

void UFF_TitleScreen::HandleButtonClicked(UFF_CommonButtonBase* Button)
{
	if (!Button)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 클릭 알림에 버튼이 없다."), *GetName());
		return;
	}

	if (Button == Btn_Play)
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 플레이 요청."), *GetName());
		// TODO: 플레이 허브 화면이 만들어지면 UFF_UIRootLayout::PushScreen으로 교체한다.
		//
		// 둘 다 쏜다. BP 이벤트는 이 WBP 안에서 쓰는 기존 경로이고,
		// 델리게이트는 화면을 띄운 쪽(레벨 BP·PlayerController)이 구독하는 경로다.
		BP_OnPlayRequested();
		OnPlayRequested.Broadcast();
		return;
	}

	if (Button == Btn_Options)
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 옵션 요청."), *GetName());
		// TODO: 옵션 화면이 만들어지면 UFF_UIRootLayout::PushScreen으로 교체한다.
		BP_OnOptionsRequested();
		OnOptionsRequested.Broadcast();
		return;
	}

	if (Button == Btn_Quit)
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 게임 종료 요청."), *GetName());

		// TODO: 확인 다이얼로그를 PanelStack에 띄운 뒤 종료하도록 바꾼다.
		APlayerController* OwningPC = UGameplayStatics::GetPlayerController(this, 0);
		UKismetSystemLibrary::QuitGame(this, OwningPC, EQuitPreference::Quit, false);
		return;
	}

	// 어느 버튼도 아니면 조용히 넘어가지 않고 남긴다. 버튼을 추가하고 처리를 빠뜨린 경우다.
	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 처리되지 않은 버튼 클릭: %s"), *GetName(), *Button->GetName());
}
