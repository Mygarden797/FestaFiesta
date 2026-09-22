// FF_KeyBindingRuntimeSubsystem.cpp
// 역할: 사용자가 저장한 키 설정을 실제 게임 입력에 적용한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_KeyBindingRuntimeSubsystem.h"

#include "Components/InputComponent.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputDeveloperSettings.h"
#include "EnhancedInputLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "TimerManager.h"
#include "UI/FF_UILog.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"
#include "UI/Settings/FF_KeyBindingDefinitions.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
	constexpr TCHAR CameraContextPath[] =
		TEXT("/Game/FestaFiesta/Input/IMC_Camera.IMC_Camera");
	constexpr TCHAR CharacterContextPath[] =
		TEXT("/Game/FestaFiesta/Input/Character/IMC_Character.IMC_Character");

	bool IsGameplayContext(const EFF_KeyBindingContext Context)
	{
		return Context == EFF_KeyBindingContext::Camera
			|| Context == EFF_KeyBindingContext::Character;
	}
}

void UFF_EnhancedInputUserSettingsBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Do not SaveConfig here. This is an explicit runtime feature bootstrap, not
	// ownership of the team's project-wide input configuration file.
	if (UEnhancedInputDeveloperSettings* Settings =
		GetMutableDefault<UEnhancedInputDeveloperSettings>())
	{
		Settings->bEnableUserSettings = true;
	}
}

void UFF_KeyBindingProfileAction::Configure(
	const FName MappingName,
	const FText& DisplayName,
	const FText& DisplayCategory,
	const bool bInTriggerWhenPaused)
{
	ActionDescription = DisplayName;
	bTriggerWhenPaused = bInTriggerWhenPaused;
	bConsumeInput = false;
	bConsumesActionAndAxisMappings = false;

	UPlayerMappableKeySettings* KeySettings =
		NewObject<UPlayerMappableKeySettings>(this, NAME_None, RF_Transient);
	KeySettings->Name = MappingName;
	KeySettings->DisplayName = DisplayName;
	KeySettings->DisplayCategory = DisplayCategory;
	PlayerMappableKeySettings = KeySettings;
}

void UFF_KeyBindingRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UEnhancedInputLocalPlayerSubsystem>();

	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(
		this, &ThisClass::HandlePostLogin);
}

void UFF_KeyBindingRuntimeSubsystem::Deinitialize()
{
	bCaptureInProgress = false;
	RemoveOptionToggleBindings(ResolvePlayerController());
	RestoreGameplayMappings();

	if (UserSettings)
	{
		UserSettings->OnSettingsChanged.RemoveDynamic(
			this, &ThisClass::HandleUserSettingsChanged);
		if (bProfileContextRegistered && ProfileMappingContext)
		{
			UserSettings->UnregisterInputMappingContext(ProfileMappingContext);
		}
	}

	if (bCapturedOriginalOpenCloseKeys)
	{
		if (UFF_FestivalSettingsDeveloperSettings* Settings =
			GetMutableDefault<UFF_FestivalSettingsDeveloperSettings>())
		{
			Settings->OpenCloseKeys = OriginalOpenCloseKeys;
		}
	}

	if (PostLoginHandle.IsValid())
	{
		FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
		PostLoginHandle.Reset();
	}

	GameplayMappingTargets.Reset();
	ResetKeyBindingEditState();
	KeyBindingDraftChanged.Clear();
	ProfileActions.Reset();
	ProfileMappingContext = nullptr;
	CameraMappingContext = nullptr;
	CharacterMappingContext = nullptr;
	UserSettings = nullptr;
	CurrentController.Reset();
	bProfileContextRegistered = false;
	bGameplayTargetsResolved = false;

	Super::Deinitialize();
}

void UFF_KeyBindingRuntimeSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	if (APlayerController* PreviousController = CurrentController.Get();
		PreviousController && PreviousController != NewPlayerController)
	{
		RemoveOptionToggleBindings(PreviousController);
	}

	CurrentController = NewPlayerController;
	EnsureReady();
}

bool UFF_KeyBindingRuntimeSubsystem::EnsureReady()
{
	// Also assert the runtime feature flag here for hot-reload/late-created
	// subsystem scenarios where the engine bootstrap may have run earlier.
	if (UEnhancedInputDeveloperSettings* DeveloperSettings =
		GetMutableDefault<UEnhancedInputDeveloperSettings>())
	{
		DeveloperSettings->bEnableUserSettings = true;
	}

	UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = GetLocalPlayer()
		? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (!EnhancedSubsystem)
	{
		return false;
	}

	if (!EnhancedSubsystem->GetUserSettings())
	{
		// Safe and idempotent. The Enhanced Input subsystem postpones creation
		// until its owning PlayerController has a valid EnhancedPlayerInput.
		EnhancedSubsystem->InitalizeUserSettings();
	}

	UEnhancedInputUserSettings* NewUserSettings = EnhancedSubsystem->GetUserSettings();
	if (!NewUserSettings)
	{
		return false;
	}

	if (UserSettings != NewUserSettings)
	{
		if (UserSettings)
		{
			UserSettings->OnSettingsChanged.RemoveDynamic(
				this, &ThisClass::HandleUserSettingsChanged);
		}
		UserSettings = NewUserSettings;
		// A transaction baseline belongs to one concrete Enhanced Input settings
		// instance. Rebase on the next Begin call if that instance is replaced.
		ResetKeyBindingEditState();
		UserSettings->OnSettingsChanged.AddUniqueDynamic(
			this, &ThisClass::HandleUserSettingsChanged);
		bProfileContextRegistered = false;
	}

	BuildProfileContext();
	if (!ProfileMappingContext)
	{
		return false;
	}

	if (!bProfileContextRegistered)
	{
		UserSettings->RegisterInputMappingContext(ProfileMappingContext);
		bProfileContextRegistered = UserSettings->IsMappingContextRegistered(ProfileMappingContext);
	}

	ResolveGameplayMappingTargets();
	ApplyGameplayMappings();
	ApplyOptionToggleBindings();
	return bProfileContextRegistered;
}

FKey UFF_KeyBindingRuntimeSubsystem::GetCurrentKey(const FName MappingName) const
{
	if (UserSettings)
	{
		if (const FPlayerKeyMapping* Mapping = UserSettings->FindCurrentMappingForSlot(
			MappingName, EPlayerMappableKeySlot::First))
		{
			const FKey CurrentKey = Mapping->GetCurrentKey();
			if (CurrentKey.IsValid())
			{
				return CurrentKey;
			}
		}
	}

	if (const FFF_KeyBindingDefinition* Definition =
		FFKeyBindingDefinitions::Find(MappingName))
	{
		return Definition->DefaultKey;
	}
	return EKeys::Invalid;
}

TArray<FKey> UFF_KeyBindingRuntimeSubsystem::GetOpenCloseKeys() const
{
	TArray<FKey> Keys;
	Keys.Reserve(2);
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		if (Definition.Context != EFF_KeyBindingContext::System)
		{
			continue;
		}

		const FKey Key = GetCurrentKey(Definition.MappingName);
		if (Key.IsValid())
		{
			Keys.AddUnique(Key);
		}
	}
	return Keys;
}

void UFF_KeyBindingRuntimeSubsystem::RefreshMappings()
{
	if (!UserSettings)
	{
		EnsureReady();
		return;
	}

	ApplyGameplayMappings();
	ApplyOptionToggleBindings();
}

void UFF_KeyBindingRuntimeSubsystem::SetCaptureInProgress(const bool bInCaptureInProgress)
{
	if (bCaptureInProgress == bInCaptureInProgress)
	{
		return;
	}

	if (bInCaptureInProgress && !UserSettings)
	{
		EnsureReady();
	}

	bCaptureInProgress = bInCaptureInProgress;
	if (bCaptureInProgress)
	{
		RemoveOptionToggleBindings(ResolvePlayerController());
		if (UFF_FestivalSettingsDeveloperSettings* Settings =
			GetMutableDefault<UFF_FestivalSettingsDeveloperSettings>())
		{
			Settings->OpenCloseKeys.Reset();
		}
	}
	else
	{
		ApplyOptionToggleBindings();
	}
}

bool UFF_KeyBindingRuntimeSubsystem::BeginKeyBindingEdit()
{
	if (bKeyBindingEditActive)
	{
		return true;
	}
	if (!EnsureReady() || !UserSettings)
	{
		return false;
	}

	if (!CaptureKeyBindingEditSnapshot())
	{
		ResetKeyBindingEditState();
		return false;
	}

	bKeyBindingEditActive = true;
	SetPendingKeyBindingChanges(false);
	return true;
}

bool UFF_KeyBindingRuntimeSubsystem::ApplyKeyBindingDraft()
{
	// Begin must happen before MapPlayerKey/ResetAllPlayerKeysInRow so the
	// snapshot can never accidentally capture the already-edited value.
	if (!bKeyBindingEditActive || !UserSettings)
	{
		return false;
	}

	UserSettings->ApplySettings();
	RefreshMappings();
	SetPendingKeyBindingChanges(IsCurrentDraftDifferentFromSnapshot());
	return true;
}

bool UFF_KeyBindingRuntimeSubsystem::CommitKeyBindingEdit()
{
	if (!EnsureReady() || !UserSettings)
	{
		return false;
	}

	// This is the only key-binding transaction path that writes persistence.
	UserSettings->ApplySettings();
	RefreshMappings();
	UserSettings->AsyncSaveSettings();
	ResetKeyBindingEditState();
	return true;
}

bool UFF_KeyBindingRuntimeSubsystem::RollbackKeyBindingEdit()
{
	if (!bKeyBindingEditActive)
	{
		return true;
	}
	if (!UserSettings || KeyBindingEditSnapshot.Num() != FFKeyBindingDefinitions::GetAll().Num())
	{
		return false;
	}

	bool bRestoredAllRows = true;
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		const FKey* SnapshotKey = KeyBindingEditSnapshot.Find(Definition.MappingName);
		if (!SnapshotKey || !SnapshotKey->IsValid())
		{
			bRestoredAllRows = false;
			continue;
		}

		FMapPlayerKeyArgs Args;
		Args.MappingName = Definition.MappingName;
		Args.Slot = EPlayerMappableKeySlot::First;
		Args.NewKey = *SnapshotKey;
		Args.bCreateMatchingSlotIfNeeded = true;
		FGameplayTagContainer FailureReasons;
		UserSettings->MapPlayerKey(Args, FailureReasons);
		bRestoredAllRows &= FailureReasons.IsEmpty()
			&& GetCurrentKey(Definition.MappingName) == *SnapshotKey;
	}

	// Apply restores live gameplay IMCs and raw option keys only. There is
	// intentionally no AsyncSaveSettings call on the discard path.
	UserSettings->ApplySettings();
	RefreshMappings();
	if (bRestoredAllRows)
	{
		ResetKeyBindingEditState();
	}
	else
	{
		SetPendingKeyBindingChanges(IsCurrentDraftDifferentFromSnapshot());
	}
	return bRestoredAllRows;
}

bool UFF_KeyBindingRuntimeSubsystem::CaptureKeyBindingEditSnapshot()
{
	KeyBindingEditSnapshot.Reset();
	const TArray<FFF_KeyBindingDefinition>& Definitions = FFKeyBindingDefinitions::GetAll();
	KeyBindingEditSnapshot.Reserve(Definitions.Num());
	for (const FFF_KeyBindingDefinition& Definition : Definitions)
	{
		const FKey CurrentKey = GetCurrentKey(Definition.MappingName);
		if (!CurrentKey.IsValid())
		{
			KeyBindingEditSnapshot.Reset();
			return false;
		}
		KeyBindingEditSnapshot.Add(Definition.MappingName, CurrentKey);
	}
	return KeyBindingEditSnapshot.Num() == Definitions.Num();
}

bool UFF_KeyBindingRuntimeSubsystem::IsCurrentDraftDifferentFromSnapshot() const
{
	if (!bKeyBindingEditActive)
	{
		return false;
	}
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		const FKey* SnapshotKey = KeyBindingEditSnapshot.Find(Definition.MappingName);
		if (!SnapshotKey || GetCurrentKey(Definition.MappingName) != *SnapshotKey)
		{
			return true;
		}
	}
	return false;
}

void UFF_KeyBindingRuntimeSubsystem::SetPendingKeyBindingChanges(const bool bInHasPendingChanges)
{
	if (bHasPendingKeyBindingChanges == bInHasPendingChanges)
	{
		return;
	}
	bHasPendingKeyBindingChanges = bInHasPendingChanges;
	KeyBindingDraftChanged.Broadcast(bHasPendingKeyBindingChanges);
}

void UFF_KeyBindingRuntimeSubsystem::ResetKeyBindingEditState()
{
	bKeyBindingEditActive = false;
	KeyBindingEditSnapshot.Reset();
	SetPendingKeyBindingChanges(false);
}

void UFF_KeyBindingRuntimeSubsystem::BuildProfileContext()
{
	if (ProfileMappingContext)
	{
		return;
	}

	ProfileMappingContext = NewObject<UInputMappingContext>(
		this, TEXT("IMC_FF_RuntimePlayerProfile"), RF_Transient);
	if (!ProfileMappingContext)
	{
		return;
	}
	ProfileMappingContext->ContextDescription =
		NSLOCTEXT("FFKeyBindingRuntime", "ProfileContextDescription", "Festa Fiesta! 사용자 키 설정");

	const TArray<FFF_KeyBindingDefinition>& Definitions = FFKeyBindingDefinitions::GetAll();
	ProfileActions.Reserve(Definitions.Num());
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		const FFF_KeyBindingDefinition& Definition = Definitions[Index];
		UFF_KeyBindingProfileAction* Action = NewObject<UFF_KeyBindingProfileAction>(
			this,
			FName(*FString::Printf(TEXT("IA_FF_Profile_%02d"), Index)),
			RF_Transient);
		if (!Action)
		{
			continue;
		}

		Action->Configure(
			Definition.MappingName,
			Definition.ActionLabel,
			FFKeyBindingDefinitions::GetContextLabel(Definition.Context),
			Definition.Context == EFF_KeyBindingContext::System);
		ProfileActions.Add(Action);
		ProfileMappingContext->MapKey(Action, Definition.DefaultKey);
	}
}

void UFF_KeyBindingRuntimeSubsystem::ResolveGameplayMappingTargets()
{
	if (bGameplayTargetsResolved)
	{
		return;
	}

	CameraMappingContext = LoadObject<UInputMappingContext>(nullptr, CameraContextPath);
	CharacterMappingContext = LoadObject<UInputMappingContext>(nullptr, CharacterContextPath);
	GameplayMappingTargets.Reset();

	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		if (!IsGameplayContext(Definition.Context))
		{
			continue;
		}

		UInputMappingContext* Context = Definition.Context == EFF_KeyBindingContext::Camera
			? CameraMappingContext.Get()
			: CharacterMappingContext.Get();
		if (!Context)
		{
			continue;
		}

		const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
		const int32 MappingIndex = Mappings.IndexOfByPredicate(
			[&Definition](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action
					&& Mapping.Action->GetFName() == Definition.SourceActionName
					&& Mapping.Key == Definition.DefaultKey;
			});
		if (MappingIndex == INDEX_NONE)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("UFF_KeyBindingRuntimeSubsystem: %s / %s 기본 매핑을 IMC에서 찾지 못했습니다."),
				*Definition.MappingName.ToString(), *Definition.DefaultKey.ToString());
			continue;
		}

		FGameplayMappingTarget& Target = GameplayMappingTargets.AddDefaulted_GetRef();
		Target.Context = Context;
		Target.MappingIndex = MappingIndex;
		Target.MappingName = Definition.MappingName;
		Target.OriginalKey = Mappings[MappingIndex].Key;
	}

	bGameplayTargetsResolved = CameraMappingContext != nullptr || CharacterMappingContext != nullptr;
}

void UFF_KeyBindingRuntimeSubsystem::ApplyGameplayMappings()
{
	if (bApplyingMappings || !UserSettings)
	{
		return;
	}
	TGuardValue<bool> ApplyingGuard(bApplyingMappings, true);

	TSet<UInputMappingContext*> ChangedContexts;
	for (const FGameplayMappingTarget& Target : GameplayMappingTargets)
	{
		UInputMappingContext* Context = Target.Context.Get();
		if (!Context || !Context->GetMappings().IsValidIndex(Target.MappingIndex))
		{
			continue;
		}

		const FKey DesiredKey = GetCurrentKey(Target.MappingName);
		if (!DesiredKey.IsValid())
		{
			continue;
		}

		FEnhancedActionKeyMapping& Mapping = Context->GetMapping(Target.MappingIndex);
		if (Mapping.Key != DesiredKey)
		{
			// Intentionally no Modify()/MarkPackageDirty()/SavePackage call. This
			// is a per-process mirror and never changes the source asset on disk.
			Mapping.Key = DesiredKey;
			ChangedContexts.Add(Context);
		}
	}

	for (UInputMappingContext* Context : ChangedContexts)
	{
		UEnhancedInputLibrary::RequestRebuildControlMappingsUsingContext(Context, true);
	}
}

void UFF_KeyBindingRuntimeSubsystem::RestoreGameplayMappings()
{
	TSet<UInputMappingContext*> ChangedContexts;
	for (const FGameplayMappingTarget& Target : GameplayMappingTargets)
	{
		UInputMappingContext* Context = Target.Context.Get();
		if (!Context || !Context->GetMappings().IsValidIndex(Target.MappingIndex))
		{
			continue;
		}

		FEnhancedActionKeyMapping& Mapping = Context->GetMapping(Target.MappingIndex);
		if (Mapping.Key != Target.OriginalKey)
		{
			Mapping.Key = Target.OriginalKey;
			ChangedContexts.Add(Context);
		}
	}

	for (UInputMappingContext* Context : ChangedContexts)
	{
		UEnhancedInputLibrary::RequestRebuildControlMappingsUsingContext(Context, true);
	}
}

void UFF_KeyBindingRuntimeSubsystem::ApplyOptionToggleBindings()
{
	APlayerController* Controller = ResolvePlayerController();
	// The pause/settings stack is owned exclusively by the festival controller.
	// Do not consume keys in frontend, minigame test, or editor utility worlds.
	if (!Cast<AFF_FestivalPlayerController>(Controller))
	{
		return;
	}
	CurrentController = Controller;

	RemoveOptionToggleBindings(Controller);
	if (bCaptureInProgress)
	{
		return;
	}

	UFF_FestivalSettingsDeveloperSettings* Settings =
		GetMutableDefault<UFF_FestivalSettingsDeveloperSettings>();
	if (!Settings)
	{
		return;
	}
	if (!bCapturedOriginalOpenCloseKeys)
	{
		OriginalOpenCloseKeys = Settings->OpenCloseKeys;
		bCapturedOriginalOpenCloseKeys = true;
	}

	Settings->OpenCloseKeys = GetOpenCloseKeys();

	RemoveConflictingDebugBindings(Controller);
	UInputComponent* InputComponent = Controller->InputComponent;
	if (!InputComponent)
	{
		return;
	}

	for (const FKey& Key : Settings->OpenCloseKeys)
	{
		FInputKeyBinding& Binding = InputComponent->BindKey(
			Key, IE_Pressed, this, &ThisClass::HandleOptionsToggleInput);
		Binding.bExecuteWhenPaused = true;
		Binding.bConsumeInput = true;
	}
}

void UFF_KeyBindingRuntimeSubsystem::RemoveOptionToggleBindings(APlayerController* Controller)
{
	if (!Controller || !Controller->InputComponent)
	{
		return;
	}

	UFF_FestivalSettingsSubsystem* SettingsSubsystem =
		UFF_FestivalSettingsSubsystem::Get(Controller);
	Controller->InputComponent->KeyBindings.RemoveAll(
		[this, SettingsSubsystem](const FInputKeyBinding& Binding)
		{
			return Binding.KeyDelegate.IsBoundToObject(this)
				|| (SettingsSubsystem && Binding.KeyDelegate.IsBoundToObject(SettingsSubsystem));
		});
}

void UFF_KeyBindingRuntimeSubsystem::RemoveConflictingDebugBindings(
	APlayerController* Controller) const
{
	UPlayerInput* PlayerInput = Controller ? Controller->PlayerInput : nullptr;
	if (!PlayerInput)
	{
		return;
	}

	TArray<FKey> ExclusiveOptionKeys = GetOpenCloseKeys();
	// F5 is project-reserved even before a saved profile has been applied, so
	// PIE can never fall through to Shader Complexity.
	ExclusiveOptionKeys.AddUnique(EKeys::F5);
	PlayerInput->DebugExecBindings.RemoveAll([&ExclusiveOptionKeys](const FKeyBind& Binding)
	{
		return ExclusiveOptionKeys.Contains(Binding.Key);
	});
}

APlayerController* UFF_KeyBindingRuntimeSubsystem::ResolvePlayerController() const
{
	if (APlayerController* Controller = CurrentController.Get())
	{
		return Controller;
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
}

void UFF_KeyBindingRuntimeSubsystem::HandlePostLogin(
	AGameModeBase* GameMode,
	APlayerController* NewPlayer)
{
	if (!GameMode || !Cast<AFF_FestivalPlayerController>(NewPlayer)
		|| NewPlayer->GetLocalPlayer() != GetLocalPlayer())
	{
		return;
	}

	CurrentController = NewPlayer;
	EnsureReady();

	// Re-own the bindings on the next tick as well. This makes delegate order
	// irrelevant if another subsystem bound its legacy ESC/F5 handlers in the
	// same PostLogin broadcast.
	if (UWorld* World = NewPlayer->GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::HandleDeferredControllerReady));
	}
}

void UFF_KeyBindingRuntimeSubsystem::HandleDeferredControllerReady()
{
	EnsureReady();
}

void UFF_KeyBindingRuntimeSubsystem::HandleUserSettingsChanged(
	UEnhancedInputUserSettings* ChangedSettings)
{
	if (ChangedSettings == UserSettings)
	{
		RefreshMappings();
	}
}

void UFF_KeyBindingRuntimeSubsystem::HandleOptionsToggleInput(FKey PressedKey)
{
	if (bCaptureInProgress)
	{
		return;
	}

	APlayerController* Controller = ResolvePlayerController();
	if (UFF_FestivalSettingsSubsystem* SettingsSubsystem =
		UFF_FestivalSettingsSubsystem::Get(Controller))
	{
		SettingsSubsystem->ToggleFestivalSettings(Controller);
	}
}
