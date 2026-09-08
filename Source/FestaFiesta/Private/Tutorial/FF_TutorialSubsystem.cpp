// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Tutorial/FF_DialogueScriptData.h"

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
}

void UFF_TutorialSubsystem::Deinitialize()
{
	UnregisterDebugConsoleCommands();
	StopScript();
	Super::Deinitialize();
}

const FFF_DialogueLine& UFF_TutorialSubsystem::GetCurrentLine() const
{
	if (!ActiveScript || !ActiveScript->Lines.IsValidIndex(CurrentLineIndex))
	{
		return GetEmptyLine();
	}
	return ActiveScript->Lines[CurrentLineIndex];
}

bool UFF_TutorialSubsystem::IsWaitingForAction() const
{
	return bDialogueActive && GetCurrentLine().RequiresPlayerAction();
}

bool UFF_TutorialSubsystem::PlayScript(UFF_DialogueScriptData* Script)
{
	if (!Script || Script->Lines.Num() == 0)
	{
		UE_LOG(LogFestaFiestaTutorial, Warning, TEXT("[튜토리얼] 재생할 대사가 없다."));
		return false;
	}

	ActiveScript = Script;
	CurrentLineIndex = INDEX_NONE;

	if (!bDialogueActive)
	{
		bDialogueActive = true;
		OnDialogueActiveChanged.Broadcast(true);
	}

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 대사 시작. Script=%s 줄=%d"),
		*Script->ScriptId.ToString(), Script->Lines.Num());

	GoToLine(0);
	return true;
}

void UFF_TutorialSubsystem::GoToLine(const int32 LineIndex)
{
	if (!ActiveScript)
	{
		return;
	}

	if (!ActiveScript->Lines.IsValidIndex(LineIndex))
	{
		FinishScript();
		return;
	}

	CurrentLineIndex = LineIndex;

	const FFF_DialogueLine& Line = ActiveScript->Lines[CurrentLineIndex];
	ApplyHighlight(Line.HighlightAnchor);

	OnLineChanged.Broadcast(Line);
}

void UFF_TutorialSubsystem::Advance()
{
	if (!bDialogueActive || !ActiveScript)
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

	GoToLine(CurrentLineIndex + 1);
}

void UFF_TutorialSubsystem::SelectChoice(const int32 ChoiceIndex)
{
	if (!bDialogueActive || !ActiveScript)
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

	const int32 Target = ActiveScript->FindLineIndexByLabel(JumpTo);
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
	if (!bDialogueActive || !ActiveScript)
	{
		return;
	}

	// 건너뛸 수 없는 줄을 만날 때까지 앞으로 간다.
	//
	// 전부 건너뛰지 않는 이유는, 조작을 배우는 부분까지 넘기면 그 뒤로
	// 무엇을 눌러야 하는지 모른 채 게임이 시작되기 때문이다.
	int32 Index = CurrentLineIndex;
	int32 Guard = 0;
	const int32 MaxSteps = ActiveScript->Lines.Num() + 1;

	while (ActiveScript->Lines.IsValidIndex(Index) && Guard++ < MaxSteps)
	{
		if (!ActiveScript->Lines[Index].CanSkip())
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

	GoToLine(CurrentLineIndex + 1);
}

void UFF_TutorialSubsystem::FinishScript()
{
	UFF_DialogueScriptData* Next = ActiveScript
		? ActiveScript->NextScript.LoadSynchronous()
		: nullptr;

	if (Next && Next != ActiveScript)
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

	ActiveScript = nullptr;
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
					ActiveScript ? *ActiveScript->ScriptId.ToString() : TEXT("없음"),
					CurrentLineIndex,
					ActiveScript ? ActiveScript->Lines.Num() : 0);

				UE_LOG(LogFestaFiestaTutorial, Display,
					TEXT("  조작 대기=%s 강조=%s 등록된 대상 %d개"),
					IsWaitingForAction() ? TEXT("예") : TEXT("아니오"),
					CurrentAnchor.IsNone() ? TEXT("없음") : *CurrentAnchor.ToString(),
					AnchorWidgets.Num());
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
