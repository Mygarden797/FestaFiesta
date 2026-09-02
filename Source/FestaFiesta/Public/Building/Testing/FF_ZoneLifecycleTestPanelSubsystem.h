// FF_ZoneLifecycleTestPanelSubsystem.h

// Test_Main 월드에서 시설 Lifecycle 개발 패널의 생성·Ctrl+G 표시 토글을 소유한다.
// 흐름: 대상 월드 확인 -> WBP를 숨김 상태로 부착 -> Ctrl+G raw chord 감지 -> 표시·상태 갱신.
// 불변식: 일반 맵에서는 생성하지 않으며 패키징에서도 입력 모드·마우스 커서 상태를 변경하지 않는다.
// 실패/수명: 플레이어 준비 전만 생성을 재시도하고 해제 시 위젯과 토글 상태를 함께 정리한다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ZoneLifecycleTestPanelSubsystem.generated.h"

class UFF_ZoneLifecycleTestPanel;

/** Editor와 패키징의 Test_Main에서 개발용 시설 Lifecycle WBP를 만들고 Ctrl+G로 표시합니다. */
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

	/** 표시 중인 디버그 카드 위에 포인터가 있어 월드 클릭을 차단해야 하는지 반환합니다. */
	bool IsPointerOverPanel() const;

private:
	bool TryCreatePanel();
	bool IsTestMainWorld() const;
	void HandleToggleInput();
	void TogglePanelVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UFF_ZoneLifecycleTestPanel> PanelWidget;

	bool bPanelCreationPending = false;
	bool bPanelLoadFailureLogged = false;
};

