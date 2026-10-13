// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Atlas/FF_AtlasScreenGeo.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_AtlasCountryScreen.generated.h"

class UCanvasPanel;
class UFF_AtlasPinButton;
class UFF_CommonButtonBase;
class UFF_FestivalAtlasCatalog;
class UImage;
class UMaterialInterface;
class USizeBox;
class UTextBlock;

/** 축제를 시작해 달라는 요청. `FName` 은 `FestivalId` 다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnAtlasFestivalStartRequested, FName, FestivalId);

/** 인자 없는 요청 (`뒤로`). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnAtlasCountryScreenRequest);

/**
 * 국가 지도 화면. **지도가 주인공이고 축제 카드는 오른쪽이다** (2026-08-26 확정).
 *
 * === 축제 핀을 직접 고른다 ===
 * **지역 선택 단계를 만들지 않는다.** `부산` 같은 지역 라벨은 핀에 보여주되 핀 자체는
 * 축제를 가리킨다. 지역을 한 단계 더 두면 축제가 하나인 지금 빈 화면이 하나 늘어난다.
 *
 * === 🔴 핀 위치는 `MapPosition`(0~1) 을 이미지 사각형에 매핑해 계산한다 ===
 * 이미지 비율·레터박스가 바뀌어도 핀이 어긋나지 않아야 한다. 그 계산은
 * `FFF_AtlasScreenGeo` 가 순수 함수로 갖고 있고 다섯 가지 창 크기로 테스트돼 있다
 * (`FestaFiesta.UI.AtlasScreenGeo.PinStaysOnMapAcrossSizes`).
 *
 * ★ **같은 사각형을 지도 이미지에도 쓴다.** UMG 의 `UImage` 는 `ScaleToFit` 이 없어서
 *   위젯 크기에 그냥 늘려 그린다. 이미지와 핀이 서로 다른 계산을 하면 핀이 지도에서 어긋난다.
 *
 * === 지도 이미지가 없어도 동작한다 ===
 * `MapImage` 가 비면 **네모 박스 + 팔레트 색**으로 대신 그린다. 2.5D 국가 지도 이미지는
 * 아직 없고, 없다고 화면이 죽으면 그 이미지가 생길 때까지 흐름을 확인할 수 없다.
 *
 * === 🔴 고르기 전에는 카드가 없다 (2026-08-27 결정) ===
 * 진입할 때 축제를 **자동으로 고르지 않는다.** 예전에는 첫 플레이 가능 축제를 미리 선택해서
 * "부산을 누르지도 않았는데 다대포 카드가 떠 있는" 화면이 나왔다. 지금은 이렇게 간다.
 *
 *   진입 → 선택 없음 · 카드 숨김 · 포커스는 `Button_Back`(브레드크럼)
 *   → 핀 호버 또는 `WASD`/`Enter` → 그 핀이 선택되고 카드가 뜬다
 *
 * ★ 마우스 호버를 따로 다루지 않는다. `UFF_CommonButtonBase` 가 `bFocusOnHover` 로 호버를
 *   포커스로 바꾸므로, 핀의 `OnPinFocusReceived` 하나가 마우스·키보드·패드를 모두 받는다.
 */
UCLASS(meta = (DisplayName = "FF Atlas Country Screen"))
class FESTAFIESTA_API UFF_AtlasCountryScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_AtlasCountryScreen(const FObjectInitializer& ObjectInitializer);

	/** 어느 국가의 지도를 그릴지. 흐름 서브시스템이 화면을 올린 뒤 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void SetCountry(FName InCountryId);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Atlas")
	FFF_OnAtlasFestivalStartRequested OnStartFestivalRequested;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Atlas")
	FFF_OnAtlasCountryScreenRequest OnBackRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// --- 지도 ---

	/**
	 * 지도 이미지. `MapImage` 가 비면 팔레트 색 네모로 대신 그린다.
	 * 크기·위치는 코드가 `ComputeImageRect` 로 정한다 — 디자이너에서 앵커를 채움으로 두면 된다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_Map;

	/**
	 * 선택 축제의 행정구역 오버레이. `MapImage`와 같은 사각형을 사용한다.
	 * 오버레이는 축제 데이터가 결정하며 화면 코드에는 부산·영남 같은 지역명이 없다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_SelectedRegionOverlay;

	/**
	 * 행정구역 경계선 레이어. 지형 이미지 위에 얹는 투명 PNG다.
	 *
	 * 🔴 **`Image_Map` 과 반드시 같은 사각형을 쓴다.** 지형과 경계는 같은 좌표계
	 * (WGS84 124–132°E · 33–39°N · 2400×2160)로 만들어졌으므로, 한쪽만 다른 크기로
	 * 늘리면 경계가 해안선에서 어긋난다. `UpdateMapLayout` 이 세 레이어를 한 값으로 맞춘다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_MapBorders;

	/**
	 * 지도와 핀이 함께 올라가는 캔버스. **이 캔버스의 크기가 계산 기준**이다.
	 * `Self Hit Test Invisible` 로 둔다 — 핀만 입력을 받아야 한다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UCanvasPanel> CanvasPanel_Map;

	// --- 오른쪽 축제 카드 ---

	/**
	 * 카드 전체를 담는 상자. **고른 축제가 없으면 이걸 통째로 `Collapsed` 한다.**
	 *
	 * 자식 위젯을 하나씩 숨기지 않는 이유는, 배경 판·테두리·여백처럼 바인딩되지 않은 장식이
	 * 카드에 남아 "빈 카드가 떠 있는" 화면이 되기 때문이다. 껍데기를 끄는 것이 유일하게
	 * 확실한 방법이다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UWidget> Panel_FestivalCard;

	/** 카드 왼쪽의 `<` 장식. 카드와 함께 나타나고 사라진다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UWidget> Image_CardCaret;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_CountryName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_FestivalName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_RegionName;

	/** 난이도. **`잠정` 을 함께 띄운다** — 산정 기준이 아직 없다 (`assumption`). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_Difficulty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_OperatingDays;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_Preview;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_LockedIcon;

	/**
	 * 잠금 사유. **`Button_StartFestival` 과 같은 자리를 나눠 쓴다** —
	 * 잠긴 축제에서는 시작 버튼을 `Collapsed` 하고 이 문구를 그 자리에 띄운다.
	 * 눌리지 않는 버튼을 남겨 두는 것보다 왜 못 하는지 읽히는 편이 낫다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_LockReason;

	/** `플레이 가능` 또는 잠금 상태를 글자로도 알린다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_Availability;

	/**
	 * 위 문구를 감싸는 알약. 문구 길이에 맞춰 자동으로 늘어난다.
	 * 고른 축제가 없으면 숨는다 — 빈 알약은 `-` 를 넣는 것과 같다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UWidget> Panel_AvailabilityPill;

	/**
	 * 알약 안의 `▶`. 자물쇠(`Image_LockedIcon`)와 교대한다.
	 *
	 * 색·문구·모양 셋으로 상태를 알린다 (AT-011). 알약 색만 바꾸면 색각 이상에서 구분되지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_PlayableIcon;

	/** 축제를 고르기 전에 보여주는 안내. 카드가 비어 있는 이유를 적는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_EmptyHint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Atlas")
	TObjectPtr<UFF_CommonButtonBase> Button_StartFestival;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Atlas")
	TObjectPtr<UFF_CommonButtonBase> Button_Back;

	/** 핀 버튼 클래스. **에디터에서 지정한다** — 비면 핀이 하나도 뜨지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas")
	TSubclassOf<UFF_AtlasPinButton> PinButtonClass;

	/**
	 * 지도를 어떻게 맞출지. **`Contain` 이 기본이다** —
	 * `Cover` 로 두면 지도가 잘려 그 지역의 핀에 도달할 수 없다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas")
	EFF_AtlasImageFit MapFit = EFF_AtlasImageFit::Contain;

	/** 지도 이미지가 없을 때 대신 그리는 네모의 크기(가로:세로 비). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas")
	FVector2D PlaceholderMapSize = FVector2D(1600.0, 1000.0);

	/**
	 * 국가별 지도 표시 머티리얼. 원본 MapImage는 크기와 핀 등록 기준으로 계속 사용한다.
	 * 해당 국가 키가 없으면 원본 텍스처를 표시하므로 다른 나라에 한국 지형이 섞이지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Appearance")
	TMap<FName, TSoftObjectPtr<UMaterialInterface>> CountryMapDisplayMaterials;

	/** 고정 문구와 글자색을 Designer에 맡긴다. 축제명·운영일·잠금 문구 등 데이터만 갱신한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Appearance")
	bool bUseDesignerTextAndColors = false;

private:
	friend struct FFF_AtlasCountryInputTestAccess;

	/** 지도 이미지는 남기고 동적으로 만든 축제 핀만 제거한다. */
	void ClearPinButtons();

	/** 국가가 바뀌면 지도·핀·카드를 전부 다시 만든다. */
	void RebuildForCountry();

	/** 지도 사각형을 계산해 이미지와 핀에 **같은 값**을 적용한다. */
	void UpdateMapLayout();

	/** 고른 축제로 오른쪽 카드를 채운다. 없으면 카드를 통째로 숨기고 안내만 남긴다. */
	void RefreshFestivalCard();

	/** 런타임 해금 변경을 핀과 카드에 반영하되 현재 선택·포커스는 보존한다. */
	void RefreshFestivalAvailability();

	/**
	 * 이 국가에서 처음 고를 만한 축제. 첫 플레이 가능 축제, 없으면 첫 항목이다.
	 *
	 * ★ **진입할 때 부르지 않는다.** `WASD`·`Enter` 로 플레이어가 지도에 들어올 때만 쓴다.
	 *   진입 시 자동 선택은 2026-08-27 에 없앴다.
	 */
	FName FindDefaultFestivalId() const;

	/** 아직 아무 핀도 고르지 않은 상태에서 지도로 들어간다. 성공하면 true. */
	bool EnterMapFocus();

	/** 마우스·키보드가 같은 경로로 선택 상태와 카드·핀 표현을 갱신한다. */
	void SelectFestival(FName FestivalId, bool bMoveFocus);

	/** 입력 방향의 다음 핀을 찾아 선택·포커스한다. */
	bool MovePinFocus(const FVector2D& ScreenDirection);

	/** 현재 선택이 이 국가에 속하고, 잠금 해제됐으며, 열 레벨도 있는지 재검증한다. */
	bool CanStartSelectedFestival() const;

	/** 자신 또는 내부 CommonButton에 키보드·패드 포커스를 가진 축제 핀. */
	UFF_AtlasPinButton* FindFocusedPin() const;

	UFUNCTION()
	void HandlePinClicked(UFF_CommonButtonBase* Button);

	/** 핀에 포커스가 들어오면 그 축제를 고른다. 마우스 호버도 여기로 들어온다. */
	UFUNCTION()
	void HandlePinFocusReceived(UFF_AtlasPinButton* Pin);

	UFUNCTION()
	void HandleStartClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleBackClicked(UFF_CommonButtonBase* Button);

	const UFF_FestivalAtlasCatalog* GetCatalog() const;

	/** 지도 이미지의 원본 크기. 없으면 `PlaceholderMapSize`. */
	FVector2D GetMapSourceSize() const;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UFF_AtlasPinButton>> PinButtons;

	/** TMap 순서는 비결정적이므로 방향 탐색은 카탈로그 순서를 별도로 보존한다. */
	TArray<FName> NavigationFestivalIds;

	FName CountryId;
	FName SelectedFestivalId;

	/** 마지막으로 계산한 캔버스 크기. 바뀔 때만 다시 배치한다. */
	FVector2D LastCanvasSize = FVector2D::ZeroVector;

	/** 지금 지도가 그려지는 사각형. 핀과 이미지가 공유한다. */
	FFF_AtlasImageRect MapRect;
};
