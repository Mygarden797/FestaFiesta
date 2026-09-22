// FF_PrepBuildInputBridgeSubsystem.h
// 역할: 건설 상태 변화와 준비 HUD의 건설 탭 표시를 연결한다.

// 기존 BuildMode 상태 이벤트와 PrepHUD 배치 탭을 연결하는 독립 표현 브리지.
// 팀 BuildMode/PrepHUD 구현은 수정하지 않고 공개 API만 사용한다.

#pragma once

#include "CoreMinimal.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_PrepBuildInputBridgeSubsystem.generated.h"

class UFF_PrepHudBar;

namespace FFPrepBuildInputBridgeModel
{
	enum class ERouteAction : uint8
	{
		None,
		OpenBuildTab,
		ReopenBuildTab
	};

	inline ERouteAction ResolveCatalogRoute(
		const EFF_BuildModeState NewState,
		const EFF_BuildModeState CurrentState,
		const bool bRouteInFlight,
		const bool bExternalPresentationActive,
		const bool bHasSingleActiveHud,
		const bool bBuildTabSelected)
	{
		if (NewState != EFF_BuildModeState::CatalogOpen ||
			CurrentState != EFF_BuildModeState::CatalogOpen ||
			bRouteInFlight ||
			bExternalPresentationActive ||
			!bHasSingleActiveHud)
		{
			return ERouteAction::None;
		}

		return bBuildTabSelected
			? ERouteAction::ReopenBuildTab
			: ERouteAction::OpenBuildTab;
	}

	inline bool IsPresentationReady(
		const EFF_BuildModeState State,
		const bool bBuildTabSelected,
		const bool bExternalPresentationActive)
	{
		return State == EFF_BuildModeState::CatalogOpen &&
			bBuildTabSelected &&
			bExternalPresentationActive;
	}
}

/**
 * BuildMode가 CatalogOpen으로 전환되는 동기 이벤트에서 현재 PrepHUD의 Build 탭을 연다.
 *
 * 기존 OpenCatalog는 상태 이벤트를 방송한 뒤 레거시 카탈로그 생성 여부를 검사한다.
 * 이 이벤트 안에서 기존 PrepBuild 패널이 external presentation을 활성화하므로 레거시 화면은
 * 생성되지 않는다. 활성 PrepHUD가 하나가 아니거나 패널 열기가 실패하면 아무것도 소유하지 않아
 * 기존 카탈로그가 그대로 폴백한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_PrepBuildInputBridgeSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandleBuildModeStateChanged(
		EFF_BuildModeState PreviousState,
		EFF_BuildModeState NewState);

	UFF_PrepHudBar* ResolveSingleActivePrepHudBar() const;
	bool IsUsablePrepHudBar(const UFF_PrepHudBar* Candidate) const;

	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildModeSubsystem> BuildModeSubsystem;

	bool bRouteInFlight = false;
};
