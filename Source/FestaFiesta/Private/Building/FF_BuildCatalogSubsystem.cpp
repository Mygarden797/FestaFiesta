// FF_BuildCatalogSubsystem.cpp

#include "Building/FF_BuildCatalogSubsystem.h"

#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "FF_FFZSFacilityDefinition.h"

#define LOCTEXT_NAMESPACE "FFBuildCatalogSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogFestaFiestaBuildCatalog, Log, All);

namespace
{
	// Asset Manager 설정과 조회에 사용하는 시설 정의 Primary Asset 타입 이름입니다. 
	const FPrimaryAssetType FacilityDefinitionAssetType(TEXT("FFZSFacilityDefinition"));
	// Facility Definition의 CatalogThumbnail 프로퍼티에 선언된 UI 전용 Asset Bundle 이름입니다.
	const FName BuildCatalogThumbnailBundle(TEXT("BuildCatalogUI"));
}

void UFF_BuildCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// GameInstance 수명과 함께 카탈로그를 준비하여 UI가 별도 초기화 순서를 관리하지 않게 합니다.
	RefreshCatalog();
}

void UFF_BuildCatalogSubsystem::Deinitialize()
{
	// 취소보다 먼저 세대를 올려 CancelHandle 과정 또는 그 이후 도착하는 콜백을 무효화합니다.
	// Asset Manager는 전역으로 에셋 수명을 관리하므로 다른 시스템도 사용할 수 있는 Primary Asset을
	// 여기서 강제로 언로드하지 않고, 이 서브시스템이 가진 요청 핸들과 읽기 모델만 정리합니다.
	FFBuildCatalogModel::BeginNextRequest(RequestGeneration);
	ThumbnailPreloadSessionRefs = 0;
	ResetThumbnailPreloadRequest(false);
	if (ActiveLoadHandle.IsValid() && !ActiveLoadHandle->HasLoadCompleted())
	{
		ActiveLoadHandle->CancelHandle();
	}

	ActiveLoadHandle.Reset();
	RequestedAssetIds.Reset();
	EntryIndexByDefinitionId.Reset();
	Entries.Reset();
	CategoryGroups.Reset();
	LoadState = EFF_BuildCatalogLoadState::NotRequested;
	LastError = FText::GetEmpty();
	SkippedAssetCount = 0;

	Super::Deinitialize();
}

void UFF_BuildCatalogSubsystem::RefreshCatalog()
{
	// 새 정의 목록에는 다른 썸네일 집합이 대응할 수 있다. 기존 프리로드 핸들을 먼저 해제하되,
	// 활성 건설 세션의 요청 의도는 유지해 새 목록이 Ready가 되면 자동으로 다시 시작한다.
	ResetThumbnailPreloadRequest(ThumbnailPreloadSessionRefs > 0);

	// 이번 요청만 상태를 반영할 수 있도록 고유 세대를 먼저 확보합니다.
	const uint64 ThisGeneration = FFBuildCatalogModel::BeginNextRequest(RequestGeneration);
	// 연속 새로고침 시 이전 비동기 작업을 중단합니다. 이전 콜백은 이미 세대가 달라 무시됩니다.
	if (ActiveLoadHandle.IsValid() && !ActiveLoadHandle->HasLoadCompleted())
	{
		ActiveLoadHandle->CancelHandle();
	}
	ActiveLoadHandle.Reset();

	// 새 결과가 준비되는 동안 호출자가 오래된 카탈로그를 현재 결과로 오인하지 않도록 모두 비웁니다.
	RequestedAssetIds.Reset();
	EntryIndexByDefinitionId.Reset();
	Entries.Reset();
	CategoryGroups.Reset();
	SkippedAssetCount = 0;
	LastError = FText::GetEmpty();
	LoadState = EFF_BuildCatalogLoadState::Loading;

	UAssetManager& AssetManager = UAssetManager::Get();
	FPrimaryAssetTypeInfo TypeInfo;
	// 프로젝트 설정에 타입 자체가 등록되지 않은 경우에는 검색 결과 0건과 구분해 구성 오류로 처리합니다.
	if (!AssetManager.GetPrimaryAssetTypeInfo(FacilityDefinitionAssetType, TypeInfo))
	{
		FinishRefresh(
			EFF_BuildCatalogLoadState::Failed,
			LOCTEXT("MissingFacilityDefinitionAssetType",
				"FFZSFacilityDefinition is not configured as a Primary Asset type."));
		return;
	}

	AssetManager.GetPrimaryAssetIdList(FacilityDefinitionAssetType, RequestedAssetIds);
	// Asset Registry의 반환 순서는 보장되지 않습니다. 로드 전부터 ID를 정렬하면 검증/로그 순서와
	// 중복 FacilityDefinitionId가 있을 때 어느 에셋을 남길지도 실행마다 동일해집니다.
	RequestedAssetIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return FFBuildCatalogModel::CompareStrings(A.ToString(), B.ToString()) < 0;
	});

	if (RequestedAssetIds.IsEmpty())
	{
		// 등록된 콘텐츠가 없는 것은 유효한 빈 카탈로그입니다. 오류 대신 UI의 빈 상태로 연결합니다.
		FinishRefresh(EFF_BuildCatalogLoadState::Ready);
		return;
	}

	FAssetManagerLoadParams LoadParams;
	// 각 콜백에 이번 세대를 함께 바인딩하여 취소된 이전 요청이 최신 결과를 덮어쓰지 못하게 합니다.
	LoadParams.OnComplete = FStreamableDelegateWithHandle::CreateUObject(
		this,
		&UFF_BuildCatalogSubsystem::HandleCatalogLoadCompleted,
		ThisGeneration);
	LoadParams.OnCancel = FStreamableDelegateWithHandle::CreateUObject(
		this,
		&UFF_BuildCatalogSubsystem::HandleCatalogLoadCanceled,
		ThisGeneration);

	// 빈 번들 목록은 Primary Asset 본체만 요청한다는 뜻입니다. 반환 핸들은 다음 새로고침이나
	// 서브시스템 종료에서 해당 요청을 취소할 수 있도록 보관합니다.
	ActiveLoadHandle = AssetManager.LoadPrimaryAssets(
		RequestedAssetIds,
		TArray<FName>(),
		MoveTemp(LoadParams));
}

void UFF_BuildCatalogSubsystem::AcquireThumbnailPreload()
{
	++ThumbnailPreloadSessionRefs;
	if (ThumbnailPreloadSessionRefs > 1)
	{
		return;
	}

	bThumbnailPreloadStarted = false;
	SetThumbnailLoadState(EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog);
	StartThumbnailPreloadIfReady();
}

void UFF_BuildCatalogSubsystem::ReleaseThumbnailPreload()
{
	if (ThumbnailPreloadSessionRefs <= 0)
	{
		UE_LOG(LogFestaFiestaBuildCatalog, Warning,
			TEXT("Ignoring an unmatched catalog thumbnail preload release."));
		return;
	}

	--ThumbnailPreloadSessionRefs;
	if (ThumbnailPreloadSessionRefs > 0)
	{
		return;
	}

	ResetThumbnailPreloadRequest(false);
}

bool UFF_BuildCatalogSubsystem::FindEntry(
	const FName FacilityDefinitionId,
	FFF_BuildCatalogEntry& OutEntry) const
{
	// 실패 경로에서도 호출자가 이전 호출의 OutEntry 값을 실수로 재사용하지 않게 기본값을 보장합니다.
	OutEntry = FFF_BuildCatalogEntry();
	const int32* EntryIndex = EntryIndexByDefinitionId.Find(FacilityDefinitionId);
	// 맵과 배열의 불일치까지 방어하여 잘못된 인덱스로 접근하지 않습니다.
	if (!EntryIndex || !Entries.IsValidIndex(*EntryIndex))
	{
		return false;
	}

	// 외부에서 내부 캐시를 변경할 수 없도록 값 복사본을 반환합니다.
	OutEntry = Entries[*EntryIndex];
	return true;
}

UFFZSFacilityDefinition* UFF_BuildCatalogSubsystem::FindDefinition(
	const FName FacilityDefinitionId) const
{
	const int32* EntryIndex = EntryIndexByDefinitionId.Find(FacilityDefinitionId);
	// TSoftObjectPtr::Get은 현재 메모리에 있는 객체만 확인하므로 이 조회는 동기 로드를 유발하지 않습니다.
	return EntryIndex && Entries.IsValidIndex(*EntryIndex)
		? Entries[*EntryIndex].Definition.Get()
		: nullptr;
}

bool UFF_BuildCatalogSubsystem::TryGetDefinitionSpec(
	const FName FacilityDefinitionId,
	FFFZSFacilitySpec& OutSpec) const
{
	// 조회 실패 시에도 출력 매개변수의 결과를 명확히 하기 위해 먼저 기본 Spec으로 초기화합니다.
	OutSpec = FFFZSFacilitySpec();
	const UFFZSFacilityDefinition* Definition = FindDefinition(FacilityDefinitionId);
	if (!Definition)
	{
		return false;
	}

	// 배치 명령 쪽이 Data Asset을 수정하거나 수명에 의존하지 않도록 Spec 값을 복사합니다.
	OutSpec = Definition->Spec;
	return true;
}

void UFF_BuildCatalogSubsystem::HandleCatalogLoadCompleted(
	TSharedPtr<FStreamableHandle> CompletedHandle,
	const uint64 Generation)
{
	// 새로고침 또는 종료 후 뒤늦게 완료된 콜백은 어떤 상태도 건드리지 않습니다.
	if (!FFBuildCatalogModel::IsCurrentRequest(Generation, RequestGeneration))
	{
		return;
	}
	// 델리게이트 시그니처상 전달되지만 현재 구현은 멤버 핸들/Asset Manager를 통해 결과를 읽습니다.
	(void)CompletedHandle;

	// 요청 핸들은 더 이상 필요하지 않습니다. 개별 에셋 누락은 재구성 단계에서 스킵으로 집계합니다.
	ActiveLoadHandle.Reset();
	RebuildCatalogFromLoadedAssets();
	FinishRefresh(EFF_BuildCatalogLoadState::Ready);
}

void UFF_BuildCatalogSubsystem::HandleCatalogLoadCanceled(
	TSharedPtr<FStreamableHandle> CanceledHandle,
	const uint64 Generation)
{
	// 이전 요청을 새로고침 과정에서 의도적으로 취소한 경우 최신 Loading 상태를 Failed로 바꾸지 않습니다.
	if (!FFBuildCatalogModel::IsCurrentRequest(Generation, RequestGeneration))
	{
		return;
	}
	// 콜백 핸들은 시그니처 호환을 위해 받으며, 정리는 보관 중인 멤버 핸들을 기준으로 수행합니다.
	(void)CanceledHandle;

	ActiveLoadHandle.Reset();
	FinishRefresh(
		EFF_BuildCatalogLoadState::Failed,
		LOCTEXT("FacilityCatalogLoadCanceled", "Construction catalog loading was canceled."));
}

void UFF_BuildCatalogSubsystem::StartThumbnailPreloadIfReady()
{
	if (ThumbnailPreloadSessionRefs <= 0 || bThumbnailPreloadStarted)
	{
		return;
	}

	if (LoadState == EFF_BuildCatalogLoadState::Failed)
	{
		SetThumbnailLoadState(EFF_BuildCatalogThumbnailLoadState::Failed);
		return;
	}
	if (LoadState != EFF_BuildCatalogLoadState::Ready)
	{
		SetThumbnailLoadState(EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog);
		return;
	}

	TArray<FPrimaryAssetId> ThumbnailAssetIds;
	ThumbnailAssetIds.Reserve(Entries.Num());
	for (const FFF_BuildCatalogEntry& Entry : Entries)
	{
		if (!Entry.Thumbnail.IsNull() && Entry.PrimaryAssetId.IsValid())
		{
			// Entries는 Primary Asset 하나당 최대 한 항목만 가지므로 대규모 목록에서도 선형으로 수집합니다.
			ThumbnailAssetIds.Add(Entry.PrimaryAssetId);
		}
	}

	bThumbnailPreloadStarted = true;
	if (ThumbnailAssetIds.IsEmpty())
	{
		SetThumbnailLoadState(EFF_BuildCatalogThumbnailLoadState::Ready);
		return;
	}

	const uint64 ThisGeneration = FFBuildCatalogModel::BeginNextRequest(ThumbnailRequestGeneration);
	SetThumbnailLoadState(EFF_BuildCatalogThumbnailLoadState::Loading);

	FAssetManagerLoadParams LoadParams;
	LoadParams.OnComplete = FStreamableDelegateWithHandle::CreateUObject(
		this,
		&UFF_BuildCatalogSubsystem::HandleThumbnailPreloadCompleted,
		ThisGeneration);
	LoadParams.OnCancel = FStreamableDelegateWithHandle::CreateUObject(
		this,
		&UFF_BuildCatalogSubsystem::HandleThumbnailPreloadCanceled,
		ThisGeneration);

	TSharedPtr<FStreamableHandle> NewHandle = UAssetManager::Get().PreloadPrimaryAssets(
		ThumbnailAssetIds,
		TArray<FName>{BuildCatalogThumbnailBundle},
		false,
		MoveTemp(LoadParams));
	// UE 계약상 할 일이 없으면 null 핸들을 반환하더라도 완료 델리게이트는 호출됩니다.
	// 따라서 null 자체를 실패로 확정하지 않고 세대가 유효한 완료 콜백이 종결 상태를 결정하게 합니다.

	// 완료 델리게이트가 동기적으로 상태를 바꾸거나 세션을 해제할 가능성까지 방어한다.
	// 세대와 참조가 그대로인 경우에만 핸들을 보관하고, 그렇지 않으면 새 요청이 잡은 참조를 즉시 놓는다.
	if (FFBuildCatalogModel::IsCurrentRequest(ThisGeneration, ThumbnailRequestGeneration) &&
		ThumbnailPreloadSessionRefs > 0 && bThumbnailPreloadStarted)
	{
		ThumbnailPreloadHandle = MoveTemp(NewHandle);
	}
	else if (NewHandle.IsValid())
	{
		if (NewHandle->HasLoadCompleted())
		{
			NewHandle->ReleaseHandle();
		}
		else
		{
			NewHandle->CancelHandle();
		}
	}
}

void UFF_BuildCatalogSubsystem::HandleThumbnailPreloadCompleted(
	TSharedPtr<FStreamableHandle> CompletedHandle,
	const uint64 Generation)
{
	if (!FFBuildCatalogModel::IsCurrentRequest(Generation, ThumbnailRequestGeneration) ||
		ThumbnailPreloadSessionRefs <= 0 || !bThumbnailPreloadStarted)
	{
		return;
	}

	// 오류가 있어도 성공한 텍스처와 핸들은 유지한다. 카드는 Get() 결과가 없는 항목만 자리표시자로 남긴다.
	SetThumbnailLoadState(
		CompletedHandle.IsValid() && CompletedHandle->HasError()
			? EFF_BuildCatalogThumbnailLoadState::Failed
			: EFF_BuildCatalogThumbnailLoadState::Ready);
}

void UFF_BuildCatalogSubsystem::HandleThumbnailPreloadCanceled(
	TSharedPtr<FStreamableHandle> CanceledHandle,
	const uint64 Generation)
{
	if (!FFBuildCatalogModel::IsCurrentRequest(Generation, ThumbnailRequestGeneration) ||
		ThumbnailPreloadSessionRefs <= 0)
	{
		return;
	}

	(void)CanceledHandle;
	ThumbnailPreloadHandle.Reset();
	bThumbnailPreloadStarted = false;
	SetThumbnailLoadState(EFF_BuildCatalogThumbnailLoadState::Failed);
}

void UFF_BuildCatalogSubsystem::ResetThumbnailPreloadRequest(const bool bKeepSessionIntent)
{
	// 먼저 세대를 넘겨 CancelHandle이 즉시 호출하는 취소 델리게이트도 오래된 요청으로 취급하게 한다.
	FFBuildCatalogModel::BeginNextRequest(ThumbnailRequestGeneration);
	if (ThumbnailPreloadHandle.IsValid())
	{
		if (ThumbnailPreloadHandle->HasLoadCompleted())
		{
			ThumbnailPreloadHandle->ReleaseHandle();
		}
		else
		{
			ThumbnailPreloadHandle->CancelHandle();
		}
	}

	ThumbnailPreloadHandle.Reset();
	bThumbnailPreloadStarted = false;
	SetThumbnailLoadState(
		bKeepSessionIntent
			? EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog
			: EFF_BuildCatalogThumbnailLoadState::NotRequested);
}

void UFF_BuildCatalogSubsystem::SetThumbnailLoadState(
	const EFF_BuildCatalogThumbnailLoadState NewState)
{
	if (ThumbnailLoadState == NewState)
	{
		return;
	}

	ThumbnailLoadState = NewState;
	OnCatalogThumbnailsChanged.Broadcast();
}

void UFF_BuildCatalogSubsystem::RebuildCatalogFromLoadedAssets()
{
	// 재구성은 이전 결과에 덧붙이지 않고, 현재 RequestedAssetIds만으로 새 스냅샷을 만듭니다.
	Entries.Reset();
	CategoryGroups.Reset();
	EntryIndexByDefinitionId.Reset();
	SkippedAssetCount = 0;

	UAssetManager& AssetManager = UAssetManager::Get();
	// 서로 다른 Primary Asset이 같은 도메인 ID를 선언하는 콘텐츠 오류를 한 번의 순회로 검출합니다.
	TSet<FName> SeenDefinitionIds;

	for (const FPrimaryAssetId& AssetId : RequestedAssetIds)
	{
		// 완료된 비동기 요청의 결과만 조회합니다. 여기서는 누락된 객체를 동기 로드하지 않습니다.
		UFFZSFacilityDefinition* Definition =
			AssetManager.GetPrimaryAssetObject<UFFZSFacilityDefinition>(AssetId);
		if (!Definition)
		{
			// 개별 에셋 로드 실패는 전체 메뉴를 막지 않고 부분 결과를 제공하되 진단 로그와 수를 남깁니다.
			++SkippedAssetCount;
			UE_LOG(LogFestaFiestaBuildCatalog, Warning,
				TEXT("Skipping Facility Definition '%s': Primary Asset failed to load."),
				*AssetId.ToString());
			continue;
		}

		FFF_BuildCatalogEntry Entry;
		FString FailureReason;
		// 원본 정의의 유효성과 이름 규칙을 모두 통과한 항목만 UI 읽기 모델로 채택합니다.
		if (!TryMakeEntry(AssetId, *Definition, Entry, FailureReason))
		{
			++SkippedAssetCount;
			UE_LOG(LogFestaFiestaBuildCatalog, Warning,
				TEXT("Skipping Facility Definition '%s': %s"),
				*AssetId.ToString(),
				*FailureReason);
			continue;
		}

		// RequestedAssetIds가 미리 정렬되어 있으므로 중복 시 먼저 남는 에셋도 결정적입니다.
		if (SeenDefinitionIds.Contains(Entry.FacilityDefinitionId))
		{
			++SkippedAssetCount;
			UE_LOG(LogFestaFiestaBuildCatalog, Warning,
				TEXT("Skipping duplicate FacilityDefinitionId '%s' from Primary Asset '%s'."),
				*Entry.FacilityDefinitionId.ToString(),
				*AssetId.ToString());
			continue;
		}

		SeenDefinitionIds.Add(Entry.FacilityDefinitionId);
		Entries.Add(MoveTemp(Entry));
	}

	// UI 표시 순서를 확정한 뒤 인덱스 맵을 만들지 않으면 정렬 전 인덱스가 가리키는 항목과 달라집니다.
	FFBuildCatalogModel::SortEntries(Entries);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		EntryIndexByDefinitionId.Add(Entries[Index].FacilityDefinitionId, Index);
	}

	RebuildCategoryGroups();
}

void UFF_BuildCatalogSubsystem::RebuildCategoryGroups()
{
	// SortEntries 결과는 같은 대/소분류가 인접하므로 그룹 빌더가 한 번의 순회로 계층을 만들 수 있습니다.
	FFBuildCatalogModel::BuildCategoryGroups(Entries, CategoryGroups);
}

void UFF_BuildCatalogSubsystem::FinishRefresh(
	const EFF_BuildCatalogLoadState NewState,
	const FText& Error)
{
	// 상태와 오류를 먼저 저장하여 이벤트를 받은 구독자가 즉시 일관된 최종 스냅샷을 읽게 합니다.
	LoadState = NewState;
	LastError = Error;
	OnCatalogChanged.Broadcast();

	if (NewState == EFF_BuildCatalogLoadState::Ready)
	{
		StartThumbnailPreloadIfReady();
	}
	else if (NewState == EFF_BuildCatalogLoadState::Failed && ThumbnailPreloadSessionRefs > 0)
	{
		SetThumbnailLoadState(EFF_BuildCatalogThumbnailLoadState::Failed);
	}
}

bool UFF_BuildCatalogSubsystem::TryMakeEntry(
	const FPrimaryAssetId& AssetId,
	UFFZSFacilityDefinition& Definition,
	FFF_BuildCatalogEntry& OutEntry,
	FString& OutFailureReason) const
{
	// 출력 매개변수는 매 호출마다 초기화하여 이전 성공/실패의 잔여 데이터를 남기지 않습니다.
	OutEntry = FFF_BuildCatalogEntry();
	OutFailureReason.Reset();

	TArray<FText> DefinitionErrors;
	// Data Asset 자체의 필수 필드와 도메인 불변식을 먼저 검사합니다.
	if (!Definition.IsDefinitionValid(DefinitionErrors))
	{
		TArray<FString> ErrorStrings;
		ErrorStrings.Reserve(DefinitionErrors.Num());
		for (const FText& Error : DefinitionErrors)
		{
			ErrorStrings.Add(Error.ToString());
		}
		// 여러 검증 오류를 한 로그 줄에서 모두 확인할 수 있게 구분자로 합칩니다.
		OutFailureReason = FString::Join(ErrorStrings, TEXT(" | "));
		return false;
	}

	FFF_FacilityNameParts NameParts;
	// FacilityDefinitionId의 프로젝트 명명 규칙을 검증하면서 UI 그룹용 Minor/Unique 토큰도 추출합니다.
	if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
		Definition.Spec.Category,
		Definition.Spec.FacilityDefinitionId,
		NameParts,
		OutFailureReason))
	{
		// 파서가 상세 사유를 제공하지 않은 예외적인 실패에도 운영 로그가 비지 않게 기본 메시지를 둡니다.
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = TEXT("FacilityDefinitionId is missing or does not follow the naming convention.");
		}
		return false;
	}

	// 검증된 원본 값과 파싱 결과만 읽기 모델에 복사합니다.
	OutEntry.PrimaryAssetId = AssetId;
	OutEntry.FacilityDefinitionId = Definition.Spec.FacilityDefinitionId;
	OutEntry.DisplayName = Definition.Spec.DisplayName;
	OutEntry.Category = Definition.Spec.Category;
	OutEntry.MinorToken = NameParts.MinorToken;
	OutEntry.UniqueNameToken = NameParts.UniqueToken;
	// 소프트 참조는 위젯에 원본 식별 경로를 제공하되 강한 객체 수명 소유권을 넘기지 않습니다.
	OutEntry.Definition = TSoftObjectPtr<UFFZSFacilityDefinition>(&Definition);
	// UI 전용 이미지도 소프트 참조 그대로 전달하여 카탈로그 구성 중 동기 로드를 강제하지 않습니다.
	OutEntry.Thumbnail = Definition.CatalogThumbnail;
	return true;
}

#undef LOCTEXT_NAMESPACE
