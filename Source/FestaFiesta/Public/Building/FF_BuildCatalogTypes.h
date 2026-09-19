// FF_BuildCatalogTypes.h

// 건설 카탈로그의 로드 상태와 UI가 소비하는 불변 항목·그룹 스냅샷을 정의한다.
// 흐름: FacilityDefinition을 Entry로 투영한 뒤 소분류와 대분류 계층으로 묶어 화면에 전달한다.
// 불변식: 표시 값은 비동기 갱신 사이에도 안정적인 값 복사본으로 유지한다.
// 실패/수명: 런타임 소유 객체는 약한 참조로 보관해 언로드된 에셋을 스냅샷이 붙잡지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "FF_FFZSTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "FF_BuildCatalogTypes.generated.h"

class UFFZSFacilityDefinition;
class UTexture2D;

// 건설 카탈로그의 비동기 로드 진행 상태입니다.
// UI는 이 값을 먼저 읽어 로딩 표시, 정상 목록/빈 상태, 오류 표시 중 무엇을 보여 줄지 결정합니다.
 
UENUM(BlueprintType)
enum class EFF_BuildCatalogLoadState : uint8
{
	// 서브시스템이 생성되었지만 아직 검색 요청을 시작하지 않았거나 종료 정리까지 마친 상태입니다. 
	NotRequested,
	// Primary Asset 목록을 확보하고 필요한 정의 에셋을 비동기로 로드하는 중입니다. 
	Loading,
	// 요청이 정상 종료된 상태입니다. 유효한 에셋이 하나도 없는 빈 카탈로그도 Ready에 포함됩니다. 
	Ready,
	// 에셋 타입 미설정 또는 현재 로드 요청 취소처럼 카탈로그 전체 요청을 완료할 수 없는 상태입니다. 
	Failed
};

// 건설 세션 동안만 유지되는 카탈로그 썸네일 Asset Bundle의 비동기 준비 상태입니다.
// 시설 정의 목록과 별도 상태로 관리하므로 이미지 일부가 실패해도 시설 선택·배치는 계속 사용할 수 있습니다.
UENUM(BlueprintType)
enum class EFF_BuildCatalogThumbnailLoadState : uint8
{
	// 활성 건설 세션이 없어 썸네일을 메모리에 고정하지 않는 상태입니다.
	NotRequested,
	// 건설 세션은 시작됐지만 시설 정의 목록이 아직 준비되지 않은 상태입니다.
	WaitingForCatalog,
	// BuildCatalogUI Asset Bundle을 비동기로 프리로드하는 중입니다.
	Loading,
	// 모든 요청이 끝났으며 세션 핸들이 썸네일을 메모리에 유지하는 상태입니다.
	Ready,
	// 일부 또는 전체 썸네일 로드가 실패한 상태입니다. UI는 준비된 이미지만 쓰고 나머지는 자리표시자로 표시합니다.
	Failed
};

// FFZS Facility Definition Primary Asset 하나를 UI에서 선택 가능한 항목으로 변환한 읽기 모델입니다.

// 원본 Data Asset의 전체 데이터를 메뉴에 노출하지 않고, 분류·표시·선택에 필요한 값만 담습니다.
// Thumbnail은 Facility Definition의 선택적 CatalogThumbnail을 전달합니다.
// null은 오류가 아니라 UI가 공통 자리표시자를 그려야 한다는 계약입니다.
 
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_BuildCatalogEntry
{
	GENERATED_BODY()

	// Asset Manager가 검색·로드에 사용하는 `타입:이름` 형태의 Primary Asset 식별자입니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	FPrimaryAssetId PrimaryAssetId;

	// 게임플레이와 배치 명령이 시설을 식별할 때 사용하는 도메인 ID입니다.
	// PrimaryAssetId와 목적이 다르며, 한 카탈로그 안에서 중복되는 정의는 제외됩니다.
	 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	FName FacilityDefinitionId = NAME_None;

	// 메뉴에 표시할 지역화 가능한 시설 이름이며, 같은 소분류 안의 1차 항목 정렬 키이기도 합니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	FText DisplayName;

	// Booth, Toilet 등 메뉴의 최상위 탭/섹션을 결정하는 시설 대분류입니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth;

	// FacilityDefinitionId에서 파싱한 두 번째 이름 토큰으로, 대분류 안의 소분류 행을 구성합니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	FName MinorToken = NAME_None;

	// FacilityDefinitionId의 세 번째 이후 이름 토큰이며, 여러 토큰 사이의 밑줄을 보존합니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	FString UniqueNameToken;

	// 원본 시설 정의를 가리키는 소프트 참조입니다. UI가 Data Asset의 강한 수명 소유자가 되지
	// 않으면서도 식별 경로를 보관할 수 있으며, 조회 API는 이미 로드된 객체만 반환합니다.
	 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	TSoftObjectPtr<UFFZSFacilityDefinition> Definition;

	// 선택적 표시용 썸네일입니다. null이면 UI가 공통 자리표시자 이미지를 사용해야 합니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	// Facility Definition Spec에서 투영한 월드 동시 설치 상한입니다. 0이면 제한 없음입니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Limit")
	int32 MaxInstancesPerWorld = 0;

	// 이 카탈로그 스냅샷을 화면에 투영한 시점의 동일 Definition 등록 시설 수입니다.
	// 권위 판정 원천이 아니며 화면을 다시 열거나 월드 변경을 받을 때 새로 계산합니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Limit")
	int32 CurrentInstanceCount = 0;

	// 유한 상한에 도달했는지 나타내는 표현 전용 값입니다. 최종 설치 판정은 Zone System이 다시 수행합니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Limit")
	bool bBuildLimitReached = false;

	// 현재 축제의 권위 필수시설 상태를 화면용으로 투영한 값입니다. 가격·건축 한도와 독립적입니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Required")
	bool bRequiredForSetup = false;

	// 설정이 유효하고 필요한 개수가 모두 설치된 경우에만 true입니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Required")
	bool bRequiredSetupSatisfied = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Required")
	int32 RequiredInstalledCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Required")
	int32 RequiredMinimumCount = 0;

	// 현재 Festival의 FacilityCostTable에서 해석한 설치비입니다. 카탈로그 구성 시 한 번 계산해
	// UMG/Slate 표현이 같은 가격 원천을 사용하게 하며, 실패하면 bHasResolvedInstallCost가 false입니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Economy")
	int32 InstallCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Economy")
	bool bHasResolvedInstallCost = false;

	// 표시/선택 경로가 같은 기준을 사용한다. 기본값 0만으로는 무료 가격이 되지 않는다.
	bool HasValidInstallCost() const
	{
		return bHasResolvedInstallCost && FFF_FacilityCostResolver::IsValidInstallCost(InstallCost);
	}

	// 현재 Festival의 철거 환급률을 설치비에 적용한 회수 금액입니다.
	// 환급률 0은 유효한 설정이므로 값이 0인지와 해석 성공 여부를 별도로 보관합니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Economy")
	int32 RecoveryCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Economy")
	bool bHasResolvedRecoveryCost = false;

	// 진단과 콘텐츠 QA용 실제 매칭 행입니다. UI에는 노출하지 않습니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog|Economy")
	FName MatchedInstallCostRow = NAME_None;
};

// 한 시설 대분류 안에서 소분류 하나를 나타내는, 결정적 순서의 메뉴 행입니다. 
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_BuildCatalogMinorGroup
{
	GENERATED_BODY()

	// 이 행을 식별하는 소분류 토큰으로, Items의 모든 항목이 같은 값을 갖습니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	FName MinorToken = NAME_None;

	// 표시 이름과 시설 정의 ID 규칙에 따라 정렬된 소분류 내 선택 항목 복사본입니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	TArray<FFF_BuildCatalogEntry> Items;
};

// 건설 메뉴에 표시되는 최상위 대분류와 그 안의 소분류 행을 묶은 읽기 모델입니다. 
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_BuildCatalogCategoryGroup
{
	GENERATED_BODY()

	// 이 그룹의 시설 대분류이며 최상위 그룹 배열에서는 enum 값 순으로 정렬됩니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth;

	// MinorToken의 대소문자 구분 사전식 순서로 정렬된 소분류 행 배열입니다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Catalog")
	TArray<FFF_BuildCatalogMinorGroup> MinorGroups;
};
