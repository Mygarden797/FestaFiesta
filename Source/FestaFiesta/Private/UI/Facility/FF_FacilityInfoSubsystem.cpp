// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoSubsystem.h"

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSCollision.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSelection.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

#define LOCTEXT_NAMESPACE "FFFacilityInfoSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogFestaFiestaFacilityInfo, Log, All);

namespace
{
	FText FormatIntegerWithSuffix(const int32 Value, const FText& Suffix)
	{
		return FText::Format(LOCTEXT("IntegerSuffixFormat", "{0}{1}"), FText::AsNumber(Value), Suffix);
	}

	FText FormatOccupancy(const int32 Occupied, const int32 Capacity)
	{
		return FText::Format(
			LOCTEXT("OccupancyFormat", "{0} / {1}명"),
			FText::AsNumber(Occupied),
			FText::AsNumber(Capacity));
	}
}

void UFF_FacilityInfoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);

	if (UFFZSWorldSubsystem* ZoneSystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(
			this, &ThisClass::HandleWorldSnapshotRestored);
	}
}

void UFF_FacilityInfoSubsystem::Deinitialize()
{
	ReleasePickControllerBinding();
	if (UWorld* World = GetWorld())
	{
		if (UFFZSWorldSubsystem* ZoneSystem = World->GetSubsystem<UFFZSWorldSubsystem>())
		{
			ZoneSystem->OnWorldChanged.RemoveAll(this);
			ZoneSystem->OnWorldSnapshotRestored.RemoveAll(this);
		}
	}

	ReleaseThumbnailSession();
	SelectedFacilityId.Invalidate();
	SelectionContext = EFF_FacilityInfoContext::None;
	if (Panel)
	{
		Panel->OnMoveRequested.RemoveAll(this);
		Panel->OnRemoveRequested.RemoveAll(this);
		Panel->OnCloseRequested.RemoveAll(this);
		Panel->RemoveFromParent();
		Panel = nullptr;
	}
	OwningController.Reset();
	Super::Deinitialize();
}

bool UFF_FacilityInfoSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UFF_FacilityInfoSubsystem::Tick(const float DeltaTime)
{
	// The camera controller already owns click timing and input gating. Bind to its public pick
	// signal here so Camera source files stay untouched and controller replacement is tolerated.
	EnsurePickControllerBinding();
	if (!HasSelection())
	{
		RefreshAccumulatorSeconds = 0.0f;
		return;
	}

	RefreshAccumulatorSeconds += FMath::Max(0.0f, DeltaTime);
	if (RefreshAccumulatorSeconds < RefreshIntervalSeconds)
	{
		return;
	}
	RefreshAccumulatorSeconds = FMath::Fmod(RefreshAccumulatorSeconds, RefreshIntervalSeconds);
	RefreshPanel();
}

TStatId UFF_FacilityInfoSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FacilityInfoSubsystem, STATGROUP_Tickables);
}

FSoftClassPath UFF_FacilityInfoSubsystem::GetDefaultPanelClassPath()
{
	return FSoftClassPath(TEXT(
		"/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel."
		"WBP_FF_FacilityInfoPanel_C"));
}

bool UFF_FacilityInfoSubsystem::PrimePanel(APlayerController* InOwningController)
{
	return IsValid(InOwningController) && InOwningController->GetLocalPlayer() &&
		EnsurePanel(*InOwningController);
}

bool UFF_FacilityInfoSubsystem::EnsurePanel(APlayerController& InOwningController)
{
	if (Panel)
	{
		if (Panel->GetOwningPlayer() == &InOwningController && Panel->IsInViewport())
		{
			return true;
		}

		// Seamless controller replacement is uncommon in this single-player project, but keeping
		// the panel bound to a stale LocalPlayer would route input and viewport ownership incorrectly.
		Panel->OnMoveRequested.RemoveAll(this);
		Panel->OnRemoveRequested.RemoveAll(this);
		Panel->OnCloseRequested.RemoveAll(this);
		Panel->RemoveFromParent();
		Panel = nullptr;
		OwningController.Reset();
	}
	if (!InOwningController.GetLocalPlayer())
	{
		return false;
	}

	UClass* PanelClass = GetDefaultPanelClassPath().TryLoadClass<UFF_FacilityInfoPanel>();
	if (!PanelClass || !PanelClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()))
	{
		UE_LOG(LogFestaFiestaFacilityInfo, Error,
			TEXT("시설 정보 WBP를 불러오지 못했습니다: %s. 에디터에서 FF.BuildFacilityInfoUMG를 실행하세요."),
			*GetDefaultPanelClassPath().ToString());
		return false;
	}

	Panel = CreateWidget<UFF_FacilityInfoPanel>(&InOwningController, PanelClass);
	if (!Panel)
	{
		UE_LOG(LogFestaFiestaFacilityInfo, Error, TEXT("시설 정보 패널 인스턴스 생성 실패."));
		return false;
	}

	Panel->OnMoveRequested.AddUObject(this, &ThisClass::HandlePanelMoveRequested);
	Panel->OnRemoveRequested.AddUObject(this, &ThisClass::HandlePanelRemoveRequested);
	Panel->OnCloseRequested.AddUObject(this, &ThisClass::HandlePanelCloseRequested);
	if (!Panel->AddToPlayerScreen(PanelZOrder))
	{
		UE_LOG(LogFestaFiestaFacilityInfo, Error,
			TEXT("시설 정보 패널을 LocalPlayer 화면에 추가하지 못했습니다."));
		Panel->OnMoveRequested.RemoveAll(this);
		Panel->OnRemoveRequested.RemoveAll(this);
		Panel->OnCloseRequested.RemoveAll(this);
		Panel = nullptr;
		return false;
	}
	Panel->SetPanelOpen(false);
	OwningController = &InOwningController;
	return true;
}

bool UFF_FacilityInfoSubsystem::SelectFacility(
	APlayerController* InOwningController,
	AFFZSFacilityActor* Facility,
	const EFF_FacilityInfoContext Context)
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!IsValid(InOwningController) || !InOwningController->GetLocalPlayer() ||
		!IsValid(Facility) || !ZoneSystem ||
		Context == EFF_FacilityInfoContext::None || !IsContextCurrentlyAllowed(Context))
	{
		return false;
	}

	const FGuid FacilityId = Facility->GetFacilityInstanceId();
	if (!FacilityId.IsValid() || ZoneSystem->FindFacility(FacilityId) != Facility || !EnsurePanel(*InOwningController))
	{
		return false;
	}

	const bool bSelectionChanged = SelectedFacilityId != FacilityId || SelectionContext != Context;
	SelectedFacilityId = FacilityId;
	SelectionContext = Context;
	OwningController = InOwningController;
	FeedbackText = FText::GetEmpty();
	bFeedbackIsError = false;
	RefreshAccumulatorSeconds = 0.0f;
	AcquireThumbnailSession();
	RefreshPanel();

	// Refresh can clear the selection synchronously if a world-change listener removed or
	// replaced the actor while the panel was being projected. Never resurrect that stale card.
	if (!Panel || SelectedFacilityId != FacilityId || SelectionContext != Context)
	{
		return false;
	}
	Panel->SetPanelOpen(true);
	if (bSelectionChanged)
	{
		OnSelectionChanged.Broadcast(FacilityId, Context);
	}
	return true;
}

bool UFF_FacilityInfoSubsystem::SelectFacilityAtWorldPoint(
	APlayerController* InOwningController,
	const FVector& WorldPoint,
	const EFF_FacilityInfoContext Context,
	const FName ZoneId)
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	AFFZSFacilityActor* Facility = ZoneSystem
		? ZoneSystem->FindFacilityAtWorldPoint(WorldPoint, ZoneId)
		: nullptr;
	return Facility && SelectFacility(InOwningController, Facility, Context);
}

void UFF_FacilityInfoSubsystem::ClearSelection()
{
	const bool bHadSelection = HasSelection();
	SelectedFacilityId.Invalidate();
	SelectionContext = EFF_FacilityInfoContext::None;
	FeedbackText = FText::GetEmpty();
	bFeedbackIsError = false;
	RefreshAccumulatorSeconds = 0.0f;
	ReleaseThumbnailSession();
	if (Panel)
	{
		// Clear the view snapshot and image brush before collapsing so the last selected thumbnail
		// is not retained for the lifetime of the world.
		Panel->ApplyViewData(FFF_FacilityInfoViewData());
		Panel->SetPanelOpen(false);
	}
	if (bHadSelection)
	{
		OnSelectionChanged.Broadcast(FGuid(), EFF_FacilityInfoContext::None);
	}
}

void UFF_FacilityInfoSubsystem::ClearSelectionIfMatches(
	const FGuid FacilityInstanceId,
	const EFF_FacilityInfoContext Context)
{
	if (SelectedFacilityId == FacilityInstanceId && SelectionContext == Context)
	{
		ClearSelection();
	}
}

AFFZSFacilityActor* UFF_FacilityInfoSubsystem::GetSelectedFacility() const
{
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	return ZoneSystem && SelectedFacilityId.IsValid()
		? ZoneSystem->FindFacility(SelectedFacilityId)
		: nullptr;
}

bool UFF_FacilityInfoSubsystem::IsPointerOverPanel() const
{
	return Panel && Panel->IsPointerOverInteractivePanel();
}

void UFF_FacilityInfoSubsystem::SetFeedback(const FText& Feedback, const bool bIsError)
{
	if (!HasSelection())
	{
		return;
	}
	FeedbackText = Feedback;
	bFeedbackIsError = bIsError;
	RefreshPanel();
}

bool UFF_FacilityInfoSubsystem::IsContextCurrentlyAllowed(
	const EFF_FacilityInfoContext Context) const
{
	if (Context != EFF_FacilityInfoContext::Live)
	{
		return Context == EFF_FacilityInfoContext::Build;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	return Stage && Stage->CurrentStage == EFF_FestivalStage::Live;
}

bool UFF_FacilityInfoSubsystem::BuildViewData(
	AFFZSFacilityActor& Facility,
	FFF_FacilityInfoViewData& OutViewData) const
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneSystem || ZoneSystem->FindFacility(Facility.GetFacilityInstanceId()) != &Facility)
	{
		return false;
	}

	OutViewData = FFF_FacilityInfoViewData();
	const FFFZSFacilitySpec& Spec = Facility.GetFacilitySpec();
	const FFFZSFacilityReadModel ReadModel = Facility.GetReadModel();
	OutViewData.FacilityInstanceId = Facility.GetFacilityInstanceId();
	OutViewData.Context = SelectionContext;
	OutViewData.ContextTitle = SelectionContext == EFF_FacilityInfoContext::Build
		? LOCTEXT("BuildContextTitle", "건축 시설 정보")
		: LOCTEXT("LiveContextTitle", "라이브 시설 정보");
	OutViewData.DisplayName = Spec.DisplayName.IsEmpty()
		? LOCTEXT("UnknownFacility", "이름 없는 시설")
		: Spec.DisplayName;
	OutViewData.CategoryText = FFFacilityInfoModel::GetCategoryText(Spec.Category);
	OutViewData.ZoneText = Facility.GetZoneId().IsNone()
		? LOCTEXT("UnknownZone", "구역 정보 없음")
		: FText::FromName(Facility.GetZoneId());
	OutViewData.FacilityStateText = FFFacilityInfoModel::GetFacilityStateText(ReadModel.State);
	OutViewData.FootprintText = FFFacilityInfoModel::FormatFootprint(Spec.FootprintSizeCells);
	OutViewData.ClearanceText = FFFacilityInfoModel::FormatDistanceMeters(Spec.ClearanceCells);
	OutViewData.QueueCapacityText = FText::Format(
		LOCTEXT("QueueCapacityFormat", "대기 {0}명 · 동시 이용 {1}명"),
		FText::AsNumber(Spec.QueueRules.MaxWaitingAgents),
		FText::AsNumber(Spec.QueueRules.ServiceCapacity));
	OutViewData.ServiceDurationText = FText::Format(
		LOCTEXT("ServiceDurationFormat", "{0}초"),
		FText::AsNumber(Spec.QueueRules.ServiceDurationSeconds));
	OutViewData.bNeedsRelocation = ReadModel.bNeedsRelocation;
	OutViewData.FeedbackText = FeedbackText;
	OutViewData.bFeedbackIsError = bFeedbackIsError;

	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_BuildCatalogSubsystem* Catalog = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>()
		: nullptr;
	FFF_BuildCatalogEntry CatalogEntry;
	const bool bHasCatalogEntry = Catalog && Catalog->FindEntry(Spec.FacilityDefinitionId, CatalogEntry);
	OutViewData.Thumbnail = bHasCatalogEntry ? CatalogEntry.Thumbnail.Get() : nullptr;
	if (!Catalog)
	{
		OutViewData.ThumbnailPlaceholderText =
			LOCTEXT("ThumbnailUnavailable", "시설 이미지 정보 없음");
	}
	else if (!Catalog->IsCatalogReady())
	{
		OutViewData.ThumbnailPlaceholderText =
			Catalog->GetLoadState() == EFF_BuildCatalogLoadState::Failed
				? LOCTEXT("ThumbnailCatalogFailed", "시설 이미지 정보를 불러오지 못했습니다")
				: LOCTEXT("ThumbnailCatalogLoading", "시설 이미지 정보 불러오는 중");
	}
	else if (!bHasCatalogEntry || CatalogEntry.Thumbnail.IsNull())
	{
		OutViewData.ThumbnailPlaceholderText =
			LOCTEXT("ThumbnailNotAuthored", "등록된 시설 이미지 없음");
	}
	else if (OutViewData.Thumbnail)
	{
		OutViewData.ThumbnailPlaceholderText = FText::GetEmpty();
	}
	else
	{
		const EFF_BuildCatalogThumbnailLoadState ThumbnailState = Catalog->GetThumbnailLoadState();
		OutViewData.ThumbnailPlaceholderText =
			ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Failed ||
			ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Ready
				? LOCTEXT("ThumbnailLoadFailed", "시설 이미지를 표시할 수 없습니다")
				: LOCTEXT("ThumbnailLoading", "시설 이미지 불러오는 중");
	}

	// Catalog entries are presentation snapshots and are not guaranteed to carry projected prices.
	// Resolve from the installed actor's frozen Spec, exactly like the demolition economy bridge.
	const UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	const UFF_FestivalDefinition* Festival = Progression && Progression->IsFestivalStarted()
		? Progression->GetDefinition()
		: nullptr;
	int32 InstallCost = 0;
	FName MatchedCostRow = NAME_None;
	FString CostFailureReason;
	const bool bHasInstallCost = Festival && Festival->FacilityCostTable &&
		FFF_FacilityCostResolver::FindInstallCost(
			Spec,
			Festival->FacilityCostTable,
			InstallCost,
			MatchedCostRow,
			CostFailureReason) &&
		InstallCost > 0;
	const int32 RecoveryCost = bHasInstallCost
		? FFF_FacilityCostResolver::CalculateDemolitionRefund(
			InstallCost, Festival->FacilityDemolitionRefundRate)
		: 0;
	OutViewData.InstallCostText = FFFacilityInfoModel::FormatGold(bHasInstallCost, InstallCost);
	// A zero-Gold refund is valid when the authored refund rate is zero.
	OutViewData.RecoveryCostText = FFFacilityInfoModel::FormatGold(bHasInstallCost, RecoveryCost);

	const UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = World
		? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>()
		: nullptr;
	const UFF_QueueSubsystem* QueueSystem = World ? World->GetSubsystem<UFF_QueueSubsystem>() : nullptr;
	const int32 QueueFacilityId = QueueBridge
		? QueueBridge->FindQueueFacilityId(OutViewData.FacilityInstanceId)
		: INDEX_NONE;
	const bool bQueueRegistered = QueueSystem && QueueFacilityId != INDEX_NONE &&
		QueueSystem->IsFacilityRegistered(QueueFacilityId);
	const FFF_QueueState QueueState = bQueueRegistered && QueueBridge
		? QueueBridge->GetZoneFacilityQueueState(OutViewData.FacilityInstanceId)
		: FFF_QueueState();
	const bool bQueueSupported = QueueBridge && QueueSystem;
	const bool bQueueAvailable = bQueueRegistered && QueueState.FacilityId == QueueFacilityId;
	const FText MissingQueueMetric = bQueueSupported
		? LOCTEXT("PendingQueueMetric", "준비 중")
		: LOCTEXT("UnsupportedQueueMetric", "사용 불가");

	OutViewData.OperationStateText = FFFacilityInfoModel::GetOperationStateText(
		bQueueSupported, bQueueAvailable, QueueState.bIsOpen, QueueState.bAcceptingNew);
	OutViewData.WaitingPartiesText = bQueueAvailable
		? FormatIntegerWithSuffix(QueueState.WaitingPartyCount, LOCTEXT("TeamSuffix", "팀"))
		: MissingQueueMetric;
	OutViewData.OccupancyText = bQueueAvailable
		? FormatOccupancy(QueueState.OccupiedMemberCount, QueueState.MaxCapacityMembers)
		: MissingQueueMetric;
	OutViewData.RemainingServiceText = bQueueAvailable
		? FormatIntegerWithSuffix(QueueState.RemainingServiceMemberCount, LOCTEXT("PeopleSuffix", "명"))
		: MissingQueueMetric;
	const float ExpectedWaitSeconds = bQueueAvailable && QueueState.bIsOpen
		? QueueSystem->GetExpectedWaitSeconds(QueueFacilityId, 1)
		: 0.0f;
	OutViewData.ExpectedWaitText = bQueueAvailable
		? FFFacilityInfoModel::FormatExpectedWait(QueueState.bIsOpen, ExpectedWaitSeconds)
		: MissingQueueMetric;
	OutViewData.ThroughputText = bQueueAvailable
		? FFFacilityInfoModel::FormatThroughput(true, QueueState.GetServiceRatePerMinute())
		: MissingQueueMetric;

	const bool bReconfigurePhase = ZoneSystem->GetChangePhase() == EFFZSChangePhase::InitialSetup ||
		ZoneSystem->GetChangePhase() == EFFZSChangePhase::MorningCheck;
	const bool bQueueEmpty = !bQueueAvailable || QueueState.OccupiedMemberCount <= 0;
	const bool bCanModify = SelectionContext == EFF_FacilityInfoContext::Build &&
		bReconfigurePhase && bQueueEmpty && Facility.CanMoveOrRemove();
	OutViewData.bCanMove = bCanModify;
	OutViewData.bCanRemove = bCanModify;
	return true;
}

void UFF_FacilityInfoSubsystem::RefreshPanel()
{
	if (!HasSelection() || !Panel)
	{
		return;
	}
	if (!IsContextCurrentlyAllowed(SelectionContext))
	{
		ClearSelection();
		return;
	}

	AFFZSFacilityActor* Facility = GetSelectedFacility();
	FFF_FacilityInfoViewData ViewData;
	if (!Facility || !BuildViewData(*Facility, ViewData))
	{
		ClearSelection();
		return;
	}
	Panel->ApplyViewData(ViewData);
}

void UFF_FacilityInfoSubsystem::AcquireThumbnailSession()
{
	if (bThumbnailSessionAcquired)
	{
		return;
	}
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UFF_BuildCatalogSubsystem* Catalog = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>()
		: nullptr)
	{
		Catalog->AcquireThumbnailPreload();
		bThumbnailSessionAcquired = true;
	}
}

void UFF_FacilityInfoSubsystem::ReleaseThumbnailSession()
{
	if (!bThumbnailSessionAcquired)
	{
		return;
	}
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UFF_BuildCatalogSubsystem* Catalog = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>()
		: nullptr)
	{
		Catalog->ReleaseThumbnailPreload();
	}
	bThumbnailSessionAcquired = false;
}

void UFF_FacilityInfoSubsystem::EnsurePickControllerBinding()
{
	UWorld* World = GetWorld();
	AFF_FestivalPlayerController* Candidate = World
		? Cast<AFF_FestivalPlayerController>(World->GetFirstPlayerController())
		: nullptr;
	if (!Candidate || !Candidate->IsLocalController())
	{
		ReleasePickControllerBinding();
		return;
	}
	if (BoundPickController.Get() == Candidate)
	{
		return;
	}

	ReleasePickControllerBinding();
	Candidate->OnActorPicked.AddUniqueDynamic(this, &ThisClass::HandleActorPicked);
	BoundPickController = Candidate;
	// Prime once after a valid LocalPlayer appears. A first-click SelectFacility call still retries
	// safely if the viewport was not ready at this exact moment.
	PrimePanel(Candidate);
}

void UFF_FacilityInfoSubsystem::ReleasePickControllerBinding()
{
	if (AFF_FestivalPlayerController* Controller = BoundPickController.Get())
	{
		Controller->OnActorPicked.RemoveAll(this);
	}
	BoundPickController.Reset();
}

AFFZSFacilityActor* UFF_FacilityInfoSubsystem::ResolveLiveFacilityUnderCursor(
	APlayerController& Controller) const
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (!World || !ZoneSystem ||
		!Controller.DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FF_FacilityInfoPick), false);
	if (APawn* Pawn = Controller.GetPawn())
	{
		QueryParams.AddIgnoredActor(Pawn);
	}
	const FVector RayEnd = RayOrigin + RayDirection * 500000.0f;

	FHitResult SelectionProxyHit;
	const bool bHasSelectionProxyHit = World->LineTraceSingleByChannel(
		SelectionProxyHit,
		RayOrigin,
		RayEnd,
		FFZSCollision::FacilitySelectionTraceChannel,
		QueryParams);
	AFFZSFacilityActor* ProxyFacility = bHasSelectionProxyHit
		? Cast<AFFZSFacilityActor>(SelectionProxyHit.GetActor())
		: nullptr;
	const bool bProxyIsRegisteredFacility = IsValid(ProxyFacility) &&
		ProxyFacility->GetFacilityInstanceId().IsValid() &&
		ZoneSystem->FindFacility(ProxyFacility->GetFacilityInstanceId()) == ProxyFacility;

	bool bOccludedByExternalBlocker = false;
	if (bHasSelectionProxyHit && bProxyIsRegisteredFacility)
	{
		// The facility may gain Visibility-blocking helper components later. Ignore the entire
		// candidate actor and reject only an external blocker before its selection proxy hit.
		FCollisionQueryParams OcclusionParams = QueryParams;
		OcclusionParams.AddIgnoredActor(ProxyFacility);
		FHitResult OcclusionHit;
		bOccludedByExternalBlocker = World->LineTraceSingleByChannel(
			OcclusionHit,
			RayOrigin,
			SelectionProxyHit.ImpactPoint,
			ECC_Visibility,
			OcclusionParams);
	}

	switch (FFFacilityInfoSelection::ResolveTraceDecision(
		bHasSelectionProxyHit,
		bProxyIsRegisteredFacility,
		bOccludedByExternalBlocker))
	{
	case FFFacilityInfoSelection::ETraceDecision::UseSelectionProxy:
		return ProxyFacility;
	case FFFacilityInfoSelection::ETraceDecision::Reject:
		return nullptr;
	case FFFacilityInfoSelection::ETraceDecision::UseFootprintFallback:
	default:
		break;
	}

	// Compatibility path for installed actors that do not yet expose a selection proxy. The
	// first Visibility blocker must itself be a walkable ground surface, so walls and obstacles
	// cannot be crossed to reach a footprint behind them.
	FHitResult GroundHit;
	if (!World->LineTraceSingleByChannel(
			GroundHit,
			RayOrigin,
			RayEnd,
			ECC_Visibility,
			QueryParams) ||
		GroundHit.ImpactNormal.Z < 0.45f)
	{
		return nullptr;
	}
	return ZoneSystem->FindFacilityAtWorldPoint(GroundHit.ImpactPoint);
}

void UFF_FacilityInfoSubsystem::HandlePanelMoveRequested()
{
	if (SelectionContext == EFF_FacilityInfoContext::Build && SelectedFacilityId.IsValid())
	{
		OnMoveRequested.Broadcast(SelectedFacilityId);
	}
}

void UFF_FacilityInfoSubsystem::HandlePanelRemoveRequested()
{
	if (SelectionContext == EFF_FacilityInfoContext::Build && SelectedFacilityId.IsValid())
	{
		OnRemoveRequested.Broadcast(SelectedFacilityId);
	}
}

void UFF_FacilityInfoSubsystem::HandlePanelCloseRequested()
{
	const FGuid ClosingFacilityId = SelectedFacilityId;
	const EFF_FacilityInfoContext ClosingContext = SelectionContext;
	if (ClosingContext == EFF_FacilityInfoContext::Build && ClosingFacilityId.IsValid())
	{
		OnCloseRequested.Broadcast(ClosingFacilityId);
	}
	if (SelectedFacilityId == ClosingFacilityId && SelectionContext == ClosingContext)
	{
		ClearSelection();
	}
}

void UFF_FacilityInfoSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	if (Event.FacilityInstanceId != SelectedFacilityId)
	{
		return;
	}
	if (Event.Type == EFFZSCommandType::Remove)
	{
		ClearSelection();
		return;
	}
	// QueueBridge consumes the same world event. Refresh on the next subsystem tick so all
	// authority adapters have committed their matching read models before the card samples them.
	RefreshAccumulatorSeconds = RefreshIntervalSeconds;
}

void UFF_FacilityInfoSubsystem::HandleWorldSnapshotRestored()
{
	// Snapshot restore replaces actor generations even when a GUID is preserved.
	ClearSelection();
}

void UFF_FacilityInfoSubsystem::HandleActorPicked(AActor* PickedActor)
{
	// The camera delegate is only the already-gated click signal. Facility identity comes from
	// the dedicated selection trace below, so a null or unrelated camera hit is still meaningful.
	(void)PickedActor;
	AFF_FestivalPlayerController* Controller = BoundPickController.Get();
	if (!Controller || IsPointerOverPanel())
	{
		return;
	}
	if (!IsContextCurrentlyAllowed(EFF_FacilityInfoContext::Live))
	{
		if (SelectionContext == EFF_FacilityInfoContext::Live)
		{
			ClearSelection();
		}
		return;
	}

	AFFZSFacilityActor* Facility = ResolveLiveFacilityUnderCursor(*Controller);
	if (Facility && SelectFacility(Controller, Facility, EFF_FacilityInfoContext::Live))
	{
		return;
	}
	if (SelectionContext == EFF_FacilityInfoContext::Live)
	{
		ClearSelection();
	}
}

#undef LOCTEXT_NAMESPACE
