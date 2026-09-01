// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ClosingHUD/Panels/FF_ClosingPanelFinale.h"

#include "Components/TextBlock.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaClosingFinale"

void UFF_ClosingPanelFinale::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// ★ 여기서 못 사는 이유를 적는다. 등급만 보여 주면 "바꿀 수 있나" 를 시도하게 된다.
	SetTextOrHide(Text_MissingNote, LOCTEXT("ClosingFinale_MissingNote",
		"피날레는 셋업에서만 고를 수 있고 축제당 한 번입니다. 여기서는 결과만 봅니다."));

	BindViewModel();
	RefreshAll();
}

void UFF_ClosingPanelFinale::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

UFF_FestivalViewModelSubsystem* UFF_ClosingPanelFinale::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_ClosingPanelFinale::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 피날레 정보가 비어 보인다."), *GetName());
		return;
	}

	CachedViewModel->OnPurchasedFinaleGradeChanged.AddUniqueDynamic(this, &UFF_ClosingPanelFinale::HandleFinaleGradeChanged);

	// 🔴 단계도 받는다. **`예정` 과 `완료` 를 가르는 것이 단계**이기 때문이다 —
	// 등급은 안 바뀌는데 3일차 피날레 연출을 지나면 문구가 달라져야 한다.
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_ClosingPanelFinale::HandleStageViewChanged);
}

void UFF_ClosingPanelFinale::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnPurchasedFinaleGradeChanged.RemoveDynamic(this, &UFF_ClosingPanelFinale::HandleFinaleGradeChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_ClosingPanelFinale::HandleStageViewChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_ClosingPanelFinale::HandleFinaleGradeChanged(EFF_FinaleGrade NewGrade)
{
	RefreshAll();
}

void UFF_ClosingPanelFinale::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	RefreshAll();
}

void UFF_ClosingPanelFinale::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	const EFF_FinaleGrade Grade = ViewModel->GetPurchasedFinaleGrade();

	// === 어느 시점인가 ===
	// 🔴 **마지막 날인지로 판단한다.** 피날레는 3일차 라이브가 끝나면 터지고, 이 패널은
	// 그 뒤 마감·리포트에서만 보인다. 즉 마지막 날에 이 화면을 보고 있다면 이미 터진 것이다.
	// `UFF_FinaleShowSubsystem` 에 "터졌는가" 를 묻는 편이 정확하지만 그 표면이 아직 없다.
	if (Grade == EFF_FinaleGrade::None)
	{
		Phase = EFF_ClosingFinalePhase::NotPurchased;
	}
	else
	{
		Phase = ViewModel->IsFinalDay()
			? EFF_ClosingFinalePhase::Done
			: EFF_ClosingFinalePhase::Scheduled;
	}

	// 문구는 `UFF_EconomyDisplayTextLibrary` 가 소유한다. enum `DisplayName` 을 읽지 않는다.
	SetTextOrHide(Text_Grade, UFF_EconomyDisplayTextLibrary::GetFinaleGradeText(Grade));

	const int32 Bonus = ViewModel->GetFinaleSatisfactionBonus();
	SetTextOrHide(Text_Bonus, (Bonus != INDEX_NONE)
		? FText::Format(LOCTEXT("ClosingFinale_Bonus", "만족도 +{0}"), FText::AsNumber(Bonus))
		: LOCTEXT("ClosingFinale_BonusUnknown", "—"));

	// !! 숫자만 두지 않는다 !! 이미 터진 것인지 앞으로 터질 것인지는 숫자로 알 수 없다 (AT-011).
	FText PhaseNote;
	switch (Phase)
	{
	case EFF_ClosingFinalePhase::Done:
		PhaseNote = LOCTEXT("ClosingFinale_Done",
			"피날레가 끝났습니다. 이 보너스는 평가 점수에 이미 들어가 있습니다.");
		break;

	case EFF_ClosingFinalePhase::Scheduled:
		PhaseNote = LOCTEXT("ClosingFinale_Scheduled",
			"3일차 라이브가 끝나면 실행됩니다. 보너스는 그때 평가 점수에 들어갑니다.");
		break;

	default:
		// 탭이 숨어 있어서 정상적으로는 여기 안 온다. 와 버렸다면 그대로 알린다.
		PhaseNote = LOCTEXT("ClosingFinale_NotPurchased", "피날레를 사지 않았습니다.");
		break;
	}
	SetTextOrHide(Text_PhaseNote, PhaseNote);
}

#undef LOCTEXT_NAMESPACE
