// FF_PlaylistPlayerWidget.h
// 역할: 축제 플레이리스트의 미니 플레이어. 지금 곡·재생 조작·곡 목록을 보여준다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "UI/Widgets/FF_PanelScrollStyle.h"
#include "FF_PlaylistPlayerWidget.generated.h"

class UButton;
class UProgressBar;
class UImage;
class UScrollBox;
class UTextBlock;
class UWidget;
class UFF_AudioSubsystem;
class UFF_PlaylistPlayerWidget;
class UFF_PlaylistTrackRowWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnPlaylistCloseRequested);

// 목록 한 줄의 클릭을 받는다. UButton 의 클릭 신호에는 인자가 없어서 줄 번호를 여기서 든다.
UCLASS()
class FESTAFIESTA_API UFF_PlaylistTrackRowProxy : public UObject
{
	GENERATED_BODY()

public:
	int32 TrackIndex = INDEX_NONE;
	TWeakObjectPtr<UFF_PlaylistPlayerWidget> Owner;

	UFUNCTION()
	void HandleClicked();
};

/* 축제 플레이리스트 표시/조작. 재생 상태의 소유자는 UFF_AudioSubsystem이다.
   WBP_FF_PlaylistFlyout은 유틸리티 호스트 안에 들어가며 목록을 항상 펼쳐 둔다.
   호스트가 위치·크기·열림을 관리하고, 닫기는 OnCloseRequested로 요청한다.
   기존 WBP_FF_PlaylistPlayer의 한 줄 미니 플레이어도 호환을 유지한다.
   피날레·폐장 곡 및 튜토리얼 대사 동안은 본문을 숨긴다. 진행 막대는 읽기 전용이다.
*/
UCLASS(Abstract, meta = (DisplayName = "FF Playlist Player"))
class FESTAFIESTA_API UFF_PlaylistPlayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_PlaylistPlayerWidget(const FObjectInitializer& ObjectInitializer);
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 목록 줄을 눌렀다. 줄 프록시가 부른다.
	UFUNCTION() void HandleTrackRowClicked(int32 TrackIndex);

	// 펼침 패널은 호스트가 자리·열림 상태를 관리한다. 기존 독립 미니 플레이어도 유지한다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	bool IsPlaylistAvailable() const;
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	bool IsHostedByHud() const { return bHostedByHud; }
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	void SetStandaloneSuppressed(bool bSuppressed);
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Audio")
	FFF_OnPlaylistCloseRequested OnCloseRequested;
	void ConfigureFlyoutDefaults(TSubclassOf<UFF_PlaylistTrackRowWidget> InRowClass);
	/** 아이콘형 셔플·반복(2026-10-07 승인안) 최초 적용용. 이후 값은 Class Defaults 에서 고친다. */
	void ConfigureIconToggleDefaults();

	// 목록을 펼치거나 접는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	void SetListExpanded(bool bExpanded);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	bool IsListExpanded() const { return bListExpanded; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	/** 패널 위의 휠은 패널이 가진다. 목록 끝에서 넘친 휠이 뒤 카메라를 확대·축소하지 않게 한다. */
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** 곡 목록 스크롤바 모양. 셋업·라이브 패널과 같은 공용 막대다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Scroll")
	FFF_PanelScrollStyle ScrollStyle;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Layout")
	bool bHostedByHud = false;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Layout")
	TSubclassOf<UFF_PlaylistTrackRowWidget> TrackRowWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Style")
	FLinearColor ToggleActiveFill;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Style")
	FLinearColor ToggleActiveOutline;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText PlayingText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText PausedText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText NoTrackText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText AutoAdvanceOnText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText AutoAdvanceOffText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText ShuffleOnText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText ShuffleOffText;

	/**
	 * 켜면 자동 다음 곡·셔플 버튼의 면(버튼 스타일)을 켬/끔에 따라 바꾼다(예전 글자 버튼).
	 * 도크 음악 패널은 꺼 두고 아이콘 색과 아래 작은 점으로 상태를 보인다 — 버튼 스타일은 Designer 그대로다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Style")
	bool bShowToggleStateByButtonStyle = true;
	/** 켜진 셔플·한 곡 반복 아이콘 색. 점(`Dot_*`)이 함께 보여 색만으로 구분하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Style")
	FLinearColor ToggleActiveIconColor;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Style")
	FLinearColor ToggleInactiveIconColor;
	/** 곡 행 사이 여백(행 위젯을 쓸 때). 행 안 여백·높이는 행 WBP Designer 에서 고친다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Layout")
	FMargin TrackRowSlotPadding = FMargin(0.f, 0.f, 5.f, 7.f);

	/** 셔플·반복·재생 버튼의 상태 설명. 마우스를 올리면 툴팁, 키보드 포커스면 `Text_ControlHint` 에 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText ShuffleOnTooltip;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText ShuffleOffTooltip;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText RepeatOneTooltip;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText AutoAdvanceTooltip;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText PlayTooltip;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text") FText PauseTooltip;
	/** 재생 목록 제목 옆 곡 수. {Count}: 곡 수. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio|Text", meta = (ToolTip = "{Count}: 곡 수"))
	FText ListCountFormat;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Close;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlaybackState;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_TrackIndex;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Elapsed;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Duration;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Play;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Pause;
	// 플레이어 전체. 숨길 때 이것만 접는다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UWidget> Panel_Player;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UTextBlock> Text_TrackTitle;

	// "3 / 12 · 1:23 / 3:40"
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UTextBlock> Text_TrackMeta;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UProgressBar> ProgressBar_Track;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UButton> Button_Prev;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UButton> Button_PlayPause;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UTextBlock> Text_PlayPause;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UButton> Button_Next;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UButton> Button_ToggleList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UTextBlock> Text_ToggleList;

	// 펼쳤을 때만 보이는 부분(목록과 자동 다음 곡·셔플).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UWidget> Panel_List;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UButton> Button_AutoAdvance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UTextBlock> Text_AutoAdvance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UButton> Button_Shuffle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UTextBlock> Text_Shuffle;

	// 아이콘형 셔플·반복 버튼. 반복은 한 곡 반복일 때 `Image_RepeatOne`(안에 작은 1), 아니면 `Image_Repeat`.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Shuffle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Repeat;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_RepeatOne;
	// 켜졌을 때 아이콘 아래 작은 점. 꺼지면 자리만 지킨다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Dot_Shuffle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Dot_Repeat;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ListCount;
	// 키보드로 조작 버튼에 포커스가 가면 그 버튼의 상태 설명을 보인다. 마우스는 툴팁으로 본다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ControlHint;

	// 곡 줄이 들어갈 자리. 줄은 실행 중에 만든다(곡 수가 뱅크에 따라 바뀐다).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Audio")
	TObjectPtr<UScrollBox> ScrollBox_Tracks;

	// 곡 줄 글자 크기.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio")
	int32 TrackRowFontSize = 12;

	// 줄 높이를 맞추는 안쪽 여백.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio")
	FMargin TrackRowPadding = FMargin(10.f, 6.f);

	// 상태가 바뀌어 다시 그렸다. 꾸밈을 붙이려면 여기에 건다.
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Audio", meta = (DisplayName = "On Playlist Refreshed"))
	void BP_OnPlaylistRefreshed();

private:
	UFUNCTION() void HandleCloseClicked();
	UFUNCTION() void HandlePrevClicked();
	UFUNCTION() void HandlePlayPauseClicked();
	UFUNCTION() void HandleNextClicked();
	UFUNCTION() void HandleToggleListClicked();
	UFUNCTION() void HandleAutoAdvanceClicked();
	UFUNCTION() void HandleShuffleClicked();

	// 오디오 쪽 상태가 바뀌었다.
	UFUNCTION()
	void HandlePlaylistChanged();

	UFF_AudioSubsystem* GetAudio() const;

	// 지금 보여야 하는지. 플레이리스트가 흐르고 튜토리얼 대사가 없을 때.
	bool ShouldShowPlayer() const;

	// 곡 이름·버튼 글자·목록 강조를 다시 그린다.
	void RefreshAll();

	// 재생 위치와 진행 막대. 매 프레임 바뀌므로 따로 둔다.
	void RefreshProgress();

	// 곡 수가 바뀌었으면 목록 줄을 다시 만든다.
	void RebuildTrackRows();

	// 지금 곡 줄을 강조한다.
	void RefreshRowHighlight();

	// 팔레트로 줄 버튼 모양을 만든다. 처음 한 번만.
	void EnsureRowStyles();

	// 셔플·반복 아이콘과 점, 툴팁.
	void RefreshToggleIcons(bool bAutoAdvance, bool bShuffle);

	// 키보드 포커스가 있는 조작 버튼의 설명.
	void RefreshControlHint();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> TrackRowButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TrackRowTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_PlaylistTrackRowProxy>> TrackRowProxies;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_PlaylistTrackRowWidget>> TrackRowWidgets;
	TArray<FString> BuiltTrackNames;

	FButtonStyle RowStyle;
	FButtonStyle CurrentRowStyle;
	FButtonStyle ToggleOnStyle;
	FButtonStyle ToggleOffStyle;
	FLinearColor RowTextColor = FLinearColor::Black;
	FLinearColor CurrentRowTextColor = FLinearColor::Black;
	bool bRowStylesReady = false;

	// 줄을 마지막으로 만든 곡 수. 다르면 다시 만든다.
	int32 BuiltTrackCount = INDEX_NONE;

	// 줄에서 강조 중인 곡.
	int32 HighlightedTrack = INDEX_NONE;

	bool bListExpanded = false;
	bool bShown = false;
	bool bBoundAudio = false;
	bool bStandaloneSuppressed = false;
	bool bLastCanControl = false;

	// 재생 위치 글자를 초 단위로만 바꾼다. 매 프레임 글자를 다시 만들 이유가 없다.
	int32 LastShownSecond = INDEX_NONE;
};
