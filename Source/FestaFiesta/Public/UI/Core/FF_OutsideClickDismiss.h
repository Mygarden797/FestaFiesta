// 비모달 HUD의 바깥 클릭 닫기. 화면 상태만 바꾸며 시뮬레이션/입력 모드는 소유하지 않는다.
#pragma once

#include "CoreMinimal.h"

class UUserWidget;
class UWidget;

namespace FFOutsideClickDismiss
{
	/**
	 * NativeConstruct에서 등록하고 NativeDestruct에서 해제한다.
	 * KeepOpenAt는 패널 본문·메뉴 바처럼 빈 여백도 내부로 취급할 실제 영역을 판정한다.
	 * 다른 UI 위의 클릭은 히트 경로로 보호하므로 드롭다운·버튼을 따로 나열할 필요는 없다.
	 */
	FESTAFIESTA_API void Register(UUserWidget* Owner, TFunction<bool()> HasOpenPanel,
		TFunction<bool(FVector2D)> KeepOpenAt, FSimpleDelegate Dismiss);
	FESTAFIESTA_API void Unregister(UUserWidget* Owner);

	/** Slate 절대 좌표를 사용하므로 창 크기·DPI·패널 드래그 위치가 그대로 반영된다. */
	FESTAFIESTA_API bool IsInside(const UWidget* Widget, FVector2D ScreenPosition);
}
