// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasGlobeScreen.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
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
		SelectButton->SetButtonLabel(LOCTEXT("AtlasGlobeSelectCountry", "축제 지도 보기"));
	}
	if (UFF_CommonButtonBase* CloseButton = GetClosePanelButton())
	{
		CloseButton->SetButtonLabel(LOCTEXT("AtlasGlobeClosePanel", "닫기"));
	}
	if (Text_ControlHints)
	{
		Text_ControlHints->SetText(
			LOCTEXT("AtlasGlobeHints", "드래그: 회전    국가 목록 / 조형물: 살펴보기    Esc: 뒤로"));
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
		Globe->SetHoveredCountry(NAME_None);
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

	// 지구 자전 중에도 커서 아래 프랍이 바뀌므로 호버를 매 프레임 확인한다.
	UpdatePinPositions();
	if (AFF_AtlasGlobeActor* Globe = GetGlobe())
	{
		Globe->SetHoveredCountry(!DragTracker.IsPressed() && Border_DragSurface && Border_DragSurface->IsHovered()
			? PickCursorLandmark() : NAME_None);
	}

	TickDrag();
}

void UFF_AtlasGlobeScreen::NativeOnDeactivated()
{
	if (AFF_AtlasGlobeActor* Globe = CachedGlobe.Get()) { Globe->SetSelectedCountry(NAME_None); Globe->SetHoveredCountry(NAME_None); }
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

FReply UFF_AtlasGlobeScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// CommonUI Back 데이터가 비어 있는 프로젝트 설정에서도 화면을 나갈 수 있게 한다.
	// 공통 라우터가 먼저 처리한 키는 여기로 오지 않는다.
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (!InKeyEvent.IsRepeat()) { NativeOnHandleBackAction(); }
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// --- 입력 ---

FReply UFF_AtlasGlobeScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (!Border_DragSurface || !Border_DragSurface->IsHovered()) { return FReply::Unhandled(); }
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

	PressedCountryId = PickCursorLandmark();
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
	if (!DragTracker.IsPressed()) { return; }
	// 마지막 Tick 뒤에 이동/해제돼도 클릭으로 잘못 판정하지 않는다.
	if (FSlateApplication::IsInitialized()) { DragTracker.AddMove(FSlateApplication::Get().GetCursorPos()); }
	const bool bWasDragging = DragTracker.IsDragging();
	DragTracker.Release();
	if (!bWasDragging && !PressedCountryId.IsNone() && Border_DragSurface && Border_DragSurface->IsHovered()
		&& PickCursorLandmark() == PressedCountryId)
	{
		InspectCountry(PressedCountryId);
	}
	// 사용자가 놓은 각도를 유지한다. 목록 클릭 시에만 국가로 회전한다.
	PressedCountryId = NAME_None;
}

FName UFF_AtlasGlobeScreen::PickCursorLandmark() const
{
	APlayerController* PC = GetOwningPlayer();
	AFF_AtlasGlobeActor* Globe = GetGlobe();
	FVector Origin, Direction;
	return PC && Globe && PC->DeprojectMousePositionToWorld(Origin, Direction)
		? Globe->PickLandmark(Origin, Direction) : NAME_None;
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

	// 클릭과 드래그는 EndDrag에서 한 번만 처리한다.
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
			Pair.Value->OnPinFocusReceived.RemoveDynamic(this, &UFF_AtlasGlobeScreen::HandleListFocused);
			Pair.Value->RemoveFromParent();
		}
	}
	PinButtons.Reset();
	if (ScrollBox_Countries) { ScrollBox_Countries->ClearChildren(); }

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

	if (!ScrollBox_Countries)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasGlobeScreen: ScrollBox_Countries 바인딩이 없어 국가 목록을 그리지 못한다."));
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
        if(UImage* Flag=Cast<UImage>(Pin->GetWidgetFromName(TEXT("Image_CountryFlag"))))
        {
            const auto* Texture=CountryFlagTextures.Find(State.CountryId);
            Flag->SetBrushFromTexture(Texture?Texture->Get():nullptr);
            Flag->SetVisibility(Texture && *Texture?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
        }
        Pin->SetIsSelectable(true);Pin->SetIsToggleable(true);

		Pin->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_AtlasGlobeScreen::HandlePinClicked);

		Pin->OnPinFocusReceived.AddUniqueDynamic(this, &UFF_AtlasGlobeScreen::HandleListFocused);
		if (UScrollBoxSlot* Row = Cast<UScrollBoxSlot>(ScrollBox_Countries->AddChild(Pin)))
		{
			Row->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			Row->SetHorizontalAlignment(HAlign_Fill);
		}
		Pin->SetPinVisible(true);
		PinButtons.Add(State.CountryId, Pin);
	}
}

void UFF_AtlasGlobeScreen::UpdatePinPositions()
{
	// 위치는 ScrollBox가 담당한다. 선택 표시는 준비 중 국가에도 동일하게 적용한다.
	for (const auto& Pair : PinButtons)
	{
		if (Pair.Value) { Pair.Value->SetIsSelected(Pair.Key == PanelCountryId, false); }
	}
}

void UFF_AtlasGlobeScreen::RefreshCountryPanel()
{
    if(UImage* Flag=Cast<UImage>(GetWidgetFromName(TEXT("Image_CountryFlag"))))
    {
        const auto* Texture=CountryFlagTextures.Find(PanelCountryId);
        Flag->SetBrushFromTexture(Texture?Texture->Get():nullptr);
        Flag->SetVisibility(Texture && *Texture?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    }

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

	if (ScrollBox_CountryDetails)
	{
		ScrollBox_CountryDetails->SetVisibility(bHasCountry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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

	// 🔴 **축제 목록을 여기서 보여주지 않는다** (2026-09-11 결정).
	// 어떤 축제가 있는지는 **국가 지도**에서 본다 — 같은 정보를 두 곳에 두면 지도로 갈 이유가 없고,
	// 왼쪽 패널은 나라 목록이 길게 보이는 것이 값어치다. 개수만 상태로 남긴다.
	int32 FestivalCount = 0;
	if (bHasCountry && Catalog)
	{
		TArray<const FFF_FestivalAtlasEntry*> Festivals;
		Catalog->GetFestivalsInCountry(PanelCountryId, Festivals);
		FestivalCount = Festivals.Num();
	}

	if (Text_FestivalCount)
	{
		Text_FestivalCount->SetText(
			FText::Format(LOCTEXT("AtlasGlobeFestivalCount", "축제 {0}개"), FText::AsNumber(FestivalCount)));
		// 값이 없으면 칸을 숨긴다. `0` 을 띄우지 않는다.
		Text_FestivalCount->SetVisibility(
			FestivalCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
		Image_LockedIcon->SetVisibility(bLocked && Image_LockedIcon->GetBrush().GetResourceObject()
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
	// 축제 목록을 만들지 않게 된 뒤로도(2026-09-11) 이 함수는 남겨 둔다 —
	// 구버전 WBP 가 남긴 행을 지우는 경로이고, 호출부 세 곳의 정리 순서를 그대로 유지한다.
	if (VerticalBox_FestivalList)
	{
		VerticalBox_FestivalList->ClearChildren();
		VerticalBox_FestivalList->SetVisibility(ESlateVisibility::Collapsed);
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
	return ScrollBox_Countries && ScrollBox_Countries->GetChildrenCount() > 0
		? Cast<UFF_AtlasPinButton>(ScrollBox_Countries->GetChildAt(0)) : nullptr;
}

// --- 핸들러 ---

void UFF_AtlasGlobeScreen::HandlePinClicked(UFF_CommonButtonBase* Button)
{
	if (UFF_AtlasPinButton* Pin = Cast<UFF_AtlasPinButton>(Button)) { InspectCountry(Pin->GetPinId()); }
}

void UFF_AtlasGlobeScreen::HandleListFocused(UFF_AtlasPinButton* Button)
{
	if (ScrollBox_Countries && Button) { ScrollBox_Countries->ScrollWidgetIntoView(Button, false); }
}

void UFF_AtlasGlobeScreen::InspectCountry(FName CountryId)
{
	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	if (!Catalog || !Catalog->FindCountry(CountryId)) { return; }
	AFF_AtlasGlobeActor* Globe = GetGlobe();
	if (!Globe) { return; }
	SkipPresentation();
	Globe->FocusOnCountry(CountryId);
	Globe->SetSelectedCountry(CountryId);
	PanelCountryId = CountryId;
	RefreshCountryPanel();
	if (ScrollBox_CountryDetails) { ScrollBox_CountryDetails->ScrollToStart(); }
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
