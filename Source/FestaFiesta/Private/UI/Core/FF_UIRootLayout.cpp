// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Core/FF_UIRootLayout.h"

#include "Brushes/SlateColorBrush.h"
#include "CoreGlobals.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "Layout/WidgetPath.h"
#include "Rendering/DrawElements.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "UI/FF_UILog.h"

/** 기존 WBP 트리는 그대로 두고 마지막 레이어에서 단색 덮개만 그린다. */
class SFF_AtlasTransitionOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFF_AtlasTransitionOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetVisibility(EVisibility::Collapsed);
	}

	void SetAppearance(const FLinearColor& InColor, float InOpacity)
	{
		Color = InColor;
		Color.A = InOpacity;
		if (InOpacity < 1.f)
		{
			OpaquePaintFrame = MAX_uint64;
		}
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	uint64 GetOpaquePaintFrame() const { return OpaquePaintFrame; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
			&Brush, ESlateDrawEffect::None, Color);
		if (Color.A >= 1.f && OpaquePaintFrame == MAX_uint64
			&& AllottedGeometry.GetLocalSize().X > 0.f && AllottedGeometry.GetLocalSize().Y > 0.f
			&& FSlateRect::DoRectanglesIntersect(AllottedGeometry.GetLayoutBoundingRect(), MyCullingRect))
		{
			// Tick만 지난 것을 렌더 완료로 간주하지 않는다. 다음 엔진 프레임까지 교체를 미룬다.
			OpaquePaintFrame = GFrameCounter;
		}
		return LayerId;
	}

protected:
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }

private:
	FSlateColorBrush Brush = FSlateColorBrush(FLinearColor::White);
	FLinearColor Color = FLinearColor::Transparent;
	mutable uint64 OpaquePaintFrame = MAX_uint64;
};

namespace
{
	constexpr float AtlasReadyTimeoutSeconds = 5.f;

	/** 게임 뷰포트에 도달하는 해당 Slate 사용자의 입력만 막는다. PIE 에디터 입력은 유지한다. */
	class FAtlasTransitionInputProcessor final : public IInputProcessor
	{
	public:
		FAtlasTransitionInputProcessor(UFF_UIRootLayout* InOwner, int32 InUserIndex,
			const TSharedRef<SWidget>& InViewport)
			: Owner(InOwner), UserIndex(InUserIndex), Viewport(InViewport) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
		virtual const TCHAR* GetDebugName() const override { return TEXT("FestaFiesta Atlas Transition"); }

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
		{ return BlocksFocusedInput(SlateApp, Event.GetUserIndex()); }
		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
		{ return BlocksFocusedInput(SlateApp, Event.GetUserIndex()); }
		virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event) override
		{ return BlocksFocusedInput(SlateApp, Event.GetUserIndex()); }
		virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& Event) override
		{ return BlocksFocusedInput(SlateApp, Event.GetUserIndex()); }
		virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{ return BlocksPointerInput(SlateApp, Event); }
		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{ return BlocksPointerInput(SlateApp, Event); }
		virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{ return BlocksPointerInput(SlateApp, Event); }
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{ return BlocksPointerInput(SlateApp, Event); }
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event,
			const FPointerEvent* GestureEvent) override
		{ return BlocksPointerInput(SlateApp, Event); }

	private:
		bool IsBlocking(uint32 EventUserIndex) const
		{
			return EventUserIndex == static_cast<uint32>(UserIndex) && Owner.IsValid()
				&& Owner->IsAtlasTransitionActive();
		}

		bool BlocksFocusedInput(FSlateApplication& SlateApp, uint32 EventUserIndex) const
		{
			const TSharedPtr<SWidget> GameViewport = Viewport.Pin();
			return IsBlocking(EventUserIndex) && GameViewport.IsValid()
				&& (SlateApp.GetUserFocusedWidget(UserIndex) == GameViewport
					|| SlateApp.HasUserFocusedDescendants(GameViewport.ToSharedRef(), UserIndex));
		}

		bool BlocksPointerInput(FSlateApplication& SlateApp, const FPointerEvent& Event) const
		{
			const TSharedPtr<SWidget> GameViewport = Viewport.Pin();
			if (!IsBlocking(Event.GetUserIndex()) || !GameViewport.IsValid()
				|| !Owner->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
			{
				return false;
			}
			const FWidgetPath PointerPath = SlateApp.LocateWindowUnderMouse(Event.GetScreenSpacePosition(),
				SlateApp.GetInteractiveTopLevelWindows(), true, UserIndex);
			return PointerPath.ContainsWidget(GameViewport.Get());
		}

		TWeakObjectPtr<UFF_UIRootLayout> Owner;
		int32 UserIndex = INDEX_NONE;
		TWeakPtr<SWidget> Viewport;
	};

	float SanitizeAtlasDuration(float Value, float DefaultValue)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, AtlasReadyTimeoutSeconds) : DefaultValue;
	}
}

TSharedRef<SWidget> UFF_UIRootLayout::RebuildWidget()
{
	CancelAtlasTransition();
	return SNew(SOverlay)
		+ SOverlay::Slot()[Super::RebuildWidget()]
		+ SOverlay::Slot()[SAssignNew(AtlasTransitionOverlay, SFF_AtlasTransitionOverlay)];
}

void UFF_UIRootLayout::NativeConstruct()
{
	Super::NativeConstruct();
	bScreenStackTransitioning = false;
	if (ScreenStack)
	{
		ScreenStack->OnTransitioningChanged.RemoveAll(this);
		ScreenStack->OnTransitioningChanged.AddUObject(this, &ThisClass::HandleScreenStackTransitioning);
	}
}

void UFF_UIRootLayout::NativeDestruct()
{
	CancelAtlasTransition();
	if (ScreenStack)
	{
		ScreenStack->OnTransitioningChanged.RemoveAll(this);
	}
	bScreenStackTransitioning = false;
	Super::NativeDestruct();
}

void UFF_UIRootLayout::ReleaseSlateResources(bool bReleaseChildren)
{
	CancelAtlasTransition();
	AtlasTransitionOverlay.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UFF_UIRootLayout::BeginDestroy()
{
	CancelAtlasTransition();
	Super::BeginDestroy();
}

void UFF_UIRootLayout::HandleScreenStackTransitioning(UCommonActivatableWidgetContainerBase* Container, bool bIsTransitioning)
{
	bScreenStackTransitioning = bIsTransitioning;
}

bool UFF_UIRootLayout::IsAtlasTransitionActive() const
{
	return AtlasTransitionPhase != EAtlasTransitionPhase::Inactive;
}

bool UFF_UIRootLayout::BeginAtlasTransition(FSimpleDelegate SwapUnderCover, TFunction<bool()> IsReady)
{
	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	if (IsAtlasTransitionActive() || IsDesignTime() || !Settings->bEnableAtlasTransition
		|| !SwapUnderCover.IsBound() || !AtlasTransitionOverlay.IsValid() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;
	const TSharedPtr<SViewport> GameViewport = LocalPlayer && LocalPlayer->ViewportClient
		? LocalPlayer->ViewportClient->GetGameViewportWidget() : nullptr;
	if (!SlateUser.IsValid() || !GameViewport.IsValid())
	{
		return false;
	}

	AtlasTransitionInputProcessor = MakeShared<FAtlasTransitionInputProcessor>(this,
		SlateUser->GetUserIndex(), GameViewport.ToSharedRef());
	FInputPreprocessorRegistrationKey Registration;
	Registration.Type = EInputPreProcessorType::PreGame;
	Registration.Priority = 0;
	if (!FSlateApplication::Get().RegisterInputPreProcessor(AtlasTransitionInputProcessor, Registration))
	{
		AtlasTransitionInputProcessor.Reset();
		return false;
	}

	// 전환 중 설정이 바뀌더라도 이번 전환은 시작 시점 값을 끝까지 사용한다.
	AtlasFadeOutDuration = SanitizeAtlasDuration(Settings->AtlasTransitionFadeOutDuration, 0.15f);
	AtlasOpaqueHoldDuration = FMath::Max(0.05f, SanitizeAtlasDuration(Settings->AtlasTransitionOpaqueHoldDuration, 0.05f));
	AtlasFadeInDuration = SanitizeAtlasDuration(Settings->AtlasTransitionFadeInDuration, 0.25f);
	if (const UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this))
	{
		if (Accessibility->IsReduceMotionEnabled())
		{
			// 움직임 감소는 점진 페이드만 생략한다. 불투명 프레임과 준비 대기는 그대로 지킨다.
			AtlasFadeOutDuration = 0.f;
			AtlasFadeInDuration = 0.f;
		}
	}
	AtlasTransitionColor = Settings->AtlasTransitionColor;
	AtlasTransitionColor.A = 1.f;
	AtlasSwapUnderCover = MoveTemp(SwapUnderCover);
	AtlasIsReady = MoveTemp(IsReady);
	++AtlasTransitionGeneration;
	AtlasPhaseElapsed = 0.f;
	AtlasTransitionPhase = EAtlasTransitionPhase::FadingOut;
	AtlasTransitionOverlay->SetVisibility(EVisibility::HitTestInvisible);
	SetAtlasTransitionOpacity(0.f);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		// 전처리기에 가려질 KeyUp 때문에 기존 눌림 상태가 남지 않게 한다.
		PlayerController->FlushPressedKeys();
	}
	AtlasTransitionTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickAtlasTransition));
	return true;
}

void UFF_UIRootLayout::SetAtlasTransitionOpacity(float Opacity)
{
	AtlasTransitionOpacity = FMath::Clamp(Opacity, 0.f, 1.f);
	if (AtlasTransitionOverlay.IsValid())
	{
		AtlasTransitionOverlay->SetAppearance(AtlasTransitionColor, AtlasTransitionOpacity);
	}
}

void UFF_UIRootLayout::CancelAtlasTransition()
{
	const bool bWasActive = IsAtlasTransitionActive();
	++AtlasTransitionGeneration;
	AtlasTransitionPhase = EAtlasTransitionPhase::Inactive;
	AtlasPhaseElapsed = 0.f;
	AtlasSwapUnderCover.Unbind();
	AtlasIsReady = nullptr;
	FTSTicker::RemoveTicker(AtlasTransitionTicker);
	AtlasTransitionTicker.Reset();
	if (AtlasTransitionInputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(AtlasTransitionInputProcessor);
	}
	AtlasTransitionInputProcessor.Reset();
	SetAtlasTransitionOpacity(0.f);
	if (AtlasTransitionOverlay.IsValid())
	{
		AtlasTransitionOverlay->SetVisibility(EVisibility::Collapsed);
	}
	if (bWasActive)
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			PlayerController->FlushPressedKeys();
		}
	}
}

bool UFF_UIRootLayout::TickAtlasTransition(float DeltaTime)
{
	if (!IsAtlasTransitionActive() || !AtlasTransitionOverlay.IsValid())
	{
		CancelAtlasTransition();
		return false;
	}

	AtlasPhaseElapsed += FMath::IsFinite(DeltaTime) ? FMath::Max(DeltaTime, 0.f) : 0.f;
	// 한 Tick에는 한 단계만 진행한다. 긴 프레임도 불투명 렌더 단계와 한 번의 교체를 건너뛰지 않는다.
	switch (AtlasTransitionPhase)
	{
	case EAtlasTransitionPhase::FadingOut:
		SetAtlasTransitionOpacity(AtlasFadeOutDuration > 0.f ? AtlasPhaseElapsed / AtlasFadeOutDuration : 1.f);
		if (AtlasTransitionOpacity >= 1.f)
		{
			AtlasTransitionPhase = EAtlasTransitionPhase::WaitingForOpaquePaint;
			AtlasPhaseElapsed = 0.f;
		}
		break;

	case EAtlasTransitionPhase::WaitingForOpaquePaint:
		if (AtlasTransitionOverlay->GetOpaquePaintFrame() != MAX_uint64
			&& GFrameCounter > AtlasTransitionOverlay->GetOpaquePaintFrame())
		{
			AtlasTransitionPhase = EAtlasTransitionPhase::HoldingOpaque;
			AtlasPhaseElapsed = 0.f;
			const uint32 Generation = AtlasTransitionGeneration;
			FSimpleDelegate Swap = MoveTemp(AtlasSwapUnderCover);
			// 덮개 아래에서 CommonUI의 0.4초 전환을 또 재생하지 않는다. 기존 설정은 즉시 복원한다.
			const TWeakObjectPtr<UCommonActivatableWidgetStack> SwapStack = ScreenStack;
			const float PreviousStackDuration = SwapStack.IsValid() ? SwapStack->GetTransitionDuration() : 0.f;
			if (SwapStack.IsValid())
			{
				SwapStack->SetTransitionDuration(0.f);
			}
			Swap.ExecuteIfBound();
			if (SwapStack.IsValid())
			{
				SwapStack->SetTransitionDuration(PreviousStackDuration);
			}
			// 교체 콜백이 루트를 제거하거나 다른 전환을 시작한 경우 이전 Tick을 끝낸다.
			return Generation == AtlasTransitionGeneration && IsAtlasTransitionActive();
		}
		if (AtlasPhaseElapsed >= AtlasReadyTimeoutSeconds)
		{
			UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] Atlas 덮개가 5초 안에 그려지지 않아 전환을 취소한다."), *GetName());
			CancelAtlasTransition();
			return false;
		}
		break;

	case EAtlasTransitionPhase::HoldingOpaque:
		{
			const uint32 Generation = AtlasTransitionGeneration;
			// 준비 확인 중 취소돼도 실행 중인 TFunction 저장소가 파괴되지 않도록 복사한다.
			const TFunction<bool()> ReadyCheck = AtlasIsReady;
			const bool bReady = !ReadyCheck || ReadyCheck();
			if (Generation != AtlasTransitionGeneration || !IsAtlasTransitionActive())
			{
				return false;
			}
			if ((bReady && AtlasPhaseElapsed >= AtlasOpaqueHoldDuration) || AtlasPhaseElapsed >= AtlasReadyTimeoutSeconds)
			{
				if (!bReady)
				{
					UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] Atlas 화면 준비 대기가 5초를 넘어 덮개를 해제한다."), *GetName());
				}
				AtlasIsReady = nullptr;
				AtlasTransitionPhase = EAtlasTransitionPhase::FadingIn;
				AtlasPhaseElapsed = 0.f;
			}
		}
		break;

	case EAtlasTransitionPhase::FadingIn:
		SetAtlasTransitionOpacity(AtlasFadeInDuration > 0.f ? 1.f - AtlasPhaseElapsed / AtlasFadeInDuration : 0.f);
		if (AtlasTransitionOpacity <= 0.f)
		{
			CancelAtlasTransition();
			return false;
		}
		break;

	default:
		return false;
	}
	return true;
}

UFF_ActivatableScreenBase* UFF_UIRootLayout::PushScreen(TSubclassOf<UFF_ActivatableScreenBase> ScreenClass)
{
	return PushToStack(ScreenStack, ScreenClass, TEXT("ScreenStack"));
}

UFF_ActivatableScreenBase* UFF_UIRootLayout::PushPanel(TSubclassOf<UFF_ActivatableScreenBase> PanelClass)
{
	return PushToStack(PanelStack, PanelClass, TEXT("PanelStack"));
}

bool UFF_UIRootLayout::PopScreen()
{
	return PopFromStack(ScreenStack, TEXT("ScreenStack"));
}

bool UFF_UIRootLayout::PopPanel()
{
	return PopFromStack(PanelStack, TEXT("PanelStack"));
}

bool UFF_UIRootLayout::RemoveScreen(UFF_ActivatableScreenBase* Screen)
{
	if (!ScreenStack)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ScreenStack 바인딩이 없어 화면을 내릴 수 없다."), *GetName());
		return false;
	}

	if (!Screen)
	{
		// 이미 사라진 화면을 내리려는 것은 오류가 아니다. 레벨이 바뀌면 위젯이 먼저 죽는다.
		return false;
	}

	ScreenStack->RemoveWidget(*Screen);
	return true;
}

UFF_ActivatableScreenBase* UFF_UIRootLayout::PushToStack(
	UCommonActivatableWidgetStack* TargetStack,
	TSubclassOf<UFF_ActivatableScreenBase> WidgetClass,
	const TCHAR* StackLabel)
{
	// 가드마다 이유를 남긴다. 조용히 nullptr을 돌려주면 화면이 안 뜨는 원인을 찾을 수 없다.
	if (!TargetStack)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] %s 바인딩이 없다. WBP에 같은 이름의 CommonActivatableWidgetStack이 있는지 확인할 것."),
			*GetName(), StackLabel);
		return nullptr;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] %s에 밀어 넣을 위젯 클래스가 지정되지 않았다."), *GetName(), StackLabel);
		return nullptr;
	}

	UFF_ActivatableScreenBase* Pushed = TargetStack->AddWidget<UFF_ActivatableScreenBase>(WidgetClass);
	if (!Pushed)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] %s에 %s 생성 실패."), *GetName(), StackLabel, *WidgetClass->GetName());
		return nullptr;
	}

	UE_LOG(LogFestaFiestaUI, Verbose,
		TEXT("[%s] %s ← %s"), *GetName(), StackLabel, *WidgetClass->GetName());

	return Pushed;
}

bool UFF_UIRootLayout::PopFromStack(UCommonActivatableWidgetStack* TargetStack, const TCHAR* StackLabel)
{
	if (!TargetStack)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] %s 바인딩이 없어 닫을 수 없다."), *GetName(), StackLabel);
		return false;
	}

	UCommonActivatableWidget* ActiveWidget = TargetStack->GetActiveWidget();
	if (!ActiveWidget)
	{
		// 닫을 것이 없는 것은 오류가 아니다(뒤로 가기 중복 입력 등).
		UE_LOG(LogFestaFiestaUI, Verbose,
			TEXT("[%s] %s가 비어 있어 닫을 것이 없다."), *GetName(), StackLabel);
		return false;
	}

	TargetStack->RemoveWidget(*ActiveWidget);
	return true;
}
