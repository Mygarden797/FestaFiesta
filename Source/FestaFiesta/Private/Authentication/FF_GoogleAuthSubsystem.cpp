#include "Authentication/FF_GoogleAuthSubsystem.h"

#include "Authentication/FF_GoogleAuthProtocol.h"
#include "Authentication/FF_GoogleAuthSettings.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "IPAddress.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#define LOCTEXT_NAMESPACE "FFGoogleLogin"

namespace
{
	void ClearPrivateString(FString& Value)
	{
		if (!Value.IsEmpty())
		{
			FMemory::Memzero(Value.GetCharArray().GetData(), Value.GetCharArray().Num() * sizeof(TCHAR));
		}
		Value.Reset();
	}

	bool ReadGoogleResponse(const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>& Response,
		const bool bSucceeded, TSharedPtr<FJsonObject>& Json)
	{
		if (!bSucceeded || !Response.IsValid() || Response->GetResponseCode() != 200
			|| Response->GetContent().Num() > 32768)
		{
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		return FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid();
	}
}

bool UFF_GoogleAuthSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UFF_GoogleAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bDeinitializing = false;
	bInitialSignInPromptConsumed = false;
	StatusMessage = IsConfigured()
		? LOCTEXT("SignedOut", "Google 계정으로 로그인할 수 있습니다.")
		: LOCTEXT("SetupNeeded", "Google 로그인 설정이 필요합니다. 오프라인으로 플레이할 수 있습니다.");
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFF_GoogleAuthSubsystem::TickLogin), 0.05f);
}

void UFF_GoogleAuthSubsystem::Deinitialize()
{
	bDeinitializing = true;
	FTSTicker::RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	StopPendingWork();
	Account = FFF_GoogleAccount();
	State = EFF_GoogleAuthState::SignedOut;
	OnAuthStateChanged.Clear();
	Super::Deinitialize();
}

bool UFF_GoogleAuthSubsystem::IsConfigured() const
{
#if PLATFORM_WINDOWS
	return FF::GoogleAuth::IsDesktopClientId(GetDefault<UFF_GoogleAuthSettings>()->DesktopClientId);
#else
	return false;
#endif
}

bool UFF_GoogleAuthSubsystem::IsBusy() const
{
	return State == EFF_GoogleAuthState::WaitingForBrowser || State == EFF_GoogleAuthState::ExchangingCode;
}

bool UFF_GoogleAuthSubsystem::ConsumeInitialSignInPrompt()
{
	if (bDeinitializing || bInitialSignInPromptConsumed)
	{
		return false;
	}
	bInitialSignInPromptConsumed = true;
	return State != EFF_GoogleAuthState::SignedIn;
}

void UFF_GoogleAuthSubsystem::SetState(const EFF_GoogleAuthState NewState, const FText& Message)
{
	State = NewState;
	StatusMessage = Message;
	if (!bDeinitializing)
	{
		OnAuthStateChanged.Broadcast();
	}
}

void UFF_GoogleAuthSubsystem::SignIn()
{
	if (bDeinitializing || IsBusy())
	{
		return;
	}
	StopPendingWork();
	Account = FFF_GoogleAccount();
	if (!IsConfigured())
	{
		SetState(EFF_GoogleAuthState::Error,
			LOCTEXT("NotConfigured", "Google 로그인 설정이 필요합니다. 오프라인으로 플레이할 수 있습니다."));
		return;
	}

	const UFF_GoogleAuthSettings* Settings = GetDefault<UFF_GoogleAuthSettings>();
	ClientId = Settings->DesktopClientId;
	ClientSecret = Settings->DesktopClientSecret;
	FString Challenge;
	if (!FF::GoogleAuth::CreateRandomToken(ExpectedState) || !FF::GoogleAuth::CreateRandomToken(CodeVerifier)
		|| !FF::GoogleAuth::CreateCodeChallenge(CodeVerifier, Challenge))
	{
		Fail(LOCTEXT("RandomFailed", "안전한 로그인을 시작하지 못했습니다. 다시 시도해 주세요."));
		return;
	}
	if (!OpenLoopbackListener())
	{
		Fail(LOCTEXT("ListenerFailed", "로그인 연결을 준비하지 못했습니다. 다시 시도해 주세요."));
		return;
	}
	Deadline = FPlatformTime::Seconds() + FMath::Clamp(Settings->LoginTimeoutSeconds, 30.f, 600.f);
	const FString Url = FF::GoogleAuth::AuthorizationUrl(ClientId, RedirectUri, ExpectedState, Challenge);
	const uint64 Generation = AttemptGeneration;
	SetState(EFF_GoogleAuthState::WaitingForBrowser,
		LOCTEXT("Waiting", "브라우저에서 Google 로그인을 완료해 주세요."));
	// A UI observer may synchronously cancel the attempt during the state event.
	if (Generation != AttemptGeneration || !IsBusy())
	{
		return;
	}
	FString LaunchError;
	FPlatformProcess::LaunchURL(*Url, nullptr, &LaunchError);
	if (!LaunchError.IsEmpty())
	{
		Fail(LOCTEXT("BrowserFailed", "기본 브라우저를 열지 못했습니다. 브라우저 설정을 확인해 주세요."));
	}
}

void UFF_GoogleAuthSubsystem::CancelSignIn()
{
	if (!IsBusy() && State != EFF_GoogleAuthState::Error)
	{
		return;
	}
	StopPendingWork();
	Account = FFF_GoogleAccount();
	SetState(EFF_GoogleAuthState::SignedOut, LOCTEXT("Cancelled", "Google 로그인을 취소했습니다."));
}

void UFF_GoogleAuthSubsystem::SignOut()
{
	StopPendingWork();
	Account = FFF_GoogleAccount();
	SetState(EFF_GoogleAuthState::SignedOut, LOCTEXT("SignedOutDone", "게임에서 Google 계정을 로그아웃했습니다."));
}

void UFF_GoogleAuthSubsystem::Fail(const FText& Message)
{
	StopPendingWork();
	Account = FFF_GoogleAccount();
	SetState(EFF_GoogleAuthState::Error, Message);
}

bool UFF_GoogleAuthSubsystem::OpenLoopbackListener()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets)
	{
		return false;
	}
	const TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
	bool bValid = false;
	Address->SetIp(TEXT("127.0.0.1"), bValid);
	Address->SetPort(0);
	Listener = Sockets->CreateSocket(NAME_Stream, TEXT("FestaFiesta Google login loopback"), FNetworkProtocolTypes::IPv4);
	if (!bValid || !Listener || !Listener->SetNonBlocking(true) || !Listener->Bind(*Address) || !Listener->Listen(4))
	{
		CloseSocket(Listener);
		return false;
	}
	Listener->GetAddress(*Address);
	if (Address->GetPort() <= 0)
	{
		CloseSocket(Listener);
		return false;
	}
	RedirectUri = FString::Printf(TEXT("http://127.0.0.1:%d/oauth2/callback"), Address->GetPort());
	return true;
}

void UFF_GoogleAuthSubsystem::CloseSocket(FSocket*& Socket)
{
	if (Socket)
	{
		Socket->Close();
		if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			Sockets->DestroySocket(Socket);
		}
		Socket = nullptr;
	}
}

void UFF_GoogleAuthSubsystem::CloseConnection()
{
	CloseSocket(Connection);
	if (RequestBytes.Num() > 0)
	{
		FMemory::Memzero(RequestBytes.GetData(), RequestBytes.Num());
	}
	RequestBytes.Reset();
	ConnectionDeadline = 0.0;
}

void UFF_GoogleAuthSubsystem::StopPendingWork()
{
	// Invalidate first: CancelRequest may invoke a completion synchronously on some backends.
	++AttemptGeneration;
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
	CloseConnection();
	CloseSocket(Listener);
	ClearPrivateString(ExpectedState);
	ClearPrivateString(CodeVerifier);
	ClearPrivateString(ClientSecret);
	ClientId.Reset();
	RedirectUri.Reset();
	Deadline = 0.0;
}

bool UFF_GoogleAuthSubsystem::TickLogin(float DeltaSeconds)
{
	if (bDeinitializing)
	{
		return false;
	}
	if (IsBusy())
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Fail(LOCTEXT("TimedOut", "로그인 시간이 초과되었습니다. 다시 시도해 주세요."));
		}
		else if (State == EFF_GoogleAuthState::WaitingForBrowser)
		{
			PollLoopback();
		}
	}
	return true;
}

void UFF_GoogleAuthSubsystem::ReplyAndClose(const bool bAccepted)
{
	if (Connection)
	{
		const FString Body = bAccepted
			? TEXT("<!doctype html><meta charset=utf-8><title>FestaFiesta</title><h1>Return to FestaFiesta</h1><p>You can close this tab and return to the game.</p>")
			: TEXT("<!doctype html><meta charset=utf-8><title>FestaFiesta</title><p>This login response was not accepted. Return to the game and try again.</p>");
		const FTCHARToUTF8 BodyUtf8(*Body);
		const FString Response = FString::Printf(
			TEXT("HTTP/1.1 %s\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; frame-ancestors 'none'\r\nX-Content-Type-Options: nosniff\r\n\r\n%s"),
			bAccepted ? TEXT("200 OK") : TEXT("400 Bad Request"), BodyUtf8.Length(), *Body);
		const FTCHARToUTF8 Utf8(*Response);
		int32 Sent = 0;
		// Small fixed response; no user-controlled text, tokens, or authorization code is reflected.
		Connection->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent);
	}
	CloseConnection();
}

void UFF_GoogleAuthSubsystem::PollLoopback()
{
	if (!Listener)
	{
		return;
	}
	if (!Connection)
	{
		bool bPending = false;
		if (!Listener->HasPendingConnection(bPending) || !bPending)
		{
			return;
		}
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		const TSharedRef<FInternetAddr> Peer = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
		Connection = Listener->Accept(*Peer, TEXT("FestaFiesta Google login callback"));
		uint32 PeerIp = 0;
		Peer->GetIp(PeerIp);
		if (!Connection || PeerIp != 0x7f000001 || !Connection->SetNonBlocking(true))
		{
			CloseConnection();
			return;
		}
		ConnectionDeadline = FPlatformTime::Seconds() + 2.0;
	}
	if (FPlatformTime::Seconds() >= ConnectionDeadline)
	{
		ReplyAndClose(false);
		return;
	}
	uint32 PendingBytes = 0;
	if (!Connection->HasPendingData(PendingBytes))
	{
		if (Connection->GetConnectionState() == SCS_ConnectionError)
		{
			CloseConnection();
		}
		return;
	}
	if (PendingBytes > static_cast<uint32>(FF::GoogleAuth::MaxCallbackBytes - RequestBytes.Num()))
	{
		ReplyAndClose(false);
		return;
	}
	uint8 Buffer[4096];
	int32 Read = 0;
	if (!Connection->Recv(Buffer, FMath::Min(static_cast<int32>(PendingBytes), static_cast<int32>(UE_ARRAY_COUNT(Buffer))), Read) || Read <= 0)
	{
		CloseConnection();
		return;
	}
	RequestBytes.Append(Buffer, Read);
	FMemory::Memzero(Buffer, sizeof(Buffer));
	// OAuth redirects contain ASCII URI/header bytes. Reject binary input before conversion.
	for (const uint8 Byte : RequestBytes)
	{
		if (Byte == 0 || Byte > 127)
		{
			ReplyAndClose(false);
			return;
		}
	}
	const FUTF8ToTCHAR Utf8(reinterpret_cast<const UTF8CHAR*>(RequestBytes.GetData()), RequestBytes.Num());
	FString Request(Utf8.Length(), Utf8.Get());
	if (!Request.Contains(TEXT("\r\n\r\n")))
	{
		return;
	}
	FString Host = RedirectUri.Mid(7);
	Host = Host.Left(Host.Find(TEXT("/")));
	FString Code;
	const FF::GoogleAuth::ECallbackResult Result = FF::GoogleAuth::ParseCallback(Request, Host, ExpectedState, Code);
	ClearPrivateString(Request);
	ReplyAndClose(Result != FF::GoogleAuth::ECallbackResult::Invalid);
	if (Result == FF::GoogleAuth::ECallbackResult::Invalid)
	{
		return; // A malformed or mismatched callback must not consume the real login attempt.
	}
	CloseSocket(Listener);
	ClearPrivateString(ExpectedState);
	if (Result == FF::GoogleAuth::ECallbackResult::Denied)
	{
		Fail(LOCTEXT("Denied", "Google 로그인이 취소되었거나 허용되지 않았습니다."));
		return;
	}
	ExchangeCode(Code);
	ClearPrivateString(Code);
}

bool UFF_GoogleAuthSubsystem::IsCurrentRequest(
	const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request, const uint64 Generation) const
{
	return !bDeinitializing && Generation == AttemptGeneration && IsBusy() && Request == ActiveRequest;
}

void UFF_GoogleAuthSubsystem::ExchangeCode(const FString& Code)
{
	const uint64 Generation = AttemptGeneration;
	SetState(EFF_GoogleAuthState::ExchangingCode, LOCTEXT("Verifying", "Google 계정을 확인하고 있습니다."));
	if (Generation != AttemptGeneration || !IsBusy())
	{
		return;
	}
	FString Body = TEXT("grant_type=authorization_code&code=") + FF::GoogleAuth::FormValue(Code)
		+ TEXT("&client_id=") + FF::GoogleAuth::FormValue(ClientId)
		+ TEXT("&redirect_uri=") + FF::GoogleAuth::FormValue(RedirectUri)
		+ TEXT("&code_verifier=") + FF::GoogleAuth::FormValue(CodeVerifier);
	if (!ClientSecret.IsEmpty())
	{
		Body += TEXT("&client_secret=") + FF::GoogleAuth::FormValue(ClientSecret);
	}
	ClearPrivateString(CodeVerifier);
	ClearPrivateString(ClientSecret);
	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
	ActiveRequest->SetURL(TEXT("https://oauth2.googleapis.com/token"));
	ActiveRequest->SetVerb(TEXT("POST"));
	ActiveRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	ActiveRequest->SetTimeout(FMath::Clamp(GetDefault<UFF_GoogleAuthSettings>()->HttpTimeoutSeconds, 5.f, 60.f));
	ActiveRequest->SetContentAsString(Body);
	ClearPrivateString(Body);
	ActiveRequest->OnProcessRequestComplete().BindUObject(this, &UFF_GoogleAuthSubsystem::HandleTokenResponse, Generation);
	if (!ActiveRequest->ProcessRequest())
	{
		Fail(LOCTEXT("TokenRequestFailed", "Google에 연결하지 못했습니다. 인터넷 연결을 확인해 주세요."));
	}
}

void UFF_GoogleAuthSubsystem::HandleTokenResponse(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
	TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response, const bool bSucceeded, const uint64 Generation)
{
	if (!IsCurrentRequest(Request, Generation))
	{
		return;
	}
	ActiveRequest.Reset();
	TSharedPtr<FJsonObject> Json;
	FString AccessToken, TokenType;
	double ExpiresIn = 0;
	if (!ReadGoogleResponse(Response, bSucceeded, Json)
		|| !Json->TryGetStringField(TEXT("access_token"), AccessToken) || AccessToken.IsEmpty() || AccessToken.Len() > 8192
		|| !Json->TryGetStringField(TEXT("token_type"), TokenType) || !TokenType.Equals(TEXT("Bearer"), ESearchCase::IgnoreCase)
		|| !Json->TryGetNumberField(TEXT("expires_in"), ExpiresIn) || ExpiresIn <= 0)
	{
		Fail(LOCTEXT("TokenFailed", "Google 로그인 확인에 실패했습니다. 다시 시도해 주세요."));
		return;
	}
	// The ID token is deliberately not decoded/trusted. Identity comes from authenticated HTTPS userinfo.
	RequestProfile(AccessToken);
	ClearPrivateString(AccessToken);
}

void UFF_GoogleAuthSubsystem::RequestProfile(const FString& AccessToken)
{
	const uint64 Generation = AttemptGeneration;
	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
	ActiveRequest->SetURL(TEXT("https://openidconnect.googleapis.com/v1/userinfo"));
	ActiveRequest->SetVerb(TEXT("GET"));
	ActiveRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);
	ActiveRequest->SetTimeout(FMath::Clamp(GetDefault<UFF_GoogleAuthSettings>()->HttpTimeoutSeconds, 5.f, 60.f));
	ActiveRequest->OnProcessRequestComplete().BindUObject(this, &UFF_GoogleAuthSubsystem::HandleProfileResponse, Generation);
	if (!ActiveRequest->ProcessRequest())
	{
		Fail(LOCTEXT("ProfileRequestFailed", "Google 프로필을 가져오지 못했습니다. 다시 시도해 주세요."));
	}
}

void UFF_GoogleAuthSubsystem::HandleProfileResponse(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
	TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response, const bool bSucceeded, const uint64 Generation)
{
	if (!IsCurrentRequest(Request, Generation))
	{
		return;
	}
	ActiveRequest.Reset();
	TSharedPtr<FJsonObject> Json;
	FFF_GoogleAccount Profile;
	bool bEmailVerified = false;
	if (!ReadGoogleResponse(Response, bSucceeded, Json)
		|| !Json->TryGetStringField(TEXT("sub"), Profile.Subject) || Profile.Subject.IsEmpty() || Profile.Subject.Len() > 255
		|| !Json->TryGetStringField(TEXT("email"), Profile.Email) || Profile.Email.IsEmpty() || Profile.Email.Len() > 320
		|| !Json->TryGetBoolField(TEXT("email_verified"), bEmailVerified) || !bEmailVerified)
	{
		Fail(LOCTEXT("ProfileFailed", "확인된 Google 계정 정보를 가져오지 못했습니다. 다시 시도해 주세요."));
		return;
	}
	Json->TryGetStringField(TEXT("name"), Profile.DisplayName);
	if (Profile.DisplayName.IsEmpty())
	{
		Profile.DisplayName = Profile.Email;
	}
	Profile.DisplayName = Profile.DisplayName.Left(128);
	StopPendingWork();
	Account = MoveTemp(Profile);
	SetState(EFF_GoogleAuthState::SignedIn, LOCTEXT("Complete", "Google 계정으로 로그인했습니다."));
}

#undef LOCTEXT_NAMESPACE
