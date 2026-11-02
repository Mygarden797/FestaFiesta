#pragma once

#include "CoreMinimal.h"

namespace FF::GoogleAuth
{
	constexpr int32 MaxCallbackBytes = 16384;

	enum class ECallbackResult : uint8 { Invalid, Code, Denied };

	bool CreateRandomToken(FString& OutToken);
	bool CreateCodeChallenge(const FString& Verifier, FString& OutChallenge);
	bool IsDesktopClientId(const FString& ClientId);
	FString FormValue(const FString& Value);
	FString AuthorizationUrl(const FString& ClientId, const FString& RedirectUri,
		const FString& State, const FString& Challenge);
	ECallbackResult ParseCallback(const FString& Request, const FString& ExpectedHost,
		const FString& ExpectedState, FString& OutCode);
}
