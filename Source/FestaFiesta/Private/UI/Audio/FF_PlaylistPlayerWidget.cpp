// FF_PlaylistPlayerWidget.cpp
// 역할: 축제 플레이리스트 미니 플레이어의 표시와 조작을 오디오 서브시스템에 잇는다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Audio/FF_PlaylistPlayerWidget.h"

#include "Audio/FF_AudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPlaylistPlayer"

namespace
{
	FText FormatClock(const float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
	}

	void BindClick(UButton* Button, UObject* Object, const FName FunctionName)
	{
		if (Button)
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Object, FunctionName);
			Button->OnClicked.AddUnique(Delegate);
		}
	}

	void UnbindClick(UButton* Button, UObject* Object)
	{
		if (Button)
		{
			Button->OnClicked.RemoveAll(Object);
		}
	}
}

void UFF_PlaylistTrackRowProxy::HandleClicked()
{
	if (UFF_PlaylistPlayerWidget* Widget = Owner.Get())
	{
		Widget->HandleTrackRowClicked(TrackIndex);
	}
}

void UFF_PlaylistPlayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 루트는 화면 전체에 깔린다. 플레이어 밖의 클릭은 그대로 아래(게임·HUD)로 흘려보낸다.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	BindClick(Button_Prev, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandlePrevClicked));
	BindClick(Button_PlayPause, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandlePlayPauseClicked));
	BindClick(Button_Next, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleNextClicked));
	BindClick(Button_ToggleList, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleToggleListClicked));
	BindClick(Button_AutoAdvance, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleAutoAdvanceClicked));
	BindClick(Button_Shuffle, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleShuffleClicked));

	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->OnPlaylistChanged.AddDynamic(this, &UFF_PlaylistPlayerWidget::HandlePlaylistChanged);
		bBoundAudio = true;
	}

	// 처음에는 접어 둔다. 플레이어는 화면 한쪽에 작게 있어야 운영 화면을 가리지 않는다.
	bShown = false;
	if (Panel_Player)
	{
		Panel_Player->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetListExpanded(false);
	RefreshAll();
}

void UFF_PlaylistPlayerWidget::NativeDestruct()
{
	UnbindClick(Button_Prev, this);
	UnbindClick(Button_PlayPause, this);
	UnbindClick(Button_Next, this);
	UnbindClick(Button_ToggleList, this);
	UnbindClick(Button_AutoAdvance, this);
	UnbindClick(Button_Shuffle, this);

	if (bBoundAudio)
	{
		if (UFF_AudioSubsystem* Audio = GetAudio())
		{
			Audio->OnPlaylistChanged.RemoveDynamic(this, &UFF_PlaylistPlayerWidget::HandlePlaylistChanged);
		}
		bBoundAudio = false;
	}

	for (UButton* Row : TrackRowButtons)
	{
		if (Row)
		{
			Row->OnClicked.Clear();
		}
	}
	TrackRowButtons.Reset();
	TrackRowTexts.Reset();
	TrackRowProxies.Reset();

	Super::NativeDestruct();
}

void UFF_PlaylistPlayerWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 보일지는 매 프레임 본다. 튜토리얼 대사는 이 위젯에 신호를 주지 않는다.
	const bool bShouldShow = ShouldShowPlayer();
	if (bShouldShow != bShown)
	{
		bShown = bShouldShow;
		if (Panel_Player)
		{
			Panel_Player->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (bShown)
		{
			RefreshAll();
		}
	}

	if (bShown)
	{
		RefreshProgress();
	}
}

UFF_AudioSubsystem* UFF_PlaylistPlayerWidget::GetAudio() const
{
	return UFF_AudioSubsystem::Get(this);
}

bool UFF_PlaylistPlayerWidget::ShouldShowPlayer() const
{
	const UFF_AudioSubsystem* Audio = GetAudio();
	if (!Audio || !Audio->IsPlaylistActive() || Audio->GetPlaylistTrackCount() <= 0)
	{
		return false;
	}

	// 튜토리얼 대사 중에는 숨는다. 대사창·강조와 겹치면 무엇을 봐야 할지 헷갈린다.
	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	return !(Tutorial && Tutorial->IsDialogueActive());
}

void UFF_PlaylistPlayerWidget::SetListExpanded(const bool bExpanded)
{
	bListExpanded = bExpanded;
	if (Panel_List)
	{
		Panel_List->SetVisibility(bExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Text_ToggleList)
	{
		Text_ToggleList->SetText(bExpanded
			? LOCTEXT("ListClose", "닫기")
			: LOCTEXT("ListOpen", "목록"));
	}
	if (bExpanded)
	{
		RebuildTrackRows();
		RefreshRowHighlight();
	}
}

void UFF_PlaylistPlayerWidget::HandlePlaylistChanged()
{
	RefreshAll();
}

void UFF_PlaylistPlayerWidget::RefreshAll()
{
	const UFF_AudioSubsystem* Audio = GetAudio();
	if (!Audio)
	{
		return;
	}

	const int32 Current = Audio->GetCurrentPlaylistTrack();
	if (Text_TrackTitle)
	{
		const FText Title = Current == INDEX_NONE
			? LOCTEXT("NoTrack", "재생 중인 곡 없음")
			: Audio->GetPlaylistTrackDisplayName(Current);
		Text_TrackTitle->SetText(Title);
		Text_TrackTitle->SetToolTipText(Title);
	}

	// 재생 중이면 멈춤 기호, 멈춰 있으면 재생 기호. 누르면 일어날 일을 보여준다.
	if (Text_PlayPause)
	{
		Text_PlayPause->SetText(Audio->IsPlaylistPaused()
			? LOCTEXT("PlayGlyph", "▶")
			: LOCTEXT("PauseGlyph", "||"));
	}
	if (Button_PlayPause)
	{
		Button_PlayPause->SetToolTipText(Audio->IsPlaylistPaused()
			? LOCTEXT("PlayTip", "재생")
			: LOCTEXT("PauseTip", "일시정지"));
	}

	EnsureRowStyles();
	const bool bAuto = Audio->IsPlaylistAutoAdvance();
	const bool bShuffle = Audio->IsPlaylistShuffle();
	if (Text_AutoAdvance)
	{
		Text_AutoAdvance->SetText(bAuto
			? LOCTEXT("AutoOn", "자동 다음 곡 켬")
			: LOCTEXT("AutoOff", "한 곡 반복"));
	}
	if (Button_AutoAdvance && bRowStylesReady)
	{
		Button_AutoAdvance->SetStyle(bAuto ? ToggleOnStyle : ToggleOffStyle);
	}
	if (Text_Shuffle)
	{
		Text_Shuffle->SetText(bShuffle
			? LOCTEXT("ShuffleOn", "셔플 켬")
			: LOCTEXT("ShuffleOff", "셔플 끔"));
	}
	if (Button_Shuffle && bRowStylesReady)
	{
		Button_Shuffle->SetStyle(bShuffle ? ToggleOnStyle : ToggleOffStyle);
	}

	// 위치 글자는 곡이 바뀌면 바로 다시 쓴다.
	LastShownSecond = INDEX_NONE;
	RefreshProgress();

	if (bListExpanded)
	{
		RebuildTrackRows();
		RefreshRowHighlight();
	}

	BP_OnPlaylistRefreshed();
}

void UFF_PlaylistPlayerWidget::RefreshProgress()
{
	const UFF_AudioSubsystem* Audio = GetAudio();
	if (!Audio)
	{
		return;
	}

	const float Position = Audio->GetPlaylistPlaybackSeconds();
	const float Length = Audio->GetPlaylistTrackLength();

	if (ProgressBar_Track)
	{
		ProgressBar_Track->SetPercent(Length > 0.f ? FMath::Clamp(Position / Length, 0.f, 1.f) : 0.f);
	}

	const int32 Second = FMath::FloorToInt(Position);
	if (Text_TrackMeta && Second != LastShownSecond)
	{
		LastShownSecond = Second;
		const int32 Current = Audio->GetCurrentPlaylistTrack();
		const FText Index = Current == INDEX_NONE
			? FText::FromString(TEXT("-"))
			: FText::AsNumber(Current + 1);
		const FText Clock = Length > 0.f
			? FText::Format(LOCTEXT("ClockWithLength", "{0} / {1}"), FormatClock(Position), FormatClock(Length))
			: FormatClock(Position);
		Text_TrackMeta->SetText(FText::Format(LOCTEXT("MetaFormat", "{0} / {1}  ·  {2}"),
			Index, FText::AsNumber(Audio->GetPlaylistTrackCount()), Clock));
	}
}

void UFF_PlaylistPlayerWidget::EnsureRowStyles()
{
	if (bRowStylesReady)
	{
		return;
	}
	bRowStylesReady = true;

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return;
	}

	/* 일시정지·설정 화면과 같은 LightTeal 팔레트를 단색 둥근 면으로 쓴다.

	   그쪽 9-slice 버튼 텍스처는 큰 메뉴 버튼 크기에 맞춰져 있어, 목록 줄처럼 작은 버튼에서는
	   테두리가 두껍게 늘어나 깨져 보였다.
	*/
	const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);
	auto Rounded = [](const FLinearColor& Fill, const float Radius, const FLinearColor& Outline, const float Width)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		return Brush;
	};

	RowStyle.Normal = Rounded(FLinearColor::Transparent, 6.f, FLinearColor::Transparent, 0.f);
	RowStyle.Hovered = Rounded(Tokens.Hover, 6.f, Tokens.Line, 1.f);
	RowStyle.Pressed = Rounded(Tokens.TealSoft, 6.f, FLinearColor::Transparent, 0.f);
	RowStyle.Disabled = Rounded(Tokens.Disabled, 6.f, FLinearColor::Transparent, 0.f);
	RowStyle.NormalPadding = TrackRowPadding;
	RowStyle.PressedPadding = TrackRowPadding;

	CurrentRowStyle = RowStyle;
	CurrentRowStyle.Normal = Rounded(Tokens.TealSoft, 6.f, Tokens.Teal, 1.f);
	CurrentRowStyle.Hovered = CurrentRowStyle.Normal;

	// 자동 다음 곡·셔플 버튼. 켜진 쪽은 청록으로 채워 상태가 한눈에 보이게 한다.
	ToggleOffStyle = RowStyle;
	ToggleOffStyle.Normal = Rounded(Tokens.PaperSunken, 6.f, Tokens.Line, 1.f);
	ToggleOffStyle.NormalPadding = FMargin(10.f, 5.f);
	ToggleOffStyle.PressedPadding = ToggleOffStyle.NormalPadding;
	ToggleOnStyle = ToggleOffStyle;
	ToggleOnStyle.Normal = Rounded(Tokens.TealSoft, 6.f, Tokens.Teal, 1.f);
	ToggleOnStyle.Hovered = Rounded(Tokens.TealSoft, 6.f, Tokens.TealDeep, 1.f);

	RowTextColor = Tokens.Ink;
	CurrentRowTextColor = Tokens.TealDeep;
}

void UFF_PlaylistPlayerWidget::RebuildTrackRows()
{
	const UFF_AudioSubsystem* Audio = GetAudio();
	if (!Audio || !ScrollBox_Tracks || !WidgetTree)
	{
		return;
	}

	const int32 Count = Audio->GetPlaylistTrackCount();
	if (Count == BuiltTrackCount)
	{
		// 곡 이름은 뱅크가 바뀌지 않는 한 그대로다. 강조만 다시 한다.
		return;
	}
	BuiltTrackCount = Count;

	for (UButton* Row : TrackRowButtons)
	{
		if (Row)
		{
			Row->OnClicked.Clear();
		}
	}
	ScrollBox_Tracks->ClearChildren();
	TrackRowButtons.Reset();
	TrackRowTexts.Reset();
	TrackRowProxies.Reset();
	HighlightedTrack = INDEX_NONE;

	EnsureRowStyles();

	// 줄 글꼴은 곡 제목 글꼴을 따른다. WBP 에서 바꾼 글꼴이 목록에도 먹게 하려는 것이다.
	FSlateFontInfo RowFont = Text_TrackTitle ? Text_TrackTitle->GetFont() : FSlateFontInfo();
	RowFont.Size = TrackRowFontSize;
	RowFont.TypefaceFontName = TEXT("Medium");

	for (int32 TrackIndex = 0; TrackIndex < Count; ++TrackIndex)
	{
		UButton* Row = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		UFF_PlaylistTrackRowProxy* Proxy = NewObject<UFF_PlaylistTrackRowProxy>(this);
		if (!Row || !Label || !Proxy)
		{
			continue;
		}

		const FText Name = Audio->GetPlaylistTrackDisplayName(TrackIndex);
		Label->SetText(FText::Format(LOCTEXT("RowFormat", "{0}.  {1}"), FText::AsNumber(TrackIndex + 1), Name));
		Label->SetToolTipText(Name);
		Label->SetFont(RowFont);
		Label->SetColorAndOpacity(FSlateColor(RowTextColor));
		Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Label->SetClipping(EWidgetClipping::ClipToBounds);

		Row->SetStyle(RowStyle);
		Row->SetClickMethod(EButtonClickMethod::PreciseClick);
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Row->AddChild(Label)))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(0.f));
		}

		Proxy->TrackIndex = TrackIndex;
		Proxy->Owner = this;
		Row->OnClicked.AddDynamic(Proxy, &UFF_PlaylistTrackRowProxy::HandleClicked);

		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(ScrollBox_Tracks->AddChild(Row)))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		TrackRowButtons.Add(Row);
		TrackRowTexts.Add(Label);
		TrackRowProxies.Add(Proxy);
	}
}

void UFF_PlaylistPlayerWidget::RefreshRowHighlight()
{
	const UFF_AudioSubsystem* Audio = GetAudio();
	const int32 Current = Audio ? Audio->GetCurrentPlaylistTrack() : INDEX_NONE;
	if (Current == HighlightedTrack || !bRowStylesReady)
	{
		return;
	}

	auto ApplyRow = [this](const int32 Index, const bool bCurrent)
	{
		if (!TrackRowButtons.IsValidIndex(Index))
		{
			return;
		}
		if (UButton* Row = TrackRowButtons[Index])
		{
			Row->SetStyle(bCurrent ? CurrentRowStyle : RowStyle);
		}
		if (UTextBlock* Label = TrackRowTexts.IsValidIndex(Index) ? TrackRowTexts[Index].Get() : nullptr)
		{
			Label->SetColorAndOpacity(FSlateColor(bCurrent ? CurrentRowTextColor : RowTextColor));
			FSlateFontInfo Font = Label->GetFont();
			Font.TypefaceFontName = bCurrent ? FName(TEXT("SemiBold")) : FName(TEXT("Medium"));
			Label->SetFont(Font);
		}
	};

	ApplyRow(HighlightedTrack, false);
	ApplyRow(Current, true);
	HighlightedTrack = Current;

	// 지금 곡이 보이게 목록을 맞춘다.
	if (ScrollBox_Tracks && TrackRowButtons.IsValidIndex(Current) && TrackRowButtons[Current])
	{
		ScrollBox_Tracks->ScrollWidgetIntoView(TrackRowButtons[Current], false, EDescendantScrollDestination::IntoView);
	}
}

void UFF_PlaylistPlayerWidget::HandleTrackRowClicked(const int32 TrackIndex)
{
	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PlayPlaylistTrack(TrackIndex);
	}
}

void UFF_PlaylistPlayerWidget::HandlePrevClicked()
{
	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PreviousPlaylistTrack();
	}
}

void UFF_PlaylistPlayerWidget::HandlePlayPauseClicked()
{
	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->SetPlaylistPaused(!Audio->IsPlaylistPaused());
	}
}

void UFF_PlaylistPlayerWidget::HandleNextClicked()
{
	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->SkipPlaylistTrack();
	}
}

void UFF_PlaylistPlayerWidget::HandleToggleListClicked()
{
	SetListExpanded(!bListExpanded);
}

void UFF_PlaylistPlayerWidget::HandleAutoAdvanceClicked()
{
	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->SetPlaylistAutoAdvance(!Audio->IsPlaylistAutoAdvance());
	}
}

void UFF_PlaylistPlayerWidget::HandleShuffleClicked()
{
	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->SetPlaylistShuffle(!Audio->IsPlaylistShuffle());
	}
}

#undef LOCTEXT_NAMESPACE
