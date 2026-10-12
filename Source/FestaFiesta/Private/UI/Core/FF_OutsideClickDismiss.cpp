#include "UI/Core/FF_OutsideClickDismiss.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Slate/SObjectWidget.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "Widgets/SViewport.h"

namespace FFOutsideClickDismissLocal
{
	void ReleaseProcessorIfIdle();

	struct FRegistration
	{
		TWeakObjectPtr<UUserWidget> Owner;
		TFunction<bool()> HasOpenPanel;
		TFunction<bool(FVector2D)> KeepOpenAt;
		FSimpleDelegate Dismiss;
	};

	class FOutsideClickProcessor final : public IInputProcessor
	{
	public:
		TArray<FRegistration> Registrations;

		virtual void Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
		{
			Registrations.RemoveAll([](const FRegistration& Item) { return !Item.Owner.IsValid(); });
			// 앱 전환으로 release를 못 받은 경우에도 프로세서를 남기지 않는다.
			if (!SlateApp.IsActive() || !SlateApp.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
			{
				bConsumeRelease = false;
			}
			ReleaseProcessorIfIdle();
		}
		virtual const TCHAR* GetDebugName() const override { return TEXT("FestaFiesta Outside Click Dismiss"); }
		bool CanRelease() const { return Registrations.IsEmpty() && !bConsumeRelease; }

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{
			if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !SlateApp.IsActive()
				|| SlateApp.HasAnyMouseCaptor() || SlateApp.IsDragDropping())
			{
				return false;
			}

			Registrations.RemoveAll([](const FRegistration& Item) { return !Item.Owner.IsValid(); });
			const FWidgetPath PointerPath = SlateApp.LocateWindowUnderMouse(Event.GetScreenSpacePosition(),
				SlateApp.GetInteractiveTopLevelWindows(), true, Event.GetUserIndex());
			if (!PointerPath.IsValid()) return false;

			TArray<FSimpleDelegate> ToDismiss;
			TSharedPtr<SViewport> FocusViewport;
			for (const FRegistration& Item : Registrations)
			{
				UUserWidget* Owner = Item.Owner.Get();
				const ULocalPlayer* Player = Owner ? Owner->GetOwningLocalPlayer() : nullptr;
				const TSharedPtr<SViewport> Viewport = Player && Player->ViewportClient
					? Player->ViewportClient->GetGameViewportWidget() : nullptr;

				// 에디터 다른 창, 버튼/스크롤/월드 UI 위의 클릭은 건드리지 않는다.
				if (!Owner || !Owner->IsVisible() || !Viewport.IsValid()
					|| !IsWorldPointerPath(PointerPath, Viewport.Get()) || !Item.HasOpenPanel())
				{
					continue;
				}

				const UGameInstance* GI = Owner->GetGameInstance();
				const UFF_UIFlowSubsystem* Flow = GI ? GI->GetSubsystem<UFF_UIFlowSubsystem>() : nullptr;
				const UFF_UIRootLayout* Root = Flow ? Flow->GetRootLayout() : nullptr;
				if (Root && (Root->HasForegroundStackContent() || Root->IsAtlasTransitionActive())) return false;

				// 보이지 않는 상위 컨테이너 안에 남은 패널은 닫기 대상으로 삼지 않는다.
				const TSharedPtr<SWidget> SlateOwner = Owner->GetCachedWidget();
				FWidgetPath OwnerPath;
				if (!SlateOwner.IsValid() || !SlateApp.FindPathToWidget(SlateOwner.ToSharedRef(), OwnerPath)) continue;

				// 하나라도 열린 패널/메뉴 안이면 다른 패널도 그대로 둔다.
				if (Item.KeepOpenAt(Event.GetScreenSpacePosition())) return false;
				ToDismiss.Add(Item.Dismiss);
				FocusViewport = Viewport;
			}

			if (ToDismiss.IsEmpty()) return false;
			ConsumedUserIndex = Event.GetUserIndex();
			ConsumedPointerIndex = Event.GetPointerIndex();
			bConsumeRelease = true;
			// 콜백에서 등록 목록이 바뀌어도 순회를 안전하게 마친다.
			for (const FSimpleDelegate& Dismiss : ToDismiss) Dismiss.ExecuteIfBound();
			// 숨겨진 버튼에 포커스가 남아 카메라 입력이 막히지 않게 돌려준다.
			SlateApp.SetUserFocus(Event.GetUserIndex(), FocusViewport, EFocusCause::Mouse);
			return true;
		}

		virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{
			if (bConsumeRelease && Event.GetEffectingButton() == EKeys::LeftMouseButton
				&& Event.GetUserIndex() == ConsumedUserIndex && Event.GetPointerIndex() == ConsumedPointerIndex)
			{
				// 닫는 클릭의 release가 건물 선택/배치 명령으로 흘러가지 않게 한다.
				bConsumeRelease = false;
				return true;
			}
			return false;
		}

		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{
			return HandleMouseButtonDownEvent(SlateApp, Event);
		}

		// 입력 경로 판정만 분리해 실제 Slate 히트 경로로 검증한다.
		bool IsWorldPointerPath(const FWidgetPath& Path, const SWidget* Viewport) const
		{
			if (!Path.ContainsWidget(Viewport)) return false;
			bool bInsideViewport = false;
			for (const FArrangedWidget& Item : Path.Widgets.GetInternalArray())
			{
				const TSharedRef<SWidget>& Widget = Item.Widget;
				if (&Widget.Get() == Viewport) { bInsideViewport = true; continue; }
				if (!bInsideViewport) continue;
				if (Widget->IsInteractable()) return false;
				if (Widget->GetType() == TEXT("SObjectWidget"))
				{
					const UUserWidget* Object = StaticCastSharedRef<SObjectWidget>(Widget)->GetWidgetObject();
					// 등록된 전체 화면 호스트는 각자의 실제 패널 영역으로 판정한다.
					// 다른 UI의 빈 카드 표면/텍스트 위에서도 바깥 닫기가 실행되면 안 된다.
					if (Object && !Object->IsA<UFF_UIRootLayout>() && !Registrations.ContainsByPredicate(
						[Object](const FRegistration& Entry) { return Entry.Owner == Object; })) return false;
				}
			}
			return true;
		}

	private:
		bool bConsumeRelease = false;
		uint32 ConsumedUserIndex = 0;
		uint32 ConsumedPointerIndex = 0;
	};

	TSharedPtr<FOutsideClickProcessor> Processor;

	void ReleaseProcessorIfIdle()
	{
		if (Processor && Processor->CanRelease())
		{
			if (FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
			Processor.Reset();
		}
	}
}

void FFOutsideClickDismiss::Register(UUserWidget* Owner, TFunction<bool()> HasOpenPanel,
	TFunction<bool(FVector2D)> KeepOpenAt, FSimpleDelegate Dismiss)
{
	using namespace FFOutsideClickDismissLocal;
	if (!IsValid(Owner) || Owner->IsDesignTime() || !FSlateApplication::IsInitialized()) return;
	if (!Processor)
	{
		Processor = MakeShared<FOutsideClickProcessor>();
		FInputPreprocessorRegistrationKey Registration;
		Registration.Type = EInputPreProcessorType::PreGame;
		Registration.Priority = 0;
		if (!FSlateApplication::Get().RegisterInputPreProcessor(Processor, Registration))
		{
			Processor.Reset();
			return;
		}
	}
	Processor->Registrations.RemoveAll([Owner](const FRegistration& Item)
	{
		return !Item.Owner.IsValid() || Item.Owner == Owner;
	});
	FRegistration& Item = Processor->Registrations.AddDefaulted_GetRef();
	Item.Owner = Owner;
	Item.HasOpenPanel = MoveTemp(HasOpenPanel);
	Item.KeepOpenAt = MoveTemp(KeepOpenAt);
	Item.Dismiss = MoveTemp(Dismiss);
}

void FFOutsideClickDismiss::Unregister(UUserWidget* Owner)
{
	using namespace FFOutsideClickDismissLocal;
	if (!Processor) return;
	Processor->Registrations.RemoveAll([Owner](const FRegistration& Item)
	{
		return !Item.Owner.IsValid() || Item.Owner == Owner;
	});
	// 마지막 닫기 콜백이 Owner를 제거해도 해당 클릭의 up까지 소비한 뒤 Tick에서 해제한다.
	ReleaseProcessorIfIdle();
}

bool FFOutsideClickDismiss::IsInside(const UWidget* Widget, FVector2D ScreenPosition)
{
	return Widget && Widget->IsVisible() && Widget->GetCachedGeometry().GetLocalSize().GetMin() > 0.f
		&& Widget->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Input/HittestGrid.h"
#include "Misc/AutomationTest.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "UI/LiveHUD/OperationAlert/FF_OperationAlertPanel.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OutsideClickDismissPathTest,
	"FestaFiesta.UI.OutsideClickDismiss.PointerPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_OutsideClickDismissPathTest::RunTest(const FString& Parameters)
{
	using namespace FFOutsideClickDismissLocal;
	const auto PaintPath = [](const TSharedRef<SViewport>& Viewport)
	{
		const FVector2D Size(320.f, 240.f);
		const TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(Size);
		Window->SetContent(Viewport);
		Window->SlatePrepass(1.f);
		FHittestGrid HitGrid;
		HitGrid.SetHittestArea(FVector2D::ZeroVector, Size);
		FSlateWindowElementList Elements(Window);
		const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform(1.f));
		const FPaintArgs Args(nullptr, HitGrid, FVector2D::ZeroVector, 0., 1.f / 60.f);
		Window->Paint(Args, Geometry, Geometry.GetLayoutBoundingRect(), Elements, 0, FWidgetStyle(), true);
		TArray<FWidgetAndPointer> Hits = HitGrid.GetBubblePath(Size * .5, 0.f, true);
		return FWidgetPath(MakeArrayView(Hits));
	};

	FOutsideClickProcessor Policy;
	const TSharedRef<SViewport> PassiveViewport = SNew(SViewport)[SNew(SOverlay) + SOverlay::Slot()[SNew(SBorder)]];
	const FWidgetPath PassivePath = PaintPath(PassiveViewport);
	TestTrue(TEXT("수동 Slate 래퍼가 히트되어도 게임 화면 바깥 클릭으로 처리한다"),
		Policy.IsWorldPointerPath(PassivePath, &PassiveViewport.Get()));

	TStrongObjectPtr<UFF_OperationAlertPanel> Host(NewObject<UFF_OperationAlertPanel>());
	const TSharedRef<SViewport> HostViewport = SNew(SViewport)
		[SNew(SObjectWidget, Host.Get())[SNew(SBorder)]];
	const FWidgetPath HostPath = PaintPath(HostViewport);
	TestFalse(TEXT("등록하지 않은 다른 UI의 카드 표면은 보호한다"),
		Policy.IsWorldPointerPath(HostPath, &HostViewport.Get()));
	Policy.Registrations.AddDefaulted_GetRef().Owner = Host.Get();
	TestTrue(TEXT("등록한 HUD 호스트의 빈 표면은 별도 패널 영역 판정에 넘긴다"),
		Policy.IsWorldPointerPath(HostPath, &HostViewport.Get()));

	const TSharedRef<SViewport> ButtonViewport = SNew(SViewport)[SNew(SButton)];
	TestFalse(TEXT("버튼 클릭은 닫기로 소비하지 않는다"),
		Policy.IsWorldPointerPath(PaintPath(ButtonViewport), &ButtonViewport.Get()));
	TestFalse(TEXT("다른 뷰포트나 에디터의 클릭은 처리하지 않는다"),
		Policy.IsWorldPointerPath(PassivePath, &HostViewport.Get()));
	return true;
}
#endif
