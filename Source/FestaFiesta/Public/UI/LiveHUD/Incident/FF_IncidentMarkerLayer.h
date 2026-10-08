// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Incident/FF_IncidentTypes.h"
#include "UI/LiveHUD/Incident/FF_IncidentMarkerRules.h"
#include "FF_IncidentMarkerLayer.generated.h"

class UCanvasPanel;
class UFF_FestivalStageSubsystem;
class UFF_IncidentMarkerButton;
class UFF_IncidentSubsystem;
class UMaterialInterface;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIncidentMarkerClicked, FFF_IncidentHandle, Handle);

// 활성 사건마다 앵커 위에 마커를 띄운다. 위치는 매 프레임 투영하고, 모양은 사건 갱신·축제 시계 방송 때만 다시 계산한다.
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

	UFUNCTION()
	void HandleClockSecond(int32 RemainingSeconds);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	FVector2D IconSize = FVector2D(40.f, 40.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float ActorHeightOffset = 120.f;

	// 화면 밖 사건 마커가 붙는 가장자리 여백. 아이콘 중심이 이 안쪽 사각형 안에 머문다. 화살표가 바깥으로 나오므로 아이콘 반지름보다 넉넉히 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	FMargin EdgeSafeMargin = FMargin(40.f, 80.f, 40.f, 40.f);

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Incident")
	TSoftObjectPtr<UTexture2D> QuestGlyph = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Incident/T_FF_LH_Icon_IncidentQuest_White_256.T_FF_LH_Icon_IncidentQuest_White_256")));

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Incident")
	TSoftObjectPtr<UTexture2D> InfoGlyph = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Incident/T_FF_LH_Icon_IncidentInfo_White_256.T_FF_LH_Icon_IncidentInfo_White_256")));

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Incident")
	TSoftObjectPtr<UTexture2D> DispatchedGlyph = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Incident/T_FF_LH_Icon_IncidentDispatched_White_256.T_FF_LH_Icon_IncidentDispatched_White_256")));

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Incident")
	TSoftObjectPtr<UMaterialInterface> RingMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/UI/Material/M_UI_IncidentTimerRing.M_UI_IncidentTimerRing")));

private:
	struct FMarker
	{
		TWeakObjectPtr<UFF_IncidentMarkerButton> Button;
		FFF_IncidentView View;
	};

	UFF_IncidentSubsystem* GetIncidentSubsystem() const;
	UFF_FestivalStageSubsystem* GetFestivalStageSubsystem() const;
	bool TryGetNowMinutes(float& OutMinutes) const;

	void LoadVisuals();
	void BindIncidents();
	void UnbindIncidents();

	void AddOrRefreshMarker(FFF_IncidentHandle Handle);
	void RemoveMarker(FFF_IncidentHandle Handle);
	void ClearMarkers();
	void ApplyLook(FMarker& Marker, bool bHasNow, float NowMinutes);
	UTexture2D* GetGlyphTexture(EFF_IncidentMarkerGlyph Glyph) const;

	void HandleMarkerClicked(FFF_IncidentHandle Handle);

	bool TryGetAnchorWorldLocation(const FFF_IncidentAnchor& Anchor, FVector& OutLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UFF_IncidentSubsystem> CachedIncident;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalStageSubsystem> CachedStage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedQuestGlyph;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedInfoGlyph;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedDispatchedGlyph;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedRingMaterial;

	TMap<FFF_IncidentHandle, FMarker> Markers;
};
