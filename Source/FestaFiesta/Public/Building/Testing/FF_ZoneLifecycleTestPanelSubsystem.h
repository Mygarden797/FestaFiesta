// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ZoneLifecycleTestPanelSubsystem.generated.h"

class UFF_ZoneLifecycleTestPanel;

/** Test_Main에서만 개발용 시설 Lifecycle WBP를 자동 표시합니다. */
UCLASS()
class FESTAFIESTA_API UFF_ZoneLifecycleTestPanelSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	bool TryCreatePanel();
	bool IsTestMainWorld() const;

	UPROPERTY(Transient)
	TObjectPtr<UFF_ZoneLifecycleTestPanel> PanelWidget;

	bool bPanelCreationPending = false;
	bool bPanelLoadFailureLogged = false;
};

