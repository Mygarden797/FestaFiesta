// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LiveHudScreen.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "UI/FF_UILog.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "UI/LiveHUD/Incident/FF_IncidentDetailPanel.h"
#include "UI/LiveHUD/Incident/FF_IncidentMarkerLayer.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
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

	// 🔴 **이 배선을 빠뜨렸었다.** 버튼은 보이고 눌리는데 아무 일도 안 났다 —
	//    가시성과 활성만 맞춰 두고 `OnClicked` 를 안 붙였다 (2026-09-03 백종태 실기 확인).
	//    피날레는 이 버튼이 넘어갈 **유일한** 수단이라 게임이 거기서 멈췄다.
	if (Button_Cta)
	{
		Button_Cta->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleCtaClicked);
	}

}

// 이 화면의 루트는 하단 바 높이만 받으므로 아이콘 레이어는 뷰포트에 직접 올린다.
void UFF_LiveHudScreen::EnsureIncidentMarkerLayer()
{
	if (!IncidentMarkerLayer)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			IncidentMarkerLayer = CreateWidget<UFF_IncidentMarkerLayer>(PC, UFF_IncidentMarkerLayer::StaticClass());
		}
		else if (UWorld* World = GetWorld())
		{
			IncidentMarkerLayer = CreateWidget<UFF_IncidentMarkerLayer>(World, UFF_IncidentMarkerLayer::StaticClass());
		}
	}

	if (IncidentMarkerLayer && !IncidentMarkerLayer->IsInViewport())
	{
		IncidentMarkerLayer->AddToViewport(-10);
	}

	if (IncidentMarkerLayer)
	{
		IncidentMarkerLayer->OnIncidentMarkerClicked.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleIncidentMarkerClicked);
	}
}

void UFF_LiveHudScreen::EnsureIncidentDetailPanel()
{
	if (!IncidentDetailPanel)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			IncidentDetailPanel = CreateWidget<UFF_IncidentDetailPanel>(PC, UFF_IncidentDetailPanel::StaticClass());
		}
		else if (UWorld* World = GetWorld())
		{
			IncidentDetailPanel = CreateWidget<UFF_IncidentDetailPanel>(World, UFF_IncidentDetailPanel::StaticClass());
		}
	}

	if (IncidentDetailPanel && !IncidentDetailPanel->IsInViewport())
	{
		IncidentDetailPanel->AddToViewport(10);
	}

	if (IncidentDetailPanel)
	{
		IncidentDetailPanel->OnDispatchRequested.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleIncidentDispatchRequested);
	}
}

void UFF_LiveHudScreen::HandleIncidentMarkerClicked(FFF_IncidentHandle Handle)
{
	EnsureIncidentDetailPanel();
	if (IncidentDetailPanel)
	{
		IncidentDetailPanel->OpenFor(Handle);
	}
}

void UFF_LiveHudScreen::HandleIncidentDispatchRequested(FFF_IncidentHandle Handle, int32 OptionIndex, int32 StaffCount)
{
	UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 사건 H=%d/%d Option=%d Staff=%d 요청 — 배치 관리자 전이라 즉시 해결"),
		*GetName(), Handle.Index, Handle.Serial, OptionIndex, StaffCount);

	const UWorld* World = GetWorld();
	if (UFF_IncidentSubsystem* Incident = World ? World->GetSubsystem<UFF_IncidentSubsystem>() : nullptr)
	{
		Incident->ResolveIncident(Handle, OptionIndex);
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
	EnsureIncidentMarkerLayer();
	EnsureIncidentDetailPanel();
}

void UFF_LiveHudScreen::NativeDestruct()
{
	UnbindViewModel();
	if (IncidentMarkerLayer)
	{
		IncidentMarkerLayer->OnIncidentMarkerClicked.RemoveDynamic(this, &UFF_LiveHudScreen::HandleIncidentMarkerClicked);
		IncidentMarkerLayer->RemoveFromParent();
	}
	if (IncidentDetailPanel)
	{
		IncidentDetailPanel->OnDispatchRequested.RemoveDynamic(this, &UFF_LiveHudScreen::HandleIncidentDispatchRequested);
		IncidentDetailPanel->Close();
		IncidentDetailPanel->RemoveFromParent();
	}
	Super::NativeDestruct();
}

void UFF_LiveHudScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 배속은 외부 시스템에서도 변경된다. 실제 값이 바뀔 때만 버튼 표시를 갱신한다.
	if (const UFF_FestivalViewModelSubsystem* VM = GetViewModel())
	{
		if (!FMath::IsNearlyEqual(LastShownTimeScale, VM->GetTimeScale())) { RefreshSpeed(); }
	}
}

float UFF_LiveHudScreen::GetBottomStatusRowHeight() const
{
	return SizeBox_StatusRow && SizeBox_StatusRow->IsHeightOverride()
		? SizeBox_StatusRow->GetHeightOverride() : StatusRowHeight;
}

float UFF_LiveHudScreen::GetBottomBarHeight() const
{
	const bool bMenuVisible = !Border_MenuRow || Border_MenuRow->GetVisibility() != ESlateVisibility::Collapsed;
	const float MenuHeight = SizeBox_MenuRow && SizeBox_MenuRow->IsHeightOverride()
		? SizeBox_MenuRow->GetHeightOverride() : MenuRowHeight;
	return GetBottomStatusRowHeight() + (bMenuVisible ? MenuHeight : 0.f);
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

	CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_LiveHudScreen::HandleStaffCountChanged);
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
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_LiveHudScreen::HandleStaffCountChanged);
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
		{ EFF_LiveTab::Density,		Tab_Density },
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
			LogPanelGeometryNextTick(*Panel);
		}
		else
		{
			Selection = FFF_LiveTabSelection::None();
		}
	}

	// 🔴 호스트를 함께 켜고 끈다. 생성기가 `Collapsed` 로 만들어 두기 때문에 패널만
	// `Visible` 로 바꾸면 **접힌 부모 안에 들어가 아무것도 안 보인다** (2026-08-27 실기 확인).
	if (Panel_PanelHost)
	{
		Panel_PanelHost->SetVisibility(Selection.bHasSelection
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
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

	// 🔴 호스트가 `UOverlay` 면 슬롯 기본값이 `Left`·`Top` 이라 **패널이 내용 크기로 쪼그라든다.**
	// 왼쪽 도킹 패널은 호스트를 가득 채워야 한다 — 안 그러면 자리는 잡혔는데 흰 상자만 보인다.
	UPanelSlot* AddedSlot = Panel_PanelHost->AddChild(Panel);
	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(AddedSlot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("[%s] `%s` 패널을 만들었다 (%s%s). 호스트=%s"),
		*GetName(), *EnumName, *PanelClass->GetName(),
		bIsPlaceholder ? TEXT(" · 자리표시") : TEXT(""),
		*Panel_PanelHost->GetClass()->GetName());

	PanelWidgets.Add(Tab, Panel);
	return Panel;
}

void UFF_LiveHudScreen::LogBarGeometryNextTick(EFF_FestivalStage Stage)
{
	// 방금 바뀐 가시성이 반영되려면 한 프레임 지나야 한다 (`LogPanelGeometryNextTick` 과 같은 이유).
	TWeakObjectPtr<UFF_LiveHudScreen> WeakThis(this);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, Stage]()
		{
			const UFF_LiveHudScreen* Screen = WeakThis.Get();
			if (!Screen)
			{
				return;
			}

			const FVector2D MenuSize = Screen->Border_MenuRow
				? Screen->Border_MenuRow->GetCachedGeometry().GetLocalSize()
				: FVector2D::ZeroVector;
			const FVector2D StatusSize = Screen->Border_StatusRow
				? Screen->Border_StatusRow->GetCachedGeometry().GetLocalSize()
				: FVector2D::ZeroVector;
			const FVector2D ScreenSize = Screen->GetCachedGeometry().GetLocalSize();

			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("[%s] 바 실측 — 단계=%s · 메뉴행 %.0fx%.0f · 상태행 %.0fx%.0f · 루트 %.0fx%.0f"),
				*Screen->GetName(),
				*UFF_UIDisplayTextLibrary::GetFestivalStageText(Stage).ToString(),
				MenuSize.X, MenuSize.Y, StatusSize.X, StatusSize.Y, ScreenSize.X, ScreenSize.Y);
		}));
	}
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

bool UFF_LiveHudScreen::IsPanelDragAllowed() const
{
	if (!bAllowPanelDrag || !Panel_PanelHost || !Selection.bHasSelection)
	{
		return false;
	}

	// `Designer` 는 코드가 자리를 안 잡는다. 끌게 두면 다음에 열 때 되돌아가서 헛수고가 된다.
	return ResolveLayout(Selection.Tab).Dock != EFF_LivePanelDock::Designer;
}

void UFF_LiveHudScreen::NudgePanelPosition(FVector2D SlateDelta)
{
	if (!IsPanelDragAllowed() || SlateDelta.IsNearlyZero())
	{
		return;
	}

	// 🔴 **표를 고쳐서 옮긴다.** 슬롯을 직접 만지면 탭을 닫았다 열 때 `ApplyPanelLayout` 이
	// 표의 값으로 덮어써서 옮긴 자리가 사라진다. 준비 단계에서 같은 판단을 했다.
	//
	// 네 방향 모두 두 축이 점 앵커라 `Offset` 의 X·Y 가 그대로 화면 이동량이 된다 —
	// 늘어난 축이 섞여 있던 준비 단계와 달리 버릴 성분이 없다.
	FFF_LivePanelLayout Layout = ResolveLayout(Selection.Tab);

	// 오른쪽 도킹은 X 부호가 뒤집혀 있다. 그대로 더하면 커서와 반대로 움직인다.
	const float SignX = (Layout.Dock == EFF_LivePanelDock::Right) ? -1.f : 1.f;

	Layout.Offset.X += SignX * SlateDelta.X;
	Layout.Offset.Y += SlateDelta.Y;

	PanelLayouts.Add(Selection.Tab, Layout);
	ApplyPanelLayout(Layout);
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

void UFF_LiveHudScreen::ApplyPanelLayout(const FFF_LivePanelLayout& Layout)
{
	if (!Panel_PanelHost)
	{
		return;
	}

	if (Layout.Dock == EFF_LivePanelDock::Designer)
	{
		return;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Panel_PanelHost->Slot);
	if (!CanvasSlot)
	{
		// 캔버스가 아니면 위치는 디자이너가 소유한다. 코드가 짐작해서 옮기지 않는다.
		//
		// 🔴 `Warning` 이다. 이 경우 패널이 **화면 가운데 조그맣게** 뜨는데, 원인이
		// "코드가 자리를 안 잡았다" 라는 것이 로그 없이는 안 보인다 (2026-08-27).
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Panel_PanelHost 가 CanvasPanel 자식이 아니라(%s) 자리를 코드가 정하지 않는다. "
				 "WBP 에서 Canvas_Root 바로 아래에 두거나 디자이너에서 자리를 잡는다."),
			*GetName(),
			Panel_PanelHost->Slot ? *Panel_PanelHost->Slot->GetClass()->GetName() : TEXT("슬롯 없음"));
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
	const float PanelTop = -GetBottomStatusRowHeight() + static_cast<float>(Layout.Offset.Y);

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
		Button.SetSelected(Selection.IsSelected(Tab));
		VisibleTabCount += bVisible ? 1 : 0;
	});

	// 🔴 탭이 하나도 없으면 메뉴 행째 접는다. 라이브가 아닌 Stage 에 이 화면이 배선돼 있으면
	// **빈 띠**만 남아 고장난 화면처럼 보인다. 상태 행은 그대로 둔다.
	const bool bHasTabs = (VisibleTabCount > 0);

	if (Border_MenuRow)
	{
		Border_MenuRow->SetVisibility(bHasTabs
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	// 🔴 **바 높이도 같이 줄인다.** 접기만 하면 두 행이 모두 `Fill` 이라 상태 행이 남은
	//    58 을 전부 가져가서, 배경만 두 배가 되고 안의 글자는 `VAlign_Center` 라 그대로다 —
	//    위아래로 빈 흰 띠가 생긴다. 실측으로 갈랐다 (2026-09-03):
	//      마감   메뉴행 75x29 · 상태행 2020x29
	//      피날레 메뉴행  0x0  · 상태행 2020x58   ← 이것
	if (SizeBox_Bar)
	{
		const float TargetHeight = GetBottomBarHeight();

		SizeBox_Bar->SetHeightOverride(TargetHeight);

		// 🔴 **`SetHeightOverride` 만으로는 안 된다** (2026-09-03 실기에서 확인).
		//
		//   `HeightOverride` 는 *희망* 크기를 바꿀 뿐이고, 실제 크기는 부모가 준다.
		//   이 바는 캔버스에 **Y 점 앵커 + 명시 크기**로 놓여 있어 (`Offsets.Bottom` = 높이)
		//   캔버스가 준 58 이 그대로 이긴다. 슬롯을 같이 고쳐야 실제로 줄어든다.
		//
		//   ★ 점 앵커라서 `Offsets.Bottom` 이 여백이 아니라 **크기**다. 늘어난 축이었으면
		//     뜻이 정반대가 된다 (`docs/memory/prep-hud-reference.md` 2절).
		if (UCanvasPanelSlot* BarSlot = Cast<UCanvasPanelSlot>(SizeBox_Bar->Slot))
		{
			FMargin Offsets = BarSlot->GetOffsets();
			Offsets.Bottom = TargetHeight;
			BarSlot->SetOffsets(Offsets);
		}
	}
	if (Text_SpeedReason)
	{
		if (UCanvasPanelSlot* ReasonSlot = Cast<UCanvasPanelSlot>(Text_SpeedReason->Slot))
		{
			FMargin Offsets = ReasonSlot->GetOffsets();
			Offsets.Top = -GetBottomBarHeight() - 4.f;
			ReasonSlot->SetOffsets(Offsets);
		}
	}

	// 사라진 탭의 패널이 남아 있으면 닫을 수단이 화면에서 없어진다.
	//
	// 🔴 세 번째 인자를 빠뜨리면 안 된다. 피날레 탭이 생기면서 가시성 판정이 `구매했는가` 를
	//    함께 보게 됐다 (2026-09-02). Dev 쪽 원본은 두 인자짜리라 그대로 쓰면 컴파일이 막힌다.
	ApplySelection(FFF_LiveHudRules::ClampSelectionToVisible(Selection, Stage, HasPurchasedFinale()));
}

void UFF_LiveHudScreen::RefreshStatusRow()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// --- 왼쪽: 축제 → 금화 → 오늘 매출 → 인력 → 관객 ---
	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	SetGroupTextOrHide(Box_FestivalName, Text_FestivalName,
		Flow ? Flow->GetActiveFestivalDisplayName() : FText::GetEmpty());

	SetGroupTextOrHide(Box_Gold, Text_Gold,
		FText::FormatNamed(GoldTextFormat, TEXT("Amount"), FText::AsNumber(ViewModel->GetGold())));

	SetGroupTextOrHide(Box_TodayRevenue, Text_TodayRevenue,
		FText::FormatNamed(RevenueTextFormat, TEXT("Amount"), FText::AsNumber(ViewModel->GetTodayRevenue())));

	// 프로그램은 인원을 묶지 않는다. 배치 개념은 사건 시스템이 와야 생긴다.
	const int32 TotalStaff = ViewModel->GetTotalStaffCount();
	SetGroupTextOrHide(Box_Staff, Text_Staff, (TotalStaff >= 0)
		? FText::FormatNamed(StaffTextFormat, TEXT("Count"), FText::AsNumber(TotalStaff))
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
		(ShownVisitorCount >= 0) ? FText::FormatNamed(VisitorTextFormat, TEXT("Count"), FText::AsNumber(ShownVisitorCount)) : FText::GetEmpty());

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
	SetTextOrHide(Text_DayStage, FText::FormatNamed(DayStageTextFormat,
		TEXT("Day"), FText::AsNumber(ViewModel->GetCurrentDay()),
		TEXT("Stage"), UFF_UIDisplayTextLibrary::GetFestivalStageText(ViewModel->GetCurrentStage())));

	if (Border_DayStage)
	{
		Border_DayStage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// --- 칩 앞의 점: 라이브에서만 불이 들어온다 ---
	//
	// 카메라 녹화등과 같은 뜻이다. 전에는 **항상 빨간색**이었는데, 그러면 마감에도 리포트에도
	// 계속 켜져 있어서 아무 뜻도 없는 장식이 된다 (2026-09-03 백종태 요청).
	//
	// !! 이 점만으로 상태를 말하지 않는다 !! 바로 옆 글자가 이미 `1일차 · 마감` 이라고
	// 적혀 있고, 점은 그 글자를 거드는 장식이다 (AT-011). 색을 못 봐도 잃는 정보가 없다.
	//
	// `IsLiveStage()` 를 그대로 쓴다 — 배속이 걸리는 단계와 같은 뜻이라 판정을 두 벌 두지 않는다.
	if (Image_LiveDot)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (Palette)
		{
			const bool bOnAir = FFF_LiveHudRules::IsLiveStage(ViewModel->GetCurrentStage());

			// 꺼진 등은 칩 바탕(`TealDeep`)보다 어두워야 눌린 것처럼 보인다.
			Image_LiveDot->SetBrushTintColor(FSlateColor(
				bOnAir ? Palette->InGame.Danger : Palette->InGame.Ink));
		}
	}

	// --- 오른쪽: 시계. 배속 버튼 왼쪽에 온다 ---
	const FText ClockText = ViewModel->GetLiveClockText();
	const int32 RemainingSeconds = FMath::Max(0, FMath::CeilToInt(ViewModel->GetRemainingSeconds()));

	SetGroupTextOrHide(Box_Clock, Text_Clock, ClockText.IsEmpty()
		? FText::GetEmpty()
		: FText::FormatNamed(ClockTextFormat,
			TEXT("Clock"), ClockText, TEXT("Seconds"), FText::AsNumber(RemainingSeconds)));
}

void UFF_LiveHudScreen::RefreshSpeed()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Live;

	const EFF_LiveSpeedState State = FFF_LiveHudRules::ResolveSpeedState(
		Stage,
		ViewModel ? ViewModel->IsTimeScaleLocked() : false);
	LastShownTimeScale = ViewModel ? ViewModel->GetTimeScale() : 1.f;

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
	const TArray<UButton*> Buttons = { Button_Speed1.Get(), Button_Speed2.Get(), Button_Speed4.Get() };
	if (SpeedButtonStyles.IsEmpty())
	{
		for (const UButton* Button : Buttons) { SpeedButtonStyles.Add(Button ? Button->GetStyle() : FButtonStyle()); }
	}
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		UButton* Button = Buttons[Index];
		if (Button)
		{
			Button->SetIsEnabled(bCanChange);
			FButtonStyle Style = SpeedButtonStyles[Index];
			if (FMath::IsNearlyEqual(LastShownTimeScale, static_cast<float>(1 << Index)))
			{
				Style.Normal = Style.Hovered = Style.Pressed;
			}
			Button->SetStyle(Style);
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

	LogBarGeometryNextTick(NewStage);
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

void UFF_LiveHudScreen::HandleStaffCountChanged(int32 NewValue)
{
	// !! 인자를 쓰지 않는다 !! 값이 울릴 때마다 전체를 다시 읽는다 —
	// 인자를 믿으면 다른 지표가 울렸을 때 그 숫자가 인원 자리에 들어간다.
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
