// 역할: Google 설정이 없는 빌드의 계정 수명과 오프라인 시작을 실제 GI에서 검증한다.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Authentication/FF_GoogleAuthSettings.h"
#include "Authentication/FF_GoogleAuthSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_GoogleAuthMissingConfigurationLifecycleTest,
	"FestaFiesta.Authentication.Integration.MissingConfigurationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_GoogleAuthMissingConfigurationLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// GI initialization can migrate save metadata. Never create it against player saves.
	FString IsolatedUserDirectory;
	const FString OrdinarySaved = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved")));
	if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), IsolatedUserDirectory)
		|| IsolatedUserDirectory.IsEmpty() || FPaths::IsRelative(IsolatedUserDirectory)
		|| FPaths::IsSamePath(FPaths::ProjectSavedDir(), OrdinarySaved))
	{
		AddError(TEXT("Google authentication integration requires -UserDir=<isolated absolute directory>."));
		return false;
	}

	// Restore the in-memory CDO on every exit. This test never calls SaveConfig.
	auto* Settings = GetMutableDefault<UFF_GoogleAuthSettings>();
	TGuardValue<FString> MissingClientId(Settings->DesktopClientId, FString());
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	if (!TestNotNull(TEXT("Isolated game instance"), Game)) return false;
	Game->OnWorldChanged(nullptr, World);
	auto* Auth = Game->GetSubsystem<UFF_GoogleAuthSubsystem>();
	if (!TestNotNull(TEXT("Production Google account subsystem"), Auth)) return false;
	ON_SCOPE_EXIT { Auth->SignOut(); };

	const EFF_GoogleAuthState InitialState = Auth->GetState();
	TestFalse(TEXT("Game startup does not sign in automatically"),
		InitialState == EFF_GoogleAuthState::SignedIn);
	TestFalse(TEXT("Game startup does not begin a browser or HTTP flow"), Auth->IsBusy());
	TestFalse(TEXT("A missing Desktop client is visibly unconfigured"), Auth->IsConfigured());
	TestTrue(TEXT("An unconfigured game exposes a status message"), !Auth->GetStatusMessage().IsEmpty());
	TestTrue(TEXT("Startup has no profile"),
		Auth->GetAccount().Subject.IsEmpty() && Auth->GetAccount().DisplayName.IsEmpty()
		&& Auth->GetAccount().Email.IsEmpty());

	// This must return before any loopback listener, browser, or HTTP request is started.
	Auth->SignIn();
	TestEqual(TEXT("Explicit sign-in without configuration reports a recoverable error"),
		Auth->GetState(), EFF_GoogleAuthState::Error);
	TestFalse(TEXT("Missing configuration never leaves an in-flight sign-in"), Auth->IsBusy());
	TestTrue(TEXT("The failure explains the unavailable account connection"),
		!Auth->GetStatusMessage().IsEmpty());
	TestTrue(TEXT("Failure cannot fabricate a Google profile"),
		Auth->GetAccount().Subject.IsEmpty() && Auth->GetAccount().DisplayName.IsEmpty()
		&& Auth->GetAccount().Email.IsEmpty());

	Auth->CancelSignIn();
	TestEqual(TEXT("Cancel clears the failed attempt back to startup state"), Auth->GetState(), InitialState);
	TestFalse(TEXT("Cancel leaves no in-flight login"), Auth->IsBusy());
	Auth->CancelSignIn();
	TestEqual(TEXT("Repeated cancel is harmless"), Auth->GetState(), InitialState);

	Auth->SignIn();
	TestEqual(TEXT("A later attempt remains recoverable"), Auth->GetState(), EFF_GoogleAuthState::Error);
	Auth->SignOut();
	Auth->SignOut();
	TestEqual(TEXT("Repeated sign-out clears error state"), Auth->GetState(), InitialState);
	TestFalse(TEXT("Sign-out leaves no in-flight login"), Auth->IsBusy());
	TestTrue(TEXT("Sign-out leaves no account identity"),
		Auth->GetAccount().Subject.IsEmpty() && Auth->GetAccount().DisplayName.IsEmpty()
		&& Auth->GetAccount().Email.IsEmpty());
	return true;
}

#endif
