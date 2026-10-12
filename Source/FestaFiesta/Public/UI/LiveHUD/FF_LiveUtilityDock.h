// SNS와 음악의 공통 진입점. 재생과 시뮬레이션은 각 원본 시스템이 소유한다.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_LiveUtilityDock.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;
class UFF_LivePanelSNS;
class UFF_PlaylistPlayerWidget;

UCLASS(meta = (DisplayName = "FF Live Utility Dock"))
class FESTAFIESTA_API UFF_LiveUtilityDock : public UUserWidget
{

	GENERATED_BODY()
public:
	/** 기존 오디오 위젯 생성 설정이 이 도크를 사용하는지. 하단 SNS 중복 진입을 막는다. */
	static bool IsConfigured();
	static UFF_LiveUtilityDock* FindForPlayer(const APlayerController* Player);
	bool HasUnifiedNews() const;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Utility")
	void ToggleSNS();
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Utility")
	void ToggleMusic();
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Utility")
	void CloseFlyout();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Layout")
	FVector2D DockPosition = FVector2D(24.f, 24.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Layout", meta = (ClampMin = "32.0"))
	float ButtonDiameter = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Layout", meta = (ClampMin = "0.0"))
	float ButtonGap = 12.f;
	/** 도크 원점에서 펼침 패널까지의 거리. 두 패널이 같은 자리를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Layout")
	FVector2D PanelOffset = FVector2D(80.f, 0.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Layout")
	FVector2D SnsPanelSize = FVector2D(480.f, 640.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Layout")
	FVector2D MusicPanelSize = FVector2D(480.f, 740.f);
	/** 하단 HUD를 가리지 않도록 남길 공간. 작은 창에서는 본문만 스크롤한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Layout", meta = (ClampMin = "0.0"))
	float ReservedBottomSpace = 84.f;

	/**
	 * 버튼 아이콘·글자 색. 배경 원이 없으므로 흰색+그림자로 월드 위에 띄운다.
	 * 열린 패널의 버튼은 ✓ 대신 이 색이 바뀐다 (2026-10-06 사용자 결정). 패널이 옆에 펼쳐져
	 * 있어 색 말고도 선택 상태가 드러난다. Designer 의 아이콘 색보다 이 값이 우선한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Style")
	FLinearColor IconColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Style")
	FLinearColor SelectedIconColor = FLinearColor::FromSRGBColor(FColor(110, 231, 190));

	/** 아이콘 아래 `SNS`·`음악` 이름. 2026-10-06 사용자 요청으로 기본은 숨기고 아이콘만 둔다. 이름은 툴팁에 남는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Style")
	bool bShowButtonLabels = false;

	/**
	 * SNS 버튼의 새 글 배지. 기본은 닫혀 있는 동안 온 글 수(숫자)다.
	 * 켜면 숫자 없이 빨간 점만 보인다 — 배지 테두리 여백을 점 크기에 맞춰 정사각으로 바꾼다.
	 * 배지 색·테두리는 Designer 의 `Border_Unread` 에서 고친다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Style")
	bool bUnreadAsDot = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Utility|Style", meta = (EditCondition = "bUnreadAsDot", ClampMin = "4.0", UIMax = "24.0"))
	float UnreadDotSize = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Utility|Panels")
	TSoftClassPtr<UFF_LivePanelSNS> SnsPanelClass = TSoftClassPtr<UFF_LivePanelSNS>(FSoftObjectPath(TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyout.WBP_FF_LiveSnsFlyout_C")));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Utility|Panels")
	TSoftClassPtr<UFF_PlaylistPlayerWidget> MusicPanelClass = TSoftClassPtr<UFF_PlaylistPlayerWidget>(FSoftObjectPath(TEXT("/Game/FestaFiesta/UI/Audio/WBP_FF_PlaylistFlyout.WBP_FF_PlaylistFlyout_C")));

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> Panel_Dock;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOverlay> Panel_Flyout;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USizeBox> SizeBox_SNS;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USizeBox> SizeBox_Music;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_SNS;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Music;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Unread;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_SNS;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Music;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SNSLabel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_MusicLabel;
	/** 예전 ✓ 표시. 이제 쓰지 않고 접어 둔다. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SNSSelected;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_MusicSelected;

private:
	void ApplyLayout(FVector2D AvailableSize);
	void RefreshAvailability();
	void RefreshSelection();
	void RefreshUnread(int32 Count);
	UPROPERTY(Transient) TObjectPtr<UFF_LivePanelSNS> SnsPanel;
	UPROPERTY(Transient) TObjectPtr<UFF_PlaylistPlayerWidget> MusicPanel;
	FDelegateHandle PendingPostsHandle;
	FVector2D LastAvailableSize = FVector2D::ZeroVector;
	// 0=닫힘, 1=SNS, 2=음악. 가시성은 이 호스트 한 곳에서만 결정한다.
	uint8 OpenPanel = 0;
	bool bSnsAvailable = false;
	bool bDockAvailable = false;
	// 확인창·ESC가 닫히면 같은 패널/스크롤 위치로 복원한다. 닫기 동작과 별개다.
	bool bDockPresented = false;
};
