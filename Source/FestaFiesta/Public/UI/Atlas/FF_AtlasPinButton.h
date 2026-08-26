// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_CommonButtonBase.h"
#include "FF_AtlasPinButton.generated.h"

class UImage;
class UTextBlock;

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

protected:
	// 핀 이름은 베이스의 `Text_Label`(UCommonTextBlock, 필수 바인딩)이 그린다.
	// 여기 따로 두지 않는다 — 위젯 둘이 같은 문구를 그리면 하나만 갱신되는 사고가 난다.

	/** `축제 3`·`부산` 같은 배지. 비면 숨는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UTextBlock> Text_PinBadge;

	/** 자물쇠. 색만으로 잠금을 알리지 않기 위한 두 번째 신호다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Atlas|Pin")
	TObjectPtr<UImage> Image_PinLock;

private:
	FName PinId;
	bool bSelectable = false;
	FText LockReason;
};
