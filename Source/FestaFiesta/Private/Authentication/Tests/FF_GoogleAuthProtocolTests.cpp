#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Authentication/FF_GoogleAuthProtocol.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_GoogleAuthPkceTest,
	"FestaFiesta.Authentication.Protocol.Pkce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_GoogleAuthPkceTest::RunTest(const FString& Parameters)
{
	using namespace FF::GoogleAuth;
#if PLATFORM_WINDOWS
	FString Challenge;
	TestTrue(TEXT("Windows SHA256 available"),
		CreateCodeChallenge(TEXT("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"), Challenge));
	TestEqual(TEXT("RFC 7636 appendix B S256 challenge"), Challenge,
		FString(TEXT("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM")));
	FString First, Second;
	TestTrue(TEXT("CSPRNG state generated"), CreateRandomToken(First));
	TestTrue(TEXT("CSPRNG verifier generated"), CreateRandomToken(Second));
	TestEqual(TEXT("256 random bits encode to 43 unpadded base64url characters"), First.Len(), 43);
	TestNotEqual(TEXT("Independent state/verifier values"), First, Second);
	for (const TCHAR Character : First + Second)
	{
		TestTrue(TEXT("PKCE unreserved encoding"), FChar::IsAlnum(Character) || Character == TEXT('-') || Character == TEXT('_'));
	}
#endif
	TestTrue(TEXT("A Google OAuth client ID is accepted"), IsDesktopClientId(TEXT("12345-desktop.apps.googleusercontent.com")));
	TestFalse(TEXT("Missing client ID rejected"), IsDesktopClientId(TEXT("")));
	TestFalse(TEXT("API key rejected"), IsDesktopClientId(TEXT("AIzaNotAnOAuthClient")));
	TestFalse(TEXT("An injected URL cannot become a client ID"), IsDesktopClientId(TEXT("https://example.com/?id=.apps.googleusercontent.com")));
	TestEqual(TEXT("Form encoder escapes authorization code separators"), FormValue(TEXT("a+b&c=d")), FString(TEXT("a%2Bb%26c%3Dd")));
	const FString Url = AuthorizationUrl(TEXT("client"), TEXT("http://127.0.0.1:53123/oauth2/callback"), TEXT("state"), TEXT("challenge"));
	TestTrue(TEXT("Only official system browser endpoint"), Url.StartsWith(TEXT("https://accounts.google.com/o/oauth2/v2/auth?")));
	TestTrue(TEXT("S256 enforced"), Url.Contains(TEXT("code_challenge_method=S256")));
	TestTrue(TEXT("Only basic profile scopes"), Url.Contains(TEXT("scope=openid%20email%20profile")));
	TestTrue(TEXT("No refresh token request"), Url.Contains(TEXT("access_type=online")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_GoogleAuthCallbackTest,
	"FestaFiesta.Authentication.Protocol.CallbackBoundary",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_GoogleAuthCallbackTest::RunTest(const FString& Parameters)
{
	using namespace FF::GoogleAuth;
	const FString Host(TEXT("127.0.0.1:53123"));
	const FString State(TEXT("expected_state"));
	auto Request = [&Host](const FString& Target, const FString& Method = TEXT("GET"))
	{
		return Method + TEXT(" ") + Target + TEXT(" HTTP/1.1\r\nHost: ") + Host + TEXT("\r\n\r\n");
	};
	FString Code;
	TestEqual(TEXT("Valid callback accepted"),
		ParseCallback(Request(TEXT("/oauth2/callback?state=expected_state&code=a%2Bb%2Fc")), Host, State, Code), ECallbackResult::Code);
	TestEqual(TEXT("Encoded code preserved"), Code, FString(TEXT("a+b/c")));
	TestEqual(TEXT("User rejection is terminal only with matching state"),
		ParseCallback(Request(TEXT("/oauth2/callback?state=expected_state&error=access_denied")), Host, State, Code), ECallbackResult::Denied);
	TestTrue(TEXT("Denied callback has no authorization code"), Code.IsEmpty());

	const TArray<FString> InvalidTargets = {
		TEXT("/oauth2/callback?state=wrong&code=code"),
		TEXT("/oauth2/callback?code=code"),
		TEXT("/oauth2/callback?state=expected_state&code="),
		TEXT("/oauth2/callback?state=expected_state&code=first&code=second"),
		TEXT("/oauth2/callback?state=expected_state&state=expected_state&code=code"),
		TEXT("/oauth2/callback?state=expected_state&code=code&error=access_denied"),
		TEXT("/oauth2/callback?state=expected_state&error=one&error=two"),
		TEXT("/oauth2/callback?state=expected_state&code=%ZZ"),
		TEXT("/oauth2/callback?state=expected_state&code=%00"),
		TEXT("/oauth2/callback?state=expected_state&code=code#fragment"),
		TEXT("/wrong?state=expected_state&code=code"),
		TEXT("https://attacker.invalid/oauth2/callback?state=expected_state&code=code")
	};
	for (const FString& Target : InvalidTargets)
	{
		Code = TEXT("stale-code");
		TestEqual(TEXT("Untrusted callback is rejected"), ParseCallback(Request(Target), Host, State, Code), ECallbackResult::Invalid);
		TestTrue(TEXT("Rejected callback clears output code"), Code.IsEmpty());
	}
	const FString ValidTarget(TEXT("/oauth2/callback?state=expected_state&code=code"));
	TestEqual(TEXT("POST cannot inject authorization"), ParseCallback(Request(ValidTarget, TEXT("POST")), Host, State, Code), ECallbackResult::Invalid);
	TestEqual(TEXT("A rebinding host cannot inject authorization"), ParseCallback(Request(ValidTarget), TEXT("attacker.invalid"), State, Code), ECallbackResult::Invalid);
	TestEqual(TEXT("An empty expected state can never accept a response"), ParseCallback(Request(ValidTarget), Host, TEXT(""), Code), ECallbackResult::Invalid);
	FString Incomplete = Request(ValidTarget);
	Incomplete.LeftChopInline(2);
	TestEqual(TEXT("Incomplete header remains invalid"), ParseCallback(Incomplete, Host, State, Code), ECallbackResult::Invalid);
	const FString DuplicateHost = Request(ValidTarget).Replace(TEXT("\r\n\r\n"), TEXT("\r\nHost: 127.0.0.1:53123\r\n\r\n"));
	TestEqual(TEXT("Duplicate Host is rejected"), ParseCallback(DuplicateHost, Host, State, Code), ECallbackResult::Invalid);
	const FString Oversized = Request(ValidTarget + TEXT("&padding=") + FString::ChrN(MaxCallbackBytes, TEXT('x')));
	TestEqual(TEXT("Oversized callback is rejected"), ParseCallback(Oversized, Host, State, Code), ECallbackResult::Invalid);
	return true;
}

#endif
