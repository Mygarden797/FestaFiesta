// FF_FacilityProgramMarkerSubsystem.cpp
// 역할: 월드에 설치한 시설 위의 프로그램 표시를 생성·갱신·정리한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityProgramMarkerSubsystem.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "Building/Synergy/FF_FacilitySynergySubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/PanelWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"

namespace
{
	bool IsPresented(const UWidget* Widget)
	{
		// A nested panel can report Visible while its stage HUD or tab host is collapsed.
		for (const UWidget* Current = Widget; Current;)
		{
			if (!Current->IsVisible()) return false;
			if (const UUserWidget* UserWidget = Cast<UUserWidget>(Current); UserWidget && UserWidget->IsInViewport()) return true;
			if (const UPanelWidget* Parent = Current->GetParent()) Current = Parent;
			else Current = Current->GetTypedOuter<UUserWidget>();
		}
		return false;
	}
}

bool FFFacilityWorldBadgeLayout::Place(const FVector2D& Anchor, const FVector2D& Size,
	const FBox2D& SafeBounds, const TArray<FBox2D>& Occupied, float Separation,
	float MaximumDisplacement, FVector2D& OutPosition)
{
	if (!SafeBounds.bIsValid || Size.X <= 0 || Size.Y <= 0 ||
		Size.X > SafeBounds.GetSize().X || Size.Y > SafeBounds.GetSize().Y) return false;
	const FVector2D Desired(Anchor.X - Size.X * .5, Anchor.Y - Size.Y - 8.f);
	const float Gap = FMath::Max(0.f, Separation);
	const float Limit = FMath::Max(0.f, MaximumDisplacement);
	auto Try = [&](FVector2D Position)
	{
		Position.X = FMath::Clamp(Position.X, SafeBounds.Min.X, SafeBounds.Max.X - Size.X);
		Position.Y = FMath::Clamp(Position.Y, SafeBounds.Min.Y, SafeBounds.Max.Y - Size.Y);
		// 제곱 거리끼리 비교하면 같은 배치 결과를 제곱근 계산 없이 얻는다.
		if ((Position - Desired).SizeSquared() > FMath::Square(Limit)) return false;
		const FBox2D Candidate(Position - FVector2D(Gap * .5), Position + Size + FVector2D(Gap * .5));
		for (const FBox2D& Rect : Occupied) if (Candidate.Intersect(Rect)) return false;
		OutPosition = Position;
		return true;
	};
	if (Try(Desired)) return true;
	// Bounded and stable: avoid bouncing between arbitrary map iteration orders or distant facilities.
	for (int32 Ring = 1; Ring <= 3; ++Ring)
	{
		const double Y = (Size.Y + Gap) * Ring;
		if (Try(Desired + FVector2D(0, -Y)) || Try(Desired + FVector2D(0, Y))) return true;
		for (int32 Side : { -1, 1 })
		{
			const double X = Side * (Size.X + Gap) * .6 * Ring;
			if (Try(Desired + FVector2D(X, 0)) || Try(Desired + FVector2D(X, -Y)) ||
				Try(Desired + FVector2D(X, Y))) return true;
		}
	}
	return false;
}

FSoftClassPath UFF_FacilityProgramMarkerSubsystem::GetDefaultWidgetClassPath()
{
	return FSoftClassPath(FFBuildUIAssetPaths::FacilityWorldBadgeClass);
}

void UFF_FacilityProgramMarkerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);
	if (UFFZSWorldSubsystem* Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
	{
		Zones->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldChanged);
		Zones->OnWorldSnapshotRestored.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
	}
	EnsureReadModelBindings();
}

void UFF_FacilityProgramMarkerSubsystem::Deinitialize()
{
	ClearPresentation();
	if (BoundProgression)
	{
		BoundProgression->OnScheduledProgramsChanged.RemoveAll(this);
		BoundProgression->OnFestivalDefinitionChanged.RemoveAll(this);
		BoundProgression->OnFacilityRevenueChanged.RemoveAll(this);
		BoundProgression->OnTodayRevenueChanged.RemoveAll(this);
	}
	if (BoundStage)
	{
		BoundStage->OnStageChanged.RemoveAll(this);
		BoundStage->OnFestivalStarted.RemoveAll(this);
	}
	if (BoundRevenueTable)
	{
		BoundRevenueTable->OnDataTableChanged().RemoveAll(this);
	}
	if (UFFZSWorldSubsystem* Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
	{
		Zones->OnWorldChanged.RemoveAll(this);
		Zones->OnWorldSnapshotRestored.RemoveAll(this);
	}
	BoundProgression = nullptr;
	BoundStage = nullptr;
	BoundReservations = nullptr;
	BoundRevenueTable = nullptr;
	MarkerStates.Reset();
	BadgeStates.Reset();
	MarkerActors.Reset();
	MarkerWidgetClass = nullptr;
	Super::Deinitialize();
}

bool UFF_FacilityProgramMarkerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UFF_FacilityProgramMarkerSubsystem::EnsureReadModelBindings()
{
	UGameInstance* Game = GetWorld()->GetGameInstance();
	UFF_FestivalProgressionSubsystem* Progression = Game
		? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (BoundProgression != Progression)
	{
		if (BoundProgression)
		{
			BoundProgression->OnScheduledProgramsChanged.RemoveAll(this);
			BoundProgression->OnFestivalDefinitionChanged.RemoveAll(this);
			BoundProgression->OnFacilityRevenueChanged.RemoveAll(this);
			BoundProgression->OnTodayRevenueChanged.RemoveAll(this);
		}
		BoundProgression = Progression;
		if (BoundProgression)
		{
			BoundProgression->OnScheduledProgramsChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
			BoundProgression->OnFestivalDefinitionChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
			BoundProgression->OnFacilityRevenueChanged.AddUniqueDynamic(this, &ThisClass::HandleRevenueChanged);
			BoundProgression->OnTodayRevenueChanged.AddUniqueDynamic(this, &ThisClass::HandleAmountChanged);
		}
		bStateDirty = true;
	}
	UFF_FestivalStageSubsystem* Stage = Game ? Game->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (BoundStage != Stage)
	{
		if (BoundStage)
		{
			BoundStage->OnStageChanged.RemoveAll(this);
			BoundStage->OnFestivalStarted.RemoveAll(this);
		}
		BoundStage = Stage;
		if (BoundStage)
		{
			BoundStage->OnStageChanged.AddUniqueDynamic(this, &ThisClass::HandleStageChanged);
			BoundStage->OnFestivalStarted.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
		}
		bStateDirty = true;
	}
	UFF_ProgramReservationSubsystem* Reservations = Game
		? Game->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
	if (BoundReservations != Reservations)
	{
		BoundReservations = Reservations;
		bStateDirty = true;
	}
	const UFF_FestivalDefinition* Definition = BoundProgression ? BoundProgression->GetDefinition() : nullptr;
	UDataTable* RevenueTable = Definition ? Definition->GetFacilityRevenueTable() : nullptr;
	if (BoundRevenueTable != RevenueTable)
	{
		if (BoundRevenueTable)
		{
			BoundRevenueTable->OnDataTableChanged().RemoveAll(this);
		}
		BoundRevenueTable = RevenueTable;
		if (BoundRevenueTable)
		{
			BoundRevenueTable->OnDataTableChanged().AddUObject(this, &ThisClass::HandleStateChanged);
		}
		bStateDirty = true;
	}
}

void UFF_FacilityProgramMarkerSubsystem::Tick(const float DeltaTime)
{
	EnsureReadModelBindings();
	const UFFZSWorldSubsystem* Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	if (Zones && CachedRegistryRevision != Zones->GetFacilityRegistryRevision())
	{
		// Direct registration/unregistration also changes this revision, including restored actors.
		bStateDirty = true;
	}
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (!IsValid(Controller) || !Controller->IsLocalController() || !Controller->GetLocalPlayer())
	{
		Controller = nullptr;
	}
	const bool bControllerChanged = OwningController.Get() != Controller;
	if (bControllerChanged)
	{
		ClearPresentation();
		OwningController = Controller;
	}
	if (bStateDirty)
	{
		RefreshMarkerStates();
		RefreshPresentation(Controller);
	}
	else if (bControllerChanged)
	{
		RefreshPresentation(Controller);
	}
	// 표시할 배지가 없으면 전체 화면 위젯 검색과 좌표 계산도 필요 없다.
	if (Markers.IsEmpty()) return;

	// 매 프레임에는 화면 좌표만 갱신하고 수입과 프로그램 정보는 변경 이벤트로 갱신한다.
	OcclusionRefreshSeconds += FMath::Max(0.f, DeltaTime);
	if (OcclusionRefreshSeconds >= .2f || bControllerChanged)
	{
		OcclusionRefreshSeconds = 0.f;
		RefreshOcclusionWidgets();
	}
	UpdateMarkerLocations();
}

TStatId UFF_FacilityProgramMarkerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FacilityProgramMarkerSubsystem, STATGROUP_Tickables);
}

EFF_FacilityProgramMarkerState UFF_FacilityProgramMarkerSubsystem::GetMarkerState(
	const FGuid FacilityInstanceId) const
{
	const EFF_FacilityProgramMarkerState* State = MarkerStates.Find(FacilityInstanceId);
	return State ? *State : EFF_FacilityProgramMarkerState::Hidden;
}

bool UFF_FacilityProgramMarkerSubsystem::GetBadgeData(FGuid FacilityInstanceId, FFF_FacilityWorldBadgeData& OutData) const
{
	if (const FFF_FacilityWorldBadgeData* Data = BadgeStates.Find(FacilityInstanceId))
	{
		OutData = *Data;
		return true;
	}
	OutData = {};
	return false;
}

void UFF_FacilityProgramMarkerSubsystem::RefreshMarkerStates()
{
	bStateDirty = false;
	MarkerStates.Reset();
	BadgeStates.Reset();
	MarkerActors.Reset();
	const UFFZSWorldSubsystem* Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	if (!Zones) return;
	CachedRegistryRevision = Zones->GetFacilityRegistryRevision();
	if (!BoundProgression) return;

	const TArray<FFF_ProgramCard> Catalog = BoundProgression->GetProgramCatalog();
	const UFF_FestivalDefinition* Definition = BoundProgression->GetDefinition();
	const UDataTable* RevenueTable = Definition ? Definition->GetFacilityRevenueTable() : nullptr;
	const bool bEconomyAvailable = Definition != nullptr;
	const UFF_FacilitySynergySubsystem* Synergies = GetWorld()->GetSubsystem<UFF_FacilitySynergySubsystem>();
	TSet<FGuid> ScheduledFacilities;
	if (BoundReservations)
	{
		for (const FFF_ScheduledProgram& Program : BoundProgression->GetScheduledPrograms())
		{
			FFF_ProgramReservation Reservation;
			if (BoundReservations->FindReservation(Program.ReservationId, Reservation))
			{
				ScheduledFacilities.Add(Reservation.FacilityInstanceId);
			}
		}
	}
	// Query the authority registry once per change; spawned preview actors never enter this list.
	for (const FFFZSFacilityReadModel& Facility : Zones->BuildReadModel().Facilities)
	{
		AFFZSFacilityActor* Actor = Zones->FindFacility(Facility.FacilityInstanceId);
		if (!IsValid(Actor) || Actor->GetFacilityState() == EFFZSFacilityState::Removed) continue;
		FFF_FacilityNameParts Parts;
		FString Failure;
		const bool bNamed = UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			Facility.Category, Facility.FacilityDefinitionId, Parts, Failure);
		const bool bCompatible = bNamed && Catalog.ContainsByPredicate([&Parts](const FFF_ProgramCard& Card)
		{
			return Card.IsValid() && FFF_ProgramScheduleRules::IsFacilityTypeAllowed(
				Card, Parts.MajorToken, Parts.MinorToken);
		});
		FFF_FacilityWorldBadgeData Badge;
		if (bCompatible)
		{
			Badge.ProgramState = ScheduledFacilities.Contains(Facility.FacilityInstanceId)
				? EFF_FacilityProgramMarkerState::Scheduled : EFF_FacilityProgramMarkerState::Available;
			MarkerStates.Add(Facility.FacilityInstanceId, Badge.ProgramState);
		}
		FFF_FacilityInstanceRevenue Revenue;
		const bool bHasLedger = BoundProgression->GetFacilityInstanceRevenue(Facility.FacilityInstanceId, Revenue);
		// Reuse the settlement rules directly. Upgrade previews can validate a future footprint;
		// displaying today's ledger must never trigger placement validation for every facility.
		const int32 RevenuePerUse = Definition ? FFF_BoothRevenueRules::ResolveRevenuePerUse(
			Actor->GetFacilitySpec(), Definition->RevenuePerUseByMinorToken, RevenueTable) : 0;
		const float SynergyMultiplier = Synergies ? Synergies->GetRevenueMultiplier(Facility.FacilityInstanceId) : 1.f;
		const float CommissionRate = Definition ? FFF_BoothRevenueRules::ResolveCommissionRate(
			Actor->GetFacilitySpec(), Definition->BoothCommissionRate, RevenueTable) : 0.f;
		const float RevenueMultiplier = FFF_BoothRevenueRules::ResolveRevenueMultiplier(
			Actor->GetFacilitySpec(), Actor->GetUpgradeLevel(), Actor->GetUpgradeRevenueMultiplier(), RevenueTable);
		const bool bDirectRevenue = Definition && FFF_BoothRevenueRules::ResolveCommission(
			RevenuePerUse, 1, CommissionRate, static_cast<double>(RevenueMultiplier) * SynergyMultiplier) > 0;
		Badge.bShowRevenue = bCompatible || bHasLedger || bDirectRevenue;
		Badge.bEconomyAvailable = bEconomyAvailable;
		Badge.TodayGold = bEconomyAvailable ? Revenue.TodayGold : 0;
		if (Badge.ProgramState != EFF_FacilityProgramMarkerState::Hidden || Badge.bShowRevenue)
		{
			BadgeStates.Add(Facility.FacilityInstanceId, Badge);
			MarkerActors.Add(Facility.FacilityInstanceId, Actor);
		}
	}
}

void UFF_FacilityProgramMarkerSubsystem::RefreshPresentation(APlayerController* Controller)
{
	for (auto It = Markers.CreateIterator(); It; ++It)
	{
		UFF_FacilityProgramMarkerWidget* Marker = It.Value();
		if (!BadgeStates.Contains(It.Key()) || !IsValid(Marker))
		{
			if (IsValid(Marker)) Marker->RemoveFromParent();
			It.RemoveCurrent();
			bMarkerOrderDirty = true;
		}
	}
	if (!Controller) return;
	if (!MarkerWidgetClass)
	{
		MarkerWidgetClass = GetDefaultWidgetClassPath().TryLoadClass<UFF_FacilityProgramMarkerWidget>();
		if (!MarkerWidgetClass) MarkerWidgetClass = UFF_FacilityProgramMarkerWidget::StaticClass();
	}
	for (const auto& Entry : BadgeStates)
	{
		UFF_FacilityProgramMarkerWidget* Marker = Markers.FindRef(Entry.Key);
		if (!IsValid(Marker))
		{
			Marker = CreateWidget<UFF_FacilityProgramMarkerWidget>(Controller, MarkerWidgetClass);
			if (!Marker) continue;
			Marker->SetAlignmentInViewport(FVector2D::ZeroVector);
			Marker->AddToViewport(-10);
			Markers.Add(Entry.Key, Marker);
			bMarkerOrderDirty = true;
		}
		Marker->SetMarkerState(Entry.Value.ProgramState);
		Marker->SetRevenue(Entry.Value.bShowRevenue, Entry.Value.bEconomyAvailable, Entry.Value.TodayGold);
		Marker->ForceLayoutPrepass();
	}
	RefreshOcclusionWidgets();
}

void UFF_FacilityProgramMarkerSubsystem::RefreshOcclusionWidgets()
{
	OcclusionWidgets.Reset();
	if (!OwningController.IsValid() || Markers.IsEmpty()) return;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		if (!Widget || Widget->GetOwningPlayer() != OwningController.Get()) continue;
		if (Widget->IsA<UFF_FacilityInfoPanel>() || Widget->IsA<UFF_FacilityRevenueRankingPanel>())
		{
			// These screens have transparent fullscreen roots. Only the visible paper blocks badges.
			if (UWidget* Surface = Widget->GetWidgetFromName(TEXT("Border_Panel"))) OcclusionWidgets.Add(Surface);
		}
		else if (Widget->IsA<UFF_FacilityPaidProgramPicker>())
		{
			if (UWidget* Surface = Widget->GetWidgetFromName(TEXT("Border_Picker"))) OcclusionWidgets.Add(Surface);
		}
		else if (Widget->IsA<UFF_PrepPanelBase>() || Widget->IsA<UFF_LivePanelBase>())
		{
			// Authored panels can have a fullscreen transparent Canvas root. The paper surface is
			// the exclusion, never the UUserWidget's full allocated rectangle. Its own collapsed
			// state also removes the catalog exclusion while separate placement tools are shown.
			if (UWidget* Surface = Widget->GetWidgetFromName(TEXT("Border_PanelRoot"))) OcclusionWidgets.Add(Surface);
			else
			{
				for (const FName Name : { TEXT("Panel_Catalog"), TEXT("Panel_PlacementCompact"), TEXT("Border_Panel") })
					if (UWidget* InnerSurface = Widget->GetWidgetFromName(Name)) OcclusionWidgets.Add(InnerSurface);
			}
		}
	}
}

void UFF_FacilityProgramMarkerSubsystem::RefreshMarkerOrder(const FGuid SelectedFacilityId)
{
	if (!bMarkerOrderDirty && OrderedSelection == SelectedFacilityId) return;

	if (bMarkerOrderDirty)
	{
		MarkerOrder.Reset(Markers.Num());
		Markers.GetKeys(MarkerOrder);
	}
	// 선택한 시설에 먼저 공간을 배정하고 나머지는 고유 번호 순서로 고정해 깜빡임을 막는다.
	MarkerOrder.Sort([SelectedFacilityId](const FGuid& A, const FGuid& B)
	{
		if ((A == SelectedFacilityId) != (B == SelectedFacilityId)) return A == SelectedFacilityId;
		return A < B;
	});
	OrderedSelection = SelectedFacilityId;
	bMarkerOrderDirty = false;
}

void UFF_FacilityProgramMarkerSubsystem::UpdateMarkerLocations()
{
	APlayerController* Controller = OwningController.Get();
	if (!Controller) return;
	const UFF_FacilityCodexPresenterSubsystem* Codex = GetWorld()->GetSubsystem<UFF_FacilityCodexPresenterSubsystem>();
	const UFF_FestivalSettingsSubsystem* Settings = UFF_FestivalSettingsSubsystem::Get(GetWorld());
	const bool bModalOpen = (Codex && Codex->IsCodexOpen()) || (Settings && Settings->IsPauseMenuOpen());
	const FGeometry ViewportGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(GetWorld());
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(GetWorld()) /
		FMath::Max(.01f, UWidgetLayoutLibrary::GetViewportScale(GetWorld()));
	TArray<FBox2D> Occupied;
	Occupied.Reserve(OcclusionWidgets.Num() + Markers.Num());
	for (const TWeakObjectPtr<UWidget>& WeakWidget : OcclusionWidgets)
	{
		const UWidget* Widget = WeakWidget.Get();
		if (!Widget || !IsPresented(Widget)) continue;
		const FGeometry Geometry = Widget->GetCachedGeometry();
		if (Geometry.GetLocalSize().GetMin() <= 0) continue;
		Occupied.Add(FBox2D(ViewportGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector)),
			ViewportGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()))));
	}
	const UFF_FacilityInfoSubsystem* Info = GetWorld()->GetSubsystem<UFF_FacilityInfoSubsystem>();
	const AFF_FestivalPlayerController* FestivalController = Cast<AFF_FestivalPlayerController>(Controller);
	const bool bGroundMode = FestivalController && FestivalController->GetViewMode() == EFF_CameraViewMode::Character;
	// 그라운드 모드에서는 현재 커서가 가리키는 시설만 표시한다. 클릭 선택은 포커스를 유지하지 않는다.
	const AFFZSFacilityActor* FocusedFacility = bGroundMode && !bModalOpen && Info &&
		FestivalController->IsPickingAllowed() && Controller->bShowMouseCursor && !Info->IsPointerOverPanel()
		? Info->ResolveFacilityUnderCursor(*Controller) : nullptr;
	const FGuid Selected = Info ? Info->GetSelectedFacilityId() : FGuid();
	RefreshMarkerOrder(Selected);
	for (const FGuid& Id : MarkerOrder)
	{
		UFF_FacilityProgramMarkerWidget* Marker = Markers.FindRef(Id);
		AFFZSFacilityActor* Facility = MarkerActors.FindRef(Id).Get();
		if (!IsValid(Marker)) continue;
		if (bModalOpen || !IsValid(Facility) || Facility->IsHidden() ||
			(bGroundMode && Facility != FocusedFacility))
		{
			Marker->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}
		const UPrimitiveComponent* BoundsComponent = Facility->VisualMesh && Facility->VisualMesh->GetStaticMesh()
			? static_cast<UPrimitiveComponent*>(Facility->VisualMesh.Get())
			: static_cast<UPrimitiveComponent*>(Facility->SelectionProxy.Get());
		const FVector Location = BoundsComponent
			? BoundsComponent->Bounds.Origin + FVector(0.f, 0.f, BoundsComponent->Bounds.BoxExtent.Z + 70.f)
			: Facility->GetActorLocation() + FVector(0.f, 0.f, 250.f);
		FVector2D Anchor;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Location, Anchor, false) ||
			!FMath::IsFinite(Anchor.X) || !FMath::IsFinite(Anchor.Y) || Anchor.X < 0 || Anchor.Y < 0 ||
			Anchor.X > ViewportSize.X || Anchor.Y > ViewportSize.Y)
		{
			Marker->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}
		const FMargin Safe = Marker->GetViewportSafePadding();
		const FBox2D SafeBounds(FVector2D(Safe.Left, Safe.Top), ViewportSize - FVector2D(Safe.Right, Safe.Bottom));
		const FVector2D Size = Marker->GetDesiredSize();
		FVector2D Position;
		if (!FFFacilityWorldBadgeLayout::Place(Anchor, Size, SafeBounds, Occupied,
			Marker->GetBadgeSeparation(), Marker->GetMaximumDisplacement(), Position))
		{
			Marker->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}
		Marker->SetDesiredSizeInViewport(Size);
		Marker->SetPositionInViewport(Position, false);
		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
		Occupied.Add(FBox2D(Position, Position + Size));
	}
}

void UFF_FacilityProgramMarkerSubsystem::ClearPresentation()
{
	for (const auto& Entry : Markers)
	{
		if (IsValid(Entry.Value)) Entry.Value->RemoveFromParent();
	}
	Markers.Reset();
	MarkerOrder.Reset();
	OrderedSelection.Invalidate();
	bMarkerOrderDirty = true;
	OcclusionWidgets.Reset();
	OwningController.Reset();
}

void UFF_FacilityProgramMarkerSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	bStateDirty = true;
}

void UFF_FacilityProgramMarkerSubsystem::HandleStateChanged()
{
	bStateDirty = true;
}

void UFF_FacilityProgramMarkerSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage, const EFF_FestivalStage NewStage)
{
	bStateDirty = true;
}

void UFF_FacilityProgramMarkerSubsystem::HandleRevenueChanged(FGuid FacilityInstanceId)
{
	bStateDirty = true;
}

void UFF_FacilityProgramMarkerSubsystem::HandleAmountChanged(int32 Amount)
{
	// Also handles the daily zero reset, which intentionally has no per-facility event.
	bStateDirty = true;
}
