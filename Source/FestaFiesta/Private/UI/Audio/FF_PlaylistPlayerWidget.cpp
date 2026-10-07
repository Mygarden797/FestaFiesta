// FF_PlaylistPlayerWidget.cpp
// 역할: 축제 플레이리스트 미니 플레이어의 표시와 조작을 오디오 서브시스템에 잇는다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Audio/FF_PlaylistPlayerWidget.h"

#include "Audio/FF_AudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Audio/FF_PlaylistTrackRowWidget.h"

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

UFF_PlaylistPlayerWidget::UFF_PlaylistPlayerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ToggleActiveFill = FLinearColor::FromSRGBColor(FColor(227, 245, 250));
	ToggleActiveOutline = FLinearColor::FromSRGBColor(FColor(100, 177, 210));
	PlayingText = LOCTEXT("PlayingState", "지금 재생 중");
	PausedText = LOCTEXT("PausedState", "일시정지");
	NoTrackText = LOCTEXT("NoTrack", "재생 중인 곡 없음");
	AutoAdvanceOnText = LOCTEXT("AutoOn", "자동 다음 곡 켬");
	AutoAdvanceOffText = LOCTEXT("AutoOff", "한 곡 반복");
	ShuffleOnText = LOCTEXT("ShuffleOn", "셔플 켬");
	ShuffleOffText = LOCTEXT("ShuffleOff", "셔플 끔");
	ToggleActiveIconColor = FLinearColor::FromSRGBColor(FColor(25, 139, 91));
	ToggleInactiveIconColor = FLinearColor::FromSRGBColor(FColor(120, 143, 136));
	ShuffleOnTooltip = LOCTEXT("ShuffleOnTip", "셔플 켬 · 곡 순서를 섞어 재생합니다. 누르면 끕니다.");
	ShuffleOffTooltip = LOCTEXT("ShuffleOffTip", "셔플 끔 · 목록 순서대로 재생합니다. 누르면 켭니다.");
	RepeatOneTooltip = LOCTEXT("RepeatOneTip", "한 곡 반복 · 지금 곡을 계속 재생합니다. 누르면 자동 다음 곡으로 바꿉니다.");
	AutoAdvanceTooltip = LOCTEXT("AutoAdvanceTip", "자동 다음 곡 · 곡이 끝나면 다음 곡으로 넘어갑니다. 누르면 한 곡 반복으로 바꿉니다.");
	PlayTooltip = LOCTEXT("PlayTip", "재생");
	PauseTooltip = LOCTEXT("PauseTip", "일시정지");
	ListCountFormat = LOCTEXT("ListCount", "{Count}곡");
}

void UFF_PlaylistPlayerWidget::ConfigureFlyoutDefaults(TSubclassOf<UFF_PlaylistTrackRowWidget> InRowClass)
{
	bHostedByHud = true;
	TrackRowWidgetClass = InRowClass;
}

void UFF_PlaylistPlayerWidget::ConfigureIconToggleDefaults()
{
	bShowToggleStateByButtonStyle = false;
	// 곡 행이 아래 구분선을 가진다. 스크롤바 여백은 Scroll Style 이 따로 잡는다.
	TrackRowSlotPadding = FMargin(0.f);
}

void UFF_PlaylistPlayerWidget::SetStandaloneSuppressed(bool bSuppressed)
{
	bStandaloneSuppressed = bSuppressed;
	if (!bHostedByHud && Panel_Player)
	{
		bShown = ShouldShowPlayer();
		Panel_Player->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFF_PlaylistPlayerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ScrollStyle.ApplyTo(WidgetTree);
}

void UFF_PlaylistPlayerWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime()) { ScrollStyle.ApplyTo(WidgetTree); }
}

FReply UFF_PlaylistPlayerWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply Reply = Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	return Reply.IsEventHandled() ? Reply : FReply::Handled();
}

void UFF_PlaylistPlayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 루트는 화면 전체에 깔린다. 플레이어 밖의 클릭은 그대로 아래(게임·HUD)로 흘려보낸다.
	if (!bHostedByHud) { SetVisibility(ESlateVisibility::SelfHitTestInvisible); }

	BindClick(Button_Prev, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandlePrevClicked));
	BindClick(Button_PlayPause, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandlePlayPauseClicked));
	BindClick(Button_Next, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleNextClicked));
	BindClick(Button_ToggleList, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleToggleListClicked));
	BindClick(Button_AutoAdvance, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleAutoAdvanceClicked));
	BindClick(Button_Shuffle, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleShuffleClicked));
	BindClick(Button_Close, this, GET_FUNCTION_NAME_CHECKED(UFF_PlaylistPlayerWidget, HandleCloseClicked));

	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->OnPlaylistChanged.AddUniqueDynamic(this, &UFF_PlaylistPlayerWidget::HandlePlaylistChanged);
		bBoundAudio = true;
	}

	// 처음에는 접어 둔다. 플레이어는 화면 한쪽에 작게 있어야 운영 화면을 가리지 않는다.
	bShown = false;
	if (Panel_Player)
	{
		Panel_Player->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetListExpanded(bHostedByHud);
	RefreshAll();
	// 펼침 호스트는 접힌 인스턴스를 다시 쓴다. 첫 Tick 전에도 본문이 준비되어 있어야 한다.
	bShown = ShouldShowPlayer();
	if (Panel_Player) { Panel_Player->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
}

void UFF_PlaylistPlayerWidget::NativeDestruct()
{
	UnbindClick(Button_Prev, this);
	UnbindClick(Button_PlayPause, this);
	UnbindClick(Button_Next, this);
	UnbindClick(Button_ToggleList, this);
	UnbindClick(Button_AutoAdvance, this);
	UnbindClick(Button_Shuffle, this);
	UnbindClick(Button_Close, this);

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
	for (UFF_PlaylistTrackRowWidget* Row : TrackRowWidgets)
	{
		if (Row) { Row->OnTrackChosen.RemoveAll(this); }
	}
	TrackRowWidgets.Reset();
	BuiltTrackCount = INDEX_NONE;
	BuiltTrackNames.Reset();

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
		if (IsPlaylistAvailable() != bLastCanControl) { RefreshAll(); }
		RefreshProgress();
		RefreshControlHint();
	}
}

void UFF_PlaylistPlayerWidget::RefreshToggleIcons(const bool bAutoAdvance, const bool bShuffle)
{
	const auto Show = [](UWidget* Widget, const bool bVisible, const bool bKeepSpace)
	{
		if (Widget)
		{
			Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible
				: bKeepSpace ? ESlateVisibility::Hidden : ESlateVisibility::Collapsed);
		}
	};
	// 한 곡 반복이 '켜진' 상태다. 자동 다음 곡은 기본 흐름이라 꺼진 색으로 둔다.
	const bool bRepeatOne = !bAutoAdvance;
	if (Image_Shuffle) { Image_Shuffle->SetColorAndOpacity(bShuffle ? ToggleActiveIconColor : ToggleInactiveIconColor); }
	if (Image_Repeat) { Image_Repeat->SetColorAndOpacity(ToggleInactiveIconColor); }
	if (Image_RepeatOne) { Image_RepeatOne->SetColorAndOpacity(ToggleActiveIconColor); }
	// 두 반복 아이콘은 같은 칸에 겹쳐 있다. 안 쓰는 쪽은 접는다.
	Show(Image_Repeat, !bRepeatOne || !Image_RepeatOne, false);
	Show(Image_RepeatOne, bRepeatOne, false);
	Show(Dot_Shuffle, bShuffle, true);
	Show(Dot_Repeat, bRepeatOne, true);
	if (Button_Shuffle) { Button_Shuffle->SetToolTipText(bShuffle ? ShuffleOnTooltip : ShuffleOffTooltip); }
	if (Button_AutoAdvance) { Button_AutoAdvance->SetToolTipText(bRepeatOne ? RepeatOneTooltip : AutoAdvanceTooltip); }
}

void UFF_PlaylistPlayerWidget::RefreshControlHint()
{
	if (!Text_ControlHint) { return; }
	FText Hint;
	for (UButton* Control : { Button_Shuffle.Get(), Button_Prev.Get(), Button_PlayPause.Get(), Button_Next.Get(), Button_AutoAdvance.Get() })
	{
		const TSharedPtr<SWidget> Slate = Control ? Control->GetCachedWidget() : nullptr;
		const TOptional<EFocusCause> Cause = Slate ? Slate->HasAnyUserFocus() : TOptional<EFocusCause>();
		// 마우스로 눌러 생긴 포커스는 툴팁이 이미 설명했다. 키보드·게임패드 이동만 글로 보인다.
		if (Cause.IsSet() && Cause.GetValue() != EFocusCause::Mouse)
		{
			Hint = Control->GetToolTipText();
			break;
		}
	}
	const bool bVisible = !Hint.IsEmpty();
	if (bVisible && !Text_ControlHint->GetText().EqualTo(Hint)) { Text_ControlHint->SetText(Hint); }
	const ESlateVisibility Wanted = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Text_ControlHint->GetVisibility() != Wanted) { Text_ControlHint->SetVisibility(Wanted); }
}

UFF_AudioSubsystem* UFF_PlaylistPlayerWidget::GetAudio() const
{
	return UFF_AudioSubsystem::Get(this);
}

bool UFF_PlaylistPlayerWidget::ShouldShowPlayer() const
{
	// 호스트에서 펼친 패널은 음악이 없어도 닫기와 비어 있음 안내를 유지한다.
	return bHostedByHud || (!bStandaloneSuppressed && IsPlaylistAvailable());
}

bool UFF_PlaylistPlayerWidget::IsPlaylistAvailable() const
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
	const bool bCanControl = IsPlaylistAvailable();
	bLastCanControl = bCanControl;
	for (UButton* Button : { Button_Prev.Get(), Button_PlayPause.Get(), Button_Next.Get(), Button_AutoAdvance.Get(), Button_Shuffle.Get() })
	{
		if (Button) { Button->SetIsEnabled(bCanControl); }
	}
	if (!Audio)
	{
		if (Text_TrackTitle) { Text_TrackTitle->SetText(NoTrackText); }
		if (Text_PlaybackState) { Text_PlaybackState->SetText(NoTrackText); }
		if (Text_TrackIndex) { Text_TrackIndex->SetText(FText::FromString(TEXT("—"))); }
		if (Text_Elapsed) { Text_Elapsed->SetText(FormatClock(0.f)); }
		if (Text_Duration) { Text_Duration->SetText(FText::FromString(TEXT("—"))); }
		if (ProgressBar_Track) { ProgressBar_Track->SetPercent(0.f); }
		if (Image_Play) { Image_Play->SetVisibility(ESlateVisibility::HitTestInvisible); }
		if (Image_Pause) { Image_Pause->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}

	const int32 Current = bCanControl ? Audio->GetCurrentPlaylistTrack() : INDEX_NONE;
	if (Text_TrackTitle)
	{
		const FText Title = Current == INDEX_NONE
			? NoTrackText
			: Audio->GetPlaylistTrackDisplayName(Current);
		Text_TrackTitle->SetText(Title);
		Text_TrackTitle->SetToolTipText(Title);
	}
	if (Text_PlaybackState) { Text_PlaybackState->SetText(Current == INDEX_NONE ? NoTrackText : (Audio->IsPlaylistPaused() ? PausedText : PlayingText)); }
	if (Text_TrackIndex)
	{
		Text_TrackIndex->SetText(FText::Format(LOCTEXT("TrackIndex", "{0} / {1}"),
			Current == INDEX_NONE ? FText::FromString(TEXT("—")) : FText::AsNumber(Current + 1),
			FText::AsNumber(Audio->GetPlaylistTrackCount())));
	}
	if (Image_Play) { Image_Play->SetVisibility(!bCanControl || Audio->IsPlaylistPaused() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (Image_Pause) { Image_Pause->SetVisibility(!bCanControl || Audio->IsPlaylistPaused() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }

	// 재생 중이면 멈춤 기호, 멈춰 있으면 재생 기호. 누르면 일어날 일을 보여준다.
	if (Text_PlayPause)
	{
		Text_PlayPause->SetText(Audio->IsPlaylistPaused()
			? LOCTEXT("PlayGlyph", "▶")
			: LOCTEXT("PauseGlyph", "||"));
	}
	if (Button_PlayPause)
	{
		Button_PlayPause->SetToolTipText(Audio->IsPlaylistPaused() ? PlayTooltip : PauseTooltip);
	}
	if (Text_ListCount)
	{
		Text_ListCount->SetText(FText::Format(ListCountFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Audio->GetPlaylistTrackCount())}}));
	}

	EnsureRowStyles();
	const bool bAuto = Audio->IsPlaylistAutoAdvance();
	const bool bShuffle = Audio->IsPlaylistShuffle();
	if (Text_AutoAdvance)
	{
		Text_AutoAdvance->SetText(bAuto ? AutoAdvanceOnText : AutoAdvanceOffText);
	}
	if (Button_AutoAdvance && bRowStylesReady && bShowToggleStateByButtonStyle)
	{
		Button_AutoAdvance->SetStyle(bAuto ? ToggleOnStyle : ToggleOffStyle);
	}
	if (Text_Shuffle)
	{
		Text_Shuffle->SetText(bShuffle ? ShuffleOnText : ShuffleOffText);
	}
	if (Button_Shuffle && bRowStylesReady && bShowToggleStateByButtonStyle)
	{
		Button_Shuffle->SetStyle(bShuffle ? ToggleOnStyle : ToggleOffStyle);
	}
	RefreshToggleIcons(bAuto, bShuffle);

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

	const bool bAvailable = IsPlaylistAvailable();
	const float Position = bAvailable ? Audio->GetPlaylistPlaybackSeconds() : 0.f;
	const float Length = bAvailable ? Audio->GetPlaylistTrackLength() : 0.f;

	if (ProgressBar_Track)
	{
		ProgressBar_Track->SetPercent(Length > 0.f ? FMath::Clamp(Position / Length, 0.f, 1.f) : 0.f);
	}

	const int32 Second = FMath::FloorToInt(Position);
	if (Second != LastShownSecond)
	{
		LastShownSecond = Second;
		const int32 Current = Audio->GetCurrentPlaylistTrack();
		const FText Index = Current == INDEX_NONE
			? FText::FromString(TEXT("-"))
			: FText::AsNumber(Current + 1);
		const FText Clock = Length > 0.f
			? FText::Format(LOCTEXT("ClockWithLength", "{0} / {1}"), FormatClock(Position), FormatClock(Length))
			: FormatClock(Position);
		if (Text_TrackMeta) { Text_TrackMeta->SetText(FText::Format(LOCTEXT("MetaFormat", "{0} / {1}  ·  {2}"),
			Index, FText::AsNumber(Audio->GetPlaylistTrackCount()), Clock)); }
		if (Text_Elapsed) { Text_Elapsed->SetText(FormatClock(Position)); }
		if (Text_Duration) { Text_Duration->SetText(Length > 0.f ? FormatClock(Length) : FText::FromString(TEXT("—"))); }
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
	if (bHostedByHud && Button_AutoAdvance)
	{
		// 기본 면·글꼴·여백은 Designer 설정을 사용하고 활성 상태 색만 별도 설정으로 바꾼다.
		ToggleOffStyle = Button_AutoAdvance->GetStyle();
		ToggleOnStyle = ToggleOffStyle;
		ToggleOnStyle.Normal.TintColor = ToggleActiveFill;
		ToggleOnStyle.Normal.OutlineSettings.Color = ToggleActiveOutline;
		ToggleOnStyle.Normal.OutlineSettings.Width = 2.f;
		ToggleOnStyle.Hovered = ToggleOnStyle.Normal;
	}

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
	TArray<FString> TrackNames;
	for (int32 Index = 0; Index < Count; ++Index) { TrackNames.Add(Audio->GetPlaylistTrackDisplayName(Index).ToString()); }
	if (Count == BuiltTrackCount && TrackNames == BuiltTrackNames)
	{
		// 곡 이름은 뱅크가 바뀌지 않는 한 그대로다. 강조만 다시 한다.
		return;
	}
	BuiltTrackCount = Count;
	BuiltTrackNames = MoveTemp(TrackNames);

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
	for (UFF_PlaylistTrackRowWidget* Row : TrackRowWidgets)
	{
		if (Row) { Row->OnTrackChosen.RemoveAll(this); }
	}
	TrackRowWidgets.Reset();
	HighlightedTrack = INDEX_NONE;

	EnsureRowStyles();

	// 줄 글꼴은 곡 제목 글꼴을 따른다. WBP 에서 바꾼 글꼴이 목록에도 먹게 하려는 것이다.
	FSlateFontInfo RowFont = Text_TrackTitle ? Text_TrackTitle->GetFont() : FSlateFontInfo();
	RowFont.Size = TrackRowFontSize;
	RowFont.TypefaceFontName = TEXT("Medium");

	for (int32 TrackIndex = 0; TrackIndex < Count; ++TrackIndex)
	{
		if (TrackRowWidgetClass)
		{
			UFF_PlaylistTrackRowWidget* Row = CreateWidget<UFF_PlaylistTrackRowWidget>(GetOwningPlayer(), TrackRowWidgetClass);
			if (Row)
			{
				Row->SetTrack(TrackIndex, Audio->GetPlaylistTrackDisplayName(TrackIndex),
					TrackIndex == Audio->GetCurrentPlaylistTrack(), Audio->IsPlaylistPaused());
				Row->OnTrackChosen.AddUniqueDynamic(this, &UFF_PlaylistPlayerWidget::HandleTrackRowClicked);
				if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(ScrollBox_Tracks->AddChild(Row)))
				{
					RowSlot->SetPadding(TrackRowSlotPadding);
					RowSlot->SetHorizontalAlignment(HAlign_Fill);
				}
				TrackRowWidgets.Add(Row);
			}
			continue;
		}
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
	const bool bAvailable = IsPlaylistAvailable();
	const int32 Current = Audio && bAvailable ? Audio->GetCurrentPlaylistTrack() : INDEX_NONE;
	if (TrackRowWidgetClass)
	{
		for (int32 Index = 0; Index < TrackRowWidgets.Num(); ++Index)
		{
			if (TrackRowWidgets[Index])
			{
				TrackRowWidgets[Index]->SetCurrent(Index == Current, Audio && Audio->IsPlaylistPaused());
				TrackRowWidgets[Index]->SetIsEnabled(bAvailable);
			}
		}
		if (Current != HighlightedTrack && ScrollBox_Tracks && TrackRowWidgets.IsValidIndex(Current))
		{
			ScrollBox_Tracks->ScrollWidgetIntoView(TrackRowWidgets[Current], false, EDescendantScrollDestination::IntoView);
		}
		HighlightedTrack = Current;
		return;
	}
	for (UButton* Row : TrackRowButtons) { if (Row) { Row->SetIsEnabled(bAvailable); } }
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

void UFF_PlaylistPlayerWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

#undef LOCTEXT_NAMESPACE
