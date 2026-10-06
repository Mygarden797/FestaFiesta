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
		FString(TEXT("출동할 스태프가 부족해요 · 안내 시설에 스태프를 배정해 주세요")));
	TestEqual(TEXT("Every rejection carries the same short label"),
		UFF_UIDisplayTextLibrary::GetIncidentResolveResultLabel(EFF_IncidentResolveResult::NoSite).ToString(), FString(TEXT("불가")));

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

	auto TextOf = [](EFF_IncidentResolveResult Result)
	{
		return UFF_UIDisplayTextLibrary::GetIncidentResolveResultText(Result);
	};
	auto ShowResult = [&Toast](EFF_IncidentResolveResult Result)
	{
		Toast->Show(UFF_UIDisplayTextLibrary::GetIncidentResolveResultLabel(Result),
			UFF_UIDisplayTextLibrary::GetIncidentResolveResultText(Result),
			UFF_UIDisplayTextLibrary::IsIncidentResolveAccepted(Result));
	};

	ShowResult(EFF_IncidentResolveResult::Dispatched);
	TestTrue(TEXT("Showing makes the toast visible"), Toast->IsShowing());
	TestEqual(TEXT("One result is one line"), Toast->GetEntryCount(), 1);
	TestEqual(TEXT("The line leads with a word, not only a color"), Toast->GetLabelAt(0).ToString(), FString(TEXT("출동")));
	TestEqual(TEXT("The line carries the result text"),
		Toast->GetMessage().ToString(), TextOf(EFF_IncidentResolveResult::Dispatched).ToString());
	TestEqual(TEXT("The toast never blocks world clicks"), Toast->GetVisibility(), ESlateVisibility::HitTestInvisible);

	Toast->AdvanceTime(1.f);
	ShowResult(EFF_IncidentResolveResult::AlreadyResponding);
	TestEqual(TEXT("A second result stacks instead of replacing"), Toast->GetEntryCount(), 2);
	TestEqual(TEXT("The newest result is on top"),
		Toast->GetMessageAt(0).ToString(), TextOf(EFF_IncidentResolveResult::AlreadyResponding).ToString());
	TestEqual(TEXT("The earlier result moves below"),
		Toast->GetMessageAt(1).ToString(), TextOf(EFF_IncidentResolveResult::Dispatched).ToString());

	Toast->AdvanceTime(0.5f);
	ShowResult(EFF_IncidentResolveResult::NotEnoughStaff);
	ShowResult(EFF_IncidentResolveResult::NoSite);
	TestEqual(TEXT("A rejection is labelled as one"), Toast->GetLabelAt(0).ToString(), FString(TEXT("불가")));
	TestEqual(TEXT("No more than three lines are shown"), Toast->GetEntryCount(), UFF_IncidentToast::MaxEntries);
	TestEqual(TEXT("The fourth result pushes out the oldest line"),
		Toast->GetMessageAt(2).ToString(), TextOf(EFF_IncidentResolveResult::AlreadyResponding).ToString());
	TestEqual(TEXT("The newest of four is on top"),
		Toast->GetMessageAt(0).ToString(), TextOf(EFF_IncidentResolveResult::NoSite).ToString());

	Toast->AdvanceTime(1.7f);
	TestEqual(TEXT("Lines expire on their own clocks: the oldest remaining line fades first"),
		Toast->GetEntryCount(), 3);
	TestTrue(TEXT("The line shown earliest is fading"), Toast->GetOpacityAt(2) < 1.f);
	TestEqual(TEXT("Newer lines are still fully visible"), Toast->GetOpacityAt(0), 1.f);

	Toast->AdvanceTime(0.3f);
	TestEqual(TEXT("The earliest line leaves when its time is up"), Toast->GetEntryCount(), 2);
	TestEqual(TEXT("The remaining newest line stays on top"),
		Toast->GetMessageAt(0).ToString(), TextOf(EFF_IncidentResolveResult::NoSite).ToString());

	Toast->AdvanceTime(3.f);
	TestFalse(TEXT("All lines hide after their display time"), Toast->IsShowing());
	TestEqual(TEXT("The empty toast is collapsed"), Toast->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

#endif
