// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Core/FF_UIFlowStubScreen.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_UIFlowStubScreen::UFF_UIFlowStubScreen()
{
	// HUD 레이어에 올라가는 동안 조작을 먹으면 건축 배치와 카메라가 죽는다.
	ScreenInputMode = EFF_ScreenInputMode::Game;
	bIgnoreInputConfig = true;
	bForceFocusOnActivate = false;
}

void UFF_UIFlowStubScreen::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Advance)
	{
		Button_Advance->OnClicked.AddUniqueDynamic(this, &UFF_UIFlowStubScreen::HandleAdvanceClicked);
	}

	UFF_FestivalStageSubsystem* Stage = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;

	if (!Stage)
	{
		// 조용히 빈 화면을 보여주지 않는다. 무엇이 없어서 비었는지 남긴다.
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowStubScreen: Stage 서브시스템이 없어 단계 이름을 표시하지 못했다."));
		return;
	}

	Stage->OnRemainingSecondsChanged.AddUniqueDynamic(
		this, &UFF_UIFlowStubScreen::HandleRemainingSecondsChanged);

	// 구독만 하면 다음 초가 될 때까지 화면이 비어 있다. 현재값을 한 번 읽는다.
	SyncFromFlow();
}

void UFF_UIFlowStubScreen::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건이다. 여기서 조기 반환하지 않는다.
	if (Button_Advance)
	{
		Button_Advance->OnClicked.RemoveDynamic(this, &UFF_UIFlowStubScreen::HandleAdvanceClicked);
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			Stage->OnRemainingSecondsChanged.RemoveDynamic(
				this, &UFF_UIFlowStubScreen::HandleRemainingSecondsChanged);
		}
	}

	Super::NativeDestruct();
}

void UFF_UIFlowStubScreen::SyncFromFlow()
{
	// --- 프론트엔드: 축제 Stage 는 여기서 의미가 없다 ---
	// Stage 서브시스템은 타이틀 화면에서도 Tick 하며 `셋업` 을 들고 있다. 그걸 그대로 띄우면
	// 플레이 허브 스텁에 `셋업` 이 찍혀 무슨 화면인지 알 수 없게 된다.
	if (UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this))
	{
		if (Flow->GetMode() != EFF_UIFlowMode::Festival)
		{
			if (Text_StageName)
			{
				Text_StageName->SetText(UEnum::GetDisplayValueAsText(Flow->GetFrontendScreen()));
			}

			if (Text_Hint)
			{
				Text_Hint->SetText(LOCTEXT("FlowStubFrontendHint", "프론트엔드 · 임시 화면입니다"));
			}

			if (Text_Clock)
			{
				// 축제 시계는 프론트엔드에서 의미가 없다. 칸을 숨긴다.
				Text_Clock->SetVisibility(ESlateVisibility::Collapsed);
			}

			if (Button_Advance)
			{
				Button_Advance->SetIsEnabled(true);
			}
			return;
		}
	}

	// --- 축제 진행 ---
	// const 로 받지 않는다. IsCurrentStageSkippable() 이 non-const 라 const_cast 가 필요해진다.
	UFF_FestivalStageSubsystem* Stage = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	if (!Stage)
	{
		return;
	}

	if (Text_StageName)
	{
		// 내부 enum 이름을 화면에 쓰지 않는다. 한글 표시명을 Stage 시스템이 준다.
		Text_StageName->SetText(Stage->GetCurrentStageDisplayName());
	}

	if (Text_Hint)
	{
		Text_Hint->SetText(FText::Format(
			LOCTEXT("FlowStubHint", "Day {0} · 임시 화면입니다"),
			FText::AsNumber(Stage->CurrentDay)));
	}

	HandleRemainingSecondsChanged(FMath::CeilToInt(Stage->RemainingTime));

	if (Button_Advance)
	{
		/* 스킵할 수 없는 단계에서 눌리는 버튼을 두지 않는다.
		   Report 는 스킵 대상이 아니지만 `다음 날` 로 넘어가므로 켠다.

		   [주의] 마지막 날 Report 는 넘어갈 다음 날이 없어서 예전에는 껐다. 그런데 이
		   화면이 최종 결산 자리에도 임시로 들어가 있어서, 끄면 축제가 끝난 뒤 아무 데도
		   못 가고 갇힌다. 그래서 켜두고 로비로 보낸다.
		   실제 결산 화면이 붙으면 이 갈래는 지운다.
		*/
		// 마지막 날은 마감에서 축제가 끝나기도 한다(Report 행이 없다) - 끝났다는 사실로 판정한다.
		const bool bFinalReport = Stage->IsFinalDay()
			&& (Stage->IsReportStage() || Stage->HasFestivalEnded());

		const bool bCanAdvance = Stage->IsReportStage() || bFinalReport
			|| Stage->IsCurrentStageSkippable();

		Button_Advance->SetIsEnabled(bCanAdvance);

		if (Text_Hint && bFinalReport)
		{
			Text_Hint->SetText(LOCTEXT("FlowStubFinalHint",
				"축제가 끝났습니다 · 임시 화면 — 진행을 누르면 로비로 돌아갑니다"));
		}
	}
}

void UFF_UIFlowStubScreen::HandleRemainingSecondsChanged(int32 NewSeconds)
{
	if (!Text_Clock)
	{
		return;
	}

	// 프론트엔드에서도 Stage 시계가 계속 tick 하므로 이 알림이 온다.
	// 막지 않으면 SyncFromFlow 가 숨긴 시계 칸이 다시 켜진다.
	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	if (Flow && Flow->GetMode() != EFF_UIFlowMode::Festival)
	{
		Text_Clock->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 값이 없으면 칸을 숨긴다. 0 이나 - 를 쓰지 않는다.
	if (NewSeconds <= 0)
	{
		Text_Clock->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Text_Clock->SetText(FText::Format(
		LOCTEXT("FlowStubRemaining", "남은 시간 {0}초"), FText::AsNumber(NewSeconds)));
	Text_Clock->SetVisibility(ESlateVisibility::Visible);
}

void UFF_UIFlowStubScreen::HandleAdvanceClicked()
{
	// UI 가 Stage 를 직접 바꾸지 않는다. 어느 함수를 부를지도 UI 가 계산하지 않는다.
	if (UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this))
	{
		Flow->RequestAdvanceFlow();
	}
}

#undef LOCTEXT_NAMESPACE
