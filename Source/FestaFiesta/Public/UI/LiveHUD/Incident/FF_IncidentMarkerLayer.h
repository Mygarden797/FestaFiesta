// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Incident/FF_IncidentTypes.h"
#include "FF_IncidentMarkerLayer.generated.h"

class UCanvasPanel;
class UFF_IncidentMarkerButton;
class UFF_IncidentSubsystem;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIncidentMarkerClicked, FFF_IncidentHandle, Handle);

// 활성 사건마다 앵커 위치 위에 MarkerIcon 을 매 프레임 투영해 띄운다. WBP 없이 C++ 만으로 트리를 만든다.
UCLASS(meta = (DisplayName = "FF Incident Marker Layer"))
class FESTAFIESTA_API UFF_IncidentMarkerLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Incident")
	void RefreshAll();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	int32 GetMarkerCount() const { return Markers.Num(); }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Incident")
	FFF_OnIncidentMarkerClicked OnIncidentMarkerClicked;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleIncidentRaised(FFF_IncidentHandle Handle);

	UFUNCTION()
	void HandleIncidentUpdated(FFF_IncidentHandle Handle);

	UFUNCTION()
	void HandleIncidentClosed(FFF_IncidentHandle Handle);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	FVector2D IconSize = FVector2D(32.f, 32.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float ActorHeightOffset = 120.f;

private:
	struct FMarker
	{
		TWeakObjectPtr<UFF_IncidentMarkerButton> Button;
		FFF_IncidentAnchor Anchor;
		FGameplayTag Tag;
		FString RaisedActorName;
		FVector RaisedLocation = FVector::ZeroVector;
		FVector2D LastScreenPosition = FVector2D::ZeroVector;
		bool bLastVisible = false;
		bool bLastProjected = false;
	};

	void LogMarker(const TCHAR* Phase, FFF_IncidentHandle Handle, const FMarker& Marker, const FVector2D& LayerSize) const;

	float DebugLogElapsed = 0.f;

	UFF_IncidentSubsystem* GetIncidentSubsystem() const;

	void BindIncidents();
	void UnbindIncidents();

	void AddOrRefreshMarker(FFF_IncidentHandle Handle);
	void RemoveMarker(FFF_IncidentHandle Handle);
	void ClearMarkers();

	void HandleMarkerClicked(FFF_IncidentHandle Handle);

	void ApplyIcon(UFF_IncidentMarkerButton& Button, const FFF_IncidentView& View);
	UTexture2D* ResolveIcon(const FFF_IncidentView& View);
	bool TryGetAnchorWorldLocation(const FFF_IncidentAnchor& Anchor, FVector& OutLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UFF_IncidentSubsystem> CachedIncident;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> IconCache;

	TMap<FFF_IncidentHandle, FMarker> Markers;
	TSet<FGameplayTag> WarnedMissingIcon;
};
