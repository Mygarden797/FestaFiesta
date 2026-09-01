// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/FF_PrepHudBar.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Texture2D.h"
#include "FF_FFZSWorldSubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "UI/Workspace/FF_FloatingPanelStateSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepHud"

namespace FFPrepHudBarLocal
{
	/**
	 * 자리표시 패널의 기본 경로. `FF.PrepHudUMG` 가 여기에 만든다.
	 *
	 * 경로를 코드에 박는 것은 아이콘 폴더와 같은 성격이다 — 규칙을 하나 정해 두고 자산을 거기 넣는다.
	 * 바꾸려면 이 상수와 생성기 두 곳을 함께 고친다.
	 */
	constexpr const TCHAR* PlaceholderPanelPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPlaceholderPanel.WBP_FF_PrepPlaceholderPanel_C");

	/** enum 항목 이름과 표시명을 한 번에 얻는다. 못 찾으면 빈 값이다. */
	void DescribeTab(EFF_PrepHudTab Tab, FString& OutName, FText& OutDisplayName)
	{
		const UEnum* TabEnum = StaticEnum<EFF_PrepHudTab>();
		if (!TabEnum)
		{
			return;
		}

		const int32 Index = TabEnum->GetIndexByValue(static_cast<int64>(Tab));
		OutName = TabEnum->GetNameStringByIndex(Index);

		// 🔴 `GetDisplayNameTextByIndex` 를 쓰지 않는다. 그 함수는 **에디터 메타데이터**를 읽는데
		//    패키징하면 메타데이터가 떨어져 나가서 조용히 `Build`·`Check` 같은 영문 항목 이름이
		//    나온다. 2026-08-28 시연 패키징에서 하단 탭이 전부 영문이었다.
		OutDisplayName = UFF_UIDisplayTextLibrary::GetPrepTabText(Tab);
	}
}

UFF_PrepHudBar::UFF_PrepHudBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// === CTA 문구 기본값 ===
	// `{Day}` 는 현재 일차로 치환된다. 문구에 `Day 1` 을 박으면 2·3일차에 거짓말이 된다.
	//
	// 🔴 `PromotionPreview` 에는 CTA 를 두지 않는다 — **이 바 자체가 거기서 안 뜬다.**
	//
	// 2026-08-27 에 두 번 바뀐 자리라 경위를 남긴다.
	//   ① 처음엔 CTA 가 없었다 (시간이 지나면 자동으로 넘어간다는 이유)
	//   ② `Day 1 의 개장 준비 자리` 로 보고 `개장하기` 를 넣었다
	//   ③ 실기에서 보니 **셋업에서 `확정하고 Day 1 시작` 을 누른 직후 준비 단계 바가
	//      다시 나오는 것**이 이상했다. 확정했는데 왜 또 준비 화면인가.
	//      → 홍보 프리뷰·노을 전환을 라이브 HUD 로 넘겼다 (`StageScreens`).
	//
	// 그래서 준비 단계 바가 뜨는 곳은 셋업과 개장 준비 둘뿐이다.
	CtaLabels.Add(EFF_FestivalStage::Setup,
		LOCTEXT("PrepCta_Setup", "확정하고 Day {Day} 시작"));
	CtaLabels.Add(EFF_FestivalStage::MorningCheck,
		LOCTEXT("PrepCta_MorningCheck", "개장하기"));

	// === 탭별 자리와 크기 ===
	//
	// 기준은 **그 탭이 월드를 만지는가**와 **내용이 얼마나 되는가** 두 개다.
	//   배치  : 지면이 보여야 시설을 놓는다 → 하단
	//   나머지: 표·카드를 읽는 동안 월드가 필요 없다 → 중앙. 크기는 내용 양에 맞춘다
	//
	// ★ 여기 숫자는 시작점이다. 화면을 보고 콘솔로 맞춘 뒤 확정한다 —
	//   순서는 `PanelLayouts` 선언 주석에.
	const FVector2D CenterNudge(0.0, -29.0);	// 하단 바 절반만큼 위로. 바를 뺀 영역의 중앙

	// 하단 — `Size.Y` 만 쓴다. 화면 높이의 40% 를 넘기지 않는다 (지면이 가려진다).
	PanelLayouts.Add(EFF_PrepHudTab::Build,
		{ EFF_PrepPanelDock::Bottom, FVector2D(0.0, 320.0), FVector2D::ZeroVector });

	// 인력 — 스텝퍼와 짧은 표뿐이다. 크게 열면 빈 공간만 늘어난다.
	PanelLayouts.Add(EFF_PrepHudTab::Staff,
		{ EFF_PrepPanelDock::Center, FVector2D(760.0, 520.0), CenterNudge });

	// 프로그램 — 슬롯·후보·예상 효과 3열.
	PanelLayouts.Add(EFF_PrepHudTab::Program,
		{ EFF_PrepPanelDock::Center, FVector2D(1160.0, 640.0), CenterNudge });

	// 홍보 — 등급 카드 3장 + 예상 방문객 범위.
	PanelLayouts.Add(EFF_PrepHudTab::Promotion,
		{ EFF_PrepPanelDock::Center, FVector2D(1080.0, 600.0), CenterNudge });

	// 피날레 — 등급 카드를 가로로 5장 놓아야 해서 가장 넓다.
	PanelLayouts.Add(EFF_PrepHudTab::Finale,
		{ EFF_PrepPanelDock::Center, FVector2D(1360.0, 700.0), CenterNudge });

	// 점검 — 개장 직전 마지막 확인이라 카드·요약·견적을 한 번에 봐야 한다.
	PanelLayouts.Add(EFF_PrepHudTab::Check,
		{ EFF_PrepPanelDock::Center, FVector2D(1280.0, 780.0), CenterNudge });

	DefaultPanelLayout = { EFF_PrepPanelDock::Center, FVector2D(1160.0, 640.0), CenterNudge };

	PlaceholderPanelClass = TSoftClassPtr<UFF_PrepPlaceholderPanel>(
		FSoftObjectPath(FString(FFPrepHudBarLocal::PlaceholderPanelPath)));
}

// =============================================================================
// 수명
// =============================================================================

void UFF_PrepHudBar::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 색은 팔레트가 소유한다. 디자이너에서 손으로 바꾼 색은 여기서 매번 덮인다 —
	// 크기·패딩·폰트는 그대로 두므로 마감은 에디터에서 계속 할 수 있다.
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] UI 팔레트를 읽을 수 없어 색을 WBP 값 그대로 둔다."), *GetName());
		return;
	}

	// 🔴 `SetBrushColor` 만으로는 아무것도 안 그려진다. 이유는 `PaintRowBackground` 주석에.
	// 메뉴 행은 버튼 폭까지만 오는 떠 있는 칩이라 반경이 상태 행과 다르다.
	PaintRowBackground(Border_MenuRow, Palette->InGame.Paper, MenuRowCornerRadius);
	PaintRowBackground(Border_StatusRow, Palette->InGame.StatusBar, RowCornerRadius);

	// 메뉴 행 Border 를 자식 크기로 줄인다. 슬롯 정렬은 `.uasset` 에 저장되는 값이라
	// 생성기만 고치면 이미 만들어진 WBP 는 그대로 남는다 — 그래서 런타임에서도 맞춘다.
	if (bMenuRowHugsButtons && Border_MenuRow)
	{
		if (UVerticalBoxSlot* MenuRowSlot = Cast<UVerticalBoxSlot>(Border_MenuRow->Slot))
		{
			MenuRowSlot->SetHorizontalAlignment(HAlign_Center);
			MenuRowSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 일차·단계는 밝은 상태 행 위에서 이 한 줄만 도드라져야 한다. 어두운 판 + 흰 글씨다.
	PaintRowBackground(Border_DayStage, Palette->InGame.TealDeep, DayStageCornerRadius);
	if (Text_DayStage)
	{
		Text_DayStage->SetColorAndOpacity(FSlateColor(Palette->InGame.White));
	}

	if (Text_Arrears)
	{
		// 미납은 경고다. **색만으로 알리지 않는다** (AT-011) — 문구에 `미납` 을 함께 넣는다.
		Text_Arrears->SetColorAndOpacity(FSlateColor(Palette->InGame.Danger));
	}
	if (Text_CtaReason)
	{
		Text_CtaReason->SetColorAndOpacity(FSlateColor(Palette->InGame.InkMuted));
	}

	// 탭 색도 팔레트에서 내려준다. 위젯이 각자 읽으면 테마를 바꿀 때 하나가 빠진다.
	const TArray<UFF_PrepHudTabButton*> AllTabs = {
		Tab_Build, Tab_Program, Tab_Staff, Tab_Promotion, Tab_Finale, Tab_Check };
	for (UFF_PrepHudTabButton* TabButton : AllTabs)
	{
		if (TabButton)
		{
			TabButton->SetContentColors(Palette->InGame.Ink, Palette->InGame.White);
		}
	}
}

void UFF_PrepHudBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 🔴 전체 화면을 덮는 HUD 가 `Visible` 이면 월드 클릭을 전부 먹어 건축 배치가 죽는다.
	// 화면은 정상으로 보이므로 증상만으로는 원인을 찾을 수 없다. 여기서 스스로 확인한다.
	if (bEnforceHitTestInvisibleRoot && GetVisibility() == ESlateVisibility::Visible)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 루트 Visibility 가 Visible 이라 월드 클릭을 먹는다. "
				 "SelfHitTestInvisible 로 바꿨다 — WBP 에서도 바꿔 두는 것이 좋다."),
			*GetName());
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	CollectTabs();
	EnsurePanelWorkspace();

	if (Button_Cta)
	{
		Button_Cta->OnClicked.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleCtaClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Button_Cta 가 없어 단계를 넘길 버튼이 없다."), *GetName());
	}

	// 도킹은 **패널을 열 때** 정한다. 탭마다 방향이 다르므로 여기서 한 번 맞춰 두면 틀린다.

	// 시작은 닫힌 상태다. 아무 패널도 열지 않는다 — 셋업에 들어오자마자 화면 절반이
	// 패널로 덮이면 월드를 볼 수 없다.
	if (Panel_PanelHost)
	{
		Panel_PanelHost->SetVisibility(ESlateVisibility::Collapsed);
	}
}

#if !UE_BUILD_SHIPPING
TWeakObjectPtr<UFF_PrepHudBar> UFF_PrepHudBar::ActiveBar;
#endif

void UFF_PrepHudBar::NativeConstruct()
{
	Super::NativeConstruct();

#if !UE_BUILD_SHIPPING
	ActiveBar = this;
#endif

	// 구독은 여기다. `NativeOnInitialized`(인스턴스당 1회)에 두면 위젯이 재사용될 때
	// 두 번째 진입부터 아무 값도 안 들어온다 (2026-08-24에 화면 5종에서 실제로 발생).
	BindSources();
	RestoreFloatingPanelState();
	RefreshAll();
}

void UFF_PrepHudBar::NativeDestruct()
{
#if !UE_BUILD_SHIPPING
	// 다음 바가 이미 등록됐을 수 있으므로 내가 등록한 것만 지운다.
	if (ActiveBar.Get() == this)
	{
		ActiveBar = nullptr;
	}
#endif

	// 화면 인스턴스는 단계마다 다시 만들어진다. 열린 상태는 저장하되, 이전 패널의
	// 외부 구독은 여기서 정확히 한 번 정리한다.
	for (const EFF_PrepHudTab Tab : OpenTabs)
	{
		if (TObjectPtr<UFF_PrepPanelBase>* Panel = PanelWidgets.Find(Tab))
		{
			if (*Panel)
			{
				(*Panel)->HandlePanelClosed();
			}
		}
	}
	SaveFloatingPanelState();
	UnbindSources();
	Super::NativeDestruct();
}

// =============================================================================
// 구독
// =============================================================================

UFF_FestivalViewModelSubsystem* UFF_PrepHudBar::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

UFF_UIFlowSubsystem* UFF_PrepHudBar::GetFlow() const
{
	return UFF_UIFlowSubsystem::Get(this);
}

void UFF_PrepHudBar::BindSources()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleGoldChanged);
		CachedViewModel->OnArrearsChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleArrearsChanged);
		CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleStaffCountChanged);
		CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleStageViewChanged);

		// 프로그램 편성은 전체 인원을 바꾸지 않는다. 이것까지 받아야 상태 행이 따라간다.
		CachedViewModel->OnAvailableStaffCountChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleStaffCountChanged);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 금화·미납·스태프·단계가 전부 비어 보인다."),
			*GetName());
	}

	// 시설 수는 Zone 이 소유한다. 시설을 놓거나 철거하면 CTA 가 즉시 따라가야 한다.
	if (UWorld* World = GetWorld())
	{
		if (UFFZSWorldSubsystem* Zone = World->GetSubsystem<UFFZSWorldSubsystem>())
		{
			Zone->OnWorldChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleWorldChanged);
			BoundZoneWorldSubsystem = Zone;
		}
		else
		{
			// 치명적이지 않다 — 시설 수를 `모름`으로 두고 CTA 를 막지 않는다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("[%s] Zone 월드 서브시스템이 없다. 시설 수를 읽지 못해 개장 조건을 검사하지 않는다."),
				*GetName());
		}
	}
}

void UFF_PrepHudBar::UnbindSources()
{
	// 해제는 조건부, 정리(`nullptr` 대입)는 무조건. 조기 return 으로 빠져나가지 않는다.
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleGoldChanged);
		CachedViewModel->OnArrearsChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleArrearsChanged);
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleStaffCountChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleStageViewChanged);
		CachedViewModel->OnAvailableStaffCountChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleStaffCountChanged);
	}
	CachedViewModel = nullptr;

	if (UFFZSWorldSubsystem* Zone = BoundZoneWorldSubsystem.Get())
	{
		Zone->OnWorldChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleWorldChanged);
	}
	BoundZoneWorldSubsystem = nullptr;
}

// =============================================================================
// 탭
// =============================================================================

void UFF_PrepHudBar::CollectTabs()
{
	TabButtons.Reset();

	// 필드와 enum 값을 한 곳에서 짝짓는다. 탭을 늘리면 이 표에 한 줄만 추가한다.
	const TPair<EFF_PrepHudTab, UFF_PrepHudTabButton*> Bindings[] = {
		{ EFF_PrepHudTab::Build,		Tab_Build },
		{ EFF_PrepHudTab::Program,		Tab_Program },
		{ EFF_PrepHudTab::Staff,		Tab_Staff },
		{ EFF_PrepHudTab::Promotion,	Tab_Promotion },
		{ EFF_PrepHudTab::Finale,		Tab_Finale },
		{ EFF_PrepHudTab::Check,		Tab_Check }
	};

	TArray<FString> MissingTabs;

	for (const TPair<EFF_PrepHudTab, UFF_PrepHudTabButton*>& Binding : Bindings)
	{
		FString EnumName;
		FText DisplayName;
		FFPrepHudBarLocal::DescribeTab(Binding.Key, EnumName, DisplayName);

		if (!Binding.Value)
		{
			MissingTabs.Add(EnumName);
			continue;
		}

		// 라벨과 아이콘은 코드가 채운다. WBP 에 문구를 박으면 enum 과 화면이 갈라지고,
		// 아이콘을 박으면 6개가 같은 그림이 된다 (2026-08-12에 라이브 HUD 에서 실제로 그랬다).
		Binding.Value->Configure(Binding.Key, DisplayName, ResolveTabIcon(Binding.Key, EnumName));
		Binding.Value->OnHudTabButtonClicked.BindUObject(this, &UFF_PrepHudBar::HandleTabButtonClicked);
		TabButtons.Add(Binding.Value);
	}

	// 빠진 탭을 조용히 넘기지 않는다. WBP 에서 이름을 틀렸을 때 이 로그가 유일한 단서다.
	if (!MissingTabs.IsEmpty())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] WBP 에 없는 탭 %d개: %s — 위젯 이름이 Tab_<enum 이름>과 같아야 한다."),
			*GetName(), MissingTabs.Num(), *FString::Join(MissingTabs, TEXT(", ")));
	}
}

FSlateBrush UFF_PrepHudBar::ResolveTabIcon(EFF_PrepHudTab Tab, const FString& EnumName) const
{
	if (const FSlateBrush* Override = TabIconOverrides.Find(Tab))
	{
		return *Override;
	}

	FSlateBrush Brush;

	if (TabIconFolder.IsEmpty() || TabIconNameFormat.IsEmpty() || EnumName.IsEmpty())
	{
		return Brush;
	}

	const FString AssetName = FString::Format(*TabIconNameFormat, { EnumName });
	const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *TabIconFolder, *AssetName, *AssetName);

	// 작은 UI 아이콘 6개라 동기 로드로 둔다. 위젯 생성 시 한 번만 돈다.
	UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (!IconTexture)
	{
		// 아이콘이 없어도 라벨로 구분된다. 치명적이지 않으므로 경고만 남기고 진행한다.
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[%s] 탭 아이콘이 없다 — %s. 아직 만들지 않았으면 정상이다 (docs/ui/icon-inventory.md)."),
			*GetName(), *ObjectPath);
		return Brush;
	}

	Brush.SetResourceObject(IconTexture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetImageSize(FVector2D(TabIconSize, TabIconSize));
	return Brush;
}

void UFF_PrepHudBar::HandleTabButtonClicked(int32 TabId)
{
	SelectTab(static_cast<EFF_PrepHudTab>(TabId));
}

void UFF_PrepHudBar::SelectTab(EFF_PrepHudTab Tab)
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Setup;
	const bool bIsFinalDay = ViewModel ? ViewModel->IsFinalDay() : false;

	if (!FFF_PrepHudRules::IsTabVisible(Tab, Stage, bIsFinalDay))
	{
		FString EnumName;
		FText DisplayName;
		FFPrepHudBarLocal::DescribeTab(Tab, EnumName, DisplayName);

		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 지금 단계에서 볼 수 없는 탭(%s)을 열려고 했다. 무시한다."),
			*GetName(), *EnumName);
		return;
	}

	// 열린 탭을 다시 눌러도 닫지 않는다. 닫기는 각 패널의 닫기 버튼으로만 한다.
	if (IsPanelOpen(Tab))
	{
		BringPanelToFront(Tab);
		return;
	}

	OpenPanel(Tab);
}

void UFF_PrepHudBar::CloseCurrentPanel()
{
	if (Selection.bHasSelection)
	{
		ClosePanel(Selection.Tab);
	}
}

void UFF_PrepHudBar::ClosePanel(EFF_PrepHudTab Tab)
{
	if (!OpenTabs.RemoveSingle(Tab))
	{
		return;
	}

	if (TObjectPtr<UFF_PrepPanelBase>* Panel = PanelWidgets.Find(Tab))
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
			? FFF_PrepTabSelection::None()
			: FFF_PrepTabSelection::Of(OpenTabs.Last());
	}

	HidePanelLimitNotice();
	RefreshPanelZOrder();
	RefreshPanelHostVisibility();
	RefreshPanelSelectionVisuals();
	OnTabSelectionChanged.Broadcast(Selection);
	SaveFloatingPanelState();
}

bool UFF_PrepHudBar::IsPanelOpen(EFF_PrepHudTab Tab) const

{
	return OpenTabs.Contains(Tab);
}

bool UFF_PrepHudBar::OpenPanel(EFF_PrepHudTab Tab)

{
	if (IsPanelOpen(Tab))
	{
		BringPanelToFront(Tab);
		return true;
	}

	// 상한 검사 뒤에는 어떤 선택·가시성·Z-order도 바꾸지 않는다.
	if (OpenTabs.Num() >= UFF_FloatingPanelStateSubsystem::MaxOpenPanelCount)
	{
		ShowPanelLimitNotice();
		return false;
	}

	UFF_PrepPanelBase* Panel = EnsurePanel(Tab);
	if (!Panel)
	{
		return false;
	}

	OpenTabs.Add(Tab);
	Selection = FFF_PrepTabSelection::Of(Tab);
	ApplyPanelLayout(Tab, ResolveLayout(Tab));
	Panel->ApplyPinnedState(PinnedTabs.Contains(Tab));
	Panel->SetVisibility(ESlateVisibility::Visible);
	Panel->HandlePanelOpened();

	HidePanelLimitNotice();
	RefreshPanelZOrder();
	RefreshPanelHostVisibility();
	RefreshPanelSelectionVisuals();
	OnTabSelectionChanged.Broadcast(Selection);
	SaveFloatingPanelState();
	return true;
}

void UFF_PrepHudBar::BringPanelToFront(EFF_PrepHudTab Tab)

{
	if (!OpenTabs.RemoveSingle(Tab))
	{
		return;
	}

	OpenTabs.Add(Tab);
	const FFF_PrepTabSelection Previous = Selection;
	Selection = FFF_PrepTabSelection::Of(Tab);
	RefreshPanelZOrder();
	RefreshPanelSelectionVisuals();
	if (Selection != Previous)
	{
		OnTabSelectionChanged.Broadcast(Selection);
	}
	SaveFloatingPanelState();
}

void UFF_PrepHudBar::SetPanelPinned(EFF_PrepHudTab Tab, bool bPinned)

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

void UFF_PrepHudBar::RefreshPanelSelectionVisuals()

{
	for (UFF_PrepHudTabButton* TabButton : TabButtons)
	{
		if (TabButton)
		{
			TabButton->SetSelected(IsPanelOpen(TabButton->GetTab()));
		}
	}
}

// =============================================================================
// 패널
// =============================================================================

void UFF_PrepHudBar::EnsurePanelWorkspace()
{
	if (PanelWorkspace || !Panel_PanelHost || !WidgetTree)
	{
		return;
	}

	// 기존 WBP는 단일 패널 크기의 Overlay를 움직이도록 만들어졌다. 에셋을 덮어쓰지 않고
	// 호스트만 전체 화면으로 넓힌 뒤, 안쪽 Canvas의 자식마다 독립 슬롯을 준다.
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
			"PrepPanelLimitNotice",
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

void UFF_PrepHudBar::ShowPanelLimitNotice()
{
	EnsurePanelWorkspace();
	if (PanelLimitNotice)
	{
		PanelLimitNotice->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	RefreshPanelHostVisibility();
}

void UFF_PrepHudBar::HidePanelLimitNotice()
{
	if (PanelLimitNotice)
	{
		PanelLimitNotice->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFF_PrepHudBar::RefreshPanelHostVisibility()
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

void UFF_PrepHudBar::RefreshPanelZOrder()
{
	int32 NormalZ = 1;
	int32 PinnedZ = 100;
	for (const EFF_PrepHudTab Tab : OpenTabs)
	{
		const TObjectPtr<UFF_PrepPanelBase>* Found = PanelWidgets.Find(Tab);
		UCanvasPanelSlot* PanelCanvasSlot = Found && *Found
			? Cast<UCanvasPanelSlot>((*Found)->Slot)
			: nullptr;
		if (!PanelCanvasSlot)
		{
			continue;
		}

		PanelCanvasSlot->SetZOrder(PinnedTabs.Contains(Tab) ? PinnedZ++ : NormalZ++);
	}

	// 안내 문구는 고정 패널보다도 위에 있어야 가려지지 않는다.
	if (PanelLimitNotice)
	{
		if (UCanvasPanelSlot* NoticeSlot = Cast<UCanvasPanelSlot>(PanelLimitNotice->Slot))
		{
			NoticeSlot->SetZOrder(10000);
		}
	}
}

void UFF_PrepHudBar::RestoreFloatingPanelState()
{
	UFF_FloatingPanelStateSubsystem* State = UFF_FloatingPanelStateSubsystem::Get(this);
	if (!State)
	{
		return;
	}

	const FFF_PrepFloatingPanelSnapshot Snapshot = State->GetPrepSnapshot();
	for (const TPair<EFF_PrepHudTab, FFF_PrepPanelLayout>& Pair : Snapshot.RuntimeLayouts)
	{
		PanelLayouts.Add(Pair.Key, Pair.Value);
	}

	PinnedTabs = Snapshot.PinnedTabs;
	OpenTabs.Reset();
	Selection = FFF_PrepTabSelection::None();

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel
		? ViewModel->GetCurrentStage()
		: EFF_FestivalStage::Setup;
	const bool bIsFinalDay = ViewModel ? ViewModel->IsFinalDay() : false;

	for (const EFF_PrepHudTab Tab : Snapshot.OpenTabs)
	{
		if (OpenTabs.Num() >= UFF_FloatingPanelStateSubsystem::MaxOpenPanelCount)
		{
			break;
		}
		if (!FFF_PrepHudRules::IsTabVisible(Tab, Stage, bIsFinalDay))
		{
			continue;
		}

		OpenPanel(Tab);
	}

	if (Snapshot.FocusedSelection.bHasSelection
		&& IsPanelOpen(Snapshot.FocusedSelection.Tab))
	{
		BringPanelToFront(Snapshot.FocusedSelection.Tab);
	}

	RefreshPanelHostVisibility();
	SaveFloatingPanelState();
}

void UFF_PrepHudBar::SaveFloatingPanelState() const
{
	if (UFF_FloatingPanelStateSubsystem* State = UFF_FloatingPanelStateSubsystem::Get(this))
	{
		FFF_PrepFloatingPanelSnapshot Snapshot;
		Snapshot.OpenTabs = OpenTabs;
		Snapshot.PinnedTabs = PinnedTabs;
		Snapshot.RuntimeLayouts = PanelLayouts;
		Snapshot.FocusedSelection = Selection;
		State->SavePrep(Snapshot);
	}
}

UClass* UFF_PrepHudBar::ResolvePanelClass(EFF_PrepHudTab Tab, const FString& EnumName, bool& bOutIsPlaceholder) const
{
	bOutIsPlaceholder = false;

	// 1) 예외 표. 이름 규칙에서 벗어나는 패널만 여기 들어간다.
	if (const TSoftClassPtr<UFF_PrepPanelBase>* Mapped = PanelClasses.Find(Tab))
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

	// 2) 이름 규칙. U5 에서 패널을 하나 만들 때마다 표를 채우지 않아도 되게 한다.
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

	// 3) 자리표시. 아무것도 안 뜨면 탭이 고장 난 것으로 보인다.
	bOutIsPlaceholder = true;
	return PlaceholderPanelClass.IsNull() ? nullptr : PlaceholderPanelClass.LoadSynchronous();
}

UFF_PrepPanelBase* UFF_PrepHudBar::EnsurePanel(EFF_PrepHudTab Tab)
{
	if (TObjectPtr<UFF_PrepPanelBase>* Existing = PanelWidgets.Find(Tab))
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
			TEXT("[%s] 플로팅 패널 작업공간을 만들지 못했다. WBP의 Panel_PanelHost를 확인한다."),
			*GetName());
		return nullptr;
	}

	FString EnumName;
	FText DisplayName;
	FFPrepHudBarLocal::DescribeTab(Tab, EnumName, DisplayName);

	bool bIsPlaceholder = false;
	UClass* PanelClass = ResolvePanelClass(Tab, EnumName, bIsPlaceholder);

	if (!PanelClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] `%s` 탭에 띄울 패널이 없다. PlaceholderPanelClass 가 비었는지 확인한다 "
				 "(에디터 콘솔에서 `FF.PrepHudUMG` 를 한 번 실행하면 만들어진다)."),
			*GetName(), *EnumName);
		return nullptr;
	}

	UFF_PrepPanelBase* Panel = CreateWidget<UFF_PrepPanelBase>(this, PanelClass);
	if (!Panel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] `%s` 탭 패널 생성에 실패했다 (%s)."),
			*GetName(), *EnumName, *PanelClass->GetName());
		return nullptr;
	}

	Panel->InitializePanel(this, Tab, DisplayName);

	if (bIsPlaceholder)
	{
		if (UFF_PrepPlaceholderPanel* Placeholder = Cast<UFF_PrepPlaceholderPanel>(Panel))
		{
			const FText* Custom = PlaceholderMessages.Find(Tab);
			Placeholder->SetPlaceholderMessage(
				(Custom && !Custom->IsEmpty())
					? *Custom
					: FText::Format(
						LOCTEXT("PrepPanel_Placeholder", "{0} 패널은 아직 만드는 중입니다."),
						DisplayName));
		}
	}

	Panel->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* PanelSlot = PanelWorkspace->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAutoSize(false);
		PanelSlot->SetZOrder(1);
	}
	PanelWidgets.Add(Tab, Panel);
	return Panel;
}

void UFF_PrepHudBar::NudgePanelPosition(EFF_PrepHudTab Tab, FVector2D SlateDelta)
{
	if (!IsPanelDragAllowed(Tab) || SlateDelta.IsNearlyZero() || !IsPanelOpen(Tab))
	{
		return;
	}

	FFF_PrepPanelLayout Layout = ResolveLayout(Tab);
	// 다중 작업공간에서는 모든 도킹이 독립적인 점 앵커를 사용하므로 두 축을 그대로 옮긴다.
	Layout.Offset += SlateDelta;

	// 인스턴스의 표를 고친다. 에셋 기본값은 그대로다 — 확정은 WBP 에 적어서 한다.
	PanelLayouts.Add(Tab, Layout);
	ApplyPanelLayout(Tab, Layout);
	SaveFloatingPanelState();
}

void UFF_PrepHudBar::ResetPanelPosition()
{
	if (!Selection.bHasSelection)
	{
		return;
	}

	FFF_PrepPanelLayout Layout = ResolveLayout(Selection.Tab);
	if (Layout.Offset.IsNearlyZero())
	{
		return;
	}

	Layout.Offset = FVector2D::ZeroVector;
	PanelLayouts.Add(Selection.Tab, Layout);
	ApplyPanelLayout(Selection.Tab, Layout);
	SaveFloatingPanelState();
}

bool UFF_PrepHudBar::IsPanelDragAllowed(EFF_PrepHudTab Tab) const
{
	if (!bAllowPanelDrag || !PanelWorkspace || !IsPanelOpen(Tab) || PinnedTabs.Contains(Tab))
	{
		return false;
	}

	// `디자이너 지정` 은 코드가 슬롯을 아예 만지지 않는 모드다. 거기서 드래그를 허용하면
	// 값만 쌓이고 화면은 안 움직여 "끌리지 않는다"로 보인다.
	return ResolveLayout(Tab).Dock != EFF_PrepPanelDock::Designer;
}

FFF_PrepPanelLayout UFF_PrepHudBar::GetOpenPanelLayout() const
{
	return Selection.bHasSelection ? ResolveLayout(Selection.Tab) : DefaultPanelLayout;
}

void UFF_PrepHudBar::SetOpenPanelLayout(const FFF_PrepPanelLayout& NewLayout)
{
	if (!Selection.bHasSelection)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 열린 패널이 없어 레이아웃을 바꿀 수 없다. 탭을 먼저 연다."), *GetName());
		return;
	}

	PanelLayouts.Add(Selection.Tab, NewLayout);
	ApplyPanelLayout(Selection.Tab, ResolveLayout(Selection.Tab));
	SaveFloatingPanelState();
}

FString UFF_PrepHudBar::DescribeOpenPanelLayout() const
{
	if (!Selection.bHasSelection)
	{
		return TEXT("열린 패널이 없다.");
	}

	FString EnumName;
	FText DisplayName;
	FFPrepHudBarLocal::DescribeTab(Selection.Tab, EnumName, DisplayName);

	const FFF_PrepPanelLayout Layout = ResolveLayout(Selection.Tab);
	const UEnum* DockEnum = StaticEnum<EFF_PrepPanelDock>();
	const FString DockName = DockEnum
		? DockEnum->GetNameStringByIndex(DockEnum->GetIndexByValue(static_cast<int64>(Layout.Dock)))
		: TEXT("?");

	// WBP 의 `Panel Layouts` 표에 그대로 옮겨 적을 수 있는 형태로 찍는다.
	return FString::Printf(
		TEXT("%s — Dock=%s  Size=(%.0f, %.0f)  Offset=(%.0f, %.0f)"),
		*EnumName, *DockName, Layout.Size.X, Layout.Size.Y, Layout.Offset.X, Layout.Offset.Y);
}

FFF_PrepPanelLayout UFF_PrepHudBar::ResolveLayout(EFF_PrepHudTab Tab) const
{
	FFF_PrepPanelLayout Layout = DefaultPanelLayout;
	if (const FFF_PrepPanelLayout* Found = PanelLayouts.Find(Tab))
	{
		Layout = *Found;
	}

	// 예전 단일 호스트에서는 Bottom의 폭과 Left/Right의 높이를 부모가 정했다. 패널별 Canvas
	// 슬롯에서는 두 축 모두 명시해야 하므로 0인 성분만 기본 크기로 보충한다.
	if (Layout.Size.X <= 0.0)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 패널 폭이 0 이하다 (%s). 기본 폭을 쓴다."),
			*GetName(), *Layout.Size.ToString());
		Layout.Size.X = DefaultPanelLayout.Size.X > 0.0 ? DefaultPanelLayout.Size.X : 1160.0;
	}
	if (Layout.Size.Y <= 0.0)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 패널 높이가 0 이하다 (%s). 기본 높이를 쓴다."),
			*GetName(), *Layout.Size.ToString());
		Layout.Size.Y = DefaultPanelLayout.Size.Y > 0.0 ? DefaultPanelLayout.Size.Y : 640.0;
	}
	return Layout;
}

void UFF_PrepHudBar::ApplyPanelLayout(EFF_PrepHudTab Tab, const FFF_PrepPanelLayout& Layout)
{
	const TObjectPtr<UFF_PrepPanelBase>* FoundPanel = PanelWidgets.Find(Tab);
	if (!FoundPanel || !*FoundPanel)
	{
		return;
	}

	const EFF_PrepPanelDock Dock = Layout.Dock;
	if (Dock == EFF_PrepPanelDock::Designer)
	{
		// 손으로 잡은 자리를 그대로 쓴다. 여기서 손대면 디자이너 작업이 매번 사라진다.
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
	// 그 축이 늘어나 있으면(min != max) 양쪽 **여백**이고, 점 앵커면 **위치와 크기**다.
	//   늘어난 축  : Left = 왼쪽 여백,  Right  = 오른쪽 여백
	//   점 앵커 축 : Left = X 위치,     Right  = 폭
	// 이걸 헷갈리면 화면 밖으로 나가거나 0 크기가 된다. 방향마다 네 값을 전부 다시 쓴다.
	switch (Dock)
	{
	case EFF_PrepPanelDock::Bottom:
		// 초기 위치만 하단이다. 점 앵커라 이후에는 X/Y 어느 쪽으로든 자유롭게 옮길 수 있다.
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
		CanvasSlot->SetOffsets(FMargin(
			static_cast<float>(Layout.Offset.X),
			-(BarHeight + PanelMargin) + static_cast<float>(Layout.Offset.Y),
			static_cast<float>(Layout.Size.X),
			static_cast<float>(Layout.Size.Y)));
		break;

	case EFF_PrepPanelDock::Center:
		// 화면 중앙. 정중앙에서 얼마나 밀지는 레이아웃의 `Offset` 이 정한다 —
		// 기본값이 바 높이의 절반만큼 위라서 바를 뺀 영역의 중앙에 온다.
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetOffsets(FMargin(
			static_cast<float>(Layout.Offset.X),	// X 위치 (중앙 정렬 기준)
			static_cast<float>(Layout.Offset.Y),	// Y 위치
			static_cast<float>(Layout.Size.X),		// 폭
			static_cast<float>(Layout.Size.Y)));	// 높이
		break;

	case EFF_PrepPanelDock::Right:
		CanvasSlot->SetAnchors(FAnchors(1.f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.5f));
		CanvasSlot->SetOffsets(FMargin(
			-PanelMargin + static_cast<float>(Layout.Offset.X),
			-BarHeight * 0.5f + static_cast<float>(Layout.Offset.Y),
			static_cast<float>(Layout.Size.X),
			static_cast<float>(Layout.Size.Y)));
		break;

	case EFF_PrepPanelDock::Left:
	default:
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 0.5f));
		CanvasSlot->SetOffsets(FMargin(
			PanelMargin + static_cast<float>(Layout.Offset.X),
			-BarHeight * 0.5f + static_cast<float>(Layout.Offset.Y),
			static_cast<float>(Layout.Size.X),
			static_cast<float>(Layout.Size.Y)));
		break;
	}
}

void UFF_PrepHudBar::PaintRowBackground(UBorder* Border, const FLinearColor& Color, float CornerRadius) const
{
	if (!Border || !bPaintRowBackgrounds)
	{
		return;
	}

	// 🔴 `SetBrushColor` 는 **틴트만** 바꾼다. `FSlateBrush` 기본값은 `ImageType = NoImage` 이고
	//    `DrawAs = Image` 라 리소스가 없으면 아무것도 그려지지 않는다. 색을 넣어도 투명한 채로
	//    글자만 월드 위에 떠 있게 된다 — 실제로 그렇게 나왔다 (2026-08-26).
	//    단색을 그리려면 `RoundedBox` 여야 한다. 그것만 텍스처 없이 칠해진다.
	if (Border->Background.GetResourceObject() != nullptr)
	{
		// 디자이너가 텍스처·9-slice 를 지정했으면 그 모양을 유지하고 색만 덮는다.
		// 이 경로는 브러시 틴트를 건드리지 않으므로 `BrushColor` 한 곳만 쓴다.
		Border->SetBrushColor(Color);
		return;
	}

	FSlateBrush Brush = Border->Background;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Color);

	// 기본 `RoundingType` 은 `HalfHeightRadius` 라 그대로 두면 **58px 바가 알약 모양이 된다.**
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.CornerRadii = FVector4(
		CornerRadius, CornerRadius, CornerRadius, CornerRadius);

	Border->SetBrush(Brush);

	// 🔴 **색을 두 곳에 넣으면 두 번 곱해진다.**
	// `SBorder::OnPaint` 가 `브러시 TintColor × 위젯 BrushColor` 로 그린다
	// (`Slate/Private/Widgets/Layout/SBorder.cpp`). 같은 색을 양쪽에 넣으면 선형 공간에서
	// 제곱이 되어 **어두운 색은 거의 검정으로 나온다** — 2026-08-26 에 일차·단계 칩이 그랬다.
	// 색은 브러시 틴트 한 곳에만 두고 여기는 곱셈 항등원(흰색)으로 못 박는다.
	// WBP 에 다른 값이 저장돼 있어도 여기서 덮어야 하므로 생략하지 않는다.
	Border->SetBrushColor(FLinearColor::White);
}

// =============================================================================
// 갱신
// =============================================================================

void UFF_PrepHudBar::RefreshAll()
{
	RefreshFacilityCount();
	RefreshTabVisibility();
	RefreshStatusRow();
	RefreshCta();
}

void UFF_PrepHudBar::RefreshTabVisibility()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Setup;
	const bool bIsFinalDay = ViewModel ? ViewModel->IsFinalDay() : false;

	int32 VisibleTabCount = 0;
	for (UFF_PrepHudTabButton* TabButton : TabButtons)
	{
		if (!TabButton)
		{
			continue;
		}

		const bool bVisible = FFF_PrepHudRules::IsTabVisible(TabButton->GetTab(), Stage, bIsFinalDay);
		TabButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		TabButton->SetSelected(bVisible && IsPanelOpen(TabButton->GetTab()));
		VisibleTabCount += bVisible ? 1 : 0;
	}

	// 🔴 탭이 하나도 없으면 메뉴 행째 접는다.
	// 준비 단계가 아닌 Stage(노을 전환 등)에 이 바가 배선돼 있으면 **빈 흰 띠**만 남아
	// 고장난 화면처럼 보인다. 상태 행은 그대로 둔다 — 금화·일차는 어느 단계에서나 유효하다.
	if (Border_MenuRow)
	{
		Border_MenuRow->SetVisibility(VisibleTabCount > 0
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	// 다른 Stage로 넘어갈 때 UIFlow가 이 HUD를 곧 제거한다. 이때 닫힌 것으로 처리하면
	// 사용자가 닫지 않은 패널이 다음 Setup/MorningCheck에서 사라지므로 상태는 보존하고
	// 이전 화면의 호스트만 즉시 숨긴다.
	if (!FFF_PrepHudRules::IsPrepStage(Stage))
	{
		if (Panel_PanelHost)
		{
			Panel_PanelHost->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// 같은 준비 화면 안에서 실제로 사라진 탭만 닫는다. 복사본을 순회해 ClosePanel이
	// 실제 배열을 바꿔도 안전하게 처리한다.
	const TArray<EFF_PrepHudTab> OpenTabsCopy = OpenTabs;
	for (const EFF_PrepHudTab OpenTab : OpenTabsCopy)
	{
		if (!FFF_PrepHudRules::IsTabVisible(OpenTab, Stage, bIsFinalDay))
		{
			ClosePanel(OpenTab);
		}
	}
	RefreshPanelSelectionVisuals();
}

void UFF_PrepHudBar::RefreshStatusRow()
{
	const UFF_UIFlowSubsystem* Flow = GetFlow();
	SetGroupTextOrHide(Box_FestivalName, Text_FestivalName,
		Flow ? Flow->GetActiveFestivalDisplayName() : FText::GetEmpty());

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	HandleGoldChanged(ViewModel->GetGold());
	HandleArrearsChanged(ViewModel->GetArrears());
	HandleStaffCountChanged(ViewModel->GetTotalStaffCount());
	RefreshDayStageText(ViewModel->GetCurrentStage(), ViewModel->GetCurrentDay());

	// 소유자 API 가 없는 두 칸. 값이 없으면 숨긴다 — 0 을 띄우면 틀린 사실이 된다.
	SetGroupTextOrHide(Box_VisitorCount, Text_VisitorCount,
		(VisitorCount >= 0) ? FText::AsNumber(VisitorCount) : FText::GetEmpty());

	const bool bHasExpectedRange = ExpectedVisitorsMin >= 0 && ExpectedVisitorsMax >= ExpectedVisitorsMin;
	SetGroupTextOrHide(Box_ExpectedVisitors, Text_ExpectedVisitors,
		bHasExpectedRange
			? FText::Format(
				LOCTEXT("PrepStatus_ExpectedVisitors", "예상 {0}~{1}명"),
				FText::AsNumber(ExpectedVisitorsMin),
				FText::AsNumber(ExpectedVisitorsMax))
			: FText::GetEmpty());
}

void UFF_PrepHudBar::RefreshCta()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Setup;
	const int32 Day = ViewModel ? ViewModel->GetCurrentDay() : 1;
	const bool bCanAdvance = ViewModel ? ViewModel->CanSkipCurrentStage() : false;

	const FText* LabelFormat = CtaLabels.Find(Stage);
	const bool bHasLabel = LabelFormat && !LabelFormat->IsEmpty();

	const EFF_PrepCtaState State = FFF_PrepHudRules::ResolveCtaState(
		Stage, bHasLabel, bCanAdvance, FacilityCount, bRequireFacilityForCta);

	// 🔴 **바의 CTA 버튼은 더 이상 띄우지 않는다** (2026-08-27 백종태 결정).
	// 점검 패널의 `개장하기` 와 같은 행동이라 화면에 버튼이 둘이었다.
	// 진행은 점검 패널이 맡고, 바는 **막힌 이유 문구만** 남긴다 — 그건 탭을 열지 않아도
	// 보여야 하는 정보다 (시설이 없으면 `배치` 탭으로 가야 한다).
	//
	// 판정은 그대로 여기서 한다. 시설 수와 단계 표를 아는 것이 바뿐이라
	// 점검 패널은 `GetCtaState()`·`GetCtaLabelText()` 로 이 결과를 읽어 간다.
	if (Button_Cta)
	{
		Button_Cta->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (State == EFF_PrepCtaState::Hidden)
	{
		if (Box_Cta)
		{
			Box_Cta->SetVisibility(ESlateVisibility::Collapsed);
		}
		SetGroupTextOrHide(nullptr, Text_CtaReason, FText::GetEmpty());
		SetCtaState(State);
		return;
	}

	if (Text_CtaLabel)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Day"), FText::AsNumber(Day));
		Text_CtaLabel->SetText(FText::Format(*LabelFormat, Args));
	}

	// !! 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011)
	FText Reason;
	switch (State)
	{
	case EFF_PrepCtaState::BlockedNoFacility:
		Reason = LOCTEXT("PrepCta_NoFacility", "시설이 하나도 없습니다. `배치` 탭에서 시설을 먼저 놓으세요.");
		break;

	case EFF_PrepCtaState::BlockedStageLocked:
		Reason = LOCTEXT("PrepCta_StageLocked", "아직 넘길 수 없는 단계입니다. 남은 준비 시간이 지나면 자동으로 진행됩니다.");
		break;

	default:
		break;
	}
	SetGroupTextOrHide(nullptr, Text_CtaReason, Reason);

	// 버튼을 접었으니 사유가 없으면 줄째 접는다. 안 그러면 빈 띠가 남는다.
	if (Box_Cta)
	{
		Box_Cta->SetVisibility(Reason.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	// 같은 상태를 매 갱신마다 찍으면 로그가 같은 줄로 가득 찬다. 바뀔 때만 남긴다.
	if (State != LastCtaState && State == EFF_PrepCtaState::BlockedStageLocked)
	{
		// 표가 이 단계를 못 넘기게 해 뒀다는 뜻이다. 의도한 것이 아니면 여기가 원인이다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] `%s` 단계를 넘길 수 없어 개장을 막았다. "
				 "의도한 것이 아니면 `DT_DaySequence` 의 Day%d_%s 행에서 bSkippable 을 확인한다."),
			*GetName(), *UEnum::GetValueAsString(Stage), Day, *UEnum::GetValueAsString(Stage));
	}

	SetCtaState(State);
}

void UFF_PrepHudBar::SetCtaState(EFF_PrepCtaState NewState)
{
	if (NewState == LastCtaState)
	{
		return;
	}

	LastCtaState = NewState;

	// 점검 패널의 `개장하기` 가 이 판정을 그대로 쓴다. 바가 시설 수를 다시 세는 순간
	// 알려 주지 않으면 패널 버튼이 예전 상태로 남는다.
	OnCtaStateChanged.Broadcast();
}

FText UFF_PrepHudBar::GetCtaLabelText() const
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Setup;

	const FText* LabelFormat = CtaLabels.Find(Stage);
	if (!LabelFormat || LabelFormat->IsEmpty())
	{
		return FText::GetEmpty();
	}

	// `{Day}` 는 현재 일차로 치환된다. 문구에 `Day 1` 을 박으면 2·3일차에 거짓말이 된다.
	FFormatNamedArguments Args;
	Args.Add(TEXT("Day"), FText::AsNumber(ViewModel ? ViewModel->GetCurrentDay() : 1));
	return FText::Format(*LabelFormat, Args);
}

void UFF_PrepHudBar::RefreshFacilityCount()
{
	// !! 못 읽었을 때 0 으로 두지 않는다 !!
	// 읽지 못한 것을 "시설이 없다"로 취급하면 시설을 놓고도 개장할 수 없는 화면이 된다.
	FacilityCount = INDEX_NONE;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UFFZSWorldSubsystem* Zone = World->GetSubsystem<UFFZSWorldSubsystem>();
	if (!Zone)
	{
		return;
	}

	const FFFZSReadModel ReadModel = Zone->BuildReadModel();

	int32 Count = 0;
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		// 철거된 것은 세지 않는다. 계획·검증 상태는 "놓은 것"으로 본다 —
		// 개장 전 준비 단계에서는 아직 Active 가 아닌 시설이 정상이다.
		if (Facility.State != EFFZSFacilityState::Removed)
		{
			++Count;
		}
	}
	FacilityCount = Count;
}

// =============================================================================
// 델리게이트 핸들러
// =============================================================================

void UFF_PrepHudBar::HandleGoldChanged(int32 NewValue)
{
	if (Text_Gold)
	{
		Text_Gold->SetText(FormatGold(NewValue));
	}
}

void UFF_PrepHudBar::HandleArrearsChanged(int32 NewValue)
{
	// 0 이면 칸을 숨긴다. `미납 0` 을 띄우면 없는 문제를 있는 것처럼 만든다.
	SetGroupTextOrHide(Box_Arrears, Text_Arrears,
		(NewValue > 0)
			? FText::Format(LOCTEXT("PrepStatus_Arrears", "미납 {0}"), FormatGold(NewValue))
			: FText::GetEmpty());
}

void UFF_PrepHudBar::HandleStaffCountChanged(int32 NewValue)
{
	// !! 인자를 쓰지 않는다 !! 이 핸들러는 전체 인원과 가용 인원 **두 델리게이트**가 함께 부른다.
	// 인자를 믿으면 가용 인원이 울렸을 때 그 숫자가 전체 자리에 들어간다.
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const int32 Total = ViewModel ? ViewModel->GetTotalStaffCount() : INDEX_NONE;
	const int32 Base = ViewModel ? ViewModel->GetBaseStaffCount() : INDEX_NONE;
	const int32 Temporary = ViewModel ? ViewModel->GetTemporaryStaffCount() : INDEX_NONE;
	const int32 Available = ViewModel ? ViewModel->GetAvailableStaffCount() : INDEX_NONE;

	FText StaffText;
	if (Total >= 0)
	{
		// 바는 폭이 좁아 내역까지 다 넣으면 잘린다. 여기서는 **Event 에 쓸 수 있는 인원**을
		// 우선한다 — 기본·알바 내역은 인력 패널에서 본다.
		if (Available >= 0)
		{
			StaffText = FText::Format(
				LOCTEXT("PrepStatus_StaffAvailable", "{0}명 중 {1}명 사용 가능"),
				FText::AsNumber(Total), FText::AsNumber(Available));
		}
		else
		{
			StaffText = (Base >= 0 && Temporary >= 0)
				? FText::Format(
					LOCTEXT("PrepStatus_StaffDetailed", "{0}명 (기본 {1} · 알바 {2})"),
					FText::AsNumber(Total), FText::AsNumber(Base), FText::AsNumber(Temporary))
				: FText::Format(LOCTEXT("PrepStatus_Staff", "{0}명"), FText::AsNumber(Total));
		}
	}

	SetGroupTextOrHide(Box_Staff, Text_Staff, StaffText);
}

void UFF_PrepHudBar::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	RefreshDayStageText(NewStage, NewDay);

	// 단계가 바뀌면 탭 구성과 CTA 가 함께 바뀐다. 부분 갱신하면 하나가 빠진다.
	RefreshTabVisibility();
	RefreshCta();
}

void UFF_PrepHudBar::RefreshDayStageText(EFF_FestivalStage Stage, int32 Day)
{
	if (!Text_DayStage)
	{
		return;
	}

	// Stage 표시명은 ViewModel 이 소유한다. 위젯이 enum 을 문자열로 바꾸지 않는다 —
	// 기획 7단계와 코드 8단계가 아직 다르고, 모르는 값이 와도 빈 화면이 되면 안 된다.
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	Text_DayStage->SetText(FText::Format(
		LOCTEXT("PrepStatus_DayStage", "{0}일차 · {1}"),
		FText::AsNumber(Day),
		ViewModel ? ViewModel->GetCurrentStageDisplayText() : FText::GetEmpty()));
}

void UFF_PrepHudBar::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	// 시설을 놓거나 철거하면 개장 조건이 바뀐다. 이 구독이 없으면 시설을 놓아도
	// 버튼이 잠긴 채로 남고, 화면만 보고는 이유를 알 수 없다.
	RefreshFacilityCount();
	RefreshCta();
}

void UFF_PrepHudBar::HandleCtaClicked()
{
	RequestStageAdvance();
}

void UFF_PrepHudBar::RequestStageAdvance()
{
	UFF_UIFlowSubsystem* Flow = GetFlow();
	if (!Flow)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] UI 흐름 서브시스템이 없어 단계를 넘길 수 없다."), *GetName());
		return;
	}

	// !! 낙관적 갱신을 하지 않는다 !!
	// 요청만 보내고 화면은 `OnStageViewChanged` 결과 이벤트로 갱신된다.
	// 어느 단계를 어디로 보낼지는 Stage 시스템과 `DT_DaySequence` 가 소유한다 (UI 규칙 2).
	Flow->RequestAdvanceStage();
}

void UFF_PrepHudBar::Debug_SetPlaceholders(int32 InVisitorCount, int32 InExpectedVisitorsMin, int32 InExpectedVisitorsMax)
{
	VisitorCount = InVisitorCount;
	ExpectedVisitorsMin = InExpectedVisitorsMin;
	ExpectedVisitorsMax = InExpectedVisitorsMax;

	RefreshStatusRow();
}

// =============================================================================
// 표시 헬퍼
// =============================================================================

void UFF_PrepHudBar::SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value)
{
	if (TextBlock)
	{
		// 숫자는 항상 채워 둔다. 묶음이 다시 보일 때 이전 값이 남아 있으면 안 된다.
		TextBlock->SetText(Value);
		TextBlock->SetVisibility(Value.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	if (!Group)
	{
		// 묶음을 안 만든 WBP 도 그대로 동작한다 — 숫자 칸만 접힌다.
		return;
	}

	Group->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

FText UFF_PrepHudBar::FormatGold(int32 Amount)
{
	// 천 단위 구분은 로케일이 결정한다. 위젯마다 다른 형식을 만들지 않는다.
	return FText::AsNumber(Amount);
}

#if !UE_BUILD_SHIPPING

// =============================================================================
// 화면을 보면서 자리와 크기를 맞추는 콘솔 명령
//
// === 왜 콘솔인가 ===
// 값을 바꿀 때마다 에디터로 돌아가 WBP 기본값을 고치고 PIE 를 다시 켜면 한 바퀴가 너무 길다.
// 여기서 즉시 반영해 눈으로 고른 뒤, `FF.PrepHud.Layout` 이 찍어 주는 숫자를 WBP 에 적어 확정한다.
//
// ★ **여기서 바꾼 값은 이 판이 돌아가는 동안만 유지된다.** 인스턴스의 표를 고치는 것이라
//   에셋은 그대로다. 확정하지 않으면 다음 실행에서 사라진다.
//
// 화면이 `ECommonInputMode::Menu` 여도 콘솔은 동작한다 — 그래서 레벨 블루프린트 키 이벤트가
// 아니라 콘솔이다 (2026-08-07 에 키 이벤트로 막힌 전례가 있다).
// =============================================================================

namespace FFPrepHudLayoutConsole
{
	UFF_PrepHudBar* Resolve()
	{
		UFF_PrepHudBar* Bar = UFF_PrepHudBar::ActiveBar.Get();
		if (!Bar)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("FF.PrepHud: 준비 단계 HUD 가 화면에 없다. 셋업·홍보·개장 준비 단계에서 실행한다."));
		}
		return Bar;
	}

	void PrintLayout(const TArray<FString>& Args, UWorld* World)
	{
		if (const UFF_PrepHudBar* Bar = Resolve())
		{
			UE_LOG(LogFestaFiestaUI, Display,
				TEXT("FF.PrepHud.Layout: %s"), *Bar->DescribeOpenPanelLayout());
			UE_LOG(LogFestaFiestaUI, Display,
				TEXT("  → WBP_FF_PrepHudBar 의 Panel Layouts 표에서 그 탭 줄에 적으면 확정된다."));
		}
	}

	void SetSize(const TArray<FString>& Args, UWorld* World)
	{
		UFF_PrepHudBar* Bar = Resolve();
		if (!Bar)
		{
			return;
		}

		if (Args.Num() < 2)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("사용법: FF.PrepHud.SetSize <폭> <높이>  — 하단은 높이만, 좌우는 폭만 쓰인다."));
			return;
		}

		FFF_PrepPanelLayout Layout = Bar->GetOpenPanelLayout();
		Layout.Size = FVector2D(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]));
		Bar->SetOpenPanelLayout(Layout);

		UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.PrepHud.SetSize: %s"), *Bar->DescribeOpenPanelLayout());
	}

	void SetOffset(const TArray<FString>& Args, UWorld* World)
	{
		UFF_PrepHudBar* Bar = Resolve();
		if (!Bar)
		{
			return;
		}

		if (Args.Num() < 2)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("사용법: FF.PrepHud.SetOffset <X> <Y>  — 양수 X 는 오른쪽, 양수 Y 는 아래."));
			return;
		}

		FFF_PrepPanelLayout Layout = Bar->GetOpenPanelLayout();
		Layout.Offset = FVector2D(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]));
		Bar->SetOpenPanelLayout(Layout);

		UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.PrepHud.SetOffset: %s"), *Bar->DescribeOpenPanelLayout());
	}

	void SetDock(const TArray<FString>& Args, UWorld* World)
	{
		UFF_PrepHudBar* Bar = Resolve();
		if (!Bar)
		{
			return;
		}

		const UEnum* DockEnum = StaticEnum<EFF_PrepPanelDock>();
		if (Args.Num() < 1 || !DockEnum)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("사용법: FF.PrepHud.SetDock <Center|Bottom|Left|Right|Designer>"));
			return;
		}

		// 이름으로 찾는다. enum 값을 숫자로 받으면 순서가 바뀔 때 조용히 다른 방향이 된다.
		const int64 Value = DockEnum->GetValueByNameString(Args[0]);
		if (Value == INDEX_NONE)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("FF.PrepHud.SetDock: `%s` 는 없는 방향이다. Center|Bottom|Left|Right|Designer"),
				*Args[0]);
			return;
		}

		FFF_PrepPanelLayout Layout = Bar->GetOpenPanelLayout();
		Layout.Dock = static_cast<EFF_PrepPanelDock>(Value);
		Bar->SetOpenPanelLayout(Layout);

		UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.PrepHud.SetDock: %s"), *Bar->DescribeOpenPanelLayout());
	}

	void ResetOffset(const TArray<FString>& Args, UWorld* World)
	{
		if (UFF_PrepHudBar* Bar = Resolve())
		{
			Bar->ResetPanelPosition();
			UE_LOG(LogFestaFiestaUI, Display,
				TEXT("FF.PrepHud.ResetOffset: %s"), *Bar->DescribeOpenPanelLayout());
		}
	}

	// 인자를 받는 자동 등록 클래스는 `WithWorldAndArgs` 계열뿐이다 —
	// `FAutoConsoleCommandWithArgs` 라는 클래스는 없다 (2026-08-26에 그걸로 빌드가 깨졌다).
	FAutoConsoleCommandWithWorldAndArgs LayoutCommand(
		TEXT("FF.PrepHud.Layout"),
		TEXT("지금 열린 준비 단계 패널의 방향·크기·위치를 찍는다. WBP 에 적을 값이다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PrintLayout));

	FAutoConsoleCommandWithWorldAndArgs SetSizeCommand(
		TEXT("FF.PrepHud.SetSize"),
		TEXT("FF.PrepHud.SetSize <폭> <높이> — 열린 패널 크기를 즉시 바꾼다 (이 판에서만)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetSize));

	FAutoConsoleCommandWithWorldAndArgs SetOffsetCommand(
		TEXT("FF.PrepHud.SetOffset"),
		TEXT("FF.PrepHud.SetOffset <X> <Y> — 열린 패널 위치를 즉시 바꾼다 (이 판에서만)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetOffset));

	FAutoConsoleCommandWithWorldAndArgs SetDockCommand(
		TEXT("FF.PrepHud.SetDock"),
		TEXT("FF.PrepHud.SetDock <Center|Bottom|Left|Right|Designer> — 열린 패널 방향을 바꾼다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetDock));

	FAutoConsoleCommandWithWorldAndArgs ResetOffsetCommand(
		TEXT("FF.PrepHud.ResetOffset"),
		TEXT("끌어 옮긴 위치를 0 으로 되돌린다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResetOffset));
}

#endif // !UE_BUILD_SHIPPING

#undef LOCTEXT_NAMESPACE
