// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_SaveSlotCard.h"

#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_SaveSlotCard::UFF_SaveSlotCard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 프론트엔드 화면에 올라가므로 Glass Orbit 팔레트를 쓴다.
	bUseFrontendPalette = true;
}

// 삭제 버튼 모양 도우미. 이름 있는 네임스페이스로 둔다. 유니티 빌드에서 익명 네임스페이스끼리 이름이 부딪힌 적이 있다.
namespace FFSaveSlotCardStyle
{
	// 모서리가 둥근 판. 반경을 크게 주면 알약 모양이 된다.
	FSlateBrush MakePill(const FLinearColor& Fill, const FLinearColor& Outline, const float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(12.f, 12.f, 12.f, 12.f), FSlateColor(Outline), OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	// 같은 색을 옅게. 붉은 확인 상태의 바탕에 쓴다.
	FLinearColor WithAlpha(FLinearColor Color, const float Alpha)
	{
		Color.A = Alpha;
		return Color;
	}
}

void UFF_SaveSlotCard::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyDeleteButtonStyle(false);
}

void UFF_SaveSlotCard::NativeConstruct()
{
	Super::NativeConstruct();

	// 카드는 목록을 다시 만들 때마다 새로 생기지만, 풀에서 재사용될 수도 있어 여기서 구독한다.
	if (Button_Delete)
	{
		Button_Delete->OnClicked.AddUniqueDynamic(this, &UFF_SaveSlotCard::HandleDeleteClicked);
	}
	SetDeleteArmed(false);
}

void UFF_SaveSlotCard::NativeDestruct()
{
	if (Button_Delete)
	{
		Button_Delete->OnClicked.RemoveDynamic(this, &UFF_SaveSlotCard::HandleDeleteClicked);
	}
	Super::NativeDestruct();
}

void UFF_SaveSlotCard::SetEntry(const FFF_SaveSlotEntry& InEntry)
{
	CachedSlotId = InEntry.SlotId;

	// 다른 기록을 담으면 확인 상태를 푼다. 앞 카드에서 눌러 둔 확인이 넘어오면 안 된다.
	SetDeleteArmed(false);

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
		// 로비 기록은 일차가 없다. `Day 0` 을 보여주지 않고 상태 문구만 쓴다.
		Text_DayStage->SetText(InEntry.bAtLobby
			? InEntry.StageName
			: FText::Format(
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

void UFF_SaveSlotCard::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	SetDeleteArmed(false);
}

void UFF_SaveSlotCard::HandleDeleteClicked()
{
	if (CachedSlotId.IsNone())
	{
		return;
	}

	if (!bDeleteArmed)
	{
		SetDeleteArmed(true);
		return;
	}

	// 알리기만 한다. 지우면 목록이 통째로 다시 만들어져 이 카드도 사라진다.
	SetDeleteArmed(false);
	OnSaveSlotDeleteRequested.Broadcast(CachedSlotId);
}

void UFF_SaveSlotCard::SetDeleteArmed(const bool bArmed)
{
	bDeleteArmed = bArmed;
	ApplyDeleteButtonStyle(bArmed);

	if (UTextBlock* Label = ResolveDeleteLabel())
	{
		Label->SetText(bArmed
			? LOCTEXT("SaveSlotDeleteConfirm", "한 번 더 누르면 삭제")
			: LOCTEXT("SaveSlotDelete", "삭제"));
	}
}

void UFF_SaveSlotCard::ApplyDeleteButtonStyle(const bool bArmed)
{
	using namespace FFSaveSlotCardStyle;

	if (!Button_Delete)
	{
		return;
	}

	// 팔레트가 없으면(에디터 초기화 전 등) 기본값 팔레트를 쓴다. 회색 기본 버튼으로 남는 것보다 낫다.
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	const FFF_UIPaletteInGame Colors = Palette ? Palette->InGame : FFF_UIPaletteInGame();

	FButtonStyle Style = Button_Delete->GetStyle();
	if (bArmed)
	{
		Style.SetNormal(MakePill(WithAlpha(Colors.Danger, 0.12f), Colors.Danger, 1.f));
		Style.SetHovered(MakePill(WithAlpha(Colors.Danger, 0.2f), Colors.Danger, 1.f));
		Style.SetPressed(MakePill(WithAlpha(Colors.Danger, 0.28f), Colors.Danger, 1.f));
	}
	else
	{
		// 평소에는 눈에 덜 띄게 둔다. 카드의 주인공은 축제 이름이고, 삭제는 가끔 쓰는 동작이다.
		Style.SetNormal(MakePill(FLinearColor::Transparent, Colors.Line, 1.f));
		Style.SetHovered(MakePill(Colors.Hover, Colors.LineStrong, 1.f));
		Style.SetPressed(MakePill(Colors.PaperSunken, Colors.LineStrong, 1.f));
	}
	Style.SetNormalPadding(FMargin(12.f, 3.f));
	Style.SetPressedPadding(FMargin(12.f, 4.f, 12.f, 2.f));
	Button_Delete->SetStyle(Style);

	// 버튼 배경색은 판 색에 곱해진다. 흰색으로 둬야 위 색이 그대로 나온다.
	Button_Delete->SetBackgroundColor(FLinearColor::White);

	/* 버튼이 글자 크기만큼만 차지하게 한다.

	   세로 칸에 채우기(Fill)로 들어가 있으면 버튼 판이 칸 폭 전체로 늘어나고 글자는 한쪽에
	   붙어서, 글자와 판이 따로 노는 모양이 된다. 위치는 WBP 가 정하지만 이 정렬만큼은
	   코드에서 맞춘다. 카드를 새로 배치해도 삭제 버튼이 늘어나는 일이 없게 하려는 것이다.
	   왼쪽 정렬은 같은 칸의 `자동 저장` 글자와 줄을 맞추려는 것이다.
	*/
	if (UVerticalBoxSlot* ColumnSlot = Cast<UVerticalBoxSlot>(Button_Delete->Slot))
	{
		ColumnSlot->SetHorizontalAlignment(HAlign_Left);
	}
	else if (UHorizontalBoxSlot* RowSlot = Cast<UHorizontalBoxSlot>(Button_Delete->Slot))
	{
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UTextBlock* Label = ResolveDeleteLabel())
	{
		// 글자는 판 한가운데. 여백은 버튼 쪽(NormalPadding)에서만 준다. 두 군데서 주면 판이 뚱뚱해진다.
		Label->SetJustification(ETextJustify::Center);
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Label->Slot))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(0.f));
		}

		Label->SetColorAndOpacity(FSlateColor(bArmed ? Colors.Danger : Colors.InkMuted));

		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 14;
		Label->SetFont(Font);
	}
}

UTextBlock* UFF_SaveSlotCard::ResolveDeleteLabel() const
{
	if (Text_Delete)
	{
		return Text_Delete;
	}
	return Button_Delete ? Cast<UTextBlock>(Button_Delete->GetContent()) : nullptr;
}

#undef LOCTEXT_NAMESPACE
