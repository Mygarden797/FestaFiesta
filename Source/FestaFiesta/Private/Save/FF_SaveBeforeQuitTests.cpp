// FF_SaveBeforeQuitTests.cpp
// 역할: "게임 저장 후 종료" 의 종료 허용 규칙을 고정한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Save/FF_SaveBeforeQuitPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SaveBeforeQuit_ClassifyAndCanQuit,
	"FestaFiesta.Save.SaveBeforeQuit.ClassifyAndCanQuit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_SaveBeforeQuit_ClassifyAndCanQuit::RunTest(const FString& Parameters)
{
	using namespace FF::SaveBeforeQuit;

	// 저장에 성공했으면 축제 여부와 상관없이 종료한다.
	TestTrue(TEXT("축제 도중 저장 성공"),
		Classify(true, true) == EFF_SaveBeforeQuitResult::Saved);
	TestTrue(TEXT("로비 저장 성공"),
		Classify(true, false) == EFF_SaveBeforeQuitResult::Saved);

	// 타이틀처럼 담을 판이 없으면 저장 없이 종료해도 잃는 것이 없다.
	TestTrue(TEXT("담을 판 없음"),
		Classify(false, false) == EFF_SaveBeforeQuitResult::NothingToSave);

	// 축제 도중 저장 실패는 종료를 막아야 진행을 잃지 않는다.
	TestTrue(TEXT("축제 도중 저장 실패"),
		Classify(false, true) == EFF_SaveBeforeQuitResult::Failed);

	TestTrue(TEXT("Saved 는 종료 허용"), CanQuit(EFF_SaveBeforeQuitResult::Saved));
	TestTrue(TEXT("NothingToSave 는 종료 허용"), CanQuit(EFF_SaveBeforeQuitResult::NothingToSave));
	TestFalse(TEXT("Failed 는 종료 보류"), CanQuit(EFF_SaveBeforeQuitResult::Failed));

	// 축제는 그날의 시작에서만 담는다. 운영 중 세이브는 하루를 처음부터 다시 돌려 매출이 두 번 잡힌다.
	TestTrue(TEXT("셋업 저장"), CanSaveFestivalAt(EFF_FestivalStage::Setup));
	TestTrue(TEXT("모닝체크 저장"), CanSaveFestivalAt(EFF_FestivalStage::MorningCheck));
	TestFalse(TEXT("라이브 저장 안 함"), CanSaveFestivalAt(EFF_FestivalStage::Live));
	TestFalse(TEXT("마감 저장 안 함"), CanSaveFestivalAt(EFF_FestivalStage::Closing));
	TestFalse(TEXT("리포트 저장 안 함"), CanSaveFestivalAt(EFF_FestivalStage::Report));
	TestFalse(TEXT("피날레 저장 안 함"), CanSaveFestivalAt(EFF_FestivalStage::Finale));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
