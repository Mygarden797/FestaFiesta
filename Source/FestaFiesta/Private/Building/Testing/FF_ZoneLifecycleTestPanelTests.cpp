// FF_ZoneLifecycleTestPanelTests.cpp

// 저장된 Lifecycle 개발 WBP가 네이티브 클래스와 필수 버튼·텍스트 이름 계약을 지키는지 검증한다.
// 흐름: Blueprint/GeneratedClass 확인 -> 필수 CommonText 목록 검사 -> 필수 Button 목록 검사.
// 불변식: 테스트는 에디터 에셋을 읽기만 하며 이름과 위젯 타입을 함께 고정한다.
// 실패/성능: 핵심 루트 계약이 없으면 조기 종료하고 각 자식 누락은 이름별 assertion으로 누적한다.

#include "Building/Testing/FF_ZoneLifecycleTestPanel.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Misc/AutomationTest.h"
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
		TEXT("/Game/FestaFiesta/Building/Debug/WBP_FF_ZoneLifecycleTestPanel."
			"WBP_FF_ZoneLifecycleTestPanel");
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
		FName(TEXT("BTN_OpenAll")),
		FName(TEXT("BTN_CloseAll")),
		FName(TEXT("BTN_EmergencyCloseAll")),
		FName(TEXT("BTN_Refresh"))})
	{
		TestTrue(*FString::Printf(TEXT("%s Button이 존재한다"), *ButtonName.ToString()),
			Cast<UButton>(Blueprint->WidgetTree->FindWidget(ButtonName)) != nullptr);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
