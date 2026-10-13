// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "Camera/FF_FestivalGameMode.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/DataTable.h"
#include "Tutorial/FF_DialogueRow.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Save/FF_SaveSubsystem.h"
#include "Tutorial/FF_TutorialMarker.h"
#include "Tutorial/FF_TutorialExitWidget.h"
#include "Tutorial/FF_TutorialSkipWidget.h"
#include "UI/Atlas/FF_AtlasCountryScreen.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Frontend/FF_PlayHubScreen.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "Tutorial/FF_DialogueScriptData.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Tutorial/FF_TutorialSettings.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIFlowSettings.h"

namespace
{
	// 아무 줄도 없을 때 돌려줄 빈 줄. 호출부가 널 검사를 안 하게 한다.
	const FFF_DialogueLine& GetEmptyLine()
	{
		static const FFF_DialogueLine Empty;
		return Empty;
	}

	/* 아틀라스 카탈로그의 튜토리얼 칸에 덮어쓴 잠금(SyncTutorialLevelLock).

	   카탈로그는 레벨이 바뀌어도 남는 애셋이라, 레벨마다 새로 생기는 이 서브시스템이 아니라 여기에 둔다.
	   원래 값은 덮어쓰기 직전에 받아 두었다가 되돌릴 때 쓴다.
	*/
	struct FTutorialLevelLockState
	{
		TWeakObjectPtr<UFF_FestivalAtlasCatalog> Catalog;
		FName FestivalId;
		bool bOriginalUnlocked = true;
		FText OriginalReason;
		bool bOverridden = false;
	};

	FTutorialLevelLockState& GetTutorialLevelLockState()
	{
		static FTutorialLevelLockState State;
		return State;
	}

	// 튜토리얼을 지나기 전에 잠근 다른 축제 칸들의 원래 값. 되돌릴 때 쓴다(SyncFestivalsLockedUntilTutorial).
	struct FTutorialFestivalGateState
	{
		struct FSavedEntry
		{
			FName FestivalId;
			bool bOriginalUnlocked = true;
			FText OriginalReason;
		};

		TWeakObjectPtr<UFF_FestivalAtlasCatalog> Catalog;
		TArray<FSavedEntry> Entries;
		bool bOverridden = false;
	};

	FTutorialFestivalGateState& GetTutorialFestivalGateState()
	{
		static FTutorialFestivalGateState State;
		return State;
	}

	FFF_FestivalAtlasEntry* FindAtlasEntry(UFF_FestivalAtlasCatalog& Catalog, const FName FestivalId)
	{
		return Catalog.Entries.FindByPredicate(
			[FestivalId](const FFF_FestivalAtlasEntry& Entry) { return Entry.FestivalId == FestivalId; });
	}

	/* 카탈로그에서 튜토리얼 레벨 칸을 찾는다. 축제 이름이 같거나, 그 칸의 레벨(맵) 이름이 같으면 그 칸이다.

	   튜토리얼 설정의 이름은 맵 이름(Tutorial_ver2)이고 카탈로그 칸의 이름은 Tutorial 이다(2026-10-01 ver2 맵 교체).
	   지도에서 들어가면 카탈로그 이름으로, 맵을 바로 열면 맵 이름으로 축제를 가리켜서 둘 다 받는다.
	*/
	FFF_FestivalAtlasEntry* FindTutorialAtlasEntry(UFF_FestivalAtlasCatalog& Catalog, const FName TutorialId)
	{
		const FString TutorialName = TutorialId.ToString();
		return Catalog.Entries.FindByPredicate([&](const FFF_FestivalAtlasEntry& Entry)
		{
			return Entry.FestivalId == TutorialId
				|| (!Entry.Level.IsNull() && Entry.Level.ToSoftObjectPath().GetAssetName() == TutorialName);
		});
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

	// 이어하기 지정은 레벨을 열기 전에 걸리고 레벨이 뜬 뒤 소비된다. 지금이 확실히 읽히는 때다.
	if (const UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		bWorldStartedFromContinue = Save->HasPendingContinue();
	}

	// 지도 화면이 뜨기 전에 튜토리얼 레벨 잠금을 맞춘다. 화면은 뜰 때 잠금 값을 읽어 핀을 그린다.
	SyncTutorialLevelLock();
	SyncFestivalsLockedUntilTutorial();

	BindGameSignals();
}

void UFF_TutorialSubsystem::Deinitialize()
{
	UnbindGameSignals();
	UnregisterDebugConsoleCommands();
	StopScript();
	RemoveWidgets();
	RestoreTutorialLevelLock();
	RestoreFestivalsLockedUntilTutorial();
	Super::Deinitialize();
}

const FFF_DialogueLine& UFF_TutorialSubsystem::GetCurrentLine() const
{
	// 확인 중에는 원래 대사 대신 확인 줄을 보여준다.
	if (bAwaitingSkipConfirm)
	{
		return SkipConfirmLine;
	}

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

FName UFF_TutorialSubsystem::ResolveCurrentFestivalId() const
{
	// 규칙은 저장 쪽에 하나만 둔다. 헤더 주석 참고.
	return UFF_SaveSubsystem::ResolveCurrentFestivalId(this);
}

void UFF_TutorialSubsystem::SetPlaybackScope(
	const EFF_TutorialScope Scope, const FName FestivalId)
{
	ActiveScope = Scope;
	ActiveScopeFestivalId = FestivalId;
}

void UFF_TutorialSubsystem::RecordScopeCompleted()
{
	UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this);
	if (!Save)
	{
		return;
	}

	switch (ActiveScope)
	{
	case EFF_TutorialScope::LobbyIntro:
		Save->MarkLobbyIntroSeen();
		break;

	case EFF_TutorialScope::Festival:
		Save->MarkFestivalTutorialSeen(ActiveScopeFestivalId);
		break;

	case EFF_TutorialScope::Briefing:
		// 비서 브리핑은 매일 새로 짓는 대사라 봤다는 기록이 필요 없다.
		break;

	default:
		/* 범위를 모르는 재생이다. 콘솔이나 UI 가 직접 부른 경우라 여기서 기록하면
		   플레이어가 보지도 않은 튜토리얼이 봤다고 남는다. 로그만 남긴다.
		*/
		UE_LOG(LogFestaFiestaTutorial, Verbose,
			TEXT("[튜토리얼] 범위 없는 재생이 끝났다. 저장에 남기지 않는다. Script=%s"),
			*ActiveScriptId.ToString());
		break;
	}
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

bool UFF_TutorialSubsystem::PlayLines(const TArray<FFF_DialogueLine>& Lines, const FName ScriptId, const EFF_TutorialScope Scope)
{
	if (bDialogueActive || Lines.Num() == 0)
	{
		return false;
	}

	PendingNextScript = nullptr;
	SetPlaybackScope(Scope, NAME_None);
	if (!StartLines(Lines, ScriptId))
	{
		SetPlaybackScope(EFF_TutorialScope::None, NAME_None);
		return false;
	}
	return true;
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

	// 끝까지 봤거나 건너뛰었으면(또는 새 게임으로 기록이 비면) 바로 지도 잠금에 반영한다.
	SyncTutorialLevelLock();
	SyncFestivalsLockedUntilTutorial();

	if (AutoStartCountdown >= 0.f)
	{
		AutoStartCountdown -= DeltaTime;
		if (AutoStartCountdown < 0.f)
		{
			TryAutoStart();
		}
	}

	// 카드 · 카테고리 버튼처럼 다시 만들어지는 위젯을 가리키는 중이면 짧은 주기로 다시 찾는다.
	if (!CurrentAnchor.IsNone())
	{
		DynamicAnchorScanCountdown -= DeltaTime;
		if (DynamicAnchorScanCountdown <= 0.f)
		{
			DynamicAnchorScanCountdown = 0.1f;
			RefreshDynamicAnchor();
		}
	}

	// 대사창이 비켜 설 배치 패널도 같은 주기로 다시 본다. 대사창은 매 프레임 이 값만 읽는다.
	DialogueLiftScanCountdown -= DeltaTime;
	if (DialogueLiftScanCountdown <= 0.f)
	{
		DialogueLiftScanCountdown = 0.1f;
		RefreshDialogueLiftPanel();
	}

	if (IsWaitingForAction())
	{
		TickCameraWatch();
		TickActionWatch();

		// 탭 대기 줄이면 탭 바를 주기적으로 다시 확인한다. TryBindPrepHudBar 주석 참고.
		if (GetCurrentLine().WaitCondition == EFF_TutorialWaitCondition::PrepTabOpened)
		{
			PrepBarScanCountdown -= DeltaTime;
			if (PrepBarScanCountdown <= 0.f)
			{
				PrepBarScanCountdown = 0.5f;
				if (TryBindPrepHudBar())
				{
					CheckPrepTabAlreadyOpen();
				}
			}
		}
	}

	/* 준비 화면 패널을 보기 전용으로 둘지 정한다. IsPrepPanelViewOnly 참고.

	   대사 중 패널은 탭 대기 줄에서 연 것이 대부분이지만, 플레이어가 대사 도중 직접 열 수도 있다.
	   그래서 탭 바를 아직 구독하지 않았으면 주기적으로 찾는다.
	*/
	bPrepPanelViewOnly = false;
	if (bDialogueActive && !IsWaitingForAction())
	{
		PrepPanelScanCountdown -= DeltaTime;
		if (PrepPanelScanCountdown <= 0.f)
		{
			PrepPanelScanCountdown = 0.5f;
			TryBindPrepHudBar();
		}

		if (const UFF_PrepHudBar* Bar = BoundPrepHudBar.Get())
		{
			bPrepPanelViewOnly = Bar->GetSelection().bHasSelection;
		}
	}

	/* 로비 버튼 구독. 플레이 허브 화면이 뜬 틱에 바로 훑는다.

	   예전에는 1초마다 한 번 훑고, 한 번 물면 더 훑지 않았다. 그래서 두 경우에 새 게임
	   신호를 놓쳤다. 플레이 허브가 뜨고 1초 안에 새 게임을 누르면 아직 구독 전이었고,
	   허브가 새로 만들어지면 옛 허브만 물고 있었다. 신호를 놓치면 기록도 안 비워지고
	   로비 인트로도 안 뜬다. "빨리 넘기면 대사창이 안 켜진다" 가 이것이었다.

	   화면이 바뀐 순간을 잡으면 플레이어가 누를 수 있기 전에 구독이 끝난다. 화면을
	   띄우는 것과 누르는 것 사이에 적어도 한 틱은 있기 때문이다. 그래도 허브가 떠 있는
	   동안 0.5초마다 한 번 더 훑는다. 허브가 화면 전환 없이 다시 만들어지는 경우를 위한 것이다.
	*/
	if (!bDialogueActive)
	{
		const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
		const uint8 Screen = Flow ? static_cast<uint8>(Flow->GetFrontendScreen()) : 0;
		const bool bScreenChanged = Screen != LastSeenFrontendScreen;
		LastSeenFrontendScreen = Screen;

		PlayHubScanCountdown -= DeltaTime;
		if (Screen == static_cast<uint8>(EFF_UIFlowScreen::PlayHub)
			&& (bScreenChanged || PlayHubScanCountdown <= 0.f))
		{
			PlayHubScanCountdown = 0.5f;
			TryBindPlayHub();
		}
	}

	// 새 게임을 눌렀으면 레벨 선택 화면이 뜰 때까지 기다렸다가 로비 인트로를 튼다.
	if (bLobbyIntroPending && !bDialogueActive)
	{
		TryStartPendingLobbyIntro();
	}

	// 로비 이야기를 튼 뒤에 본다. 먼저 보면 이야기가 뜨는 틱에 지도 버튼이 한 번 깜빡인다.
	UpdateMapSkipButton();
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
		if (UFF_FestivalStageSubsystem* Stage =
			UGameInstance::GetSubsystem<UFF_FestivalStageSubsystem>(World->GetGameInstance()))
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

	if (UFF_PrepHudBar* Bar = BoundPrepHudBar.Get())
	{
		Bar->OnTabSelectionChanged.RemoveDynamic(
			this, &UFF_TutorialSubsystem::HandlePrepTabSelectionChanged);
	}
	BoundPrepHudBar.Reset();

	if (bBoundStage)
	{
		if (UFF_FestivalStageSubsystem* Stage =
			UGameInstance::GetSubsystem<UFF_FestivalStageSubsystem>(World->GetGameInstance()))
		{
			Stage->OnStageChanged.RemoveDynamic(this, &UFF_TutorialSubsystem::HandleStageChanged);
		}
		bBoundStage = false;
	}
}

void UFF_TutorialSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	// 설치만 본다. 철거나 개폐는 튜토리얼에서 기다릴 일이 없다.
	if (Event.Type != EFFZSCommandType::Install)
	{
		return;
	}

	/* 줄에 시설을 적어 두었으면 그 시설만 센다(2026-10-07).

	   메인공연장 · 햄버거 가게처럼 시설을 하나씩 짚어 가며 설치시키는데, 다른 시설을 놓아도 넘어가면
	   안내한 것과 다른 것을 놓은 채 다음 단계로 간다.
	*/
	const FFF_DialogueLine& Line = GetCurrentLine();
	const FName PlacedId = ResolveFacilityDefinitionId(Event.FacilityInstanceId);
	if (bAwaitingSkipConfirm)
	{
		// 확인 창 중에는 NotifyAction 이 원래 줄을 보고 기억해 둔다.
		NotifyAction(EFF_TutorialWaitCondition::FacilityPlaced);
		return;
	}
	if (!bDialogueActive || !Line.RequiresPlayerAction() || Line.WaitCondition != EFF_TutorialWaitCondition::FacilityPlaced)
	{
		// 설치 대기 줄보다 먼저 놓았다. 그 줄이 오면 이미 놓은 것으로 친다(PrepareActionWatch).
		if (!PlacedId.IsNone())
		{
			EarlyPlacedFacilityIds.Add(PlacedId);
		}
		return;
	}
	if (!Line.WaitTarget.IsNone() && PlacedId != Line.WaitTarget)
	{
		return;
	}

	NotifyAction(EFF_TutorialWaitCondition::FacilityPlaced);
}

bool UFF_TutorialSubsystem::TryBindPrepHudBar()
{
	UFF_PrepHudBar* Current = BoundPrepHudBar.Get();

	// 화면에 올라가 있는 것을 구독 중이면 그대로 둔다.
	// 슬레이트 위젯이 있으면 화면에 붙어 있다는 뜻이다. 풀에서 쉬는 인스턴스는 없다.
	if (Current && Current->GetCachedWidget().IsValid())
	{
		return false;
	}

	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this, FoundWidgets, UFF_PrepHudBar::StaticClass(), false);

	UFF_PrepHudBar* OnScreen = nullptr;
	UFF_PrepHudBar* Fallback = nullptr;

	for (UUserWidget* Widget : FoundWidgets)
	{
		UFF_PrepHudBar* Bar = Cast<UFF_PrepHudBar>(Widget);
		if (!Bar)
		{
			continue;
		}

		if (Bar->GetCachedWidget().IsValid())
		{
			OnScreen = Bar;
			break;
		}

		if (!Fallback)
		{
			Fallback = Bar;
		}
	}

	// 화면에 올라간 것이 없으면 지금 것을 유지한다. 아무것도 없을 때만 대안을 문다.
	UFF_PrepHudBar* Chosen = OnScreen ? OnScreen : (Current ? Current : Fallback);
	if (!Chosen || Chosen == Current)
	{
		return false;
	}

	if (Current)
	{
		Current->OnTabSelectionChanged.RemoveDynamic(
			this, &UFF_TutorialSubsystem::HandlePrepTabSelectionChanged);
	}

	Chosen->OnTabSelectionChanged.AddUniqueDynamic(
		this, &UFF_TutorialSubsystem::HandlePrepTabSelectionChanged);
	BoundPrepHudBar = Chosen;

	UE_LOG(LogFestaFiestaTutorial, Log, TEXT("[튜토리얼] 준비 화면 탭 바를 구독했다%s."),
		Current ? TEXT(" (새로 만들어진 것으로 갈아탔다)") : TEXT(""));
	return true;
}

void UFF_TutorialSubsystem::CheckPrepTabAlreadyOpen()
{
	if (const UFF_PrepHudBar* Bar = BoundPrepHudBar.Get())
	{
		// 신호가 온 것처럼 같은 판정을 태운다. 판정 규칙을 두 벌 만들지 않는다.
		HandlePrepTabSelectionChanged(Bar->GetSelection());
	}
}

void UFF_TutorialSubsystem::HandlePrepTabSelectionChanged(const FFF_PrepTabSelection Selection)
{
	// 닫힐 때도 온다. 여는 것만 본다.
	if (!Selection.bHasSelection)
	{
		return;
	}

	const FFF_DialogueLine& Line = GetCurrentLine();
	if (Line.WaitCondition != EFF_TutorialWaitCondition::PrepTabOpened)
	{
		return;
	}

	// 어느 탭인지 정해뒀으면 그것만 통과시킨다.
	if (!Line.ExpectedTab.IsNone())
	{
		const UEnum* EnumClass = StaticEnum<EFF_PrepHudTab>();
		const FString Opened = EnumClass
			? EnumClass->GetNameStringByValue(static_cast<int64>(Selection.Tab))
			: FString();

		if (!Opened.Equals(Line.ExpectedTab.ToString(), ESearchCase::IgnoreCase))
		{
			return;
		}
	}

	NotifyAction(EFF_TutorialWaitCondition::PrepTabOpened);
}

bool UFF_TutorialSubsystem::DismissViewOnlyPrepPanel()
{
	if (!bPrepPanelViewOnly)
	{
		return false;
	}

	// 다음 Tick 이 다시 판정하기 전까지 오버레이가 클릭을 계속 막지 않게 바로 내린다.
	bPrepPanelViewOnly = false;
	return ClosePrepPanelForDialogue();
}

bool UFF_TutorialSubsystem::ClosePrepPanelForDialogue()
{
	// 탭 대기 줄을 거치지 않고 왔을 수 있다(설치 대기 등). 화면에 있는 탭 바를 다시 확인한다.
	TryBindPrepHudBar();

	UFF_PrepHudBar* Bar = BoundPrepHudBar.Get();
	if (!Bar || !Bar->GetSelection().bHasSelection)
	{
		return false;
	}

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 대사를 넘기려 해서 열린 패널부터 닫는다. 이번 클릭으로는 줄을 넘기지 않는다."));

	// 닫힘도 탭 전환 방송으로 오지만 HandlePrepTabSelectionChanged 가 여는 것만 보므로 대사는 그대로다.
	Bar->CloseCurrentPanel();
	return true;
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

	/* 대사창 순서는 무엇을 재생하느냐로 고른다. 로비 인트로와 축제 튜토리얼이 다르다.

	   레벨이 아니라 범위로 가르는 이유: 범위는 재생 직전에 정해져 있고(SetPlaybackScope),
	   "로비 레벨인가" 를 따로 묻지 않아도 된다. 이미 떠 있는 창의 순서가 다르면
	   내렸다가 다시 올린다 — 순서는 AddToViewport 할 때만 정해진다.
	*/
	const int32 WantedBoxZOrder = (ActiveScope == EFF_TutorialScope::LobbyIntro)
		? Settings->LobbyDialogueBoxZOrder
		: Settings->DialogueBoxZOrder;

	if (DialogueBoxWidget && DialogueBoxAppliedZOrder != WantedBoxZOrder)
	{
		DialogueBoxWidget->RemoveFromParent();
		DialogueBoxWidget->AddToViewport(WantedBoxZOrder);
		DialogueBoxAppliedZOrder = WantedBoxZOrder;
	}

	if (!DialogueBoxWidget)
	{
		if (UClass* BoxClass = Settings->DialogueBoxWidgetClass.LoadSynchronous())
		{
			DialogueBoxWidget = CreateWidget<UUserWidget>(Controller, BoxClass);
			if (DialogueBoxWidget)
			{
				DialogueBoxWidget->AddToViewport(WantedBoxZOrder);
				DialogueBoxAppliedZOrder = WantedBoxZOrder;
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

	if (ExitWidget)
	{
		ExitWidget->RemoveFromParent();
		ExitWidget = nullptr;
	}

	if (MapSkipWidget)
	{
		MapSkipWidget->RemoveFromParent();
		MapSkipWidget = nullptr;
	}
}

void UFF_TutorialSubsystem::TryBindPlayHub()
{
	/* 봤든 안 봤든 늘 구독한다.

	   예전에는 로비 인트로를 봤으면 구독하지 않았다. 지금은 이 버튼이 "새 게임" 의
	   시작점이라 튜토리얼 기록을 비우는 일까지 맡는다(HandleNewGameRequested).
	   구독을 안 하면 새 게임을 눌러도 기록이 안 비워져 튜토리얼이 안 나온다.
	*/
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this, FoundWidgets, UFF_PlayHubScreen::StaticClass(), false);

	for (UUserWidget* Widget : FoundWidgets)
	{
		UFF_PlayHubScreen* PlayHub = Cast<UFF_PlayHubScreen>(Widget);
		if (!PlayHub)
		{
			continue;
		}

		// 찾은 허브를 전부 문다. 화면 스택이 옛 허브를 들고 있으면 둘 다 살아 있을 수 있는데,
		// 어느 쪽이 지금 보이는지 여기서는 모른다.
		// 이미 문 허브는 건너뛴다. 0.5초마다 훑을 때마다 로그가 찍히지 않게 한다.
		if (PlayHub->OnNewFestivalRequested.IsAlreadyBound(this, &UFF_TutorialSubsystem::HandleNewGameRequested))
		{
			continue;
		}

		PlayHub->OnNewFestivalRequested.AddUniqueDynamic(
			this, &UFF_TutorialSubsystem::HandleNewGameRequested);
		BoundPlayHub = PlayHub;

		UE_LOG(LogFestaFiestaTutorial, Log, TEXT("[튜토리얼] 새 게임 버튼을 구독했다. (%s)"), *PlayHub->GetName());
	}
}

void UFF_TutorialSubsystem::HandleNewGameRequested()
{
	/* 새 게임이다. 이 판의 튜토리얼 기록을 먼저 비운다.

	   새 게임은 처음부터라서 로비 인트로도 축제 튜토리얼도 다시 봐야 한다.
	   이 월드에서 건너뛴 기억(bTutorialSkipped)도 같이 푼다.
	*/
	if (UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		Save->BeginNewRun();
	}
	bTutorialSkipped = false;

	/* 여기서 바로 틀지 않는다. 레벨 선택 화면까지 기다린다.

	   흐름은 새 게임 → 나라 선택(지구본) → 레벨 선택(국가 지도)이다. 로비 인트로는
	   레벨을 고르는 자리에서 봐야 하는데, 이 버튼 시점에 틀면 지구본 위에 뜬다.
	   화면 전환은 UIFlow 가 하고 우리는 그 순서를 바꿀 수 없어서, 표시만 해두고
	   Tick 에서 화면을 보고 튼다(TryStartPendingLobbyIntro).
	*/
	bLobbyIntroPending = true;
}

void UFF_TutorialSubsystem::TryStartPendingLobbyIntro()
{
	/* UIFlow 가 지금 어느 화면인지는 공개 조회로 읽는다.

	   UIFlow 에 "화면이 바뀌었다" 신호를 새로 만들면 UI 파트 파일을 고쳐야 한다.
	   값 하나를 읽는 것으로 충분하고, 화면 전환은 드물어서 매 틱 읽어도 싸다.
	*/
	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	if (!Flow || Flow->GetFrontendScreen() != EFF_UIFlowScreen::AtlasCountry)
	{
		return;
	}

	// 한 번 시도하면 끝이다. 뒤로 갔다 다시 와도 또 틀지 않는다.
	bLobbyIntroPending = false;

	// 방금 비웠으니 보통은 안 봤다. 저장 시스템이 없는 테스트 맵을 위해 한 번 더 묻는다.
	if (const UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		if (Save->WasLobbyIntroSeen())
		{
			return;
		}
	}

	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	UDataTable* Table = Settings ? Settings->LobbyIntroTable.LoadSynchronous() : nullptr;

	if (!Table)
	{
		// 로비 대사가 없는 것은 정상이다. 그대로 레벨을 고르면 된다.
		return;
	}

	/* 로비 인트로로 재생한다. 아직 축제를 고르기 전이라 축제 아이디는 없다.

	   여기서 범위를 정해두지 않으면 끝났을 때 축제 튜토리얼로 잘못 기록되고,
	   그게 원래 "새 축제에서 튜토리얼이 안 나온다" 의 원인이었다.
	*/
	SetPlaybackScope(EFF_TutorialScope::LobbyIntro, NAME_None);

	// 레벨 선택 화면 위에 대사가 얹힌다. 대사창 순서는 LobbyDialogueBoxZOrder 를 쓴다.
	if (!PlayTable(Table))
	{
		// 재생이 안 됐으면 범위도 물린다. 남겨두면 다음에 콘솔로 튼 것이 로비 인트로로 기록된다.
		SetPlaybackScope(EFF_TutorialScope::None, NAME_None);
	}
}

void UFF_TutorialSubsystem::TryAutoStart()
{
	// 이미 다른 경로로 시작했으면 건드리지 않는다.
	if (bDialogueActive)
	{
		return;
	}

	// 건너뛴 뒤에는 다시 시작하지 않는다.
	// 레벨을 다시 들어와도 마찬가지다. 한 번 넘긴 걸 또 보여주면 성가시다.
	if (bTutorialSkipped)
	{
		return;
	}

	/* 이미 봤으면 다시 보여주지 않는다. 게임을 껐다 켜도 남아야 하므로 저장에서 읽는다.

	   두 가지를 따로 묻는다. 안내를 껐는지는 전역이고, 튜토리얼을 봤는지는
	   축제별이다. 하나로 합쳐 두면 첫 축제를 끝낸 사람이 다음 축제의 안내를
	   영영 못 보는데, 그게 고치려는 문제 자체다.

	   튜토리얼 레벨만은 예외다. 새로 들어왔으면(이어하기가 아니면) 봤든 건너뛰었든 다시 튼다.
	   예전에는 지도에서 다시 골라 연습하는 길이었다(2026-09-29 피드백). 2026-10-07 부터는 끝까지
	   봤거나 건너뛴 판에서 지도가 튜토리얼 레벨을 잠가(SyncTutorialLevelLock) 흐름으로는 다시 못 들어오고,
	   에디터에서 맵을 바로 열어 고칠 때 처음부터 다시 보는 길로 남는다.
	*/
	const FName FestivalId = ResolveCurrentFestivalId();
	const bool bReplayTutorialLevel = IsTutorialFestival(FestivalId) && !bWorldStartedFromContinue;

	if (const UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		/* 이어하기가 아직 안 끝났으면 조금 뒤에 다시 본다.

		   튜토리얼 기록은 이어하기가 세이브에서 가져온다. GameMode 가 그걸 한 틱 미뤄서
		   하므로, 시작 지연이 짧으면 이쪽이 먼저 돌아 "안 본 것" 으로 판정하고 튜토리얼을
		   틀어버린다. 세이브에는 봤다고 적혀 있는데 또 나오는 꼴이 된다.
		*/
		if (Save->HasPendingContinue())
		{
			AutoStartCountdown = 0.1f;
			return;
		}

		/* 봤거나 꺼뒀으면 틀지 않는다. 레벨에서 내보내지도 않는다.

		   여기까지 오는 튜토리얼 레벨은 이어하기로 들어온 경우뿐이다. 하던 판을 계속하려고
		   고른 것이라 대사 없이 그대로 두고, 레벨 선택으로 돌려보내지 않는다.
		   돌려보내면 튜토리얼 레벨의 저장은 이어할 수 없는 기록이 된다.
		*/
		if (bReplayTutorialLevel)
		{
			UE_LOG(LogFestaFiestaTutorial, Log,
				TEXT("[튜토리얼] 튜토리얼 레벨에 새로 들어왔다. 기록과 상관없이 처음부터 튼다."));
		}
		else if (Save->IsTutorialOptedOut())
		{
			UE_LOG(LogFestaFiestaTutorial, Log,
				TEXT("[튜토리얼] 안내를 꺼둔 상태라 시작하지 않는다."));
			return;
		}
		else if (Save->WasFestivalTutorialSeen(FestivalId))
		{
			UE_LOG(LogFestaFiestaTutorial, Log,
				TEXT("[튜토리얼] %s 튜토리얼은 이미 봤으므로 시작하지 않는다."),
				*FestivalId.ToString());
			return;
		}
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
			SetPlaybackScope(EFF_TutorialScope::Festival, FestivalId);
			if (!PlayTable(Table))
			{
				SetPlaybackScope(EFF_TutorialScope::None, NAME_None);
			}
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

	SetPlaybackScope(EFF_TutorialScope::Festival, FestivalId);
	if (!PlayScript(Script))
	{
		SetPlaybackScope(EFF_TutorialScope::None, NAME_None);
	}
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
	CameraBaselineYaw = State.Yaw;
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

	/* 참조가 아니라 복사로 든다.

	   아래 방송을 받은 쪽이 대사를 멈추거나 다른 대사를 틀 수 있다. 그러면
	   ActiveLines 가 비거나 새로 채워져서, 참조로 들고 있던 줄이 사라진 메모리를 가리킨다.
	*/
	const FFF_DialogueLine Line = ActiveLines[CurrentLineIndex];

	// 강조 방식을 먼저 정한다. 같은 대상을 이어서 가리켜도 줄마다 덮개 여부가 다를 수 있다.
	bHighlightOutlineOnly = Line.bHighlightOutlineOnly;
	ApplyHighlight(Line.HighlightAnchor);

	const bool bWaitsForTab = Line.RequiresPlayerAction()
		&& Line.WaitCondition == EFF_TutorialWaitCondition::PrepTabOpened;

	// 조작 대기 줄에 들어갈 때마다 기준점을 다시 잡는다.
	// 안 그러면 이전 줄에서 움직인 거리가 누적돼 즉시 통과한다.
	if (Line.RequiresPlayerAction())
	{
		ResetCameraBaseline();
		PrepareActionWatch(Line);

		// 탭을 기다리는 줄이면 지금 구독한다. 늦게 뜨는 화면은 Tick 이 다시 잡는다.
		if (bWaitsForTab)
		{
			TryBindPrepHudBar();
			PrepBarScanCountdown = 0.5f;
		}
	}

	OnLineChanged.Broadcast(Line);

	/* 탭이 이미 열려 있는지는 방송 뒤에 본다.

	   통과하면 여기서 다음 줄로 넘어가는데, 방송 전에 넘기면 화면이 다음 줄을 받은
	   뒤에 이 줄의 방송이 도착해 옛 대사가 화면에 남는다.
	*/
	if (bWaitsForTab && bDialogueActive && CurrentLineIndex == LineIndex)
	{
		CheckPrepTabAlreadyOpen();
	}
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

	/* 준비 화면 패널이 열려 있으면 이번 클릭은 패널을 닫는 데만 쓴다.

	   튜토리얼 레벨의 대사창은 준비 HUD 아래에 깔린다(DialogueBoxZOrder 가 음수).
	   그래서 시킨 탭을 연 채로 다음 대사가 오면 패널이 대사를 가린다. 조작을 마친 순간
	   바로 닫으면 열린 탭을 볼 틈이 없으므로(피드백), 탭은 그대로 보여주고 플레이어가
	   직접 닫거나 대사를 넘기려 할 때 닫는다. 닫는 클릭과 넘기는 클릭을 나눠야
	   가려져 있던 대사를 읽지 못한 채 지나가지 않는다.
	*/
	if (ClosePrepPanelForDialogue())
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

	// 건너뛸지 물어보는 중이면 그 답부터 처리한다.
	if (bAwaitingSkipConfirm)
	{
		bAwaitingSkipConfirm = false;

		if (ChoiceIndex == 0)
		{
			SkipTutorial();
			return;
		}

		// 취소했다. 읽던 줄로 되돌아간다.
		const int32 Resume = ResumeLineIndex;
		ResumeLineIndex = INDEX_NONE;

		const bool bReleaseAction = bActionArrivedDuringSkipConfirm;
		bActionArrivedDuringSkipConfirm = false;

		GoToLine(Resume);

		// 확인 중에 기다리던 조작이 이미 들어왔다. 헤더의 bActionArrivedDuringSkipConfirm 참고.
		if (bReleaseAction && bDialogueActive && CurrentLineIndex == Resume)
		{
			NotifyAction(GetCurrentLine().WaitCondition);
		}
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
	// 확인 중에는 넘기지 않는다. 줄은 옮겨가는데 화면은 확인 줄에 머물러 둘이 어긋난다.
	if (!bDialogueActive || bAwaitingSkipConfirm)
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

	// 확인 창이 떠 있으면 원래 줄을 보고 기억만 해둔다. 취소할 때 통과시킨다.
	if (bAwaitingSkipConfirm)
	{
		if (ActiveLines.IsValidIndex(ResumeLineIndex))
		{
			const FFF_DialogueLine& Resume = ActiveLines[ResumeLineIndex];
			if (Resume.RequiresPlayerAction() && Resume.WaitCondition == Condition)
			{
				bActionArrivedDuringSkipConfirm = true;
			}
		}
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

	// 이어지는 씬이 있으면 대사창을 내리지 않고 그대로 넘어간다.
	// 내렸다 다시 올리면 화면이 한 번 깜빡인다.
	//
	// 못 틀었으면(빈 애셋) 여기서 끝낸 것으로 친다. 그냥 돌아가면 대사창이 빈 줄을
	// 든 채로 남아 아무것도 못 누르는 상태가 된다.
	if (Next && PlayScript(Next))
	{
		return;
	}

	/* 끝까지 봤다. 건너뛴 경우는 여기로 오지 않는다.

	   순서가 중요하다.
	   1. 기록 — StopScript 가 범위를 지우므로 그보다 먼저.
	   2. 정리 — 대사 상태를 비운다.
	   3. 방송 — 맨 마지막. 받는 쪽이 다음 대사를 틀 수 있는데, 방송을 먼저 하면
	      그 대사를 바로 뒤의 StopScript 가 지운다.
	*/
	/* 튜토리얼 레벨이면 대사가 끝나는 대로 레벨을 끝내고 레벨 선택으로 돌아간다.

	   튜토리얼 대사는 개장 직후에 마무리까지 이어진다. 그 뒤에 레벨에 남겨 두면 할 일 없는
	   축제를 끝까지 치러야 해서, 대사를 다 본 것을 실습의 끝으로 본다(피드백).
	   범위는 StopScript 가 지우므로 그 전에 판단해 둔다.
	*/
	const bool bTutorialLevelFinished = ActiveScope == EFF_TutorialScope::Festival
		&& (IsTutorialFestival(ActiveScopeFestivalId) || IsTutorialLevel());

	RecordScopeCompleted();
	StopScript();
	OnTutorialCompleted.Broadcast();

	if (!bTutorialLevelFinished)
	{
		return;
	}

	/* 건너뛰기 버튼은 대사창과 함께 사라진다. 그 자리에 "나가기" 를 남긴다.

	   흐름을 타고 들어왔으면 바로 아래에서 레벨을 떠나 이 버튼은 잠깐만 보인다. 흐름 밖에서
	   맵을 바로 열었으면 자동으로 떠나지 않으므로(LeaveIfTutorialLevel), 이 버튼이 유일한 출구다.
	*/
	ShowExitButton();

	// 방송 뒤에 떠난다. 레벨을 떠나면 이 월드의 구독자들이 곧 사라지므로 알릴 것은 먼저 알린다.
	LeaveIfTutorialLevel(ResolveCurrentFestivalId());
}

void UFF_TutorialSubsystem::ShowExitButton()
{
	if (ExitWidget)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return;
	}

	ExitWidget = CreateWidget<UFF_TutorialExitWidget>(Controller, UFF_TutorialExitWidget::StaticClass());
	if (ExitWidget)
	{
		// UI 루트(0) 아래에 둔다. 일시정지 화면이 위를 덮고, 오른쪽 위는 HUD 가 비워 둔 자리라 눌린다.
		ExitWidget->AddToViewport(-40);
	}
}

bool UFF_TutorialSubsystem::IsMapSkipAvailable() const
{
	// 대사 중에는 대사창의 건너뛰기가 있다. 로비 이야기가 곧 뜰 참이면 그 대사를 기다린다.
	if (bDialogueActive || bLobbyIntroPending || bTutorialSkipped)
	{
		return false;
	}

	// 축제를 고르는 국가 지도에서만. 다른 화면(지구본 등)의 오른쪽 위는 무엇이 있는지 이 시스템이 모른다.
	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	if (!Flow || Flow->GetMode() != EFF_UIFlowMode::Frontend
		|| Flow->GetFrontendScreen() != EFF_UIFlowScreen::AtlasCountry)
	{
		return false;
	}

	return GetTutorialFestivalGateState().bOverridden;
}

void UFF_TutorialSubsystem::UpdateMapSkipButton()
{
	if (!IsMapSkipAvailable())
	{
		if (MapSkipWidget)
		{
			MapSkipWidget->RemoveFromParent();
			MapSkipWidget = nullptr;
		}
		return;
	}

	if (MapSkipWidget)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return;
	}

	MapSkipWidget = CreateWidget<UFF_TutorialSkipWidget>(Controller, UFF_TutorialSkipWidget::StaticClass());
	if (MapSkipWidget)
	{
		const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
		MapSkipWidget->AddToViewport(Settings ? Settings->MapSkipButtonZOrder : 110);
	}
}

void UFF_TutorialSubsystem::RefreshAtlasLocks()
{
	// 잠금 값을 지금 맞춘다. Tick 을 기다리면 아래에서 핀을 다시 그릴 때 옛 값을 읽는다.
	SyncTutorialLevelLock();
	SyncFestivalsLockedUntilTutorial();

	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	if (!Flow || Flow->GetMode() != EFF_UIFlowMode::Frontend || Flow->GetActiveCountryId().IsNone())
	{
		return;
	}

	/* 국가 지도 파일(UI 파트)은 고치지 않고 공개 함수 SetCountry 로 다시 그리게 한다.
	   같은 국가면 아무것도 하지 않는 함수라 한 번 비웠다가 되돌린다. 화면에 올라오지 않은 지도는 올라올 때 다시 그린다.
	*/
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundWidgets, UFF_AtlasCountryScreen::StaticClass(), false);
	for (UUserWidget* Widget : FoundWidgets)
	{
		UFF_AtlasCountryScreen* Screen = Cast<UFF_AtlasCountryScreen>(Widget);
		if (Screen && Screen->IsActivated())
		{
			Screen->SetCountry(NAME_None);
			Screen->SetCountry(Flow->GetActiveCountryId());
		}
	}
}

void UFF_TutorialSubsystem::ExitTutorialLevel()
{
	UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	if (!Flow)
	{
		return;
	}

	UE_LOG(LogFestaFiestaTutorial, Log, TEXT("[튜토리얼] 나가기를 눌렀다. 레벨 선택으로 돌아간다."));
	ReturnToAtlasFromTutorialLevel(*Flow);
}

void UFF_TutorialSubsystem::ReturnToAtlasFromTutorialLevel(UFF_UIFlowSubsystem& Flow)
{
	// 흐름 밖(에디터에서 맵을 바로 연 경우)에는 판이 없어서 남길 세이브도 없다.
	if (Flow.GetMode() == EFF_UIFlowMode::Festival)
	{
		if (UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
		{
			Save->WriteLobbyAutoSaveAfterTutorial();
		}
	}
	Flow.ReturnToAtlas();
}

void UFF_TutorialSubsystem::SyncTutorialLevelLock()
{
	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	const UFF_UIFlowSettings* FlowSettings = UFF_UIFlowSettings::Get();
	const UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this);
	if (!Settings || Settings->TutorialFestivalId.IsNone() || !FlowSettings || FlowSettings->AtlasCatalog.IsNull() || !Save)
	{
		return;
	}

	UFF_FestivalAtlasCatalog* Catalog = FlowSettings->AtlasCatalog.LoadSynchronous();
	FFF_FestivalAtlasEntry* Entry = Catalog ? FindTutorialAtlasEntry(*Catalog, Settings->TutorialFestivalId) : nullptr;
	if (!Entry)
	{
		static bool bWarnedMissingEntry = false;
		if (!bWarnedMissingEntry)
		{
			bWarnedMissingEntry = true;
			UE_LOG(LogFestaFiestaTutorial, Warning,
				TEXT("[튜토리얼] 아틀라스 카탈로그에 튜토리얼 레벨(%s)이 없어 지도에서 잠그지 못한다. ")
				TEXT("튜토리얼 설정의 Tutorial Festival Id 가 카탈로그 칸의 FestivalId 나 레벨(맵) 이름과 같은지 볼 것."),
				*Settings->TutorialFestivalId.ToString());
		}
		return;
	}

	// 끝까지 본 기록은 들어온 길에 따라 카탈로그 이름이나 맵 이름으로 남는다(FindTutorialAtlasEntry 주석).
	const bool bSeen = Save->WasFestivalTutorialSeen(Entry->FestivalId)
		|| Save->WasFestivalTutorialSeen(Settings->TutorialFestivalId);
	const bool bOptedOut = Save->IsTutorialOptedOut();
	const bool bWantLocked = bSeen || bOptedOut;

	FTutorialLevelLockState& State = GetTutorialLevelLockState();
	if (State.bOverridden && State.Catalog.Get() != Catalog)
	{
		// 덮어쓴 카탈로그가 내려갔다(편집기에서 애셋을 다시 불러온 경우). 새로 덮어쓴다.
		State = FTutorialLevelLockState();
	}
	if (bWantLocked == State.bOverridden)
	{
		return;
	}

	if (!bWantLocked)
	{
		RestoreTutorialLevelLock();
		UE_LOG(LogFestaFiestaTutorial, Log, TEXT("[튜토리얼] 기록이 비어 튜토리얼 레벨을 다시 열었다(새 게임)."));
		return;
	}

	State.Catalog = Catalog;
	State.FestivalId = Entry->FestivalId;
	State.bOriginalUnlocked = Entry->bUnlocked;
	State.OriginalReason = Entry->LockReason;
	State.bOverridden = true;

	Entry->bUnlocked = false;
	Entry->LockReason = Settings->TutorialLevelLockedReason;

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 튜토리얼 레벨을 지도에서 잠갔다(끝까지 봄=%s, 건너뜀=%s). 새 게임에서 다시 열린다."),
		bSeen ? TEXT("예") : TEXT("아니오"), bOptedOut ? TEXT("예") : TEXT("아니오"));
}

void UFF_TutorialSubsystem::RestoreTutorialLevelLock()
{
	FTutorialLevelLockState& State = GetTutorialLevelLockState();
	if (!State.bOverridden)
	{
		return;
	}

	if (UFF_FestivalAtlasCatalog* Catalog = State.Catalog.Get())
	{
		if (FFF_FestivalAtlasEntry* Entry = FindAtlasEntry(*Catalog, State.FestivalId))
		{
			Entry->bUnlocked = State.bOriginalUnlocked;
			Entry->LockReason = State.OriginalReason;
		}
	}
	State = FTutorialLevelLockState();
}

bool UFF_TutorialSubsystem::HasPassedTutorial(const UFF_SaveSubsystem& Save) const
{
	if (Save.IsTutorialOptedOut() || Save.HasClearedAnyFestival())
	{
		return true;
	}
	// 끝까지 본 기록은 들어온 길에 따라 카탈로그 이름이나 맵 이름으로 남는다(FindTutorialAtlasEntry 주석).
	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	if (!Settings)
	{
		return false;
	}
	if (Save.WasFestivalTutorialSeen(Settings->TutorialFestivalId))
	{
		return true;
	}
	const UFF_UIFlowSettings* FlowSettings = UFF_UIFlowSettings::Get();
	UFF_FestivalAtlasCatalog* Catalog = FlowSettings ? FlowSettings->AtlasCatalog.LoadSynchronous() : nullptr;
	const FFF_FestivalAtlasEntry* Entry = Catalog ? FindTutorialAtlasEntry(*Catalog, Settings->TutorialFestivalId) : nullptr;
	return Entry && Save.WasFestivalTutorialSeen(Entry->FestivalId);
}

void UFF_TutorialSubsystem::SyncFestivalsLockedUntilTutorial()
{
	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	const UFF_UIFlowSettings* FlowSettings = UFF_UIFlowSettings::Get();
	UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this);
	if (!Settings || Settings->TutorialFestivalId.IsNone() || !FlowSettings || FlowSettings->AtlasCatalog.IsNull() || !Save)
	{
		return;
	}

	bool bPassed = HasPassedTutorial(*Save);

	/* 기록 없이 튜토리얼이 아닌 축제에 들어와 있다(이 기능 전의 세이브로 이어한 판). '건너뜀'으로 남긴다.
	   이어하기가 기록을 되돌린 뒤에 본다. 먼저 남기면 세이브의 기록이 그 위를 덮는다.
	*/
	const UWorld* World = GetWorld();
	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	if (!bPassed && Save->IsRunActive() && !Save->HasPendingContinue() && Flow && Flow->GetMode() == EFF_UIFlowMode::Festival
		&& World && Cast<AFF_FestivalGameMode>(World->GetAuthGameMode()) && !IsTutorialLevel())
	{
		UE_LOG(LogFestaFiestaTutorial, Log,
			TEXT("[튜토리얼] 튜토리얼을 지나지 않은 판에서 다른 축제에 들어왔다(예전 세이브). 튜토리얼을 건너뛴 것으로 남긴다."));
		Save->MarkTutorialOptedOut();
		bPassed = true;
	}

	UFF_FestivalAtlasCatalog* Catalog = FlowSettings->AtlasCatalog.LoadSynchronous();
	const FFF_FestivalAtlasEntry* TutorialEntry = Catalog ? FindTutorialAtlasEntry(*Catalog, Settings->TutorialFestivalId) : nullptr;

	// 판을 시작하지 않았으면(레벨을 바로 연 개발 중) 잠그지 않는다. 튜토리얼 칸이 없으면 다른 축제도 잠그지 않는다(전부 막힌다).
	const bool bWantLocked = !bPassed && Save->IsRunActive() && TutorialEntry != nullptr;

	FTutorialFestivalGateState& State = GetTutorialFestivalGateState();
	if (State.bOverridden && State.Catalog.Get() != Catalog)
	{
		// 덮어쓴 카탈로그가 내려갔다(편집기에서 애셋을 다시 불러온 경우). 새로 덮어쓴다.
		State = FTutorialFestivalGateState();
	}
	if (bWantLocked == State.bOverridden)
	{
		return;
	}
	if (!bWantLocked)
	{
		RestoreFestivalsLockedUntilTutorial();
		UE_LOG(LogFestaFiestaTutorial, Log, TEXT("[튜토리얼] 튜토리얼을 지났다. 다른 축제를 지도에서 다시 열었다."));
		return;
	}

	const FName TutorialId = TutorialEntry->FestivalId;
	State.Catalog = Catalog;
	State.bOverridden = true;
	State.Entries.Reset();
	for (FFF_FestivalAtlasEntry& Entry : Catalog->Entries)
	{
		// 튜토리얼 칸 · 원래 잠긴 칸 · 레벨이 없는 칸(아직 못 고르는 축제)은 그대로 둔다.
		if (Entry.FestivalId == TutorialId || !Entry.bUnlocked || Entry.Level.IsNull())
		{
			continue;
		}
		State.Entries.Add({ Entry.FestivalId, Entry.bUnlocked, Entry.LockReason });
		Entry.bUnlocked = false;
		Entry.LockReason = Settings->FestivalsLockedUntilTutorialReason;
	}

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 튜토리얼을 마치거나 건너뛰기 전이라 다른 축제 %d곳을 지도에서 잠갔다."), State.Entries.Num());
}

void UFF_TutorialSubsystem::RestoreFestivalsLockedUntilTutorial()
{
	FTutorialFestivalGateState& State = GetTutorialFestivalGateState();
	if (!State.bOverridden)
	{
		return;
	}

	if (UFF_FestivalAtlasCatalog* Catalog = State.Catalog.Get())
	{
		for (const FTutorialFestivalGateState::FSavedEntry& Saved : State.Entries)
		{
			if (FFF_FestivalAtlasEntry* Entry = FindAtlasEntry(*Catalog, Saved.FestivalId))
			{
				Entry->bUnlocked = Saved.bOriginalUnlocked;
				Entry->LockReason = Saved.OriginalReason;
			}
		}
	}
	State = FTutorialFestivalGateState();
}

bool UFF_TutorialSubsystem::IsTutorialLevel() const
{
	if (IsTutorialFestival(ResolveCurrentFestivalId()))
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const AFF_FestivalGameMode* GameMode =
		World ? Cast<AFF_FestivalGameMode>(World->GetAuthGameMode()) : nullptr;
	return GameMode && !GameMode->TutorialStartTable.IsNull();
}

void UFF_TutorialSubsystem::RequestSkipConfirm()
{
	if (!CanSkipTutorial())
	{
		return;
	}

	// 비서 브리핑은 되돌릴 것이 없다. 묻지 않고 그 브리핑만 닫는다.
	if (IsBriefingActive())
	{
		StopScript();
		return;
	}

	// 로비 이야기는 튜토리얼이 아니다. 묻지 않고 실습장으로 가는 줄로 넘긴다.
	if (TrySkipLobbyStory())
	{
		return;
	}

	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	if (!Settings)
	{
		// 설정이 없으면 물어볼 문구도 없다. 그냥 건너뛴다.
		SkipTutorial();
		return;
	}

	// 돌아올 자리를 기억해둔다. 취소하면 읽던 줄로 되돌아가야 한다.
	ResumeLineIndex = CurrentLineIndex;
	bActionArrivedDuringSkipConfirm = false;

	// 확인 줄을 만든다. 선택지 두 개짜리 대사와 구조가 같다.
	SkipConfirmLine = FFF_DialogueLine();
	SkipConfirmLine.Kind = EFF_DialogueLineKind::Choice;
	SkipConfirmLine.Text = Settings->SkipConfirmText;
	SkipConfirmLine.bSkippable = false;

	FFF_DialogueChoice Yes;
	Yes.Text = Settings->SkipConfirmYesText;
	SkipConfirmLine.Choices.Add(Yes);

	FFF_DialogueChoice No;
	No.Text = Settings->SkipConfirmNoText;
	SkipConfirmLine.Choices.Add(No);

	bAwaitingSkipConfirm = true;

	// 물어보는 동안에는 강조를 끈다. 화면에서 다른 곳을 가리키고 있으면 헷갈린다.
	ApplyHighlight(NAME_None);

	OnLineChanged.Broadcast(SkipConfirmLine);
}

bool UFF_TutorialSubsystem::TrySkipLobbyStory()
{
	if (!bDialogueActive || bAwaitingSkipConfirm || ActiveScope != EFF_TutorialScope::LobbyIntro)
	{
		return false;
	}

	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	const FName Label = Settings ? Settings->LobbyStorySkipLabel : NAME_None;
	if (Label.IsNone())
	{
		return false;
	}

	const int32 Target = FindLineIndexByLabel(Label);
	if (Target == INDEX_NONE)
	{
		// 라벨을 잘못 적었다. 넘길 곳을 모르니 예전처럼 진짜 건너뛰기로 둔다.
		UE_LOG(LogFestaFiestaTutorial, Warning,
			TEXT("[튜토리얼] 로비 이야기 건너뛰기 라벨 %s 를 찾지 못했다. 진짜 건너뛰기로 처리한다."),
			*Label.ToString());
		return false;
	}

	// 이미 실습장으로 가는 부분이다. 실습장 안과 같이 진짜 건너뛰기다(2026-10-13 사용자 결정).
	if (CurrentLineIndex >= Target)
	{
		return false;
	}

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 로비 이야기 건너뜀. %d → %d(%s), 건너뜀 기록 없음."),
		CurrentLineIndex, Target, *Label.ToString());

	GoToLine(Target);
	return true;
}

bool UFF_TutorialSubsystem::CanSkipTutorial() const
{
	// 진행 중이 아니면 건너뛸 것도 없다. 이미 물어보는 중이어도 마찬가지다.
	return bDialogueActive && !bTutorialSkipped && !bAwaitingSkipConfirm;
}

void UFF_TutorialSubsystem::SkipTutorial()
{
	if (bTutorialSkipped)
	{
		return;
	}

	/* 대사가 없으면 건너뛸 것도 없다. 국가 지도의 건너뛰기 버튼만 예외다(IsMapSkipAvailable).

	   이 검사가 없으면 대사가 끝난 뒤에도 떠 있던 확인 창에서 `예` 를 누르거나
	   콘솔을 잘못 치면, 끝까지 본 사람의 안내가 전역으로 꺼진다. 지도 버튼은 실습장을 아직 안 지난
	   판에서만 뜨므로 그럴 일이 없다.
	*/
	const bool bFromMap = !bDialogueActive && IsMapSkipAvailable();
	if (!bDialogueActive && !bFromMap)
	{
		UE_LOG(LogFestaFiestaTutorial, Log,
			TEXT("[튜토리얼] 진행 중인 대사가 없어 건너뛰기를 무시한다."));
		return;
	}

	// 비서 브리핑을 닫는 건 튜토리얼 건너뛰기가 아니다. 안내를 끄거나 레벨을 떠나지 않는다.
	if (IsBriefingActive())
	{
		StopScript();
		return;
	}

	bTutorialSkipped = true;

	if (bFromMap)
	{
		UE_LOG(LogFestaFiestaTutorial, Log, TEXT("[튜토리얼] 국가 지도의 건너뛰기 버튼으로 건너뛰었다."));
	}
	else
	{
		UE_LOG(LogFestaFiestaTutorial, Log,
			TEXT("[튜토리얼] 건너뛰었다. 줄 %d/%d 에서 중단."),
			CurrentLineIndex, ActiveLines.Num());
	}

	StopScript();

	// 위젯도 내린다. 건너뛴 뒤에도 대사창이 남아 있으면 화면을 가린다.
	RemoveWidgets();

	/* 안내를 통째로 끈다. 축제별이 아니라 이 판 전체다. 새 게임을 누르면 풀린다.

	   건너뛰기는 "이 축제만 넘긴다" 가 아니라 "안내 없이 운영만 하겠다" 는 선언이다.
	   축제별로 기록하면 축제를 바꿀 때마다 다시 눌러야 한다.
	*/
	if (UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		Save->MarkTutorialOptedOut();
	}

	// 지도 위에서 건너뛰었으면(지도 버튼 · 로비 이야기의 실습장 가는 줄) 핀을 바로 다시 그린다.
	RefreshAtlasLocks();

	OnTutorialSkipped.Broadcast();

	// 튜토리얼 레벨에서 건너뛰었으면 레벨도 끝낸다. 방송 뒤에 한다 — 레벨을 떠나면
	// 이 월드의 구독자들이 곧 사라지므로, 알릴 것은 그 전에 알린다.
	LeaveIfTutorialLevel(ResolveCurrentFestivalId());
}

bool UFF_TutorialSubsystem::IsTutorialFestival(const FName FestivalId)
{
	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	return Settings && !Settings->TutorialFestivalId.IsNone() && FestivalId == Settings->TutorialFestivalId;
}

bool UFF_TutorialSubsystem::LeaveIfTutorialLevel(const FName FestivalId)
{
	/* 튜토리얼 레벨이면 레벨을 끝내고 레벨 선택으로 돌아간다.

	   튜토리얼 레벨은 그 자체가 안내라서, 안내를 건너뛴 사람에게 레벨만 남겨두면
	   할 일 없는 축제를 억지로 치르게 된다.

	   흐름을 타고 들어온 경우(UIFlow 가 축제 중)에만 돌려보낸다. 에디터에서 레벨을
	   직접 열어 테스트할 때 쫓겨나면 튜토리얼 레벨을 고칠 수가 없다.
	*/
	// 식별자가 달라도 튜토리얼 레벨이면 떠난다. IsTutorialLevel 참고.
	if (!IsTutorialFestival(FestivalId) && !IsTutorialLevel())
	{
		return false;
	}

	UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	if (!Flow || Flow->GetMode() != EFF_UIFlowMode::Festival)
	{
		UE_LOG(LogFestaFiestaTutorial, Log,
			TEXT("[튜토리얼] 튜토리얼 레벨이지만 흐름 밖에서 열려 레벨 선택으로 돌아가지 않는다."));
		return false;
	}

	UE_LOG(LogFestaFiestaTutorial, Log,
		TEXT("[튜토리얼] 튜토리얼을 건너뛰었거나 이미 봤다. 튜토리얼 레벨을 끝내고 레벨 선택으로 돌아간다."));

	ReturnToAtlasFromTutorialLevel(*Flow);
	return true;
}

void UFF_TutorialSubsystem::StopScript()
{
	if (!bDialogueActive)
	{
		return;
	}

	ActiveLines.Reset();
	ActiveScriptId = NAME_None;

	// 범위도 같이 지운다. 남겨두면 다음에 콘솔로 재생한 것이 직전 축제의
	// 튜토리얼을 본 것으로 기록된다.
	ActiveScope = EFF_TutorialScope::None;
	ActiveScopeFestivalId = NAME_None;

	PendingNextScript = nullptr;
	bAwaitingSkipConfirm = false;
	bActionArrivedDuringSkipConfirm = false;
	ResumeLineIndex = INDEX_NONE;
	CurrentLineIndex = INDEX_NONE;
	bDialogueActive = false;

	ApplyHighlight(NAME_None);
	OnDialogueActiveChanged.Broadcast(false);

	UE_LOG(LogFestaFiestaTutorial, Log, TEXT("[튜토리얼] 대사 종료."));
}
