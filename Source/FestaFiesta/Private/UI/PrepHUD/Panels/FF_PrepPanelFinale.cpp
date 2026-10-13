#include "UI/PrepHUD/Panels/FF_PrepPanelFinale.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Engine/GameInstance.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "UI/FF_UILog.h"
#include "UI/Frontend/FF_FinaleConfirmDialog.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

UFF_PrepPanelFinale::UFF_PrepPanelFinale()
{
	TierPresentations.FindOrAdd(EFF_FinaleGrade::C).Description =
		NSLOCTEXT("FestaFiestaFinale", "DescriptionC", "단발 불꽃");
	TierPresentations.FindOrAdd(EFF_FinaleGrade::B).Description =
		NSLOCTEXT("FestaFiestaFinale", "DescriptionB", "묶음 불꽃");
	TierPresentations.FindOrAdd(EFF_FinaleGrade::A).Description =
		NSLOCTEXT("FestaFiestaFinale", "DescriptionA", "일렬 · 부채형");
	TierPresentations.FindOrAdd(EFF_FinaleGrade::S).Description =
		NSLOCTEXT("FestaFiestaFinale", "DescriptionS", "대형 혼합 연출");
}

void UFF_PrepPanelFinale::ConfigurePresentationDefaults(TSubclassOf<UFF_PrepFinaleCard> InCardClass,
	TSubclassOf<UFF_FinaleConfirmDialog> InConfirmClass,
	const TMap<EFF_FinaleGrade, FFF_FinaleTierPresentation>& InPresentations)
{
	CardWidgetClass = InCardClass;
	ConfirmDialogClass = InConfirmClass;
	TierPresentations = InPresentations;
}

void UFF_PrepPanelFinale::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Purchase)
		Button_Purchase->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelFinale::HandlePurchaseClicked);
	if (Button_Later)
		Button_Later->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelFinale::HandleLaterClicked);
}

void UFF_PrepPanelFinale::NativeDestruct()
{
	ReleaseConfirmDialog(true);
	UnbindViewModel();
	Super::NativeDestruct();
}

void UFF_PrepPanelFinale::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();
	SelectedGrade = EFF_FinaleGrade::None;
	LastFailureText = FText::GetEmpty();
	BindViewModel();
	RebuildCards();
	RefreshAll();
}

void UFF_PrepPanelFinale::NativeOnPanelClosed()
{
	ReleaseConfirmDialog(true);
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

void UFF_PrepPanelFinale::BindViewModel()
{
	UnbindViewModel();
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	if (!CachedViewModel) return;
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_PrepPanelFinale::HandleGoldChanged);
	CachedViewModel->OnPurchasedFinaleGradeChanged.AddUniqueDynamic(
		this, &UFF_PrepPanelFinale::HandlePurchasedGradeChanged);
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_PrepPanelFinale::HandleStageViewChanged);
}

void UFF_PrepPanelFinale::UnbindViewModel()
{
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_PrepPanelFinale::HandleGoldChanged);
		CachedViewModel->OnPurchasedFinaleGradeChanged.RemoveDynamic(
			this, &UFF_PrepPanelFinale::HandlePurchasedGradeChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_PrepPanelFinale::HandleStageViewChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_PrepPanelFinale::RebuildCards()
{
	CardWidgets.Reset();
	if (Box_FinaleCards) Box_FinaleCards->ClearChildren();
	Tiers = CachedViewModel ? CachedViewModel->GetFinaleTiers() : TArray<FFF_FinaleTier>();
	UClass* CardClass = ResolveWidgetClass(CardWidgetClass, CardWidgetFallbackPath);
	if (!Box_FinaleCards || !CardClass || !CardClass->IsChildOf(UFF_PrepFinaleCard::StaticClass()))
	{
		UE_LOG(LogFestaFiestaUI, Error, TEXT("[%s] 피날레 카드 컨테이너 또는 클래스 배선이 없습니다."), *GetName());
		return;
	}
	for (const FFF_FinaleTier& Tier : Tiers)
	{
		if (Tier.Grade == EFF_FinaleGrade::None) continue;
		UFF_PrepFinaleCard* Card = CreateWidget<UFF_PrepFinaleCard>(this, CardClass);
		if (!Card) continue;
		Card->ConfigureTier(Tier, GetPresentation(Tier.Grade));
		Card->OnHudTabButtonClicked.BindUObject(this, &UFF_PrepPanelFinale::HandleCardClicked);
		Box_FinaleCards->AddChild(Card);
		CardWidgets.Add(Card);
	}
}

const FFF_FinaleTier* UFF_PrepPanelFinale::FindTier(EFF_FinaleGrade Grade) const
{
	return Tiers.FindByPredicate([Grade](const FFF_FinaleTier& Tier) { return Tier.Grade == Grade; });
}

FFF_FinaleTierPresentation UFF_PrepPanelFinale::GetPresentation(EFF_FinaleGrade Grade) const
{
	const FFF_FinaleTierPresentation* Presentation = TierPresentations.Find(Grade);
	return Presentation ? *Presentation : FFF_FinaleTierPresentation();
}

bool UFF_PrepPanelFinale::IsPurchaseReady(EFF_FinaleGrade Grade) const
{
	const FFF_FinaleTier* Tier = FindTier(Grade);
	return !bSubmitting && CachedViewModel && Tier
		&& CachedViewModel->CanPurchaseFinaleNow() && CachedViewModel->CanAfford(Tier->CostGold);
}

FText UFF_PrepPanelFinale::GetBlockReason(EFF_FinaleGrade Grade) const
{
	if (!CachedViewModel || Tiers.IsEmpty()) return UnknownDataText;
	if (CachedViewModel->GetPurchasedFinaleGrade() != EFF_FinaleGrade::None) return PurchasedNoticeText;
	if (!CachedViewModel->CanPurchaseFinaleNow()) return UnavailableStageText;
	const FFF_FinaleTier* Tier = FindTier(Grade);
	if (!Tier) return SelectTierText;
	if (!CachedViewModel->CanAfford(Tier->CostGold)) return InsufficientBalanceText;
	return FText::GetEmpty();
}

void UFF_PrepPanelFinale::RefreshAll()
{
	const EFF_FinaleGrade Purchased = CachedViewModel
		? CachedViewModel->GetPurchasedFinaleGrade() : EFF_FinaleGrade::None;
	const bool bPurchased = Purchased != EFF_FinaleGrade::None;
	const bool bCanPurchaseNow = CachedViewModel && CachedViewModel->CanPurchaseFinaleNow();
	for (UFF_PrepFinaleCard* Card : CardWidgets)
	{
		if (!Card) continue;
		if (bPurchased)
		{
			Card->ApplyFinaleState(Card->GetGrade() == Purchased ? EFF_PrepCardState::Committed : EFF_PrepCardState::Locked,
				Card->GetGrade() == Purchased ? PurchasedBadgeText : FText::GetEmpty());
		}
		else if (!bCanPurchaseNow)
			Card->ApplyFinaleState(EFF_PrepCardState::Locked, StageBadgeText);
		else
			Card->ApplyFinaleState(Card->GetGrade() == SelectedGrade ? EFF_PrepCardState::Selected : EFF_PrepCardState::Selectable,
				Card->GetGrade() == SelectedGrade ? SelectedBadgeText : FText::GetEmpty());
	}

	const EFF_FinaleGrade SummaryGrade = bPurchased ? Purchased : SelectedGrade;
	const FFF_FinaleTier* Tier = FindTier(SummaryGrade);
	SetText(Text_SelectedFinale, SummaryGrade != EFF_FinaleGrade::None
		? FText::Format(bPurchased ? PurchasedTierFormat : SelectedTierFormat,
			UFF_EconomyDisplayTextLibrary::GetFinaleGradeText(SummaryGrade)) : SelectTierText);
	SetText(Text_PurchaseCost, Tier ? FText::AsNumber(Tier->CostGold) : MissingValueText);
	// 성공 뒤에는 실제 현재 잔액을 읽는다. 이미 차감된 돈에서 비용을 또 빼지 않는다.
	// 모자라면 음수 잔액 대신 부족한 금액을 보인다(시설 강화·프로그램 화면과 같은 표기).
	const bool bHasBalance = CachedViewModel && (bPurchased || Tier);
	const int32 GoldAfter = bHasBalance ? CachedViewModel->GetGold() - (bPurchased ? 0 : Tier->CostGold) : 0;
	const bool bShort = bHasBalance && !bPurchased && GoldAfter < 0;
	SetText(Text_GoldAfterValue, bHasBalance ? FText::AsNumber(FMath::Abs(GoldAfter)) : MissingValueText);
	SetText(Text_GoldAfterLabel, bPurchased ? CurrentBalanceLabel : (bShort ? ShortageLabel : AfterBalanceLabel));
	SetText(Text_StageNotice, CachedViewModel && CachedViewModel->GetCurrentStage() == EFF_FestivalStage::MorningCheck
		? FText::Format(StageNoticeFormat, FText::AsNumber(CachedViewModel->GetCurrentDay())) : UnavailableStageText);
	SetText(Text_OnceNotice, OnceNoticeText);
	SetText(Text_PurchaseLabel, bPurchased ? PurchasedLabel : PurchaseLabel);
	SetText(Text_BlockReason, !LastFailureText.IsEmpty() && !bPurchased ? LastFailureText : GetBlockReason(SelectedGrade));
	if (Button_Purchase) Button_Purchase->SetIsEnabled(IsPurchaseReady(SelectedGrade) && !PendingConfirm.IsValid());
	RefreshConfirmDialog();
}

void UFF_PrepPanelFinale::RefreshConfirmDialog()
{
	UFF_FinaleConfirmDialog* Dialog = PendingConfirm.Get();
	if (!Dialog) return;
	const FFF_FinaleTier* Tier = FindTier(ConfirmGrade);
	if (!CachedViewModel || !Tier)
	{
		Dialog->SetPurchaseAvailability(false, UnknownDataText);
		return;
	}
	const bool bPurchased = CachedViewModel->GetPurchasedFinaleGrade() != EFF_FinaleGrade::None;
	Dialog->ConfigureFinale(*Tier, GetPresentation(ConfirmGrade),
		CachedViewModel->GetGold() - (bPurchased ? 0 : Tier->CostGold),
		IsPurchaseReady(ConfirmGrade), GetBlockReason(ConfirmGrade));
}

void UFF_PrepPanelFinale::HandleCardClicked(int32 GradeId)
{
	if (PendingConfirm.IsValid() || bSubmitting || !CachedViewModel || !CachedViewModel->CanPurchaseFinaleNow()) return;
	const EFF_FinaleGrade Grade = static_cast<EFF_FinaleGrade>(GradeId);
	if (!FindTier(Grade)) return;
	SelectedGrade = Grade;
	LastFailureText = FText::GetEmpty();
	RefreshAll();
}

void UFF_PrepPanelFinale::HandlePurchaseClicked()
{
	if (!IsPurchaseReady(SelectedGrade) || PendingConfirm.IsValid()) return;
	LastFailureText = FText::GetEmpty();
	if (!OpenConfirmDialog()) LastFailureText = DialogUnavailableText;
	RefreshAll();
}

bool UFF_PrepPanelFinale::OpenConfirmDialog()
{
	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	UFF_UIRootLayout* Root = Flow ? Flow->GetRootLayout() : nullptr;
	UClass* DialogClass = ResolveWidgetClass(ConfirmDialogClass, ConfirmDialogFallbackPath);
	if (!Root || !DialogClass || !DialogClass->IsChildOf(UFF_FinaleConfirmDialog::StaticClass())) return false;
	UFF_FinaleConfirmDialog* Dialog = Cast<UFF_FinaleConfirmDialog>(Root->PushPanel(DialogClass));
	if (!Dialog) return false;
	PendingConfirm = Dialog;
	ConfirmGrade = SelectedGrade;
	Dialog->OnConfirmed.AddUniqueDynamic(this, &UFF_PrepPanelFinale::HandlePurchaseConfirmed);
	Dialog->OnCancelled.AddUniqueDynamic(this, &UFF_PrepPanelFinale::HandlePurchaseCancelled);
	RefreshConfirmDialog();
	return true;
}

void UFF_PrepPanelFinale::ReleaseConfirmDialog(bool bClose)
{
	UFF_FinaleConfirmDialog* Dialog = PendingConfirm.Get();
	PendingConfirm.Reset();
	ConfirmGrade = EFF_FinaleGrade::None;
	if (!Dialog) return;
	// 풀 재사용 시 예전 패널로 결과가 전달되지 않도록 먼저 구독을 끊는다.
	Dialog->OnConfirmed.RemoveDynamic(this, &UFF_PrepPanelFinale::HandlePurchaseConfirmed);
	Dialog->OnCancelled.RemoveDynamic(this, &UFF_PrepPanelFinale::HandlePurchaseCancelled);
	if (bClose) Dialog->DeactivateWidget();
}

void UFF_PrepPanelFinale::HandlePurchaseConfirmed()
{
	const EFF_FinaleGrade RequestedGrade = ConfirmGrade;
	ReleaseConfirmDialog(false);
	if (bSubmitting || !CachedViewModel || RequestedGrade == EFF_FinaleGrade::None) return;
	// 구매 명령의 단계 검사 유무와 별개로, 창을 여는 동안 바뀐 구매 가능 상태를 다시 읽는다.
	if (!CachedViewModel->CanPurchaseFinaleNow())
	{
		LastFailureText = GetBlockReason(RequestedGrade);
		RefreshAll();
		return;
	}
	{
		TGuardValue<bool> SubmittingGuard(bSubmitting, true);
		// 중복 구매·잔액의 최종 판단은 경제 명령에 맡긴다.
		const EFF_FinalePurchaseResult Result = CachedViewModel->RequestPurchaseFinale(RequestedGrade);
		LastFailureText = Result == EFF_FinalePurchaseResult::Success ? FText::GetEmpty()
			: UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText(Result);
	}
	// 결과 이벤트 이후 원본을 다시 읽는다. 성공했다고 등급이나 돈을 직접 바꾸지 않는다.
	RefreshAll();
}

void UFF_PrepPanelFinale::HandlePurchaseCancelled()
{
	ReleaseConfirmDialog(false);
	RefreshAll();
}

void UFF_PrepPanelFinale::HandleLaterClicked()
{
	RequestClose();
}

void UFF_PrepPanelFinale::HandleGoldChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_PrepPanelFinale::HandlePurchasedGradeChanged(EFF_FinaleGrade NewGrade)
{
	RefreshAll();
}

void UFF_PrepPanelFinale::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	// 구매 가능 단계를 벗어나면 확인창까지 닫고 남은 콜백을 버린다.
	if (!CachedViewModel || !CachedViewModel->CanPurchaseFinaleNow()) ReleaseConfirmDialog(true);
	RefreshAll();
}

void UFF_PrepPanelFinale::SetText(UTextBlock* Block, const FText& Value)
{
	if (Block) Block->SetText(Value);
}
