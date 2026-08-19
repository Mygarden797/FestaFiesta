// FF_ZoneLifecycleTestPanelSubsystem.h

// Test_Main 월드에서만 시설 Lifecycle 개발 패널을 한 번 생성하는 월드 서브시스템을 선언한다.
// 흐름: 대상 월드 확인 -> 로컬 플레이어 준비 대기 -> WBP 클래스 로드·Viewport 부착.
// 불변식: 일반 맵과 Shipping 빌드에서는 패널을 생성하지 않고 성공 뒤 생성 재시도를 멈춘다.
// 실패/수명: 플레이어 준비 전에는 Tick에서 재시도하고 클래스 로드 실패는 한 번만 기록한 뒤 중단한다.

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

