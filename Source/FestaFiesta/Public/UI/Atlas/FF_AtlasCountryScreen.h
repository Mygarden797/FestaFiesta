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
class USizeBox;
class UTextBlock;

/** 축제를 시작해 달라는 요청. `FName` 은 `FestivalId` 다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnAtlasFestivalStartRequested, FName, FestivalId);

/** 인자 없는 요청 (`뒤로`). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnAtlasCountryScreenRequest);

/**
 * 국가 지도 화면. **지도가 주인공이고 UI 는 왼쪽이다** (2026-08-21 확정).
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
 */
UCLASS(meta = (DisplayName = "FF Atlas Country Screen"))
class FESTAFIESTA_API UFF_AtlasCountryScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
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
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// --- 지도 ---

	/**
	 * 지도 이미지. `MapImage` 가 비면 팔레트 색 네모로 대신 그린다.
	 * 크기·위치는 코드가 `ComputeImageRect` 로 정한다 — 디자이너에서 앵커를 채움으로 두면 된다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UImage> Image_Map;

	/**
	 * 지도와 핀이 함께 올라가는 캔버스. **이 캔버스의 크기가 계산 기준**이다.
	 * `Self Hit Test Invisible` 로 둔다 — 핀만 입력을 받아야 한다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UCanvasPanel> CanvasPanel_Map;

	// --- 왼쪽 축제 카드 ---

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas")
	TObjectPtr<UTextBlock> Text_LockReason;

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

private:
	/** 지도 이미지는 남기고 동적으로 만든 축제 핀만 제거한다. */
	void ClearPinButtons();

	/** 국가가 바뀌면 지도·핀·카드를 전부 다시 만든다. */
	void RebuildForCountry();

	/** 지도 사각형을 계산해 이미지와 핀에 **같은 값**을 적용한다. */
	void UpdateMapLayout();

	/** 고른 축제로 왼쪽 카드를 채운다. 없으면 안내만 남긴다. */
	void RefreshFestivalCard();

	UFUNCTION()
	void HandlePinClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleStartClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleBackClicked(UFF_CommonButtonBase* Button);

	const UFF_FestivalAtlasCatalog* GetCatalog() const;

	/** 지도 이미지의 원본 크기. 없으면 `PlaceholderMapSize`. */
	FVector2D GetMapSourceSize() const;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UFF_AtlasPinButton>> PinButtons;

	FName CountryId;
	FName SelectedFestivalId;

	/** 마지막으로 계산한 캔버스 크기. 바뀔 때만 다시 배치한다. */
	FVector2D LastCanvasSize = FVector2D::ZeroVector;

	/** 지금 지도가 그려지는 사각형. 핀과 이미지가 공유한다. */
	FFF_AtlasImageRect MapRect;
};
