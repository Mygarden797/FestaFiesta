// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ClosingHUD/FF_ClosingHudScreen.h"

#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/ClosingHUD/FF_ClosingPanelBase.h"
#include "UI/FF_UILog.h"
#include "UI/LiveHUD/FF_LiveTabButton.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaClosingHud"

namespace FFClosingHudLocal
{
	/** enum 이름과 화면 문구를 한 곳에서 낸다. */
	void DescribeTab(EFF_ClosingTab Tab, FString& OutEnumName, FText& OutDisplayName)
	{
		// 🔴 `UMETA(DisplayName)` 을 런타임에 읽지 않는다 — 패키징 빌드에서 영문이 뜬다.
		if (const UEnum* TabEnum = StaticEnum<EFF_ClosingTab>())
		{
			OutEnumName = TabEnum->GetNameStringByValue(static_cast<int64>(Tab));
		}

		switch (Tab)
		{
		case EFF_ClosingTab::Report:	OutDisplayName = LOCTEXT("ClosingTab_Report", "리포트"); break;
		case EFF_ClosingTab::Finale:	OutDisplayName = LOCTEXT("ClosingTab_Finale", "피날레"); break;
		default:						OutDisplayName = FText::FromString(OutEnumName); break;
		}
	}
}

// =============================================================================
// 수명
// =============================================================================

void UFF_ClosingHudScreen::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너에서도 탭 문구가 보이게 여기서 한 번 채운다.
	ForEachTabButton([](EFF_ClosingTab Tab, UFF_LiveTabButton& Button)
	{
		FString EnumName;
		FText DisplayName;
		FFClosingHudLocal::DescribeTab(Tab, EnumName, DisplayName);
		Button.Configure(static_cast<EFF_LiveTab>(0), DisplayName, FSlateBrush());
	});
}

void UFF_ClosingHudScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 🔴 탭 버튼은 `EFF_LiveTab` 을 실어 나르게 되어 있다. 결산 탭 값을 그대로 넣으면
	// 라이브 탭 값으로 해석되므로, **버튼 자체가 아니라 순서로 구분한다** —
	// `Configure` 에 넣는 id 와 클릭에서 읽는 id 가 같기만 하면 된다.
	int32 TabId = 0;
	ForEachTabButton([this, &TabId](EFF_ClosingTab Tab, UFF_LiveTabButton& Button)
	{
		FString EnumName;
		FText DisplayName;
		FFClosingHudLocal::DescribeTab(Tab, EnumName, DisplayName);

		Button.Configure(static_cast<EFF_LiveTab>(static_cast<uint8>(Tab)), DisplayName, FSlateBrush());
		Button.OnHudTabButtonClicked.BindUObject(this, &UFF_ClosingHudScreen::HandleTabButtonClicked);
		++TabId;
	});

	if (Button_Cta)
	{
		Button_Cta->OnClicked.AddUniqueDynamic(this, &UFF_ClosingHudScreen::HandleCtaClicked);
	}
}

void UFF_ClosingHudScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// 🔴 전체 화면을 덮는 HUD 가 `Visible` 이면 월드 클릭을 전부 먹는다.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	BindViewModel();
	RefreshAll();
}

void UFF_ClosingHudScreen::NativeDestruct()
{
	UnbindViewModel();
	Super::NativeDestruct();
}

// =============================================================================
// 구독
// =============================================================================

UFF_FestivalViewModelSubsystem* UFF_ClosingHudScreen::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_ClosingHudScreen::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 결산 값이 전부 비어 보인다."), *GetName());
		return;
	}

	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_ClosingHudScreen::HandleStageViewChanged);
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_ClosingHudScreen::HandleGoldChanged);

	// 피날레 등급이 바뀌면 **탭이 생기거나 사라진다.** 값만 바뀌는 것이 아니다.
	CachedViewModel->OnPurchasedFinaleGradeChanged.AddUniqueDynamic(this, &UFF_ClosingHudScreen::HandleFinaleGradeChanged);
}

void UFF_ClosingHudScreen::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_ClosingHudScreen::HandleStageViewChanged);
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_ClosingHudScreen::HandleGoldChanged);
		CachedViewModel->OnPurchasedFinaleGradeChanged.RemoveDynamic(this, &UFF_ClosingHudScreen::HandleFinaleGradeChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_ClosingHudScreen::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	// 단계가 바뀌면 안 보이게 된 탭의 선택을 푼다 — 탭은 사라졌는데 패널만 남지 않게.
	ApplySelection(FFF_ClosingHudRules::ClampSelectionToVisible(
		Selection, NewStage, HasPurchasedFinale()));

	RefreshAll();
}

void UFF_ClosingHudScreen::HandleGoldChanged(int32 NewValue)
{
	RefreshStatusRow();
}

void UFF_ClosingHudScreen::HandleFinaleGradeChanged(EFF_FinaleGrade NewGrade)
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Closing;

	ApplySelection(FFF_ClosingHudRules::ClampSelectionToVisible(
		Selection, Stage, HasPurchasedFinale()));

	RefreshTabs();
}

bool UFF_ClosingHudScreen::HasPurchasedFinale() const
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	return ViewModel && ViewModel->GetPurchasedFinaleGrade() != EFF_FinaleGrade::None;
}

// =============================================================================
// 탭
// =============================================================================

void UFF_ClosingHudScreen::ForEachTabButton(
	TFunctionRef<void(EFF_ClosingTab, UFF_LiveTabButton&)> Visitor) const
{
	const TPair<EFF_ClosingTab, UFF_LiveTabButton*> Bindings[] = {
		{ EFF_ClosingTab::Report,	Tab_Report },
		{ EFF_ClosingTab::Finale,	Tab_Finale }
	};

	for (const TPair<EFF_ClosingTab, UFF_LiveTabButton*>& Binding : Bindings)
	{
		if (Binding.Value)
		{
			Visitor(Binding.Key, *Binding.Value);
		}
	}
}

void UFF_ClosingHudScreen::HandleTabButtonClicked(int32 TabId)
{
	ToggleTab(static_cast<EFF_ClosingTab>(TabId));
}

void UFF_ClosingHudScreen::ToggleTab(EFF_ClosingTab Tab)
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Closing;

	if (!FFF_ClosingHudRules::IsTabVisible(Tab, Stage, HasPurchasedFinale()))
	{
		FString EnumName;
		FText DisplayName;
		FFClosingHudLocal::DescribeTab(Tab, EnumName, DisplayName);

		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 지금 볼 수 없는 탭(%s)을 열려고 했다. 무시한다."), *GetName(), *EnumName);
		return;
	}

	ApplySelection(FFF_ClosingHudRules::ToggleTab(Selection, Tab));
}

void UFF_ClosingHudScreen::CloseCurrentPanel()
{
	ApplySelection(FFF_ClosingTabSelection::None());
}

void UFF_ClosingHudScreen::ApplySelection(const FFF_ClosingTabSelection& NewSelection)
{
	if (NewSelection == Selection)
	{
		return;
	}

	// 1) 열려 있던 것을 닫는다.
	if (Selection.bHasSelection)
	{
		if (TObjectPtr<UFF_ClosingPanelBase>* Previous = PanelWidgets.Find(Selection.Tab))
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
		ApplyPanelLayout();

		if (UFF_ClosingPanelBase* Panel = EnsurePanel(Selection.Tab))
		{
			Panel->SetVisibility(ESlateVisibility::Visible);
			Panel->HandlePanelOpened();
		}
		else
		{
			Selection = FFF_ClosingTabSelection::None();
		}
	}

	// 3) 호스트를 함께 켜고 끈다. 생성기가 `Collapsed` 로 만들어 두기 때문에 패널만
	//    `Visible` 로 바꾸면 접힌 부모 안에 들어가 아무것도 안 보인다.
	if (Panel_PanelHost)
	{
		Panel_PanelHost->SetVisibility(Selection.bHasSelection
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	RefreshTabs();
}

// =============================================================================
// 패널
// =============================================================================

UClass* UFF_ClosingHudScreen::ResolvePanelClass(
	EFF_ClosingTab Tab, const FString& EnumName, bool& bOutIsPlaceholder) const
{
	bOutIsPlaceholder = false;

	// 1) 예외 표. 이름 규칙에서 벗어나는 패널만 여기 들어간다.
	if (const TSoftClassPtr<UFF_ClosingPanelBase>* Mapped = PanelClasses.Find(Tab))
	{
		if (!Mapped->IsNull())
		{
			if (UClass* MappedClass = Mapped->LoadSynchronous())
			{
				return MappedClass;
			}

			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] `%s` 탭의 패널 클래스를 로드하지 못했다 (%s)."),
				*GetName(), *EnumName, *Mapped->ToString());
		}
	}

	// 2) 이름 규칙.
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

		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[%s] `%s` 탭 패널을 아직 만들지 않았다 (%s). 자리표시를 띄운다."),
			*GetName(), *EnumName, *ObjectPath);
	}

	// 3) 자리표시.
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

UFF_ClosingPanelBase* UFF_ClosingHudScreen::EnsurePanel(EFF_ClosingTab Tab)
{
	if (TObjectPtr<UFF_ClosingPanelBase>* Existing = PanelWidgets.Find(Tab))
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
	FFClosingHudLocal::DescribeTab(Tab, EnumName, DisplayName);

	bool bIsPlaceholder = false;
	UClass* PanelClass = ResolvePanelClass(Tab, EnumName, bIsPlaceholder);
	if (!PanelClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] `%s` 탭의 패널 클래스를 못 찾았다. 자리표시도 없다."), *GetName(), *EnumName);
		return nullptr;
	}

	UFF_ClosingPanelBase* Panel = CreateWidget<UFF_ClosingPanelBase>(GetOwningPlayer(), PanelClass);
	if (!Panel)
	{
		return nullptr;
	}

	Panel->InitializePanel(this, Tab, DisplayName);

	if (bIsPlaceholder)
	{
		if (UFF_ClosingPlaceholderPanel* Placeholder = Cast<UFF_ClosingPlaceholderPanel>(Panel))
		{
			Placeholder->SetPlaceholderMessage(FText::Format(
				LOCTEXT("ClosingPanel_Placeholder", "{0} 패널은 아직 만드는 중입니다."), DisplayName));
		}
	}

	Panel->SetVisibility(ESlateVisibility::Collapsed);

	// 🔴 `UOverlaySlot` 기본값은 `Left`·`Top` 이라 그대로 두면 패널이 호스트를 채우지 않고
	//    **내용 크기로 줄어든다** (라이브 HUD 에서 실제로 밟았다).
	if (UOverlaySlot* HostSlot = Cast<UOverlaySlot>(Panel_PanelHost->AddChild(Panel)))
	{
		HostSlot->SetHorizontalAlignment(HAlign_Fill);
		HostSlot->SetVerticalAlignment(VAlign_Fill);
	}

	PanelWidgets.Add(Tab, Panel);
	return Panel;
}

void UFF_ClosingHudScreen::ApplyPanelLayout()
{
	if (!Panel_PanelHost)
	{
		return;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Panel_PanelHost->Slot);
	if (!CanvasSlot)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Panel_PanelHost 가 CanvasPanel 자식이 아니라 자리를 코드가 정하지 않는다."),
			*GetName());
		return;
	}

	// 🔴 **두 축 다 점 앵커 + 명시 크기.** 늘어난 축을 쓰면 크기를 부모가 정하는데
	// HUD 루트가 화면 높이를 못 받아 음수 높이가 된다
	// (`docs/memory/prep-hud-reference.md` 2절).
	//
	// 기준점은 화면 아래 가운데다. 아래 끝이 탭 줄 밑으로 들어가 버튼이 패널에 얹힌
	// 것처럼 보인다 — 준비·라이브와 같은 모양이다.
	CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
	CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
	CanvasSlot->SetOffsets(FMargin(
		0.f, -StatusRowHeight,
		static_cast<float>(PanelSize.X), static_cast<float>(PanelSize.Y)));
}

// =============================================================================
// 갱신
// =============================================================================

void UFF_ClosingHudScreen::RefreshAll()
{
	RefreshTabs();
	RefreshStatusRow();
	RefreshCta();
}

void UFF_ClosingHudScreen::RefreshTabs()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Closing;
	const bool bPurchased = HasPurchasedFinale();

	ForEachTabButton([this, Stage, bPurchased](EFF_ClosingTab Tab, UFF_LiveTabButton& Button)
	{
		const bool bVisible = FFF_ClosingHudRules::IsTabVisible(Tab, Stage, bPurchased);

		Button.SetVisibility(bVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);

		Button.SetSelected(Selection.IsSelected(Tab));
	});
}

void UFF_ClosingHudScreen::RefreshStatusRow()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	const int32 Day = ViewModel->GetCurrentDay();
	const bool bIsReport = ViewModel->GetCurrentStage() == EFF_FestivalStage::Report;

	// 🔴 단계 이름을 리플렉션으로 읽지 않는다. 이 바는 두 단계에서만 뜨므로 박아도 된다.
	SetTextOrHide(Text_DayStage, FText::Format(
		bIsReport
			? LOCTEXT("ClosingStatus_Report", "{0}일차 · 리포트")
			: LOCTEXT("ClosingStatus_Closing", "{0}일차 · 마감"),
		FText::AsNumber(Day)));

	SetTextOrHide(Text_Gold, FText::AsNumber(ViewModel->GetGold()));
}

void UFF_ClosingHudScreen::RefreshCta()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_FestivalStage Stage = ViewModel ? ViewModel->GetCurrentStage() : EFF_FestivalStage::Closing;
	const bool bIsReport = (Stage == EFF_FestivalStage::Report);
	const bool bIsFinalDay = ViewModel && ViewModel->IsFinalDay();

	CtaState = FFF_ClosingHudRules::ResolveCtaState(Stage, bIsReport, bIsFinalDay);

	if (Button_Cta)
	{
		Button_Cta->SetVisibility(CtaState == EFF_ClosingCtaState::Hidden
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
		Button_Cta->SetIsEnabled(CtaState == EFF_ClosingCtaState::Ready);
	}

	// 마지막 날에는 넘길 날이 없다. 문구도 달라야 한다 — `다음 날로` 를 띄워 놓고
	// 못 누르게 하면 무엇이 잘못됐는지 알 수 없다.
	SetTextOrHide(Text_CtaLabel, bIsFinalDay
		? LOCTEXT("ClosingCta_Finish", "축제 마치기")
		: LOCTEXT("ClosingCta_NextDay", "다음 날로"));

	// !! 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011)
	FText Reason;
	switch (CtaState)
	{
	case EFF_ClosingCtaState::BlockedNotReport:
		Reason = LOCTEXT("ClosingCta_NotReport",
			"결과를 먼저 확인하세요. 남은 마감 시간이 지나면 리포트로 넘어갑니다.");
		break;

	case EFF_ClosingCtaState::BlockedFinalDay:
		Reason = LOCTEXT("ClosingCta_FinalDay", "마지막 날입니다. 넘어갈 다음 날이 없습니다.");
		break;

	default:
		break;
	}
	SetTextOrHide(Text_CtaReason, Reason);
}

void UFF_ClosingHudScreen::HandleCtaClicked()
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// 여기서 미리 막지 않는다. 판정의 소유자는 Stage 시스템이고, 불가능하면 저쪽이
	// 무시하고 경고를 남긴다 (UI 규칙 2).
	ViewModel->RequestAdvanceToNextDay();
}

void UFF_ClosingHudScreen::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
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
