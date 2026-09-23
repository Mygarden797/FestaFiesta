// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"

#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "UI/FF_UILog.h"
#include "UI/PrepHUD/Panels/FF_ProgramFacilityGuide.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepProgram"

// =============================================================================
// FFF_PrepProgramRules — 순수 함수
// =============================================================================

EFF_PrepProgramBlockReason FFF_PrepProgramRules::ResolveBlockReason(
	EFF_ProgramPurchaseResult OwnerResult,
	bool bCanScheduleNow,
	bool bHasCardSelected,
	int32 EligibleFacilityCount)
{
	// ① 축제가 시작되기 전이다. 이때 `단계 아님` 을 띄우면 기다리면 열릴 것처럼 읽힌다.
	if (OwnerResult == EFF_ProgramPurchaseResult::FestivalNotStarted)
	{
		return EFF_PrepProgramBlockReason::Unknown;
	}

	// ② 단계는 카드와 무관하게 전체를 막는다. 카드별 사유보다 먼저다.
	if (!bCanScheduleNow)
	{
		return EFF_PrepProgramBlockReason::NotSchedulingStage;
	}

	// ③ 아직 고르지 않았다. 막힌 것이 아니라 다음 행동을 안내하는 상태다.
	if (!bHasCardSelected)
	{
		return EFF_PrepProgramBlockReason::NoCardSelected;
	}

	// ④ 이미 편성한 카드에 "부스를 지으세요" 는 틀린 안내다. 시설보다 먼저 본다.
	switch (OwnerResult)
	{
	case EFF_ProgramPurchaseResult::AlreadyScheduled:
		return EFF_PrepProgramBlockReason::AlreadyScheduled;

	case EFF_ProgramPurchaseResult::UnknownProgram:
		return EFF_PrepProgramBlockReason::UnknownProgram;

	default:
		break;
	}

	// ⑤ 열 장소가 없으면 돈을 모아도 이 카드를 못 연다.
	//    `INDEX_NONE` 은 아직 못 읽은 것이라 "시설이 없다"로 단정하지 않는다.
	if (EligibleFacilityCount == INDEX_NONE)
	{
		return EFF_PrepProgramBlockReason::Unknown;
	}
	if (EligibleFacilityCount <= 0)
	{
		return EFF_PrepProgramBlockReason::NoEligibleFacility;
	}

	// ⑥ 남은 것은 소유자가 낸 잔액 판정이다. 인원은 보지 않는다 —
	//    프로그램은 Staff 를 쓰지 않는다(Staff 는 사건 대응 축).
	switch (OwnerResult)
	{
	case EFF_ProgramPurchaseResult::InsufficientGold:
		return EFF_PrepProgramBlockReason::NotEnoughGold;

	case EFF_ProgramPurchaseResult::FacilityTypeMismatch:
		// 목록에서 고른 시설은 이미 종류가 맞다. 여기 오면 목록과 판정이 갈라진 것이다.
		return EFF_PrepProgramBlockReason::NoEligibleFacility;

	case EFF_ProgramPurchaseResult::Success:
		return EFF_PrepProgramBlockReason::None;

	default:
		// 새 결과가 추가됐는데 여기 매핑이 안 된 경우다. `가능` 으로 흘리지 않는다 —
		// 눌러도 실패하는 버튼이 켜져 있는 쪽이 더 나쁘다.
		return EFF_PrepProgramBlockReason::Unknown;
	}
}

FFF_PrepProgramQuote FFF_PrepProgramRules::BuildQuote(
	const FFF_ProgramCard* Card,
	EFF_ProgramPurchaseResult OwnerResult,
	bool bCanScheduleNow,
	int32 EligibleFacilityCount,
	int32 CurrentGold)
{
	FFF_PrepProgramQuote Result;
	Result.EligibleFacilityCount = EligibleFacilityCount;
	Result.BlockReason = ResolveBlockReason(
		OwnerResult, bCanScheduleNow, /*bHasCardSelected*/ Card != nullptr, EligibleFacilityCount);

	if (!Card)
	{
		// 카드가 없으면 채울 금액이 없다. 0 을 넣으면 "공짜 프로그램"이 된다.
		return Result;
	}

	Result.ProgramId = Card->ProgramId;
	Result.CostGold = Card->CostGold;
	Result.MaxRevenue = Card->CalcMaxRevenue();

	if (CurrentGold != INDEX_NONE)
	{
		// 음수가 될 수 있다. 그대로 둔다 — "얼마나 모자란지" 가 화면에 필요한 정보다.
		Result.GoldAfterSchedule = CurrentGold - Card->CostGold;
		Result.bHasGoldAfterSchedule = true;
	}

	return Result;
}

// =============================================================================
// UFF_PrepPanelProgram
// =============================================================================

void UFF_PrepPanelProgram::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureCatalogChoices();

	if (DailyNoticeText.IsEmpty())
	{
		DailyNoticeText = LOCTEXT("PrepProgram_DailyNotice",
			"프로그램은 하루짜리입니다. 다음 날에는 다시 편성해야 합니다.");
	}
	SetTextOrHide(Text_DailyNotice, DailyNoticeText);

	// 내부 버튼이라 인스턴스 수명과 같다. 외부 시스템 구독만 패널을 열 때 붙인다.
	if (Button_Schedule)
	{
		Button_Schedule->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelProgram::HandleScheduleClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Button_Schedule 이 없어 편성할 방법이 없다."), *GetName());
	}

	if (!ResolveWidgetClass(CardWidgetClass, CardWidgetFallbackPath))
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 카드 위젯 클래스를 얻지 못해 카드를 하나도 그릴 수 없다. "
				 "CardWidgetClass 를 배선하거나 `FF.PrepHudUMG` 를 한 번 실행한다 (%s)."),
			*GetName(), *CardWidgetFallbackPath);
	}
}

void UFF_PrepPanelProgram::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// 열 때마다 선택을 비우고 시작한다. 이전 선택을 남기면 그 사이에 편성됐거나 시설이
	// 철거됐을 수 있어 "왜 이게 골라져 있는지" 설명되지 않는다.
	SelectedCardIndex = INDEX_NONE;
	SelectedFacilityIndex = INDEX_NONE;

	BindViewModel();
	RebuildCards();
	RebuildFacilityChoices();
	RefreshAll();
}

void UFF_PrepPanelProgram::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

void UFF_PrepPanelProgram::NativeDestruct()
{
	UnbindViewModel();
	Super::NativeDestruct();
}

// =============================================================================
// 구독
// =============================================================================

UFF_FestivalViewModelSubsystem* UFF_PrepPanelProgram::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_PrepPanelProgram::BindViewModel()
{
	UnbindViewModel();
	UGameInstance* GI = GetGameInstance();
	if (UFF_BuildCatalogSubsystem* BuildCatalog = GI ? GI->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr)
	{
		BuildCatalog->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::HandleBuildCatalogChanged);
	}
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	BoundProgression = GI ? GI->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (BoundProgression)
	{
		BoundProgression->OnFestivalDefinitionChanged.AddUniqueDynamic(this, &ThisClass::HandleFestivalDefinitionChanged);
	}
	BindRevenueTable();

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 카드와 견적이 전부 비어 보인다."), *GetName());
		return;
	}

	// 잔액이 바뀌면 살 수 있는 카드가 바뀌고, 편성이 바뀌면 카드 잠금이 바뀐다.
	// 둘 다 받아야 화면이 따라간다.
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_PrepPanelProgram::HandleGoldChanged);
	CachedViewModel->OnScheduledProgramsChanged.AddUniqueDynamic(
		this, &UFF_PrepPanelProgram::HandleScheduledProgramsChanged);

	// 단계가 바뀌면 편성 자체가 막히거나 풀린다 (셋업·개장 준비에서만 된다).
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_PrepPanelProgram::HandleStageViewChanged);
}

void UFF_PrepPanelProgram::UnbindViewModel()
{
	if (BoundRevenueTable)
	{
		BoundRevenueTable->OnDataTableChanged().RemoveAll(this);
		BoundRevenueTable = nullptr;
	}
	if (BoundProgression)
	{
		BoundProgression->OnFestivalDefinitionChanged.RemoveAll(this);
		BoundProgression = nullptr;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UFF_BuildCatalogSubsystem* BuildCatalog = GI->GetSubsystem<UFF_BuildCatalogSubsystem>())
		{
			BuildCatalog->OnCatalogChanged.RemoveDynamic(this, &ThisClass::HandleBuildCatalogChanged);
		}
	}
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_PrepPanelProgram::HandleGoldChanged);
		CachedViewModel->OnScheduledProgramsChanged.RemoveDynamic(
			this, &UFF_PrepPanelProgram::HandleScheduledProgramsChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_PrepPanelProgram::HandleStageViewChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_PrepPanelProgram::BindRevenueTable()
{
	const UFF_FestivalDefinition* Definition = BoundProgression ? BoundProgression->GetDefinition() : nullptr;
	UDataTable* RevenueTable = Definition ? Definition->GetFacilityRevenueTable() : nullptr;
	if (BoundRevenueTable == RevenueTable) return;
	if (BoundRevenueTable)
	{
		BoundRevenueTable->OnDataTableChanged().RemoveAll(this);
	}
	BoundRevenueTable = RevenueTable;
	if (BoundRevenueTable)
	{
		BoundRevenueTable->OnDataTableChanged().AddUObject(this, &ThisClass::HandleRevenueTableChanged);
	}
}

void UFF_PrepPanelProgram::HandleRevenueTableChanged()
{
	// 금액만 다시 조회하여 프로그램과 시설 선택은 그대로 유지한다.
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleFestivalDefinitionChanged()
{
	BindRevenueTable();
	SelectedCardIndex = INDEX_NONE;
	SelectedFacilityIndex = INDEX_NONE;
	RebuildCards();
	RebuildFacilityChoices();
	RefreshAll();
}

// =============================================================================
// 목록 만들기
// =============================================================================

void UFF_PrepPanelProgram::RebuildCards()
{
	// 카드 위젯은 매번 새로 만든다. 카탈로그가 축제마다 다르고 개수도 다르므로
	// 재사용하려면 개수 맞추기 코드가 따로 필요한데, 카드가 몇 장뿐이라 값어치가 없다.
	CardWidgets.Reset();
	if (Box_ProgramCards)
	{
		Box_ProgramCards->ClearChildren();
	}

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	Catalog = ViewModel ? ViewModel->GetProgramCatalog() : TArray<FFF_ProgramCard>();

	UClass* ResolvedCardClass = ResolveWidgetClass(CardWidgetClass, CardWidgetFallbackPath);
	if (!Box_ProgramCards || !ResolvedCardClass)
	{
		// 로그는 NativeOnInitialized 에서 이미 남겼다. 여기서 매번 반복하지 않는다.
		return;
	}

	for (int32 Index = 0; Index < Catalog.Num(); ++Index)
	{
		UFF_PrepProgramCard* CardWidget = CreateWidget<UFF_PrepProgramCard>(this, ResolvedCardClass);
		if (!CardWidget)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] 카드 위젯 생성에 실패했다 (%s)."), *GetName(), *ResolvedCardClass->GetName());
			continue;
		}

		const FFF_ProgramCard& Card = Catalog[Index];

		// 시각 변환은 Stage 시스템이 소유한다. UI 가 개장 시각에 분을 더하지 않는다.
		FText TimeRange;
		if (ViewModel)
		{
			const FText StartText = ViewModel->GetLiveClockTextAtMinutes(FMath::RoundToInt(Card.StartMinutes));
			const FText EndText = ViewModel->GetLiveClockTextAtMinutes(FMath::RoundToInt(Card.EndMinutes));
			if (!StartText.IsEmpty() && !EndText.IsEmpty())
			{
				TimeRange = FText::Format(LOCTEXT("PrepProgram_TimeRange", "{0} ~ {1}"), StartText, EndText);
			}
		}

		CardWidget->ConfigureCard(Index, Card, TimeRange);
		CardWidget->SetRevenuePreview(nullptr);
		CardWidget->OnHudTabButtonClicked.BindUObject(this, &UFF_PrepPanelProgram::HandleCardClicked);

		Box_ProgramCards->AddChild(CardWidget);
		CardWidgets.Add(CardWidget);
	}

	if (Catalog.Num() == 0)
	{
		// 빈 카탈로그는 "살 게 없다"가 아니라 대개 축제 전이다. 화면은 사유 문구로 설명한다.
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[%s] 프로그램 카탈로그가 비어 있다. 축제가 시작되지 않았을 수 있다."), *GetName());
	}
}

void UFF_PrepPanelProgram::RebuildFacilityChoices()
{
	RebuildCatalogChoices();
	FacilityWidgets.Reset();
	if (Box_FacilityChoices)
	{
		Box_FacilityChoices->ClearChildren();
	}

	FacilityOptions.Reset();
	SelectedFacilityIndex = INDEX_NONE;

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const FFF_ProgramCard* Card = FindSelectedCard();

	// 카드를 고르기 전에는 시설 줄째 숨긴다. 빈 줄만 남으면 만들다 만 화면으로 보인다.
	if (!ViewModel || !Card)
	{
		if (Box_FacilityRow)
		{
			Box_FacilityRow->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (Box_FacilityRow)
	{
		Box_FacilityRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	FacilityOptions = ViewModel->GetEligibleFacilitiesForProgram(Card->ProgramId);

	UClass* ResolvedChoiceClass = ResolveWidgetClass(FacilityChoiceClass, FacilityChoiceFallbackPath);
	if (Box_FacilityChoices && ResolvedChoiceClass)
	{
		for (int32 Index = 0; Index < FacilityOptions.Num(); ++Index)
		{
			UFF_PrepChoiceButton* Choice = CreateWidget<UFF_PrepChoiceButton>(this, ResolvedChoiceClass);
			if (!Choice)
			{
				continue;
			}

			Choice->ConfigureChoice(Index, FacilityOptions[Index].DisplayName);
			Choice->OnHudTabButtonClicked.BindUObject(this, &UFF_PrepPanelProgram::HandleFacilityClicked);

			Box_FacilityChoices->AddChild(Choice);
			FacilityWidgets.Add(Choice);
		}
	}
	else if (FacilityOptions.Num() > 0)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 시설 선택지를 그릴 자리나 클래스가 없다. 열 수 있는 시설 %d곳을 못 보여준다."),
			*GetName(), FacilityOptions.Num());
	}

	// 한 곳뿐이면 골라 준다. 선택지가 하나인데 누르게 하는 것은 일만 늘린다.
	// **여러 곳이면 고르게 둔다** — 어디서 여는지가 대기열과 동선에 영향을 준다.
	if (FacilityOptions.Num() == 1)
	{
		SelectedFacilityIndex = 0;
	}
}

void UFF_PrepPanelProgram::EnsureCatalogChoices()
{
	if (Box_CatalogChoices || !WidgetTree || !Text_FacilityNotice) { return; }
	UPanelWidget* Parent = Text_FacilityNotice->GetParent();
	if (!Parent) { return; }

	// Extend existing designer-authored panels before Slate construction, without replacing their tree.
	UWrapBox* Choices = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("Box_CatalogChoices"));
	Choices->SetInnerSlotPadding(FVector2D(8.f, 8.f));
	Choices->SetVisibility(ESlateVisibility::Collapsed);
	UPanelSlot* ChoiceSlot = Parent->InsertChildAt(Parent->GetChildIndex(Text_FacilityNotice) + 1, Choices);
	if (!ChoiceSlot) { return; }
	if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(ChoiceSlot))
	{
		ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
		ScrollSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
	}
	Box_CatalogChoices = Choices;
}

void UFF_PrepPanelProgram::RebuildCatalogChoices()
{
	CatalogChoiceWidgets.Reset();
	CompatibleCatalogEntries.Reset();
	if (Box_CatalogChoices) { Box_CatalogChoices->ClearChildren(); }
	const UGameInstance* GI = GetGameInstance();
	const UFF_BuildCatalogSubsystem* BuildCatalog = GI ? GI->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	const FFF_ProgramCard* Card = FindSelectedCard();
	if (Card && BuildCatalog && BuildCatalog->IsCatalogReady())
	{
		CompatibleCatalogEntries = FFF_ProgramFacilityGuide::FindCompatibleEntries(*Card, BuildCatalog->GetEntriesView());
	}
	if (!Box_CatalogChoices) { return; }
	Box_CatalogChoices->SetVisibility(CompatibleCatalogEntries.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	UClass* ChoiceClass = ResolveWidgetClass(FacilityChoiceClass, FacilityChoiceFallbackPath);
	if (!ChoiceClass) { return; }
	for (int32 Index = 0; Index < CompatibleCatalogEntries.Num(); ++Index)
	{
		UFF_PrepChoiceButton* Choice = CreateWidget<UFF_PrepChoiceButton>(this, ChoiceClass);
		if (!Choice) { continue; }
		const FFF_BuildCatalogEntry& Entry = CompatibleCatalogEntries[Index];
		Choice->ConfigureChoice(Index, FText::Format(
			LOCTEXT("PrepProgram_FindInCatalog", "{0} · 건축목록에서 보기"), Entry.DisplayName));
		Choice->SetToolTipText(FFF_ProgramFacilityGuide::GetCatalogPath(Entry));
		Choice->OnHudTabButtonClicked.BindUObject(this, &ThisClass::HandleCatalogFacilityClicked);
		Box_CatalogChoices->AddChild(Choice);
		CatalogChoiceWidgets.Add(Choice);
	}
}

// =============================================================================
// 갱신
// =============================================================================

const FFF_ProgramCard* UFF_PrepPanelProgram::FindSelectedCard() const
{
	return Catalog.IsValidIndex(SelectedCardIndex) ? &Catalog[SelectedCardIndex] : nullptr;
}

void UFF_PrepPanelProgram::RefreshAll()
{
	BindRevenueTable();
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const FFF_ProgramCard* SelectedCard = FindSelectedCard();
	const bool bHasFacility = FacilityOptions.IsValidIndex(SelectedFacilityIndex);
	const TArray<FFF_ProgramCard> FacilityCatalog = ViewModel && bHasFacility
		? ViewModel->GetProgramCatalogForFacility(FacilityOptions[SelectedFacilityIndex].FacilityInstanceId)
		: TArray<FFF_ProgramCard>();
	const FFF_ProgramCard* FacilityCard = SelectedCard
		? FFF_ProgramScheduleRules::FindCard(FacilityCatalog, SelectedCard->ProgramId) : nullptr;

	const bool bCanScheduleNow = ViewModel ? ViewModel->CanScheduleProgramNow() : false;
	const int32 Gold = ViewModel ? ViewModel->GetGold() : INDEX_NONE;

	// 시설 수는 카드를 골랐을 때만 의미가 있다. 안 골랐으면 `모름` 이다 — 0 을 넣으면
	// 카드도 안 골랐는데 `부스를 먼저 지으세요` 가 뜬다.
	const int32 FacilityCount = SelectedCard ? FacilityOptions.Num() : INDEX_NONE;

	// "하루 한 번" 은 시설 단위다. 시설을 골랐으면 그 시설 기준으로 묻는다.
	const EFF_ProgramPurchaseResult OwnerResult = (ViewModel && SelectedCard)
		? (bHasFacility
			? ViewModel->CanScheduleProgramAtFacility(
				SelectedCard->ProgramId, FacilityOptions[SelectedFacilityIndex].FacilityInstanceId)
			: ViewModel->CanScheduleProgram(SelectedCard->ProgramId))
		: (ViewModel ? EFF_ProgramPurchaseResult::Success : EFF_ProgramPurchaseResult::FestivalNotStarted);

	Quote = FFF_PrepProgramRules::BuildQuote(
		FacilityCard ? FacilityCard : SelectedCard, OwnerResult, bCanScheduleNow, FacilityCount, Gold);
	if (!FacilityCard)
	{
		// 한 프로그램도 시설마다 참가비가 다를 수 있다. 장소를 고르기 전에는 금액을 확정하지 않는다.
		Quote.MaxRevenue = INDEX_NONE;
	}


	// --- 카드 상태 ---
	// 선택한 시설에 편성한 카드만 "✓ 편성함" 이다. 다른 공방에 연 카드는 여기서 또 열 수 있다.
	const TArray<FName> ScheduledIds = (ViewModel && bHasFacility)
		? ViewModel->GetScheduledProgramIdsAtFacility(FacilityOptions[SelectedFacilityIndex].FacilityInstanceId)
		: TArray<FName>();

	for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
	{
		UFF_PrepProgramCard* CardWidget = CardWidgets[Index];
		if (!CardWidget || !Catalog.IsValidIndex(Index))
		{
			continue;
		}

		const FFF_ProgramCard& Card = Catalog[Index];
		CardWidget->SetRevenuePreview(Index == SelectedCardIndex ? FacilityCard : nullptr);

		// 상태를 매번 전부 다시 정한다. 부분 갱신하면 편성 뒤에도 선택 표시가 남는다.
		if (ScheduledIds.Contains(Card.ProgramId))
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Committed,
				LOCTEXT("PrepProgram_BadgeScheduled", "✓ 편성함"));
		}
		else if (!bCanScheduleNow)
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Locked,
				LOCTEXT("PrepProgram_BadgeNotStage", "지금은 편성 불가"));
		}
		else if (Index == SelectedCardIndex)
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Selected, FText::GetEmpty());
		}
		else
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Selectable, FText::GetEmpty());
		}
	}

	// --- 시설 선택 표시 ---
	for (int32 Index = 0; Index < FacilityWidgets.Num(); ++Index)
	{
		if (UFF_PrepChoiceButton* Choice = FacilityWidgets[Index])
		{
			Choice->SetSelected(Index == SelectedFacilityIndex);
		}
	}

	const bool bShowFacilityNotice = (SelectedCard && FacilityOptions.Num() == 0);
	FText FacilityNotice;
	if (SelectedCard)
	{
		const UGameInstance* GI = GetGameInstance();
		const UFF_BuildCatalogSubsystem* BuildCatalog = GI ? GI->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
		FacilityNotice = BuildCatalog && BuildCatalog->IsCatalogReady()
			? FFF_ProgramFacilityGuide::BuildFacilityNotice(*SelectedCard, CompatibleCatalogEntries, FacilityCount)
			: LOCTEXT("PrepProgram_LoadingFacilities", "프로그램을 열 수 있는 시설 목록을 불러오는 중입니다.");
		if (!BuildCatalog || BuildCatalog->GetLoadState() == EFF_BuildCatalogLoadState::Failed)
		{
			FacilityNotice = LOCTEXT("PrepProgram_FacilityCatalogFailed", "시설 목록을 불러오지 못했습니다. 배치 탭에서 건축목록을 다시 열어 주세요.");
		}
	}
	SetTextOrHide(Text_FacilityNotice, FacilityNotice);

	// --- 견적 ---
	SetTextOrHide(Text_CostPreview, (Quote.CostGold != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepProgram_CostPreview", "{0} Gold"),
			FText::AsNumber(Quote.CostGold))
		: FText::GetEmpty());

	// 🔴 `!= INDEX_NONE` 로 검사하지 않는다. 편성 후 잔액이 정확히 -1 인 경우가 실제로 있고
	// 그때 줄이 사라지면 **얼마나 모자란지 보여주려던 의도가 그 지점에서만 뒤집힌다.**
	SetTextOrHide(Text_GoldAfter, Quote.bHasGoldAfterSchedule
		? FText::Format(
			LOCTEXT("PrepProgram_GoldAfter", "편성 후 잔액 {0}"),
			FText::AsNumber(Quote.GoldAfterSchedule))
		: FText::GetEmpty());

	SetTextOrHide(Text_RevenuePreview, (Quote.MaxRevenue != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepProgram_FacilityRevenue", "선택 시설 최대 매출 {0} Gold"),
			FText::AsNumber(Quote.MaxRevenue))
		: (SelectedCard ? LOCTEXT("PrepProgram_RevenueAfterFacility", "시설 선택 후 매출을 확인할 수 있습니다.") : FText::GetEmpty()));

	// --- 편성 버튼 ---
	// 시설을 골랐는지는 규칙 struct 가 모른다 — 그건 화면의 상태이지 경제 판정이 아니다.
	if (Button_Schedule)
	{
		Button_Schedule->SetIsEnabled(Quote.CanSchedule() && bHasFacility);
	}

	FText BlockReason = BuildBlockReasonText();
	if (BlockReason.IsEmpty() && !bHasFacility && FacilityOptions.Num() > 1)
	{
		BlockReason = LOCTEXT("PrepProgram_PickFacility", "프로그램을 열 시설을 고르세요.");
	}

	// 🔴 같은 문구를 두 번 띄우지 않는다. `열 장소` 안내가 이미 그 말을 하고 있으면
	// 하단 사유는 비운다 — 화면 두 곳에 같은 문장이 있으면 읽는 사람이 둘을 다른 것으로 본다.
	if (bShowFacilityNotice && Quote.BlockReason == EFF_PrepProgramBlockReason::NoEligibleFacility)
	{
		BlockReason = FText::GetEmpty();
	}

	// 🔴 `Collapsed` 가 아니라 `Hidden` 이다. 이 칸은 `Fill` 이라 접으면 슬롯째 사라지고
	// **편성하기 버튼이 왼쪽으로 튄다.** 자리는 남기고 글자만 지운다.
	if (Text_BlockReason)
	{
		Text_BlockReason->SetText(BlockReason);
		Text_BlockReason->SetVisibility(BlockReason.IsEmpty()
			? ESlateVisibility::Hidden
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

FText UFF_PrepPanelProgram::BuildBlockReasonText() const
{
	switch (Quote.BlockReason)
	{
	case EFF_PrepProgramBlockReason::Unknown:
		return LOCTEXT("PrepProgram_Unknown", "축제가 아직 시작되지 않아 프로그램 정보를 읽을 수 없습니다.");

	case EFF_PrepProgramBlockReason::NotSchedulingStage:
		return LOCTEXT("PrepProgram_NotStage", "프로그램은 셋업과 개장 준비에서만 편성할 수 있습니다.");

	case EFF_PrepProgramBlockReason::NoCardSelected:
		return LOCTEXT("PrepProgram_NoCard", "편성할 프로그램을 고르세요.");

	case EFF_PrepProgramBlockReason::AlreadyScheduled:
		return LOCTEXT("PrepProgram_Already", "오늘 이미 편성한 프로그램입니다. 내일 다시 편성할 수 있습니다.");

	case EFF_PrepProgramBlockReason::NoEligibleFacility:
		return LOCTEXT("PrepProgram_NoFacilityReason", "이 프로그램을 열 수 있는 시설이 없습니다. `배치` 탭에서 먼저 지으세요.");

	case EFF_PrepProgramBlockReason::NotEnoughGold:
		return (Quote.CostGold != INDEX_NONE)
			? FText::Format(
				LOCTEXT("PrepProgram_NoGold", "{0} Gold 를 낼 잔액이 없습니다."),
				FText::AsNumber(Quote.CostGold))
			: LOCTEXT("PrepProgram_NoGoldNoCost", "편성 비용을 낼 잔액이 없습니다.");

	case EFF_PrepProgramBlockReason::UnknownProgram:
		return LOCTEXT("PrepProgram_UnknownCard", "카탈로그에 없는 프로그램입니다. 데이터를 확인해야 합니다.");

	default:
		return FText::GetEmpty();
	}
}

// =============================================================================
// 입력
// =============================================================================

void UFF_PrepPanelProgram::HandleCardClicked(int32 ChoiceId)
{
	if (!Catalog.IsValidIndex(ChoiceId))
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 카탈로그 범위를 벗어난 카드 %d 를 눌렀다. 목록과 위젯이 갈라졌다."),
			*GetName(), ChoiceId);
		return;
	}

	// 같은 카드를 다시 누르면 선택을 푼다. 날씨와 달리 "아무것도 안 고른 상태"가 유효하다.
	SelectedCardIndex = (SelectedCardIndex == ChoiceId) ? INDEX_NONE : ChoiceId;

	// 카드가 바뀌면 열 수 있는 시설도 바뀐다. 목록을 먼저 새로 만든 뒤 그린다.
	RebuildFacilityChoices();
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleFacilityClicked(int32 ChoiceId)
{
	if (!FacilityOptions.IsValidIndex(ChoiceId))
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 시설 목록 범위를 벗어난 %d 를 눌렀다."), *GetName(), ChoiceId);
		return;
	}

	SelectedFacilityIndex = ChoiceId;
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleCatalogFacilityClicked(int32 ChoiceId)
{
	if (!CompatibleCatalogEntries.IsValidIndex(ChoiceId)) { return; }
	const FName DefinitionId = CompatibleCatalogEntries[ChoiceId].FacilityDefinitionId;
	UFF_BuildModeSubsystem* BuildMode = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	// The build input bridge switches the owning HUD to the build tab and focuses this exact card.
	if (!BuildMode || !BuildMode->OpenCatalogForFacility(DefinitionId))
	{
		SetTextOrHide(Text_BlockReason, LOCTEXT("PrepProgram_CatalogOpenFailed",
			"지금은 건축목록을 열 수 없습니다. 준비 단계에서 다시 시도해 주세요."));
	}
}

void UFF_PrepPanelProgram::HandleBuildCatalogChanged()
{
	RebuildFacilityChoices();
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleScheduleClicked()
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const FFF_ProgramCard* Card = FindSelectedCard();

	if (!ViewModel || !Card || !FacilityOptions.IsValidIndex(SelectedFacilityIndex))
	{
		// 버튼이 꺼져 있어야 하는 상태다. 여기 왔다면 표시와 상태가 갈라진 것이므로 남긴다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 편성 요청에 필요한 값이 없다. 버튼 활성 조건을 확인한다."), *GetName());
		return;
	}

	// !! 낙관적 갱신을 하지 않는다 !!
	// 실패하면 아무것도 바뀌지 않는다. 화면은 결과 이벤트로만 갱신된다.
	const EFF_ProgramPurchaseResult Result = ViewModel->RequestScheduleProgram(
		Card->ProgramId, FacilityOptions[SelectedFacilityIndex].FacilityInstanceId);

	if (Result != EFF_ProgramPurchaseResult::Success)
	{
		// 🔴 실패 문구를 enum `DisplayName` 으로 만들지 않는다. 패키징에서 영문이 뜬다
		// (2026-08-19 김시리 정정). 표시 문구는 `UFF_EconomyDisplayTextLibrary` 가 소유한다.
		SetTextOrHide(Text_BlockReason,
			UFF_EconomyDisplayTextLibrary::GetProgramPurchaseResultText(Result));

		// 실패하면 이벤트가 오지 않으므로 여기서 한 번 다시 그린다 — 그러지 않으면
		// 눌렀는데 아무 반응이 없는 화면이 된다. 사유 문구는 위에서 이미 덮었다.
		return;
	}

	// 성공하면 `OnScheduledProgramsChanged` 가 카드 잠금까지 끌고 온다.
	// 선택은 거기서 비운다 — 편성한 카드가 골라진 채로 남으면 두 번 누르기 쉽다.
}

// =============================================================================
// 결과 이벤트
// =============================================================================

void UFF_PrepPanelProgram::HandleGoldChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleScheduledProgramsChanged()
{
	// 편성이 성사됐으면 그 카드는 이제 못 고른다. 선택을 비우지 않으면 잠긴 카드가
	// 골라진 채로 남아 견적이 그 카드 기준으로 계속 그려진다.
	SelectedCardIndex = INDEX_NONE;

	RebuildFacilityChoices();
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	// 날이 바뀌면 편성 목록이 비워지고 카탈로그 잠금도 풀린다. 목록째 다시 만든다.
	SelectedCardIndex = INDEX_NONE;

	RebuildCards();
	RebuildFacilityChoices();
	RefreshAll();
}

// =============================================================================
// 표시 헬퍼
// =============================================================================

void UFF_PrepPanelProgram::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
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
