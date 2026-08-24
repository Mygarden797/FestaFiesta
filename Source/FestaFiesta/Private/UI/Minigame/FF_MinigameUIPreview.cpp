// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Minigame/FF_MinigameSubsystem.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UI/Minigame/FF_MinigameScreenBase.h"
#include "UI/Minigame/FF_SurfingScreen.h"
#include "UI/Minigame/FF_TargetShootingScreen.h"
#include "Blueprint/WidgetLayoutLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogFestaFiestaMinigameUIPreview, Log, All);

namespace FFMinigameUIPreview
{
	constexpr float ScreenshotDelaySeconds = 0.20f;
	constexpr float ScreenshotVerificationIntervalSeconds = 0.05f;
	constexpr float ScreenshotVerificationTimeoutSeconds = 3.0f;
	constexpr float AutoExitDelaySeconds = 1.00f;
	constexpr int32 PreviewZOrder = 200;

	const TCHAR* const TargetScreenClassPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_TargetShooting."
			 "WBP_FF_TargetShooting_C");
	const TCHAR* const SurfScreenClassPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing_FreeRide."
			 "WBP_FF_Surfing_FreeRide_C");

	enum class EPreviewMode : uint8
	{
		Target,
		TargetResult,
		Surf,
		SurfResult,
		Close,
		Invalid
	};

	TWeakObjectPtr<UFF_MinigameScreenBase> PreviewScreen;
	FDelegateHandle PreviewCloseDelegateHandle;
	FTSTicker::FDelegateHandle ScreenshotTickerHandle;
	FDelegateHandle ScreenshotProcessedDelegateHandle;
	FTSTicker::FDelegateHandle AutoExitTickerHandle;

	EPreviewMode ParseMode(const FString& Value)
	{
		if (Value.Equals(TEXT("Target"), ESearchCase::IgnoreCase))
		{
			return EPreviewMode::Target;
		}
		if (Value.Equals(TEXT("TargetResult"), ESearchCase::IgnoreCase))
		{
			return EPreviewMode::TargetResult;
		}
		if (Value.Equals(TEXT("Surf"), ESearchCase::IgnoreCase))
		{
			return EPreviewMode::Surf;
		}
		if (Value.Equals(TEXT("SurfResult"), ESearchCase::IgnoreCase))
		{
			return EPreviewMode::SurfResult;
		}
		if (Value.Equals(TEXT("Close"), ESearchCase::IgnoreCase))
		{
			return EPreviewMode::Close;
		}
		return EPreviewMode::Invalid;
	}

	void CancelScheduledScreenshot()
	{
		if (ScreenshotTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(ScreenshotTickerHandle);
			ScreenshotTickerHandle.Reset();
		}
		if (ScreenshotProcessedDelegateHandle.IsValid())
		{
			FScreenshotRequest::OnScreenshotRequestProcessed().Remove(
				ScreenshotProcessedDelegateHandle);
			ScreenshotProcessedDelegateHandle.Reset();
		}
		if (AutoExitTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(AutoExitTickerHandle);
			AutoExitTickerHandle.Reset();
		}
	}

	void ClosePreview()
	{
		CancelScheduledScreenshot();

		// Reset ownership before invoking widget lifecycle code so a close request that
		// re-enters this function cannot operate on the same preview twice.
		UFF_MinigameScreenBase* ExistingScreen = PreviewScreen.Get();
		PreviewScreen.Reset();
		if (ExistingScreen)
		{
			if (PreviewCloseDelegateHandle.IsValid())
			{
				ExistingScreen->OnCloseRequested.Remove(PreviewCloseDelegateHandle);
			}
			PreviewCloseDelegateHandle.Reset();
			ExistingScreen->DeactivateWidget();
			ExistingScreen->RemoveFromParent();
		}
		else
		{
			PreviewCloseDelegateHandle.Reset();
		}
	}

	void HandlePreviewCloseRequested(
		UFF_MinigameScreenBase* RequestedScreen,
		const FGuid RequestedSessionId)
	{
		if (!RequestedScreen
			|| PreviewScreen.Get() != RequestedScreen
			|| RequestedScreen->GetSessionId() != RequestedSessionId)
		{
			return;
		}

		UE_LOG(LogFestaFiestaMinigameUIPreview, Display,
			TEXT("FF.PreviewMinigameUI: preview screen requested close."));
		ClosePreview();
	}

	void AdoptPreviewScreen(UFF_MinigameScreenBase& Screen)
	{
		PreviewScreen = &Screen;
		PreviewCloseDelegateHandle = Screen.OnCloseRequested.AddStatic(
			&HandlePreviewCloseRequested);
	}

	FFF_TargetShootingSnapshot MakeTargetSnapshot(const bool bShowResult)
	{
		FFF_TargetShootingSnapshot Snapshot;
		Snapshot.Session.Context.SessionId = FGuid(
			0xA11CE001, 0x7A2E4A10, 0xB0315A17, 0xF17A0001);
		Snapshot.Session.Context.FacilityInstanceId = FGuid(
			0xB007A001, 0x5A004D22, 0x9001C0DE, 0x00000001);
		Snapshot.Session.Context.FacilityDefinitionId = TEXT("Booth_ActivityGame_Shoot");
		Snapshot.Session.Context.MinigameId = TEXT("TargetShooting");
		Snapshot.Session.Context.RandomSeed = 130013;
		Snapshot.Session.State = bShowResult
			? EFF_MinigameSessionState::Completed
			: EFF_MinigameSessionState::Playing;
		Snapshot.Session.EndReason = bShowResult
			? EFF_MinigameEndReason::TimeExpired
			: EFF_MinigameEndReason::None;
		Snapshot.Session.ElapsedSeconds = bShowResult ? 30.0f : 17.75f;
		Snapshot.Session.RemainingSeconds = bShowResult ? 0.0f : 12.25f;
		Snapshot.Session.Score = bShowResult ? 2480 : 1875;

		Snapshot.TargetSerial = 17;
		Snapshot.bTargetVisible = !bShowResult;
		Snapshot.TargetCenterNormalized = FVector2D(0.61f, 0.47f);
		Snapshot.TargetRadiusNormalized = FVector2D(0.047f, 0.083f);
		Snapshot.PrecisionRadiusRatio = 0.58f;
		Snapshot.BullseyeRadiusRatio = 0.26f;
		Snapshot.TargetLifetimeSeconds = 0.92f;
		Snapshot.TargetRemainingSeconds = bShowResult ? 0.0f : 0.58f;
		Snapshot.TargetAgeSeconds = bShowResult ? 0.92f : 0.34f;
		Snapshot.bBonusTarget = true;
		Snapshot.HitCount = 16;
		Snapshot.MissCount = 2;
		Snapshot.EscapedTargetCount = 1;
		Snapshot.CurrentCombo = bShowResult ? 0 : 6;
		Snapshot.BestCombo = 9;
		Snapshot.LatestFeedbackSerial = bShowResult ? 0 : 4;
		Snapshot.LatestFeedbackType = bShowResult
			? EFF_TargetShootingFeedbackType::None
			: EFF_TargetShootingFeedbackType::Bullseye;
		Snapshot.LatestFeedbackPointNormalized = FVector2D(0.61f, 0.47f);
		Snapshot.LatestScoreDelta = bShowResult ? 0 : 300;
		Snapshot.LatestReactionSeconds = bShowResult ? 0.0f : 0.16f;
		Snapshot.bLatestFeedbackWasBonusTarget = !bShowResult;
		Snapshot.AccuracyPercent = 84.2f;
		Snapshot.HitsPerSecond = 2.35f;
		Snapshot.AverageHitReactionSeconds = 0.23f;
		return Snapshot;
	}

	FFF_SurfingSnapshot MakeSurfSnapshot(const bool bShowResult)
	{
		FFF_SurfingSnapshot Snapshot;
		Snapshot.Session.Context.SessionId = FGuid(
			0xA11CE002, 0x5A8F4C22, 0xA013C0DE, 0xF17A0002);
		Snapshot.Session.Context.FacilityInstanceId = FGuid(
			0xB007A002, 0x5A004D22, 0x9001C0DE, 0x00000002);
		Snapshot.Session.Context.FacilityDefinitionId = TEXT("Booth_ActivityGame_Surfing");
		Snapshot.Session.Context.MinigameId = TEXT("Surfing");
		Snapshot.Session.Context.RandomSeed = 130029;
		Snapshot.Session.State = bShowResult
			? EFF_MinigameSessionState::Completed
			: EFF_MinigameSessionState::Playing;
		Snapshot.Session.EndReason = bShowResult
			? EFF_MinigameEndReason::TimeExpired
			: EFF_MinigameEndReason::None;
		Snapshot.Session.ElapsedSeconds = bShowResult ? 45.0f : 33.6f;
		Snapshot.Session.RemainingSeconds = bShowResult ? 0.0f : 11.4f;
		Snapshot.Session.Score = bShowResult ? 3425 : 2380;

		Snapshot.PlayerPositionNormalized = FVector2D(0.12f, -0.08f);
		Snapshot.ArenaHorizontalMetricScale = 2.18f;
		Snapshot.MovementInput = FVector2D(0.32f, 0.95f);
		Snapshot.CurrentHeading = Snapshot.MovementInput.GetSafeNormal();
		Snapshot.DistanceMeters = bShowResult ? 168.4f : 126.8f;
		Snapshot.CurrentSpeed = bShowResult ? 0.0f : 14.2f;
		Snapshot.Lives = 2;
		Snapshot.MaxLives = 3;
		Snapshot.BoostCharges = 2;
		Snapshot.MaxBoostCharges = 3;
		Snapshot.BoostRemainingSeconds = bShowResult ? 0.0f : 0.85f;
		Snapshot.bBoosting = !bShowResult;
		Snapshot.CollisionCount = 1;
		Snapshot.SmashedCount = 2;
		Snapshot.CoinCount = 9;
		Snapshot.HeartCount = 1;
		Snapshot.BoostPickupCount = 2;
		Snapshot.SteerCount = 18;
		Snapshot.LatestEventSerial = bShowResult ? 0 : 8;
		Snapshot.LatestEventType = bShowResult
			? EFF_SurfingEventType::None
			: EFF_SurfingEventType::BoostActivated;

		auto AddObject = [&Snapshot](
			const int32 ObjectId,
			const EFF_SurfingObjectType Type,
			const FVector2D Position,
			const float Radius)
		{
			FFF_SurfingObjectSnapshot Object;
			Object.ObjectId = ObjectId;
			Object.Type = Type;
			Object.PositionNormalized = Position;
			Object.CollisionRadiusNormalized = Radius;
			Snapshot.VisibleObjects.Add(Object);
		};

		AddObject(11, EFF_SurfingObjectType::Rock, FVector2D(-0.62f, 0.58f), 0.085f);
		AddObject(12, EFF_SurfingObjectType::Log, FVector2D(0.54f, 0.64f), 0.090f);
		AddObject(13, EFF_SurfingObjectType::Buoy, FVector2D(-0.48f, -0.38f), 0.070f);
		AddObject(14, EFF_SurfingObjectType::Coin, FVector2D(0.40f, 0.28f), 0.050f);
		AddObject(15, EFF_SurfingObjectType::Heart, FVector2D(0.68f, -0.54f), 0.055f);
		AddObject(16, EFF_SurfingObjectType::BoostPickup, FVector2D(-0.12f, 0.72f), 0.058f);
		return Snapshot;
	}

	FFF_MinigameResult MakeResult(
		const FGuid& SessionId,
		const FName MinigameId,
		const int32 Score,
		const int32 RewardGold,
		const TCHAR* Grade)
	{
		FFF_MinigameResult Result;
		Result.SessionId = SessionId;
		Result.MinigameId = MinigameId;
		Result.Score = Score;
		Result.RewardGold = RewardGold;
		Result.GradeText = FText::FromString(Grade);
		Result.EndReason = EFF_MinigameEndReason::TimeExpired;
		return Result;
	}

	template <typename TScreen>
	TScreen* CreateDesignerScreen(
		APlayerController& Controller,
		const TCHAR* ScreenClassPath)
	{
		UClass* ScreenClass = LoadClass<TScreen>(nullptr, ScreenClassPath);
		if (!ScreenClass || !ScreenClass->IsChildOf(TScreen::StaticClass()))
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("FF.PreviewMinigameUI: WBP class load failed: %s"),
				ScreenClassPath);
			return nullptr;
		}

		TScreen* Screen = CreateWidget<TScreen>(&Controller, ScreenClass);
		if (!Screen || !Screen->AddToPlayerScreen(PreviewZOrder))
		{
			if (Screen)
			{
				Screen->RemoveFromParent();
			}
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("FF.PreviewMinigameUI: failed to add WBP to the local player screen: %s"),
				ScreenClassPath);
			return nullptr;
		}

		Screen->ActivateWidget();
		if (!Screen->IsUsingDesignerWidgetTree())
		{
			Screen->DeactivateWidget();
			Screen->RemoveFromParent();
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("FF.PreviewMinigameUI: %s did not produce its designer WidgetTree; "
					 "visual QA aborted instead of showing the native fallback."),
				ScreenClassPath);
			return nullptr;
		}
		return Screen;
	}

	bool ShowTargetPreview(APlayerController& Controller, const bool bShowResult)
	{
		UFF_TargetShootingScreen* Screen =
			CreateDesignerScreen<UFF_TargetShootingScreen>(Controller, TargetScreenClassPath);
		if (!Screen)
		{
			return false;
		}

		const FFF_TargetShootingSnapshot Snapshot = MakeTargetSnapshot(bShowResult);
		Screen->SetSessionId(Snapshot.Session.Context.SessionId);
		Screen->ApplySnapshot(Snapshot);
		if (bShowResult)
		{
			Screen->ShowResult(MakeResult(
				Snapshot.Session.Context.SessionId,
				Snapshot.Session.Context.MinigameId,
				Snapshot.Session.Score,
				420,
				TEXT("S RANK / ELITE SHOOTER")));
		}
		AdoptPreviewScreen(*Screen);
		return true;
	}

	bool ShowSurfPreview(APlayerController& Controller, const bool bShowResult)
	{
		UFF_SurfingScreen* Screen =
			CreateDesignerScreen<UFF_SurfingScreen>(Controller, SurfScreenClassPath);
		if (!Screen)
		{
			return false;
		}

		const FFF_SurfingSnapshot Snapshot = MakeSurfSnapshot(bShowResult);
		Screen->SetSessionId(Snapshot.Session.Context.SessionId);
		Screen->ApplySnapshot(Snapshot);
		if (bShowResult)
		{
			Screen->ShowResult(MakeResult(
				Snapshot.Session.Context.SessionId,
				Snapshot.Session.Context.MinigameId,
				Snapshot.Session.Score,
				525,
				TEXT("S RANK / WAVE MASTER")));
		}
		AdoptPreviewScreen(*Screen);
		return true;
	}

	FString ResolveScreenshotPath(const FString& RequestedPath)
	{
		FString ResolvedPath = RequestedPath;
		if (FPaths::IsRelative(ResolvedPath))
		{
			ResolvedPath = FPaths::ProjectSavedDir()
				/ TEXT("Screenshots/MinigameUI")
				/ ResolvedPath;
		}
		ResolvedPath = FPaths::ConvertRelativePathToFull(ResolvedPath);
		ResolvedPath = FPaths::ChangeExtension(ResolvedPath, TEXT("png"));
		FPaths::NormalizeFilename(ResolvedPath);
		return ResolvedPath;
	}

	void ScheduleScreenshot(
		const FString& RequestedPath,
		const EWorldType::Type WorldType,
		const bool bAutoExit)
	{
		CancelScheduledScreenshot();
		const FString ScreenshotPath = ResolveScreenshotPath(RequestedPath);
		const FString ScreenshotDirectory = FPaths::GetPath(ScreenshotPath);
		if (ScreenshotDirectory.IsEmpty()
			|| !IFileManager::Get().MakeDirectory(*ScreenshotDirectory, true))
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("FF.PreviewMinigameUI: cannot create screenshot directory: %s"),
				*ScreenshotDirectory);
			return;
		}

		if (WorldType == EWorldType::PIE)
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Warning,
				TEXT("FF.PreviewMinigameUI: PIE UI capture includes the editor window. "
					 "Use -game (Standalone) for a clean frame."));
		}

		ScreenshotTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([ScreenshotPath, bAutoExit](const float DeltaSeconds)
			{
				(void)DeltaSeconds;
				ScreenshotTickerHandle.Reset();

				UGameViewportClient* GameViewport = GEngine ? GEngine->GameViewport : nullptr;
				if (!PreviewScreen.IsValid()
					|| !GameViewport
					|| !FSlateApplication::IsInitialized()
					|| !GameViewport->GetWindow().IsValid())
				{
					UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
						TEXT("FF.PreviewMinigameUI: no Slate game window; UI screenshot skipped. "
							 "Do not use -NullRHI or a commandlet."));
					return false;
				}

				if (FScreenshotRequest::IsScreenshotRequested())
				{
					UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
						TEXT("FF.PreviewMinigameUI: another screenshot request is pending; "
							 "capture skipped: %s"),
						*ScreenshotPath);
					return false;
				}

				// bShowUI=true is essential: a regular high-resolution scene capture omits UMG.
				if (bAutoExit)
				{
					// A pre-existing deterministic capture may have exactly the same byte size.
					// Preserve both identity signals and require this request to update at least
					// one before treating the processed callback as a successful fresh capture.
					const FFileStatData PreviousScreenshotStat =
						IFileManager::Get().GetStatData(*ScreenshotPath);
					ScreenshotProcessedDelegateHandle =
						FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda(
							[ScreenshotPath, PreviousScreenshotStat]()
							{
								// Screenshot processing broadcasts before the async PNG write is
								// guaranteed to be visible on disk. Defer both delegate removal and
								// stat verification to a ticker; removing this callback from inside
								// itself would immediately destroy its captured path in UE 5.7.
								ScreenshotTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
									FTickerDelegate::CreateLambda(
										[ScreenshotPath, PreviousScreenshotStat, ElapsedSeconds = 0.0f]
										(const float VerifyDeltaSeconds) mutable
										{
											if (ScreenshotProcessedDelegateHandle.IsValid())
											{
												FScreenshotRequest::OnScreenshotRequestProcessed().Remove(
													ScreenshotProcessedDelegateHandle);
												ScreenshotProcessedDelegateHandle.Reset();
											}

											ElapsedSeconds += FMath::Max(
												VerifyDeltaSeconds,
												ScreenshotVerificationIntervalSeconds);
											const FFileStatData CurrentScreenshotStat =
												IFileManager::Get().GetStatData(*ScreenshotPath);
											const bool bPreviousFileWasValid =
												PreviousScreenshotStat.bIsValid
												&& !PreviousScreenshotStat.bIsDirectory;
											const bool bCurrentFileIsValid =
												CurrentScreenshotStat.bIsValid
												&& !CurrentScreenshotStat.bIsDirectory
												&& CurrentScreenshotStat.FileSize > 0;
											const bool bCaptureUpdatedFile = bCurrentFileIsValid
												&& (!bPreviousFileWasValid
													|| CurrentScreenshotStat.ModificationTime
														!= PreviousScreenshotStat.ModificationTime
													|| CurrentScreenshotStat.FileSize
														!= PreviousScreenshotStat.FileSize);
											if (!bCaptureUpdatedFile
												&& ElapsedSeconds < ScreenshotVerificationTimeoutSeconds)
											{
												return true;
											}

											ScreenshotTickerHandle.Reset();
											if (!bCaptureUpdatedFile)
											{
												UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
													TEXT("FF.PreviewMinigameUI: screenshot processing finished "
														 "without creating or updating the exact requested PNG; "
														 "AutoExit cancelled: %s"),
													*ScreenshotPath);
												return false;
											}

											AutoExitTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
												FTickerDelegate::CreateLambda([](const float AutoExitDeltaSeconds)
												{
													(void)AutoExitDeltaSeconds;
													AutoExitTickerHandle.Reset();
													UE_LOG(LogFestaFiestaMinigameUIPreview, Display,
														TEXT("FF.PreviewMinigameUI: screenshot saved; "
															 "requesting graceful AutoExit."));
													FPlatformMisc::RequestExit(
														false,
														TEXT("FF.PreviewMinigameUI.AutoExit"));
													return false;
												}),
												AutoExitDelaySeconds);
											return false;
										}),
									ScreenshotVerificationIntervalSeconds);
							});
				}

				FScreenshotRequest::RequestScreenshot(
					ScreenshotPath,
					true,
					false,
					false);
				UE_LOG(LogFestaFiestaMinigameUIPreview, Display,
					TEXT("FF.PreviewMinigameUI: UI screenshot requested: %s"),
					*ScreenshotPath);
				return false;
			}),
			ScreenshotDelaySeconds);
	}

	void RunPreviewCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("Usage: FF.PreviewMinigameUI "
					 "<Target|TargetResult|Surf|SurfResult|Close> "
					 "[ScreenshotPath] [AutoExit]"));
			return;
		}

		const EPreviewMode Mode = ParseMode(Args[0]);
		const bool bAutoExit = Args.Num() == 3
			&& Args[2].Equals(TEXT("AutoExit"), ESearchCase::IgnoreCase);
		if (Mode == EPreviewMode::Invalid
			|| Args.Num() > 3
			|| (Args.Num() == 3 && !bAutoExit)
			|| (Mode == EPreviewMode::Close && Args.Num() != 1))
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("Usage: FF.PreviewMinigameUI "
					 "<Target|TargetResult|Surf|SurfResult|Close> "
					 "[ScreenshotPath] [AutoExit]"));
			return;
		}
		if (Mode == EPreviewMode::Close)
		{
			ClosePreview();
			UE_LOG(LogFestaFiestaMinigameUIPreview, Display,
				TEXT("FF.PreviewMinigameUI: preview closed."));
			return;
		}

		if (!World
			|| (World->WorldType != EWorldType::Game
				&& World->WorldType != EWorldType::PIE
				&& World->WorldType != EWorldType::GamePreview))
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("FF.PreviewMinigameUI: run this command in PIE, Standalone, or -game."));
			return;
		}
		if (bAutoExit && World->WorldType != EWorldType::Game)
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("FF.PreviewMinigameUI: AutoExit is accepted only in a standalone "
					 "or -game process, never in PIE."));
			return;
		}

		if (const UFF_MinigameSubsystem* MinigameSubsystem =
			World->GetSubsystem<UFF_MinigameSubsystem>();
			MinigameSubsystem && MinigameSubsystem->HasActiveSession())
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("FF.PreviewMinigameUI: an authoritative minigame session is active; "
					 "preview refused without changing that session."));
			return;
		}

		APlayerController* Controller = World->GetFirstPlayerController();
		if (!Controller || !Controller->IsLocalController() || !Controller->GetLocalPlayer())
		{
			UE_LOG(LogFestaFiestaMinigameUIPreview, Error,
				TEXT("FF.PreviewMinigameUI: no local player controller is ready."));
			return;
		}

		ClosePreview();
		// The frontend test map owns its own title/menu widgets. A deterministic
		// visual-QA frame must contain only the requested minigame WBP; this command
		// runs in a disposable preview world and never touches an active session.
		UWidgetLayoutLibrary::RemoveAllWidgets(World);
		bool bShown = false;
		switch (Mode)
		{
		case EPreviewMode::Target:
			bShown = ShowTargetPreview(*Controller, false);
			break;
		case EPreviewMode::TargetResult:
			bShown = ShowTargetPreview(*Controller, true);
			break;
		case EPreviewMode::Surf:
			bShown = ShowSurfPreview(*Controller, false);
			break;
		case EPreviewMode::SurfResult:
			bShown = ShowSurfPreview(*Controller, true);
			break;
		default:
			break;
		}

		if (!bShown)
		{
			return;
		}

		UE_LOG(LogFestaFiestaMinigameUIPreview, Display,
			TEXT("FF.PreviewMinigameUI: deterministic %s WBP preview is visible."),
			*Args[0]);
		if (Args.Num() >= 2 && !Args[1].IsEmpty())
		{
			ScheduleScreenshot(Args[1], World->WorldType, bAutoExit);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs PreviewCommand(
		TEXT("FF.PreviewMinigameUI"),
		TEXT("Shows a deterministic designer-WBP minigame preview. "
			 "FF.PreviewMinigameUI <Target|TargetResult|Surf|SurfResult|Close> "
			 "[ScreenshotPath] [AutoExit]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunPreviewCommand));
}

#endif // !UE_BUILD_SHIPPING
