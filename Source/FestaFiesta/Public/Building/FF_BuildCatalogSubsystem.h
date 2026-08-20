// FF_BuildCatalogSubsystem.h

// FacilityDefinition을 비동기로 수집해 UI용 카탈로그와 썸네일 로드 상태를 관리한다.
// 흐름: 기본 에셋 검색 -> 정의 로드/검증 -> 정렬·그룹화 -> 세션 기반 썸네일 프리로드.
// 불변식: 요청 세대로 오래된 콜백을 무시하고 동일 정의 ID는 카탈로그에 한 번만 남긴다.
// 실패/수명: 로드 실패는 종결 상태로 알리며 세션이 끝나면 핸들과 썸네일 강한 참조를 해제한다.

#pragma once

#include "CoreMinimal.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_BuildCatalogSubsystem.generated.h"

struct FStreamableHandle;
class UFFZSFacilityDefinition;

// 카탈로그 새로고침이 Ready 또는 Failed의 종결 상태에 도달했음을 알리는 이벤트입니다.
// 인자를 싣지 않으므로 구독자는 알림을 받은 뒤 서브시스템의 상태와 읽기 모델을 다시 조회합니다.

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnBuildCatalogChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnBuildCatalogThumbnailsChanged);

// FFZS Facility Definition Primary Asset들을 건설 UI용 읽기 모델로 변환하는 GameInstance 서브시스템입니다.

// 에셋 검색, 비동기 로드, ID 이름 규칙 파싱, 유효성 검증과 결정적 정렬을 한곳에서 담당합니다.
// 따라서 건설 위젯은 Asset Registry를 직접 검색하거나 식별자 문자열을 해석할 필요가 없고,
// 권위 있는 시설 Data Asset의 수명을 위젯이 직접 소유하지 않아도 됩니다.

// 평면 Entries와 메뉴에 바로 사용할 수 있는 CategoryGroups는 같은 유효 항목의 서로 다른
// 표현이며, 새로고침이 끝날 때 함께 교체됩니다.

UCLASS()
class FESTAFIESTA_API UFF_BuildCatalogSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// GameInstance 초기화 시 즉시 첫 비동기 카탈로그 새로고침을 시작합니다. 
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 진행 중 요청을 무효화·취소하고 런타임 캐시를 비운 뒤 부모 서브시스템을 종료합니다. 
	virtual void Deinitialize() override;

	// 등록된 모든 FFZSFacilityDefinition Primary Asset을 다시 열거하고 비동기로 로드합니다.
	// 기존 진행 요청은 취소·무효화되며, 현재 결과/오류/스킵 수는 먼저 초기화됩니다.
	// 호출 직후 상태는 Loading이고, 완료 또는 실패 시 OnCatalogChanged가 한 번 방송됩니다.
	 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Catalog")
	void RefreshCatalog();

	// 카탈로그 썸네일 세션 참조를 하나 획득합니다. 첫 참조에서만 BuildCatalogUI 번들을 비동기로 프리로드합니다.
	// 시설 정의 목록이 아직 로딩 중이면 의도만 기록하고 목록이 Ready가 된 직후 자동으로 시작합니다.
	void AcquireThumbnailPreload();

	// 카탈로그 썸네일 세션 참조를 하나 반환합니다. 마지막 참조가 사라질 때 프리로드 핸들을 해제합니다.
	void ReleaseThumbnailPreload();

	// `대분류 enum > 소분류 토큰 > 표시 이름 > 시설 정의 ID` 순으로 정렬된 평면 스냅샷입니다.
	// Blueprint 호출자에게 배열 복사본을 반환하므로 내부 캐시는 수정되지 않습니다.
	 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	TArray<FFF_BuildCatalogEntry> GetEntries() const { return Entries; }
	// 네이티브 UI 소유자가 즉시 복사할 때 중간 배열 복사본을 만들지 않는 읽기 전용 뷰다.
	const TArray<FFF_BuildCatalogEntry>& GetEntriesView() const { return Entries; }

	// 같은 항목들을 `대분류 > 소분류 > 항목`으로 묶어 메뉴에서 바로 순회할 수 있게 한 스냅샷입니다. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	TArray<FFF_BuildCatalogCategoryGroup> GetCategoryGroups() const { return CategoryGroups; }

	// 현재 요청의 진행 상태를 반환합니다. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	EFF_BuildCatalogLoadState GetLoadState() const { return LoadState; }

	// 빈 카탈로그를 포함해 마지막 새로고침이 정상 완료되었는지 반환합니다. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	bool IsCatalogReady() const { return LoadState == EFF_BuildCatalogLoadState::Ready; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	EFF_BuildCatalogThumbnailLoadState GetThumbnailLoadState() const { return ThumbnailLoadState; }

	// 마지막 전체 요청 실패 사유입니다. 정상 상태와 새 요청 시작 시에는 빈 텍스트입니다. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	FText GetLastError() const { return LastError; }

	// 마지막 새로고침에서 유효하지 않음, ID 중복 또는 로드 실패로 제외된 정의의 수입니다. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	int32 GetSkippedAssetCount() const { return SkippedAssetCount; }

	// 도메인 FacilityDefinitionId로 카탈로그 항목을 조회합니다.
	// OutEntry: 성공하면 내부 항목의 복사본, 실패하면 기본값으로 초기화된 항목
	// ID가 존재하고 내부 인덱스가 유효하면 true
	 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	bool FindEntry(FName FacilityDefinitionId, FFF_BuildCatalogEntry& OutEntry) const;

	// ID에 해당하는 이미 로드된 시설 정의 Data Asset을 반환합니다.
	// 찾을 수 없거나 객체가 현재 로드되어 있지 않으면 null이며, 이 함수는 동기 로드를 수행하지 않습니다.
	 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	UFFZSFacilityDefinition* FindDefinition(FName FacilityDefinitionId) const;
	
	// 선택한 정의의 Spec을 배치 명령 생성에 사용할 값으로 복사합니다.
	// 실패 시 OutSpec은 기본값으로 초기화되며, 원본 Data Asset은 변경하지 않습니다.
	 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Catalog")
	bool TryGetDefinitionSpec(FName FacilityDefinitionId, FFFZSFacilitySpec& OutSpec) const;

	
	// 새로고침이 Ready 또는 Failed에 도달한 뒤 방송됩니다.
	// 이벤트는 과거 완료를 재생하지 않으므로 늦게 구독한 UI는 즉시 LoadState도 함께 읽어야 합니다.
	 
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Building|Catalog")
	FFF_OnBuildCatalogChanged OnCatalogChanged;

	// 썸네일 프리로드가 Ready/Failed/NotRequested로 바뀔 때 UI 갱신 기회를 알립니다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Building|Catalog")
	FFF_OnBuildCatalogThumbnailsChanged OnCatalogThumbnailsChanged;

private:
	// 로드 완료 콜백입니다. Generation이 최신 요청과 일치할 때만 결과를 재구성합니다. 
	void HandleCatalogLoadCompleted(TSharedPtr<FStreamableHandle> CompletedHandle, uint64 Generation);

	// 로드 취소 콜백입니다. 현재 요청의 취소에만 Failed 상태를 반영합니다. 
	void HandleCatalogLoadCanceled(TSharedPtr<FStreamableHandle> CanceledHandle, uint64 Generation);

	// 활성 썸네일 세션이 있고 시설 목록이 준비됐을 때 아직 시작하지 않은 프리로드를 시작합니다.
	void StartThumbnailPreloadIfReady();
	void HandleThumbnailPreloadCompleted(TSharedPtr<FStreamableHandle> CompletedHandle, uint64 Generation);
	void HandleThumbnailPreloadCanceled(TSharedPtr<FStreamableHandle> CanceledHandle, uint64 Generation);
	// 진행 중/완료 핸들을 해제하고 콜백 세대를 무효화합니다. 활성 세션 참조는 호출자가 결정합니다.
	void ResetThumbnailPreloadRequest(bool bKeepSessionIntent);
	void SetThumbnailLoadState(EFF_BuildCatalogThumbnailLoadState NewState);

	// 로드된 Primary Asset을 검증·변환·중복 제거한 뒤 평면 배열과 조회 인덱스를 다시 만듭니다. 
	void RebuildCatalogFromLoadedAssets();

	// 정렬된 평면 Entries를 UI 계층형 CategoryGroups로 다시 투영합니다. 
	void RebuildCategoryGroups();

	// 종결 상태와 오류를 저장하고 OnCatalogChanged를 방송합니다. 
	void FinishRefresh(EFF_BuildCatalogLoadState NewState, const FText& Error = FText::GetEmpty());
	
	// 시설 정의 하나를 검증하고 이름 규칙을 파싱하여 카탈로그 항목으로 변환합니다.
	// 성공 시 OutEntry를 채우고 true를 반환합니다. 실패 시 OutEntry는 기본값을 유지하며
	//  OutFailureReason에 로그용 상세 사유를 기록합니다. Definition 자체는 변경하지 않습니다.
	 
	bool TryMakeEntry(
		const FPrimaryAssetId& AssetId,
		UFFZSFacilityDefinition& Definition,
		FFF_BuildCatalogEntry& OutEntry,
		FString& OutFailureReason) const;

	// 마지막으로 정상 구성한 유효 항목의 결정적 평면 순서입니다. 저장되지 않는 런타임 캐시입니다. 
	UPROPERTY(Transient)
	TArray<FFF_BuildCatalogEntry> Entries;

	// Entries를 메뉴 계층으로 복사한 런타임 캐시입니다. 
	UPROPERTY(Transient)
	TArray<FFF_BuildCatalogCategoryGroup> CategoryGroups;

	// 현재 비동기 요청의 수명 주기 상태입니다. 
	UPROPERTY(Transient)
	EFF_BuildCatalogLoadState LoadState = EFF_BuildCatalogLoadState::NotRequested;

	// 전체 요청이 실패했을 때 UI에 제공할 오류이며, 부분 스킵 사유는 로그로만 남습니다. 
	UPROPERTY(Transient)
	FText LastError;

	// 현재 새로고침에서 열거한 Primary Asset ID들입니다. 재현 가능한 처리를 위해 문자열 순으로 정렬됩니다. 
	TArray<FPrimaryAssetId> RequestedAssetIds;

	// FacilityDefinitionId에서 정렬 완료된 Entries 배열 인덱스로 가는 O(1) 조회 테이블입니다. 
	TMap<FName, int32> EntryIndexByDefinitionId;

	// 현재 비동기 로드를 취소하고 진행 상태를 관찰하기 위한 핸들입니다. 
	TSharedPtr<FStreamableHandle> ActiveLoadHandle;

	// 완료 뒤에도 마지막 건설 세션이 끝날 때까지 유지해야 프리로드된 텍스처가 메모리에 남습니다.
	TSharedPtr<FStreamableHandle> ThumbnailPreloadHandle;

	// 늦게 도착한 이전 요청 콜백을 거부하기 위해 새 요청/종료 때마다 증가하는 세대값입니다. 
	uint64 RequestGeneration = 0;
	uint64 ThumbnailRequestGeneration = 0;

	// 여러 월드/소비자가 같은 GameInstance 카탈로그를 사용할 수 있어 세션 소유권을 참조 수로 관리합니다.
	int32 ThumbnailPreloadSessionRefs = 0;
	bool bThumbnailPreloadStarted = false;

	UPROPERTY(Transient)
	EFF_BuildCatalogThumbnailLoadState ThumbnailLoadState = EFF_BuildCatalogThumbnailLoadState::NotRequested;

	// 마지막 재구성 중 개별 사유로 카탈로그에서 제외된 Primary Asset 수입니다. 
	int32 SkippedAssetCount = 0;
};
