#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Authentication/FF_GoogleAuthSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/Account/FF_GoogleAccountPanel.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_GoogleAccountSurfacesTest,
	"FestaFiesta.Authentication.Google.SharedAccountSurfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_GoogleAccountSurfacesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Playerless editor world"), World)) return false;
	// A transient session double avoids GI initialization, player saves and any Google request.
	TStrongObjectPtr<UGameInstance> Game(NewObject<UGameInstance>());
	TStrongObjectPtr<UFF_GoogleAuthSubsystem> Auth(NewObject<UFF_GoogleAuthSubsystem>(Game.Get()));
	TStrongObjectPtr<UFF_GoogleAccountPanel> First(CreateWidget<UFF_GoogleAccountPanel>(World));
	TStrongObjectPtr<UFF_GoogleAccountPanel> Second(CreateWidget<UFF_GoogleAccountPanel>(World));
	if (!TestNotNull(TEXT("First account surface"), First.Get())
		|| !TestNotNull(TEXT("Second account surface"), Second.Get())) return false;
	First->SetAuthSubsystem(Auth.Get());
	Second->SetAuthSubsystem(Auth.Get());
	TSharedPtr<SWidget> FirstSlate = First->TakeWidget();
	TSharedPtr<SWidget> SecondSlate = Second->TakeWidget();
	auto ReadText = [this](UFF_GoogleAccountPanel& Panel, const TCHAR* Name)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Name));
		TestNotNull(FString(TEXT("Account field: ")) + Name, Text);
		return Text ? Text->GetText().ToString() : FString();
	};
	const FString Name(TEXT("검증용 축제 플레이어"));
	const FString Email(TEXT("festival.account@example.invalid"));
	Auth->Account.Subject = TEXT("test-only-subject");
	Auth->Account.DisplayName = Name;
	Auth->Account.Email = Email;
	Auth->SetState(EFF_GoogleAuthState::SignedIn, FText::FromString(TEXT("Google 계정으로 로그인했습니다.")));
	for (UFF_GoogleAccountPanel* Panel : { First.Get(), Second.Get() })
	{
		TestEqual(TEXT("Session event displays the real account name"), ReadText(*Panel, TEXT("Text_GoogleAccountName")), Name);
		TestEqual(TEXT("Session event displays the real account email"), ReadText(*Panel, TEXT("Text_GoogleAccountEmail")), Email);
		TestEqual(TEXT("Signed-in action is logout"), ReadText(*Panel, TEXT("Text_GoogleAccountAction")), FString(TEXT("로그아웃")));
	}
	UButton* Logout = Cast<UButton>(Second->GetWidgetFromName(TEXT("Button_GoogleAccount")));
	if (!TestNotNull(TEXT("Logout action"), Logout)) return false;
	Logout->OnClicked.Broadcast();
	TestEqual(TEXT("The displayed logout action updates the shared session"), Auth->GetState(), EFF_GoogleAuthState::SignedOut);
	for (UFF_GoogleAccountPanel* Panel : { First.Get(), Second.Get() })
	{
		TestFalse(TEXT("Logout removes the prior name from every surface"), ReadText(*Panel, TEXT("Text_GoogleAccountName")).Contains(Name));
		TestFalse(TEXT("Logout removes the prior email from every surface"), ReadText(*Panel, TEXT("Text_GoogleAccountEmail")).Contains(Email));
	}
	Auth->SetState(EFF_GoogleAuthState::WaitingForBrowser, FText::FromString(TEXT("브라우저에서 로그인을 완료해 주세요.")));
	TestTrue(TEXT("Waiting displays the cancel action"), ReadText(*First, TEXT("Text_GoogleAccountAction")).Contains(TEXT("취소")));
	UButton* Cancel = Cast<UButton>(First->GetWidgetFromName(TEXT("Button_GoogleAccount")));
	if (!TestNotNull(TEXT("Cancel action"), Cancel)) return false;
	Cancel->OnClicked.Broadcast();
	TestEqual(TEXT("The displayed cancel action clears the attempt"), Auth->GetState(), EFF_GoogleAuthState::SignedOut);
	const FString Failure(TEXT("테스트 연결 오류"));
	Auth->Fail(FText::FromString(Failure));
	TestEqual(TEXT("Failure messages stay live on both surfaces"), ReadText(*Second, TEXT("Text_GoogleAccountStatus")), Failure);
	FirstSlate.Reset();
	First->ReleaseSlateResources(true);
	Auth->Account.DisplayName = TEXT("다시 연결된 플레이어");
	Auth->Account.Email = TEXT("reconnected@example.invalid");
	Auth->Account.Subject = TEXT("test-only-reconnected-subject");
	Auth->SetState(EFF_GoogleAuthState::SignedIn, FText::FromString(TEXT("로그인됨")));
	FirstSlate = First->TakeWidget();
	TestEqual(TEXT("Reconstructed account surface reads the current session"),
		ReadText(*First, TEXT("Text_GoogleAccountEmail")), FString(TEXT("reconnected@example.invalid")));
	Auth->SignOut();
	return true;
}

#endif
