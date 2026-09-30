// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Texture2D.h"
#include "FF_FFZSWorldSubsystem.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

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

bool FFF_PrepPanelLayout::TryResolveCenteredPosition(const FGeometry& ViewportGeometry,
	const FGeometry& CanvasGeometry, FVector2D& OutPosition) const
{
	const FVector2D ViewportSize = ViewportGeometry.GetLocalSize();
	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();
	if (ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0 || CanvasSize.X <= 0.0 || CanvasSize.Y <= 0.0)
	{
		return false;
	}

	// Geometry 변환에 DPI와 창의 화면상 원점이 이미 포함된다. ViewportScale로 다시 나누지 않는다.
	const FVector2D CenterAbsolute = ViewportGeometry.LocalToAbsolute(ViewportSize * 0.5);
	// 0.5 앵커를 유지해야 Canvas DesiredSize가 이동량 때문에 늘어나지 않는다.
	// (0,0) 앵커는 |Offset|을 DesiredSize에 더해 Auto 높이 부모에서 재배치 피드백이 생긴다.
	OutPosition = FVector2D(CanvasGeometry.AbsoluteToLocal(CenterAbsolute)) - CanvasSize * 0.5 + Offset;
	return !OutPosition.ContainsNaN();
}

UFF_PrepHudBar::UFF_PrepHudBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GoldTextFormat = LOCTEXT("PrepStatus_GoldFormat", "{Amount} Gold");
	StaffTextFormat = LOCTEXT("PrepStatus_StaffFormat", "{Count}명");
	DayStageTextFormat = LOCTEXT("PrepStatus_DayStageFormat", "{Day}일차 · {Stage}");

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
		{ EFF_PrepPanelDock::Center, FVector2D(760.0, 520.0), FVector2D::ZeroVector });

	// 프로그램 — 슬롯·후보·예상 효과 3열.
	PanelLayouts.Add(EFF_PrepHudTab::Program,
		{ EFF_PrepPanelDock::Center, FVector2D(1160.0, 640.0), FVector2D::ZeroVector });

	// 홍보 — 등급 카드 3장 + 예상 방문객 범위.
	PanelLayouts.Add(EFF_PrepHudTab::Promotion,
		{ EFF_PrepPanelDock::Center, FVector2D(1080.0, 600.0), FVector2D::ZeroVector });

	// 피날레 — 등급 카드를 가로로 5장 놓아야 해서 가장 넓다.
	PanelLayouts.Add(EFF_PrepHudTab::Finale,
		{ EFF_PrepPanelDock::Center, FVector2D(1360.0, 700.0), CenterNudge });

	// 점검 — 개장 직전 마지막 확인이라 카드·요약·견적을 한 번에 봐야 한다.
	PanelLayouts.Add(EFF_PrepHudTab::Check,
		{ EFF_PrepPanelDock::Center, FVector2D(1280.0, 780.0), FVector2D::ZeroVector });

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
	if (bUseDesignerStyle)
	{
		return;
	}

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

	if (Button_Cta)
	{
		Button_Cta->OnClicked.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleCtaClicked);
	}
	// 하단 바에는 사유만 표시하고 진행 버튼은 기존 점검 패널이 소유한다.

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

	UnbindSources();
	Super::NativeDestruct();
}

void UFF_PrepHudBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Selection.bHasSelection)
	{
		const FFF_PrepPanelLayout Layout = ResolveLayout(Selection.Tab);
		if (UsesViewportCenter(Layout))
		{
			// 창 크기·DPI·부모 레이아웃 변화만 위치에 반영하고 Designer의 Size/Offset은 덮어쓰지 않는다.
			ApplyPanelLayout(Layout, &MyGeometry);
		}
	}
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

void UFF_PrepHudBar::HandleRequiredFacilitiesChanged()
{
	RefreshCta();
}

void UFF_PrepHudBar::BindSources()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleGoldChanged);
		CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleStaffCountChanged);
		CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleStageViewChanged);

		// 🔴 초마다 온다 (`ApplyIntMetric` 이 값이 바뀔 때만 쏜다). CTA 사유에 적은
		// 남은 시간이 실제로 줄어들려면 이 구독이 필요하다.
		CachedViewModel->OnRemainingSecondsChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleRemainingSecondsChanged);

	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 금화·스태프·단계가 전부 비어 보인다."),
			*GetName());
	}

	// 시설 수는 Zone 이 소유한다. 시설을 놓거나 철거하면 CTA 가 즉시 따라가야 한다.
	if (UWorld* World = GetWorld())
	{
		if (auto* Required = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>())
		{
			Required->OnRequiredFacilitiesChanged.AddUniqueDynamic(this, &ThisClass::HandleRequiredFacilitiesChanged);
			BoundRequiredFacilities = Required;
		}
		if (UFFZSWorldSubsystem* Zone = World->GetSubsystem<UFFZSWorldSubsystem>())
		{
			Zone->OnWorldChanged.AddUniqueDynamic(this, &UFF_PrepHudBar::HandleWorldChanged);
			Zone->OnWorldSnapshotRestored.AddUniqueDynamic(this, &UFF_PrepHudBar::RefreshAll);
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
	if (auto* Required = BoundRequiredFacilities.Get())
	{
		Required->OnRequiredFacilitiesChanged.RemoveDynamic(this, &ThisClass::HandleRequiredFacilitiesChanged);
	}
	BoundRequiredFacilities.Reset();
	// 해제는 조건부, 정리(`nullptr` 대입)는 무조건. 조기 return 으로 빠져나가지 않는다.
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleGoldChanged);
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleStaffCountChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleStageViewChanged);
		CachedViewModel->OnRemainingSecondsChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleRemainingSecondsChanged);
	}
	CachedViewModel = nullptr;

	if (UFFZSWorldSubsystem* Zone = BoundZoneWorldSubsystem.Get())
	{
		Zone->OnWorldChanged.RemoveDynamic(this, &UFF_PrepHudBar::HandleWorldChanged);
		Zone->OnWorldSnapshotRestored.RemoveDynamic(this, &UFF_PrepHudBar::RefreshAll);
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

	ApplySelection(FFF_PrepHudRules::ToggleTab(Selection, Tab));
}

void UFF_PrepHudBar::CloseCurrentPanel()
{
	ApplySelection(FFF_PrepTabSelection::None());
}

// =============================================================================
// 패널
// =============================================================================

void UFF_PrepHudBar::ApplySelection(const FFF_PrepTabSelection& NewSelection)
{
	if (NewSelection == Selection)
	{
		return;
	}

	// 1) 열려 있던 것을 닫는다. **패널이 스스로 닫지 않는다** — 여기가 유일한 경로다.
	if (Selection.bHasSelection)
	{
		if (TObjectPtr<UFF_PrepPanelBase>* Previous = PanelWidgets.Find(Selection.Tab))
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

		if (UFF_PrepPanelBase* Panel = EnsurePanel(Selection.Tab))
		{
			Panel->SetVisibility(ESlateVisibility::Visible);
			Panel->HandlePanelOpened();

			// 움직임 감소가 켜져 있으면 재생하지 않는다. 판정은 접근성 서브시스템이 소유한다.
			const UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this);
			const float MotionDuration = Accessibility
				? Accessibility->GetMotionDuration(EFF_UIMotionSpeed::Base)
				: 0.f;

			if (Anim_PanelOpen && MotionDuration > 0.f)
			{
				PlayAnimation(Anim_PanelOpen);
			}
		}
		else
		{
			Selection = FFF_PrepTabSelection::None();
		}
	}

	// 3) 호스트는 `SelfHitTestInvisible` 로 띄운다. `Visible` 이면 패널 바깥의 빈 영역까지
	//    월드 클릭을 먹는다. 자식(패널)은 그대로 클릭을 받는다.
	RefreshPanelHostVisibility();

	// 4) 탭 선택 표시. 색만 바꾸지 않고 배경 브러시가 함께 바뀐다 (AT-011).
	for (UFF_PrepHudTabButton* TabButton : TabButtons)
	{
		if (TabButton)
		{
			TabButton->SetSelected(Selection.IsSelected(TabButton->GetTab()));
		}
	}

	OnTabSelectionChanged.Broadcast(Selection);
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

	// The shipped Build panel moved to the construction catalog. Old WBP CDOs
	// still contain the original FString folder/name convention, so resolve that
	// one case explicitly. Custom mappings above and custom folders still win.
	if (Tab == EFF_PrepHudTab::Build &&
		FFBuildUIAssetPaths::UsesOriginalPanelConvention(PanelClassFolder, PanelClassNameFormat))
	{
		if (UClass* BuildClass = LoadObject<UClass>(nullptr, FFBuildUIAssetPaths::BuildPanelClass))
		{
			return BuildClass;
		}
		UE_LOG(LogFestaFiestaUI, Error, TEXT("[%s] Construction catalog class is missing (%s)."),
			*GetName(), FFBuildUIAssetPaths::BuildPanelClass);
		bOutIsPlaceholder = true;
		return PlaceholderPanelClass.IsNull() ? nullptr : PlaceholderPanelClass.LoadSynchronous();
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

	if (!Panel_PanelHost)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] Panel_PanelHost 가 없어 패널을 띄울 자리가 없다. WBP 에 컨테이너를 넣는다."),
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

	// 🔴 `UOverlaySlot` 기본값은 `HAlign_Left` + `VAlign_Top` 이다.
	//    그대로 두면 패널이 호스트를 채우지 않고 **내용 크기로 줄어들어**
	//    레이아웃 표의 `Size` 를 아무리 키워도 화면이 안 커진다 (2026-08-26 실제 발생).
	//    호스트가 다른 컨테이너면 그쪽 슬롯 타입에 맞춰 디자이너가 맞춘다.
	if (UOverlaySlot* HostSlot = Cast<UOverlaySlot>(Panel_PanelHost->AddChild(Panel)))
	{
		HostSlot->SetHorizontalAlignment(HAlign_Fill);
		HostSlot->SetVerticalAlignment(VAlign_Fill);
	}
	PanelWidgets.Add(Tab, Panel);
	return Panel;
}

void UFF_PrepHudBar::NudgePanelPosition(FVector2D SlateDelta)
{
	if (!IsPanelDragAllowed() || SlateDelta.IsNearlyZero() || !Selection.bHasSelection)
	{
		return;
	}

	// 늘어난 축은 화면이 정하므로 그 방향의 이동은 버린다. 안 버리면 여백 값이 커져
	// 패널이 반대쪽으로 밀린다.
	FFF_PrepPanelLayout Layout = ResolveLayout(Selection.Tab);
	switch (Layout.Dock)
	{
	case EFF_PrepPanelDock::Bottom:
		Layout.Offset.Y += SlateDelta.Y;
		break;

	case EFF_PrepPanelDock::Left:
	case EFF_PrepPanelDock::Right:
		Layout.Offset.X += SlateDelta.X;
		break;

	default:
		Layout.Offset += SlateDelta;
		break;
	}

	// 인스턴스의 표를 고친다. 에셋 기본값은 그대로다 — 확정은 WBP 에 적어서 한다.
	PanelLayouts.Add(Selection.Tab, Layout);
	ApplyPanelLayout(Layout);
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
	ApplyPanelLayout(Layout);
}

bool UFF_PrepHudBar::IsPanelDragAllowed() const
{
	if (!bAllowPanelDrag || !Panel_PanelHost || !Selection.bHasSelection)
	{
		return false;
	}

	// `디자이너 지정` 은 코드가 슬롯을 아예 만지지 않는 모드다. 거기서 드래그를 허용하면
	// 값만 쌓이고 화면은 안 움직여 "끌리지 않는다"로 보인다.
	return ResolveLayout(Selection.Tab).Dock != EFF_PrepPanelDock::Designer;
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
	ApplyPanelLayout(ResolveLayout(Selection.Tab));
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

	// 방향에 따라 실제로 쓰이는 성분만 검사한다. 하단 도킹의 `Size.X` 처럼 안 쓰는 값이
	// 0 인 것은 정상이라, 그것까지 막으면 기본값으로 되돌아가 버린다.
	const bool bNeedsWidth = Layout.Dock == EFF_PrepPanelDock::Center
		|| Layout.Dock == EFF_PrepPanelDock::Left
		|| Layout.Dock == EFF_PrepPanelDock::Right;
	const bool bNeedsHeight = Layout.Dock == EFF_PrepPanelDock::Center
		|| Layout.Dock == EFF_PrepPanelDock::Bottom;

	if ((bNeedsWidth && Layout.Size.X <= 0.0) || (bNeedsHeight && Layout.Size.Y <= 0.0))
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 패널 크기가 0 이하다 (%s). 패널이 사라지므로 기본 레이아웃 크기를 쓴다."),
			*GetName(), *Layout.Size.ToString());
		Layout.Size = DefaultPanelLayout.Size;
	}
	return Layout;
}

float UFF_PrepHudBar::GetBottomStatusRowHeight() const
{
	if (SizeBox_StatusRow)
	{
		return SizeBox_StatusRow->IsHeightOverride()
			? SizeBox_StatusRow->GetHeightOverride()
			: SizeBox_StatusRow->GetDesiredSize().Y;
	}
	return StatusRowHeight;
}

float UFF_PrepHudBar::GetBottomBarHeight() const
{
	if (SizeBox_StatusRow && SizeBox_MenuRow)
	{
		const float MenuHeight = Border_MenuRow && Border_MenuRow->GetVisibility() == ESlateVisibility::Collapsed
			? 0.f : (SizeBox_MenuRow->IsHeightOverride()
				? SizeBox_MenuRow->GetHeightOverride() : SizeBox_MenuRow->GetDesiredSize().Y);
		return GetBottomStatusRowHeight() + MenuHeight;
	}
	return BarHeight;
}

bool UFF_PrepHudBar::UsesViewportCenter(const FFF_PrepPanelLayout& Layout) const
{
	if (!Selection.bHasSelection || Layout.Dock != EFF_PrepPanelDock::Center)
	{
		return false;
	}
	return Selection.Tab == EFF_PrepHudTab::Program || Selection.Tab == EFF_PrepHudTab::Staff
		|| Selection.Tab == EFF_PrepHudTab::Promotion || Selection.Tab == EFF_PrepHudTab::Check;
}

void UFF_PrepHudBar::RefreshPanelHostVisibility()
{
	if (Panel_PanelHost)
	{
		Panel_PanelHost->SetVisibility(!Selection.bHasSelection ? ESlateVisibility::Collapsed
			: bAwaitingCenteredLayout ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UFF_PrepHudBar::ApplyPanelLayout(const FFF_PrepPanelLayout& Layout, const FGeometry* CurrentHudGeometry)
{
	bAwaitingCenteredLayout = false;
	if (!Panel_PanelHost)
	{
		return;
	}

	const EFF_PrepPanelDock Dock = Layout.Dock;
	if (Dock == EFF_PrepPanelDock::Designer)
	{
		// 손으로 잡은 자리를 그대로 쓴다. 여기서 손대면 디자이너 작업이 매번 사라진다.
		RefreshPanelHostVisibility();
		return;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Panel_PanelHost->Slot);
	if (!CanvasSlot)
	{
		// 캔버스가 아니면 위치는 디자이너가 소유한다. 코드가 짐작해서 옮기지 않는다.
		UE_LOG(LogFestaFiestaUI, Verbose,
			TEXT("[%s] Panel_PanelHost 가 CanvasPanel 자식이 아니라 도킹을 코드가 정하지 않는다."),
			*GetName());
		RefreshPanelHostVisibility();
		return;
	}

	if (UsesViewportCenter(Layout))
	{
		const UWidget* Canvas = Panel_PanelHost->GetParent();
		const FGeometry HudGeometry = CurrentHudGeometry ? *CurrentHudGeometry : GetCachedGeometry();
		// 루트 Canvas는 HUD와 같은 공간이다. Tick의 최신 geometry를 써 resize 직전 캐시를 피한다.
		const FGeometry CanvasGeometry = WidgetTree && WidgetTree->RootWidget == Canvas
			? HudGeometry : Canvas->GetCachedGeometry();
		const UWorld* World = GetWorld();
		// 게임에서는 실제 viewport만 쓴다. 에디터의 독립 WidgetRenderer는 HUD 표면을 기준으로 검증한다.
		const FGeometry ViewportGeometry = World && World->IsGameWorld()
			? UWidgetLayoutLibrary::GetViewportWidgetGeometry(this) : HudGeometry;
		FVector2D Position;
		bAwaitingCenteredLayout = !Layout.TryResolveCenteredPosition(ViewportGeometry, CanvasGeometry, Position);
		if (!bAwaitingCenteredLayout)
		{
			const FAnchors Anchors(0.5f, 0.5f);
			const FVector2D Alignment(0.5, 0.5);
			const FMargin Offsets(Position.X, Position.Y, Layout.Size.X, Layout.Size.Y);
			if (CanvasSlot->GetAutoSize()) { CanvasSlot->SetAutoSize(false); }
			if (CanvasSlot->GetAnchors() != Anchors) { CanvasSlot->SetAnchors(Anchors); }
			if (CanvasSlot->GetAlignment() != Alignment) { CanvasSlot->SetAlignment(Alignment); }
			if (CanvasSlot->GetOffsets() != Offsets) { CanvasSlot->SetOffsets(Offsets); }
		}
		RefreshPanelHostVisibility();
		return;
	}
	RefreshPanelHostVisibility();

	// === CanvasPanelSlot 의 Offsets 는 앵커에 따라 뜻이 달라진다 ===
	// 그 축이 늘어나 있으면(min != max) 양쪽 **여백**이고, 점 앵커면 **위치와 크기**다.
	//   늘어난 축  : Left = 왼쪽 여백,  Right  = 오른쪽 여백
	//   점 앵커 축 : Left = X 위치,     Right  = 폭
	// 이걸 헷갈리면 화면 밖으로 나가거나 0 크기가 된다. 방향마다 네 값을 전부 다시 쓴다.
	switch (Dock)
	{
	case EFF_PrepPanelDock::Bottom:
		// 가로로 꽉 채우고, 아래 끝을 **탭 줄 밑으로** 밀어 넣는다.
		CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 1.f));
		// 가로로 늘어난 축이라 X 는 여백이다 — `Size.X` 와 `Offset.X` 는 쓰이지 않는다.
		CanvasSlot->SetOffsets(FMargin(
			PanelMargin,													// 왼쪽 여백

			// 🔴 예전에는 `-(BarHeight + PanelMargin)` = 70px 를 띄웠다. 그래서 배치 패널만
			//    바 위에 **동동 떠 있었고**, 가운데 패널들(`CenterNudge` = -29)과 아래 끝이
			//    어긋났다 (2026-09-03 백종태 지적). 상태 행 높이만 띄워 다른 패널과 맞춘다 —
			//    패널 아래 끝이 탭 줄 밑으로 들어가서 버튼이 패널에 얹힌 것처럼 보인다.
			-GetBottomStatusRowHeight() + static_cast<float>(Layout.Offset.Y),

			PanelMargin,													// 오른쪽 여백
			static_cast<float>(Layout.Size.Y)));							// 높이
		break;

	case EFF_PrepPanelDock::Center:
		// 화면 중앙. 정중앙에서 얼마나 밀지는 레이아웃의 `Offset` 이 정한다 —
		// 기본값이 바 높이의 절반만큼 위라서 바를 뺀 영역의 중앙에 온다.
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetOffsets(FMargin(
			static_cast<float>(Layout.Offset.X),	// X 위치 (중앙 정렬 기준)
			static_cast<float>(Layout.Offset.Y) - (SizeBox_StatusRow ? GetBottomStatusRowHeight() - StatusRowHeight : 0.f),
			static_cast<float>(Layout.Size.X),		// 폭
			static_cast<float>(Layout.Size.Y)));	// 높이
		break;

	case EFF_PrepPanelDock::Right:
		// 세로로 꽉 채우고 오른쪽에 붙인다. 아래는 바 높이만큼 비운다.
		CanvasSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
		// 세로로 늘어난 축이라 Y 는 여백이다 — `Size.Y` 와 `Offset.Y` 는 쓰이지 않는다.
		CanvasSlot->SetOffsets(FMargin(
			-PanelMargin + static_cast<float>(Layout.Offset.X),	// X 위치 (오른쪽 정렬이라 음수)
			PanelMargin,										// 위쪽 여백
			static_cast<float>(Layout.Size.X),					// 폭
			GetBottomBarHeight() + PanelMargin));
		break;

	case EFF_PrepPanelDock::Left:
	default:
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
		CanvasSlot->SetOffsets(FMargin(
			PanelMargin + static_cast<float>(Layout.Offset.X),
			PanelMargin,
			static_cast<float>(Layout.Size.X),
			GetBottomBarHeight() + PanelMargin));
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

	// 사라진 탭의 패널이 남아 있으면 닫을 수단이 화면에서 없어진다.
	ApplySelection(FFF_PrepHudRules::ClampSelectionToVisible(Selection, Stage, bIsFinalDay));
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
		SetCtaState(State, FText::GetEmpty());
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
	{
		// 🔴 **남은 시간을 숫자로 적는다.** `남은 준비 시간이 지나면` 만 두면 언제까지인지
		// 알 수 없어서 멈춘 것처럼 보인다 (2026-09-02 백종태 지적). 준비 단계도 시계가
		// 흐르고 시간이 다 되면 자동으로 넘어간다.
		const int32 RemainingSeconds = ViewModel
			? FMath::CeilToInt(ViewModel->GetRemainingSeconds())
			: 0;

		Reason = (RemainingSeconds > 0)
			? FText::Format(
				LOCTEXT("PrepCta_StageLockedWithTime",
					"아직 넘길 수 없는 단계입니다. {0}초 뒤 자동으로 진행됩니다."),
				FText::AsNumber(RemainingSeconds))
			: LOCTEXT("PrepCta_StageLocked",
				"아직 넘길 수 없는 단계입니다. 남은 준비 시간이 지나면 자동으로 진행됩니다.");
		break;
	}

	default:
		break;
	}
	// 시간 경과로 해결되지 않는 설치 조건은 카운트다운 안내보다 우선한다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (auto* StageSystem = GI->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			const FText RequiredReason = StageSystem->GetStageAdvanceBlockingReason();
			if (!RequiredReason.IsEmpty()) { Reason = RequiredReason; }
		}
	}
	SetGroupTextOrHide(nullptr, Text_CtaReason, Reason);
	if (Text_CtaReason)
	{
		Text_CtaReason->SetToolTipText(Reason);
	}

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

		// 디버그(매일 재검사 원인 조사)
		if (const UWorld* World = GetWorld())
		{
			if (const auto* Required = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>())
			{
				UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 필수 시설 실제 상태: %s"),
					*GetName(), *Required->DebugDescribeRequiredFacilities());
			}
		}
	}

	SetCtaState(State, Reason);
}

void UFF_PrepHudBar::SetCtaState(EFF_PrepCtaState NewState, const FText& NewReason)
{
	if (NewState == LastCtaState && LastCtaReason.EqualTo(NewReason))
	{
		return;
	}

	LastCtaState = NewState;
	LastCtaReason = NewReason;

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

	// Count through the registry without copying and sorting every facility and queue.
	// Planned/validated facilities still count during preparation; removed ones do not.
	FacilityCount = Zone->GetPlacedFacilityCount();
}

// =============================================================================
// 델리게이트 핸들러
// =============================================================================

void UFF_PrepHudBar::HandleGoldChanged(int32 NewValue)
{
	if (Text_Gold)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Amount"), FormatGold(NewValue));
		Text_Gold->SetText(FText::Format(GoldTextFormat, Args));
	}
}

void UFF_PrepHudBar::HandleStaffCountChanged(int32 NewValue)
{
	// 총원은 ViewModel이 소유한다. 세부 인원은 인력 패널에서 보여준다.
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const int32 Total = ViewModel ? ViewModel->GetTotalStaffCount() : INDEX_NONE;

	FText StaffText;
	if (Total >= 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), FText::AsNumber(Total));
		StaffText = FText::Format(StaffTextFormat, Args);
	}

	SetGroupTextOrHide(Box_Staff, Text_Staff, StaffText);
}

void UFF_PrepHudBar::HandleRemainingSecondsChanged(int32 NewValue)
{
	// CTA 사유에만 쓰인다. 다른 값은 각자 델리게이트로 갱신된다.
	RefreshCta();
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
	FFormatNamedArguments Args;
	Args.Add(TEXT("Day"), FText::AsNumber(Day));
	Args.Add(TEXT("Stage"), ViewModel ? ViewModel->GetCurrentStageDisplayText() : FText::GetEmpty());
	Text_DayStage->SetText(FText::Format(DayStageTextFormat, Args));
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
