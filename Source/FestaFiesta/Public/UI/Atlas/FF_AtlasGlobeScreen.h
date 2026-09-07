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

/** 국가 하나를 골랐다는 알림. `FName` 은 `CountryId` 다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnAtlasCountryPicked, FName, CountryId);

/** `뒤로` · `축제 둘러보기` 처럼 인자 없는 요청. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnAtlasScreenRequest);

/**
 * 나라 선택 화면. **지구가 주인공이고 UI 는 왼쪽에만 둔다** (2026-08-21 확정).
 *
 * === 3D 씬 위에 얹히는 오버레이다 ===
 * 지구는 `AFF_AtlasGlobeActor` 가 그린다. 이 화면은 그 위에 **국가 핀 버튼**을 얹고
 * 왼쪽 패널에 고른 나라를 보여준다. 오른쪽은 비워 지구가 크게 보이게 한다.
 *
 * === 베이스가 `UFF_ActivatableScreenBase` 인 이유 ===
 * 흐름 설정의 `FrontendScreens` 가 `TSoftClassPtr<UFF_ActivatableScreenBase>` 라
 * **그 파생만 받는다.** 일반 `UUserWidget` 으로 만들면 흐름에 꽂을 자리가 없고 뒤로 가기
 * 히스토리도 못 쓴다. 계획서 화면 목록표가 `일반 UUserWidget` 으로 적혀 있던 것은 낡은 것이다.
 *
 * === 🔴 이 화면이 Tick 을 쓰는 이유 ===
 * 이 프로젝트의 다른 UI 는 전부 델리게이트 구독이고 `NativeTick` 을 쓰는 위젯이 하나도 없다.
 * 여기가 처음이다 — **지구가 상시 자전하므로 핀의 화면 좌표가 매 프레임 바뀐다.**
 * 이벤트로 알릴 수 있는 종류의 변화가 아니다. 대신 Tick 이 하는 일은 좁다:
 * 액터에서 핀 상태를 받아 **좌표만 갱신한다.** 판단은 전부 순수 함수와 액터가 한다.
 *
 * === 🔴 드래그 회전 — 마우스 캡처가 필요하다 ===
 * 프론트엔드 화면은 `bShowMouseCursor=true` → `EMouseCaptureMode::NoCapture` 라
 * 뷰포트가 마우스를 잡지 않는다. 그래서 **위젯이 직접 캡처해야** 커서가 위젯 밖으로 나가도
 * 드래그가 이어진다. 이 리포에 `CaptureMouse` 사용례가 없어 여기가 첫 사례다.
 *
 * 핀 버튼이 위에 있으므로 핀을 누르면 그 클릭은 버튼이 먹고 드래그는 시작되지 않는다 —
 * 그게 맞는 동작이다. 빈 곳을 누르면 이 화면이 받아 지구를 돌린다.
 * 그래도 **같은 클릭이 둘 다에 먹히는 경우**가 있어 `FFF_AtlasDragTracker` 로
 * 임계 픽셀 안에서 떼면 클릭, 넘으면 드래그로 갈라 준다.
 */
UCLASS(meta = (DisplayName = "FF Atlas Globe Screen"))
class FESTAFIESTA_API UFF_AtlasGlobeScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_AtlasGlobeScreen(const FObjectInitializer& ObjectInitializer);

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

	// --- 입력 ---

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/**
	 * 커서를 직접 읽어 지구를 돌린다. **이동을 마우스 이벤트로 처리하지 않는 이유**는
	 * `NativeOnMouseButtonDown` 주석에 있다 — 캡처를 못 쓰는 상황이 있어서다.
	 */
	void TickDrag();

	/** 드래그를 끝내고 가까운 핀으로 재스냅한다. 위젯 안/밖 어디서 떼든 여기로 모인다. */
	void EndDrag();
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	// --- 왼쪽 패널 ---

	/** 국가를 누르기 전에는 접혀 있다. 지구 구도를 바꾸지 않고 그 위에 겹친다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UBorder> Border_Panel;

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

private:
	/** 추적 중인 핀과 미추적 잔여 핀을 모두 제거한다. */
	void ClearPinButtons();

	/** 액터에서 핀 상태를 받아 버튼을 만들거나 재사용한다. */
	void RebuildPinButtons();

	/** 매 프레임 핀의 화면 좌표와 가시성을 갱신한다. */
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

	/** 핀이 눌렸을 때. 잠긴 국가면 사유만 띄우고 선택하지 않는다. */
	UFUNCTION()
	void HandlePinClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleSelectCountryClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleClosePanelClicked(UFF_CommonButtonBase* Button);

	/** 액터가 회전을 끝내 국가가 정면에 섰을 때. */
	UFUNCTION()
	void HandleCountryFocused(FName CountryId);

	AFF_AtlasGlobeActor* GetGlobe() const;
	const UFF_FestivalAtlasCatalog* GetCatalog() const;

	/** 진행 중인 카메라·회전 연출을 즉시 끝낸다. **모든 연출은 입력으로 Skip 된다.** */
	void SkipPresentation();

	/** `CountryId` → 핀 버튼. 인덱스로 짝짓지 않는다 (안티패턴 15). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UFF_AtlasPinButton>> PinButtons;

	/** 패널에서 정보 행으로만 쓰는 축제 핀 위젯. 입력은 받지 않는다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_AtlasPinButton>> FestivalRows;

	/**
	 * 지구 액터. 월드와 함께 죽으므로 약참조로 든다.
	 * `mutable` 인 것은 조회용 캐시라서다 — `GetGlobe()` 는 const 인데 처음 찾을 때 채운다.
	 */
	mutable TWeakObjectPtr<AFF_AtlasGlobeActor> CachedGlobe;

	FFF_AtlasDragTracker DragTracker;

	/** 지금 왼쪽 패널이 보여주는 국가. */
	FName PanelCountryId;
};
