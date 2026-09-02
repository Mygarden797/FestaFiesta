// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LiveHudScreen.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/LiveHUD/FF_LiveTabButton.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "UI/Workspace/FF_FloatingPanelStateSubsystem.h"

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

		// 문구 표는 `UFF_UIDisplayTextLibrary` 하나뿐이다. 여기 따로 두면 준비 단계 바와
		// 갈라지고, 갈라진 쪽은 자동 테스트가 못 본다.
		OutDisplayName = UFF_UIDisplayTextLibrary::GetLiveTabText(Tab);
		if (OutDisplayName.IsEmpty())
		{
			OutDisplayName = FText::FromString(OutEnumName);
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
	EnsurePanelWorkspace();

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
	RestoreFloatingPanelState();
	RefreshAll();
}

void UFF_LiveHudScreen::NativeDestruct()
{
	for (const EFF_LiveTab Tab : OpenTabs)
	{
		if (TObjectPtr<UFF_LivePanelBase>* Panel = PanelWidgets.Find(Tab))
		{
			if (*Panel)
			{
				(*Panel)->HandlePanelClosed();
			}
		}
	}
	SaveFloatingPanelState();
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
	CachedViewModel->OnTimeScaleLockChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleTimeScaleLockChanged);

	// 피날레 등급이 바뀌면 **탭이 생기거나 사라진다.** 값만 바뀌는 것이 아니다.
	CachedViewModel->OnPurchasedFinaleGradeChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleFinaleGradeChanged);
	CachedViewModel->OnRemainingSecondsChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleRemainingSecondsChanged);
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleGoldChanged);
	CachedViewModel->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleTodayRevenueChanged);
	CachedViewModel->OnArrearsChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleArrearsChanged);

	// 🔴 전체 인원과 가용 인원 **둘 다** 받는다. 프로그램 편성은 전체를 바꾸지 않고,
	// 알바 고용은 편성을 바꾸지 않는다 — 하나만 받으면 한쪽 변화를 놓친다.
	CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleStaffCountChanged);
	CachedViewModel->OnAvailableStaffCountChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleStaffCountChanged);
}

void UFF_LiveHudScreen::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleStageViewChanged);
		CachedViewModel->OnTimeScaleLockChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleTimeScaleLockChanged);
		CachedViewModel->OnPurchasedFinaleGradeChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleFinaleGradeChanged);
		CachedViewModel->OnRemainingSecondsChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleRemainingSecondsChanged);
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleGoldChanged);
		CachedViewModel->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleTodayRevenueChanged);
		CachedViewModel->OnArrearsChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleArrearsChanged);
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleStaffCountChanged);
		CachedViewModel->OnAvailableStaffCountChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleStaffCountChanged);
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
		{ EFF_LiveTab::Staff,		Tab_Staff },
		{ EFF_LiveTab::Report,		Tab_Report },
		{ EFF_LiveTab::Finale,		Tab_Finale }
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

	if (!FFF_LiveHudRules::IsTabVisible(Tab, Stage, HasPurchasedFinale()))
	{
		FString EnumName;
		FText DisplayName;
		FFLiveHudScreenLocal::DescribeTab(Tab, EnumName, DisplayName);

		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 지금 단계에서 볼 수 없는 탭(%s)을 열려고 했다. 무시한다."),
			*GetName(), *EnumName);
		return;
	}

	if (IsPanelOpen(Tab))
	{
		BringPanelToFront(Tab);
		return;
	}

	OpenPanel(Tab);
}

void UFF_LiveHudScreen::CloseCurrentPanel()
{
	if (Selection.bHasSelection)
	{
		ClosePanel(Selection.Tab);
	}
}

void UFF_LiveHudScreen::ClosePanel(EFF_LiveTab Tab)
{
	if (!OpenTabs.RemoveSingle(Tab))
	{
		return;
	}

	if (TObjectPtr<UFF_LivePanelBase>* Panel = PanelWidgets.Find(Tab))
	{
		if (*Panel)
		{
			(*Panel)->SetVisibility(ESlateVisibility::Collapsed);
			(*Panel)->HandlePanelClosed();
		}
	}

	if (Selection.IsSelected(Tab))
	{
		Selection = OpenTabs.IsEmpty()
			? FFF_LiveTabSelection::None()
			: FFF_LiveTabSelection::Of(OpenTabs.Last());
	}

	HidePanelLimitNotice();
	RefreshPanelZOrder();
	RefreshPanelHostVisibility();
	RefreshPanelSelectionVisuals();
	OnTabSelectionChanged.Broadcast(Selection);
	SaveFloatingPanelState();
}

bool UFF_LiveHudScreen::IsPanelOpen(EFF_LiveTab Tab) const
{
	return OpenTabs.Contains(Tab);
}

bool UFF_LiveHudScreen::OpenPanel(EFF_LiveTab Tab)
{
	if (IsPanelOpen(Tab))
	{
		BringPanelToFront(Tab);
		return true;
	}

	if (OpenTabs.Num() >= UFF_FloatingPanelStateSubsystem::MaxOpenPanelCount)
	{
		ShowPanelLimitNotice();
		return false;
	}

	UFF_LivePanelBase* Panel = EnsurePanel(Tab);
	if (!Panel)
	{
		return false;
	}

	OpenTabs.Add(Tab);
	Selection = FFF_LiveTabSelection::Of(Tab);
	ApplyPanelLayout(Tab, ResolveLayout(Tab));
	Panel->ApplyPinnedState(PinnedTabs.Contains(Tab));
	Panel->SetVisibility(ESlateVisibility::Visible);
	Panel->HandlePanelOpened();
	LogPanelGeometryNextTick(*Panel);

	HidePanelLimitNotice();
	RefreshPanelZOrder();
	RefreshPanelHostVisibility();
	RefreshPanelSelectionVisuals();
	OnTabSelectionChanged.Broadcast(Selection);
	SaveFloatingPanelState();
	return true;
}

void UFF_LiveHudScreen::BringPanelToFront(EFF_LiveTab Tab)
{
	if (!OpenTabs.RemoveSingle(Tab))
	{
		return;
	}

	OpenTabs.Add(Tab);
	const FFF_LiveTabSelection Previous = Selection;
	Selection = FFF_LiveTabSelection::Of(Tab);
	RefreshPanelZOrder();
	RefreshPanelSelectionVisuals();
	if (Selection != Previous)
	{
		OnTabSelectionChanged.Broadcast(Selection);
	}
	SaveFloatingPanelState();
}

void UFF_LiveHudScreen::SetPanelPinned(EFF_LiveTab Tab, bool bPinned)
{
	if (bPinned)
	{
		PinnedTabs.Add(Tab);
	}
	else
	{
		PinnedTabs.Remove(Tab);
	}

	RefreshPanelZOrder();
	SaveFloatingPanelState();
}

void UFF_LiveHudScreen::RefreshPanelSelectionVisuals()
{
	ForEachTabButton([this](EFF_LiveTab Tab, UFF_LiveTabButton& Button)
	{
		Button.SetSelected(IsPanelOpen(Tab));
	});
}

// =============================================================================
// 패널
// =============================================================================

void UFF_LiveHudScreen::EnsurePanelWorkspace()
{
	if (PanelWorkspace || !Panel_PanelHost || !WidgetTree)
	{
		return;
	}

	if (UCanvasPanelSlot* HostCanvasSlot = Cast<UCanvasPanelSlot>(Panel_PanelHost->Slot))
	{
		HostCanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		HostCanvasSlot->SetAlignment(FVector2D::ZeroVector);
		HostCanvasSlot->SetOffsets(FMargin(0.f));
	}

	PanelWorkspace = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("Canvas_FloatingPanelWorkspace"));
	if (!PanelWorkspace)
	{
		return;
	}

	UPanelSlot* WorkspaceSlot = Panel_PanelHost->AddChild(PanelWorkspace);
	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(WorkspaceSlot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}
	else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(WorkspaceSlot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
	}

	PanelLimitNotice = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_PanelLimitNotice"));
	UTextBlock* NoticeText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_PanelLimitNotice"));
	if (PanelLimitNotice && NoticeText)
	{
		NoticeText->SetText(LOCTEXT(
			"LivePanelLimitNotice",
			"패널은 최대 5개까지 열 수 있습니다. 하나를 닫고 다시 시도하세요."));
		NoticeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		NoticeText->SetJustification(ETextJustify::Center);
		NoticeText->SetShadowOffset(FVector2D(1.f, 1.f));
		NoticeText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		FSlateFontInfo Font = NoticeText->GetFont();
		Font.Size = 14;
		NoticeText->SetFont(Font);

		FSlateBrush NoticeBrush;
		NoticeBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		NoticeBrush.TintColor = FSlateColor(FLinearColor(0.06f, 0.12f, 0.14f, 0.96f));
		NoticeBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		NoticeBrush.OutlineSettings.CornerRadii = FVector4(6.f, 6.f, 6.f, 6.f);
		PanelLimitNotice->SetBrush(NoticeBrush);
		PanelLimitNotice->SetPadding(FMargin(16.f, 9.f));
		PanelLimitNotice->SetContent(NoticeText);
		PanelLimitNotice->SetVisibility(ESlateVisibility::Collapsed);

		if (UCanvasPanelSlot* NoticeSlot = PanelWorkspace->AddChildToCanvas(PanelLimitNotice))
		{
			NoticeSlot->SetAnchors(FAnchors(0.5f, 0.f));
			NoticeSlot->SetAlignment(FVector2D(0.5f, 0.f));
			NoticeSlot->SetPosition(FVector2D(0.f, 24.f));
			NoticeSlot->SetAutoSize(true);
			NoticeSlot->SetZOrder(10000);
		}
	}
}

void UFF_LiveHudScreen::ShowPanelLimitNotice()
{
	EnsurePanelWorkspace();
	if (PanelLimitNotice)
	{
		PanelLimitNotice->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	RefreshPanelHostVisibility();
}

void UFF_LiveHudScreen::HidePanelLimitNotice()
{
	if (PanelLimitNotice)
	{
		PanelLimitNotice->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFF_LiveHudScreen::RefreshPanelHostVisibility()
{
	if (!Panel_PanelHost)
	{
		return;
	}

	const bool bShowNotice = PanelLimitNotice
		&& PanelLimitNotice->GetVisibility() != ESlateVisibility::Collapsed;
	Panel_PanelHost->SetVisibility(!OpenTabs.IsEmpty() || bShowNotice
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}

void UFF_LiveHudScreen::RefreshPanelZOrder()
{
	int32 NormalZ = 1;
	int32 PinnedZ = 100;
	for (const EFF_LiveTab Tab : OpenTabs)
	{
		const TObjectPtr<UFF_LivePanelBase>* Found = PanelWidgets.Find(Tab);
		UCanvasPanelSlot* PanelCanvasSlot = Found && *Found
			? Cast<UCanvasPanelSlot>((*Found)->Slot)
			: nullptr;
		if (PanelCanvasSlot)
		{
			PanelCanvasSlot->SetZOrder(PinnedTabs.Contains(Tab) ? PinnedZ++ : NormalZ++);
		}
	}

	if (PanelLimitNotice)
	{
		if (UCanvasPanelSlot* NoticeSlot = Cast<UCanvasPanelSlot>(PanelLimitNotice->Slot))
		{
			NoticeSlot->SetZOrder(10000);
		}
	}
}

void UFF_LiveHudScreen::RestoreFloatingPanelState()
{
	UFF_FloatingPanelStateSubsystem* State = UFF_FloatingPanelStateSubsystem::Get(this);
	if (!State)
	{
		return;
	}

	const FFF_LiveFloatingPanelSnapshot Snapshot = State->GetLiveSnapshot();
	for (const TPair<EFF_LiveTab, FFF_LivePanelLayout>& Pair : Snapshot.RuntimeLayouts)
	{
		PanelLayouts.Add(Pair.Key, Pair.Value);
	}

	PinnedTabs = Snapshot.PinnedTabs;
	OpenTabs.Reset();
	Selection = FFF_LiveTabSelection::None();

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel
		? ViewModel->GetCurrentStage()
		: EFF_FestivalStage::Live;
	for (const EFF_LiveTab Tab : Snapshot.OpenTabs)
	{
		if (OpenTabs.Num() >= UFF_FloatingPanelStateSubsystem::MaxOpenPanelCount)
		{
			break;
		}
		if (FFF_LiveHudRules::IsTabVisible(Tab, Stage, HasPurchasedFinale()))
		{
			OpenPanel(Tab);
		}
	}

	if (Snapshot.FocusedSelection.bHasSelection
		&& IsPanelOpen(Snapshot.FocusedSelection.Tab))
	{
		BringPanelToFront(Snapshot.FocusedSelection.Tab);
	}

	RefreshPanelHostVisibility();
	SaveFloatingPanelState();
}

void UFF_LiveHudScreen::SaveFloatingPanelState() const
{
	if (UFF_FloatingPanelStateSubsystem* State = UFF_FloatingPanelStateSubsystem::Get(this))
	{
		FFF_LiveFloatingPanelSnapshot Snapshot;
		Snapshot.OpenTabs = OpenTabs;
		Snapshot.PinnedTabs = PinnedTabs;
		Snapshot.RuntimeLayouts = PanelLayouts;
		Snapshot.FocusedSelection = Selection;
		State->SaveLive(Snapshot);
	}
}

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

	EnsurePanelWorkspace();
	if (!PanelWorkspace)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 플로팅 패널 작업공간을 만들지 못했다."), *GetName());
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

	if (UCanvasPanelSlot* PanelSlot = PanelWorkspace->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAutoSize(false);
		PanelSlot->SetZOrder(1);
	}

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("[%s] `%s` 패널을 만들었다 (%s%s). 호스트=%s"),
		*GetName(), *EnumName, *PanelClass->GetName(),
		bIsPlaceholder ? TEXT(" · 자리표시") : TEXT(""),
		*PanelWorkspace->GetClass()->GetName());

	PanelWidgets.Add(Tab, Panel);
	return Panel;
}

void UFF_LiveHudScreen::LogPanelGeometryNextTick(UFF_LivePanelBase& Panel)
{
	// === 왜 다음 틱인가 ===
	// 방금 붙인 위젯의 `CachedGeometry` 는 아직 이전 프레임 값(보통 0×0)이다. 지금 찍으면
	// 항상 `0x0` 이 나와서 "패널이 안 보인다" 의 원인을 못 가른다.
	//
	// 🔴 이 로그는 **크기 문제를 눈이 아니라 숫자로 가르기 위한 것**이다.
	//   - 패널이 `520 x 8xx` 인데 안 보인다 → 색·폰트 문제
	//   - 패널이 `30 x 30` 같이 작다      → 슬롯 정렬·부모 크기 문제
	//   - 호스트가 `0 x 0`               → 캔버스 앵커·여백 문제
	// 셋의 대처가 완전히 달라서 추측으로 고치면 매번 한 번씩 헛짚는다 (2026-08-28).
	TWeakObjectPtr<UFF_LiveHudScreen> WeakThis(this);
	TWeakObjectPtr<UFF_LivePanelBase> WeakPanel(&Panel);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, WeakPanel]()
		{
			const UFF_LiveHudScreen* Screen = WeakThis.Get();
			const UFF_LivePanelBase* Panel = WeakPanel.Get();
			if (!Screen || !Panel)
			{
				return;
			}

			const FVector2D PanelSize = Panel->GetCachedGeometry().GetLocalSize();
			const FVector2D HostSize = Screen->Panel_PanelHost
				? Screen->Panel_PanelHost->GetCachedGeometry().GetLocalSize()
				: FVector2D::ZeroVector;

			// 🔴 `화면` 크기가 실제 해상도보다 훨씬 작으면 HUD 루트가 화면을 못 채우고 있는 것이다.
			//    패널은 그 값에 의존하지 않게 고쳤지만, 원인 자체는 아직 남아 있다.
			const FVector2D ScreenSize = Screen->GetCachedGeometry().GetLocalSize();

			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("[%s] 패널 실측 — 패널 %.0fx%.0f · 호스트 %.0fx%.0f · 화면 %.0fx%.0f · "
					 "패널가시성=%d · 호스트가시성=%d"),
				*Screen->GetName(),
				PanelSize.X, PanelSize.Y, HostSize.X, HostSize.Y, ScreenSize.X, ScreenSize.Y,
				static_cast<int32>(Panel->GetVisibility()),
				Screen->Panel_PanelHost ? static_cast<int32>(Screen->Panel_PanelHost->GetVisibility()) : -1);
		}));
	}
}

bool UFF_LiveHudScreen::IsPanelDragAllowed(EFF_LiveTab Tab) const
{
	if (!bAllowPanelDrag || !PanelWorkspace || !IsPanelOpen(Tab) || PinnedTabs.Contains(Tab))
	{
		return false;
	}

	// `Designer` 는 코드가 자리를 안 잡는다. 끌게 두면 다음에 열 때 되돌아가서 헛수고가 된다.
	return ResolveLayout(Tab).Dock != EFF_LivePanelDock::Designer;
}

void UFF_LiveHudScreen::NudgePanelPosition(EFF_LiveTab Tab, FVector2D SlateDelta)
{
	if (!IsPanelDragAllowed(Tab) || SlateDelta.IsNearlyZero())
	{
		return;
	}

	// 🔴 **표를 고쳐서 옮긴다.** 슬롯을 직접 만지면 탭을 닫았다 열 때 `ApplyPanelLayout` 이
	// 표의 값으로 덮어써서 옮긴 자리가 사라진다. 준비 단계에서 같은 판단을 했다.
	//
	// 네 방향 모두 두 축이 점 앵커라 `Offset` 의 X·Y 가 그대로 화면 이동량이 된다 —
	// 늘어난 축이 섞여 있던 준비 단계와 달리 버릴 성분이 없다.
	FFF_LivePanelLayout Layout = ResolveLayout(Tab);

	// 패널별 Canvas 슬롯은 모든 도킹에서 같은 화면 축을 사용한다.
	Layout.Offset += SlateDelta;

	PanelLayouts.Add(Tab, Layout);
	ApplyPanelLayout(Tab, Layout);
	SaveFloatingPanelState();
}

FFF_LivePanelLayout UFF_LiveHudScreen::ResolveLayout(EFF_LiveTab Tab) const
{
	const FFF_LivePanelLayout* Found = PanelLayouts.Find(Tab);
	FFF_LivePanelLayout Layout = Found ? *Found : DefaultPanelLayout;

	// 크기가 0 이하면 패널이 사라진다. 값을 잘못 넣었을 때 화면이 통째로 없어지지 않게 되돌린다.
	if (Layout.Width <= 0.f)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 패널 폭이 %.1f 라 기본값으로 되돌린다."), *GetName(), Layout.Width);
		Layout.Width = DefaultPanelLayout.Width > 0.f ? DefaultPanelLayout.Width : 520.f;
	}

	if (Layout.Height <= 0.f)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 패널 높이가 %.1f 라 기본값으로 되돌린다."), *GetName(), Layout.Height);
		Layout.Height = DefaultPanelLayout.Height > 0.f ? DefaultPanelLayout.Height : 620.f;
	}
	return Layout;
}

void UFF_LiveHudScreen::ApplyPanelLayout(EFF_LiveTab Tab, const FFF_LivePanelLayout& Layout)
{
	const TObjectPtr<UFF_LivePanelBase>* FoundPanel = PanelWidgets.Find(Tab);
	if (!FoundPanel || !*FoundPanel)
	{
		return;
	}

	if (Layout.Dock == EFF_LivePanelDock::Designer)
	{
		return;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>((*FoundPanel)->Slot);
	if (!CanvasSlot)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 패널별 Canvas 슬롯이 없어 `%d` 패널 위치를 적용하지 못했다."),
			*GetName(), static_cast<int32>(Tab));
		return;
	}

	// === CanvasPanelSlot 의 Offsets 는 앵커에 따라 뜻이 달라진다 ===
	// 늘어난 축(min != max)이면 양쪽 **여백**이고, 점 앵커면 **위치와 크기**다.
	//
	// 🔴 **어느 방향이든 두 축 다 점 앵커로 둔다** (2026-08-28 실측 후 변경).
	// 전에는 세로를 늘어난 축으로 두고 위아래 여백만 줬는데, 그러면 높이를 **부모가**
	// 정한다. 그런데 HUD 루트가 화면 높이를 못 받고 있어서 높이가 `-12` 로 나왔고
	// 화면에는 닫기 버튼만 한 흰 상자가 떴다. 준비 단계 패널이 멀쩡한 것은 실제로 쓰는
	// 도킹이 전부 점 앵커 + 명시 크기라 부모 높이를 안 보기 때문이다.
	//
	// 🔴 세로 기준점은 **상태 행 위**다. 바 전체를 비우면 패널과 탭 버튼 사이에 틈이 생기는데,
	// 준비 단계는 패널 아래 끝이 탭 줄 밑으로 들어가서 버튼이 패널에 얹힌 것처럼 보인다.
	// 바가 패널보다 뒤에 추가되어 있어 탭이 위에 그려지고, 가리는 자리는 패널의 빈 아래쪽이다.
	//   Left = X 위치, Top = Y 위치(음수 = 위로), Right = 폭, Bottom = 높이
	const float PanelTop = -StatusRowHeight + static_cast<float>(Layout.Offset.Y);

	switch (Layout.Dock)
	{
	case EFF_LivePanelDock::Center:
		// 가로 가운데. 준비 단계에서 탭을 눌렀을 때와 같은 자리다.
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
		CanvasSlot->SetOffsets(FMargin(
			static_cast<float>(Layout.Offset.X), PanelTop, Layout.Width, Layout.Height));
		break;

	case EFF_LivePanelDock::Left:
		CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(
			PanelMargin + static_cast<float>(Layout.Offset.X), PanelTop, Layout.Width, Layout.Height));
		break;

	case EFF_LivePanelDock::Right:
		// 오른쪽 정렬이라 X 는 음수 방향으로 민다.
		CanvasSlot->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(
			-PanelMargin + static_cast<float>(Layout.Offset.X), PanelTop, Layout.Width, Layout.Height));
		break;

	case EFF_LivePanelDock::Designer:
		// 손으로 잡은 자리를 그대로 쓴다. 여기서 손대면 디자이너 작업이 매번 사라진다.
		break;
	}
}

// =============================================================================
// 갱신
// =============================================================================

void UFF_LiveHudScreen::RefreshAll()
{
	RefreshTabs();
	RefreshStatusRow();
	RefreshSpeed();
	RefreshCta();
}

void UFF_LiveHudScreen::RefreshTabs()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Live;
	const bool bPurchasedFinale = HasPurchasedFinale();

	int32 VisibleTabCount = 0;
	ForEachTabButton([this, Stage, bPurchasedFinale, &VisibleTabCount](EFF_LiveTab Tab, UFF_LiveTabButton& Button)
	{
		const bool bVisible = FFF_LiveHudRules::IsTabVisible(Tab, Stage, bPurchasedFinale);
		Button.SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button.SetSelected(bVisible && IsPanelOpen(Tab));
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

	// Closing/Report로 넘어가면 UIFlow가 이 HUD를 제거한다. 명시적인 닫기가 아니므로
	// 작업공간 상태는 유지하고 이전 화면의 호스트만 숨긴다.
	if (!FFF_LiveHudRules::IsLiveHudStage(Stage))
	{
		if (Panel_PanelHost)
		{
			Panel_PanelHost->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const TArray<EFF_LiveTab> OpenTabsCopy = OpenTabs;
	for (const EFF_LiveTab OpenTab : OpenTabsCopy)
	{
		if (!FFF_LiveHudRules::IsTabVisible(OpenTab, Stage, HasPurchasedFinale()))
		{
			ClosePanel(OpenTab);
		}
	}
	RefreshPanelSelectionVisuals();
}

void UFF_LiveHudScreen::RefreshStatusRow()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// --- 왼쪽: 축제 → 금화 → 미납 → 오늘 매출 → 인력 → 관객 ---
	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	SetGroupTextOrHide(Box_FestivalName, Text_FestivalName,
		Flow ? Flow->GetActiveFestivalDisplayName() : FText::GetEmpty());

	SetGroupTextOrHide(Box_Gold, Text_Gold,
		FText::AsNumber(ViewModel->GetGold()));

	// 미납은 0 이면 숨긴다. `미납 0` 은 없는 문제를 있는 것처럼 만든다.
	const int32 Arrears = ViewModel->GetArrears();
	SetGroupTextOrHide(Box_Arrears, Text_Arrears, (Arrears > 0)
		? FText::Format(LOCTEXT("LiveStatus_Arrears", "미납 {0}"), FText::AsNumber(Arrears))
		: FText::GetEmpty());

	SetGroupTextOrHide(Box_TodayRevenue, Text_TodayRevenue,
		FText::AsNumber(ViewModel->GetTodayRevenue()));

	// 🔴 전체만 띄우지 않는다. 프로그램에 묶인 인원은 Event 대응에 못 쓴다 —
	// 라이브에서는 그 구분이 준비 단계보다 더 중요하다.
	const int32 TotalStaff = ViewModel->GetTotalStaffCount();
	const int32 AvailableStaff = ViewModel->GetAvailableStaffCount();
	SetGroupTextOrHide(Box_Staff, Text_Staff, (TotalStaff >= 0 && AvailableStaff >= 0)
		? FText::Format(
			LOCTEXT("LiveStatus_Staff", "{0}명 중 {1}명 사용 가능"),
			FText::AsNumber(TotalStaff), FText::AsNumber(AvailableStaff))
		: FText::GetEmpty());

	// 🔴 이 값은 **동시 인원**이다 (지금 축제장 안에 있는 사람 수).
	// 일일 목표(오늘 누적 500명)의 분자로 쓰지 않는다 — 운영 패널의 목표 진행이 여전히
	// `모름` 인 것은 그래서다. 누적 방문객 API 를 `UI_API.md` 에 요청해 뒀다.
	//
	// **못 읽었으면 숨긴다** — 0 을 띄우면 "관객이 없다"는 틀린 사실이 된다.
	const int32 LiveVisitorCount = ViewModel->GetConcurrentVisitorCount();

	// 디버그로 넣은 값이 있으면 그것을 우선한다. 실값만 쓰게 되면 이 분기와
	// `Debug_SetPlaceholders` 를 함께 지운다.
	const int32 ShownVisitorCount = (VisitorCount >= 0) ? VisitorCount : LiveVisitorCount;

	SetGroupTextOrHide(Box_VisitorCount, Text_VisitorCount,
		(ShownVisitorCount >= 0) ? FText::AsNumber(ShownVisitorCount) : FText::GetEmpty());

	// --- 가운데: `1일차 · 마감` ---
	//
	// 🔴 단계 이름을 **박아 두면 안 된다.** 이 바가 라이브 전용이던 시절에는 `라이브` 를
	// 박아 뒀는데, 2026-09-02 에 마감·리포트·피날레까지 이 바 하나로 합치면서 마감 화면에도
	// `1일차 · 라이브` 가 그대로 떴다. 칩이 단계를 알려 주는 유일한 표시인데 그게 틀렸으니
	// **화면이 거짓말을 한 셈이다** (2026-09-03 백종태 발견).
	//
	// 문구는 `UFF_UIDisplayTextLibrary` 하나가 소유한다. ViewModel 의
	// `GetCurrentStageDisplayText()` 도 지금은 같은 표를 거치지만, 거기에는 표에 없는 값을
	// 위한 대체 경로가 붙어 있다. 칩은 표에 있는 값만 쓰므로 표를 직접 읽는다.
	SetTextOrHide(Text_DayStage, FText::Format(
		LOCTEXT("LiveStatus_DayStage", "{0}일차 · {1}"),
		FText::AsNumber(ViewModel->GetCurrentDay()),
		UFF_UIDisplayTextLibrary::GetFestivalStageText(ViewModel->GetCurrentStage())));

	if (Border_DayStage)
	{
		Border_DayStage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// --- 오른쪽: 시계. 배속 버튼 왼쪽에 온다 ---
	const FText ClockText = ViewModel->GetLiveClockText();
	const int32 RemainingSeconds = FMath::Max(0, FMath::CeilToInt(ViewModel->GetRemainingSeconds()));

	SetGroupTextOrHide(Box_Clock, Text_Clock, ClockText.IsEmpty()
		? FText::GetEmpty()
		: FText::Format(
			LOCTEXT("LiveStatus_Clock", "{0} · 남은 {1}초"),
			ClockText, FText::AsNumber(RemainingSeconds)));
}

void UFF_LiveHudScreen::RefreshSpeed()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Live;

	const EFF_LiveSpeedState State = FFF_LiveHudRules::ResolveSpeedState(
		Stage,
		ViewModel ? ViewModel->IsTimeScaleLocked() : false);

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

	default:
		break;
	}
	SetTextOrHide(Text_SpeedReason, Reason);
}

// =============================================================================
// 결과 이벤트
// =============================================================================

void UFF_LiveHudScreen::HandleTimeScaleLockChanged(bool bNewLocked)
{
	// 인자를 쓰지 않는다. 잠금 상태는 `IsTimeScaleLocked()` 를 다시 읽는 쪽이 정확하다 —
	// 이 신호는 "지금 다시 읽어라" 는 뜻이다.
	RefreshSpeed();
}

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

bool UFF_LiveHudScreen::HasPurchasedFinale() const
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	return ViewModel && ViewModel->GetPurchasedFinaleGrade() != EFF_FinaleGrade::None;
}

void UFF_LiveHudScreen::HandleFinaleGradeChanged(EFF_FinaleGrade NewGrade)
{
	RefreshTabs();
}

EFF_LiveCtaState UFF_LiveHudScreen::GetCtaState() const
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Live;

	return FFF_LiveHudRules::ResolveCtaState(
		Stage,
		ViewModel && ViewModel->IsFinalDay(),
		ViewModel && ViewModel->CanSkipCurrentStage());
}

FText UFF_LiveHudScreen::GetCtaLabelText() const
{
	switch (GetCtaState())
	{
	// 피날레는 날짜를 넘기는 것이 아니라 연출을 건너뛰는 것이다.
	case EFF_LiveCtaState::ReadyFinaleSkip:
		return LOCTEXT("LiveCta_FinaleSkip", "다음으로");

	// 마지막 날에는 넘어갈 다음 날이 아니라 **축제가 끝난다.**
	case EFF_LiveCtaState::ReadyFinishFestival:
		return LOCTEXT("LiveCta_Finish", "축제 마치기");

	default:
		return LOCTEXT("LiveCta_NextDay", "다음 날로");
	}
}

FText UFF_LiveHudScreen::GetCtaReasonText() const
{
	// !! 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011)
	switch (GetCtaState())
	{
	case EFF_LiveCtaState::BlockedNotReport:
		return LOCTEXT("LiveCta_NotReport",
			"결과를 먼저 확인하세요. 남은 마감 시간이 지나면 리포트로 넘어갑니다.");

	case EFF_LiveCtaState::ReadyFinishFestival:
		// 되돌릴 수 없는 마지막 버튼이라 무엇이 일어나는지 미리 적는다.
		return LOCTEXT("LiveCta_FinishNote", "누르면 축제가 끝나고 나라 지도로 돌아갑니다.");

	default:
		return FText::GetEmpty();
	}
}

void UFF_LiveHudScreen::RequestAdvance()
{
	HandleCtaClicked();
}

void UFF_LiveHudScreen::HandleCtaClicked()
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	const EFF_LiveCtaState State = GetCtaState();

	// 🔴 셋이 **서로 다른 함수**다. 하나로 묶으면 조용히 안 먹는다 —
	// `AdvanceToNextDay()` 는 리포트 단계 밖에서, 그리고 마지막 날에 거절된다.
	switch (State)
	{
	case EFF_LiveCtaState::ReadyFinaleSkip:
		ViewModel->RequestSkipCurrentStage();
		return;

	// 🔴 마감에서 눌렀으면 **두 걸음**이다. `AdvanceToNextDay()` 는 리포트 단계 밖에서
	//    거절되므로, 마감을 먼저 건너뛰어 리포트로 보낸 다음 날짜를 넘긴다.
	//    `RequestTransition` 이 동기라서 한 번의 클릭으로 둘 다 끝난다.
	//
	//    합쳐서 한 번에 부르지 않는 이유: 둘은 **서로 다른 요청**이고 앞의 것이 거절되면
	//    (마감이 `bSkippable` 이 아니면) 뒤의 것도 조용히 무시되어야 한다. 그래서
	//    성공을 가정하지 않고 단계를 다시 읽어 확인한다.
	case EFF_LiveCtaState::ReadyNextDay:
		if (ViewModel->GetCurrentStage() != EFF_FestivalStage::Report)
		{
			ViewModel->RequestSkipCurrentStage();

			if (ViewModel->GetCurrentStage() != EFF_FestivalStage::Report)
			{
				// 저쪽이 거절했다. 조용히 넘어가지 않고 남긴다 — 눌러도 안 되는 버튼의 원인이다.
				UE_LOG(LogFestaFiestaUI, Warning,
					TEXT("[%s] 마감 건너뛰기가 거절됐다. DaySequence 의 마감 행 `bSkippable` 을 확인한다."),
					*GetName());
				return;
			}
		}

		ViewModel->RequestAdvanceToNextDay();
		return;

	case EFF_LiveCtaState::ReadyFinishFestival:
		// 나라 지도 복귀는 `UFF_UIFlowSubsystem` 이 소유한다 — UI 가 레벨을 직접 열지 않는다.
		if (UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this))
		{
			Flow->ReturnToAtlas();
		}
		else
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] UI 흐름 시스템이 없어 나라 지도로 돌아갈 수 없다."), *GetName());
		}
		return;

	// `Hidden`·`BlockedNotReport` 는 버튼이 없거나 꺼져 있는 상태다. 여기까지 왔다면
	// 화면 상태와 버튼이 어긋난 것이므로 넘기지 않고 남긴다.
	default:
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 넘길 수 없는 상태(%d)에서 버튼이 눌렸다."), *GetName(), static_cast<int32>(State));
		return;
	}
}

void UFF_LiveHudScreen::RefreshCta()
{
	const EFF_LiveCtaState CtaState = GetCtaState();

	// 🔴 바의 버튼은 **피날레에서만** 띄운다. 마감·리포트에서는 리포트 패널 안에 같은
	// 버튼이 있어서, 바에도 두면 화면에 `다음 날로` 가 둘이 된다.
	const bool bShowBarCta = (CtaState == EFF_LiveCtaState::ReadyFinaleSkip);

	if (Button_Cta)
	{
		Button_Cta->SetVisibility(bShowBarCta
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
		Button_Cta->SetIsEnabled(bShowBarCta);
	}
	SetTextOrHide(Text_CtaLabel, bShowBarCta ? GetCtaLabelText() : FText::GetEmpty());
}

void UFF_LiveHudScreen::HandleArrearsChanged(int32 NewValue)
{
	RefreshStatusRow();
}

void UFF_LiveHudScreen::HandleStaffCountChanged(int32 NewValue)
{
	// !! 인자를 쓰지 않는다 !! 전체 인원과 가용 인원 두 델리게이트가 함께 부른다.
	// 인자를 믿으면 가용 인원이 울렸을 때 그 숫자가 전체 자리에 들어간다.
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
