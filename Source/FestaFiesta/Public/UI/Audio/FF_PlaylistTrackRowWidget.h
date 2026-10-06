// 음악 펼침 패널의 편집 가능한 한 곡 행. 곡 데이터와 조작은 오디오 원본을 따른다.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_PlaylistTrackRowWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnPlaylistTrackChosen, int32, TrackIndex);

/**
 * 곡 한 줄. 행 전체가 버튼이라 어디를 눌러도 그 곡을 재생한다.
 *
 * 상태별 표시 (2026-10-07 승인안):
 * - 평소: 번호와 제목만. 오른쪽 재생 삼각형은 숨긴다(자리는 남긴다).
 * - 마우스·키보드 포커스: 연한 하늘색 바탕 + 오른쪽 작은 재생 삼각형.
 * - 지금 곡: 번호 대신 이퀄라이저, 연한 민트 바탕 + 왼쪽 강조선, 오른쪽 작은 `재생 중` 문구.
 * 아이콘 칸은 미리 잡혀 있어 상태가 바뀌어도 제목 위치·폭이 움직이지 않는다.
 * 색만으로 구분하지 않는다 — 지금 곡은 이퀄라이저·강조선·문구, 호버는 재생 삼각형이 함께 바뀐다.
 *
 * 런타임은 바탕 색(`Border_Row` 의 색)과 보이기·글자색만 바꾼다. 모양·크기·글꼴·여백은 Designer 가 가진다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Playlist Track Row"))
class FESTAFIESTA_API UFF_PlaylistTrackRowWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFF_PlaylistTrackRowWidget(const FObjectInitializer& ObjectInitializer);
	void SetTrack(int32 InIndex, const FText& InTitle, bool bCurrent, bool bPaused);
	void SetCurrent(bool bCurrent, bool bPaused);
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Playlist")
	FFF_OnPlaylistTrackChosen OnTrackChosen;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Track;
	/** 상태 바탕. 런타임은 색만 바꾼다. 둥글기·여백은 Designer 에서 고친다. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_Row;
	/** 지금 곡의 왼쪽 강조선. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Image_Accent;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Index;
	/** 지금 곡에서 번호 자리에 보이는 이퀄라이저. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Image_Equalizer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;
	/** 지금 곡의 `재생 중`·`일시정지` 문구. 다른 곡에서는 숨긴다. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_State;
	/** 마우스·포커스가 있는 다른 곡의 작은 재생 삼각형. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Image_PlayHint;
	/** 행 아래 구분선. 지금 곡·호버 바탕과 겹치지 않게 그 행에서는 숨긴다. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Image_Divider;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Style") FLinearColor NormalBackgroundColor;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Style") FLinearColor HoverBackgroundColor;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Style") FLinearColor CurrentBackgroundColor;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Style") FLinearColor NormalTextColor;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Style") FLinearColor CurrentTextColor;
	/** 지금 곡 제목의 굵기(Typeface 이름). 비우면 Designer 굵기를 그대로 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Style") FName CurrentTitleTypeface;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Text") FText PlayingText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Text") FText PausedText;
	/** 행 툴팁. {Title}: 곡 제목. 지금 곡이 아닌 행에서 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Text", meta = (ToolTip = "{Title}: 곡 제목"))
	FText PlayTooltipFormat;
	/** 지금 곡 행 툴팁. {Title}: 곡 제목, {State}: 재생 중·일시정지. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Playlist|Text", meta = (ToolTip = "{Title}: 곡 제목, {State}: 재생 상태"))
	FText CurrentTooltipFormat;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
	void Refresh();
	int32 TrackIndex = INDEX_NONE;
	FText TrackTitle;
	FName DesignerTitleTypeface;
	bool bCapturedTitleTypeface = false;
	bool bIsCurrent = false;
	bool bIsPaused = false;
	bool bHovered = false;
	bool bFocused = false;
};
