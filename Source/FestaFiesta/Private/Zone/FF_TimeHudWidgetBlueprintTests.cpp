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

	constexpr TCHAR TimeHudClassPath[] =
		TEXT("/Game/FestaFiesta/UI/WBP_TimeHud.WBP_TimeHud_C");
	UClass* TimeHudClass = LoadClass<UUserWidget>(nullptr, TimeHudClassPath);
	TestNotNull(TEXT("Time HUD generated class loads from its stable asset path"), TimeHudClass);

	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(TimeHudClass);
	TestNotNull(TEXT("Time HUD is a compiled Widget Blueprint"), GeneratedClass);
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
