// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_SaveListScreen.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"
#include "UI/Frontend/FF_SaveListProvider.h"
#include "UI/Frontend/FF_SaveSlotCard.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

void UFF_SaveListScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_SaveListScreen::HandleBackClicked);
		Button_Back->SetButtonLabel(LOCTEXT("SaveListBack", "뒤로"));
	}

	if (UFF_SaveListProvider* Provider = UFF_SaveListProvider::Get(this))
	{
		Provider->OnSaveListChanged.AddUniqueDynamic(this, &UFF_SaveListScreen::HandleSaveListChanged);
	}
}

void UFF_SaveListScreen::NativeOnActivated()
{
	// 구독만 하면 다음 변경까지 빈 화면이다. 현재값을 한 번 읽는다 (Bind + Sync).
	RebuildList();
	Super::NativeOnActivated();
}

void UFF_SaveListScreen::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건.
	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.RemoveDynamic(this, &UFF_SaveListScreen::HandleBackClicked);
	}

	if (UFF_SaveListProvider* Provider = UFF_SaveListProvider::Get(this))
	{
		Provider->OnSaveListChanged.RemoveDynamic(this, &UFF_SaveListScreen::HandleSaveListChanged);
	}

	Super::NativeDestruct();
}

void UFF_SaveListScreen::HandleSaveListChanged()
{
	RebuildList();
}

void UFF_SaveListScreen::RebuildList()
{
	const UFF_SaveListProvider* Provider = UFF_SaveListProvider::Get(this);

	// 저장 기능이 아직 없다는 사실을 화면에 남긴다.
	if (Text_SaveSystemNotice)
	{
		const bool bReal = Provider && Provider->IsBackedByRealSaveSystem();
		Text_SaveSystemNotice->SetText(
			LOCTEXT("SaveListNotYet", "저장 기능 준비 중 — 목록은 예시이고 불러올 수 없습니다"));
		Text_SaveSystemNotice->SetVisibility(
			bReal ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (VerticalBox_Auto)
	{
		VerticalBox_Auto->ClearChildren();
	}
	if (VerticalBox_Manual)
	{
		VerticalBox_Manual->ClearChildren();
	}

	int32 AutoCount = 0;
	int32 ManualCount = 0;

	if (Provider && CardWidgetClass)
	{
		for (const FFF_SaveSlotEntry& Entry : Provider->GetSlots())
		{
			UVerticalBox* Target = Entry.bAutoSave ? VerticalBox_Auto : VerticalBox_Manual;
			if (!Target)
			{
				continue;
			}

			UFF_SaveSlotCard* Card = CreateWidget<UFF_SaveSlotCard>(this, CardWidgetClass);
			if (!Card)
			{
				UE_LOG(LogFestaFiestaUI, Error,
					TEXT("UFF_SaveListScreen: 카드 생성 실패 (%s)."), *CardWidgetClass->GetName());
				continue;
			}

			Card->SetEntry(Entry);
			Card->OnSaveSlotSelected.AddUniqueDynamic(this, &UFF_SaveListScreen::HandleSlotSelected);
			Target->AddChild(Card);

			Entry.bAutoSave ? ++AutoCount : ++ManualCount;
		}
	}
	else if (Provider && !CardWidgetClass)
	{
		// 기록이 있는데 카드 클래스가 없으면 빈 화면이 된다. 이유를 남긴다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_SaveListScreen: CardWidgetClass 가 없어 기록 %d건을 그리지 못했다. "
				 "WBP 에서 WBP_FF_SaveSlotCard 를 지정한다."),
			Provider->GetSlots().Num());
	}

	// 그룹이 비면 머리글째로 숨긴다. `수동 저장` 머리글만 남기지 않는다.
	if (Panel_AutoGroup)
	{
		Panel_AutoGroup->SetVisibility(
			AutoCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_ManualGroup)
	{
		Panel_ManualGroup->SetVisibility(
			ManualCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_Empty)
	{
		const bool bEmpty = (AutoCount + ManualCount) == 0;
		Text_Empty->SetText(LOCTEXT("SaveListEmpty", "저장 기록이 없습니다"));
		Text_Empty->SetVisibility(bEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UFF_SaveListScreen::HandleSlotSelected(FName SlotId)
{
	// 불러오기 Command 가 없다. 낙관적으로 화면을 바꾸지 않는다 —
	// 결과 이벤트를 받은 뒤에 전환하는 것이 저장 UI 경계 규칙이다.
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_SaveListScreen: 슬롯 `%s` 선택. 불러오기 API 가 아직 없어 아무것도 하지 않는다."),
		*SlotId.ToString());
}

void UFF_SaveListScreen::HandleBackClicked(UFF_CommonButtonBase* Button)
{
	OnBackRequested.Broadcast();
}

#undef LOCTEXT_NAMESPACE
