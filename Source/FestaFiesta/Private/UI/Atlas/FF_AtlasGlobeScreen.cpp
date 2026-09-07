// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasGlobeScreen.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Atlas/FF_AtlasCameraRig.h"
#include "UI/Atlas/FF_AtlasGlobeActor.h"
#include "UI/Atlas/FF_AtlasPinButton.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_AtlasGlobeScreen::UFF_AtlasGlobeScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CommonUI 기본 동작(자기 자신만 Deactivate)이 아니라 UI Flow의 히스토리를 통해 뒤로 간다.
	bIsBackHandler = true;
}

void UFF_AtlasGlobeScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 문구는 인스턴스에 한 번만 넣으면 된다.
	if (UFF_CommonButtonBase* SelectButton = GetSelectCountryButton())
	{
		SelectButton->SetButtonLabel(LOCTEXT("AtlasGlobeSelectCountry", "선택"));
	}
	if (UFF_CommonButtonBase* CloseButton = GetClosePanelButton())
	{
		CloseButton->SetButtonLabel(LOCTEXT("AtlasGlobeClosePanel", "닫기"));
	}
	if (Text_ControlHints)
	{
		Text_ControlHints->SetText(
			LOCTEXT("AtlasGlobeHints", "드래그: 회전    ← →: 핀 이동    Enter: 선택    Esc: 플레이 허브"));
	}
}

void UFF_AtlasGlobeScreen::NativeConstruct()
{
	// 기존 WBP CDO가 예전 false 값을 갖고 있어도 Super가 Back 입력을 등록하기 전에 바로잡는다.
	bIsBackHandler = true;
	Super::NativeConstruct();

	// 화면에 새로 들어오면 지구만 보이는 상태에서 시작한다. 액터의 최초 Focus 값은 패널을 열지 않는다.
	PanelCountryId = NAME_None;
	ClearFestivalRows();

	// 🔴 구독은 `NativeConstruct` 다. CommonUI 스택이 위젯을 풀에서 재사용하므로
	// `NativeOnInitialized` 에 구독하고 `NativeDestruct` 에 해제하면 재진입 시 버튼이 죽는다.
	if (UFF_CommonButtonBase* SelectButton = GetSelectCountryButton())
	{
		SelectButton->OnFFButtonClicked.AddUniqueDynamic(
			this, &UFF_AtlasGlobeScreen::HandleSelectCountryClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: Button_SelectCountry 바인딩이 없어 국가 지도로 갈 수 없다."));
	}
	if (UFF_CommonButtonBase* CloseButton = GetClosePanelButton())
	{
		CloseButton->OnFFButtonClicked.AddUniqueDynamic(
			this, &UFF_AtlasGlobeScreen::HandleClosePanelClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: Button_ClosePanel 바인딩이 없어 패널을 닫을 수 없다."));
	}

	if (!Border_DragSurface)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: Border_DragSurface 바인딩이 없어 빈 공간에서 지구를 돌릴 수 없다."));
	}
	if (!Border_Panel)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: Border_Panel 바인딩이 없어 국가 패널을 열고 닫을 수 없다."));
	}
	if (!VerticalBox_FestivalList)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: VerticalBox_FestivalList 바인딩이 없어 축제 목록을 표시할 수 없다."));
	}

	DragTracker.DragThresholdPixels = DragThresholdPixels;
	DragTracker.Reset();

	if (AFF_AtlasGlobeActor* Globe = GetGlobe())
	{
		// 새 진입은 패널도 선택 조명도 없는 지구 상태다. 회전 각도 자체는 유지한다.
		Globe->SetSelectedCountry(NAME_None);
		Globe->OnCountryFocused.AddUniqueDynamic(this, &UFF_AtlasGlobeScreen::HandleCountryFocused);
	}
}

void UFF_AtlasGlobeScreen::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건.
	if (UFF_CommonButtonBase* SelectButton = GetSelectCountryButton())
	{
		SelectButton->OnFFButtonClicked.RemoveDynamic(
			this, &UFF_AtlasGlobeScreen::HandleSelectCountryClicked);
	}
	if (UFF_CommonButtonBase* CloseButton = GetClosePanelButton())
	{
		CloseButton->OnFFButtonClicked.RemoveDynamic(
			this, &UFF_AtlasGlobeScreen::HandleClosePanelClicked);
	}

	// 액터는 위젯보다 오래 산다. 해제를 빼면 죽은 위젯을 역참조한다.
	if (AFF_AtlasGlobeActor* Globe = CachedGlobe.Get())
	{
		Globe->OnCountryFocused.RemoveDynamic(this, &UFF_AtlasGlobeScreen::HandleCountryFocused);
	}

	ClearPinButtons();
	ClearFestivalRows();
	PanelCountryId = NAME_None;
	DragTracker.Reset();
	CachedGlobe.Reset();

	Super::NativeDestruct();
}

void UFF_AtlasGlobeScreen::NativeOnActivated()
{
	// 화면에 올라올 때마다 핀을 다시 만든다. 카탈로그가 이 화면 밖에서 바뀔 수 있고
	// (`FF.Atlas.Rebuild`), 위젯이 풀에서 재사용되므로 이전 핀이 남아 있을 수 있다.
	RebuildPinButtons();
	RefreshCountryPanel();

	// 🔴 "지구 누름" 로그가 아예 안 뜬다는 보고(2026-09-03)를 가르는 두 번째 축이다.
	//    이 줄이 뜨는데 누름이 안 오면 **위젯은 살아 있는데 입력만 안 오는 것**이고,
	//    이 줄조차 없으면 **화면이 아예 안 올라온 것**이다. 대처가 완전히 다르다.
	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 지구본 화면 활성화 — 드래그면=%s · 가시성=%d"),
		*GetName(),
		Border_DragSurface ? TEXT("있음") : TEXT("**없음**"),
		Border_DragSurface ? static_cast<int32>(Border_DragSurface->GetVisibility()) : -1);

	Super::NativeOnActivated();
}

void UFF_AtlasGlobeScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 지구가 상시 자전하므로 핀 좌표가 매 프레임 바뀐다. 이벤트로 알릴 수 있는 변화가 아니다.
	UpdatePinPositions();

	TickDrag();
}

void UFF_AtlasGlobeScreen::NativeOnDeactivated()
{
	if (AFF_AtlasGlobeActor* Globe = CachedGlobe.Get()) { Globe->SetSelectedCountry(NAME_None); }
	PanelCountryId = NAME_None;
	DragTracker.Reset();
	Super::NativeOnDeactivated();
}

UWidget* UFF_AtlasGlobeScreen::NativeGetDesiredFocusTarget() const
{
	if (!PanelCountryId.IsNone())
	{
		if (UFF_CommonButtonBase* SelectButton = GetSelectCountryButton();
			SelectButton && SelectButton->GetIsEnabled())
		{
			return SelectButton;
		}
		if (UFF_CommonButtonBase* CloseButton = GetClosePanelButton())
		{
			return CloseButton;
		}
	}
	if (UFF_AtlasPinButton* Pin = FindFirstVisiblePin())
	{
		return Pin;
	}
	return Super::NativeGetDesiredFocusTarget();
}

bool UFF_AtlasGlobeScreen::NativeOnHandleBackAction()
{
	if (!PanelCountryId.IsNone())
	{
		HandleClosePanelClicked(nullptr);
		return true;
	}
	// 기본 CommonUI Deactivate만 쓰면 UIFlowSubsystem의 현재 화면·히스토리가 어긋난다.
	OnBackRequested.Broadcast();
	return true;
}

// --- 입력 ---

FReply UFF_AtlasGlobeScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	// 연출 중에 손을 대면 즉시 끝낸다. 반복 플레이에서 매번 다 보게 하지 않는다.
	SkipPresentation();

	// 🔴 **이 줄이 로그에 안 뜨면 원인이 다른 곳이다.**
	//
	// "축제를 한 번 하고 오면 지구가 안 돌아간다"(2026-09-03)의 원인을 코드만 읽어서는
	// 못 갈랐다. 두 가지가 가능한데 대처가 완전히 다르다 —
	//   ① 누름 이벤트가 **위젯까지 온다** → 캡처·이동 라우팅 문제 (`TickDrag` 이 고친다)
	//   ② 누름 이벤트가 **아예 안 온다** → CommonUI 입력 설정 문제 (여기서는 못 고친다)
	// 앞서 ① 이라고 단정했는데, 프론트엔드는 `FUIInputConfig(Menu, NoCapture)` 를 쓰므로
	// 캡처가 멀쩡해야 맞다. 즉 ② 일 수 있다. 숫자로 가른다.
	if (const APlayerController* PC = GetOwningPlayer())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 지구 누름 — 커서표시=%d · 입력이 위젯까지 왔다."),
			*GetName(), PC->bShowMouseCursor ? 1 : 0);
	}

	DragTracker.Press(InMouseEvent.GetScreenSpacePosition());

	// 🔴 **캡처하지 않는다** (2026-09-03 정정).
	//
	// 예전에는 캡처했다 — 프론트엔드가 `NoCapture` 라 캡처 없이는 커서가 위젯 밖으로 나가면
	// 이동 이벤트가 끊긴다는 이유였다. 그 전제가 **축제를 한 번 하고 오면 깨진다.**
	// 건축 모드를 끌 때 `SetInputMode(FInputModeGameOnly())` 를 부르고
	// (`FF_BuildModeSubsystem.cpp`), `ReturnToAtlas()` 는 입력 모드를 되돌리지 않는다.
	// `FInputModeGameOnly` 는 `bHideCursorDuringCapture = true` 라 캡처하는 순간 커서가
	// 잠기고 좌표가 안 변해 **지구가 안 돌아간다.**
	//
	// 셋업·라이브 패널에서 같은 함정을 이미 겪었다 (`docs/memory/prep-hud-reference.md` 3절).
	// 거기서 쓴 해법을 그대로 쓴다 — 캡처 없이 `NativeTick` 에서 커서를 직접 읽는다.
	// 입력 모드가 무엇이든 동작하고, 커서가 위젯 밖으로 나가도 안 끊긴다.
	return FReply::Handled();
}

void UFF_AtlasGlobeScreen::TickDrag()
{
	if (!DragTracker.IsPressed())
	{
		return;
	}

	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	FSlateApplication& Slate = FSlateApplication::Get();

	// ★ 뗀 것도 여기서 본다. 캡처가 없으면 위젯 밖에서 뗀 이벤트가 안 오고,
	//   그러면 눌린 상태가 남아 **손을 뗀 뒤에도 지구가 커서를 따라다닌다.**
	if (!Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		EndDrag();
		return;
	}

	// 임계 안이면 0 이 돌아온다 (데드존). 넘은 뒤에는 이동량이 그대로 온다.
	const FVector2D Delta = DragTracker.AddMove(Slate.GetCursorPos());

	if (!Delta.IsNearlyZero())
	{
		if (!PanelCountryId.IsNone())
		{
			HandleClosePanelClicked(nullptr);
		}
		if (AFF_AtlasGlobeActor* Globe = GetGlobe())
		{
			// 픽셀 → 회전은 순수 함수가 한다 (`FFF_AtlasGeo::ApplyDrag`). 여기서 각도를 만들지 않는다.
			Globe->AddDragInput(Delta);
		}
	}
}

void UFF_AtlasGlobeScreen::EndDrag()
{
	const bool bWasDragging = DragTracker.IsDragging();
	DragTracker.Release();

	if (bWasDragging)
	{
		// 손을 뗐으니 가장 가까운 핀으로 다시 붙인다. 확정 요구사항이다 —
		// "스틱을 직접 움직이면 자유 회전, 끝나면 가까운 활성 핀으로 재스냅".
		if (AFF_AtlasGlobeActor* Globe = GetGlobe())
		{
			Globe->ReSnapToNearestCountry();
		}
	}
}

FReply UFF_AtlasGlobeScreen::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 이동은 `TickDrag()` 이 처리한다. 여기서는 눌린 동안 이벤트만 먹어
	// 아래 위젯으로 새지 않게 한다 — 이동을 여기서도 하면 한 프레임에 두 번 돈다.
	return DragTracker.IsPressed()
		? FReply::Handled()
		: Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UFF_AtlasGlobeScreen::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	// 클릭이었으면(임계를 안 넘었으면) 지구 빈 곳을 눌렀다는 뜻이다. 선택을 바꾸지 않는다 —
	// 국가 선택은 핀 버튼만 한다.
	//
	// 위젯 안에서 뗐으면 여기가 먼저 오고, 밖에서 뗐으면 `TickDrag()` 이 잡는다.
	// 둘 중 어느 쪽이 와도 `DragTracker.Release()` 뒤에는 눌림이 풀려 두 번 돌지 않는다.
	EndDrag();

	return FReply::Handled();
}

void UFF_AtlasGlobeScreen::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// 캡처를 잃으면 드래그 상태가 남아 다음 클릭이 이어지는 드래그로 오인된다.
	DragTracker.Reset();

	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

// --- 핀 ---

void UFF_AtlasGlobeScreen::ClearPinButtons()
{
	for (const TPair<FName, TObjectPtr<UFF_AtlasPinButton>>& Pair : PinButtons)
	{
		if (Pair.Value)
		{
			Pair.Value->OnFFButtonClicked.RemoveDynamic(this, &UFF_AtlasGlobeScreen::HandlePinClicked);
			Pair.Value->RemoveFromParent();
		}
	}
	PinButtons.Reset();

	// 핀 전용 캔버스라 안전하다. 이전 버전이 추적 맵을 먼저 비워 남긴 잔여 핀까지 제거한다.
	if (CanvasPanel_Pins)
	{
		CanvasPanel_Pins->ClearChildren();
	}
}

void UFF_AtlasGlobeScreen::RebuildPinButtons()
{
	// 조건 검사보다 먼저 정리한다. 클래스·액터가 잠시 없더라도 옛 핀이 화면에 남으면 안 된다.
	ClearPinButtons();

	if (!CanvasPanel_Pins)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: CanvasPanel_Pins 바인딩이 없어 핀을 그리지 못한다."));
		return;
	}

	if (!PinButtonClass)
	{
		// 생성기가 CDO 프로퍼티를 채우지 못한다. 에디터에서 지정해야 하는 항목이다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: PinButtonClass 가 없어 핀이 하나도 뜨지 않는다. "
				 "WBP 에서 WBP_FF_AtlasPin 을 지정한다."));
		return;
	}

	AFF_AtlasGlobeActor* Globe = GetGlobe();
	if (!Globe)
	{
		return;
	}

	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();

	TArray<FFF_AtlasPinState> PinStates;
	Globe->GetPinStates(PinStates);

	for (const FFF_AtlasPinState& State : PinStates)
	{
		UFF_AtlasPinButton* Pin = CreateWidget<UFF_AtlasPinButton>(this, PinButtonClass);
		if (!Pin)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("UFF_AtlasGlobeScreen: 핀 위젯 생성 실패 (%s)."), *PinButtonClass->GetName());
			continue;
		}

		// 표시 문구는 카탈로그가 갖는다. 액터는 좌표와 잠금 여부만 안다.
		FText Title = FText::FromName(State.CountryId);
		FText Badge;
		FText LockReason;

		if (Catalog)
		{
			if (const FFF_AtlasCountryEntry* Country = Catalog->FindCountry(State.CountryId))
			{
				if (!Country->DisplayName.IsEmpty())
				{
					Title = Country->DisplayName;
				}
				LockReason = Country->LockReason;

				const int32 Count = Catalog->CountFestivalsInCountry(State.CountryId);
				if (Count > 0)
				{
					Badge = FText::Format(LOCTEXT("AtlasPinFestivalCount", "축제 {0}"), FText::AsNumber(Count));
				}
			}
		}

		Pin->SetPinData(State.CountryId, Title, Badge, State.bSelectable, LockReason);
		Pin->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_AtlasGlobeScreen::HandlePinClicked);

		// 캔버스에 올리고 앵커를 점으로 둔다. 좌표는 매 프레임 갱신한다.
		if (UCanvasPanelSlot* PinSlot = CanvasPanel_Pins->AddChildToCanvas(Pin))
		{
			PinSlot->SetAnchors(FAnchors(0.f, 0.f));
			// 핀의 중심이 지구 표면 지점에 오게 한다. (0,0) 이면 좌상단이 걸려 어긋난다.
			PinSlot->SetAlignment(FVector2D(0.5, 0.5));
			PinSlot->SetAutoSize(true);
		}

		Pin->SetPinVisible(State.bFacingViewer);
		PinButtons.Add(State.CountryId, Pin);
	}
}

void UFF_AtlasGlobeScreen::UpdatePinPositions()
{
	if (PinButtons.Num() == 0 || !CanvasPanel_Pins)
	{
		return;
	}

	AFF_AtlasGlobeActor* Globe = GetGlobe();
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	if (!Globe || !Controller)
	{
		return;
	}

	TArray<FFF_AtlasPinState> PinStates;
	Globe->GetPinStates(PinStates);

	for (const FFF_AtlasPinState& State : PinStates)
	{
		TObjectPtr<UFF_AtlasPinButton>* Found = PinButtons.Find(State.CountryId);
		if (!Found || !*Found)
		{
			// 액터의 핀 집합이 바뀌었다. 다음 활성화에서 다시 만든다.
			continue;
		}

		UFF_AtlasPinButton* Pin = *Found;

		// 🔴 이 함수 하나만 쓴다. 내부에서 `ScreenToViewport` 를 거쳐 **DPI 배율을 이미 제거한**
		//    슬레이트 좌표를 돌려주므로 캔버스 슬롯에 그대로 넣는다.
		//    여기에 배율을 한 번 더 적용하면 두 번 나뉘어, 배율 1 인 환경에서는 티가 안 나고
		//    창을 리사이즈할 때만 핀이 어긋난다.
		FVector2D WidgetPosition = FVector2D::ZeroVector;
		const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			Controller, State.WorldLocation, WidgetPosition, /*bPlayerViewportRelative=*/false);

		// 카메라 뒤로 넘어가면 투영이 실패한다. 그때 좌표를 쓰면 화면 반대쪽에 핀이 튄다.
		const bool bVisible = State.bFacingViewer && bProjected;
		Pin->SetPinVisible(bVisible);

		if (!bVisible)
		{
			continue;
		}

		if (UCanvasPanelSlot* PinSlot = Cast<UCanvasPanelSlot>(Pin->Slot))
		{
			PinSlot->SetPosition(WidgetPosition);
		}
	}
}

void UFF_AtlasGlobeScreen::RefreshCountryPanel()
{
	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	ClearFestivalRows();

	// 패널 열림 상태는 화면이 소유한다. 액터는 시작부터 첫 국가를 Focused로 둘 수 있으므로
	// 매번 액터 값을 읽으면 "처음에는 지구만"이라는 상태를 유지할 수 없다.
	const FFF_AtlasCountryEntry* Country = (Catalog && !PanelCountryId.IsNone())
		? Catalog->FindCountry(PanelCountryId)
		: nullptr;

	// 표시를 부분 갱신하지 않고 전부 다시 선언한다. 어떤 순서로 들어와도 결과가 같다.
	const bool bHasCountry = (Country != nullptr);
	if (!bHasCountry)
	{
		PanelCountryId = NAME_None;
	}

	if (Border_Panel)
	{
		Border_Panel->SetVisibility(bHasCountry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_CountryName)
	{
		Text_CountryName->SetText(bHasCountry ? Country->DisplayName : FText::GetEmpty());
	}

	if (Text_Description)
	{
		Text_Description->SetText(bHasCountry ? Country->Description : FText::GetEmpty());
		Text_Description->SetVisibility(
			(bHasCountry && !Country->Description.IsEmpty())
				? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 축제 개수와 목록. 대표 축제 한 줄은 목록과 중복이라 구버전 WBP에서도 숨긴다.
	int32 FestivalCount = 0;
	if (bHasCountry && Catalog)
	{
		TArray<const FFF_FestivalAtlasEntry*> Festivals;
		Catalog->GetFestivalsInCountry(PanelCountryId, Festivals);
		FestivalCount = Festivals.Num();

		if (VerticalBox_FestivalList && PinButtonClass)
		{
			for (const FFF_FestivalAtlasEntry* Festival : Festivals)
			{
				if (!Festival)
				{
					continue;
				}

				UFF_AtlasPinButton* Row = CreateWidget<UFF_AtlasPinButton>(this, PinButtonClass);
				if (!Row)
				{
					UE_LOG(LogFestaFiestaUI, Error,
						TEXT("UFF_AtlasGlobeScreen: 축제 목록 행 생성 실패 (%s)."), *PinButtonClass->GetName());
					continue;
				}

				Row->SetPinData(
					Festival->FestivalId,
					Festival->FestivalName,
					Festival->RegionName,
					Festival->bUnlocked,
					Festival->LockReason);

				// 이 목록은 국가 진입 전에 내용을 훑는 정보 행이다. 클릭·포커스 가능한 죽은 버튼으로 두지 않는다.
				Row->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (UVerticalBoxSlot* RowSlot = VerticalBox_FestivalList->AddChildToVerticalBox(Row))
				{
					RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
				}
				FestivalRows.Add(Row);
			}
		}
	}

	if (VerticalBox_FestivalList)
	{
		VerticalBox_FestivalList->SetVisibility(
			FestivalRows.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_FestivalCount)
	{
		Text_FestivalCount->SetText(
			FText::Format(LOCTEXT("AtlasGlobeFestivalCount", "축제 {0}개"), FText::AsNumber(FestivalCount)));
		// 값이 없으면 칸을 숨긴다. `0` 을 띄우지 않는다.
		Text_FestivalCount->SetVisibility(
			FestivalCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_RepresentativeFestival)
	{
		Text_RepresentativeFestival->SetText(FText::GetEmpty());
		Text_RepresentativeFestival->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Image_Preview)
	{
		// 이미지가 없으면 칸을 숨긴다. 빈 사각형을 남기지 않는다.
		UTexture2D* Preview = (bHasCountry && !Country->PreviewImage.IsNull())
			? Country->PreviewImage.LoadSynchronous()
			: nullptr;

		if (Preview)
		{
			Image_Preview->SetBrushFromTexture(Preview);
		}
		Image_Preview->SetVisibility(Preview ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 잠금 — 색만으로 알리지 않는다. 자물쇠 + 사유를 함께 띄우고 진입을 막는다 (AT-011).
	const bool bLocked = bHasCountry && !Country->bUnlocked;

	if (Image_LockedIcon)
	{
		Image_LockedIcon->SetVisibility(bLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_LockReason)
	{
		Text_LockReason->SetText(bLocked ? Country->LockReason : FText::GetEmpty());
		Text_LockReason->SetVisibility(bLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UFF_CommonButtonBase* SelectButton = GetSelectCountryButton())
	{
		// 잠겼거나 축제가 없으면 들어갈 곳이 없다. 비활성만 두지 않고 위의 사유 문구를 함께 띄운다.
		SelectButton->SetIsEnabled(bHasCountry && !bLocked && FestivalCount > 0);
	}
	if (UFF_CommonButtonBase* CloseButton = GetClosePanelButton())
	{
		CloseButton->SetIsEnabled(bHasCountry);
	}
}

void UFF_AtlasGlobeScreen::ClearFestivalRows()
{
	for (UFF_AtlasPinButton* Row : FestivalRows)
	{
		if (Row)
		{
			Row->RemoveFromParent();
		}
	}
	FestivalRows.Reset();

	// 축제 행 전용 컨테이너라 미추적 잔여 행까지 안전하게 제거할 수 있다.
	if (VerticalBox_FestivalList)
	{
		VerticalBox_FestivalList->ClearChildren();
	}
}

UFF_CommonButtonBase* UFF_AtlasGlobeScreen::GetSelectCountryButton() const
{
	return Button_SelectCountry ? Button_SelectCountry.Get() : Button_BrowseFestivals.Get();
}

UFF_CommonButtonBase* UFF_AtlasGlobeScreen::GetClosePanelButton() const
{
	return Button_ClosePanel ? Button_ClosePanel.Get() : Button_Back.Get();
}

UFF_AtlasPinButton* UFF_AtlasGlobeScreen::FindFirstVisiblePin() const
{
	for (const TPair<FName, TObjectPtr<UFF_AtlasPinButton>>& Pair : PinButtons)
	{
		if (Pair.Value && Pair.Value->GetVisibility() == ESlateVisibility::Visible)
		{
			return Pair.Value.Get();
		}
	}
	return nullptr;
}

// --- 핸들러 ---

void UFF_AtlasGlobeScreen::HandlePinClicked(UFF_CommonButtonBase* Button)
{
	UFF_AtlasPinButton* Pin = Cast<UFF_AtlasPinButton>(Button);
	if (!Pin)
	{
		return;
	}

	SkipPresentation();

	AFF_AtlasGlobeActor* Globe = GetGlobe();
	if (!Globe)
	{
		return;
	}

	// 잠긴 국가도 **정면으로는 돌린다.** 포커스는 가고 선택만 막는 것이 확정안이다 —
	// 그래야 왜 못 고르는지 왼쪽 패널에서 읽을 수 있다.
	Globe->FocusOnCountry(Pin->GetPinId());

	// 선택은「살펴보기」다. 잠금 국가는 동일하게 연출하되 지도 진입은 별도로 차단한다.
	Globe->SetSelectedCountry(Pin->GetPinId());
	if (!Pin->IsSelectable())
	{
		// 이전의 열린 국가가 계속 선택 조명으로 남지 않게 한다. 잠금은 패널의 아이콘·사유로 알린다.
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_AtlasGlobeScreen: 잠긴 국가 `%s` 를 눌렀다 — %s"),
			*Pin->GetPinId().ToString(),
			Pin->GetLockReason().IsEmpty() ? TEXT("(사유 없음)") : *Pin->GetLockReason().ToString());
	}

	// 패널은 즉시 눌린 국가를 보여준다. 회전이 끝날 때까지 기다리면 반응이 없어 보인다.
	PanelCountryId = Pin->GetPinId();
	RefreshCountryPanel();
}

void UFF_AtlasGlobeScreen::HandleSelectCountryClicked(UFF_CommonButtonBase* Button)
{
	// 지금 패널이 보여주는 국가로 넘어간다. 액터의 포커스를 다시 묻지 않는다 —
	// 회전 중이면 포커스가 비어 있어 엉뚱한 국가로 갈 수 있다.
	if (PanelCountryId.IsNone())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: 고른 국가가 없어 국가 지도로 넘어가지 않는다."));
		return;
	}

	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	const FFF_AtlasCountryEntry* Country = Catalog ? Catalog->FindCountry(PanelCountryId) : nullptr;
	if (!Country || !Country->bUnlocked) { return; }
	OnBrowseFestivalsRequested.Broadcast(PanelCountryId);
}

void UFF_AtlasGlobeScreen::HandleClosePanelClicked(UFF_CommonButtonBase* Button)
{
	if (AFF_AtlasGlobeActor* Globe = GetGlobe())
	{
		Globe->SetSelectedCountry(NAME_None);
	}

	PanelCountryId = NAME_None;
	RefreshCountryPanel();

	// 닫힌 패널의 버튼에 포커스가 남지 않게 가시 국가 핀으로 돌려준다.
	if (UFF_AtlasPinButton* Pin = FindFirstVisiblePin())
	{
		Pin->SetFocus();
	}
}

void UFF_AtlasGlobeScreen::HandleCountryFocused(FName CountryId)
{
	// 패널이 이미 열린 경우에만 회전 도착 국가로 맞춘다. 액터의 최초 Focus는 패널을 열지 않는다.
	if (!PanelCountryId.IsNone() && !CountryId.IsNone())
	{
		PanelCountryId = CountryId;
		if (AFF_AtlasGlobeActor* Globe = GetGlobe())
		{
			Globe->SetSelectedCountry(CountryId);
		}
		RefreshCountryPanel();
	}
}

// --- 조회 ---

AFF_AtlasGlobeActor* UFF_AtlasGlobeScreen::GetGlobe() const
{
	if (AFF_AtlasGlobeActor* Cached = CachedGlobe.Get())
	{
		return Cached;
	}

	// 위젯이 액터를 스폰하지 않는다 (안티패턴 7). 레벨에 놓인 것을 찾을 뿐이다.
	AFF_AtlasGlobeActor* Found = AFF_AtlasGlobeActor::Find(this);
	CachedGlobe = Found;
	return Found;
}

const UFF_FestivalAtlasCatalog* UFF_AtlasGlobeScreen::GetCatalog() const
{
	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	if (!Settings || Settings->AtlasCatalog.IsNull())
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_AtlasGlobeScreen: Atlas 카탈로그가 지정되지 않았다. "
				 "Project Settings > Game > FestaFiesta UI Flow 에서 지정한다."));
		return nullptr;
	}

	return Settings->AtlasCatalog.LoadSynchronous();
}

void UFF_AtlasGlobeScreen::SkipPresentation()
{
	// 카메라와 지구 회전 둘 다 끝낸다. 하나만 끊으면 반쪽만 멈춘 상태가 된다.
	if (AFF_AtlasCameraRig* Rig = AFF_AtlasCameraRig::Find(this, /*bLogIfMissing=*/false))
	{
		Rig->SkipCameraMove();
	}
	if (AFF_AtlasGlobeActor* Globe = GetGlobe())
	{
		Globe->SkipTransition();
	}
}

#undef LOCTEXT_NAMESPACE
