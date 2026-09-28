// FF_ZoneLifecycleTestPanelTests.cpp

// 저장된 Lifecycle 개발 WBP가 네이티브 클래스와 필수 버튼·텍스트 이름 계약을 지키는지 검증한다.
// 흐름: Blueprint/GeneratedClass 확인 -> 필수 CommonText 목록 검사 -> 필수 Button 목록 검사.
// 불변식: 테스트는 에디터 에셋을 읽기만 하며 이름과 위젯 타입을 함께 고정한다.
// 실패/성능: 핵심 루트 계약이 없으면 조기 종료하고 각 자식 누락은 이름별 assertion으로 누적한다.

#include "Building/Testing/FF_ZoneLifecycleTestPanel.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#include "FF_ZoneLifecycleDebugDayControl.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "WidgetBlueprint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneLifecycleTestPanelAssetContractTest,
	"FestaFiesta.Building.UI.ZoneLifecycleTestPanel.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneLifecycleTestPanelAssetContractTest::RunTest(const FString& Parameters)
{
	// 루트 계약이 깨진 경우 자식 순회를 중단해 null 접근보다 실제 에셋 원인을 먼저 보고한다.
	(void)Parameters;
	static const TCHAR* PanelAssetPath =
		FFBuildUIAssetPaths::ZoneLifecycleTestPanelObject;
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, PanelAssetPath);
	if (!TestNotNull(TEXT("시설 Lifecycle 테스트 WBP가 존재한다"), Blueprint)
		|| !TestNotNull(TEXT("WBP에 WidgetTree가 존재한다"),
			Blueprint ? Blueprint->WidgetTree.Get() : nullptr)
		|| !TestNotNull(TEXT("WBP GeneratedClass가 존재한다"),
			Blueprint ? Blueprint->GeneratedClass.Get() : nullptr))
	{
		return false;
	}

	TestTrue(TEXT("WBP가 네이티브 테스트 패널을 상속한다"),
		Blueprint->GeneratedClass->IsChildOf(UFF_ZoneLifecycleTestPanel::StaticClass()));
	// 필드별 assertion을 누적해 한 번의 실행에서 누락된 모든 BindWidget 이름을 확인한다.
	for (const FName TextName : {
		FName(TEXT("TXT_Stage")),
		FName(TEXT("TXT_PhaseRevision")),
		FName(TEXT("TXT_FacilityCounts")),
		FName(TEXT("TXT_QueueCounts")),
		FName(TEXT("TXT_LastResult"))})
	{
		TestTrue(*FString::Printf(TEXT("%s CommonText가 존재한다"), *TextName.ToString()),
			Cast<UCommonTextBlock>(Blueprint->WidgetTree->FindWidget(TextName)) != nullptr);
	}
	for (const FName ButtonName : {
		FName(TEXT("BTN_Setup")),
		FName(TEXT("BTN_MorningCheck")),
		FName(TEXT("BTN_Live")),
		FName(TEXT("BTN_Closing")),
		FName(TEXT("BTN_PreviousDay")),
		FName(TEXT("BTN_NextDay")),
		FName(TEXT("BTN_OpenAll")),
		FName(TEXT("BTN_CloseAll")),
		FName(TEXT("BTN_EmergencyCloseAll")),
		FName(TEXT("BTN_Refresh"))})
	{
		TestTrue(*FString::Printf(TEXT("%s Button이 존재한다"), *ButtonName.ToString()),
			Cast<UButton>(Blueprint->WidgetTree->FindWidget(ButtonName)) != nullptr);
	}
	const UHorizontalBox* DayRow = Cast<UHorizontalBox>(
		Blueprint->WidgetTree->FindWidget(TEXT("Day_Button_Row")));
	const UButton* PreviousDay = Cast<UButton>(
		Blueprint->WidgetTree->FindWidget(TEXT("BTN_PreviousDay")));
	const UButton* NextDay = Cast<UButton>(
		Blueprint->WidgetTree->FindWidget(TEXT("BTN_NextDay")));
	TestNotNull(TEXT("날짜 이동 버튼 행이 존재한다"), DayRow);
	TestTrue(TEXT("이전 날 버튼은 날짜 이동 행에 속한다"),
		DayRow && PreviousDay && PreviousDay->GetParent() == DayRow);
	TestTrue(TEXT("다음 날 버튼은 날짜 이동 행에 속한다"),
		DayRow && NextDay && NextDay->GetParent() == DayRow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneLifecycleDayControlTest,
	"FestaFiesta.Building.UI.ZoneLifecycleTestPanel.DayControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneLifecycleDayControlTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UFF_FestivalStageSubsystem* Stage = GameInstance
		? NewObject<UFF_FestivalStageSubsystem>(GameInstance)
		: nullptr;
	if (!TestNotNull(TEXT("Stage 테스트 인스턴스"), Stage))
	{
		return false;
	}

	Stage->CurrentDay = 3;
	Stage->CurrentStage = EFF_FestivalStage::Report;
	TestTrue(TEXT("Day3은 이전 날로 이동한다"),
		FFZoneLifecycleDebugDayControl::TryMoveToPreviousDay(*Stage));
	TestEqual(TEXT("Day3 -> Day2"), Stage->CurrentDay, 2);
	TestEqual(TEXT("Day2 진입점은 MorningCheck"),
		Stage->CurrentStage, EFF_FestivalStage::MorningCheck);

	Stage->CurrentStage = EFF_FestivalStage::Live;
	TestTrue(TEXT("Day2 Live도 이전 날로 이동한다"),
		FFZoneLifecycleDebugDayControl::TryMoveToPreviousDay(*Stage));
	TestEqual(TEXT("Day2 -> Day1"), Stage->CurrentDay, 1);
	TestEqual(TEXT("Day1 진입점은 Setup"), Stage->CurrentStage, EFF_FestivalStage::Setup);
	TestFalse(TEXT("Day1에서는 더 이전으로 이동하지 않는다"),
		FFZoneLifecycleDebugDayControl::TryMoveToPreviousDay(*Stage));
	TestEqual(TEXT("Day1 하한을 유지한다"), Stage->CurrentDay, 1);

	TestTrue(TEXT("Day1은 다음 날로 이동한다"),
		FFZoneLifecycleDebugDayControl::TryMoveToNextDay(*Stage));
	TestEqual(TEXT("Day1 -> Day2"), Stage->CurrentDay, 2);
	TestEqual(TEXT("Day2 진입점은 MorningCheck"),
		Stage->CurrentStage, EFF_FestivalStage::MorningCheck);
	TestTrue(TEXT("Day2도 다음 날로 이동한다"),
		FFZoneLifecycleDebugDayControl::TryMoveToNextDay(*Stage));
	TestEqual(TEXT("Day2 -> Day3"), Stage->CurrentDay, 3);
	TestFalse(TEXT("Day3에서는 다음 날로 이동하지 않는다"),
		FFZoneLifecycleDebugDayControl::TryMoveToNextDay(*Stage));
	TestEqual(TEXT("Day3 상한을 유지한다"), Stage->CurrentDay, 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
