#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/Incident/FF_IncidentToast.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentToastTest,
	"FestaFiesta.UI.Incident.ResolveResultToast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentToastTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("A dispatch is an accepted result"),
		UFF_UIDisplayTextLibrary::IsIncidentResolveAccepted(EFF_IncidentResolveResult::Dispatched));
	TestTrue(TEXT("An immediate acknowledgement is an accepted result"),
		UFF_UIDisplayTextLibrary::IsIncidentResolveAccepted(EFF_IncidentResolveResult::Resolved));
	TestFalse(TEXT("A staff shortage is a rejection"),
		UFF_UIDisplayTextLibrary::IsIncidentResolveAccepted(EFF_IncidentResolveResult::NotEnoughStaff));
	TestEqual(TEXT("A staff shortage tells the player what to do"),
		UFF_UIDisplayTextLibrary::GetIncidentResolveResultText(EFF_IncidentResolveResult::NotEnoughStaff).ToString(),
		FString(TEXT("출동할 스태프가 부족합니다. 안내 시설에 스태프를 배정하세요")));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor fixture world"), World))
	{
		return false;
	}
	TStrongObjectPtr<UFF_IncidentToast> Toast(CreateWidget<UFF_IncidentToast>(World, UFF_IncidentToast::StaticClass()));
	if (!TestNotNull(TEXT("Toast instance"), Toast.Get()))
	{
		return false;
	}
	Toast->TakeWidget();
	TestFalse(TEXT("The toast starts hidden"), Toast->IsShowing());
	TestEqual(TEXT("A hidden toast is collapsed"), Toast->GetVisibility(), ESlateVisibility::Collapsed);

	const FText Message = UFF_UIDisplayTextLibrary::GetIncidentResolveResultText(EFF_IncidentResolveResult::Dispatched);
	Toast->Show(Message, true);
	TestTrue(TEXT("Showing makes the toast visible"), Toast->IsShowing());
	TestEqual(TEXT("The toast carries the result text"), Toast->GetMessage().ToString(), Message.ToString());
	TestEqual(TEXT("The toast never blocks world clicks"), Toast->GetVisibility(), ESlateVisibility::HitTestInvisible);

	Toast->AdvanceTime(2.3f);
	TestTrue(TEXT("The toast stays readable for its display time"), Toast->IsShowing());
	TestTrue(TEXT("The toast fades near the end"), Toast->GetRenderOpacity() < 1.f);

	Toast->Show(UFF_UIDisplayTextLibrary::GetIncidentResolveResultText(EFF_IncidentResolveResult::AlreadyResponding), false);
	TestEqual(TEXT("A new result restarts at full opacity"), Toast->GetRenderOpacity(), 1.f);
	Toast->AdvanceTime(3.f);
	TestFalse(TEXT("The toast hides after its display time"), Toast->IsShowing());
	TestEqual(TEXT("The expired toast is collapsed"), Toast->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

#endif
