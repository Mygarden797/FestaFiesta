// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LiveHudTypes.h"
#include "UI/Widgets/FF_HudTabButtonBase.h"
#include "FF_LiveTabButton.generated.h"

/**
 * 라이브 운영 HUD 하단 바의 탭 버튼 하나.
 *
 * 시각 처리는 전부 `UFF_HudTabButtonBase` 에 있다. 여기는 **탭 종류를 `EFF_LiveTab` 으로
 * 다루는 얇은 껍데기**다. 베이스가 `int32` 로만 들고 있어서 바가 캐스팅을 하게 두면
 * 캐스팅이 호출부마다 흩어진다. `UFF_PrepHudTabButton` 과 같은 모양이다.
 *
 * === 기존 `UFF_LiveHudTabButton` 과 다른 것이다 ===
 * 그쪽은 `UFF_HudTabButtonBase` 를 쓰지 않고 같은 시각 처리를 한 벌 더 갖고 있다.
 * 이 새 HUD 는 처음부터 베이스를 쓴다 — 그래야 선택 표현이 세 벌로 갈라지지 않는다 (AT-011).
 * 기존 위젯은 2026-08-27 지시대로 건드리지 않고 남겨 뒀다.
 *
 * WBP 는 `WBP_FF_LiveTabButton` 하나만 만들고 바에 7개를 놓는다.
 * **탭을 런타임에 생성하지 않는다** — 디자이너에서 안 보여 위치·크기를 손으로 못 만진다
 * (2026-08-13 라이브 HUD 에서 실제로 겪었다. docs/memory/wbp-codegen-workflow.md).
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Tab Button"))
class FESTAFIESTA_API UFF_LiveTabButton : public UFF_HudTabButtonBase
{
	GENERATED_BODY()

public:
	void Configure(EFF_LiveTab InTab, const FText& InLabel, const FSlateBrush& InIconBrush)
	{
		ConfigureTab(static_cast<int32>(InTab), InLabel, InIconBrush);
	}

	EFF_LiveTab GetTab() const { return static_cast<EFF_LiveTab>(GetTabId()); }
};
