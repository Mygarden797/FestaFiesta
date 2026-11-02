#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_GoogleAuthSubsystem.generated.h"

class FSocket;
class IHttpRequest;
class IHttpResponse;

UENUM(BlueprintType)
enum class EFF_GoogleAuthState : uint8
{
	SignedOut,
	WaitingForBrowser,
	ExchangingCode,
	SignedIn,
	Error
};

/** Verified Google profile for this running game only; never serialized into saves. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_GoogleAccount
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Google Login")
	FString Subject;

	UPROPERTY(BlueprintReadOnly, Category = "Google Login")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Google Login")
	FString Email;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnGoogleAuthStateChanged);

/** Optional local Google profile session. Offline play and existing local saves remain available. */
UCLASS()
class FESTAFIESTA_API UFF_GoogleAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Google Login")
	bool IsConfigured() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Google Login")
	bool IsBusy() const;

	/** Show the optional title sign-in prompt once per running game, including recreated title screens. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Google Login")
	bool ConsumeInitialSignInPrompt();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Google Login")
	EFF_GoogleAuthState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Google Login")
	FFF_GoogleAccount GetAccount() const { return Account; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Google Login")
	FText GetStatusMessage() const { return StatusMessage; }

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Google Login")
	void SignIn();

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Google Login")
	void CancelSignIn();

	/** Clears this game's session. Does not sign the browser out of Google or alter local saves. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Google Login")
	void SignOut();

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Google Login")
	FFF_OnGoogleAuthStateChanged OnAuthStateChanged;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_GoogleAuthAttemptLifecycleTest;
	friend class FFF_GoogleAccountSurfacesTest;
#endif

	bool TickLogin(float DeltaSeconds);
	bool OpenLoopbackListener();
	void PollLoopback();
	void CloseSocket(FSocket*& Socket);
	void CloseConnection();
	void ReplyAndClose(bool bAccepted);
	void StopPendingWork();
	void SetState(EFF_GoogleAuthState NewState, const FText& Message);
	void Fail(const FText& Message);
	void ExchangeCode(const FString& Code);
	void RequestProfile(const FString& AccessToken);
	bool IsCurrentRequest(const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request, uint64 Generation) const;
	void HandleTokenResponse(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
		TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response, bool bSucceeded, uint64 Generation);
	void HandleProfileResponse(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
		TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response, bool bSucceeded, uint64 Generation);

	UPROPERTY(Transient)
	EFF_GoogleAuthState State = EFF_GoogleAuthState::SignedOut;

	UPROPERTY(Transient)
	FFF_GoogleAccount Account;

	UPROPERTY(Transient)
	FText StatusMessage;

	FTSTicker::FDelegateHandle TickerHandle;
	FSocket* Listener = nullptr;
	FSocket* Connection = nullptr;
	TArray<uint8> RequestBytes;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	FString ExpectedState;
	FString CodeVerifier;
	FString RedirectUri;
	FString ClientId;
	FString ClientSecret;
	uint64 AttemptGeneration = 0;
	double Deadline = 0.0;
	double ConnectionDeadline = 0.0;
	bool bDeinitializing = false;
	bool bInitialSignInPromptConsumed = false;
};
