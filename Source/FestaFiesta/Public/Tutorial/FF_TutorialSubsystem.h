// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/FF_DebugConsole.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "FF_FFZSTypes.h"
#include "Time/FF_FestivalStage.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "Tutorial/FF_TutorialTypes.h"
#include "FF_TutorialSubsystem.generated.h"

class AFF_OverviewCameraPawn;
class UDataTable;
class UFF_DialogueScriptData;
class UUserWidget;
class UWidget;

// 지금 줄이 바뀌었다. UI 가 이걸 받아 대사창을 갱신한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnDialogueLineChanged, const FFF_DialogueLine&, Line);

// 대화가 시작되거나 끝났다. 대사창을 띄우고 내리는 데 쓴다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnDialogueActiveChanged, bool, bActive);

// 튜토리얼을 건너뛰었다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnTutorialSkipped);

// 튜토리얼을 끝까지 봤다.
//
// 건너뛴 것과 나눠 두는 이유는, 둘 다 "다시 안 보여준다" 는 같지만
// 건너뛴 사람에게는 나중에 다시 볼 기회를 주고 싶어질 수 있어서다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnTutorialCompleted);

// 어디를 강조할지 바뀌었다. 비어 있으면 강조를 끈다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnHighlightAnchorChanged, FName, AnchorId);

/* 지금 재생 중인 것이 어디에 속하는가.

   끝까지 봤을 때 저장의 어디에 남길지가 이걸로 갈린다. 재생 자체는 같은 길을 타지만
   "다시 안 보여준다" 의 범위가 서로 다르다.

   범위를 재생 시점에 정해 두는 이유는, 끝나는 시점에는 판단할 근거가 남아 있지 않기
   때문이다. 대사가 이어지는 씬으로 넘어가거나 축제가 끝나면 그 사이에 문맥이 바뀐다.
*/
UENUM()
enum class EFF_TutorialScope : uint8
{
	/* 어디에도 기록하지 않는다.

	   콘솔이나 UI 가 직접 재생한 경우다. 여기서 기록하면 안 본 튜토리얼까지
	   봤다고 남는다.
	*/
	None,

	// 로비 인트로. 축제를 고르기 전이라 계정 단위로 한 번만 본다.
	LobbyIntro,

	// 축제 레벨의 튜토리얼. 축제마다 배울 것이 달라 축제별로 남긴다.
	Festival,
};

// 튜토리얼 대사를 굴리고 조작을 기다린다.
//
// ─── 왜 대사와 튜토리얼을 한 시스템에 두는가 ────────────────────────────────
// 둘을 나누면 "이 대사 다음에 이 조작" 을 잇는 코드가 또 필요해진다.
// 대사 한 줄이 조작 하나를 기다릴 수 있게 해두면 그 연결이 데이터로 표현된다.
//
// ─── UI 를 모른다 ───────────────────────────────────────────────────────────
// 여기서는 위젯을 만들지도 찾지도 않는다. 지금 무슨 줄인지 알리기만 하고,
// 그리는 건 위젯이 한다. 그래야 대사창 모양이 바뀌어도 진행 로직이 안 깨진다.
//
// 강조할 위젯도 이름으로만 다룬다. 실제 위젯은 밖에서 등록해준다.
UCLASS()
class FESTAFIESTA_API UFF_TutorialSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_TutorialSubsystem, STATGROUP_Tickables);
	}

	static UFF_TutorialSubsystem* Get(const UObject* WorldContextObject);

	// ── 재생 ────────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	bool PlayScript(UFF_DialogueScriptData* Script);

	// CSV 로 만든 표를 재생한다. 행 순서대로 흐르고 행 이름이 분기 목적지가 된다.
	//
	// 데이터 애셋과 표를 둘 다 받는 이유는, 짧은 대사는 애셋이 편하고
	// 긴 대사는 표가 편해서다. 재생 뒤로는 둘이 같은 길을 탄다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	bool PlayTable(UDataTable* Table);

	// 다음 줄로. 대사창에서 클릭했을 때 부른다.
	//
	// 조작을 기다리는 줄에서는 아무 일도 하지 않는다. 그래야 클릭으로
	// 튜토리얼을 통과해버리는 일이 없다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void Advance();

	// 선택지를 골랐다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void SelectChoice(int32 ChoiceIndex);

	// 서사를 건너뛴다. 조작을 기다리는 줄에서 멈춘다.
	//
	// 전부 건너뛰면 조작을 못 배운 채로 게임이 시작된다. 대사만 넘기고
	// 안내는 남기는 게 목적이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void SkipNarration();

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void StopScript();

	// 건너뛸지 물어본다. 버튼을 누르면 이걸 부른다.
	//
	// 바로 끝내지 않는 이유는 되돌릴 수 없는 선택이기 때문이다.
	// 확인은 대사창의 선택지로 띄운다. 창을 따로 만들지 않아도 되고,
	// 대사를 읽던 흐름 그대로라 눈이 다른 데로 가지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void RequestSkipConfirm();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	bool IsAwaitingSkipConfirm() const { return bAwaitingSkipConfirm; }

	// 튜토리얼을 통째로 끝낸다.
	//
	// SkipNarration 과 다르다. 저쪽은 서사만 넘기고 조작 안내는 남기는데,
	// 이건 안내까지 전부 없앤다. 운영 자체를 즐기려는 사람을 위한 문이다.
	//
	// 되돌릴 수 없으므로 호출하기 전에 확인을 받아야 한다.
	// 확인 창을 여기서 만들지 않는 이유는, 그러면 이 시스템이 UI 를 알아야 하고
	// 창 모양을 바꿀 때마다 여기도 고쳐야 하기 때문이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void SkipTutorial();

	// 이번 판에서 튜토리얼을 건너뛰었는지.
	//
	// 한 번 건너뛰면 다시 시작하지 않는다. 레벨을 다시 들어와도 마찬가지다.
	//
	// [주의] 이건 이번 월드의 기억이다. 게임을 껐다 켠 뒤까지 유지되는 값은
	//        UFF_SaveSubsystem::IsTutorialOptedOut() 이 답한다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	bool WasTutorialSkipped() const { return bTutorialSkipped; }

	/* 지금 축제의 식별자. 튜토리얼을 봤는지 묻고 남길 때의 열쇠다.

	   UIFlow 가 답이다. 아틀라스에서 고른 값을 들고 있고 GameInstance 스코프라
	   레벨을 넘어 살아남는다.

	   진행 서브시스템의 Definition 을 쓰지 않는 이유는, 그쪽이 Initialize 에서
	   프로젝트 설정의 DefaultFestival 을 무조건 물기 때문이다. 실제로 고른 축제와
	   다를 수 있어 판정 기준으로 쓸 수 없다.

	   흐름을 타지 않고 레벨을 직접 열면 비어 있다(PIE 에서 흔하다). 그때는 레벨
	   이름으로 대신한다 — 레벨과 축제가 1:1 이라 실용적으로 같은 값이고, 적어도
	   레벨끼리는 구분된다.

	   규칙은 저장 쪽(UFF_SaveSubsystem::ResolveCurrentFestivalId)이 들고 여기는
	   그걸 부른다. 저장도 같은 이름으로 판을 담으므로 규칙이 한 곳에 있어야 한다.
	*/
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	FName ResolveCurrentFestivalId() const;

	// 지금 건너뛸 수 있는지. 버튼을 보일지 말지 정하는 데 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	bool CanSkipTutorial() const;

	// ── 조회 ────────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	bool IsDialogueActive() const { return bDialogueActive; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	const FFF_DialogueLine& GetCurrentLine() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	bool IsWaitingForAction() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	FName GetHighlightAnchor() const { return CurrentAnchor; }

	// 지금 강조를 테두리만 그릴지. 읽고 넘기는 줄의 강조는 화면을 어둡게 덮지 않는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	bool IsHighlightOutlineOnly() const { return bHighlightOutlineOnly; }

	// ── 조작 완료 통지 ──────────────────────────────────────────────────────

	// 플레이어가 무언가 했다고 알린다. 기다리던 조건과 맞으면 다음 줄로 간다.
	//
	// 조건을 만족했는지 여기서 직접 확인하지 않는 이유는, 그러려면 이 시스템이
	// 카메라와 건축과 단계를 전부 알아야 하기 때문이다. 알리는 쪽이 판단한다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void NotifyAction(EFF_TutorialWaitCondition Condition);

	// ── 강조 대상 등록 ──────────────────────────────────────────────────────

	// 이 이름으로 강조할 위젯을 알려준다. 화면이 만들어질 때 등록한다.
	//
	// 튜토리얼이 위젯을 직접 찾지 않는 이유는, 그러려면 UI 구조를 알아야 하고
	// UI 가 바뀔 때마다 여기도 고쳐야 하기 때문이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void RegisterAnchor(FName AnchorId, UWidget* Widget);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	void UnregisterAnchor(FName AnchorId);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	UWidget* FindAnchorWidget(FName AnchorId) const;

	// 화면에 떠 있는 준비 화면 탭 버튼을 찾아 등록한다.
	//
	// 각 위젯 BP 에서 일일이 등록하게 하면 UI 쪽 파일을 고쳐야 하고,
	// BP 는 바이너리라 같은 파일을 두 사람이 만지면 병합이 안 된다.
	//
	// 이름으로 찾지 않고 클래스로 찾아 공개 API 에 물어보는 이유는,
	// 위젯 이름이 바뀌어도 조용히 깨지지 않게 하려는 것이다.
	// 클래스나 열거형이 바뀌면 그때는 컴파일이 멈춘다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	int32 ScanPrepHudAnchors();

	// 이 이름의 세계 지점을 찾는다. 없으면 false.
	//
	// UI 와 세계를 같은 이름 공간에서 다루는 이유는, 대사를 쓰는 쪽이
	// 이게 화면 위인지 세계 안인지 신경 쓰지 않아도 되게 하려는 것이다.
	// 찾는 순서는 UI 가 먼저다. 이름이 겹치면 관리로 막는다.
	bool FindAnchorWorldPoint(FName AnchorId, FVector& OutLocation, float& OutRadius) const;

	// ── 신호 ────────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Tutorial")
	FFF_OnDialogueLineChanged OnLineChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Tutorial")
	FFF_OnDialogueActiveChanged OnDialogueActiveChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Tutorial")
	FFF_OnHighlightAnchorChanged OnHighlightAnchorChanged;

	// 튜토리얼을 건너뛰었다. 진행 상태를 저장하거나 안내를 띄우는 데 쓴다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Tutorial")
	FFF_OnTutorialSkipped OnTutorialSkipped;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Tutorial")
	FFF_OnTutorialCompleted OnTutorialCompleted;

private:
	// 시설이 설치·철거되면 온다. Zone 이 확정한 뒤에만 방송되므로 실제 성공만 잡힌다.
	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	// 준비 화면 탭이 열리거나 닫히면 온다.
	UFUNCTION()
	void HandlePrepTabSelectionChanged(FFF_PrepTabSelection Selection);

	/* 탭 바를 구독한다. 화면이 만들어진 뒤에야 가능하므로 여러 번 시도한다.

	   탭 바는 축제 레벨에 들어온 뒤에 만들어지고, 단계가 바뀌면 다시 만들어진다.
	   예전에는 탭 대기 줄에 들어가는 순간 한 번만 찾아서, 그때 화면이 없거나 뒤에
	   새로 만들어지면 탭을 열어도 안 넘어가는 채로 멈췄다. 그래서 대기 중에는
	   Tick 에서 주기적으로 다시 확인한다.

	   화면에 올라가 있는 것을 먼저 고른다. 풀에 남은 옛 인스턴스를 물면 신호가 안 온다.
	   새로 물었으면 true.
	*/
	bool TryBindPrepHudBar();

	/* 원하는 탭이 이미 열려 있으면 바로 통과시킨다.

	   탭 신호는 바뀔 때만 온다. 대사보다 먼저 탭을 열어둔 사람은 닫았다 다시
	   열어야 넘어가는데, 무엇을 해야 하는지 알 수 없어 멈춘 것처럼 보인다.
	*/
	void CheckPrepTabAlreadyOpen();

	/* 시킨 조작을 마쳤으면 열려 있는 준비 화면 패널을 닫는다.

	   튜토리얼 레벨의 대사창은 준비 HUD 아래에 깔린다(DialogueBoxZOrder 가 음수).
	   조작 대기 줄에서 HUD 를 눌러야 하기 때문이다. 그래서 탭을 연 채로 다음 대사로
	   넘어가면 패널이 대사창과 선택지를 덮었다. 조작이 끝났으면 패널을 내리고 대사를 보인다.

	   다음 줄도 조작 대기면 닫지 않는다. "탭 열기 → 시설 설치" 처럼 같은 패널에서
	   이어서 해야 하는 일이 있기 때문이다. 그래서 대사나 선택지 줄에 도착했을 때만 닫는다.

	   배치 탭을 닫으면 패널이 건축 세션도 끝낸다(UFF_PrepPanelBuild::EndPanelSession).
	   연속 설치 상태로 남은 배치 화면도 같이 정리된다.
	*/
	void DismissPrepPanelForDialogue();

	void BindGameSignals();
	void UnbindGameSignals();

	// 줄 배열로 재생을 시작한다. 애셋과 표가 여기로 모인다.
	bool StartLines(const TArray<FFF_DialogueLine>& Lines, FName ScriptId);

	/* 이제부터 재생할 것이 무엇에 속하는지 정한다. PlayTable/PlayScript 직전에 부른다.

	   재생 함수의 인자로 받지 않는 이유는, 대사가 이어지는 씬으로 넘어갈 때
	   FinishScript 가 PlayScript 를 다시 부르기 때문이다. 인자로 받으면 그 자리에서
	   범위를 또 정해야 하고, 빠뜨리면 조용히 None 이 된다. 범위는 재생 한 묶음에
	   한 번만 정하고 StopScript 에서 지운다.
	*/
	void SetPlaybackScope(EFF_TutorialScope Scope, FName FestivalId);

	// 끝까지 본 것을 저장에 남긴다. 범위에 따라 남기는 자리가 다르다.
	void RecordScopeCompleted();

	// 라벨로 줄을 찾는다. 행 이름이 곧 라벨이다.
	int32 FindLineIndexByLabel(FName Label) const;

	// 지금 줄 다음에 갈 자리. NextLabel 이 있으면 그리로, 없으면 바로 다음 줄로.
	int32 ResolveNextLineIndex() const;

	/* 새 게임 버튼을 구독한다. 화면이 만들어진 뒤에야 가능하다.

	   플레이 허브는 타이틀에서 넘어올 때 만들어지므로, 월드가 시작하는 시점에는
	   아직 없다. 매 틱 훑으면 낭비라 화면이 바뀌는 신호에 맞춰 시도한다.
	*/
	void TryBindPlayHub();

	UFUNCTION()
	void HandleNewGameRequested();

	// 레벨 선택 화면이 떠 있으면 로비 인트로를 튼다. 새 게임을 누른 뒤 Tick 에서 부른다.
	void TryStartPendingLobbyIntro();

	// 새 게임을 눌렀고 아직 로비 인트로를 틀지 않았다. 레벨 선택 화면이 뜨면 튼다.
	bool bLobbyIntroPending = false;

	// 레벨이 뜨면 설정에 지정된 대사를 시작한다. 지정이 없으면 아무것도 하지 않는다.
	void TryAutoStart();

	// 튜토리얼 레벨이면 레벨을 끝내고 레벨 선택으로 돌아간다. 돌아갔으면 true.
	bool LeaveIfTutorialLevel(FName FestivalId);

	// 이 축제가 튜토리얼 레벨인지. 튜토리얼 설정의 TutorialFestivalId 와 비교한다.
	static bool IsTutorialFestival(FName FestivalId);

	// 카메라 조작을 감시한다.
	//
	// 카메라 쪽에서 알려주게 하면 카메라가 튜토리얼을 알아야 한다.
	// 여기서 상태를 읽는 편이 의존이 한쪽으로만 흐른다.
	void TickCameraWatch();

	// 감시 기준점을 지금 위치로 다시 잡는다. 조작 대기 줄에 들어갈 때 부른다.
	void ResetCameraBaseline();

	AFF_OverviewCameraPawn* FindOverviewPawn() const;

	// 설정에 지정된 위젯을 화면에 띄운다. 이미 있으면 아무것도 하지 않는다.
	//
	// 대사만 자동으로 시작되고 위젯은 직접 띄워야 하면, 레벨마다 그 코드를 넣어야 한다.
	// 튜토리얼을 켜고 끄는 게 설정 한 곳에서 끝나야 실수가 없다.
	void EnsureWidgets();

	void RemoveWidgets();

	// 인덱스로 이동하고 신호를 보낸다. 범위를 벗어나면 스크립트를 끝낸다.
	void GoToLine(int32 LineIndex);

	// 이어질 스크립트가 있으면 재생하고, 없으면 끝낸다.
	void FinishScript();

	void ApplyHighlight(FName AnchorId);

	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();

	// 재생 중인 줄. 애셋에서 왔든 표에서 왔든 여기로 모인다.
	//
	// 애셋을 직접 들고 다니지 않는 이유는, 표로 재생할 때 들 것이 없어서다.
	// 줄만 복사해두면 출처와 무관하게 같은 코드로 굴릴 수 있다.
	TArray<FFF_DialogueLine> ActiveLines;

	// 이어질 씬. 표에는 없으므로 애셋으로 재생할 때만 채워진다.
	UPROPERTY(Transient)
	TObjectPtr<UFF_DialogueScriptData> PendingNextScript;

	// 지금 재생 중인 것의 이름. 로그에만 쓴다.
	FName ActiveScriptId;

	// 지금 재생 중인 것이 속한 범위. 끝났을 때 무엇을 기록할지 정한다.
	EFF_TutorialScope ActiveScope = EFF_TutorialScope::None;

	/* ActiveScope 가 Festival 일 때 어느 축제인가. 재생을 시작할 때 굳혀 둔다.

	   끝나는 시점에 다시 묻지 않는 이유는, 그 사이에 축제가 끝나 UIFlow 가 값을
	   비웠을 수 있기 때문이다. 그러면 방금 본 튜토리얼이 기록되지 않는다.
	*/
	FName ActiveScopeFestivalId;

	int32 CurrentLineIndex = INDEX_NONE;
	bool bDialogueActive = false;

	FName CurrentAnchor = NAME_None;

	// 지금 강조가 테두리만인지. 줄이 바뀔 때 그 줄의 값으로 바꾼다.
	bool bHighlightOutlineOnly = false;

	/* 이 월드가 저장에서 이어하기로 열렸는지. 월드가 시작할 때 한 번 정한다.

	   튜토리얼 레벨은 지도에서 새로 고르면 기록과 상관없이 다시 튼다. 이어하기로 들어온
	   경우는 하던 판을 계속하려는 것이라 대사를 다시 틀지 않는다.
	   시작할 때 읽는 이유는, 이어하기 지정은 레벨이 뜬 뒤 한 틱 만에 소비되어
	   자동 시작 시점에는 이미 비어 있을 수 있기 때문이다.
	*/
	bool bWorldStartedFromContinue = false;

	// 이름 → 위젯. 화면이 사라지면 자동으로 비도록 약참조로 든다.
	TMap<FName, TWeakObjectPtr<UWidget>> AnchorWidgets;

	// 자동 시작까지 남은 시간. 음수면 이미 처리했다.
	float AutoStartCountdown = -1.f;

	// 조작 대기 줄에 들어간 시점의 카메라 상태.
	// 여기서 얼마나 벗어났는지로 조작 여부를 판단한다.
	FVector CameraBaselineFocus = FVector::ZeroVector;
	float CameraBaselineArmLength = 0.f;
	bool bCameraBaselineValid = false;

	// 건너뛰었으면 다시 시작하지 않는다.
	bool bTutorialSkipped = false;

	// 건너뛸지 물어보는 중. 이때는 원래 대사 대신 확인 줄을 보여준다.
	bool bAwaitingSkipConfirm = false;

	// 확인을 취소하면 돌아갈 자리.
	int32 ResumeLineIndex = INDEX_NONE;

	// 확인용으로 만든 줄. 데이터에 없는 줄이라 여기서 들고 있는다.
	FFF_DialogueLine SkipConfirmLine;

	/* 확인 창이 떠 있는 동안 원래 줄이 기다리던 조작이 들어왔다.

	   확인 중에는 GetCurrentLine 이 확인 줄을 돌려줘서 NotifyAction 이 그 조작을
	   버린다. 단계 전환처럼 시간이 가면 저절로 일어나는 것은 그 사이에 지나가
	   버려서, 취소하고 돌아오면 다시는 안 올 신호를 기다리며 멈춘다.
	   그래서 기억해뒀다가 취소할 때 통과시킨다.
	*/
	bool bActionArrivedDuringSkipConfirm = false;

	// 지금 구독 중인 탭 바. 화면이 다시 만들어지면 바뀐다.
	TWeakObjectPtr<class UFF_PrepHudBar> BoundPrepHudBar;

	/* 조작을 마쳐서 패널을 닫아야 한다. 다음 Tick 에서 처리한다.

	   그 자리에서 닫지 않는 이유: 조작 신호는 탭 전환 방송이나 설치 확정 도중에 온다.
	   그 안에서 패널을 닫으면 탭 바의 선택 적용과 건축 세션 종료가 진행 중인 호출 위로
	   다시 들어간다. 호출이 다 풀린 다음 틱에 닫는다.
	*/
	bool bPrepPanelDismissPending = false;

	// 마지막으로 구독한 허브. 로그와 디버그 확인용이다. 구독 여부는 허브 쪽 델리게이트로 판단한다.
	TWeakObjectPtr<class UFF_PlayHubScreen> BoundPlayHub;

	// 다음 구독 시도까지 남은 시간. 매 틱 훑지 않기 위한 것이다.
	float PlayHubScanCountdown = 0.f;

	// 지난 틱의 프론트엔드 화면(EFF_UIFlowScreen 값). 플레이 허브가 막 뜬 틱을 잡는 데 쓴다.
	// 화면 타입 헤더를 여기서 끌어오지 않으려고 숫자로 든다.
	uint8 LastSeenFrontendScreen = 0;

	// 탭 대기 중 탭 바를 다시 확인할 때까지 남은 시간.
	float PrepBarScanCountdown = 0.f;

	bool bBoundZoneWorld = false;
	bool bBoundStage = false;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DialogueBoxWidget;

	// 대사창을 올릴 때 쓴 화면 순서. 로비와 축제가 달라서 바뀌면 다시 올린다.
	int32 DialogueBoxAppliedZOrder = 0;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> OverlayWidget;

	// 개발용 콘솔 명령. 이름 충돌·해제 규칙은 FFF_DebugConsoleCommands 가 맡는다.
	FFF_DebugConsoleCommands DebugCommands;
};
