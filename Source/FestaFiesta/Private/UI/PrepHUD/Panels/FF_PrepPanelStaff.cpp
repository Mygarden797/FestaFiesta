// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepPanelStaff.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepStaff"

// =============================================================================
// FFF_PrepStaffRules — 순수 함수
// =============================================================================

EFF_PrepHireBlockReason FFF_PrepStaffRules::ResolveBlockReason(int32 Affordable, int32 RemainingCap)
{
	if (Affordable == INDEX_NONE || RemainingCap == INDEX_NONE)
	{
		return EFF_PrepHireBlockReason::Unknown;
	}

	// ★ 상한을 잔액보다 먼저 본다. 상한을 다 쓴 상태에서 `돈을 모으세요` 라고 하면
	//   돈을 모아도 안 되는 행동을 안내하게 된다.
	if (RemainingCap <= 0)
	{
		return EFF_PrepHireBlockReason::CapReached;
	}

	if (Affordable <= 0)
	{
		return EFF_PrepHireBlockReason::NotEnoughGold;
	}

	return EFF_PrepHireBlockReason::None;
}

FFF_PrepHireQuote FFF_PrepStaffRules::BuildQuote(
	int32 DesiredCount,
	int32 Affordable,
	int32 RemainingCap,
	int32 HireCostPerStaff,
	int32 Gold)
{
	FFF_PrepHireQuote Result;
	Result.BlockReason = ResolveBlockReason(Affordable, RemainingCap);

	// 모르는 상태에서는 인원도 금액도 만들지 않는다. 0 을 넣으면 "공짜"·"고용 불가"가 사실이 된다.
	if (Result.BlockReason == EFF_PrepHireBlockReason::Unknown)
	{
		return Result;
	}

	Result.MaxHireable = FMath::Max(0, Affordable);
	Result.SelectedCount = FMath::Clamp(DesiredCount, 0, Result.MaxHireable);

	if (HireCostPerStaff == INDEX_NONE)
	{
		// 고용비 단가를 모르면 금액 칸을 비운다. 인원은 이미 소유자 판정에서 나온 값이라 유효하다.
		return Result;
	}

	Result.HireCostTotal = Result.SelectedCount * HireCostPerStaff;

	if (Gold != INDEX_NONE)
	{
		Result.GoldAfterHire = Gold - Result.HireCostTotal;
	}
	return Result;
}

// =============================================================================
// UFF_PrepPanelStaff
// =============================================================================

void UFF_PrepPanelStaff::NativePreConstruct()
{
	Super::NativePreConstruct();

	// ★ 영구 고용이라는 안내는 비어 있으면 안 된다. 값을 안 넣은 WBP 도 문구가 나오게 기본을 채운다.
	if (DailyNoticeText.IsEmpty())
	{
		DailyNoticeText = LOCTEXT("PrepStaff_HireNotice",
			"고용한 인원은 축제가 끝날 때까지 남습니다. 일찍 뽑을수록 오래 씁니다.");
	}

	if (Text_DailyNotice)
	{
		Text_DailyNotice->SetText(DailyNoticeText);
	}
}

void UFF_PrepPanelStaff::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 내부 버튼이라 인스턴스 수명과 같다. 외부 시스템 구독만 패널을 열 때 붙인다.
	if (Button_Decrease)
	{
		Button_Decrease->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelStaff::HandleDecreaseClicked);
	}
	if (Button_Increase)
	{
		Button_Increase->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelStaff::HandleIncreaseClicked);
	}
	if (Button_Hire)
	{
		Button_Hire->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelStaff::HandleHireClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Button_Hire 가 없어 고용할 방법이 없다."), *GetName());
	}
}

void UFF_PrepPanelStaff::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// 열 때마다 기본값에서 시작한다. 이전에 고른 값이 남아 있으면 잔액이 달라진 뒤에도
	// 같은 인원이 선택돼 있어 "왜 이 숫자인지" 설명되지 않는다.
	DesiredCount = FMath::Max(0, DefaultSelectedCount);

	BindViewModel();
	RefreshAll();
}

void UFF_PrepPanelStaff::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

// =============================================================================
// 구독
// =============================================================================

UFF_FestivalViewModelSubsystem* UFF_PrepPanelStaff::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_PrepPanelStaff::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 인원·잔액이 전부 비어 보인다."), *GetName());
		return;
	}

	// 잔액이 바뀌면 고용 가능 인원이 바뀐다. 인원이 바뀌면 상한 여유가 바뀐다.
	// 둘 다 구독하지 않으면 다른 패널에서 돈을 쓴 뒤 이 화면이 거짓 견적을 띄운다.
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_PrepPanelStaff::HandleGoldChanged);
	CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_PrepPanelStaff::HandleStaffCountChanged);

}

void UFF_PrepPanelStaff::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_PrepPanelStaff::HandleGoldChanged);
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_PrepPanelStaff::HandleStaffCountChanged);
	}
	CachedViewModel = nullptr;
}

// =============================================================================
// 갱신
// =============================================================================

void UFF_PrepPanelStaff::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();

	const int32 Base = ViewModel ? ViewModel->GetBaseStaffCount() : INDEX_NONE;
	const int32 Hired = ViewModel ? ViewModel->GetHiredStaffCount() : INDEX_NONE;
	const int32 Total = ViewModel ? ViewModel->GetTotalStaffCount() : INDEX_NONE;
	const int32 MaxHired = ViewModel ? ViewModel->GetMaxHiredStaffCount() : INDEX_NONE;
	const int32 HireCost = ViewModel ? ViewModel->GetStaffHireCostGold() : INDEX_NONE;
	const int32 Gold = ViewModel ? ViewModel->GetGold() : INDEX_NONE;

	Quote = FFF_PrepStaffRules::BuildQuote(
		DesiredCount,
		ViewModel ? ViewModel->GetAffordableHireCount() : INDEX_NONE,
		ViewModel ? ViewModel->GetRemainingHireCap() : INDEX_NONE,
		HireCost,
		Gold);

	// --- 현재 인원 ---
	//
	// 내역(기본 · 고용)은 이 화면에서 플레이어가 직접 바꾸는 숫자라 지우지 않고 뒤에 덧붙인다.
	// 프로그램은 인원을 묶지 않으므로 전체가 곧 사건에 쓸 수 있는 인원이다.
	SetTextOrHide(Text_CurrentStaff, (Total >= 0 && Base >= 0 && Hired >= 0)
		? FText::Format(
			LOCTEXT("PrepStaff_Current", "{0}명 (기본 {1} · 고용 {2})"),
			FText::AsNumber(Total), FText::AsNumber(Base), FText::AsNumber(Hired))
		: FText::GetEmpty());

	// 최대는 기본 팀 + 총원 상한이다. 상한만 쓰면 "최대 6명"으로 읽혀 절반이 사라진다.
	SetTextOrHide(Text_MaxStaff, (Base >= 0 && MaxHired >= 0)
		? FText::Format(
			LOCTEXT("PrepStaff_Max", "최대 {0}명 (기본 {1} + 고용 {2})"),
			FText::AsNumber(Base + MaxHired), FText::AsNumber(Base), FText::AsNumber(MaxHired))
		: FText::GetEmpty());

	// --- 스텝퍼 ---
	if (Text_SelectedCount)
	{
		Text_SelectedCount->SetText(FText::AsNumber(Quote.SelectedCount));
	}
	if (Button_Decrease)
	{
		Button_Decrease->SetIsEnabled(Quote.SelectedCount > 0);
	}
	if (Button_Increase)
	{
		Button_Increase->SetIsEnabled(Quote.SelectedCount < Quote.MaxHireable);
	}

	// --- 견적 ---
	SetTextOrHide(Text_WagePreview, (Quote.HireCostTotal != INDEX_NONE && HireCost != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepStaff_HirePreview", "{0}명 × {1} = {2} Gold"),
			FText::AsNumber(Quote.SelectedCount), FText::AsNumber(HireCost), FText::AsNumber(Quote.HireCostTotal))
		: FText::GetEmpty());

	SetTextOrHide(Text_GoldAfterHire, (Quote.GoldAfterHire != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepStaff_GoldAfter", "고용 후 잔액 {0}"),
			FText::AsNumber(Quote.GoldAfterHire))
		: FText::GetEmpty());

	if (Button_Hire)
	{
		Button_Hire->SetIsEnabled(Quote.CanHire());
	}

	// !! 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011)
	FText BlockReason;
	switch (Quote.BlockReason)
	{
	case EFF_PrepHireBlockReason::CapReached:
		BlockReason = (MaxHired >= 0)
			? FText::Format(
				LOCTEXT("PrepStaff_CapReached", "고용 상한 {0}명을 다 썼습니다. 더 뽑을 수 없습니다."),
				FText::AsNumber(MaxHired))
			: LOCTEXT("PrepStaff_CapReachedNoMax", "고용 상한을 다 썼습니다. 더 뽑을 수 없습니다.");
		break;

	case EFF_PrepHireBlockReason::NotEnoughGold:
		BlockReason = (HireCost != INDEX_NONE)
			? FText::Format(
				LOCTEXT("PrepStaff_NotEnoughGold", "1명 고용비 {0} Gold 를 낼 잔액이 없습니다."),
				FText::AsNumber(HireCost))
			: LOCTEXT("PrepStaff_NotEnoughGoldNoCost", "고용비를 낼 잔액이 없습니다.");
		break;

	case EFF_PrepHireBlockReason::Unknown:
		BlockReason = LOCTEXT("PrepStaff_Unknown", "축제가 아직 시작되지 않아 인력 정보를 읽을 수 없습니다.");
		break;

	default:
		// 고용은 가능한데 0명을 고른 상태. 버튼이 꺼진 이유를 그래도 설명한다.
		if (Quote.SelectedCount <= 0)
		{
			BlockReason = LOCTEXT("PrepStaff_ZeroSelected", "고용할 인원을 1명 이상 선택하세요.");
		}
		break;
	}
	SetTextOrHide(Text_HireBlockReason, BlockReason);

	// 이번 축제에 고용비로 쓴 누계다. 1회 구매라 "/ 일" 이 아니다.
	SetTextOrHide(Text_WageCost, (Hired >= 0 && HireCost != INDEX_NONE)
		? FText::Format(LOCTEXT("PrepStaff_HireSpent", "{0} (누계)"), FText::AsNumber(Hired * HireCost))
		: FText::GetEmpty());
}

void UFF_PrepPanelStaff::ChangeSelection(int32 Delta)
{
	// 원하는 값은 상한과 무관하게 들고 있는다. 잔액이 회복되면 그 값으로 되돌아간다.
	// 상한을 넘긴 값은 `BuildQuote` 가 표시할 때 잘라 준다.
	DesiredCount = FMath::Max(0, DesiredCount + Delta);
	RefreshAll();
}

void UFF_PrepPanelStaff::HandleDecreaseClicked()
{
	// 눌린 시점의 실제 표시값을 기준으로 내린다. 상한에 잘려 있던 경우
	// `DesiredCount` 에서 1을 빼면 화면 숫자가 안 움직인다.
	DesiredCount = Quote.SelectedCount;
	ChangeSelection(-1);
}

void UFF_PrepPanelStaff::HandleIncreaseClicked()
{
	DesiredCount = Quote.SelectedCount;
	ChangeSelection(1);
}

void UFF_PrepPanelStaff::HandleHireClicked()
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 이 없어 고용을 요청할 수 없다."), *GetName());
		return;
	}

	const int32 RequestCount = Quote.SelectedCount;
	if (RequestCount <= 0)
	{
		// 버튼이 꺼져 있어야 하는 상태다. 여기 왔다면 표시와 상태가 갈라진 것이므로 남긴다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 0명 고용 요청이 들어왔다. 버튼 활성 조건을 확인한다."), *GetName());
		return;
	}

	// !! 낙관적 갱신을 하지 않는다 !!
	// 부분 고용이 없어서 실패하면 아무것도 바뀌지 않는다. 화면은 결과 이벤트로만 갱신된다.
	if (!ViewModel->RequestHireStaff(RequestCount))
	{
		// 실패해도 이벤트가 오지 않으므로 여기서 한 번 다시 그린다 —
		// 그러지 않으면 거부된 이유가 화면에 반영되지 않는다.
		RefreshAll();
	}
}

// =============================================================================
// 결과 이벤트
// =============================================================================

void UFF_PrepPanelStaff::HandleGoldChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_PrepPanelStaff::HandleStaffCountChanged(int32 NewValue)
{
	// 고용이 성공했으면 선택을 초기값으로 되돌린다. 그러지 않으면 같은 인원이 선택된 채로
	// 남아 실수로 두 번 고용하기 쉽다.
	DesiredCount = FMath::Max(0, DefaultSelectedCount);
	RefreshAll();
}

// =============================================================================
// 표시 헬퍼
// =============================================================================

void UFF_PrepPanelStaff::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}

	TextBlock->SetText(Value);
	TextBlock->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

void UFF_PrepPanelStaff::SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value)
{
	SetTextOrHide(TextBlock, Value);

	if (!Group)
	{
		return;
	}

	Group->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
