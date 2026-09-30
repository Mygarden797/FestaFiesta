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

void UFF_SaveListScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// 🔴 구독은 `NativeConstruct` 다. CommonUI 스택이 위젯을 풀에서 재사용하므로
	// `NativeOnInitialized`(인스턴스당 한 번)에 구독하고 `NativeDestruct`(내려갈 때마다)에
	// 해제하면, 뒤로 갔다 다시 들어온 화면은 **버튼이 죽는다.**
	//
	// 이 화면은 특히 중요하다 — Provider 는 위젯보다 **오래 사는 서브시스템**이라
	// 해제를 빼면 죽은 위젯을 역참조한다. 즉 해제를 없앨 수 없고, 구독을 옮기는 것이 답이다.
	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_SaveListScreen::HandleBackClicked);
	}

	if (UFF_SaveListProvider* Provider = UFF_SaveListProvider::Get(this))
	{
		Provider->OnSaveListChanged.AddUniqueDynamic(this, &UFF_SaveListScreen::HandleSaveListChanged);
	}
}

void UFF_SaveListScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 문구는 인스턴스에 한 번만 넣으면 된다.
	if (Button_Back)
	{
		Button_Back->SetButtonLabel(LOCTEXT("SaveListBack", "뒤로"));
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
			Card->OnSaveSlotDeleteRequested.AddUniqueDynamic(this, &UFF_SaveListScreen::HandleSlotDeleteRequested);
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
	/* 알리기만 한다. 여기서 레벨을 열지 않는다.

	   불러오기는 레벨 전환을 동반한다. 화면이 자기를 없애는 전환을 스스로 시작하면
	   전환 도중 이 위젯이 사라져 수명이 꼬인다. 흐름 쪽이 받아서 연다.
	*/
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_SaveListScreen: 슬롯 `%s` 선택."), *SlotId.ToString());

	OnSlotChosen.Broadcast(SlotId);
}

void UFF_SaveListScreen::HandleSlotDeleteRequested(FName SlotId)
{
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_SaveListScreen: 슬롯 `%s` 삭제 요청."), *SlotId.ToString());

	// 지워지면 공급자가 목록을 다시 읽어 OnSaveListChanged 로 알려온다. 여기서 다시 그리지 않는다.
	if (UFF_SaveListProvider* Provider = UFF_SaveListProvider::Get(this))
	{
		Provider->DeleteSlot(SlotId);
	}
}

void UFF_SaveListScreen::HandleBackClicked(UFF_CommonButtonBase* Button)
{
	OnBackRequested.Broadcast();
}

#undef LOCTEXT_NAMESPACE
