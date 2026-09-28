// Copyright Epic Games, Inc. All Rights Reserved.

// 실제 Controller/Pawn으로 가장자리 대기, 창 포커스, 입력 차단 시 잔여 이동을 검사한다.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_FinaleViewpoint.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

namespace
{
	// OS 커서/포커스만 제공한다. 입력 정책, 입력 소비, 카메라 이동은 실제 게임 코드를 쓴다.
	class FCameraInputViewport final : public FViewport
	{
	public:
		explicit FCameraInputViewport(FViewportClient* Client) : FViewport(Client) {}
		virtual FIntPoint GetSizeXY() const override { return FIntPoint(800, 600); }
		virtual void* GetWindow() override { return nullptr; }
		virtual void MoveWindow(int32, int32, int32, int32) override {}
		virtual void Destroy() override {}
		virtual bool SetUserFocus(bool) override { return true; }
		virtual bool HasFocus() const override { return bFocused; }
		virtual bool IsForegroundWindow() const override { return bForeground; }
		virtual bool KeyState(FKey) const override { return false; }
		virtual int32 GetMouseX() const override { return Cursor.X; }
		virtual int32 GetMouseY() const override { return Cursor.Y; }
		virtual void GetMousePos(FIntPoint& Position, bool = true) override { Position = Cursor; }
		virtual void SetMouse(int32 X, int32 Y) override { Cursor = FIntPoint(X, Y); }
		virtual void ProcessInput(float) override {}
		virtual FVector2D VirtualDesktopPixelToViewport(FIntPoint Point) const override
		{
			return FVector2D(Point) / FVector2D(GetSizeXY());
		}
		virtual FIntPoint ViewportToVirtualDesktopPixel(FVector2D Point) const override
		{
			return FIntPoint(FMath::RoundToInt(Point.X * 800), FMath::RoundToInt(Point.Y * 600));
		}
		virtual void InvalidateDisplay() override {}
		virtual FViewportFrame* GetViewportFrame() override { return nullptr; }

		FIntPoint Cursor = FIntPoint(1, 300);
		bool bFocused = true;
		bool bForeground = true;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalCameraIdleInputTest,
	"FestaFiesta.Camera.Input.IdleFreezeAndOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalCameraIdleInputTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Slate application exists"), FSlateApplication::IsInitialized())) return false;
	FSlateApplication& Slate = FSlateApplication::Get();
	const bool bPreviouslyActive = Slate.IsActive();
	Slate.OnApplicationActivationChanged(true);
	ON_SCOPE_EXIT { Slate.OnApplicationActivationChanged(bPreviouslyActive); };

	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	FWorldContext* Context = World ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	if (!TestNotNull(TEXT("World"), World) || !TestNotNull(TEXT("Game"), Game) ||
		!TestNotNull(TEXT("World context"), Context)) return false;

	UGameViewportClient* ViewportClient = NewObject<UGameViewportClient>(GEngine);
	Context->GameViewport = ViewportClient;
	ViewportClient->Init(*Context, Game, false);
	FCameraInputViewport Viewport(ViewportClient);
	ViewportClient->Viewport = &Viewport;
	ON_SCOPE_EXIT { ViewportClient->Viewport = nullptr; };
	FString PlayerError;
	ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
	AFF_FestivalPlayerController* Controller = World->SpawnActor<AFF_FestivalPlayerController>();
	AFF_OverviewCameraPawn* Camera = World->SpawnActor<AFF_OverviewCameraPawn>();
	if (!TestNotNull(*FString::Printf(TEXT("Local player: %s"), *PlayerError), Player) ||
		!TestNotNull(TEXT("Controller"), Controller) || !TestNotNull(TEXT("Camera"), Camera)) return false;
	Controller->SetPlayer(Player);
	Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
	World->AddController(Controller);
	Camera->CameraSettings.bUseZoneBounds = false;
	Camera->CameraSettings.bConstrainFocusToZones = false;
	Camera->CameraSettings.bFollowGroundHeight = false;
	Camera->CameraSettings.bEnableEdgeScroll = true;
	Camera->InitializeOverview(FVector(0, 0, 100), 4000.f);

	constexpr float DeltaSeconds = 1.f / 60.f;
	const auto TickFrame = [&]()
	{
		Controller->PlayerTick(DeltaSeconds);
		Camera->Tick(DeltaSeconds);
	};
	const auto TestUnchanged = [&](const TCHAR* Label, const FFF_OverviewCameraState& Before)
	{
		const FFF_OverviewCameraState After = Camera->GetCameraState();
		TestEqual(*FString::Printf(TEXT("%s: focus"), Label), After.FocusLocation, Before.FocusLocation);
		TestEqual(*FString::Printf(TEXT("%s: yaw"), Label), After.Yaw, Before.Yaw);
		TestEqual(*FString::Printf(TEXT("%s: pitch"), Label), After.Pitch, Before.Pitch);
		TestEqual(*FString::Printf(TEXT("%s: zoom"), Label), After.ArmLength, Before.ArmLength);
	};
	const auto QueueMotion = [&]()
	{
		Camera->AddPanInput(FVector2D(1, 1));
		Camera->AddRotateInput(1.f);
		Camera->AddZoomInput(1.f);
	};
	const auto SendHold = [&](EInputEvent Event)
	{
		Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
			Controller->EdgeScrollActivationKey, Event, Event == IE_Pressed ? 1.f : 0.f));
	};

	TestTrue(TEXT("Focused overview accepts camera movement"), Controller->IsCameraMovementAllowed());
	const FFF_OverviewCameraState Initial = Camera->GetCameraState();
	for (int32 Frame = 0; Frame < 60; ++Frame) TickFrame();
	TestUnchanged(TEXT("A parked edge cursor never moves the camera"), Initial);

	SendHold(IE_Pressed);
	TickFrame();
	TestFalse(TEXT("Holding the edge key deliberately moves the camera"),
		Camera->GetCameraState().FocusLocation.Equals(Initial.FocusLocation));
	SendHold(IE_Released);
	TickFrame();
	const FFF_OverviewCameraState Released = Camera->GetCameraState();
	for (int32 Frame = 0; Frame < 60; ++Frame) TickFrame();
	TestUnchanged(TEXT("Releasing the edge key freezes the parked cursor"), Released);

	QueueMotion();
	const FFF_OverviewCameraState BeforeFlush = Camera->GetCameraState();
	Controller->FlushPressedKeys();
	Camera->Tick(DeltaSeconds);
	TestUnchanged(TEXT("Key flush also clears queued motion"), BeforeFlush);

	QueueMotion();
	const FFF_OverviewCameraState BeforeModal = Camera->GetCameraState();
	Controller->SetModalUiActive(true);
	Camera->Tick(DeltaSeconds);
	TestUnchanged(TEXT("Modal opening cancels queued motion immediately"), BeforeModal);
	Controller->SetModalUiActive(false);
	TickFrame();
	TestUnchanged(TEXT("Modal closing does not resume an old target"), BeforeModal);

	QueueMotion();
	const FFF_OverviewCameraState BeforeFocusLoss = Camera->GetCameraState();
	Viewport.bFocused = false;
	TestEqual(TEXT("Viewport focus loss is diagnosed"), Controller->GetInputBlockReason(),
		EFF_CameraInputBlock::InactiveViewport);
	TickFrame();
	TestUnchanged(TEXT("Lost viewport focus discards queued motion"), BeforeFocusLoss);
	Viewport.bFocused = true;
	TickFrame();
	TestUnchanged(TEXT("Focus restoration never resumes discarded motion"), BeforeFocusLoss);

	QueueMotion();
	Viewport.bForeground = false;
	TickFrame();
	TestUnchanged(TEXT("A background window cannot keep moving"), BeforeFocusLoss);
	Viewport.bForeground = true;

	QueueMotion();
	Slate.OnApplicationActivationChanged(false);
	TestFalse(TEXT("An inactive app rejects movement"), Controller->IsCameraMovementAllowed());
	TickFrame();
	TestUnchanged(TEXT("Application deactivation discards queued motion"), BeforeFocusLoss);
	Slate.OnApplicationActivationChanged(true);

	AFF_FinaleViewpoint* ScriptedView = World->SpawnActor<AFF_FinaleViewpoint>();
	AActor* OtherView = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Scripted view"), ScriptedView) || !TestNotNull(TEXT("Other view"), OtherView))
		return false;
	QueueMotion();
	Camera->SetFollowTarget(OtherView);
	const FFF_OverviewCameraState BeforeScripted = Camera->GetCameraState();
	TestTrue(TEXT("Enter scripted view"), Controller->EnterScriptedView(ScriptedView, 0.f));
	Camera->Tick(DeltaSeconds);
	TestUnchanged(TEXT("Entering scripted view freezes the overview immediately"), BeforeScripted);
	TestNull(TEXT("Scripted ownership releases explicit tracking"), Camera->GetFollowTarget());
	Controller->ExitScriptedView(0.f);
	TickFrame();
	TestUnchanged(TEXT("Returning from scripted view preserves that frozen view"), BeforeScripted);

	QueueMotion();
	TestTrue(TEXT("Switch to actor view"), Controller->SwitchToActorView(OtherView, 0.f));
	Camera->Tick(DeltaSeconds);
	TestUnchanged(TEXT("Actor view never advances hidden overview motion"), BeforeScripted);
	Controller->SwitchToOverview(0.f);
	TickFrame();
	TestUnchanged(TEXT("Returning to overview does not jump to a stale target"), BeforeScripted);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
