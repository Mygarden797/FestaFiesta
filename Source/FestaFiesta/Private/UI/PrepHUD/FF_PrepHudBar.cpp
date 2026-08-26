// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/FF_PrepHudBar.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "FF_FFZSWorldSubsystem.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"
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
		OutDisplayName = TabEnum->GetDisplayNameTextByIndex(Index);
	}
}

UFF_PrepHudBar::UFF_PrepHudBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// === CTA 문구 기본값 ===
	// `{Day}` 는 현재 일차로 치환된다. 문구에 `Day 1` 을 박으면 2·3일차에 거짓말이 된다.
	//
	// 홍보(PromotionPreview)에는 CTA 가 없다. 홍보는 이미 하단 탭에 있고, 이 단계는 남은 시간이
	// 지나면 표(`DT_DaySequence`)대로 자동으로 넘어간다.
	CtaLabels.Add(EFF_FestivalStage::Setup,
		LOCTEXT("PrepCta_Setup", "확정하고 Day {Day} 시작"));
	CtaLabels.Add(EFF_FestivalStage::MorningCheck,
		LOCTEXT("PrepCta_MorningCheck", "개장하기"));

	// === 도킹 예외 ===
	// **월드를 만지는 탭만** 하단으로 연다. 배치는 지면이 보여야 시설을 놓을 수 있다.
	// 나머지는 `PanelDock`(중앙)을 쓴다 — 표·카드를 읽는 동안 월드는 필요 없다.
	PanelDockOverrides.Add(EFF_PrepHudTab::Build, EFF_PrepPanelDock::Bottom);

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
	PaintRowBackground(Border_MenuRow, Palette->InGame.Paper);
	PaintRowBackground(Border_StatusRow, Palette->InGame.StatusBar);

	// 일차·단계는 밝은 상태 행 위에서 이 한 줄만 도드라져야 한다. 어두운 판 + 흰 글씨다.
	PaintRowBackground(Border_DayStage, Palette->InGame.TealDeep);
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

void UFF_PrepHudBar::NativeConstruct()
{
	Super::NativeConstruct();

	// 구독은 여기다. `NativeOnInitialized`(인스턴스당 1회)에 두면 위젯이 재사용될 때
	// 두 번째 진입부터 아무 값도 안 들어온다 (2026-08-24에 화면 5종에서 실제로 발생).
	BindSources();
	RefreshAll();
}

void UFF_PrepHudBar::NativeDestruct()
{
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
		// 자리를 먼저 맞춘 뒤 패널을 넣는다. 순서가 반대면 이전 탭의 방향으로 한 프레임 그려진다.
		ApplyPanelDock(ResolveDock(Selection.Tab));

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
	if (Panel_PanelHost)
	{
		Panel_PanelHost->SetVisibility(Selection.bHasSelection
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

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
	Panel_PanelHost->AddChild(Panel);
	PanelWidgets.Add(Tab, Panel);
	return Panel;
}

EFF_PrepPanelDock UFF_PrepHudBar::ResolveDock(EFF_PrepHudTab Tab) const
{
	if (const EFF_PrepPanelDock* Override = PanelDockOverrides.Find(Tab))
	{
		return *Override;
	}
	return PanelDock;
}

void UFF_PrepHudBar::ApplyPanelDock(EFF_PrepPanelDock Dock)
{
	if (!Panel_PanelHost)
	{
		return;
	}

	if (Dock == EFF_PrepPanelDock::Designer)
	{
		// 손으로 잡은 자리를 그대로 쓴다. 여기서 손대면 디자이너 작업이 매번 사라진다.
		return;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Panel_PanelHost->Slot);
	if (!CanvasSlot)
	{
		// 캔버스가 아니면 위치는 디자이너가 소유한다. 코드가 짐작해서 옮기지 않는다.
		UE_LOG(LogFestaFiestaUI, Verbose,
			TEXT("[%s] Panel_PanelHost 가 CanvasPanel 자식이 아니라 도킹을 코드가 정하지 않는다."),
			*GetName());
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
		// 가로로 꽉 채우고, 아래쪽 점 앵커에서 바 높이만큼 띄운다.
		CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(
			PanelMargin,					// 왼쪽 여백
			-(BarHeight + PanelMargin),		// 바 위로 띄운다 (아래쪽 정렬이라 음수)
			PanelMargin,					// 오른쪽 여백
			PanelHeightWhenBottomDocked));	// 높이
		break;

	case EFF_PrepPanelDock::Center:
		// 화면 중앙. 정중앙에서 얼마나 밀지는 `PanelCenterOffset` 이 정한다 —
		// 기본값이 바 높이의 절반만큼 위라서 바를 뺀 영역의 중앙에 온다.
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetOffsets(FMargin(
			static_cast<float>(PanelCenterOffset.X),	// X 위치 (중앙 정렬 기준)
			static_cast<float>(PanelCenterOffset.Y),	// Y 위치
			PanelCenterSize.X,							// 폭
			PanelCenterSize.Y));						// 높이
		break;

	case EFF_PrepPanelDock::Right:
		// 세로로 꽉 채우고 오른쪽에 붙인다. 아래는 바 높이만큼 비운다.
		CanvasSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
		CanvasSlot->SetOffsets(FMargin(
			-PanelMargin,					// X 위치 (오른쪽 정렬이라 음수)
			PanelMargin,					// 위쪽 여백
			PanelWidthWhenSideDocked,		// 폭
			BarHeight + PanelMargin));		// 아래쪽 여백
		break;

	case EFF_PrepPanelDock::Left:
	default:
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
		CanvasSlot->SetOffsets(FMargin(
			PanelMargin,
			PanelMargin,
			PanelWidthWhenSideDocked,
			BarHeight + PanelMargin));
		break;
	}
}

void UFF_PrepHudBar::PaintRowBackground(UBorder* Border, const FLinearColor& Color) const
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
		RowCornerRadius, RowCornerRadius, RowCornerRadius, RowCornerRadius);

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

	if (State == EFF_PrepCtaState::Hidden)
	{
		if (Box_Cta)
		{
			Box_Cta->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (Button_Cta)
		{
			Button_Cta->SetVisibility(ESlateVisibility::Collapsed);
		}
		SetGroupTextOrHide(nullptr, Text_CtaReason, FText::GetEmpty());
		LastCtaState = State;
		return;
	}

	if (Box_Cta)
	{
		Box_Cta->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (Button_Cta)
	{
		Button_Cta->SetVisibility(ESlateVisibility::Visible);
		Button_Cta->SetIsEnabled(State == EFF_PrepCtaState::Ready);
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

	// 같은 상태를 매 갱신마다 찍으면 로그가 같은 줄로 가득 찬다. 바뀔 때만 남긴다.
	if (State != LastCtaState)
	{
		if (State == EFF_PrepCtaState::BlockedStageLocked)
		{
			// 표가 이 단계를 못 넘기게 해 뒀다는 뜻이다. 의도한 것이 아니면 여기가 원인이다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("[%s] `%s` 단계를 넘길 수 없어 CTA 를 막았다. "
					 "의도한 것이 아니면 `DT_DaySequence` 의 Day%d_%s 행에서 bSkippable 을 확인한다."),
				*GetName(), *UEnum::GetValueAsString(Stage), Day, *UEnum::GetValueAsString(Stage));
		}
		LastCtaState = State;
	}
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
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const int32 Base = ViewModel ? ViewModel->GetBaseStaffCount() : INDEX_NONE;
	const int32 Temporary = ViewModel ? ViewModel->GetTemporaryStaffCount() : INDEX_NONE;

	FText StaffText;
	if (NewValue >= 0)
	{
		StaffText = (Base >= 0 && Temporary >= 0)
			? FText::Format(
				LOCTEXT("PrepStatus_StaffDetailed", "{0}명 (기본 {1} · 알바 {2})"),
				FText::AsNumber(NewValue), FText::AsNumber(Base), FText::AsNumber(Temporary))
			: FText::Format(LOCTEXT("PrepStatus_Staff", "{0}명"), FText::AsNumber(NewValue));
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

#undef LOCTEXT_NAMESPACE
