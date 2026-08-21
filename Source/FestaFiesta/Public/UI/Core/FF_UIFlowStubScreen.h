// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Time/FF_FestivalStage.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_UIFlowStubScreen.generated.h"

class UButton;
class UTextBlock;

/**
 * Stage 마다 올라가는 **임시 화면.** 지금 어느 단계인지와 남은 시간을 보여주고 다음으로 보낸다.
 *
 * === 왜 스텁을 먼저 만드나 ===
 * 화면 12개를 하나씩 완성한 뒤에 이어 붙이면 **한 번도 실행해 보지 못한 상태로** 여섯 단계를
 * 만들게 된다. `dev-schedule.md` 의 원칙은 "깊이보다 관통을 먼저 — 가장 큰 리스크는 기능 부족이
 * 아니라 통합 실패" 다. 스텁으로 한 바퀴가 돌면 화면 하나를 만들 때마다 전체 사이클로 확인할 수 있다.
 *
 * === 교체 방법 ===
 * `Project Settings > Game > FestaFiesta UI Flow > Stage Screens` 에서 그 Stage 의 클래스를
 * 실제 화면으로 바꾼다. **흐름 코드를 고치지 않는다.**
 *
 * === 남은 시간을 띄우는 이유 ===
 * 통관 점검에 "Live 중 패널을 열어도 시계가 계속 가는가" 가 있다. 스텁이 그것을 눈으로
 * 확인시켜 주면 별도 디버그 UI 를 만들 필요가 없다.
 *
 * === 입력을 가로채지 않는다 ===
 * `UFF_ActivatableScreenBase` 파생이지만 생성자에서 `bIgnoreInputConfig = true` 와
 * `ScreenInputMode = Game` 으로 둔다. HUD 레이어에 올라가는 동안 조작을 먹으면
 * 건축 배치와 카메라가 죽는다.
 *
 * ⚠️ 그래서 **결산 계열 풀스크린 시트로 쓰기에는 부족하다.** 시트는 메뉴 입력을 독점해야 한다.
 * 스텁 단계에서는 전부 HUD 레이어로 두고, 실제 시트가 들어오는 U7 에서 Layer 를 화면 스택으로
 * 바꾼다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF UI Flow Stub Screen"))
class FESTAFIESTA_API UFF_UIFlowStubScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_UIFlowStubScreen();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleAdvanceClicked();

	/** 남은 시간이 정수 초로 바뀔 때마다 불린다. */
	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewSeconds);

	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 한다 ---

	/** 지금 단계의 한글 이름. `GetCurrentStageDisplayName()` 이 준다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_StageName;

	/** `Day 1 · 임시 화면입니다` 같은 보조 문구. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Hint;

	/** 남은 시간. 이 숫자가 계속 줄어드는지가 시계 확인 지점이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Clock;

	/** 다음 단계로 보내 달라고 요청한다. 스킵할 수 없는 단계에서는 비활성으로 둔다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_Advance;

private:
	/**
	 * 구독한 직후 현재값을 한 번 읽는다. 이벤트는 미래만 알려준다 (Bind + Sync).
	 * 프론트엔드면 화면 이름을, 축제 중이면 Stage 이름과 남은 시간을 표시한다.
	 */
	void SyncFromFlow();
};
