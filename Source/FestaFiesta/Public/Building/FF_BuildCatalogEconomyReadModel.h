// FF_BuildCatalogEconomyReadModel.h

// Festival 진행 재화를 건설 카탈로그의 비용 표시용 스냅샷으로 투영하는 읽기 모델을 선언한다.
// 흐름: 진행 서브시스템 의존성 초기화 -> 비용 투영 요청 처리 -> 골드 변경 이벤트 중계.
// 불변식: 원본 카탈로그 캐시는 변경하지 않고 권위 골드는 항상 0 이상의 값으로 노출한다.
// 실패/수명: 진행 서브시스템이 없으면 미확정 상태를 유지하고 Deinitialize에서 구독을 해제한다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_BuildCatalogEconomyReadModel.generated.h"

class UFF_FestivalProgressionSubsystem;
class UFFZSFacilityDefinition;
struct FFF_BuildCatalogEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFF_OnBuildCatalogGoldChanged,
	int32,
	NewGold);

/**
 * 건축 카탈로그가 사용하는 경제 읽기 모델입니다.
 *
 * 팀 공용 UI ViewModel을 변경하지 않으면서도 위젯이 경제 소유자에게 직접 결합되지 않도록,
 * Progression의 실제 Gold를 같은 GameInstance 안에서 구독해 읽기 전용으로 중계합니다.
 */
UCLASS()
class FESTAFIESTA_API UFF_BuildCatalogEconomyReadModel : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Economy")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Economy")
	bool HasAuthoritativeGold() const { return bHasAuthoritativeGold; }

	/** Projects the active festival's authoritative facility pricing into a catalog snapshot. */
	bool TryProjectFacilityCosts(
		FFF_BuildCatalogEntry& Entry,
		const UFFZSFacilityDefinition* Definition,
		FString& OutFailureReason) const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Building|Economy")
	FFF_OnBuildCatalogGoldChanged OnGoldChanged;

private:
	UFUNCTION()
	void HandleProgressionGoldChanged(int32 NewGold);

	UPROPERTY()
	TObjectPtr<UFF_FestivalProgressionSubsystem> ProgressionSubsystem;

	int32 Gold = 0;
	bool bHasAuthoritativeGold = false;
};
