// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_SaveSlotCard.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_SaveSlotCard::UFF_SaveSlotCard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 프론트엔드 화면에 올라가므로 Glass Orbit 팔레트를 쓴다.
	bUseFrontendPalette = true;
}

void UFF_SaveSlotCard::SetEntry(const FFF_SaveSlotEntry& InEntry)
{
	CachedSlotId = InEntry.SlotId;

	if (Text_FestivalName)
	{
		Text_FestivalName->SetText(InEntry.FestivalName);
	}

	if (Text_Region)
	{
		Text_Region->SetText(InEntry.RegionName);
		// 값이 없으면 칸을 숨긴다. `-` 를 넣지 않는다.
		Text_Region->SetVisibility(
			InEntry.RegionName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (Text_DayStage)
	{
		Text_DayStage->SetText(FText::Format(
			LOCTEXT("SaveSlotDayStage", "Day {0} · {1}"),
			FText::AsNumber(InEntry.Day),
			InEntry.StageName));
	}

	if (Text_SavedAt)
	{
		// 날짜는 문화권 서식을 따른다. 문자열을 직접 조립하지 않는다.
		Text_SavedAt->SetText(FText::AsDateTime(InEntry.SavedAt));
	}

	// 자동/수동은 문구가 주된 표현이다. 색은 보조다.
	if (Text_Kind)
	{
		Text_Kind->SetText(InEntry.bAutoSave
			? LOCTEXT("SaveSlotAuto", "자동 저장")
			: LOCTEXT("SaveSlotManual", "수동 저장"));
	}

	if (Image_KindIcon)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (Palette)
		{
			// 아이콘 텍스처는 아직 없다. 틴트만 넣어 두고 나중에 실루엣이 다른 것으로 교체한다.
			Image_KindIcon->SetColorAndOpacity(
				InEntry.bAutoSave ? Palette->Frontend.Sub : Palette->Frontend.Title);
		}
	}
}

FReply UFF_SaveSlotCard::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CachedSlotId.IsNone())
	{
		// 내용이 안 채워진 카드가 눌렸다. 조용히 넘어가면 왜 반응이 없는지 알 수 없다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_SaveSlotCard: SlotId 가 없는 카드가 눌렸다. SetEntry 를 먼저 부른다."));
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	OnSaveSlotSelected.Broadcast(CachedSlotId);

	// 처리했음을 알리고 포커스를 이 카드로 가져온다. 그래야 링이 켜진다.
	return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

#undef LOCTEXT_NAMESPACE
