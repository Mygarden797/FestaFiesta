// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/FF_MinigameSubsystem.h"

#include "Camera/FF_FestivalPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Minigame/FF_MinigameCatalogSubsystem.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_MinigameRuntime.h"
#include "Minigame/FF_TargetShootingRuntime.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/Minigame/FF_MinigameScreenBase.h"
#include "UI/Minigame/FF_SurfingScreen.h"
#include "UI/Minigame/FF_TargetShootingScreen.h"

#define LOCTEXT_NAMESPACE "FFMinigameSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogFestaFiestaMinigame, Log, All);

void UFF_MinigameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Collection.InitializeDependency<UFF_FacilityInfoSubsystem>();
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr)
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(
			this, &ThisClass::HandleWorldSnapshotRestored);
	}
	if (UFF_FacilityInfoSubsystem* FacilityInfo = World
		? World->GetSubsystem<UFF_FacilityInfoSubsystem>()
		: nullptr)
	{
		FacilityInfo->OnMinigameRequested.AddUObject(
			this, &ThisClass::HandleFacilityMinigameRequested);
	}
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr)
	{
		Stage->OnStageChanged.AddUniqueDynamic(this, &ThisClass::HandleStageChanged);
	}
}

void UFF_MinigameSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (UFF_FacilityInfoSubsystem* FacilityInfo = World
		? World->GetSubsystem<UFF_FacilityInfoSubsystem>()
		: nullptr)
	{
		FacilityInfo->OnMinigameRequested.RemoveAll(this);
	}
	if (UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr)
	{
		ZoneSystem->OnWorldChanged.RemoveAll(this);
		ZoneSystem->OnWorldSnapshotRestored.RemoveAll(this);
	}
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr)
	{
		Stage->OnStageChanged.RemoveAll(this);
	}

	if (Runtime && Runtime->IsPlaying())
	{
		Runtime->Cancel(EFF_MinigameEndReason::WorldShutdown);
	}
	TeardownSession();
	Super::Deinitialize();
}

bool UFF_MinigameSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UFF_MinigameSubsystem::Tick(const float DeltaTime)
{
	if (!Runtime || !Runtime->IsPlaying())
	{
		return;
	}

	const FGuid FacilityId = Runtime->GetSnapshot().Context.FacilityInstanceId;
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	const AFFZSFacilityActor* Facility = ZoneSystem
		? ZoneSystem->FindFacility(FacilityId)
		: nullptr;
	if (!Facility || Facility->GetFacilityState() != EFFZSFacilityState::Active ||
		Facility->NeedsRelocation())
	{
		CancelActiveMinigame(EFF_MinigameEndReason::FacilityRemoved);
		return;
	}

	Runtime->Advance(DeltaTime);
	if (Runtime && Runtime->GetSnapshot().State == EFF_MinigameSessionState::Completed)
	{
		CompleteActiveSession();
	}
}

TStatId UFF_MinigameSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_MinigameSubsystem, STATGROUP_Tickables);
}

EFF_MinigameStartResult UFF_MinigameSubsystem::StartMinigame(
	APlayerController* OwningController,
	const FGuid FacilityInstanceId)
{
	if (!IsValid(OwningController) || !OwningController->IsLocalController() ||
		!OwningController->GetLocalPlayer() || OwningController->GetWorld() != GetWorld())
	{
		return EFF_MinigameStartResult::InvalidController;
	}
	if (Runtime || Screen)
	{
		return EFF_MinigameStartResult::AnotherSessionActive;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	if (!Stage || Stage->CurrentStage != EFF_FestivalStage::Live)
	{
		return EFF_MinigameStartResult::NotLiveStage;
	}

	UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	AFFZSFacilityActor* Facility = ZoneSystem
		? ZoneSystem->FindFacility(FacilityInstanceId)
		: nullptr;
	if (!Facility)
	{
		return EFF_MinigameStartResult::FacilityNotFound;
	}
	if (Facility->GetFacilityState() != EFFZSFacilityState::Active)
	{
		return EFF_MinigameStartResult::FacilityNotActive;
	}
	if (Facility->NeedsRelocation())
	{
		return EFF_MinigameStartResult::FacilityNeedsRelocation;
	}

	UFF_MinigameCatalogSubsystem* Catalog = GameInstance
		? GameInstance->GetSubsystem<UFF_MinigameCatalogSubsystem>()
		: nullptr;
	UFF_MinigameDefinition* CandidateDefinition = Catalog
		? Catalog->FindDefinitionForFacility(Facility->GetFacilitySpec().FacilityDefinitionId)
		: nullptr;
	if (!CandidateDefinition)
	{
		return EFF_MinigameStartResult::NoMinigameForFacility;
	}
	TArray<FText> DefinitionErrors;
	if (!CandidateDefinition->IsDefinitionValid(DefinitionErrors))
	{
		for (const FText& Error : DefinitionErrors)
		{
			UE_LOG(LogFestaFiestaMinigame, Error, TEXT("미니게임 정의 오류: %s"), *Error.ToString());
		}
		return EFF_MinigameStartResult::DefinitionInvalid;
	}

	UClass* RuntimeClass = CandidateDefinition->RuntimeClass.Get();
	if (!RuntimeClass || !RuntimeClass->IsChildOf(UFF_MinigameRuntime::StaticClass()))
	{
		return EFF_MinigameStartResult::RuntimeCreationFailed;
	}
	UFF_MinigameRuntime* CandidateRuntime = NewObject<UFF_MinigameRuntime>(this, RuntimeClass);
	if (!CandidateRuntime)
	{
		return EFF_MinigameStartResult::RuntimeCreationFailed;
	}

	FFF_MinigameSessionContext Context;
	Context.SessionId = FGuid::NewGuid();
	Context.FacilityInstanceId = FacilityInstanceId;
	Context.FacilityDefinitionId = Facility->GetFacilitySpec().FacilityDefinitionId;
	Context.MinigameId = CandidateDefinition->MinigameId;
	Context.RandomSeed = static_cast<int32>(HashCombine(
		GetTypeHash(Context.SessionId),
		GetTypeHash(Context.FacilityInstanceId)));
	FString RuntimeFailure;
	if (!CandidateRuntime->Initialize(CandidateDefinition, Context, &RuntimeFailure))
	{
		UE_LOG(LogFestaFiestaMinigame, Error,
			TEXT("%s runtime 초기화 실패: %s"),
			*CandidateDefinition->MinigameId.ToString(), *RuntimeFailure);
		return EFF_MinigameStartResult::RuntimeCreationFailed;
	}

	UClass* ScreenClass = CandidateDefinition->ScreenClass.LoadSynchronous();
	if (!ScreenClass || !ScreenClass->IsChildOf(UFF_MinigameScreenBase::StaticClass()))
	{
		if (CandidateDefinition->IsA<UFF_TargetShootingDefinition>())
		{
			ScreenClass = UFF_TargetShootingScreen::StaticClass();
		}
		else if (CandidateDefinition->IsA<UFF_SurfingDefinition>())
		{
			ScreenClass = UFF_SurfingScreen::StaticClass();
		}

		if (ScreenClass)
		{
			UE_LOG(LogFestaFiestaMinigame, Warning,
				TEXT("%s WBP를 불러오지 못해 네이티브 폴백 화면을 사용합니다: %s"),
				*CandidateDefinition->MinigameId.ToString(),
				*CandidateDefinition->ScreenClass.ToString());
		}
	}
	if (!ScreenClass || !ScreenClass->IsChildOf(UFF_MinigameScreenBase::StaticClass()))
	{
		return EFF_MinigameStartResult::ScreenUnavailable;
	}

	UFF_MinigameScreenBase* CandidateScreen =
		CreateWidget<UFF_MinigameScreenBase>(OwningController, ScreenClass);
	if (!CandidateScreen || !CandidateScreen->AddToPlayerScreen(200))
	{
		if (CandidateScreen)
		{
			CandidateScreen->RemoveFromParent();
		}
		return EFF_MinigameStartResult::ScreenUnavailable;
	}
	CandidateScreen->ActivateWidget();

	CandidateScreen->SetSessionId(Context.SessionId);
	if (!CandidateScreen->BindRuntime(CandidateRuntime))
	{
		CandidateScreen->DeactivateWidget();
		CandidateScreen->RemoveFromParent();
		return EFF_MinigameStartResult::ScreenUnavailable;
	}

	// Commit only after every fallible dependency has been prepared. No half-open session can
	// leave modal input or a stale widget behind.
	ActiveDefinition = CandidateDefinition;
	Runtime = CandidateRuntime;
	Screen = CandidateScreen;
	bRewardCommitted = false;
	bCompletionHandled = false;
	Screen->OnCloseRequested.AddUObject(this, &ThisClass::HandleScreenCloseRequested);

	if (AFF_FestivalPlayerController* FestivalController =
		Cast<AFF_FestivalPlayerController>(OwningController))
	{
		ModalController = FestivalController;
		bPreviousCursorVisible = FestivalController->bShowMouseCursor;
		FestivalController->SetModalUiActive(true);
		FestivalController->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		FestivalController->SetInputMode(InputMode);
	}
	if (UFF_FacilityInfoSubsystem* FacilityInfo =
		World->GetSubsystem<UFF_FacilityInfoSubsystem>())
	{
		FacilityInfo->ClearSelectionIfMatches(
			FacilityInstanceId, EFF_FacilityInfoContext::Live);
	}

	UE_LOG(LogFestaFiestaMinigame, Log,
		TEXT("미니게임 시작: Id=%s Session=%s Facility=%s Seed=%d"),
		*Context.MinigameId.ToString(), *Context.SessionId.ToString(),
		*Context.FacilityInstanceId.ToString(), Context.RandomSeed);
	OnMinigameStarted.Broadcast(Context.SessionId);
	return EFF_MinigameStartResult::Success;
}

void UFF_MinigameSubsystem::CancelActiveMinigame(const EFF_MinigameEndReason Reason)
{
	if (!Runtime && !Screen)
	{
		return;
	}
	if (Runtime && Runtime->IsPlaying())
	{
		Runtime->Cancel(Reason);
	}
	TeardownSession();
}

FFF_MinigameSessionSnapshot UFF_MinigameSubsystem::GetActiveSnapshot() const
{
	return Runtime ? Runtime->GetSnapshot() : FFF_MinigameSessionSnapshot();
}

FText UFF_MinigameSubsystem::GetStartResultText(const EFF_MinigameStartResult Result)
{
	switch (Result)
	{
	case EFF_MinigameStartResult::Success:
		return LOCTEXT("StartSuccess", "게임을 시작했습니다.");
	case EFF_MinigameStartResult::InvalidController:
		return LOCTEXT("InvalidController", "로컬 플레이어를 찾을 수 없습니다.");
	case EFF_MinigameStartResult::AnotherSessionActive:
		return LOCTEXT("AnotherSession", "이미 다른 미니게임이 진행 중입니다.");
	case EFF_MinigameStartResult::NotLiveStage:
		return LOCTEXT("NotLive", "라이브 운영 시간에만 게임을 시작할 수 있습니다.");
	case EFF_MinigameStartResult::FacilityNotFound:
		return LOCTEXT("FacilityNotFound", "설치된 시설을 찾을 수 없습니다.");
	case EFF_MinigameStartResult::FacilityNotActive:
		return LOCTEXT("FacilityNotActive", "운영 중인 시설에서만 게임을 시작할 수 있습니다.");
	case EFF_MinigameStartResult::FacilityNeedsRelocation:
		return LOCTEXT("FacilityNeedsRelocation", "재배치가 필요한 시설에서는 게임을 시작할 수 없습니다.");
	case EFF_MinigameStartResult::NoMinigameForFacility:
		return LOCTEXT("NoMinigame", "이 시설에 연결된 미니게임이 없습니다.");
	case EFF_MinigameStartResult::DefinitionInvalid:
		return LOCTEXT("DefinitionInvalid", "미니게임 설정이 올바르지 않습니다.");
	case EFF_MinigameStartResult::RuntimeCreationFailed:
		return LOCTEXT("RuntimeFailed", "게임 규칙을 준비하지 못했습니다.");
	case EFF_MinigameStartResult::ScreenUnavailable:
	default:
		return LOCTEXT("ScreenUnavailable", "게임 화면을 준비하지 못했습니다.");
	}
}

void UFF_MinigameSubsystem::HandleFacilityMinigameRequested(const FGuid FacilityInstanceId)
{
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const EFF_MinigameStartResult Result = StartMinigame(Controller, FacilityInstanceId);
	if (Result == EFF_MinigameStartResult::Success)
	{
		return;
	}
	if (UFF_FacilityInfoSubsystem* FacilityInfo = GetWorld()
		? GetWorld()->GetSubsystem<UFF_FacilityInfoSubsystem>()
		: nullptr)
	{
		FacilityInfo->SetFeedback(GetStartResultText(Result), true);
	}
}

void UFF_MinigameSubsystem::HandleScreenCloseRequested(
	UFF_MinigameScreenBase* RequestingScreen,
	const FGuid SessionId)
{
	if (!Runtime || Screen != RequestingScreen ||
		Runtime->GetSnapshot().Context.SessionId != SessionId)
	{
		return;
	}
	CancelActiveMinigame(EFF_MinigameEndReason::PlayerCancelled);
}

void UFF_MinigameSubsystem::CompleteActiveSession()
{
	if (bCompletionHandled || !Runtime ||
		Runtime->GetSnapshot().State != EFF_MinigameSessionState::Completed)
	{
		return;
	}
	bCompletionHandled = true;

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	const UFF_FestivalDefinition* FestivalDefinition = Progression
		? Progression->GetDefinition()
		: nullptr;
	const UFF_MinigameDefinition* RuntimeDefinition = Runtime->GetDefinition();
	int32 RewardCap = 0;
	if (Progression)
	{
		// A non-negative value is authored per minigame. -1 preserves compatibility
		// with older definitions by falling back to the festival-wide balance value.
		const int32 FestivalFallbackGold = FestivalDefinition
			? FestivalDefinition->MinigameRewardMaxGold
			: 0;
		RewardCap = RuntimeDefinition
			? RuntimeDefinition->ResolveRewardGoldAtMaxScore(FestivalFallbackGold)
			: FestivalFallbackGold;
	}
	const FFF_MinigameResult Result = Runtime->BuildResult(RewardCap);

	if (!bRewardCommitted)
	{
		// Set the guard before touching the economy boundary. Re-entrant UI callbacks can never
		// duplicate this session's ledger entry.
		bRewardCommitted = true;
		if (Progression && Result.RewardGold > 0)
		{
			Progression->AddRevenue(
				Result.RewardGold,
				EFF_EarnReason::MinigameReward,
				Runtime->GetSnapshot().Context.FacilityDefinitionId);
		}
	}
	if (Screen)
	{
		Screen->ShowResult(Result);
	}
	OnMinigameFinished.Broadcast(Result);
}

void UFF_MinigameSubsystem::TeardownSession()
{
	if (Screen)
	{
		Screen->OnCloseRequested.RemoveAll(this);
		Screen->DeactivateWidget();
		Screen->RemoveFromParent();
	}

	RestoreControllerInput();
	Screen = nullptr;
	Runtime = nullptr;
	ActiveDefinition = nullptr;
	ModalController.Reset();
	bPreviousCursorVisible = false;
	bRewardCommitted = false;
	bCompletionHandled = false;
}

void UFF_MinigameSubsystem::RestoreControllerInput()
{
	AFF_FestivalPlayerController* Controller = ModalController.Get();
	if (!Controller)
	{
		return;
	}
	Controller->SetModalUiActive(false);
	Controller->bShowMouseCursor = bPreviousCursorVisible;
	if (bPreviousCursorVisible)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(InputMode);
	}
	else
	{
		Controller->SetInputMode(FInputModeGameOnly());
	}
}

void UFF_MinigameSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage,
	const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;
	if (NewStage != EFF_FestivalStage::Live)
	{
		CancelActiveMinigame(EFF_MinigameEndReason::StageChanged);
	}
}

void UFF_MinigameSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	if (Runtime && Event.Type == EFFZSCommandType::Remove &&
		Event.FacilityInstanceId == Runtime->GetSnapshot().Context.FacilityInstanceId)
	{
		CancelActiveMinigame(EFF_MinigameEndReason::FacilityRemoved);
	}
}

void UFF_MinigameSubsystem::HandleWorldSnapshotRestored()
{
	CancelActiveMinigame(EFF_MinigameEndReason::WorldRestored);
}

#undef LOCTEXT_NAMESPACE
