// FF_PlaylistPlayerWidget.h
// 역할: 축제 플레이리스트의 미니 플레이어. 지금 곡·재생 조작·곡 목록을 보여준다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "FF_PlaylistPlayerWidget.generated.h"

class UButton;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UWidget;
class UFF_AudioSubsystem;
class UFF_PlaylistPlayerWidget;

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

/* 축제 플레이리스트의 미니 플레이어.

   ─── HUD 밖에 따로 뜬다 ───────────────────────────────────────────────────────
   LiveHUD·준비 HUD 를 고치지 않으려고 화면에 따로 올린다(UFF_AudioEventLink). 순서는
   UI 루트보다 아래라 일시정지·설정 화면이 위를 덮는다(UFF_AudioSettings::PlaylistPlayerZOrder).

   ─── 평소에는 한 줄, 펼치면 목록 ─────────────────────────────────────────────
   화면 왼쪽 아래, HUD 아래 띠 바로 위에 한 줄 바로 있다. 지금 곡과 이전·재생/일시정지·다음만
   보인다. 목록 버튼을 누르면 바 위로 곡 목록이 펼쳐져 직접 고르고, 자동 다음 곡·셔플을
   켜고 끈다. 설정은 오디오 쪽이 저장한다.

   ─── 보일 때 ────────────────────────────────────────────────────────────────
   플레이리스트가 흐르는 동안만 보인다(준비·운영 단계). 피날레·폐장 곡일 때와 튜토리얼
   대사가 떠 있는 동안은 숨는다. 루트는 화면 전체에 깔리고 클릭은 흘려보낸다.
   숨길 때는 Panel_Player 만 접는다 — 루트를 접으면 이 위젯이 Tick 을 받지 못해 다시 못 뜬다.

   ─── WBP 이름 ───────────────────────────────────────────────────────────────
   에디터 콘솔 `FF.PlaylistPlayerUMG` 가 WBP_FF_PlaylistPlayer 를 만든다. 디자이너에서 옮기거나
   꾸며도 되고, 아래 이름만 유지하면 된다. 없는 이름은 그 기능만 빠진다.
*/
UCLASS(Abstract, meta = (DisplayName = "FF Playlist Player"))
class FESTAFIESTA_API UFF_PlaylistPlayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 목록 줄을 눌렀다. 줄 프록시가 부른다.
	void HandleTrackRowClicked(int32 TrackIndex);

	// 목록을 펼치거나 접는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	void SetListExpanded(bool bExpanded);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	bool IsListExpanded() const { return bListExpanded; }

protected:
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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> TrackRowButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TrackRowTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_PlaylistTrackRowProxy>> TrackRowProxies;

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

	// 재생 위치 글자를 초 단위로만 바꾼다. 매 프레임 글자를 다시 만들 이유가 없다.
	int32 LastShownSecond = INDEX_NONE;
};
