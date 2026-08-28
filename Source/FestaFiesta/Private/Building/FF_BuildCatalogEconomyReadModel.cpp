// FF_BuildCatalogEconomyReadModel.cpp

// Festival 진행 서브시스템의 비용표와 골드를 카탈로그 UI가 읽는 스냅샷으로 투영한다.
// 흐름: 의존성 초기화·이벤트 구독 -> 요청 시 비용 계산 -> 골드 변경 이벤트 중계.
// 불변식: 골드는 0 이상으로 정규화하고 같은 값에는 중복 변경 이벤트를 방송하지 않는다.
// 실패/수명: 진행 서브시스템이 없으면 미확정 상태를 유지하고 해제 시 구독을 반드시 끊는다.

#include "Building/FF_BuildCatalogEconomyReadModel.h"

#include "Building/FF_BuildCatalogModel.h"
#include "Engine/GameInstance.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "UI/FF_UILog.h"

void UFF_BuildCatalogEconomyReadModel::Initialize(FSubsystemCollectionBase& Collection)
{
	// 비용과 잔액의 권위 소유자를 먼저 초기화해 UI 읽기 모델이 부분 상태를 방송하지 않게 한다.
	Collection.InitializeDependency<UFF_FestivalProgressionSubsystem>();
	Super::Initialize(Collection);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		ProgressionSubsystem =
			GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	}

	if (!ProgressionSubsystem)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[BuildCatalogEconomy] FestivalProgressionSubsystem을 찾지 못했습니다."));
		return;
	}

	ProgressionSubsystem->OnGoldChanged.AddUniqueDynamic(
		this, &ThisClass::HandleProgressionGoldChanged);
	// Progression은 이 읽기 모델보다 먼저 초기 Festival을 시작하므로 과거 이벤트에 의존하지 않고
	// 구독 직후 현재 값을 함께 동기화합니다.
	HandleProgressionGoldChanged(ProgressionSubsystem->GetGold());
}

void UFF_BuildCatalogEconomyReadModel::Deinitialize()
{
	if (ProgressionSubsystem)
	{
		ProgressionSubsystem->OnGoldChanged.RemoveDynamic(
			this, &ThisClass::HandleProgressionGoldChanged);
	}
	ProgressionSubsystem = nullptr;
	bHasAuthoritativeGold = false;
	Super::Deinitialize();
}

bool UFF_BuildCatalogEconomyReadModel::TryProjectFacilityCosts(
	FFF_BuildCatalogEntry& Entry,
	const UFFZSFacilityDefinition* Definition,
	FString& OutFailureReason) const
{
	// Festival이 시작되지 않았거나 비용표가 아직 메모리에 없으면 null 입력을 그대로 순수 모델에
	// 전달한다. 모델이 공통 실패 이유를 만들게 해 화면마다 다른 폴백 규칙이 생기지 않게 한다.
	const UFF_FestivalDefinition* Festival =
		ProgressionSubsystem && ProgressionSubsystem->IsFestivalStarted()
		? ProgressionSubsystem->GetDefinition()
		: nullptr;
	const UDataTable* CostTable = Festival ? Festival->FacilityCostTable.Get() : nullptr;
	const float DemolitionRefundRate = Festival
		? Festival->FacilityDemolitionRefundRate
		: 0.0f;

	return FFBuildCatalogModel::ApplyFacilityCosts(
		Entry,
		Definition,
		CostTable,
		DemolitionRefundRate,
		OutFailureReason);
}

void UFF_BuildCatalogEconomyReadModel::HandleProgressionGoldChanged(const int32 NewGold)
{
	// 외부 데이터 오류로 음수가 들어와도 표시와 구매 가능 판정이 서로 다른 잔액을 보지 않게 한다.
	const int32 SanitizedGold = FMath::Max(0, NewGold);
	const bool bValueChanged = !bHasAuthoritativeGold || Gold != SanitizedGold;
	Gold = SanitizedGold;
	bHasAuthoritativeGold = true;

	if (bValueChanged)
	{
		OnGoldChanged.Broadcast(Gold);
	}
}
