// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "FF_FFZSTypes.h"
#include "Time/FF_FestivalStage.h"
#include "Tutorial/FF_TutorialTypes.h"
#include "FF_TutorialSubsystem.generated.h"

class AFF_OverviewCameraPawn;
class UDataTable;
class UFF_DialogueScriptData;
class UUserWidget;
class UWidget;
struct IConsoleCommand;

// 지금 줄이 바뀌었다. UI 가 이걸 받아 대사창을 갱신한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnDialogueLineChanged, const FFF_DialogueLine&, Line);

// 대화가 시작되거나 끝났다. 대사창을 띄우고 내리는 데 쓴다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnDialogueActiveChanged, bool, bActive);

// 튜토리얼을 건너뛰었다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnTutorialSkipped);

// 어디를 강조할지 바뀌었다. 비어 있으면 강조를 끈다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnHighlightAnchorChanged, FName, AnchorId);

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
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	bool WasTutorialSkipped() const { return bTutorialSkipped; }

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

private:
	// 시설이 설치·철거되면 온다. Zone 이 확정한 뒤에만 방송되므로 실제 성공만 잡힌다.
	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	void BindGameSignals();
	void UnbindGameSignals();

	// 줄 배열로 재생을 시작한다. 애셋과 표가 여기로 모인다.
	bool StartLines(const TArray<FFF_DialogueLine>& Lines, FName ScriptId);

	// 라벨로 줄을 찾는다. 행 이름이 곧 라벨이다.
	int32 FindLineIndexByLabel(FName Label) const;

	// 지금 줄 다음에 갈 자리. NextLabel 이 있으면 그리로, 없으면 바로 다음 줄로.
	int32 ResolveNextLineIndex() const;

	// 레벨이 뜨면 설정에 지정된 대사를 시작한다. 지정이 없으면 아무것도 하지 않는다.
	void TryAutoStart();

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

	// 연출 줄처럼 멈출 필요가 없는 것들을 지나친다.
	void SkipNonStoppingLines();

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

	int32 CurrentLineIndex = INDEX_NONE;
	bool bDialogueActive = false;

	FName CurrentAnchor = NAME_None;

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

	bool bBoundZoneWorld = false;
	bool bBoundStage = false;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DialogueBoxWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> OverlayWidget;

	IConsoleCommand* SkipConsoleCommand = nullptr;
	IConsoleCommand* StatConsoleCommand = nullptr;
	IConsoleCommand* HighlightConsoleCommand = nullptr;
	IConsoleCommand* SkipAllConsoleCommand = nullptr;
};
