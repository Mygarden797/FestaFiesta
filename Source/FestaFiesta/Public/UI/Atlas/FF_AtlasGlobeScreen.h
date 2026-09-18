// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Atlas/FF_AtlasScreenGeo.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_AtlasGlobeScreen.generated.h"

class AFF_AtlasGlobeActor;
class UBorder;
class UCanvasPanel;
class UFF_AtlasPinButton;
class UFF_CommonButtonBase;
class UFF_FestivalAtlasCatalog;
class UImage;
class UTextBlock;
class UVerticalBox;
class UScrollBox;

/** 국가 하나를 골랐다는 알림. `FName` 은 `CountryId` 다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnAtlasCountryPicked, FName, CountryId);

/** `뒤로` · `축제 둘러보기` 처럼 인자 없는 요청. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnAtlasScreenRequest);

/**
 * 지구 위 이름표 대신 왼쪽 국가 목록을 표시하는 CommonUI 화면.
 * 목록과 프랍 클릭은 InspectCountry를 공유하고 국가 지도 진입은 별도로 확인한다.
 * 드래그는 캡처 없이 Slate 커서로 추적한다. Tick은 드래그와 프랍 호버에만 사용한다.
 */
UCLASS(meta = (DisplayName = "FF Atlas Globe Screen"))
class FESTAFIESTA_API UFF_AtlasGlobeScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_AtlasGlobeScreen(const FObjectInitializer& ObjectInitializer);

	/** 나라 목록·상세 카드용 국기. UI 전용 참조라 지구 액터의 데이터와 분리한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Atlas|Appearance")
	TMap<FName, TObjectPtr<UTexture2D>> CountryFlagTextures;

	/** 목록과 프랍이 공유하는 국가 살펴보기. 잠긴 나라도 회전하되 지도 진입은 막는다. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Atlas")
	void InspectCountry(FName CountryId);

	/** `축제 둘러보기` — 국가 지도로 간다. 흐름 서브시스템이 받는다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Atlas")
	FFF_OnAtlasCountryPicked OnBrowseFestivalsRequested;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Atlas")
	FFF_OnAtlasScreenRequest OnBackRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// --- 입력 ---

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/**
	 * 커서를 직접 읽어 지구를 돌린다. **이동을 마우스 이벤트로 처리하지 않는 이유**는
	 * `NativeOnMouseButtonDown` 주석에 있다 — 캡처를 못 쓰는 상황이 있어서다.
	 */
	void TickDrag();

	/** 드래그를 끝내거나 이동 없는 프랍 클릭을 확정한다. 위젯 안/밖 어디서 떼든 여기로 모인다. */
	void EndDrag();
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	// --- 왼쪽 패널 ---

	/** 국가를 누르기 전에는 접혀 있다. 지구 구도를 바꾸지 않고 그 위에 겹친다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UBorder> Border_Panel;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="FestaFiesta|Atlas")
	TObjectPtr<UScrollBox> ScrollBox_Countries;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="FestaFiesta|Atlas")
	TObjectPtr<UScrollBox> ScrollBox_CountryDetails;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_CountryName;

	/** `이용 가능한 축제 1`. 축제가 없으면 숨는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_FestivalCount;

	/** 대표 이미지. 없으면 칸을 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_Preview;

	/**
	 * 구버전 WBP 호환용. 새 화면은 `VerticalBox_FestivalList`를 쓰므로 항상 숨긴다.
	 * 에디터 마감 때 위젯 트리에서 제거해도 된다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_RepresentativeFestival;

	/** 선택한 국가의 축제 목록. 행은 `UFF_AtlasPinButton`의 이름·배지·자물쇠 표현을 재사용한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UVerticalBox> VerticalBox_FestivalList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_Description;

	/** 자물쇠와 사유. 잠긴 국가를 골랐을 때만 보인다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_LockedIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_LockReason;

	/** 하단 조작 안내 — `회전` `핀 이동` `선택` `뒤로`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_ControlHints;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UFF_CommonButtonBase> Button_SelectCountry;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UFF_CommonButtonBase> Button_ClosePanel;

	/** 구버전 WBP 호환용. 새 WBP에서는 `Button_SelectCountry`를 쓴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UFF_CommonButtonBase> Button_BrowseFestivals;

	/** 구버전 WBP 호환용. 새 WBP에서는 `Button_ClosePanel`을 쓴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UFF_CommonButtonBase> Button_Back;

	// --- 핀 층 ---

	/**
	 * 핀 버튼이 올라가는 캔버스. **화면 전체를 덮어야** 지구 어디에나 핀을 놓을 수 있다.
	 * `Self Hit Test Invisible` 로 둔다 — `Visible` 이면 캔버스가 드래그를 먹어 지구가 안 돌아간다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UCanvasPanel> CanvasPanel_Pins;

	/**
	 * 투명하지만 `Visible`인 전면 입력 표면. 빈 공간의 마우스 입력을 화면까지 버블링해
	 * `NativeOnMouseButtonDown`이 드래그를 시작할 수 있게 한다. 핀·패널보다 아래에 둔다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UBorder> Border_DragSurface;

	/**
	 * 핀 버튼 클래스. **에디터에서 지정한다** — 생성기가 CDO 프로퍼티를 채우지 못한다.
	 * 비어 있으면 핀이 하나도 안 뜨고 경고를 남긴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas")
	TSubclassOf<UFF_AtlasPinButton> PinButtonClass;

	/** 드래그로 볼지 클릭으로 볼지 가르는 임계(픽셀). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas", meta = (ClampMin = "0.0"))
	float DragThresholdPixels = 5.f;

	/** 왼쪽 패널 배치를 읽을 수 없는 첫 프레임/구버전 위젯의 국가 포커스 위치(0~1). */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Atlas|Presentation")
	FVector2D FallbackCountryFocusPosition = FVector2D(0.68, 0.5);

private:
	/** 실제 패널 오른쪽부터 뷰포트 끝까지의 중앙을 지구에 전달한다. */
	void UpdateCountryFocusViewport();

	/** 추적 중인 핀과 미추적 잔여 핀을 모두 제거한다. */
	void ClearPinButtons();

	/** 액터에서 핀 상태를 받아 버튼을 만들거나 재사용한다. */
	void RebuildPinButtons();

	/** 국가 목록의 선택 표시를 갱신한다. */
	void UpdatePinPositions();

	/** 왼쪽 패널을 지금 포커스된 국가로 다시 채운다. */
	void RefreshCountryPanel();

	/** 국가 패널 안의 축제 미리보기 행을 모두 내린다. */
	void ClearFestivalRows();

	/** 새 이름을 우선하고 구버전 WBP의 버튼 이름을 임시 대체로 쓴다. */
	UFF_CommonButtonBase* GetSelectCountryButton() const;
	UFF_CommonButtonBase* GetClosePanelButton() const;

	/** 패널이 닫혔을 때 포커스를 돌려줄 첫 가시 국가 핀. */
	UFF_AtlasPinButton* FindFirstVisiblePin() const;

	/** 목록 버튼을 눌렀을 때. 잠긴 국가도 살펴보되 지도 진입은 막는다. */
	UFUNCTION()
	void HandlePinClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleListFocused(UFF_AtlasPinButton* Button);

	FName PickCursorLandmark() const;

	UFUNCTION()
	void HandleSelectCountryClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleClosePanelClicked(UFF_CommonButtonBase* Button);

	/** 액터가 회전을 끝내 국가가 정면에 섰을 때. */
	UFUNCTION()
	void HandleCountryFocused(FName CountryId);

	AFF_AtlasGlobeActor* GetGlobe() const;
	const UFF_FestivalAtlasCatalog* GetCatalog() const;

	/** 화면 진입 카메라만 즉시 끝낸다. 기존 국가 회전·접근은 현재 위치에서 새 입력으로 이어진다. */
	void SkipEntranceCameraMove();

	/** `CountryId` → 핀 버튼. 인덱스로 짝짓지 않는다 (안티패턴 15). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UFF_AtlasPinButton>> PinButtons;

	/** 패널에서 정보 행으로만 쓰는 축제 핀 위젯. 입력은 받지 않는다. */
	UPROPERTY(Transient)


	/**
	 * 지구 액터. 월드와 함께 죽으므로 약참조로 든다.
	 * `mutable` 인 것은 조회용 캐시라서다 — `GetGlobe()` 는 const 인데 처음 찾을 때 채운다.
	 */
	mutable TWeakObjectPtr<AFF_AtlasGlobeActor> CachedGlobe;

	FFF_AtlasDragTracker DragTracker;

	/** 지금 왼쪽 패널이 보여주는 국가. */
	FName PanelCountryId;
	FName PressedCountryId;
};
