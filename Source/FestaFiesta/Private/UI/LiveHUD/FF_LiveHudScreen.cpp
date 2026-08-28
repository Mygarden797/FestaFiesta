// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LiveHudScreen.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/LiveHUD/FF_LiveTabButton.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveHud"

namespace FFLiveHudScreenLocal
{
	/** enum 이름과 화면 문구를 한 곳에서 낸다. 흩어 두면 탭이 늘 때 한쪽만 고치게 된다. */
	void DescribeTab(EFF_LiveTab Tab, FString& OutEnumName, FText& OutDisplayName)
	{
		// 🔴 `UMETA(DisplayName)` 을 런타임에 읽지 않는다. 패키징 빌드에서 영문이 뜬다
		// (2026-08-19 김시리 정정). enum 이름만 리플렉션으로 얻고 문구는 여기서 만든다.
		if (const UEnum* TabEnum = StaticEnum<EFF_LiveTab>())
		{
			OutEnumName = TabEnum->GetNameStringByValue(static_cast<int64>(Tab));
		}

		switch (Tab)
		{
		case EFF_LiveTab::Operations:	OutDisplayName = LOCTEXT("LiveTab_Operations", "운영"); break;
		case EFF_LiveTab::Facilities:	OutDisplayName = LOCTEXT("LiveTab_Facilities", "시설"); break;
		case EFF_LiveTab::Guests:		OutDisplayName = LOCTEXT("LiveTab_Guests", "관객"); break;
		case EFF_LiveTab::Queues:		OutDisplayName = LOCTEXT("LiveTab_Queues", "대기열"); break;
		case EFF_LiveTab::SNS:			OutDisplayName = LOCTEXT("LiveTab_SNS", "SNS"); break;
		case EFF_LiveTab::Report:		OutDisplayName = LOCTEXT("LiveTab_Report", "리포트"); break;
		case EFF_LiveTab::Staff:		OutDisplayName = LOCTEXT("LiveTab_Staff", "스태프"); break;
		default:						OutDisplayName = FText::FromString(OutEnumName); break;
		}
	}
}

// =============================================================================
// 수명
// =============================================================================

void UFF_LiveHudScreen::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너에서도 탭 문구가 보이게 여기서 한 번 채운다.
	ForEachTabButton([](EFF_LiveTab Tab, UFF_LiveTabButton& Button)
	{
		FString EnumName;
		FText DisplayName;
		FFLiveHudScreenLocal::DescribeTab(Tab, EnumName, DisplayName);
		Button.Configure(Tab, DisplayName, FSlateBrush());
	});
}

void UFF_LiveHudScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 내부 버튼이라 인스턴스 수명과 같다. 외부 시스템 구독만 화면이 뜰 때 붙인다.
	ForEachTabButton([this](EFF_LiveTab Tab, UFF_LiveTabButton& Button)
	{
		Button.OnHudTabButtonClicked.BindUObject(this, &UFF_LiveHudScreen::HandleTabButtonClicked);
	});

	if (Button_Speed1)
	{
		Button_Speed1->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleSpeed1Clicked);
	}
	if (Button_Speed2)
	{
		Button_Speed2->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleSpeed2Clicked);
	}
	if (Button_Speed4)
	{
		Button_Speed4->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleSpeed4Clicked);
	}
}

void UFF_LiveHudScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// 🔴 전체 화면을 덮는 HUD 가 `Visible` 이면 월드 클릭을 전부 먹는다.
	// 준비 단계 바에서 실제로 건축 배치가 죽었다 — 라이브는 월드 조작이 더 잦아 더 치명적이다.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	BindViewModel();
	RefreshAll();
}

void UFF_LiveHudScreen::NativeDestruct()
{
	UnbindViewModel();
	Super::NativeDestruct();
}

// =============================================================================
// 구독
// =============================================================================

UFF_FestivalViewModelSubsystem* UFF_LiveHudScreen::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_LiveHudScreen::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 상태 행이 전부 비어 보인다."), *GetName());
		return;
	}

	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleStageViewChanged);
	CachedViewModel->OnRemainingSecondsChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleRemainingSecondsChanged);
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleGoldChanged);
	CachedViewModel->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleTodayRevenueChanged);
	CachedViewModel->OnSimSatisfactionChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleSimSatisfactionChanged);
	CachedViewModel->OnSafetyChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleSafetyChanged);
}

void UFF_LiveHudScreen::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleStageViewChanged);
		CachedViewModel->OnRemainingSecondsChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleRemainingSecondsChanged);
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleGoldChanged);
		CachedViewModel->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleTodayRevenueChanged);
		CachedViewModel->OnSimSatisfactionChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleSimSatisfactionChanged);
		CachedViewModel->OnSafetyChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleSafetyChanged);
	}
	CachedViewModel = nullptr;
}

// =============================================================================
// 탭
// =============================================================================

void UFF_LiveHudScreen::ForEachTabButton(TFunctionRef<void(EFF_LiveTab, UFF_LiveTabButton&)> Visitor) const
{
	// 이름 순서가 곧 화면 왼쪽부터의 순서다. enum 과 같게 유지한다.
	const TPair<EFF_LiveTab, UFF_LiveTabButton*> Bindings[] = {
		{ EFF_LiveTab::Operations,	Tab_Operations },
		{ EFF_LiveTab::Facilities,	Tab_Facilities },
		{ EFF_LiveTab::Guests,		Tab_Guests },
		{ EFF_LiveTab::Queues,		Tab_Queues },
		{ EFF_LiveTab::SNS,			Tab_SNS },
		{ EFF_LiveTab::Report,		Tab_Report },
		{ EFF_LiveTab::Staff,		Tab_Staff }
	};

	for (const TPair<EFF_LiveTab, UFF_LiveTabButton*>& Binding : Bindings)
	{
		if (Binding.Value)
		{
			Visitor(Binding.Key, *Binding.Value);
		}
	}
}

void UFF_LiveHudScreen::HandleTabButtonClicked(int32 TabId)
{
	ToggleTab(static_cast<EFF_LiveTab>(TabId));
}

void UFF_LiveHudScreen::ToggleTab(EFF_LiveTab Tab)
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Live;

	if (!FFF_LiveHudRules::IsTabVisible(Tab, Stage))
	{
		FString EnumName;
		FText DisplayName;
		FFLiveHudScreenLocal::DescribeTab(Tab, EnumName, DisplayName);

		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 지금 단계에서 볼 수 없는 탭(%s)을 열려고 했다. 무시한다."),
			*GetName(), *EnumName);
		return;
	}

	ApplySelection(FFF_LiveHudRules::ToggleTab(Selection, Tab));
}

void UFF_LiveHudScreen::CloseCurrentPanel()
{
	ApplySelection(FFF_LiveTabSelection::None());
}

void UFF_LiveHudScreen::ApplySelection(const FFF_LiveTabSelection& NewSelection)
{
	if (NewSelection == Selection)
	{
		return;
	}

	// 1) 열려 있던 것을 닫는다. **패널이 스스로 닫지 않는다** — 여기가 유일한 경로다.
	if (Selection.bHasSelection)
	{
		if (TObjectPtr<UFF_LivePanelBase>* Previous = PanelWidgets.Find(Selection.Tab))
		{
			if (*Previous)
			{
				(*Previous)->SetVisibility(ESlateVisibility::Collapsed);
				(*Previous)->HandlePanelClosed();
			}
		}
	}

	Selection = NewSelection;

	// 2) 새 패널을 연다. 만들지 못하면 닫힌 상태로 되돌린다 — 탭만 눌린 채로 두지 않는다.
	if (Selection.bHasSelection)
	{
		// 자리를 먼저 맞춘 뒤 패널을 넣는다. 순서가 반대면 이전 탭의 자리로 한 프레임 그려진다.
		ApplyPanelLayout(ResolveLayout(Selection.Tab));

		if (UFF_LivePanelBase* Panel = EnsurePanel(Selection.Tab))
		{
			Panel->SetVisibility(ESlateVisibility::Visible);
			Panel->HandlePanelOpened();
		}
		else
		{
			Selection = FFF_LiveTabSelection::None();
		}
	}

	RefreshTabs();
	OnTabSelectionChanged.Broadcast(Selection);
}

// =============================================================================
// 패널
// =============================================================================

UClass* UFF_LiveHudScreen::ResolvePanelClass(EFF_LiveTab Tab, const FString& EnumName, bool& bOutIsPlaceholder) const
{
	bOutIsPlaceholder = false;

	// 1) 예외 표. 이름 규칙에서 벗어나는 패널만 여기 들어간다.
	if (const TSoftClassPtr<UFF_LivePanelBase>* Mapped = PanelClasses.Find(Tab))
	{
		if (!Mapped->IsNull())
		{
			if (UClass* MappedClass = Mapped->LoadSynchronous())
			{
				return MappedClass;
			}

			// 배선을 해 놓고 로드가 안 되는 것은 오타나 삭제된 에셋이다. 조용히 넘기지 않는다.
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] `%s` 탭의 패널 클래스를 로드하지 못했다 (%s). 이름 규칙으로 다시 찾는다."),
				*GetName(), *EnumName, *Mapped->ToString());
		}
	}

	// 2) 이름 규칙. 패널을 하나 만들 때마다 표를 채우지 않아도 되게 한다.
	if (!PanelClassFolder.IsEmpty() && !PanelClassNameFormat.IsEmpty() && !EnumName.IsEmpty())
	{
		const FString AssetName = FString::Format(*PanelClassNameFormat, { EnumName });

		// 블루프린트 클래스는 `_C` 가 붙는다. 이걸 빼면 에셋은 있는데 못 찾는다.
		const FString ObjectPath = FString::Printf(
			TEXT("%s/%s.%s_C"), *PanelClassFolder, *AssetName, *AssetName);

		if (UClass* FoundClass = LoadObject<UClass>(nullptr, *ObjectPath))
		{
			return FoundClass;
		}

		// 아직 안 만든 패널이면 정상이다. 자리표시로 넘어간다.
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[%s] `%s` 탭 패널을 아직 만들지 않았다 (%s). 자리표시를 띄운다."),
			*GetName(), *EnumName, *ObjectPath);
	}

	// 3) 자리표시. 빈 화면보다 "무엇이 들어올 예정인지" 적힌 쪽이 낫다.
	if (!PlaceholderPanelClass.IsNull())
	{
		if (UClass* PlaceholderClass = PlaceholderPanelClass.LoadSynchronous())
		{
			bOutIsPlaceholder = true;
			return PlaceholderClass;
		}
	}

	return nullptr;
}

UFF_LivePanelBase* UFF_LiveHudScreen::EnsurePanel(EFF_LiveTab Tab)
{
	if (TObjectPtr<UFF_LivePanelBase>* Existing = PanelWidgets.Find(Tab))
	{
		if (*Existing)
		{
			return *Existing;
		}
	}

	if (!Panel_PanelHost)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] Panel_PanelHost 가 없어 패널을 넣을 자리가 없다."), *GetName());
		return nullptr;
	}

	FString EnumName;
	FText DisplayName;
	FFLiveHudScreenLocal::DescribeTab(Tab, EnumName, DisplayName);

	bool bIsPlaceholder = false;
	UClass* PanelClass = ResolvePanelClass(Tab, EnumName, bIsPlaceholder);

	if (!PanelClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] `%s` 탭에 띄울 패널이 없다. PlaceholderPanelClass 가 비었는지 확인한다 "
				 "(에디터 콘솔에서 `FF.LiveHudUMG` 를 한 번 실행하면 만들어진다)."),
			*GetName(), *EnumName);
		return nullptr;
	}

	UFF_LivePanelBase* Panel = CreateWidget<UFF_LivePanelBase>(this, PanelClass);
	if (!Panel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] `%s` 탭 패널 생성에 실패했다 (%s)."),
			*GetName(), *EnumName, *PanelClass->GetName());
		return nullptr;
	}

	// 제목과 탭을 함께 넣는다. 따로 두면 제목만 바뀌고 탭이 남는다.
	Panel->InitializePanel(this, Tab, DisplayName);

	if (bIsPlaceholder)
	{
		if (UFF_LivePlaceholderPanel* Placeholder = Cast<UFF_LivePlaceholderPanel>(Panel))
		{
			Placeholder->SetPlaceholderMessage(FText::Format(
				LOCTEXT("LivePanel_Placeholder", "`{0}` 패널은 U6 에서 만듭니다."), DisplayName));
		}
	}

	Panel->SetVisibility(ESlateVisibility::Collapsed);
	Panel_PanelHost->AddChild(Panel);
	PanelWidgets.Add(Tab, Panel);
	return Panel;
}

FFF_LivePanelLayout UFF_LiveHudScreen::ResolveLayout(EFF_LiveTab Tab) const
{
	const FFF_LivePanelLayout* Found = PanelLayouts.Find(Tab);
	FFF_LivePanelLayout Layout = Found ? *Found : DefaultPanelLayout;

	// 폭이 0 이하면 패널이 사라진다. 값을 잘못 넣었을 때 화면이 통째로 없어지지 않게 되돌린다.
	if (Layout.Width <= 0.f)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 패널 폭이 %.1f 라 기본값으로 되돌린다."), *GetName(), Layout.Width);
		Layout.Width = DefaultPanelLayout.Width > 0.f ? DefaultPanelLayout.Width : 520.f;
	}
	return Layout;
}

void UFF_LiveHudScreen::ApplyPanelLayout(const FFF_LivePanelLayout& Layout)
{
	if (!Panel_PanelHost)
	{
		return;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Panel_PanelHost->Slot);
	if (!CanvasSlot)
	{
		// 캔버스가 아니면 위치는 디자이너가 소유한다. 코드가 짐작해서 옮기지 않는다.
		UE_LOG(LogFestaFiestaUI, Verbose,
			TEXT("[%s] Panel_PanelHost 가 CanvasPanel 자식이 아니라 자리를 코드가 정하지 않는다."),
			*GetName());
		return;
	}

	// === CanvasPanelSlot 의 Offsets 는 앵커에 따라 뜻이 달라진다 ===
	// 늘어난 축(min != max)이면 양쪽 **여백**이고, 점 앵커면 **위치와 크기**다.
	// 라이브 패널은 왼쪽 고정이라 세로가 늘어난 축이다 —
	//   Left = X 위치, Top = 위쪽 여백, Right = 폭, Bottom = 아래쪽 여백
	// 아래는 하단 바를 비워 둔다. 안 그러면 패널이 바 뒤로 들어가 탭을 못 누른다.
	CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
	CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
	CanvasSlot->SetOffsets(FMargin(
		PanelMargin + static_cast<float>(Layout.Offset.X),
		PanelMargin,
		Layout.Width,
		BarHeight + PanelMargin));
}

// =============================================================================
// 갱신
// =============================================================================

void UFF_LiveHudScreen::RefreshAll()
{
	RefreshTabs();
	RefreshStatusRow();
	RefreshSpeed();
}

void UFF_LiveHudScreen::RefreshTabs()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Live;

	int32 VisibleTabCount = 0;
	ForEachTabButton([this, Stage, &VisibleTabCount](EFF_LiveTab Tab, UFF_LiveTabButton& Button)
	{
		const bool bVisible = FFF_LiveHudRules::IsTabVisible(Tab, Stage);
		Button.SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button.SetSelected(Selection.IsSelected(Tab));
		VisibleTabCount += bVisible ? 1 : 0;
	});

	// 🔴 탭이 하나도 없으면 메뉴 행째 접는다. 라이브가 아닌 Stage 에 이 화면이 배선돼 있으면
	// **빈 띠**만 남아 고장난 화면처럼 보인다. 상태 행은 그대로 둔다.
	if (Border_MenuRow)
	{
		Border_MenuRow->SetVisibility(VisibleTabCount > 0
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	// 사라진 탭의 패널이 남아 있으면 닫을 수단이 화면에서 없어진다.
	ApplySelection(FFF_LiveHudRules::ClampSelectionToVisible(Selection, Stage));
}

void UFF_LiveHudScreen::RefreshStatusRow()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// ★ 라벨은 `관객 만족도` 다. Live 중에는 FinaleBonus 가 없어 `ScoreSatisfaction` 과
	//   값이 같으므로 같은 숫자를 두 번 띄우지 않는다 (U6 계획).
	SetGroupTextOrHide(Box_Satisfaction, Text_Satisfaction,
		FText::AsNumber(ViewModel->GetSimSatisfaction()));

	SetGroupTextOrHide(Box_Safety, Text_Safety,
		FText::AsNumber(ViewModel->GetSafety()));

	SetGroupTextOrHide(Box_TodayRevenue, Text_TodayRevenue,
		FText::AsNumber(ViewModel->GetTodayRevenue()));

	SetGroupTextOrHide(Box_Gold, Text_Gold,
		FText::AsNumber(ViewModel->GetGold()));

	// 소유자 API 가 없는 칸. **못 읽었으면 숨긴다** — 0 을 띄우면 "관객이 없다"는 틀린 사실이 된다.
	SetGroupTextOrHide(Box_VisitorCount, Text_VisitorCount,
		(VisitorCount >= 0) ? FText::AsNumber(VisitorCount) : FText::GetEmpty());

	// 시계와 남은 시간을 한 줄로 합친다. Live 가 아니면 시계가 비어 있다.
	const FText ClockText = ViewModel->GetLiveClockText();
	const int32 RemainingSeconds = FMath::Max(0, FMath::CeilToInt(ViewModel->GetRemainingSeconds()));

	SetTextOrHide(Text_Clock, ClockText.IsEmpty()
		? FText::GetEmpty()
		: FText::Format(
			LOCTEXT("LiveStatus_Clock", "{0} · 남은 {1}초"),
			ClockText, FText::AsNumber(RemainingSeconds)));

	if (Border_Clock)
	{
		Border_Clock->SetVisibility(ClockText.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

void UFF_LiveHudScreen::RefreshSpeed()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Live;

	const EFF_LiveSpeedState State = FFF_LiveHudRules::ResolveSpeedState(
		Stage,
		ViewModel ? ViewModel->IsTimeScaleLocked() : false,
		ViewModel ? ViewModel->IsFinalDay() : false);

	if (Box_Speed)
	{
		Box_Speed->SetVisibility(State == EFF_LiveSpeedState::Hidden
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	if (State == EFF_LiveSpeedState::Hidden)
	{
		return;
	}

	const bool bCanChange = (State == EFF_LiveSpeedState::Ready);
	for (UButton* Button : { Button_Speed1.Get(), Button_Speed2.Get(), Button_Speed4.Get() })
	{
		if (Button)
		{
			Button->SetIsEnabled(bCanChange);
		}
	}

	// !! 흐려짐만으로는 왜 못 바꾸는지 알 수 없다 !! (AT-011)
	FText Reason;
	switch (State)
	{
	case EFF_LiveSpeedState::LockedMainProgram:
		Reason = LOCTEXT("LiveSpeed_MainProgram", "메인 공연 중에는 1배속으로 고정됩니다. 공연이 끝나면 풀립니다.");
		break;

	case EFF_LiveSpeedState::LockedFinalDay:
		Reason = LOCTEXT("LiveSpeed_FinalDay", "마지막 날에는 배속을 바꿀 수 없습니다.");
		break;

	default:
		break;
	}
	SetTextOrHide(Text_SpeedReason, Reason);
}

// =============================================================================
// 결과 이벤트
// =============================================================================

void UFF_LiveHudScreen::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	// 단계가 바뀌면 탭 가시성·배속 잠금이 함께 바뀐다. 전체를 다시 그린다.
	RefreshAll();
}

void UFF_LiveHudScreen::HandleRemainingSecondsChanged(int32 NewValue)
{
	// 초마다 온다. 상태 행만 다시 쓴다 — 탭까지 돌리면 초당 갱신이 된다.
	RefreshStatusRow();
}

void UFF_LiveHudScreen::HandleGoldChanged(int32 NewValue)
{
	RefreshStatusRow();
}

void UFF_LiveHudScreen::HandleTodayRevenueChanged(int32 NewValue)
{
	RefreshStatusRow();
}

void UFF_LiveHudScreen::HandleSimSatisfactionChanged(int32 NewValue)
{
	RefreshStatusRow();
}

void UFF_LiveHudScreen::HandleSafetyChanged(int32 NewValue)
{
	RefreshStatusRow();
}

// =============================================================================
// 배속
// =============================================================================

void UFF_LiveHudScreen::HandleSpeed1Clicked()
{
	if (UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel())
	{
		// !! 여기서 잠금을 미리 검사하지 않는다 !! 판정의 소유자는 Stage 시스템이다 (UI 규칙 2).
		ViewModel->RequestSetTimeScale(1.f);
	}
}

void UFF_LiveHudScreen::HandleSpeed2Clicked()
{
	if (UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel())
	{
		ViewModel->RequestSetTimeScale(2.f);
	}
}

void UFF_LiveHudScreen::HandleSpeed4Clicked()
{
	if (UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel())
	{
		ViewModel->RequestSetTimeScale(4.f);
	}
}

void UFF_LiveHudScreen::Debug_SetPlaceholders(int32 InVisitorCount)
{
	VisitorCount = InVisitorCount;
	RefreshStatusRow();
}

// =============================================================================
// 표시 헬퍼
// =============================================================================

void UFF_LiveHudScreen::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
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

void UFF_LiveHudScreen::SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value)
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
