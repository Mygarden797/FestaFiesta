#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_GoogleAuthSettings.generated.h"

/** Google Auth Platform Desktop OAuth credentials. This flow does not use paid Cloud services. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Google Login"))
class FESTAFIESTA_API UFF_GoogleAuthSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** A Desktop app client ID, never a Web application client ID or API key. */
	UPROPERTY(EditAnywhere, Config, Category = "Google Login")
	FString DesktopClientId;

	/** Optional compatibility value from Desktop client JSON. Native apps cannot keep this confidential. */
	UPROPERTY(EditAnywhere, Config, Category = "Google Login", meta = (PasswordField = true))
	FString DesktopClientSecret;

	UPROPERTY(EditAnywhere, Config, Category = "Google Login", meta = (ClampMin = "30.0", ClampMax = "600.0", Units = "s"))
	float LoginTimeoutSeconds = 180.f;

	UPROPERTY(EditAnywhere, Config, Category = "Google Login", meta = (ClampMin = "5.0", ClampMax = "60.0", Units = "s"))
	float HttpTimeoutSeconds = 20.f;
};
