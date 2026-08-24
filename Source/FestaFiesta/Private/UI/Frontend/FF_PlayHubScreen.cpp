// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_PlayHubScreen.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"
#include "UI/Frontend/FF_SaveListProvider.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

void UFF_PlayHubScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// 🔴 구독은 `NativeConstruct` 다 — `NativeOnInitialized` 가 아니다.
	//
	// CommonUI 의 화면 스택은 위젯을 **풀에 넣고 재사용한다**
	// (`UCommonActivatableWidgetContainerBase::AddWidgetInternal` → `GeneratedWidgetsPool`).
	// `NativeOnInitialized` 는 인스턴스당 한 번뿐이고 `NativeDestruct` 는 스택에서 내려갈 때마다
	// 돈다. 그래서 구독을 `NativeOnInitialized` 에 두면 —
	//   ① 첫 진입: 구독됨, 버튼 동작
	//   ② 뒤로: `NativeDestruct` 가 **해제**
	//   ③ 다시 진입: 같은 인스턴스가 재사용되므로 `NativeOnInitialized` 가 돌지 않아 **버튼이 죽는다**
	// 실제로 그렇게 죽었다 (2026-08-23, 플레이 허브의 `새 축제 시작`·`뒤로`).
	// **구독과 해제의 수명이 같은 짝이어야 한다.**
	const TArray<UFF_CommonButtonBase*> Buttons =
		{ Button_Continue, Button_NewFestival, Button_SaveList, Button_Back };

	// 버튼이 자기를 넘겨주므로 핸들러 하나로 넷을 구분한다.
	for (UFF_CommonButtonBase* Button : Buttons)
	{
		if (Button)
		{
			Button->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_PlayHubScreen::HandleButtonClicked);
		}
	}
}

void UFF_PlayHubScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 문구를 코드에서 넣는다. WBP 마다 다르게 적히면 화면 문구가 갈라진다.
	// 이쪽은 인스턴스에 한 번만 해도 되는 것이라 여기 남는다.
	if (Button_Continue)
	{
		Button_Continue->SetButtonLabel(LOCTEXT("PlayHubContinue", "최근 축제 계속하기"));
	}
	if (Button_NewFestival)
	{
		Button_NewFestival->SetButtonLabel(LOCTEXT("PlayHubNewFestival", "새 축제 시작"));
	}
	if (Button_SaveList)
	{
		Button_SaveList->SetButtonLabel(LOCTEXT("PlayHubSaveList", "저장 기록 선택"));
	}
	if (Button_Back)
	{
		Button_Back->SetButtonLabel(LOCTEXT("PlayHubBack", "뒤로"));
	}
}

void UFF_PlayHubScreen::NativeOnActivated()
{
	// 활성화될 때마다 다시 읽는다. 저장 기록은 이 화면 밖에서 늘어난다
	// (축제를 하고 돌아오는 경로). 생성 시 한 번만 읽으면 영영 `기록 없음` 이 된다.
	RefreshFromSaveList();

	// 포커스는 베이스가 NativeGetDesiredFocusTarget 으로 잡는다.
	// 그 함수가 bHasSave 를 보므로 **반드시 RefreshFromSaveList 뒤에** Super 를 부른다.
	Super::NativeOnActivated();
}

void UFF_PlayHubScreen::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건.
	const TArray<UFF_CommonButtonBase*> Buttons =
		{ Button_Continue, Button_NewFestival, Button_SaveList, Button_Back };

	for (UFF_CommonButtonBase* Button : Buttons)
	{
		if (Button)
		{
			Button->OnFFButtonClicked.RemoveDynamic(this, &UFF_PlayHubScreen::HandleButtonClicked);
		}
	}

	Super::NativeDestruct();
}

void UFF_PlayHubScreen::RefreshFromSaveList()
{
	const UFF_SaveListProvider* Provider = UFF_SaveListProvider::Get(this);
	if (!Provider)
	{
		// 조용히 잠긴 화면을 보여주지 않는다. 왜 잠겼는지 남긴다.
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_PlayHubScreen: 저장 기록 Provider 를 찾지 못했다. 이어하기를 잠근 상태로 둔다."));
		bHasSave = false;
	}
	else
	{
		bHasSave = Provider->HasAnySave();
	}

	// 표시를 부분 갱신하지 않고 전부 다시 선언한다. 어떤 순서로 들어와도 결과가 같다.
	if (Button_Continue)
	{
		Button_Continue->SetIsEnabled(bHasSave);
	}
	if (Button_SaveList)
	{
		Button_SaveList->SetIsEnabled(bHasSave);
	}

	// 잠금은 **색이 아니라** 자물쇠 자리 + 문구로 알린다 (AT-011).
	const ESlateVisibility LockVisibility =
		bHasSave ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;

	if (Image_LockedIcon)
	{
		Image_LockedIcon->SetVisibility(LockVisibility);
	}
	if (Text_LockedNotice)
	{
		Text_LockedNotice->SetText(LOCTEXT("PlayHubNoRecord", "기록 없음"));
		Text_LockedNotice->SetVisibility(LockVisibility);
	}

	// 우측 최근 기록 목록. 기록이 없으면 목록을 숨기고 빈 상태 문구만 남긴다.
	if (Panel_Recent)
	{
		Panel_Recent->SetVisibility(bHasSave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_RecentEmpty)
	{
		Text_RecentEmpty->SetText(LOCTEXT("PlayHubRecentEmpty", "저장 기록이 없습니다"));
		Text_RecentEmpty->SetVisibility(LockVisibility);
	}
}

UWidget* UFF_PlayHubScreen::NativeGetDesiredFocusTarget() const
{
	// 잠긴 버튼에 포커스를 주면 게임패드로 아무것도 할 수 없는 화면이 된다.
	if (bHasSave && Button_Continue)
	{
		return Button_Continue;
	}

	if (Button_NewFestival)
	{
		return Button_NewFestival;
	}

	return Super::NativeGetDesiredFocusTarget();
}

void UFF_PlayHubScreen::HandleButtonClicked(UFF_CommonButtonBase* Button)
{
	if (!Button)
	{
		return;
	}

	// 잠긴 버튼이 눌리는 경로가 남아 있을 수 있다(BP 배선 등). 여기서 한 번 더 막는다.
	if ((Button == Button_Continue || Button == Button_SaveList) && !bHasSave)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_PlayHubScreen: 저장 기록이 없는데 `%s` 가 눌렸다. 무시했다."),
			*Button->GetName());
		return;
	}

	if (Button == Button_Continue)
	{
		OnContinueRequested.Broadcast();
	}
	else if (Button == Button_NewFestival)
	{
		OnNewFestivalRequested.Broadcast();
	}
	else if (Button == Button_SaveList)
	{
		OnSaveListRequested.Broadcast();
	}
	else if (Button == Button_Back)
	{
		OnBackRequested.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE
