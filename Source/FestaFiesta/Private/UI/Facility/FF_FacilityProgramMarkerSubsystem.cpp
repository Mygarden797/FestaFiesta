// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityProgramMarkerSubsystem.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"

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
	}
	if (BoundStage)
	{
		BoundStage->OnStageChanged.RemoveAll(this);
		BoundStage->OnFestivalStarted.RemoveAll(this);
	}
	if (UFFZSWorldSubsystem* Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
	{
		Zones->OnWorldChanged.RemoveAll(this);
		Zones->OnWorldSnapshotRestored.RemoveAll(this);
	}
	BoundProgression = nullptr;
	BoundStage = nullptr;
	BoundReservations = nullptr;
	MarkerStates.Reset();
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
		}
		BoundProgression = Progression;
		if (BoundProgression)
		{
			BoundProgression->OnScheduledProgramsChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
			BoundProgression->OnFestivalDefinitionChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
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

	// Screen widget components track the camera themselves. Only mesh bounds need polling:
	// a soft mesh can finish loading after the install event, and upgrades may change its height.
	LocationRefreshSeconds += FMath::Max(0.f, DeltaTime);
	if (LocationRefreshSeconds >= .2f)
	{
		LocationRefreshSeconds = 0.f;
		UpdateMarkerLocations();
	}
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

void UFF_FacilityProgramMarkerSubsystem::RefreshMarkerStates()
{
	bStateDirty = false;
	MarkerStates.Reset();
	const UFFZSWorldSubsystem* Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	if (!Zones) return;
	CachedRegistryRevision = Zones->GetFacilityRegistryRevision();
	if (!BoundProgression) return;

	const TArray<FFF_ProgramCard> Catalog = BoundProgression->GetProgramCatalog();
	if (Catalog.IsEmpty()) return;
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
		const AFFZSFacilityActor* Actor = Zones->FindFacility(Facility.FacilityInstanceId);
		if (!IsValid(Actor) || Actor->GetFacilityState() == EFFZSFacilityState::Removed) continue;
		FFF_FacilityNameParts Parts;
		FString Failure;
		if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			Facility.Category, Facility.FacilityDefinitionId, Parts, Failure)) continue;
		const bool bCompatible = Catalog.ContainsByPredicate([&Parts](const FFF_ProgramCard& Card)
		{
			return Card.IsValid() && FFF_ProgramScheduleRules::IsFacilityTypeAllowed(
				Card, Parts.MajorToken, Parts.MinorToken);
		});
		if (bCompatible)
		{
			MarkerStates.Add(Facility.FacilityInstanceId,
				ScheduledFacilities.Contains(Facility.FacilityInstanceId)
					? EFF_FacilityProgramMarkerState::Scheduled : EFF_FacilityProgramMarkerState::Available);
		}
	}
}

void UFF_FacilityProgramMarkerSubsystem::RefreshPresentation(APlayerController* Controller)
{
	UFFZSWorldSubsystem* Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	for (auto It = Markers.CreateIterator(); It; ++It)
	{
		UWidgetComponent* Marker = It.Value();
		if (!MarkerStates.Contains(It.Key()) || !IsValid(Marker) || !Zones ||
			Zones->FindFacility(It.Key()) != Marker->GetOwner())
		{
			if (IsValid(Marker)) Marker->DestroyComponent();
			It.RemoveCurrent();
		}
	}
	if (!Controller || !Zones) return;
	for (const auto& Entry : MarkerStates)
	{
		AFFZSFacilityActor* Facility = Zones->FindFacility(Entry.Key);
		if (!IsValid(Facility)) continue;
		UWidgetComponent* Marker = Markers.FindRef(Entry.Key);
		if (!IsValid(Marker))
		{
			UFF_FacilityProgramMarkerWidget* Widget = CreateWidget<UFF_FacilityProgramMarkerWidget>(
				Controller, UFF_FacilityProgramMarkerWidget::StaticClass());
			if (!Widget) continue;
			Marker = NewObject<UWidgetComponent>(Facility, NAME_None, RF_Transient);
			Marker->SetupAttachment(Facility->GetRootComponent());
			Marker->SetWidgetSpace(EWidgetSpace::Screen);
			Marker->SetDrawSize(FVector2D(156.f, 34.f));
			Marker->SetPivot(FVector2D(.5f, 1.f));
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Marker->SetGenerateOverlapEvents(false);
			Marker->SetCanEverAffectNavigation(false);
			Marker->SetOwnerPlayer(Controller->GetLocalPlayer());
			Marker->SetWidget(Widget);
			Marker->RegisterComponent();
			Markers.Add(Entry.Key, Marker);
		}
		if (UFF_FacilityProgramMarkerWidget* Widget = Cast<UFF_FacilityProgramMarkerWidget>(
			Marker->GetUserWidgetObject()))
		{
			Widget->SetMarkerState(Entry.Value);
		}
	}
	UpdateMarkerLocations();
}

void UFF_FacilityProgramMarkerSubsystem::UpdateMarkerLocations()
{
	for (const auto& Entry : Markers)
	{
		UWidgetComponent* Marker = Entry.Value;
		AFFZSFacilityActor* Facility = IsValid(Marker) ? Cast<AFFZSFacilityActor>(Marker->GetOwner()) : nullptr;
		if (!IsValid(Facility)) continue;
		Marker->SetVisibility(!Facility->IsHidden());
		const UPrimitiveComponent* BoundsComponent = Facility->VisualMesh && Facility->VisualMesh->GetStaticMesh()
			? static_cast<UPrimitiveComponent*>(Facility->VisualMesh.Get())
			: static_cast<UPrimitiveComponent*>(Facility->SelectionProxy.Get());
		const FVector Location = BoundsComponent
			? BoundsComponent->Bounds.Origin + FVector(0.f, 0.f, BoundsComponent->Bounds.BoxExtent.Z + 70.f)
			: Facility->GetActorLocation() + FVector(0.f, 0.f, 250.f);
		Marker->SetWorldLocation(Location);
	}
}

void UFF_FacilityProgramMarkerSubsystem::ClearPresentation()
{
	for (const auto& Entry : Markers)
	{
		if (IsValid(Entry.Value)) Entry.Value->DestroyComponent();
	}
	Markers.Reset();
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
