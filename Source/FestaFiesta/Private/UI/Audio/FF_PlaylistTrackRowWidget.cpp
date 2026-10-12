#include "UI/Audio/FF_PlaylistTrackRowWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPlaylistTrackRow"

UFF_PlaylistTrackRowWidget::UFF_PlaylistTrackRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const auto Color = [](uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R, G, B)); };
	NormalBackgroundColor = FLinearColor::Transparent;
	HoverBackgroundColor = Color(234, 245, 250);
	CurrentBackgroundColor = Color(222, 244, 234);
	NormalTextColor = Color(23, 66, 55);
	CurrentTextColor = Color(23, 66, 55);
	CurrentTitleTypeface = TEXT("SemiBold");
	PlayingText = LOCTEXT("Playing", "재생 중");
	PausedText = LOCTEXT("Paused", "일시정지");
	PlayTooltipFormat = LOCTEXT("PlayTip", "{Title} — 눌러서 재생");
	CurrentTooltipFormat = LOCTEXT("CurrentTip", "{Title} — {State}");
}

void UFF_PlaylistTrackRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Track)
	{
		Button_Track->SetClickMethod(EButtonClickMethod::PreciseClick);
		Button_Track->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
		Button_Track->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleHovered);
		Button_Track->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleUnhovered);
	}
	Refresh();
}

void UFF_PlaylistTrackRowWidget::NativeDestruct()
{
	if (Button_Track)
	{
		Button_Track->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
		Button_Track->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
		Button_Track->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleUnhovered);
	}
	bHovered = bFocused = false;
	Super::NativeDestruct();
}

void UFF_PlaylistTrackRowWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	// 마우스로 눌러 생긴 포커스는 호버로 이미 보였다. 남겨 두면 손을 뗀 뒤에도 강조가 남는다.
	bFocused = InFocusEvent.GetCause() != EFocusCause::Mouse;
	Refresh();
}

void UFF_PlaylistTrackRowWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	bFocused = false;
	Refresh();
}

void UFF_PlaylistTrackRowWidget::SetTrack(int32 InIndex, const FText& InTitle, bool bCurrent, bool bPaused)
{
	TrackIndex = InIndex;
	TrackTitle = InTitle;
	SetCurrent(bCurrent, bPaused);
}

void UFF_PlaylistTrackRowWidget::SetCurrent(bool bCurrent, bool bPaused)
{
	bIsCurrent = bCurrent;
	bIsPaused = bPaused;
	Refresh();
}

void UFF_PlaylistTrackRowWidget::HandleHovered() { bHovered = true; Refresh(); }
void UFF_PlaylistTrackRowWidget::HandleUnhovered() { bHovered = false; Refresh(); }

void UFF_PlaylistTrackRowWidget::Refresh()
{
	// 비활성(음악 없음) 행은 눌러도 소용없으니 호버 표시도 하지 않는다.
	const bool bPointed = (bHovered || bFocused) && GetIsEnabled();
	const auto Show = [](UWidget* Widget, bool bVisible)
	{
		// 숨겨도 자리는 남긴다. 상태가 바뀌어도 제목 위치·폭이 그대로다.
		if (Widget) { Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden); }
	};
	if (Border_Row)
	{
		Border_Row->SetBrushColor(bIsCurrent ? CurrentBackgroundColor : bPointed ? HoverBackgroundColor : NormalBackgroundColor);
	}
	Show(Image_Accent, bIsCurrent);
	Show(Image_Equalizer, bIsCurrent);
	Show(Image_PlayHint, !bIsCurrent && bPointed);
	Show(Image_Divider, !bIsCurrent && !bPointed);
	if (Text_Index)
	{
		Text_Index->SetText(FText::FromString(FString::Printf(TEXT("%02d"), TrackIndex + 1)));
		Show(Text_Index, !bIsCurrent);
	}
	const FText StateText = bIsPaused ? PausedText : PlayingText;
	if (Text_Title)
	{
		Text_Title->SetText(TrackTitle);
		Text_Title->SetColorAndOpacity(bIsCurrent ? CurrentTextColor : NormalTextColor);
		if (!CurrentTitleTypeface.IsNone())
		{
			if (!bCapturedTitleTypeface)
			{
				DesignerTitleTypeface = Text_Title->GetFont().TypefaceFontName;
				bCapturedTitleTypeface = true;
			}
			FSlateFontInfo Font = Text_Title->GetFont();
			const FName Wanted = bIsCurrent ? CurrentTitleTypeface : DesignerTitleTypeface;
			if (Font.TypefaceFontName != Wanted)
			{
				Font.TypefaceFontName = Wanted;
				Text_Title->SetFont(Font);
			}
		}
	}
	if (Text_State)
	{
		Text_State->SetText(StateText);
		Show(Text_State, bIsCurrent);
	}
	const FText Tooltip = bIsCurrent
		? FText::Format(CurrentTooltipFormat, FFormatNamedArguments{{TEXT("Title"), TrackTitle}, {TEXT("State"), StateText}})
		: FText::Format(PlayTooltipFormat, FFormatNamedArguments{{TEXT("Title"), TrackTitle}});
	if (Button_Track) { Button_Track->SetToolTipText(Tooltip); }
}

void UFF_PlaylistTrackRowWidget::HandleClicked()
{
	if (TrackIndex != INDEX_NONE) { OnTrackChosen.Broadcast(TrackIndex); }
}

#undef LOCTEXT_NAMESPACE
