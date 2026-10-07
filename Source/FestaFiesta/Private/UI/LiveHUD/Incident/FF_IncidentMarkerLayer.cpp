// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentMarkerLayer.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/Rules/FF_IncidentRules.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"
#include "UI/LiveHUD/Incident/FF_IncidentMarkerButton.h"

TSharedRef<SWidget> UFF_IncidentMarkerLayer::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_Markers"));
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}

void UFF_IncidentMarkerLayer::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	LoadVisuals();
	BindIncidents();
	RefreshAll();
}

void UFF_IncidentMarkerLayer::NativeDestruct()
{
	UnbindIncidents();
	ClearMarkers();
	Super::NativeDestruct();
}

void UFF_IncidentMarkerLayer::LoadVisuals()
{
	LoadedQuestGlyph = QuestGlyph.LoadSynchronous();
	LoadedInfoGlyph = InfoGlyph.LoadSynchronous();
	LoadedDispatchedGlyph = DispatchedGlyph.LoadSynchronous();
	LoadedRingMaterial = RingMaterial.LoadSynchronous();

	if (!LoadedQuestGlyph || !LoadedInfoGlyph || !LoadedDispatchedGlyph || !LoadedRingMaterial)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 사건 마커 에셋 일부를 찾지 못했다 (Quest=%d Info=%d Dispatched=%d Ring=%d). Tools/ui/incident_marker/apply_and_verify.py 를 실행했는지 확인한다."),
			*GetName(), LoadedQuestGlyph != nullptr, LoadedInfoGlyph != nullptr, LoadedDispatchedGlyph != nullptr, LoadedRingMaterial != nullptr);
	}
}

UFF_IncidentSubsystem* UFF_IncidentMarkerLayer::GetIncidentSubsystem() const
{
	if (CachedIncident)
	{
		return CachedIncident;
	}

	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFF_IncidentSubsystem>() : nullptr;
}

UFF_FestivalStageSubsystem* UFF_IncidentMarkerLayer::GetFestivalStageSubsystem() const
{
	if (CachedStage)
	{
		return CachedStage;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
}

bool UFF_IncidentMarkerLayer::TryGetNowMinutes(float& OutMinutes) const
{
	const UFF_FestivalStageSubsystem* Stage = GetFestivalStageSubsystem();
	return Stage && Stage->TryGetLiveElapsedMinutes(OutMinutes);
}

void UFF_IncidentMarkerLayer::BindIncidents()
{
	UFF_IncidentSubsystem* Incident = GetIncidentSubsystem();
	if (!Incident)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 사건 서브시스템을 찾지 못했다. 사건 아이콘이 뜨지 않는다."), *GetName());
		return;
	}

	CachedIncident = Incident;

	Incident->OnIncidentRaised.AddUniqueDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentRaised);
	Incident->OnIncidentUpdated.AddUniqueDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentUpdated);
	Incident->OnIncidentClosed.AddUniqueDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentClosed);

	if (UFF_FestivalStageSubsystem* Stage = GetFestivalStageSubsystem())
	{
		CachedStage = Stage;
		Stage->OnRemainingSecondsChanged.AddUniqueDynamic(this, &UFF_IncidentMarkerLayer::HandleClockSecond);
	}
}

void UFF_IncidentMarkerLayer::UnbindIncidents()
{
	if (CachedIncident)
	{
		CachedIncident->OnIncidentRaised.RemoveDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentRaised);
		CachedIncident->OnIncidentUpdated.RemoveDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentUpdated);
		CachedIncident->OnIncidentClosed.RemoveDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentClosed);
	}
	if (CachedStage)
	{
		CachedStage->OnRemainingSecondsChanged.RemoveDynamic(this, &UFF_IncidentMarkerLayer::HandleClockSecond);
	}
	CachedIncident = nullptr;
	CachedStage = nullptr;
}

void UFF_IncidentMarkerLayer::RefreshAll()
{
	ClearMarkers();

	if (const UFF_IncidentSubsystem* Incident = GetIncidentSubsystem())
	{
		TArray<FFF_IncidentHandle> Active;
		Incident->GetActiveHandle(Active);
		for (const FFF_IncidentHandle& Handle : Active)
		{
			AddOrRefreshMarker(Handle);
		}
	}
}

void UFF_IncidentMarkerLayer::HandleIncidentRaised(FFF_IncidentHandle Handle)
{
	AddOrRefreshMarker(Handle);
}

void UFF_IncidentMarkerLayer::HandleIncidentUpdated(FFF_IncidentHandle Handle)
{
	AddOrRefreshMarker(Handle);
}

void UFF_IncidentMarkerLayer::HandleIncidentClosed(FFF_IncidentHandle Handle)
{
	RemoveMarker(Handle);
}

void UFF_IncidentMarkerLayer::HandleClockSecond(int32 RemainingSeconds)
{
	if (Markers.Num() == 0)
	{
		return;
	}
	float Now = 0.f;
	const bool bHasNow = TryGetNowMinutes(Now);
	for (TPair<FFF_IncidentHandle, FMarker>& Pair : Markers)
	{
		ApplyLook(Pair.Value, bHasNow, Now);
	}
}

void UFF_IncidentMarkerLayer::HandleMarkerClicked(FFF_IncidentHandle Handle)
{
	OnIncidentMarkerClicked.Broadcast(Handle);
}

UTexture2D* UFF_IncidentMarkerLayer::GetGlyphTexture(EFF_IncidentMarkerGlyph Glyph) const
{
	switch (Glyph)
	{
	case EFF_IncidentMarkerGlyph::Info:       return LoadedInfoGlyph;
	case EFF_IncidentMarkerGlyph::Dispatched: return LoadedDispatchedGlyph;
	case EFF_IncidentMarkerGlyph::Quest:
	default:                                  return LoadedQuestGlyph;
	}
}

void UFF_IncidentMarkerLayer::ApplyLook(FMarker& Marker, bool bHasNow, float NowMinutes)
{
	UFF_IncidentMarkerButton* Button = Marker.Button.Get();
	if (!Button)
	{
		return;
	}
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	const FFF_UIPaletteInGame Colors = Palette ? Palette->InGame : FFF_UIPaletteInGame();
	const FFF_IncidentMarkerLook Look = FFIncidentMarkerRules::Resolve(Marker.View, bHasNow, NowMinutes, Colors);
	Button->ApplyLook(Look, GetGlyphTexture(Look.Glyph));
}

void UFF_IncidentMarkerLayer::AddOrRefreshMarker(FFF_IncidentHandle Handle)
{
	const UFF_IncidentSubsystem* Incident = GetIncidentSubsystem();
	if (!Incident || !Canvas || !WidgetTree)
	{
		return;
	}

	FFF_IncidentView View;
	if (!Incident->GetIncidentView(Handle, View))
	{
		return;
	}

	float Now = 0.f;
	const bool bHasNow = TryGetNowMinutes(Now);

	if (FMarker* Existing = Markers.Find(Handle))
	{
		Existing->View = MoveTemp(View);
		ApplyLook(*Existing, bHasNow, Now);
		return;
	}

	UFF_IncidentMarkerButton* Button = CreateWidget<UFF_IncidentMarkerButton>(this, UFF_IncidentMarkerButton::StaticClass());
	if (!Button)
	{
		return;
	}

	Button->SetHandle(Handle);
	Button->OnMarkerClicked.BindUObject(this, &UFF_IncidentMarkerLayer::HandleMarkerClicked);
	Button->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* MarkerSlot = Canvas->AddChildToCanvas(Button))
	{
		MarkerSlot->SetAnchors(FAnchors(0.f, 0.f));
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MarkerSlot->SetAutoSize(false);
		MarkerSlot->SetSize(IconSize);
	}
	Button->SetMarkerSize(IconSize);
	Button->SetRingMaterial(LoadedRingMaterial);

	FMarker& Marker = Markers.Add(Handle);
	Marker.Button = Button;
	Marker.View = MoveTemp(View);
	ApplyLook(Marker, bHasNow, Now);
}

void UFF_IncidentMarkerLayer::RemoveMarker(FFF_IncidentHandle Handle)
{
	FMarker Marker;
	if (Markers.RemoveAndCopyValue(Handle, Marker))
	{
		if (UFF_IncidentMarkerButton* Button = Marker.Button.Get())
		{
			Button->OnMarkerClicked.Unbind();
			Button->RemoveFromParent();
		}
	}
}

void UFF_IncidentMarkerLayer::ClearMarkers()
{
	for (TPair<FFF_IncidentHandle, FMarker>& Pair : Markers)
	{
		if (UFF_IncidentMarkerButton* Button = Pair.Value.Button.Get())
		{
			Button->OnMarkerClicked.Unbind();
			Button->RemoveFromParent();
		}
	}
	Markers.Reset();
}

bool UFF_IncidentMarkerLayer::TryGetAnchorWorldLocation(const FFF_IncidentAnchor& Anchor, FVector& OutLocation) const
{
	if (!FFF_IncidentRules::TryGetAnchorLocation(GetWorld(), Anchor, OutLocation))
	{
		return false;
	}
	if (Anchor.Kind == EFF_IncidentAnchorKind::Actor)
	{
		OutLocation.Z += ActorHeightOffset;
	}
	return true;
}

void UFF_IncidentMarkerLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Markers.Num() == 0)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(this, 0);
	}
	if (!PC)
	{
		return;
	}

	const FVector2D LocalSize = MyGeometry.GetLocalSize();

	for (TPair<FFF_IncidentHandle, FMarker>& Pair : Markers)
	{
		UFF_IncidentMarkerButton* Button = Pair.Value.Button.Get();
		if (!Button)
		{
			continue;
		}

		FVector WorldLocation;
		FVector2D ScreenPosition = FVector2D::ZeroVector;
		const bool bVisible =
			TryGetAnchorWorldLocation(Pair.Value.View.Anchor, WorldLocation)
			&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldLocation, ScreenPosition, false)
			&& ScreenPosition.X >= 0.f && ScreenPosition.Y >= 0.f
			&& ScreenPosition.X <= LocalSize.X && ScreenPosition.Y <= LocalSize.Y;

		if (!bVisible)
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Button->Slot))
		{
			MarkerSlot->SetPosition(ScreenPosition);
		}
		Button->SetVisibility(ESlateVisibility::Visible);
	}
}
