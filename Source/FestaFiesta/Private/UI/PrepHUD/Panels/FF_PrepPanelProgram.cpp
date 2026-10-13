// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"

#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/WrapBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSWorldSubsystem.h"
#include "UI/FF_UILog.h"
#include "UI/PrepHUD/Panels/FF_ProgramFacilityGuide.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCatalogChoice.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramFacilityRow.h"
#include "UI/PrepHUD/Panels/FF_PrepScheduledProgramRow.h"

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
		// 완료된 시설을 다시 확인할 때 비용을 한 번 더 차감한 견적을 보여 주지 않는다.
		Result.GoldAfterSchedule = OwnerResult == EFF_ProgramPurchaseResult::AlreadyScheduled
			? CurrentGold : CurrentGold - Card->CostGold;
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
	if (Border_FacilityGuide) NormalFacilityGuideBrush = Border_FacilityGuide->Background;
	if (Text_FacilityGuideTitle) NormalFacilityGuideTitleColor = Text_FacilityGuideTitle->GetColorAndOpacity();
	HandleDismissScheduleToast();
	if (Button_DismissScheduleToast)
	{
		Button_DismissScheduleToast->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDismissScheduleToast);
	}

	if (DailyNoticeText.IsEmpty())
	{
		DailyNoticeText = LOCTEXT("PrepProgram_DailyNotice",
			"프로그램은 하루짜리입니다. 다음 날에는 다시 편성해야 합니다.");
	}
	SetTextOrHide(Text_DailyNotice, bUseCompactLayout ? LOCTEXT("CompactDailyRule", "오늘만 운영") : DailyNoticeText);
	if (Text_DailyNotice && bUseCompactLayout)
	{
		Text_DailyNotice->SetVisibility(ESlateVisibility::Visible);
		Text_DailyNotice->SetToolTipText(DailyNoticeText);
	}
	if (Combo_Facilities)
	{
		Combo_Facilities->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleFacilitySelectionChanged);
		// 디자이너가 직접 연결한 항목 생성 함수는 보존한다.
		if (!Combo_Facilities->OnGenerateWidgetEvent.IsBound())
		{
			Combo_Facilities->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::GenerateFacilityOptionWidget);
		}
	}

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

	// 위치 확인으로 접었다가 돌아올 때만 선택을 복원한다. 시설은 최신 GUID 목록에서 찾는다.
	SelectedCardIndex = INDEX_NONE;
	SelectedFacilityIndex = INDEX_NONE;
	AttentionProgramId = NAME_None;
	HandleDismissScheduleToast();

	BindViewModel();
	RebuildCards();
	if (!ResumeProgramId.IsNone())
	{
		SelectedCardIndex = Catalog.IndexOfByPredicate([this](const FFF_ProgramCard& Card)
		{
			return Card.ProgramId == ResumeProgramId;
		});
	}
	RebuildFacilityChoices();
	if (ResumeFacilityId.IsValid())
	{
		SelectedFacilityIndex = FacilityOptions.IndexOfByPredicate([this](const FFF_PrepFacilityOption& Facility)
		{
			return Facility.FacilityInstanceId == ResumeFacilityId;
		});
		RefreshFacilityCombo();
	}
	ResumeProgramId = NAME_None;
	ResumeFacilityId.Invalidate();
	RefreshAll();
}

void UFF_PrepPanelProgram::NativeOnPanelClosed()
{
	HandleDismissScheduleToast();
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

void UFF_PrepPanelProgram::NativeDestruct()
{
	HandleDismissScheduleToast();
	UnbindViewModel();
	Super::NativeDestruct();
}

void UFF_PrepPanelProgram::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 게임 배속·셋업 시간 정지와 무관하게 실제 표시 시간으로 닫는다.
	if (ScheduleToastExpiresAt > 0.0 && FPlatformTime::Seconds() >= ScheduleToastExpiresAt)
	{
		HandleDismissScheduleToast();
	}
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
	BoundFacilityWorld = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (BoundFacilityWorld)
	{
		BoundFacilityWorld->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleFacilityWorldChanged);
		BoundFacilityWorld->OnWorldSnapshotRestored.AddUniqueDynamic(this, &ThisClass::HandleBuildCatalogChanged);
	}
}

void UFF_PrepPanelProgram::UnbindViewModel()
{
	if (BoundFacilityWorld)
	{
		BoundFacilityWorld->OnWorldChanged.RemoveAll(this);
		BoundFacilityWorld->OnWorldSnapshotRestored.RemoveAll(this);
		BoundFacilityWorld = nullptr;
	}
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
	HandleDismissScheduleToast();
	AttentionProgramId = NAME_None;
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
	SetTextOrHide(Text_EmptyCatalog, Catalog.IsEmpty() ? EmptyCatalogText : FText::GetEmpty());

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
	const FGuid PreviousFacility = FacilityOptions.IsValidIndex(SelectedFacilityIndex)
		? FacilityOptions[SelectedFacilityIndex].FacilityInstanceId : FGuid();
	RebuildCatalogChoices();
	FacilityWidgets.Reset();
	FacilityRowWidgets.Reset();
	if (VerticalBox_FacilityInstances) VerticalBox_FacilityInstances->ClearChildren();
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
		RefreshFacilityCombo();
		return;
	}

	if (Box_FacilityRow)
	{
		Box_FacilityRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	FacilityOptions = ViewModel->GetEligibleFacilitiesForProgram(Card->ProgramId);

	UClass* ResolvedChoiceClass = ResolveWidgetClass(FacilityChoiceClass, FacilityChoiceFallbackPath);
	UClass* ResolvedRowClass = VerticalBox_FacilityInstances ? ResolveWidgetClass(FacilityRowClass, FacilityRowFallbackPath) : nullptr;
	if (VerticalBox_FacilityInstances && ResolvedRowClass)
	{
		for (int32 Index = 0; Index < FacilityOptions.Num(); ++Index)
		{
			UFF_PrepProgramFacilityRow* Row = CreateWidget<UFF_PrepProgramFacilityRow>(this, ResolvedRowClass);
			if (!Row) continue;
			Row->ConfigureFacility(Index, FacilityOptions[Index]);
			Row->OnFacilitySelected.BindUObject(this, &ThisClass::HandleFacilityClicked);
			Row->OnLocateRequested.BindUObject(this, &ThisClass::HandleLocateFacilityClicked);
			if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(VerticalBox_FacilityInstances->AddChild(Row)))
			{
				RowSlot->SetHorizontalAlignment(HAlign_Fill);
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			}
			FacilityRowWidgets.Add(Row);
		}
	}
	else if (!Combo_Facilities && Box_FacilityChoices && ResolvedChoiceClass)
	{
		for (int32 Index = 0; Index < FacilityOptions.Num(); ++Index)
		{
			UFF_PrepChoiceButton* Choice = CreateWidget<UFF_PrepChoiceButton>(this, ResolvedChoiceClass);
			if (!Choice)
			{
				continue;
			}

			Choice->ConfigureChoice(Index, FacilityOptions[Index].InstanceLabel);
			Choice->OnHudTabButtonClicked.BindUObject(this, &UFF_PrepPanelProgram::HandleFacilityClicked);

			Box_FacilityChoices->AddChild(Choice);
			FacilityWidgets.Add(Choice);
		}
	}
	else if (!Combo_Facilities && FacilityOptions.Num() > 0)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 시설 선택지를 그릴 자리나 클래스가 없다. 열 수 있는 시설 %d곳을 못 보여준다."),
			*GetName(), FacilityOptions.Num());
	}

	// 한 곳뿐이면 골라 준다. 선택지가 하나인데 누르게 하는 것은 일만 늘린다.
	if (Combo_Facilities && VerticalBox_FacilityInstances)
	{
		// 새 행 에셋을 못 읽은 경우에도 기존 선택 상자로 편성할 수 있게 한다.
		Combo_Facilities->SetVisibility(ResolvedRowClass ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	// **여러 곳이면 고르게 둔다** — 어디서 여는지가 대기열과 동선에 영향을 준다.
	SelectedFacilityIndex = FacilityOptions.IndexOfByPredicate([&PreviousFacility](const FFF_PrepFacilityOption& Option)
	{
		return PreviousFacility.IsValid() && Option.FacilityInstanceId == PreviousFacility;
	});
	if (SelectedFacilityIndex == INDEX_NONE && FacilityOptions.Num() == 1)
	{
		SelectedFacilityIndex = 0;
	}
	RefreshFacilityCombo();
}

void UFF_PrepPanelProgram::RefreshFacilityCombo()
{
	if (!Combo_Facilities) { return; }
	TGuardValue<bool> RefreshGuard(bRefreshingFacilityCombo, true);
	Combo_Facilities->ClearOptions();
	Combo_Facilities->AddOption((FacilityOptions.IsEmpty() ? NoFacilityText : FacilityPlaceholderText).ToString());
	for (int32 Index = 0; Index < FacilityOptions.Num(); ++Index)
	{
		// 같은 이름의 부스도 별개 선택지다. 표시 문자열은 고유하게 만들고 실제 요청은 GUID를 쓴다.
		Combo_Facilities->AddOption(FText::Format(
			LOCTEXT("FacilityOptionLocated", "{0} · {1}"),
			FacilityOptions[Index].InstanceLabel, FacilityOptions[Index].LocationText).ToString());
	}
	Combo_Facilities->SetSelectedIndex(FacilityOptions.IsValidIndex(SelectedFacilityIndex) ? SelectedFacilityIndex + 1 : 0);
	Combo_Facilities->SetIsEnabled(!FacilityOptions.IsEmpty());
}

UWidget* UFF_PrepPanelProgram::GenerateFacilityOptionWidget(FString Item)
{
	if (!Combo_Facilities) { return nullptr; }
	UTextBlock* Label = NewObject<UTextBlock>(this);
	Label->SetText(FText::FromString(Item));
	Label->SetToolTipText(FText::FromString(Item));
	Label->SetFont(Combo_Facilities->GetFont());
	Label->SetColorAndOpacity(Combo_Facilities->GetForegroundColor());
	Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	return Label;
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
	UClass* ChoiceClass = ResolveWidgetClass(CatalogChoiceClass, CatalogChoiceFallbackPath);
	if (!ChoiceClass)
	{
		// 이전 에셋을 아직 사용 중인 프로젝트도 기존 바로가기는 계속 동작한다.
		ChoiceClass = ResolveWidgetClass(FacilityChoiceClass, FacilityChoiceFallbackPath);
	}
	if (!ChoiceClass) { return; }
	for (int32 Index = 0; Index < CompatibleCatalogEntries.Num(); ++Index)
	{
		UFF_PrepChoiceButton* Choice = CreateWidget<UFF_PrepChoiceButton>(this, ChoiceClass);
		if (!Choice) { continue; }
		const FFF_BuildCatalogEntry& Entry = CompatibleCatalogEntries[Index];
		if (UFF_PrepProgramCatalogChoice* CatalogChoice = Cast<UFF_PrepProgramCatalogChoice>(Choice))
		{
			CatalogChoice->ConfigureCatalogChoice(Index, Entry.DisplayName);
		}
		else
		{
			Choice->ConfigureChoice(Index, FText::Format(
				LOCTEXT("PrepProgram_FindInCatalog", "{0} · 건축목록에서 보기"), Entry.DisplayName));
		}
		Choice->SetToolTipText(FFF_ProgramFacilityGuide::GetCatalogPath(Entry));
		Choice->OnHudTabButtonClicked.BindUObject(this, &ThisClass::HandleCatalogFacilityClicked);
		if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(Box_CatalogChoices))
		{
			const int32 Columns = FMath::Clamp(CatalogChoiceColumns, 1, 4);
			UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Choice, Index / Columns, Index % Columns);
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
		else
		{
			Box_CatalogChoices->AddChild(Choice);
		}
		CatalogChoiceWidgets.Add(Choice);
	}
}

void UFF_PrepPanelProgram::RefreshFacilityGuide()
{
	const FFF_ProgramCard* Card = FindSelectedCard();
	const bool bNeedsFacility = Card && FacilityOptions.IsEmpty();
	const UGameInstance* GI = GetGameInstance();
	const UFF_BuildCatalogSubsystem* BuildCatalog = GI ? GI->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	const bool bCatalogReady = BuildCatalog && BuildCatalog->IsCatalogReady();
	FText Title = Card ? AvailableFacilityTitleText : FText::GetEmpty();
	FText Description = Card ? AvailableFacilityDescriptionText : FText::GetEmpty();
	if (bNeedsFacility)
	{
		if (!BuildCatalog || BuildCatalog->GetLoadState() == EFF_BuildCatalogLoadState::Failed)
		{
			Title = LOCTEXT("PrepProgram_FacilityCatalogFailedTitle", "시설 목록을 확인할 수 없습니다");
			Description = LOCTEXT("PrepProgram_FacilityCatalogFailed", "시설 목록을 불러오지 못했습니다. 배치 탭에서 건축목록을 다시 열어 주세요.");
		}
		else if (!bCatalogReady)
		{
			Title = LOCTEXT("PrepProgram_FacilityCatalogLoadingTitle", "시설 목록 확인 중");
			Description = LOCTEXT("PrepProgram_LoadingFacilities", "프로그램을 열 수 있는 시설 목록을 불러오는 중입니다.");
		}
		else
		{
			Title = MissingFacilityTitleText;
			Description = CompatibleCatalogEntries.IsEmpty()
				? LOCTEXT("PrepProgram_NoCompatibleCatalog", "이 프로그램에 맞는 시설을 건축목록에서 찾지 못했습니다.")
				: MissingFacilityDescriptionText;
		}
	}
	if (Border_FacilityGuide)
	{
		Border_FacilityGuide->SetVisibility(Card ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		FSlateBrush Brush = NormalFacilityGuideBrush;
		if (bNeedsFacility)
		{
			Brush.TintColor = MissingGuideBackgroundColor;
			Brush.OutlineSettings.Color = MissingGuideOutlineColor;
		}
		Border_FacilityGuide->SetBrush(Brush);
	}
	SetTextOrHide(Text_FacilityGuideTitle, Title);
	if (Text_FacilityGuideTitle)
	{
		Text_FacilityGuideTitle->SetColorAndOpacity(bNeedsFacility ? FSlateColor(MissingGuideTitleColor) : NormalFacilityGuideTitleColor);
	}
	// 정상 상태의 반복 설명은 접되, 로드 실패·진행 중·호환 시설 없음은 남긴다.
	if (bUseCompactLayout && Card && bCatalogReady && !CompatibleCatalogEntries.IsEmpty()) Description = FText::GetEmpty();
	SetTextOrHide(Text_FacilityNotice, Description);
	if (Box_CatalogChoices)
	{
		Box_CatalogChoices->SetVisibility(Card && bCatalogReady && !CompatibleCatalogEntries.IsEmpty()
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	for (UFF_PrepChoiceButton* Choice : CatalogChoiceWidgets)
	{
		UFF_PrepProgramCatalogChoice* CatalogChoice = Cast<UFF_PrepProgramCatalogChoice>(Choice);
		if (!CatalogChoice || !CompatibleCatalogEntries.IsValidIndex(Choice->GetChoiceId())) continue;
		const FName DefinitionId = CompatibleCatalogEntries[Choice->GetChoiceId()].FacilityDefinitionId;
		const int32 InstalledCount = FacilityOptions.FilterByPredicate([DefinitionId](const FFF_PrepFacilityOption& Facility)
		{
			return Facility.FacilityDefinitionId == DefinitionId;
		}).Num();
		CatalogChoice->SetInstalledCount(InstalledCount);
	}
	if (!bNeedsFacility)
	{
		AttentionProgramId = NAME_None;
		for (UFF_PrepChoiceButton* Choice : CatalogChoiceWidgets)
		{
			if (UFF_PrepProgramCatalogChoice* CatalogChoice = Cast<UFF_PrepProgramCatalogChoice>(Choice))
			{
				CatalogChoice->SetAttention(false);
			}
		}
	}
	else if (bCatalogReady && !CompatibleCatalogEntries.IsEmpty() && AttentionProgramId != Card->ProgramId)
	{
		AttentionProgramId = Card->ProgramId;
		for (UFF_PrepChoiceButton* Choice : CatalogChoiceWidgets)
		{
			if (UFF_PrepProgramCatalogChoice* CatalogChoice = Cast<UFF_PrepProgramCatalogChoice>(Choice))
			{
				CatalogChoice->SetAttention(true);
			}
		}
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
	const int32 Day = ViewModel ? ViewModel->GetCurrentDay() : INDEX_NONE;
	SetTextOrHide(Text_SelectedProgram, !bUseCompactLayout ? (SelectedCard ? SelectedCard->DisplayName : NoSelectionText) : FText::GetEmpty());
	SetTextOrHide(Text_Day, Day > 0 ? FText::Format(DayTextFormat, FText::AsNumber(Day)) : FText::GetEmpty());
	SetTextOrHide(Text_EmptyCatalog, Catalog.IsEmpty() ? EmptyCatalogText : FText::GetEmpty());

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
	const TArray<FName> AllScheduledIds = ViewModel ? ViewModel->GetScheduledProgramIds() : TArray<FName>();
	const bool bSelectedScheduled = SelectedCard && ScheduledIds.Contains(SelectedCard->ProgramId);
	if (bSelectedScheduled && Gold != INDEX_NONE)
	{
		Quote.GoldAfterSchedule = Gold;
		Quote.bHasGoldAfterSchedule = true;
	}

	for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
	{
		UFF_PrepProgramCard* CardWidget = CardWidgets[Index];
		if (!CardWidget || !Catalog.IsValidIndex(Index))
		{
			continue;
		}

		const FFF_ProgramCard& Card = Catalog[Index];
		CardWidget->SetRevenuePreview(Index == SelectedCardIndex ? FacilityCard : nullptr, bUseCompactLayout);
		CardWidget->SetScheduledFacilityCount(AllScheduledIds.FilterByPredicate([&Card](FName ProgramId)
		{
			return ProgramId == Card.ProgramId;
		}).Num(), bUseCompactLayout);

		// 단계 제한만 카드를 잠근다. 완료 카드는 읽고 다른 시설을 고를 수 있다.
		if (!bCanScheduleNow)
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Locked,
				LOCTEXT("PrepProgram_BadgeNotStage", "지금은 편성 불가"));
		}
		else if (ScheduledIds.Contains(Card.ProgramId))
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Committed, FText::GetEmpty());
		}
		else if (Index == SelectedCardIndex)
		{
			CardWidget->ApplyCardState(EFF_PrepCardState::Selected,
				LOCTEXT("PrepProgram_BadgeSelected", "✓ 선택됨"));
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
	for (int32 Index = 0; Index < FacilityRowWidgets.Num(); ++Index)
	{
		if (UFF_PrepProgramFacilityRow* Row = FacilityRowWidgets[Index]; Row && FacilityOptions.IsValidIndex(Index))
		{
			const bool bScheduled = ViewModel && SelectedCard
				&& ViewModel->GetScheduledProgramIdsAtFacility(FacilityOptions[Index].FacilityInstanceId).Contains(SelectedCard->ProgramId);
			Row->SetFacilityState(Index == SelectedFacilityIndex, bScheduled);
		}
	}
	SetTextOrHide(Text_FacilityLabel, SelectedCard ? FText::Format(
		LOCTEXT("FacilityListHeading", "운영 시설 · 설치 {0}곳"), FText::AsNumber(FacilityOptions.Num())) : FText::GetEmpty());
	SetTextOrHide(Text_FacilityEmpty, SelectedCard && FacilityOptions.IsEmpty() ? NoFacilityText : FText::GetEmpty());

	const bool bShowFacilityNotice = (SelectedCard && FacilityOptions.Num() == 0);
	RefreshFacilityGuide();
	FText CompletionText;
	if (bSelectedScheduled)
	{
		FFormatNamedArguments CompletionArgs;
		CompletionArgs.Add(TEXT("Program"), SelectedCard->DisplayName);
		CompletionArgs.Add(TEXT("Facility"), FacilityOptions[SelectedFacilityIndex].InstanceLabel);
		CompletionText = FText::Format(ProgramCompletionFormat, CompletionArgs);
	}
	SetTextOrHide(Text_ProgramCompletion, bUseCompactLayout ? FText::GetEmpty() : CompletionText);
	SetTextOrHide(Text_SummaryStatus, !bUseCompactLayout && bSelectedScheduled ? SummaryScheduledText : FText::GetEmpty());

	// --- 견적 ---
	SetTextOrHide(Text_CostPreview, (!bUseCompactLayout && Quote.CostGold != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepProgram_CostPreview", "{0} Gold"),
			FText::AsNumber(Quote.CostGold))
		: FText::GetEmpty());

	// 🔴 `!= INDEX_NONE` 로 검사하지 않는다. 편성 후 잔액이 정확히 -1 인 경우가 실제로 있고
	// 그때 줄이 사라지면 **얼마나 모자란지 보여주려던 의도가 그 지점에서만 뒤집힌다.**
	SetTextOrHide(Text_GoldAfter, Quote.bHasGoldAfterSchedule
		? FText::Format(
			bSelectedScheduled ? CurrentGoldFormat : LOCTEXT("PrepProgram_GoldAfter", "편성 후 잔액 {0}"),
			FText::AsNumber(Quote.GoldAfterSchedule))
		: FText::GetEmpty());

	SetTextOrHide(Text_RevenuePreview, bUseCompactLayout ? FText::GetEmpty() : ((Quote.MaxRevenue != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepProgram_FacilityRevenue", "선택 시설 최대 매출 {0} Gold"),
			FText::AsNumber(Quote.MaxRevenue))
		: (SelectedCard ? LOCTEXT("PrepProgram_RevenueAfterFacility", "시설 선택 후 매출을 확인할 수 있습니다.") : FText::GetEmpty())));

	// --- 편성 버튼 ---
	// 시설을 골랐는지는 규칙 struct 가 모른다 — 그건 화면의 상태이지 경제 판정이 아니다.
	if (Button_Schedule)
	{
		Button_Schedule->SetIsEnabled(Quote.CanSchedule() && bHasFacility && !bSelectedScheduled);
	}
	SetTextOrHide(Text_ScheduleAction, bSelectedScheduled ? ScheduledActionText
		: bUseCompactLayout && Quote.CostGold != INDEX_NONE
			? FText::Format(LOCTEXT("CompactScheduleAction", "{0} Gold · {1}"), FText::AsNumber(Quote.CostGold), ScheduleActionText)
			: ScheduleActionText);

	FText BlockReason = BuildBlockReasonText();
	if (bSelectedScheduled && bCanScheduleNow && Text_ProgramCompletion)
	{
		BlockReason = FText::GetEmpty();
	}
	if (BlockReason.IsEmpty() && !bHasFacility && FacilityOptions.Num() > 1)
	{
		BlockReason = LOCTEXT("PrepProgram_PickFacility", "프로그램을 열 시설을 고르세요.");
	}

	// 🔴 같은 문구를 두 번 띄우지 않는다. `열 장소` 안내가 이미 그 말을 하고 있으면
	// 하단 사유는 비운다 — 화면 두 곳에 같은 문장이 있으면 읽는 사람이 둘을 다른 것으로 본다.
	if (!Combo_Facilities && bShowFacilityNotice && Quote.BlockReason == EFF_PrepProgramBlockReason::NoEligibleFacility)
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
	RefreshScheduledPrograms();
}

void UFF_PrepPanelProgram::RefreshScheduledPrograms()
{
	if (!bUseCompactLayout || !VerticalBox_ScheduledPrograms) return;
	const UFF_FestivalViewModelSubsystem* VM = GetViewModel();
	TArray<FFF_ScheduledProgram> Scheduled;
	const bool bKnown = VM && VM->TryGetScheduledPrograms(Scheduled);
	SetTextOrHide(Text_ScheduledCount, bKnown
		? FText::Format(LOCTEXT("CompactScheduledCount", "총 {0}건"), FText::AsNumber(Scheduled.Num()))
		: LOCTEXT("CompactScheduledUnknownCount", "—"));
	SetTextOrHide(Text_ScheduledEmpty, !bKnown
		? LOCTEXT("CompactScheduleUnknown", "편성 정보를 확인할 수 없습니다.")
		: Scheduled.IsEmpty() ? LOCTEXT("CompactScheduleEmpty", "아직 편성한 프로그램이 없습니다.") : FText::GetEmpty());
	UClass* RowClass = Scheduled.IsEmpty() ? nullptr : ResolveWidgetClass(SummaryRowClass, SummaryRowFallbackPath);
	if (!Scheduled.IsEmpty() && !RowClass)
	{
		ScheduledRowWidgets.Reset();
		VerticalBox_ScheduledPrograms->ClearChildren();
		SetTextOrHide(Text_ScheduledEmpty, LOCTEXT("CompactScheduleRowsUnavailable", "편성 목록을 표시할 수 없습니다."));
		return;
	}
	// 금화나 선택만 바뀔 때 행을 새로 만들지 않아 스크롤과 위젯을 보존한다.
	while (ScheduledRowWidgets.Num() > Scheduled.Num())
	{
		if (UFF_PrepScheduledProgramRow* Row = ScheduledRowWidgets.Last()) Row->RemoveFromParent();
		ScheduledRowWidgets.Pop();
	}
	while (ScheduledRowWidgets.Num() < Scheduled.Num())
	{
		auto* Row = CreateWidget<UFF_PrepScheduledProgramRow>(this, RowClass);
		if (!Row)
		{
			SetTextOrHide(Text_ScheduledEmpty, LOCTEXT("CompactScheduleRowsUnavailable", "편성 목록을 표시할 수 없습니다."));
			break;
		}
		if (auto* RowSlot = Cast<UVerticalBoxSlot>(VerticalBox_ScheduledPrograms->AddChild(Row)))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		ScheduledRowWidgets.Add(Row);
	}
	TMap<FName, TArray<FFF_PrepFacilityOption>> FacilitiesByProgram;
	for (int32 Index = 0; Index < ScheduledRowWidgets.Num(); ++Index)
	{
		const FFF_ScheduledProgram& Entry = Scheduled[Index];
		const FFF_ProgramCard* Card = FFF_ProgramScheduleRules::FindCard(Catalog, Entry.ProgramId);
		FText FacilityName = LOCTEXT("CompactScheduledFacilityUnknown", "시설 정보 확인 불가");
		FText TimeRange;
		if (Card)
		{
			if (!FacilitiesByProgram.Contains(Entry.ProgramId))
				FacilitiesByProgram.Add(Entry.ProgramId, VM->GetEligibleFacilitiesForProgram(Entry.ProgramId));
			const auto* Facility = FacilitiesByProgram[Entry.ProgramId].FindByPredicate([&Entry](const FFF_PrepFacilityOption& Option)
			{
				return Option.FacilityInstanceId == Entry.FacilityInstanceId;
			});
			if (Facility) FacilityName = Facility->InstanceLabel;
			TimeRange = FText::Format(LOCTEXT("PrepProgram_TimeRange", "{0} ~ {1}"),
				VM->GetLiveClockTextAtMinutes(FMath::RoundToInt(Card->StartMinutes)),
				VM->GetLiveClockTextAtMinutes(FMath::RoundToInt(Card->EndMinutes)));
		}
		ScheduledRowWidgets[Index]->ConfigureProgram(Card ? Card->DisplayName
			: LOCTEXT("CompactScheduledProgramUnknown", "프로그램 정보 확인 불가"), FacilityName, TimeRange);
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
		return LOCTEXT("PrepProgram_AlreadyAtFacility", "이 시설에는 오늘 이미 편성했습니다. 다른 시설을 선택하면 추가로 편성할 수 있습니다.");

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
	SelectedFacilityIndex = INDEX_NONE;
	AttentionProgramId = NAME_None;

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
	RefreshFacilityCombo();
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleLocateFacilityClicked(int32 ChoiceId)
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const FFF_ProgramCard* Card = FindSelectedCard();
	if (!ViewModel || !Card || !FacilityOptions.IsValidIndex(ChoiceId)) return;
	if (!ViewModel->RequestLocateProgramFacility(FacilityOptions[ChoiceId].FacilityInstanceId))
	{
		RebuildFacilityChoices();
		RefreshAll();
		SetTextOrHide(Text_BlockReason, LOCTEXT("LocateFacilityFailed", "시설 위치를 확인할 수 없습니다. 시설 목록에서 다시 선택해 주세요."));
		return;
	}
	// 위치 보기는 편성 대상을 바꾸지 않는다. 중앙 패널을 접어 월드가 보이게 하고 복귀 선택을 보관한다.
	ResumeProgramId = Card->ProgramId;
	ResumeFacilityId = FacilityOptions.IsValidIndex(SelectedFacilityIndex)
		? FacilityOptions[SelectedFacilityIndex].FacilityInstanceId : FGuid();
	RequestClose();
}

void UFF_PrepPanelProgram::HandleFacilityWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	RebuildFacilityChoices();
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleFacilitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectedItem;
	(void)SelectionType;
	if (bRefreshingFacilityCombo || !Combo_Facilities) { return; }
	const int32 ChoiceId = Combo_Facilities->GetSelectedIndex() - 1;
	SelectedFacilityIndex = FacilityOptions.IsValidIndex(ChoiceId) ? ChoiceId : INDEX_NONE;
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

	// 요청 중에 잔액·편성 이벤트가 동기적으로 발생한다. 알림에 쓸 값은 먼저 복사한다.
	const FName RequestedProgramId = Card->ProgramId;
	const FText RequestedProgramName = Card->DisplayName;
	const FGuid RequestedFacilityId = FacilityOptions[SelectedFacilityIndex].FacilityInstanceId;
	const FText RequestedFacilityName = FacilityOptions[SelectedFacilityIndex].InstanceLabel;
	const FText TimeRange = FText::Format(LOCTEXT("PrepProgram_TimeRange", "{0} ~ {1}"),
		ViewModel->GetLiveClockTextAtMinutes(FMath::RoundToInt(Card->StartMinutes)),
		ViewModel->GetLiveClockTextAtMinutes(FMath::RoundToInt(Card->EndMinutes)));
	HandleDismissScheduleToast();
	// 성공 결과가 오기 전에는 완료 표시를 만들지 않는다.
	const EFF_ProgramPurchaseResult Result = ViewModel->RequestScheduleProgram(
		RequestedProgramId, RequestedFacilityId);

	if (Result != EFF_ProgramPurchaseResult::Success)
	{
		// 시설 철거나 외부 편성으로 목록이 바뀌었을 수 있다. 결과 사유는 갱신 후 마지막에 보여 준다.
		RebuildFacilityChoices();
		RefreshAll();
		// 🔴 실패 문구를 enum `DisplayName` 으로 만들지 않는다. 패키징에서 영문이 뜬다
		// (2026-08-19 김시리 정정). 표시 문구는 `UFF_EconomyDisplayTextLibrary` 가 소유한다.
		SetTextOrHide(Text_BlockReason,
			UFF_EconomyDisplayTextLibrary::GetProgramPurchaseResultText(Result));

		return;
	}

	// 결과를 확인할 수 있도록 프로그램과 시설 선택을 유지한다. 재요청만 비활성화한다.
	SelectedCardIndex = Catalog.IndexOfByPredicate([RequestedProgramId](const FFF_ProgramCard& Entry)
	{
		return Entry.ProgramId == RequestedProgramId;
	});
	RebuildFacilityChoices();
	SelectedFacilityIndex = FacilityOptions.IndexOfByPredicate([RequestedFacilityId](const FFF_PrepFacilityOption& Option)
	{
		return Option.FacilityInstanceId == RequestedFacilityId;
	});
	RefreshFacilityCombo();
	RefreshAll();
	ShowScheduleToast(RequestedProgramName, RequestedFacilityName, TimeRange);
}

void UFF_PrepPanelProgram::ShowScheduleToast(const FText& ProgramName, const FText& FacilityName, const FText& TimeRange)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Program"), ProgramName);
	Args.Add(TEXT("Facility"), FacilityName);
	Args.Add(TEXT("Time"), TimeRange);
	SetTextOrHide(Text_ScheduleToastTitle, FText::Format(ScheduleToastTitleFormat, Args));
	SetTextOrHide(Text_ScheduleToastDetail, FText::Format(ScheduleToastDetailFormat, Args));
	if (Border_ScheduleToast)
	{
		Border_ScheduleToast->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	ScheduleToastExpiresAt = FPlatformTime::Seconds() + FMath::Max(0.5f, ScheduleToastDuration);
}

void UFF_PrepPanelProgram::HandleDismissScheduleToast()
{
	ScheduleToastExpiresAt = 0.0;
	if (Border_ScheduleToast)
	{
		Border_ScheduleToast->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetTextOrHide(Text_ScheduleToastTitle, FText::GetEmpty());
	SetTextOrHide(Text_ScheduleToastDetail, FText::GetEmpty());
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
	// 완료된 시설을 그대로 보여 준다. 다른 카드·시설을 선택하는 흐름은 유지한다.
	RebuildFacilityChoices();
	RefreshAll();
}

void UFF_PrepPanelProgram::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	HandleDismissScheduleToast();
	AttentionProgramId = NAME_None;
	// 날이 바뀌면 편성 목록이 비워지고 카탈로그 잠금도 풀린다. 목록째 다시 만든다.
	SelectedCardIndex = INDEX_NONE;
	SelectedFacilityIndex = INDEX_NONE;

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
