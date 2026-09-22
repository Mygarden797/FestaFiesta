// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_CommonButtonBase.h"
#include "FF_AtlasPinButton.generated.h"

class UFF_AtlasPinButton;
class UImage;
class UTextBlock;

/**
 * 이 핀에 포커스가 들어왔다.
 *
 * ★ 호버를 따로 알리지 않는다. `UFF_CommonButtonBase::NativeOnHovered` 가 `bFocusOnHover` 로
 *   마우스 진입을 그 자리에서 포커스로 바꾸기 때문에, 마우스와 패드가 이미 같은 경로를 탄다.
 *   호버 델리게이트를 따로 두면 마우스일 때만 두 번 불린다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnAtlasPinFocusReceived, UFF_AtlasPinButton*, Pin);

/**
 * 지구본의 **국가 핀**과 국가 지도의 **축제 핀**을 겸하는 버튼.
 *
 * === 왜 하나로 두는가 ===
 * 둘이 보여주는 것이 같다 — 이름, 작은 배지, 자물쇠. 클래스를 둘로 나누면 같은 잠금 표시
 * 코드를 두 번 쓰고, 한쪽만 AT-011 을 지키는 상태가 생긴다.
 * 담는 것이 국가인지 축제인지는 `PinId` 가 무엇을 가리키느냐의 차이뿐이다.
 *
 * === 🔴 클릭 판정은 이 버튼이 한다 ===
 * 확정안이 "국가 클릭 판정 = **넓은 UMG 핀 버튼**. 조형물 메시로 판정하지 않는다" 다
 * (`visual-direction.md` 4절). 그래서 지구 액터의 모든 컴포넌트는 콜리전이 꺼져 있고,
 * 이 버튼의 히트 영역이 실제 판정 영역이다. **작게 만들지 않는다** — 핀은 작아 보여도
 * 누를 곳은 넉넉해야 한다.
 *
 * === 잠긴 핀 ===
 * **포커스는 간다. 선택만 막는다.** 그래서 `SetIsEnabled(false)` 를 쓰지 않는다 —
 * 비활성 버튼은 게임패드 포커스에서 빠져 "그 나라가 존재하지 않는 것"처럼 된다.
 * 대신 눌렸을 때 화면이 사유를 띄운다. 색만으로 알리지 않는다 (AT-011) — 자물쇠와 문구를 함께 쓴다.
 */
UCLASS(meta = (DisplayName = "FF Atlas Pin Button"))
class FESTAFIESTA_API UFF_AtlasPinButton : public UFF_CommonButtonBase
{
	GENERATED_BODY()

public:
	/**
	 * 포커스가 이 핀으로 들어왔다고 알린다.
	 *
	 * 국가 지도 화면이 이걸 듣고 축제 카드를 채운다. **핀을 고르기 전에는 카드가 없다**는
	 * 규칙(2026-08-27 결정)을 지키려면 "선택"의 출발점이 화면의 자동 선택이 아니라
	 * 플레이어의 포커스 이동이어야 한다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Atlas|Pin")
	FFF_OnAtlasPinFocusReceived OnPinFocusReceived;

	/**
	 * 핀 하나의 표시 내용을 한 번에 넣는다.
	 *
	 * ★ 따로 넣는 setter 를 두지 않는다. 이름과 배지를 따로 넣으면 "대한민국 / 축제 3" 처럼
	 *   어긋난 조합이 화면에 뜬다 — `UFF_MetricRowWidget::SetMetric` 이 같은 이유로 한 번에 받는다.
	 *
	 * `Badge` 가 비면 배지 칸을 숨긴다. `0` 이나 `-` 를 넣지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas|Pin")
	void SetPinData(FName InPinId, const FText& Title, const FText& Badge, bool bInSelectable, const FText& InLockReason);

	/**
	 * 지구 뒤편으로 넘어갔을 때 숨긴다.
	 * `Collapsed` 가 아니라 `Hidden` 을 쓴다 — 캔버스 슬롯의 절대 좌표를 계속 갱신하므로
	 * 레이아웃에서 빼지 않아도 되고, 다시 보일 때 위치가 이미 맞아 있다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas|Pin")
	void SetPinVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas|Pin")
	FName GetPinId() const { return PinId; }

	/** 고를 수 있는가. 잠긴 핀도 포커스는 가므로 `IsEnabled` 와 다른 값이다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas|Pin")
	bool IsSelectable() const { return bSelectable; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas|Pin")
	FText GetLockReason() const { return LockReason; }

	/** 국가 지도에서 현재 확정된 축제 핀인지 표시한다. 포커스와 별개로 유지된다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas|Pin")
	void SetSelectedState(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas|Pin")
	bool IsSelectedState() const { return bSelected; }

	/**
	 * 지도 좌표가 실제로 닿는 지점. 위젯 안에서의 비율이다.
	 *
	 * 핀 모양마다 다르다 — 아래가 뾰족한 조형물 핀은 `(0.5, 1)`(바닥 중앙)이고,
	 * 점 마커는 점의 한가운데다. 화면이 이 값을 캔버스 슬롯 정렬에 그대로 넣는다.
	 *
	 * ★ 화면에 숫자를 박지 않고 핀이 들고 있는 이유는, 핀 WBP 를 바꿀 때 화면 코드를 같이
	 *   고쳐야 하는 상태를 만들지 않기 위해서다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas|Pin")
	FVector2D GetMapAnchor() const { return MapAnchor; }

protected:
	/** 위 `GetMapAnchor` 참고. 기본값은 옛 조형물 핀 기준이라 기존 WBP 가 그대로 동작한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Pin")
	FVector2D MapAnchor = FVector2D(0.5, 1.0);

	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

	// 핀 이름은 베이스의 `Text_Label`(UCommonTextBlock, 필수 바인딩)이 그린다.
	// 여기 따로 두지 않는다 — 위젯 둘이 같은 문구를 그리면 하나만 갱신되는 사고가 난다.

	/**
	 * 핀 아래 한 줄. 고를 수 있으면 `부산` 같은 배지, **잠겨 있으면 `준비 중`** 이다.
	 *
	 * 🔴 색·모양 말고 **글자로도** 잠금을 알리기 위한 자리다 (AT-011). 지도 위 마커는 작아서
	 * 채움/테두리 차이만으로는 구분이 안 된다 — `icon-inventory.md` 가 같은 이유로 `● ◐ ○` 를 금지한다.
	 * 그래서 잠긴 핀에서는 배지 대신 상태 문구가 나간다. 지역 이름은 카드에서 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UTextBlock> Text_PinBadge;

	/** 자물쇠. 색만으로 잠금을 알리지 않기 위한 두 번째 신호다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UImage> Image_PinLock;

	/** 자연지도용 기본 핀. 기존 텍스트형 WBP에는 없어도 된다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UImage> Image_PinDefault;

	/** 자연지도용 선택 핀. `SetSelectedState`가 기본 핀과 교대한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UImage> Image_PinSelected;

	/** 선택 핀 위의 이중 화살표. 색뿐 아니라 형태로 선택을 알린다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UImage> Image_PinChevrons;

	/** 밝은 지도 핀의 자물쇠/체크 문양. 없으면 기존 핀 표시를 그대로 쓴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UImage> Image_PinLockGlyph;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UImage> Image_PinSelectedGlyph;

private:
	void RefreshPinVisual();

	FName PinId;

	/** `SetPinData` 가 받은 배지 원문. 잠금 문구로 갈아 끼웠다가 되돌리려면 원문이 필요하다. */
	FText BadgeText;

	bool bSelectable = false;
	bool bSelected = false;
	FText LockReason;
};
