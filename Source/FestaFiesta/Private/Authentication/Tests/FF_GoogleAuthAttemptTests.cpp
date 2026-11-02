#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Authentication/FF_GoogleAuthSubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "UObject/StrongObjectPtr.h"
namespace
{
	/** A fixed in-memory provider response. No request in this fixture is ever ProcessRequest'ed. */
	class FFF_GoogleAuthTestResponse final : public IHttpResponse
	{
	public:
		FFF_GoogleAuthTestResponse()
		{
			const FTCHARToUTF8 Utf8(*Body);
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		}
		virtual const FString& GetURL() const override { return Url; }
		virtual const FString& GetEffectiveURL() const override { return Url; }
		virtual EHttpRequestStatus::Type GetStatus() const override { return EHttpRequestStatus::Succeeded; }
		virtual EHttpFailureReason GetFailureReason() const override { return EHttpFailureReason::None; }
		virtual FString GetURLParameter(const FString&) const override { return FString(); }
		virtual FString GetHeader(const FString&) const override { return FString(); }
		virtual TArray<FString> GetAllHeaders() const override { return {}; }
		virtual FString GetContentType() const override { return TEXT("application/json"); }
		virtual uint64 GetContentLength() const override { return Bytes.Num(); }
		virtual const TArray<uint8>& GetContent() const override { return Bytes; }
		virtual int32 GetResponseCode() const override { return 200; }
		virtual FString GetContentAsString() const override { return Body; }
		virtual FUtf8StringView GetContentAsUtf8StringView() const override
		{
			return FUtf8StringView(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
		}
	private:
		FString Url = TEXT("https://openidconnect.googleapis.com/v1/userinfo");
		FString Body = TEXT("{\"sub\":\"verified-test-sub\",\"name\":\"Test Profile\",\"email\":\"test@example.invalid\",\"email_verified\":true}");
		TArray<uint8> Bytes;
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_GoogleAuthAttemptLifecycleTest,
	"FestaFiesta.Authentication.Attempt.TimeoutCancellationAndLateCompletion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_GoogleAuthAttemptLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Do not initialize GI: this fixture touches neither game subsystems nor player saves.
	TStrongObjectPtr<UGameInstance> Game(NewObject<UGameInstance>());
	TStrongObjectPtr<UFF_GoogleAuthSubsystem> Auth(NewObject<UFF_GoogleAuthSubsystem>(Game.Get()));
	TestTrue(TEXT("First title may offer sign-in"), Auth->ConsumeInitialSignInPrompt());
	TestFalse(TEXT("Recreated title must not repeat the initial prompt"), Auth->ConsumeInitialSignInPrompt());
	const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Profile = MakeShared<FFF_GoogleAuthTestResponse, ESPMode::ThreadSafe>();
	// Unprocessed HTTP objects exercise real cancellation bookkeeping without sending anything.
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> OldRequest = FHttpModule::Get().CreateRequest();
	Auth->ActiveRequest = OldRequest;
	Auth->State = EFF_GoogleAuthState::ExchangingCode;
	Auth->Deadline = FPlatformTime::Seconds() + 30.0;
	const uint64 CancelledGeneration = Auth->AttemptGeneration;
	Auth->CancelSignIn();
	TestEqual(TEXT("Active HTTP cancellation signs out"), Auth->GetState(), EFF_GoogleAuthState::SignedOut);
	TestFalse(TEXT("Active HTTP object released on cancel"), Auth->ActiveRequest.IsValid());
	Auth->HandleTokenResponse(OldRequest, nullptr, false, CancelledGeneration);
	Auth->HandleProfileResponse(OldRequest, Profile, true, CancelledGeneration);
	TestEqual(TEXT("Late token/profile callbacks cannot change cancelled state"), Auth->GetState(), EFF_GoogleAuthState::SignedOut);
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> NewRequest = FHttpModule::Get().CreateRequest();
	Auth->ActiveRequest = NewRequest;
	Auth->State = EFF_GoogleAuthState::ExchangingCode;
	const uint64 NewGeneration = Auth->AttemptGeneration;
	Auth->HandleProfileResponse(OldRequest, Profile, true, CancelledGeneration);
	Auth->HandleProfileResponse(OldRequest, Profile, true, NewGeneration);
	Auth->HandleProfileResponse(NewRequest, Profile, true, CancelledGeneration);
	TestEqual(TEXT("Request identity and generation both isolate a newer attempt"),
		Auth->GetState(), EFF_GoogleAuthState::ExchangingCode);
	TestTrue(TEXT("Stale completion leaves new request untouched"), Auth->ActiveRequest == NewRequest);
	TestTrue(TEXT("Stale completion cannot populate the new account"), Auth->GetAccount().Subject.IsEmpty());
	Auth->HandleProfileResponse(NewRequest, Profile, true, NewGeneration);
	TestEqual(TEXT("Current provider response can complete the local session"), Auth->GetState(), EFF_GoogleAuthState::SignedIn);
	TestEqual(TEXT("Current response produces provider subject"), Auth->GetAccount().Subject, FString(TEXT("verified-test-sub")));

	Auth->SignOut();
	TestFalse(TEXT("Signout does not repeat a dismissed startup prompt"), Auth->ConsumeInitialSignInPrompt());
	Auth->HandleProfileResponse(NewRequest, Profile, true, NewGeneration);
	TestEqual(TEXT("Late callback cannot undo signout"), Auth->GetState(), EFF_GoogleAuthState::SignedOut);
	TestTrue(TEXT("Signout clears a populated profile"), Auth->GetAccount().Subject.IsEmpty()
		&& Auth->GetAccount().DisplayName.IsEmpty() && Auth->GetAccount().Email.IsEmpty());
	TStrongObjectPtr<UGameInstance> SignedInGame(NewObject<UGameInstance>());
	TStrongObjectPtr<UFF_GoogleAuthSubsystem> SignedInAuth(NewObject<UFF_GoogleAuthSubsystem>(SignedInGame.Get()));
	SignedInAuth->State = EFF_GoogleAuthState::SignedIn;
	TestFalse(TEXT("An already signed-in game skips the startup prompt"), SignedInAuth->ConsumeInitialSignInPrompt());
	SignedInAuth->SignOut();
	TestFalse(TEXT("A skipped startup prompt stays consumed after signout"), SignedInAuth->ConsumeInitialSignInPrompt());

	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> TimedOutRequest = FHttpModule::Get().CreateRequest();
	Auth->ActiveRequest = TimedOutRequest;
	Auth->State = EFF_GoogleAuthState::ExchangingCode;
	Auth->Deadline = FPlatformTime::Seconds() - 1.0;
	TestTrue(TEXT("Temporary callback binds on loopback"), Auth->OpenLoopbackListener());
	const uint64 HttpGeneration = Auth->AttemptGeneration;
	Auth->TickLogin(0.f);
	Auth->HandleProfileResponse(TimedOutRequest, Profile, true, HttpGeneration);
	TestEqual(TEXT("Late HTTP success cannot undo timeout"), Auth->GetState(), EFF_GoogleAuthState::Error);
	TestFalse(TEXT("HTTP timeout cancels its request"), Auth->ActiveRequest.IsValid());
	TestTrue(TEXT("Timeout closes both sockets"), Auth->Listener == nullptr && Auth->Connection == nullptr);
	Auth->Deinitialize();
	return true;
}
#endif
