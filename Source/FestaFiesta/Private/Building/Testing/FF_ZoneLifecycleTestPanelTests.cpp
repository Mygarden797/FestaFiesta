// Copyright Epic Games, Inc. All Rights Reserved.

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
