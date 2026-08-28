// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/FF_HudTabButtonBase.h"
#include "FF_PrepChoiceButton.generated.h"

/**
 * 준비 단계 패널 안에서 **여러 개 중 하나를 고르는** 작은 버튼. 날씨 선택이 첫 사용처다.
 *
 * === 왜 탭 버튼 베이스를 재사용하나 ===
 * 필요한 것이 `id + 라벨 + 선택 상태` 뿐이고, 선택 상태를 **배경 브러시 교체로** 표현하는
 * 처리가 이미 `UFF_HudTabButtonBase` 에 있다. 새로 만들면 그 브러시 상태 코드가 세 벌이 되고,
 * 그중 하나는 반드시 색만 바꾸는 구현이 된다 (AT-011 위반).
 *
 * 탭이 아니라 선택지이므로 이름만 다르다. WBP 는 `WBP_FF_PrepChoiceButton` 하나면 되고,
 * 쓰는 패널이 라벨과 id 를 넣어 준다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Choice Button"))
class FESTAFIESTA_API UFF_PrepChoiceButton : public UFF_HudTabButtonBase
{
	GENERATED_BODY()

public:
	/**
	 * 선택지 하나를 설정한다. `InChoiceId` 는 쓰는 쪽의 enum 을 `int32` 로 캐스팅한 값이다.
	 * 아이콘은 쓰지 않으므로 빈 브러시를 넘긴다 — 베이스가 텍스처 없는 아이콘을 접는다.
	 */
	void ConfigureChoice(int32 InChoiceId, const FText& InLabel)
	{
		ConfigureTab(InChoiceId, InLabel, FSlateBrush());
	}

	int32 GetChoiceId() const { return GetTabId(); }
};
