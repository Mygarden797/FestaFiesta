#include "Authentication/FF_GoogleAuthProtocol.h"

#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/Base64.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace FF::GoogleAuth
{
	static FString Base64Url(const uint8* Bytes, const uint32 Count)
	{
		FString Result = FBase64::Encode(Bytes, Count, EBase64Mode::UrlSafe);
		while (Result.EndsWith(TEXT("=")))
		{
			Result.LeftChopInline(1);
		}
		return Result;
	}

	bool CreateRandomToken(FString& OutToken)
	{
		OutToken.Reset();
#if PLATFORM_WINDOWS
		uint8 Bytes[32];
		if (BCryptGenRandom(nullptr, Bytes, sizeof(Bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
		{
			return false;
		}
		OutToken = Base64Url(Bytes, sizeof(Bytes));
		FMemory::Memzero(Bytes, sizeof(Bytes));
		return true;
#else
		return false;
#endif
	}

	bool CreateCodeChallenge(const FString& Verifier, FString& OutChallenge)
	{
		OutChallenge.Reset();
#if PLATFORM_WINDOWS
		BCRYPT_ALG_HANDLE Algorithm = nullptr;
		if (BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
		{
			return false;
		}
		ULONG ObjectSize = 0;
		ULONG ResultSize = 0;
		bool bSuccess = BCryptGetProperty(Algorithm, BCRYPT_OBJECT_LENGTH,
			reinterpret_cast<PUCHAR>(&ObjectSize), sizeof(ObjectSize), &ResultSize, 0) >= 0;
		TArray<uint8> HashObject;
		HashObject.SetNumZeroed(ObjectSize);
		BCRYPT_HASH_HANDLE Hash = nullptr;
		bSuccess = bSuccess && BCryptCreateHash(Algorithm, &Hash, HashObject.GetData(),
			ObjectSize, nullptr, 0, 0) >= 0;
		const FTCHARToUTF8 Utf8(*Verifier);
		uint8 Digest[32] = {};
		bSuccess = bSuccess && BCryptHashData(Hash,
			const_cast<uint8*>(reinterpret_cast<const uint8*>(Utf8.Get())), Utf8.Length(), 0) >= 0;
		bSuccess = bSuccess && BCryptFinishHash(Hash, Digest, sizeof(Digest), 0) >= 0;
		if (bSuccess)
		{
			OutChallenge = Base64Url(Digest, sizeof(Digest));
		}
		if (Hash)
		{
			BCryptDestroyHash(Hash);
		}
		BCryptCloseAlgorithmProvider(Algorithm, 0);
		FMemory::Memzero(Digest, sizeof(Digest));
		return bSuccess;
#else
		return false;
#endif
	}

	bool IsDesktopClientId(const FString& ClientId)
	{
		if (ClientId.Len() > 256 || !ClientId.EndsWith(TEXT(".apps.googleusercontent.com")))
		{
			return false;
		}
		FString Prefix = ClientId.LeftChop(27);
		if (Prefix.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : ClientId)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('.'))
			{
				return false;
			}
		}
		return true;
	}

	FString FormValue(const FString& Value)
	{
		return FGenericPlatformHttp::UrlEncode(Value);
	}

	FString AuthorizationUrl(const FString& ClientId, const FString& RedirectUri,
		const FString& State, const FString& Challenge)
	{
		return TEXT("https://accounts.google.com/o/oauth2/v2/auth?response_type=code&client_id=")
			+ FormValue(ClientId) + TEXT("&redirect_uri=") + FormValue(RedirectUri)
			+ TEXT("&scope=openid%20email%20profile&state=") + FormValue(State)
			+ TEXT("&code_challenge=") + FormValue(Challenge)
			+ TEXT("&code_challenge_method=S256&access_type=online&prompt=select_account");
	}

	static bool DecodeQueryValue(FString Value, FString& Out)
	{
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			if (Value[Index] == TEXT('%'))
			{
				if (Index + 2 >= Value.Len() || !FChar::IsHexDigit(Value[Index + 1])
					|| !FChar::IsHexDigit(Value[Index + 2]))
				{
					return false;
				}
				Index += 2;
			}
		}
		Value.ReplaceInline(TEXT("+"), TEXT(" "));
		Out = FGenericPlatformHttp::UrlDecode(Value);
		for (const TCHAR Character : Out)
		{
			if (Character < 32 || Character == 127)
			{
				return false;
			}
		}
		return true;
	}

	ECallbackResult ParseCallback(const FString& Request, const FString& ExpectedHost,
		const FString& ExpectedState, FString& OutCode)
	{
		OutCode.Reset();
		if (Request.Len() > MaxCallbackBytes || ExpectedState.IsEmpty() || !Request.EndsWith(TEXT("\r\n\r\n")))
		{
			return ECallbackResult::Invalid;
		}
		TArray<FString> Lines;
		Request.ParseIntoArray(Lines, TEXT("\r\n"), false);
		TArray<FString> FirstLine;
		Lines[0].ParseIntoArray(FirstLine, TEXT(" "), false);
		if (FirstLine.Num() != 3 || FirstLine[0] != TEXT("GET")
			|| (FirstLine[2] != TEXT("HTTP/1.1") && FirstLine[2] != TEXT("HTTP/1.0")))
		{
			return ECallbackResult::Invalid;
		}
		int32 HostCount = 0;
		for (int32 Index = 1; Index < Lines.Num(); ++Index)
		{
			FString HeaderName, HeaderValue;
			if (Lines[Index].Split(TEXT(":"), &HeaderName, &HeaderValue)
				&& HeaderName.Equals(TEXT("Host"), ESearchCase::IgnoreCase))
			{
				++HostCount;
				if (HeaderValue.TrimStartAndEnd() != ExpectedHost)
				{
					return ECallbackResult::Invalid;
				}
			}
		}
		FString Path, Query;
		if (HostCount != 1 || !FirstLine[1].Split(TEXT("?"), &Path, &Query)
			|| Path != TEXT("/oauth2/callback") || Query.Contains(TEXT("#")))
		{
			return ECallbackResult::Invalid;
		}
		TMap<FString, FString> Values;
		TArray<FString> Parts;
		Query.ParseIntoArray(Parts, TEXT("&"), false);
		for (const FString& Part : Parts)
		{
			FString Key, EncodedValue, Value;
			if (!Part.Split(TEXT("="), &Key, &EncodedValue) || !DecodeQueryValue(Key, Key)
				|| !DecodeQueryValue(EncodedValue, Value) || Values.Contains(Key))
			{
				return ECallbackResult::Invalid;
			}
			Values.Add(Key, Value);
		}
		const FString* State = Values.Find(TEXT("state"));
		const FString* Code = Values.Find(TEXT("code"));
		const FString* Error = Values.Find(TEXT("error"));
		if (!State || *State != ExpectedState || (Code && Error))
		{
			return ECallbackResult::Invalid;
		}
		if (Error && !Error->IsEmpty())
		{
			return ECallbackResult::Denied;
		}
		if (!Code || Code->IsEmpty() || Code->Len() > 4096)
		{
			return ECallbackResult::Invalid;
		}
		OutCode = *Code;
		return ECallbackResult::Code;
	}
}
