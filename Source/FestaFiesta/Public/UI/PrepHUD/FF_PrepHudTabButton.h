// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "UI/Widgets/FF_HudTabButtonBase.h"
#include "FF_PrepHudTabButton.generated.h"

/**
 * 준비 단계 하단 바의 탭 버튼 하나.
 *
 * 시각 처리는 전부 `UFF_HudTabButtonBase` 에 있다. 여기는 **탭 종류를 `EFF_PrepHudTab` 으로
 * 다루는 얇은 껍데기**다. 베이스가 `int32` 로만 들고 있어서 바가 캐스팅을 하게 두면
 * 캐스팅이 호출부마다 흩어진다.
 *
 * WBP 는 `WBP_FF_PrepHudTabButton` 하나만 만들고 바에 6개를 놓는다.
 * **탭을 런타임에 생성하지 않는다** — 디자이너에서 안 보여 위치·크기를 손으로 못 만진다
 * (2026-08-13 라이브 HUD 에서 실제로 겪었다. docs/memory/wbp-codegen-workflow.md).
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep HUD Tab Button"))
class FESTAFIESTA_API UFF_PrepHudTabButton : public UFF_HudTabButtonBase
{
	GENERATED_BODY()

public:
	void Configure(EFF_PrepHudTab InTab, const FText& InLabel, const FSlateBrush& InIconBrush)
	{
		ConfigureTab(static_cast<int32>(InTab), InLabel, InIconBrush);
	}

	EFF_PrepHudTab GetTab() const { return static_cast<EFF_PrepHudTab>(GetTabId()); }
};
