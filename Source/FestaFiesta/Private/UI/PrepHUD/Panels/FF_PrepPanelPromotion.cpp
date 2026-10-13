// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepPanelPromotion.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "UI/FF_UILog.h"
#include "UI/Frontend/FF_ConfirmDialog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepPromotion"

// =============================================================================
// FFF_PrepPromotionRules — 순수 함수
// =============================================================================

EFF_PrepPromotionBlockReason FFF_PrepPromotionRules::ResolveBlockReason(
	EFF_PromotionChoice Choice,
	EFF_PromotionChoice Purchased,
	bool bCanPurchaseNow,
	int32 CostGold,
	int32 Gold)
{
	// ② 이미 산 사람에게 `셋업이 지났습니다` 는 못 산 것처럼 읽힌다. 단계보다 먼저 본다.
	if (Purchased != EFF_PromotionChoice::None)
	{
		return EFF_PrepPromotionBlockReason::AlreadyPurchased;
	}

	// ③ 단계가 지났으면 등급을 골라도 소용없다.
	if (!bCanPurchaseNow)
	{
		return EFF_PrepPromotionBlockReason::NotSetupStage;
	}

	// ④ `안 함` 은 사는 것이 아니다. 고르지 않은 것과 같이 다룬다.
	if (Choice == EFF_PromotionChoice::None)
	{
		return EFF_PrepPromotionBlockReason::NoChoiceSelected;
	}

	// ① 비용을 못 읽었으면 살 수 있다고 알리지 않는다. 눌러도 실패하는 버튼이 더 나쁘다.
	if (CostGold == INDEX_NONE)
	{
		return EFF_PrepPromotionBlockReason::Unknown;
	}

	// ⑤ 잔액. 모르면 막지 않는다 — 최종 판정은 `TrySpend` 가 한다.
	if (Gold != INDEX_NONE && Gold < CostGold)
	{
		return EFF_PrepPromotionBlockReason::NotEnoughGold;
	}

	return EFF_PrepPromotionBlockReason::None;
}

FFF_PrepPromotionQuote FFF_PrepPromotionRules::BuildQuote(
	const FFF_PromotionOptionView* Option,
	EFF_PromotionChoice Purchased,
	bool bCanPurchaseNow,
	int32 Gold)
{
	FFF_PrepPromotionQuote Result;

	// 고른 것이 없으면 등급도 비용도 모른다. `안 함` 을 고른 것과 구분된다 —
	// 그쪽은 0 Gold 라는 확정값이 있다.
	const EFF_PromotionChoice Choice = Option ? Option->Choice : EFF_PromotionChoice::None;
	const int32 CostGold = Option ? Option->CostGold : INDEX_NONE;

	Result.Choice = Choice;
	Result.CostGold = CostGold;
	Result.VisitorMultiplier = Option ? Option->VisitorMultiplier : 0.f;
	Result.BlockReason = ResolveBlockReason(Choice, Purchased, bCanPurchaseNow, CostGold, Gold);

	if (Gold != INDEX_NONE && CostGold != INDEX_NONE)
	{
		// 음수를 자르지 않는다. "얼마나 모자란지" 가 화면에 필요한 정보다.
		// 그래서 `모름` 은 값이 아니라 플래그로 나타낸다 — -1 이 정상값이기 때문이다.
		Result.GoldAfterPurchase = Gold - CostGold;
		Result.bHasGoldAfterPurchase = true;
	}

	return Result;
}

TOptional<int32> FFF_PrepPromotionRules::ResolveSummaryBalance(
	const FFF_PrepPromotionQuote& Quote, EFF_PromotionChoice Purchased, int32 Gold)
{
	if (Purchased != EFF_PromotionChoice::None)
	{
		return Gold != INDEX_NONE ? TOptional<int32>(Gold) : TOptional<int32>();
	}
	return Quote.bHasGoldAfterPurchase
		? TOptional<int32>(Quote.GoldAfterPurchase) : TOptional<int32>();
}

// =============================================================================
// UFF_PrepPanelPromotion
// =============================================================================

void UFF_PrepPanelPromotion::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 홍보가 반영된 예상 방문객은 군중 유입에도 전달된다. 예상치와 실제 입장 인원은 구분한다.
	SetTextOrHide(Text_NotAppliedNotice, NotAppliedNoticeText);

	// `Text_OnceNotice` 는 여기서 안 채운다 — 사유 문구와 같은 말을 할 때가 있어
	// `RefreshAll` 이 상태를 보고 켜고 끈다.

	if (Button_Purchase)
	{
		Button_Purchase->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelPromotion::HandlePurchaseClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Button_Purchase 가 없어 홍보를 살 방법이 없다."), *GetName());
	}
}

void UFF_PrepPanelPromotion::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	SelectedIndex = INDEX_NONE;

	BindViewModel();
	RebuildCards();
	RefreshAll();
}

void UFF_PrepPanelPromotion::NativeOnPanelClosed()
{
	// 패널이 내려가는데 확인창이 떠 있으면 그 창의 결과가 갈 곳이 없다. 먼저 끊는다.
	ReleaseConfirmDialog();

	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

// =============================================================================
// 구독
// =============================================================================

UFF_FestivalViewModelSubsystem* UFF_PrepPanelPromotion::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_PrepPanelPromotion::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 등급과 견적이 전부 비어 보인다."), *GetName());
		return;
	}

	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_PrepPanelPromotion::HandleGoldChanged);
	CachedViewModel->OnAdmissionFeeChanged.AddUniqueDynamic(this, &ThisClass::HandleGoldChanged);
	CachedViewModel->OnExpectedVisitorsChanged.AddUniqueDynamic(this, &ThisClass::HandleGoldChanged);
	CachedViewModel->OnBuzzChanged.AddUniqueDynamic(this, &ThisClass::HandleBuzzChanged);
	CachedViewModel->OnWeatherChanged.AddUniqueDynamic(this, &ThisClass::HandleWeatherChanged);
	CachedViewModel->OnPurchasedPromotionChanged.AddUniqueDynamic(
		this, &UFF_PrepPanelPromotion::HandlePurchasedPromotionChanged);

	// 셋업이 지나면 살 수 없다. 단계 전환을 받아야 카드가 제때 잠긴다.
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_PrepPanelPromotion::HandleStageViewChanged);
}

void UFF_PrepPanelPromotion::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_PrepPanelPromotion::HandleGoldChanged);
		CachedViewModel->OnAdmissionFeeChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
		CachedViewModel->OnExpectedVisitorsChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
		CachedViewModel->OnBuzzChanged.RemoveDynamic(this, &ThisClass::HandleBuzzChanged);
		CachedViewModel->OnWeatherChanged.RemoveDynamic(this, &ThisClass::HandleWeatherChanged);
		CachedViewModel->OnPurchasedPromotionChanged.RemoveDynamic(
			this, &UFF_PrepPanelPromotion::HandlePurchasedPromotionChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_PrepPanelPromotion::HandleStageViewChanged);
	}
	CachedViewModel = nullptr;
}

// =============================================================================
// 목록 만들기
// =============================================================================

void UFF_PrepPanelPromotion::RebuildCards()
{
	CardWidgets.Reset();
	if (Box_PromotionCards)
	{
		Box_PromotionCards->ClearChildren();
	}

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	Options = ViewModel ? ViewModel->GetPromotionOptions() : TArray<FFF_PromotionOptionView>();

	UClass* ResolvedCardClass = ResolveWidgetClass(CardWidgetClass, CardWidgetFallbackPath);
	if (!Box_PromotionCards || !ResolvedCardClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 카드 자리나 클래스가 없어 등급을 그릴 수 없다 (%s)."),
			*GetName(), *CardWidgetFallbackPath);
		return;
	}

	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		UFF_PrepPromotionCard* CardWidget = CreateWidget<UFF_PrepPromotionCard>(this, ResolvedCardClass);
		if (!CardWidget)
		{
			continue;
		}

		CardWidget->ConfigureOption(Options[Index], GetChoiceLabel(Options[Index].Choice), bUseCompactLayout);
		CardWidget->OnHudTabButtonClicked.BindUObject(this, &UFF_PrepPanelPromotion::HandleCardClicked);

		Box_PromotionCards->AddChild(CardWidget);
		CardWidgets.Add(CardWidget);
	}
}

// =============================================================================
// 갱신
// =============================================================================

const FFF_PromotionOptionView* UFF_PrepPanelPromotion::FindSelectedOption() const
{
	return Options.IsValidIndex(SelectedIndex) ? &Options[SelectedIndex] : nullptr;
}

void UFF_PrepPanelPromotion::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	// 입장료·날씨·화제성 결과 이벤트마다 현재 조건으로 다시 예측한다. 카드 선택은 유지한다.
	Options = ViewModel ? ViewModel->GetPromotionOptions() : TArray<FFF_PromotionOptionView>();

	const EFF_PromotionChoice Purchased = ViewModel
		? ViewModel->GetPurchasedPromotion() : EFF_PromotionChoice::None;
	const bool bCanPurchaseNow = ViewModel ? ViewModel->CanPurchasePromotionNow() : false;
	const int32 Gold = ViewModel ? ViewModel->GetGold() : INDEX_NONE;

	Quote = FFF_PrepPromotionRules::BuildQuote(FindSelectedOption(), Purchased, bCanPurchaseNow, Gold);

	const int32 AdmissionFee = ViewModel ? ViewModel->GetAdmissionFeeGold() : INDEX_NONE;
	SetTextOrHide(Text_ForecastBasis, bUseCompactLayout
		? AdmissionFee != INDEX_NONE
			? FText::Format(LOCTEXT("CompactForecastBasis", "예상 방문객 · 입장료 {0} 기준"), FText::AsNumber(AdmissionFee))
			: LOCTEXT("CompactForecastBasisPending", "예상 방문객 · 입장료 확인 중")
		: FText::GetEmpty());

	// --- 카드 상태 ---
	for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
	{
		UFF_PrepPromotionCard* CardWidget = CardWidgets[Index];
		if (!CardWidget || !Options.IsValidIndex(Index))
		{
			continue;
		}

		const EFF_PromotionChoice Choice = Options[Index].Choice;
		CardWidget->ConfigureOption(Options[Index], GetChoiceLabel(Choice), bUseCompactLayout);

		// 상태를 매번 전부 다시 정한다. 부분 갱신하면 구매 뒤에도 선택 표시가 남는다.
		if (Purchased != EFF_PromotionChoice::None)
		{
			// 산 등급만 배지를 달고, 나머지는 이유와 함께 잠근다.
			(Choice == Purchased)
				? CardWidget->ApplyCardState(EFF_PrepCardState::Committed,
					LOCTEXT("PrepPromotion_BadgePurchased", "✓ 구매함"))
				: CardWidget->ApplyCardState(EFF_PrepCardState::Locked,
					LOCTEXT("PrepPromotion_BadgeAlready", "이미 구매함"));
		}
		else if (!bCanPurchaseNow)
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Locked,
				LOCTEXT("PrepPromotion_BadgeNotStage", "셋업에서만"));
		}
		else if (Index == SelectedIndex)
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Selected, SelectedBadgeText);
		}
		else
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Selectable, FText::GetEmpty());
		}
	}

	// --- 견적 ---
	// `안 함` 의 0 Gold 는 확정값이라 `INDEX_NONE` 검사에 걸리지 않는다.
	FNumberFormattingOptions MultiplierFormat;
	MultiplierFormat.SetMinimumFractionalDigits(2);
	MultiplierFormat.SetMaximumFractionalDigits(2);

	SetTextOrHide(Text_CostPreview, (Quote.CostGold != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepPromotion_CostPreview", "{0} Gold · 예상 방문객 ×{1}"),
			FText::AsNumber(Quote.CostGold),
			FText::AsNumber(Quote.VisitorMultiplier, &MultiplierFormat))
		: FText::GetEmpty());

	SetTextOrHide(Text_GoldAfter, Quote.bHasGoldAfterPurchase
		? FText::Format(
			LOCTEXT("PrepPromotion_GoldAfter", "구매 후 잔액 {0}"),
			FText::AsNumber(Quote.GoldAfterPurchase))
		: FText::GetEmpty());

	// 새 요약 영역은 구매 이후에도 유지한다. 이미 낸 홍보비를 다시 차감하지 않는다.
	const bool bPurchased = Purchased != EFF_PromotionChoice::None;
	const FFF_PromotionOptionView* DisplayOption = bPurchased
		? Options.FindByPredicate([Purchased](const FFF_PromotionOptionView& One)
			{ return One.Choice == Purchased; })
		: FindSelectedOption();
	SetTextOrHide(Text_SelectedPromotion, bPurchased
		? FText::Format(PurchasedPromotionFormat, GetChoiceLabel(Purchased))
		: DisplayOption
			? FText::Format(SelectedPromotionFormat, GetChoiceLabel(DisplayOption->Choice))
			: NoPromotionSelectedText);
	SetTextOrHide(Text_PurchaseCost, DisplayOption && DisplayOption->CostGold != INDEX_NONE
		? FText::Format(PurchaseCostFormat, FText::AsNumber(DisplayOption->CostGold))
		: MissingValueText);
	const TOptional<int32> SummaryBalance =
		FFF_PrepPromotionRules::ResolveSummaryBalance(Quote, Purchased, Gold);
	SetTextOrHide(Text_GoldAfterValue, SummaryBalance.IsSet()
		? FText::Format(GoldAfterValueFormat, FText::AsNumber(SummaryBalance.GetValue()))
		: MissingValueText);
	SetTextOrHide(Text_GoldAfterLabel, bPurchased ? CurrentGoldLabel : GoldAfterLabel);

	if (bUseCompactLayout)
	{
		SetTextOrHide(Text_PurchaseAction, bPurchased
			? LOCTEXT("CompactPurchased", "구매 완료")
			: Quote.Choice != EFF_PromotionChoice::None && Quote.CostGold != INDEX_NONE
				? FText::Format(LOCTEXT("CompactPurchaseAction", "{0} Gold · 구매하기"), FText::AsNumber(Quote.CostGold))
				: LOCTEXT("CompactPurchaseDefault", "홍보 구매하기"));
	}

	if (Button_Purchase)
	{
		Button_Purchase->SetIsEnabled(Quote.CanPurchase());
	}

	const FText BlockReason = BuildBlockReasonText();

	// 🔴 같은 말을 두 번 쓰지 않는다. 상시 안내는 "한 번만 · 셋업에서만" 을 말하는데,
	// `이미 구매함`·`셋업 아님` 사유가 정확히 그 말이다. 둘이 나란히 있으면 읽는 사람이
	// 서로 다른 것으로 본다 (프로그램 패널의 시설 안내와 같은 처리).
	const bool bReasonRepeatsNotice =
		Quote.BlockReason == EFF_PrepPromotionBlockReason::AlreadyPurchased
		|| Quote.BlockReason == EFF_PrepPromotionBlockReason::NotSetupStage;

	SetTextOrHide(Text_OnceNotice, !bUseCompactLayout && bReasonRepeatsNotice
		? FText::GetEmpty()
		: OnceNoticeText);

	// 기존 화면은 액션 행의 Fill을 보존한다. 간소화 화면은 구매 버튼이 별도 행이다.
	if (Text_BlockReason)
	{
		Text_BlockReason->SetText(BlockReason);
		Text_BlockReason->SetVisibility(BlockReason.IsEmpty()
			? (bUseCompactLayout ? ESlateVisibility::Collapsed : ESlateVisibility::Hidden)
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

FText UFF_PrepPanelPromotion::BuildBlockReasonText() const
{
	if (bUseCompactLayout)
	{
		if (Quote.BlockReason == EFF_PrepPromotionBlockReason::AlreadyPurchased)
		{
			const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
			return ViewModel
				? FText::Format(PurchasedPromotionFormat, GetChoiceLabel(ViewModel->GetPurchasedPromotion()))
				: LOCTEXT("CompactPurchased", "구매 완료");
		}
		if (Quote.BlockReason == EFF_PrepPromotionBlockReason::NotSetupStage)
		{
			return LOCTEXT("CompactNotSetup", "셋업에서만 구매할 수 있습니다.");
		}
		if (Quote.BlockReason == EFF_PrepPromotionBlockReason::NoChoiceSelected)
		{
			return FindSelectedOption()
				? LOCTEXT("CompactNoPromotion", "홍보 없이 진행합니다. 별도 구매는 필요하지 않습니다.")
				: LOCTEXT("CompactSelectPromotion", "홍보 등급을 선택하세요.");
		}
	}

	switch (Quote.BlockReason)
	{
	case EFF_PrepPromotionBlockReason::Unknown:
		return LOCTEXT("PrepPromotion_Unknown", "축제가 아직 시작되지 않아 홍보 정보를 읽을 수 없습니다.");

	case EFF_PrepPromotionBlockReason::AlreadyPurchased:
		return LOCTEXT("PrepPromotion_Already", "홍보는 축제당 한 번만 살 수 있습니다.");

	case EFF_PrepPromotionBlockReason::NotSetupStage:
		return LOCTEXT("PrepPromotion_NotStage", "홍보는 셋업에서만 살 수 있습니다. 이번 축제에는 적용되지 않습니다.");

	case EFF_PrepPromotionBlockReason::NoChoiceSelected:
		return LOCTEXT("PrepPromotion_NoChoice", "홍보 등급을 선택해 주세요. 홍보하지 않으면 예상 방문객이 줄어듭니다.");

	case EFF_PrepPromotionBlockReason::NotEnoughGold:
		return (Quote.CostGold != INDEX_NONE)
			? FText::Format(
				LOCTEXT("PrepPromotion_NoGold", "{0} Gold 를 낼 잔액이 없습니다."),
				FText::AsNumber(Quote.CostGold))
			: LOCTEXT("PrepPromotion_NoGoldNoCost", "홍보비를 낼 잔액이 없습니다.");

	default:
		return FText::GetEmpty();
	}
}

FText UFF_PrepPanelPromotion::GetChoiceLabel(EFF_PromotionChoice Choice)
{
	// 🔴 enum 의 `DisplayName` 을 런타임에 읽지 않는다. 패키징 빌드에서 영문이 뜬다
	// (2026-08-19 김시리 정정). 경제 쪽 표시 라이브러리에 홍보 등급 항목이 아직 없어
	// 여기서 만든다 — 생기면 그쪽으로 옮긴다.
	switch (Choice)
	{
	case EFF_PromotionChoice::Basic:		return LOCTEXT("PrepPromotion_LabelBasic", "지역 SNS");
	case EFF_PromotionChoice::Focused:		return LOCTEXT("PrepPromotion_LabelFocused", "전국 홍보");
	case EFF_PromotionChoice::Influencer:	return LOCTEXT("PrepPromotion_LabelInfluencer", "인플루언서");
	default:								return LOCTEXT("PrepPromotion_LabelNone", "홍보 없음");
	}
}

// =============================================================================
// 입력
// =============================================================================

void UFF_PrepPanelPromotion::HandleCardClicked(int32 ChoiceId)
{
	const EFF_PromotionChoice Clicked = static_cast<EFF_PromotionChoice>(ChoiceId);

	const int32 NewIndex = Options.IndexOfByPredicate(
		[Clicked](const FFF_PromotionOptionView& One) { return One.Choice == Clicked; });

	if (NewIndex == INDEX_NONE)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 목록에 없는 홍보 등급 %d 를 눌렀다. 목록과 위젯이 갈라졌다."), *GetName(), ChoiceId);
		return;
	}

	// 같은 카드를 다시 누르면 선택을 푼다.
	SelectedIndex = (SelectedIndex == NewIndex) ? INDEX_NONE : NewIndex;
	RefreshAll();
}

void UFF_PrepPanelPromotion::HandlePurchaseClicked()
{
	if (!Quote.CanPurchase())
	{
		// 버튼이 꺼져 있어야 하는 상태다. 여기 왔다면 표시와 상태가 갈라진 것이므로 남긴다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 살 수 없는 상태에서 구매 요청이 들어왔다. 버튼 활성 조건을 확인한다."), *GetName());
		return;
	}

	// 🔴 확인창을 못 띄우면 **사지 않는다.** 되돌릴 수 없는 결정이라 확인 없이 진행하지 않는다.
	if (!OpenConfirmDialog())
	{
		SetTextOrHide(Text_BlockReason, LOCTEXT("PrepPromotion_NoDialog",
			"확인창을 띄우지 못해 구매를 진행하지 않았습니다."));
	}
}

FText UFF_PrepPanelPromotion::GetPurchaseConfirmationTitle(EFF_PromotionChoice Choice) const
{
	return FText::Format(ConfirmTitleFormat, GetChoiceLabel(Choice));
}

bool UFF_PrepPanelPromotion::OpenConfirmDialog()
{
	// 이미 떠 있으면 두 번 띄우지 않는다.
	if (PendingConfirm.IsValid())
	{
		return true;
	}

	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	UFF_UIRootLayout* RootLayout = Flow ? Flow->GetRootLayout() : nullptr;
	if (!RootLayout)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 루트 레이아웃이 없어 확인창을 띄울 수 없다."), *GetName());
		return false;
	}

	UClass* DialogClass = ResolveWidgetClass(nullptr, ConfirmDialogPath);
	if (!DialogClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 확인창 클래스를 얻지 못했다 (%s). `FF.FrontendUMG` 를 한 번 실행한다."),
			*GetName(), *ConfirmDialogPath);
		return false;
	}

	UFF_ConfirmDialog* Dialog = Cast<UFF_ConfirmDialog>(RootLayout->PushPanel(DialogClass));
	if (!Dialog)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 확인창을 스택에 올리지 못했다 (%s)."), *GetName(), *DialogClass->GetName());
		return false;
	}

	PendingConfirm = Dialog;
	Dialog->OnConfirmed.AddUniqueDynamic(this, &UFF_PrepPanelPromotion::HandlePurchaseConfirmed);
	Dialog->OnCancelled.AddUniqueDynamic(this, &UFF_PrepPanelPromotion::HandlePurchaseCancelled);

	// 인스턴스를 얻는 창구가 `PushPanel` 반환값뿐이라 내용은 Push 이후에 채운다.
	// 기본 포커스가 취소인 것은 확인창이 코드로 강제하므로 여기서 할 일이 없다.
	TArray<FFF_ConfirmDialogRow> Rows;

	FFF_ConfirmDialogRow& CostRow = Rows.AddDefaulted_GetRef();
	CostRow.Label = LOCTEXT("PrepPromotion_RowCost", "홍보비");
	CostRow.Value = FText::Format(PurchaseCostFormat,
		FText::AsNumber(Quote.CostGold));

	if (Quote.bHasGoldAfterPurchase)
	{
		FFF_ConfirmDialogRow& GoldRow = Rows.AddDefaulted_GetRef();
		GoldRow.Label = LOCTEXT("PrepPromotion_RowGoldAfter", "구매 후 잔액");
		GoldRow.Value = FText::Format(GoldAfterValueFormat, FText::AsNumber(Quote.GoldAfterPurchase));
	}

	Dialog->Configure(
		GetPurchaseConfirmationTitle(Quote.Choice),
		LOCTEXT("PrepPromotion_ConfirmBody",
			"홍보는 축제당 한 번뿐이고 되돌릴 수 없습니다. 3일 전체에 적용됩니다.\n"
			"홍보 효과는 예상 방문객에 반영됩니다. 실제 입장 인원은 운영 상황에 따라 달라집니다."),
		Rows,
		LOCTEXT("PrepPromotion_ConfirmOk", "구매하기"));

	return true;
}

void UFF_PrepPanelPromotion::ReleaseConfirmDialog()
{
	// 🔴 결과를 받은 시점에도 끊는다. 확인창은 풀에서 재사용되고 이 패널은 하단 바가
	// 캐시해서 안 죽는다. 여기서 안 끊으면 **나중에 다른 확인창의 `확인` 이 홍보를 산다.**
	if (UFF_ConfirmDialog* Dialog = PendingConfirm.Get())
	{
		Dialog->OnConfirmed.RemoveDynamic(this, &UFF_PrepPanelPromotion::HandlePurchaseConfirmed);
		Dialog->OnCancelled.RemoveDynamic(this, &UFF_PrepPanelPromotion::HandlePurchaseCancelled);
	}
	PendingConfirm.Reset();
}

void UFF_PrepPanelPromotion::HandlePurchaseConfirmed()
{
	ReleaseConfirmDialog();

	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const FFF_PromotionOptionView* Option = FindSelectedOption();
	if (!ViewModel || !Option)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 확인은 받았는데 살 등급이 없다."), *GetName());
		return;
	}

	// !! 낙관적 갱신을 하지 않는다 !! 화면은 결과 이벤트로만 갱신된다.
	if (!ViewModel->RequestPurchasePromotion(Option->Choice))
	{
		// 실패하면 이벤트가 오지 않으므로 여기서 한 번 다시 그린다.
		RefreshAll();
	}
}

void UFF_PrepPanelPromotion::HandlePurchaseCancelled()
{
	ReleaseConfirmDialog();
}

// =============================================================================
// 결과 이벤트
// =============================================================================

void UFF_PrepPanelPromotion::HandleGoldChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_PrepPanelPromotion::HandleBuzzChanged(float NewBuzz, float PreviousBuzz)
{
	RefreshAll();
}

void UFF_PrepPanelPromotion::HandleWeatherChanged(EFF_WeatherType NewWeather)
{
	RefreshAll();
}

void UFF_PrepPanelPromotion::HandlePurchasedPromotionChanged(EFF_PromotionChoice NewChoice)
{
	// 샀으면 선택을 비운다. 산 카드가 골라진 채로 남으면 견적이 계속 그 카드 기준으로 그려진다.
	SelectedIndex = INDEX_NONE;
	RefreshAll();
}

void UFF_PrepPanelPromotion::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	RefreshAll();
}

// =============================================================================
// 표시 헬퍼
// =============================================================================

void UFF_PrepPanelPromotion::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
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

#undef LOCTEXT_NAMESPACE
