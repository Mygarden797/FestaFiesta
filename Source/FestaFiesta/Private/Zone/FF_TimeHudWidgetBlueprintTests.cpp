// FF_TimeHudWidgetBlueprintTests.cpp

// Time HUD Blueprint의 루트 hit-test 계약이 UI 입력을 가로막지 않는지 검증한다.
// 흐름: 고정 자산 경로의 생성 클래스를 로드하고 컴파일된 WidgetTree 루트의 Visibility를 확인한다.
// 불변식: 투명한 루트는 입력을 무시하지만 자식 버튼은 계속 상호작용할 수 있어야 한다.
// 실패/성능: 에디터 개발 자동화 빌드에서만 자산을 읽으며 런타임 상태는 변경하지 않는다.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TimeHudHitTestContractTest,
	"FestaFiesta.Time.UI.TimeHudHitTestContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_TimeHudHitTestContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// 자산 이름 변경도 UI 계약 파손이므로 런타임에서 사용하는 고정 생성 클래스 경로를 그대로 검증한다.
	constexpr TCHAR TimeHudClassPath[] =
		TEXT("/Game/FestaFiesta/UI/WBP_TimeHud.WBP_TimeHud_C");
	UClass* TimeHudClass = LoadClass<UUserWidget>(nullptr, TimeHudClassPath);
	TestNotNull(TEXT("Time HUD generated class loads from its stable asset path"), TimeHudClass);

	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(TimeHudClass);
	TestNotNull(TEXT("Time HUD is a compiled Widget Blueprint"), GeneratedClass);
	// 앞선 assertion이 원인을 기록하므로 잘못된 타입을 역참조해 부수 오류를 만들지 않는다.
	if (!GeneratedClass)
	{
		return true;
	}

	const UWidgetTree* WidgetTree = GeneratedClass->GetWidgetTreeArchetype();
	TestNotNull(TEXT("Time HUD has a compiled WidgetTree"), WidgetTree);
	if (!WidgetTree)
	{
		return true;
	}

	const UWidget* RootWidget = WidgetTree->RootWidget.Get();
	TestNotNull(TEXT("Time HUD has a root widget"), RootWidget);
	if (RootWidget)
	{
		// The HUD spans the viewport. Its root must ignore hits on transparent space while
		// allowing visible child buttons to remain interactive. Visible blocks screens below;
		// HitTestInvisible also disables every child, so SelfHitTestInvisible is the only safe policy.
		TestEqual(
			TEXT("Time HUD root ignores self hits without disabling child controls"),
			RootWidget->GetVisibility(),
			ESlateVisibility::SelfHitTestInvisible);
	}

	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
