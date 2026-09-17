// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tutorial/FF_DialogueScriptData.h"
#include "Tutorial/FF_TutorialTypes.h"

namespace FFF_TutorialTestHelpers
{
	FFF_DialogueLine MakeSpeech(FName Label = NAME_None)
	{
		FFF_DialogueLine Line;
		Line.Kind = EFF_DialogueLineKind::Speech;
		Line.Label = Label;
		return Line;
	}

	FFF_DialogueLine MakeWait(EFF_TutorialWaitCondition Condition)
	{
		FFF_DialogueLine Line;
		Line.Kind = EFF_DialogueLineKind::WaitForAction;
		Line.WaitCondition = Condition;
		return Line;
	}

	FFF_DialogueLine MakeChoice()
	{
		FFF_DialogueLine Line;
		Line.Kind = EFF_DialogueLineKind::Choice;
		return Line;
	}
}

using namespace FFF_TutorialTestHelpers;

// 1. 건너뛸 수 있는 줄과 없는 줄이 구분되는지.
//
//    조작을 기다리는 줄까지 건너뛰면 무엇을 눌러야 하는지 모른 채 게임이 시작된다.
//    서사만 넘기고 안내는 남기는 것이 목적이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Tutorial_SkippabilityBlocksActionLines, "FestaFiesta.Tutorial.Line.SkippabilityBlocksActionLines", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Tutorial_SkippabilityBlocksActionLines::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("일반 대사는 건너뛸 수 있다"), MakeSpeech().CanSkip());

	// 조작 대기는 건너뛰면 진행이 막힌다.
	TestFalse(TEXT("조작 대기는 건너뛸 수 없다"),
		MakeWait(EFF_TutorialWaitCondition::FacilityPlaced).CanSkip());

	// 선택지도 마찬가지다. 안 고르면 분기가 정해지지 않는다.
	TestFalse(TEXT("선택지는 건너뛸 수 없다"), MakeChoice().CanSkip());

	// 플래그로 개별 지정도 된다. 중요한 안내는 서사여도 남길 수 있다.
	FFF_DialogueLine Important = MakeSpeech();
	Important.bSkippable = false;
	TestFalse(TEXT("플래그로 막을 수 있다"), Important.CanSkip());
	return true;
}

// 2. 조작을 기다리는지 판정.
//     조건이 없으면 기다리지 않는다. 조건을 안 넣고 대기 줄을 만들면 영영 멈춘다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Tutorial_WaitRequiresCondition, "FestaFiesta.Tutorial.Line.WaitRequiresCondition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Tutorial_WaitRequiresCondition::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("조건이 있으면 기다린다"),
		MakeWait(EFF_TutorialWaitCondition::CameraMoved).RequiresPlayerAction());

	// 조건 없는 대기 줄은 기다리지 않는 것으로 본다.
	// 그래야 데이터를 잘못 만들어도 진행이 막히지 않는다.
	FFF_DialogueLine Broken;
	Broken.Kind = EFF_DialogueLineKind::WaitForAction;
	Broken.WaitCondition = EFF_TutorialWaitCondition::None;
	TestFalse(TEXT("조건이 없으면 기다리지 않는다"), Broken.RequiresPlayerAction());

	TestFalse(TEXT("일반 대사는 기다리지 않는다"), MakeSpeech().RequiresPlayerAction());
	return true;
}

// 3. 라벨로 줄을 찾는다.
//     분기를 줄 번호로 두면 중간에 대사를 끼워 넣을 때 전부 어긋난다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Tutorial_LabelLookup, "FestaFiesta.Tutorial.Script.LabelLookup", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Tutorial_LabelLookup::RunTest(const FString& Parameters)
{
	UFF_DialogueScriptData* Script = NewObject<UFF_DialogueScriptData>();
	Script->Lines.Add(MakeSpeech());
	Script->Lines.Add(MakeSpeech(TEXT("Branch_A")));
	Script->Lines.Add(MakeSpeech());
	Script->Lines.Add(MakeSpeech(TEXT("Branch_B")));

	TestEqual(TEXT("첫 라벨"), Script->FindLineIndexByLabel(TEXT("Branch_A")), 1);
	TestEqual(TEXT("둘째 라벨"), Script->FindLineIndexByLabel(TEXT("Branch_B")), 3);

	// 없는 라벨은 INDEX_NONE. 호출부가 다음 줄로 넘겨 진행이 막히지 않게 한다.
	TestEqual(TEXT("없는 라벨"), Script->FindLineIndexByLabel(TEXT("Nope")), INDEX_NONE);
	TestEqual(TEXT("빈 라벨"), Script->FindLineIndexByLabel(NAME_None), INDEX_NONE);

	// 라벨을 안 붙인 줄이 NAME_None 으로 잡히면 안 된다.
	// 그러면 라벨 없는 첫 줄로 항상 점프한다.
	TestNotEqual(TEXT("라벨 없는 줄이 걸리지 않는다"),
		Script->FindLineIndexByLabel(NAME_None), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
