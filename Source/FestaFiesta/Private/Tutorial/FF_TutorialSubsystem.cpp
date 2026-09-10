// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Blueprint/UserWidget.h"
#include "Camera/FF_FestivalGameMode.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/DataTable.h"
#include "Tutorial/FF_DialogueRow.h"
#include "Tutorial/FF_DialogueScriptData.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Tutorial/FF_TutorialSettings.h"

namespace
{
	// 아무 줄도 없을 때 돌려줄 빈 줄. 호출부가 널 검사를 안 하게 한다.
	const FFF_DialogueLine& GetEmptyLine()
	{
		static const FFF_DialogueLine Empty;
		return Empty;
	}
}

bool UFF_TutorialSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UFF_TutorialSubsystem* UFF_TutorialSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UFF_TutorialSubsystem>() : nullptr;
}

void UFF_TutorialSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RegisterDebugConsoleCommands();

	// 바로 시작하지 않는다. 레벨이 아직 정리되기 전이라 화면이 어수선하다.
	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	AutoStartCountdown = Settings ? FMath::Max(0.f, Settings->StartDelaySeconds) : 0.f;

	BindGameSignals();
}

void UFF_TutorialSubsystem::Deinitialize()
{
	UnbindGameSignals();
	UnregisterDebugConsoleCommands();
	StopScript();
	RemoveWidgets();
	Super::Deinitialize();
}

const FFF_DialogueLine& UFF_TutorialSubsystem::GetCurrentLine() const
{
	if (!ActiveLines.IsValidIndex(CurrentLineIndex))
	{
		return GetEmptyLine();
	}
	return ActiveLines[CurrentLineIndex];
}

bool UFF_TutorialSubsystem::IsWaitingForAction() const
{
	return bDialogueActive && GetCurrentLine().RequiresPlayerAction();
}

int32 UFF_TutorialSubsystem::ResolveNextLineIndex() const
{
	const FFF_DialogueLine& Line = GetCurrentLine();

	// 합류 지점이 지정돼 있으면 그리로. 분기가 반대편으로 새는 걸 막는다.
	if (!Line.NextLabel.IsNone())
	{
		const int32 Target = FindLineIndexByLabel(Line.NextLabel);
		if (Target != INDEX_NONE)
		{
			return Target;
		}

		// 이름을 잘못 적었다. 진행이 막히는 것보다는 다음 줄로 가는 게 낫다.
		UE_LOG(LogFestaFiestaTutorial, Warning,
			TEXT("[튜토리얼] NextLabel %s 를 찾지 못했다. 다음 줄로 간다."),
			*Line.NextLabel.ToString());
	}

	return CurrentLineIndex + 1;
}

int32 UFF_TutorialSubsystem::FindLineIndexByLabel(const FName Label) const
{
	if (Label.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < ActiveLines.Num(); ++Index)
	{
		if (ActiveLines[Index].Label == Label)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool UFF_TutorialSubsystem::StartLines(const TArray<FFF_DialogueLine>& Lines, const FName ScriptId)
{
	if (Lines.Num() == 0)
	{
		return false;
	}

	// 위젯이 없으면 대사가 재생돼도 화면에 아무것도 안 나온다.
	// 재생 직전에 확인하는 이유는, 레벨이 뜨는 시점에는 플레이어 컨트롤러가
	// 아직 없을 수 있기 때문이다.
	EnsureWidgets();

	ActiveLines = Lines;
	ActiveScriptId = ScriptId;
	CurrentLineIndex = INDEX_NONE;

	if (!bDialogueActive)
	{
		bDialogueActive = true;
		OnDialogueActiveChanged.Broadcast(true);
	}

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 대사 시작. Script=%s 줄=%d"),
		*ScriptId.ToString(), ActiveLines.Num());

	GoToLine(0);
	return true;
}

bool UFF_TutorialSubsystem::PlayTable(UDataTable* Table)
{
	if (!Table)
	{
		UE_LOG(LogFestaFiestaTutorial, Warning, TEXT("[튜토리얼] 재생할 표가 없다."));
		return false;
	}

	TArray<FFF_DialogueLine> Lines;
	if (!FFF_DialogueTable::BuildLines(Table, Lines))
	{
		UE_LOG(LogFestaFiestaTutorial, Warning,
			TEXT("[튜토리얼] 표 %s 에서 읽을 줄이 없다. 행 구조체가 FFF_DialogueRow 인지 확인할 것."),
			*Table->GetName());
		return false;
	}

	// 표에는 이어지는 씬 개념이 없다. 필요하면 호출부가 다음 표를 부른다.
	PendingNextScript = nullptr;
	return StartLines(Lines, FName(*Table->GetName()));
}

bool UFF_TutorialSubsystem::PlayScript(UFF_DialogueScriptData* Script)
{
	if (!Script || Script->Lines.Num() == 0)
	{
		UE_LOG(LogFestaFiestaTutorial, Warning, TEXT("[튜토리얼] 재생할 대사가 없다."));
		return false;
	}

	PendingNextScript = Script->NextScript.LoadSynchronous();
	return StartLines(Script->Lines, Script->ScriptId);
}

void UFF_TutorialSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (AutoStartCountdown >= 0.f)
	{
		AutoStartCountdown -= DeltaTime;
		if (AutoStartCountdown < 0.f)
		{
			TryAutoStart();
		}
	}

	if (IsWaitingForAction())
	{
		TickCameraWatch();
	}
}

void UFF_TutorialSubsystem::BindGameSignals()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bBoundZoneWorld)
	{
		if (UFFZSWorldSubsystem* ZoneWorld = World->GetSubsystem<UFFZSWorldSubsystem>())
		{
			ZoneWorld->OnWorldChanged.AddDynamic(this, &UFF_TutorialSubsystem::HandleWorldChanged);
			bBoundZoneWorld = true;
		}
	}

	if (!bBoundStage)
	{
		const UGameInstance* GameInstance = World->GetGameInstance();
		if (UFF_FestivalStageSubsystem* Stage =
			GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr)
		{
			Stage->OnStageChanged.AddDynamic(this, &UFF_TutorialSubsystem::HandleStageChanged);
			bBoundStage = true;
		}
	}
}

void UFF_TutorialSubsystem::UnbindGameSignals()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		bBoundZoneWorld = false;
		bBoundStage = false;
		return;
	}

	if (bBoundZoneWorld)
	{
		if (UFFZSWorldSubsystem* ZoneWorld = World->GetSubsystem<UFFZSWorldSubsystem>())
		{
			ZoneWorld->OnWorldChanged.RemoveDynamic(this, &UFF_TutorialSubsystem::HandleWorldChanged);
		}
		bBoundZoneWorld = false;
	}

	if (bBoundStage)
	{
		const UGameInstance* GameInstance = World->GetGameInstance();
		if (UFF_FestivalStageSubsystem* Stage =
			GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr)
		{
			Stage->OnStageChanged.RemoveDynamic(this, &UFF_TutorialSubsystem::HandleStageChanged);
		}
		bBoundStage = false;
	}
}

void UFF_TutorialSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	// 설치만 본다. 철거나 개폐는 튜토리얼에서 기다릴 일이 없다.
	if (Event.Type == EFFZSCommandType::Install)
	{
		NotifyAction(EFF_TutorialWaitCondition::FacilityPlaced);
	}
}

void UFF_TutorialSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage, const EFF_FestivalStage NewStage)
{
	(void)PreviousStage; (void)NewStage;
	NotifyAction(EFF_TutorialWaitCondition::StageAdvanced);
}

void UFF_TutorialSubsystem::EnsureWidgets()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return;
	}

	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	if (!Settings)
	{
		return;
	}

	// 오버레이를 먼저 띄운다. 대사창이 그 위에 와야 덮개에 가려지지 않는다.
	if (!OverlayWidget)
	{
		if (UClass* OverlayClass = Settings->OverlayWidgetClass.LoadSynchronous())
		{
			OverlayWidget = CreateWidget<UUserWidget>(Controller, OverlayClass);
			if (OverlayWidget)
			{
				OverlayWidget->AddToViewport(Settings->OverlayZOrder);
			}
		}
	}

	if (!DialogueBoxWidget)
	{
		if (UClass* BoxClass = Settings->DialogueBoxWidgetClass.LoadSynchronous())
		{
			DialogueBoxWidget = CreateWidget<UUserWidget>(Controller, BoxClass);
			if (DialogueBoxWidget)
			{
				DialogueBoxWidget->AddToViewport(Settings->DialogueBoxZOrder);
			}
		}
	}
}

void UFF_TutorialSubsystem::RemoveWidgets()
{
	if (DialogueBoxWidget)
	{
		DialogueBoxWidget->RemoveFromParent();
		DialogueBoxWidget = nullptr;
	}

	if (OverlayWidget)
	{
		OverlayWidget->RemoveFromParent();
		OverlayWidget = nullptr;
	}
}

void UFF_TutorialSubsystem::TryAutoStart()
{
	// 이미 다른 경로로 시작했으면 건드리지 않는다.
	if (bDialogueActive)
	{
		return;
	}

	// 레벨 지정이 먼저다. 튜토리얼 레벨에만 대사를 걸 수 있어야 한다.
	const UWorld* World = GetWorld();
	const AFF_FestivalGameMode* GameMode =
		World ? Cast<AFF_FestivalGameMode>(World->GetAuthGameMode()) : nullptr;

	// 표가 애셋보다 먼저다. 둘 다 지정하는 경우는 없겠지만 순서는 정해둔다.
	if (GameMode)
	{
		if (UDataTable* Table = GameMode->TutorialStartTable.LoadSynchronous())
		{
			PlayTable(Table);
			return;
		}
	}

	UFF_DialogueScriptData* Script = nullptr;
	if (GameMode)
	{
		Script = GameMode->TutorialStartScript.LoadSynchronous();
	}

	// 레벨에 없으면 전역 설정을 본다. 보통 비어 있다.
	if (!Script)
	{
		if (const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get())
		{
			Script = Settings->DefaultStartScript.LoadSynchronous();
		}
	}

	if (!Script)
	{
		// 튜토리얼이 없는 레벨이다. 정상이므로 경고하지 않는다.
		return;
	}

	PlayScript(Script);
}

AFF_OverviewCameraPawn* UFF_TutorialSubsystem::FindOverviewPawn() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AFF_OverviewCameraPawn> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

void UFF_TutorialSubsystem::ResetCameraBaseline()
{
	bCameraBaselineValid = false;

	const AFF_OverviewCameraPawn* Pawn = FindOverviewPawn();
	if (!Pawn)
	{
		return;
	}

	// 표시 상태를 쓴다. 목표 상태는 비공개고, 감시에는 보이는 값이면 충분하다.
	const FFF_OverviewCameraState State = Pawn->GetCameraState();
	CameraBaselineFocus = State.FocusLocation;
	CameraBaselineArmLength = State.ArmLength;
	bCameraBaselineValid = true;
}

void UFF_TutorialSubsystem::TickCameraWatch()
{
	const EFF_TutorialWaitCondition Condition = GetCurrentLine().WaitCondition;
	if (Condition != EFF_TutorialWaitCondition::CameraMoved
		&& Condition != EFF_TutorialWaitCondition::CameraZoomed)
	{
		return;
	}

	// 기준점이 없으면 지금 잡는다. 카메라가 대사보다 늦게 뜨는 경우가 있다.
	if (!bCameraBaselineValid)
	{
		ResetCameraBaseline();
		return;
	}

	const AFF_OverviewCameraPawn* Pawn = FindOverviewPawn();
	if (!Pawn)
	{
		return;
	}

	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	if (!Settings)
	{
		return;
	}

	const FFF_OverviewCameraState State = Pawn->GetCameraState();

	if (Condition == EFF_TutorialWaitCondition::CameraMoved)
	{
		// 높이는 지형 추종이 바꾸므로 세지 않는다. 수평 이동만 본다.
		const FVector2D Moved(
			State.FocusLocation.X - CameraBaselineFocus.X,
			State.FocusLocation.Y - CameraBaselineFocus.Y);

		if (Moved.Size() >= Settings->CameraMoveThresholdCm)
		{
			NotifyAction(EFF_TutorialWaitCondition::CameraMoved);
		}
		return;
	}

	// 줌은 거리 차가 아니라 비율로 본다. 멀리서 100cm 와 가까이서 100cm 는 체감이 다르다.
	if (CameraBaselineArmLength > KINDA_SMALL_NUMBER)
	{
		const float Ratio = FMath::Abs(State.ArmLength - CameraBaselineArmLength)
			/ CameraBaselineArmLength;

		if (Ratio >= Settings->CameraZoomThresholdRatio)
		{
			NotifyAction(EFF_TutorialWaitCondition::CameraZoomed);
		}
	}
}

void UFF_TutorialSubsystem::GoToLine(const int32 LineIndex)
{
	if (!ActiveLines.IsValidIndex(LineIndex))
	{
		FinishScript();
		return;
	}

	CurrentLineIndex = LineIndex;

	const FFF_DialogueLine& Line = ActiveLines[CurrentLineIndex];
	ApplyHighlight(Line.HighlightAnchor);

	// 조작 대기 줄에 들어갈 때마다 기준점을 다시 잡는다.
	// 안 그러면 이전 줄에서 움직인 거리가 누적돼 즉시 통과한다.
	if (Line.RequiresPlayerAction())
	{
		ResetCameraBaseline();
	}

	OnLineChanged.Broadcast(Line);
}

void UFF_TutorialSubsystem::Advance()
{
	if (!bDialogueActive)
	{
		return;
	}

	const FFF_DialogueLine& Line = GetCurrentLine();

	// 조작을 기다리는 줄에서는 클릭으로 넘어가지 않는다.
	// 넘어가게 두면 안내만 읽고 실제로는 해보지 않은 채 지나간다.
	if (Line.RequiresPlayerAction())
	{
		return;
	}

	// 선택지도 마찬가지다. 고르지 않고 넘어가면 분기가 무의미해진다.
	if (Line.Kind == EFF_DialogueLineKind::Choice)
	{
		return;
	}

	GoToLine(ResolveNextLineIndex());
}

void UFF_TutorialSubsystem::SelectChoice(const int32 ChoiceIndex)
{
	if (!bDialogueActive)
	{
		return;
	}

	const FFF_DialogueLine& Line = GetCurrentLine();
	if (Line.Kind != EFF_DialogueLineKind::Choice || !Line.Choices.IsValidIndex(ChoiceIndex))
	{
		return;
	}

	const FName JumpTo = Line.Choices[ChoiceIndex].JumpToLabel;

	// 목적지가 없으면 그냥 다음 줄로. 선택지가 대사만 다르고 흐름은 같은 경우다.
	if (JumpTo.IsNone())
	{
		GoToLine(CurrentLineIndex + 1);
		return;
	}

	const int32 Target = FindLineIndexByLabel(JumpTo);
	if (Target == INDEX_NONE)
	{
		// 라벨을 잘못 적었다. 진행이 막히는 것보다는 다음 줄로 넘기는 게 낫다.
		UE_LOG(LogFestaFiestaTutorial, Warning,
			TEXT("[튜토리얼] 라벨 %s 를 찾지 못했다. 다음 줄로 넘어간다."), *JumpTo.ToString());
		GoToLine(CurrentLineIndex + 1);
		return;
	}

	GoToLine(Target);
}

void UFF_TutorialSubsystem::SkipNarration()
{
	if (!bDialogueActive)
	{
		return;
	}

	// 건너뛸 수 없는 줄을 만날 때까지 앞으로 간다.
	//
	// 전부 건너뛰지 않는 이유는, 조작을 배우는 부분까지 넘기면 그 뒤로
	// 무엇을 눌러야 하는지 모른 채 게임이 시작되기 때문이다.
	int32 Index = CurrentLineIndex;
	int32 Guard = 0;
	const int32 MaxSteps = ActiveLines.Num() + 1;

	while (ActiveLines.IsValidIndex(Index) && Guard++ < MaxSteps)
	{
		if (!ActiveLines[Index].CanSkip())
		{
			break;
		}
		++Index;
	}

	if (Index == CurrentLineIndex)
	{
		// 지금 줄부터 건너뛸 수 없다. 아무것도 하지 않는다.
		return;
	}

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 서사 건너뜀. %d → %d"), CurrentLineIndex, Index);

	GoToLine(Index);
}

void UFF_TutorialSubsystem::NotifyAction(const EFF_TutorialWaitCondition Condition)
{
	if (!bDialogueActive || Condition == EFF_TutorialWaitCondition::None)
	{
		return;
	}

	const FFF_DialogueLine& Line = GetCurrentLine();
	if (!Line.RequiresPlayerAction() || Line.WaitCondition != Condition)
	{
		// 기다리던 게 아니다. 플레이어가 다른 걸 만졌을 뿐이므로 조용히 넘긴다.
		return;
	}

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 조작 확인. 조건=%d"), static_cast<int32>(Condition));

	GoToLine(ResolveNextLineIndex());
}

void UFF_TutorialSubsystem::FinishScript()
{
	UFF_DialogueScriptData* Next = PendingNextScript;
	PendingNextScript = nullptr;

	if (Next)
	{
		// 이어지는 씬이 있으면 대사창을 내리지 않고 그대로 넘어간다.
		// 내렸다 다시 올리면 화면이 한 번 깜빡인다.
		PlayScript(Next);
		return;
	}

	StopScript();
}

void UFF_TutorialSubsystem::StopScript()
{
	if (!bDialogueActive)
	{
		return;
	}

	ActiveLines.Reset();
	ActiveScriptId = NAME_None;
	PendingNextScript = nullptr;
	CurrentLineIndex = INDEX_NONE;
	bDialogueActive = false;

	ApplyHighlight(NAME_None);
	OnDialogueActiveChanged.Broadcast(false);

	UE_LOG(LogFestaFiestaTutorial, Log, TEXT("[튜토리얼] 대사 종료."));
}

void UFF_TutorialSubsystem::ApplyHighlight(const FName AnchorId)
{
	if (CurrentAnchor == AnchorId)
	{
		return;
	}

	CurrentAnchor = AnchorId;
	OnHighlightAnchorChanged.Broadcast(AnchorId);
}

void UFF_TutorialSubsystem::RegisterAnchor(const FName AnchorId, UWidget* Widget)
{
	if (AnchorId.IsNone() || !Widget)
	{
		return;
	}
	AnchorWidgets.Add(AnchorId, Widget);

	// 지금 가리키려던 대상이 방금 들어왔으면 다시 알린다.
	// 화면이 대사보다 늦게 뜨는 경우가 있어서, 그때 강조가 조용히 빠진다.
	if (CurrentAnchor == AnchorId)
	{
		OnHighlightAnchorChanged.Broadcast(AnchorId);
	}
}

void UFF_TutorialSubsystem::UnregisterAnchor(const FName AnchorId)
{
	AnchorWidgets.Remove(AnchorId);
}

UWidget* UFF_TutorialSubsystem::FindAnchorWidget(const FName AnchorId) const
{
	const TWeakObjectPtr<UWidget>* Found = AnchorWidgets.Find(AnchorId);
	return Found ? Found->Get() : nullptr;
}

void UFF_TutorialSubsystem::RegisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	UnregisterDebugConsoleCommands();

	if (!ConsoleManager.FindConsoleObject(TEXT("FF.Tutorial.Skip")))
	{
		SkipConsoleCommand = ConsoleManager.RegisterConsoleCommand(
			TEXT("FF.Tutorial.Skip"),
			TEXT("진행 중인 대사를 끝낸다."),
			FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
			{
				StopScript();
			}));
	}

	if (!ConsoleManager.FindConsoleObject(TEXT("FF.Tutorial.Stat")))
	{
		StatConsoleCommand = ConsoleManager.RegisterConsoleCommand(
			TEXT("FF.Tutorial.Stat"),
			TEXT("대사 진행 상태를 출력한다."),
			FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
			{
				UE_LOG(LogFestaFiestaTutorial, Display,
					TEXT("[튜토리얼] 진행=%s Script=%s 줄=%d/%d"),
					bDialogueActive ? TEXT("예") : TEXT("아니오"),
					ActiveScriptId.IsNone() ? TEXT("없음") : *ActiveScriptId.ToString(),
					CurrentLineIndex,
					ActiveLines.Num());

				UE_LOG(LogFestaFiestaTutorial, Display,
					TEXT("  조작 대기=%s 조건=%d 강조=%s 등록된 대상 %d개"),
					IsWaitingForAction() ? TEXT("예") : TEXT("아니오"),
					static_cast<int32>(GetCurrentLine().WaitCondition),
					CurrentAnchor.IsNone() ? TEXT("없음") : *CurrentAnchor.ToString(),
					AnchorWidgets.Num());

				// 무엇을 구독했는지 본다. 안 붙었으면 조작해도 안 넘어간다.
				// 어디서 대사를 가져왔는지 본다. 두 군데서 올 수 있어 헷갈리기 쉽다.
				const UWorld* World = GetWorld();
				const AFF_FestivalGameMode* GameMode =
					World ? Cast<AFF_FestivalGameMode>(World->GetAuthGameMode()) : nullptr;
				const bool bLevelTable = GameMode && !GameMode->TutorialStartTable.IsNull();
				const bool bLevelScript = GameMode && !GameMode->TutorialStartScript.IsNull();

				UE_LOG(LogFestaFiestaTutorial, Display,
					TEXT("  시작 대사 출처: %s"),
					bLevelTable ? TEXT("GameMode 표")
					: bLevelScript ? TEXT("GameMode 애셋")
					: TEXT("Project Settings 또는 없음"));

				UE_LOG(LogFestaFiestaTutorial, Display,
					TEXT("  구독: Zone=%s 단계=%s / 카메라 기준점=%s"),
					bBoundZoneWorld ? TEXT("예") : TEXT("아니오"),
					bBoundStage ? TEXT("예") : TEXT("아니오"),
					bCameraBaselineValid ? TEXT("있음") : TEXT("없음"));
			}));
	}
}

void UFF_TutorialSubsystem::UnregisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	if (SkipConsoleCommand)
	{
		ConsoleManager.UnregisterConsoleObject(SkipConsoleCommand);
	}
	SkipConsoleCommand = nullptr;

	if (StatConsoleCommand)
	{
		ConsoleManager.UnregisterConsoleObject(StatConsoleCommand);
	}
	StatConsoleCommand = nullptr;
}
