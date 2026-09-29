// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentMarkerLayer.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/FF_StaffDispatchSubsystem.h"
#include "UI/LiveHUD/Incident/FF_IncidentMarkerButton.h"
#include "Kismet/GameplayStatics.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FFIncidentMarkerLayer"

namespace FFIncidentMarkerDebug
{
	static TAutoConsoleVariable<float> CVarLogInterval(
		TEXT("FF.Incident.MarkerLogInterval"),
		2.f,
		TEXT("사건 아이콘 진단 로그 주기(초). 0 이면 끈다."));
}

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

	BindIncidents();
	RefreshAll();
}

void UFF_IncidentMarkerLayer::NativeDestruct()
{
	UnbindIncidents();
	ClearMarkers();
	PendingResolve.Reset();
	Super::NativeDestruct();
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
}

void UFF_IncidentMarkerLayer::UnbindIncidents()
{
	if (CachedIncident)
	{
		CachedIncident->OnIncidentRaised.RemoveDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentRaised);
		CachedIncident->OnIncidentUpdated.RemoveDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentUpdated);
		CachedIncident->OnIncidentClosed.RemoveDynamic(this, &UFF_IncidentMarkerLayer::HandleIncidentClosed);
	}
	CachedIncident = nullptr;
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

void UFF_IncidentMarkerLayer::HandleMarkerClicked(FFF_IncidentHandle Handle)
{
	PendingResolve.AddUnique(Handle);
	OnIncidentMarkerClicked.Broadcast(Handle);
}

void UFF_IncidentMarkerLayer::FlushPendingResolve()
{
	if (PendingResolve.Num() == 0)
	{
		return;
	}

	const TArray<FFF_IncidentHandle> ToResolve = MoveTemp(PendingResolve);
	PendingResolve.Reset();

	UFF_IncidentSubsystem* Incident = GetIncidentSubsystem();
	if (!Incident)
	{
		return;
	}

	for (const FFF_IncidentHandle& Handle : ToResolve)
	{
		const bool bAccepted = Incident->ResolveIncident(Handle);
		// Resolution may synchronously remove the marker, so re-find it after the command.
		if (const FMarker* Marker = Markers.Find(Handle))
		{
			if (UFF_IncidentMarkerButton* Button = Marker->Button.Get())
			{
				const UWorld* World = GetWorld();
				const UFF_StaffDispatchSubsystem* Dispatch = World ? World->GetSubsystem<UFF_StaffDispatchSubsystem>() : nullptr;
				Button->SetToolTipText(Dispatch && Dispatch->IsResponding(Handle)
					? LOCTEXT("StaffResponding", "스태프 출동 중입니다. 현장에 도착하면 사건을 해결합니다.")
					: !bAccepted ? LOCTEXT("StaffUnavailable", "출동할 수 없습니다. 안내 시설에 배정된 대기 스태프와 이동 경로를 확인하세요.")
					: FText::GetEmpty());
			}
		}
	}
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

	if (FMarker* Existing = Markers.Find(Handle))
	{
		Existing->Anchor = View.Anchor;
		if (UFF_IncidentMarkerButton* Button = Existing->Button.Get())
		{
			ApplyIcon(*Button, View);
		}
		return;
	}

	UFF_IncidentMarkerButton* Button = CreateWidget<UFF_IncidentMarkerButton>(this, UFF_IncidentMarkerButton::StaticClass());
	if (!Button)
	{
		return;
	}

	Button->SetHandle(Handle);
	Button->OnMarkerClicked.BindUObject(this, &UFF_IncidentMarkerLayer::HandleMarkerClicked);
	ApplyIcon(*Button, View);
	Button->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* MarkerSlot = Canvas->AddChildToCanvas(Button))
	{
		MarkerSlot->SetAnchors(FAnchors(0.f, 0.f));
		MarkerSlot->SetAlignment(FVector2D(0.5f, 1.f));
		MarkerSlot->SetAutoSize(false);
		MarkerSlot->SetSize(IconSize);
	}

	FMarker& Marker = Markers.Add(Handle);
	Marker.Button = Button;
	Marker.Anchor = View.Anchor;
	Marker.Tag = View.IncidentTag;
	if (const AActor* Actor = View.Anchor.Actor.Get())
	{
		Marker.RaisedActorName = Actor->GetName();
		Marker.RaisedLocation = Actor->GetActorLocation();
	}
	else
	{
		Marker.RaisedLocation = View.Anchor.WorldPoint;
	}

	if (FFIncidentMarkerDebug::CVarLogInterval.GetValueOnGameThread() > 0.f)
	{
		LogMarker(TEXT("RAISE"), Handle, Marker, GetCachedGeometry().GetLocalSize());
	}
}

void UFF_IncidentMarkerLayer::LogMarker(const TCHAR* Phase, FFF_IncidentHandle Handle, const FMarker& Marker, const FVector2D& LayerSize) const
{
	const AActor* Actor = Marker.Anchor.Actor.Get();
	const UEnum* KindEnum = StaticEnum<EFF_IncidentAnchorKind>();
	const FString KindName = KindEnum ? KindEnum->GetNameStringByValue(static_cast<int64>(Marker.Anchor.Kind)) : TEXT("?");

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[IncidentMarker] %s H=%d/%d Tag=%s Kind=%s Actor=%s (raised=%s) Hidden=%s ActorLoc=%s RaisedLoc=%s Moved=%.0f Projected=%s Visible=%s Screen=(%.0f,%.0f) Layer=(%.0f,%.0f)"),
		Phase,
		Handle.Index, Handle.Serial,
		*Marker.Tag.ToString(),
		*KindName,
		Actor ? *Actor->GetName() : TEXT("<null>"),
		*Marker.RaisedActorName,
		Actor ? (Actor->IsHidden() ? TEXT("Y") : TEXT("N")) : TEXT("-"),
		Actor ? *Actor->GetActorLocation().ToCompactString() : TEXT("-"),
		*Marker.RaisedLocation.ToCompactString(),
		Actor ? FVector::Dist(Actor->GetActorLocation(), Marker.RaisedLocation) : 0.f,
		Marker.bLastProjected ? TEXT("Y") : TEXT("N"),
		Marker.bLastVisible ? TEXT("Y") : TEXT("N"),
		Marker.LastScreenPosition.X, Marker.LastScreenPosition.Y,
		LayerSize.X, LayerSize.Y);
}

void UFF_IncidentMarkerLayer::RemoveMarker(FFF_IncidentHandle Handle)
{
	FMarker Marker;
	if (Markers.RemoveAndCopyValue(Handle, Marker))
	{
		if (FFIncidentMarkerDebug::CVarLogInterval.GetValueOnGameThread() > 0.f)
		{
			LogMarker(TEXT("CLOSE"), Handle, Marker, GetCachedGeometry().GetLocalSize());
		}

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

void UFF_IncidentMarkerLayer::ApplyIcon(UFF_IncidentMarkerButton& Button, const FFF_IncidentView& View)
{
	UTexture2D* Icon = ResolveIcon(View);
	Button.SetIcon(Icon, FallbackColor, IconSize);
	const UWorld* World = GetWorld();
	const UFF_StaffDispatchSubsystem* Dispatch = World ? World->GetSubsystem<UFF_StaffDispatchSubsystem>() : nullptr;
	Button.SetToolTipText(Dispatch && Dispatch->IsResponding(Button.GetHandle())
		? LOCTEXT("StaffResponding", "스태프 출동 중입니다. 현장에 도착하면 사건을 해결합니다.")
		: LOCTEXT("StaffDispatchPrompt", "클릭하여 안내 시설의 스태프를 출동시킵니다."));

	if (!Icon)
	{
		if (!WarnedMissingIcon.Contains(View.IncidentTag))
		{
			WarnedMissingIcon.Add(View.IncidentTag);
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("[%s] %s 사건 정의에 MarkerIcon 이 없어 기본 점으로 표시한다."),
				*GetName(), *View.IncidentTag.ToString());
		}
	}
}

UTexture2D* UFF_IncidentMarkerLayer::ResolveIcon(const FFF_IncidentView& View)
{
	if (View.MarkerIcon.IsNull())
	{
		return nullptr;
	}

	if (const TObjectPtr<UTexture2D>* Cached = IconCache.Find(View.IncidentTag))
	{
		return *Cached;
	}

	UTexture2D* Loaded = View.MarkerIcon.LoadSynchronous();
	IconCache.Add(View.IncidentTag, Loaded);
	return Loaded;
}

bool UFF_IncidentMarkerLayer::TryGetAnchorWorldLocation(const FFF_IncidentAnchor& Anchor, FVector& OutLocation) const
{
	switch (Anchor.Kind)
	{
	case EFF_IncidentAnchorKind::Actor:
		if (const AActor* Actor = Anchor.Actor.Get())
		{
			OutLocation = Actor->GetActorLocation() + FVector(0.f, 0.f, ActorHeightOffset);
			return true;
		}
		return false;

	case EFF_IncidentAnchorKind::WorldPoint:
		OutLocation = Anchor.WorldPoint;
		return true;

	default:
		return false;
	}
}

void UFF_IncidentMarkerLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	FlushPendingResolve();

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
		const bool bProjected =
			TryGetAnchorWorldLocation(Pair.Value.Anchor, WorldLocation)
			&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldLocation, ScreenPosition, false);
		const bool bVisible = bProjected
			&& ScreenPosition.X >= 0.f && ScreenPosition.Y >= 0.f
			&& ScreenPosition.X <= LocalSize.X && ScreenPosition.Y <= LocalSize.Y;

		Pair.Value.bLastProjected = bProjected;
		Pair.Value.bLastVisible = bVisible;
		Pair.Value.LastScreenPosition = ScreenPosition;

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

	const float LogInterval = FFIncidentMarkerDebug::CVarLogInterval.GetValueOnGameThread();
	if (LogInterval <= 0.f)
	{
		return;
	}

	DebugLogElapsed += InDeltaTime;
	if (DebugLogElapsed < LogInterval)
	{
		return;
	}
	DebugLogElapsed = 0.f;

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[IncidentMarker] TICK Markers=%d ViewportSize=%s ViewportScale=%.3f Layer=(%.0f,%.0f) PC=%s"),
		Markers.Num(),
		*UWidgetLayoutLibrary::GetViewportSize(this).ToString(),
		UWidgetLayoutLibrary::GetViewportScale(this),
		LocalSize.X, LocalSize.Y,
		*PC->GetName());

	for (const TPair<FFF_IncidentHandle, FMarker>& Pair : Markers)
	{
		LogMarker(TEXT("TICK"), Pair.Key, Pair.Value, LocalSize);
	}
}

#undef LOCTEXT_NAMESPACE
