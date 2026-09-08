// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tutorial/FF_TutorialTypes.h"
#include "FF_TutorialSubsystem.generated.h"

class UFF_DialogueScriptData;
class UWidget;
struct IConsoleCommand;

// 지금 줄이 바뀌었다. UI 가 이걸 받아 대사창을 갱신한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnDialogueLineChanged, const FFF_DialogueLine&, Line);

// 대화가 시작되거나 끝났다. 대사창을 띄우고 내리는 데 쓴다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnDialogueActiveChanged, bool, bActive);

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
class FESTAFIESTA_API UFF_TutorialSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	static UFF_TutorialSubsystem* Get(const UObject* WorldContextObject);

	// ── 재생 ────────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Tutorial")
	bool PlayScript(UFF_DialogueScriptData* Script);

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

	// ── 신호 ────────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Tutorial")
	FFF_OnDialogueLineChanged OnLineChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Tutorial")
	FFF_OnDialogueActiveChanged OnDialogueActiveChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Tutorial")
	FFF_OnHighlightAnchorChanged OnHighlightAnchorChanged;

private:
	// 인덱스로 이동하고 신호를 보낸다. 범위를 벗어나면 스크립트를 끝낸다.
	void GoToLine(int32 LineIndex);

	// 연출 줄처럼 멈출 필요가 없는 것들을 지나친다.
	void SkipNonStoppingLines();

	// 이어질 스크립트가 있으면 재생하고, 없으면 끝낸다.
	void FinishScript();

	void ApplyHighlight(FName AnchorId);

	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();

	UPROPERTY(Transient)
	TObjectPtr<UFF_DialogueScriptData> ActiveScript;

	int32 CurrentLineIndex = INDEX_NONE;
	bool bDialogueActive = false;

	FName CurrentAnchor = NAME_None;

	// 이름 → 위젯. 화면이 사라지면 자동으로 비도록 약참조로 든다.
	TMap<FName, TWeakObjectPtr<UWidget>> AnchorWidgets;

	IConsoleCommand* SkipConsoleCommand = nullptr;
	IConsoleCommand* StatConsoleCommand = nullptr;
};
