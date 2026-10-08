#include "Building/Presentation/FF_BuildingFeatureDiscoverySubsystem.h"
#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Engine/GameInstance.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	constexpr const TCHAR* Section = TEXT("FestaFiesta.Building.FirstUse");
	constexpr const TCHAR* SeenKey = TEXT("SeenFeatureIds");
	constexpr const TCHAR* LightingSection = TEXT("FestaFiesta.FacilityLighting");
	constexpr const TCHAR* LightingSeenKey = TEXT("bHasSeenColorIntroduction");
	constexpr const TCHAR* CodexSection = TEXT("FestaFiesta.FacilityCodex");
	constexpr const TCHAR* CodexSeenKey = TEXT("bHasSeenIntroduction");
}

void UFF_BuildingFeatureDiscoverySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SeenFeatureIds.Reset();
	if (!GConfig || GGameUserSettingsIni.IsEmpty()) return;
	TArray<FString> SavedIds;
	GConfig->GetArray(Section, SeenKey, SavedIds, GGameUserSettingsIni);
	for (const FString& SavedId : SavedIds)
	{
		const FName FeatureId(*SavedId.TrimStartAndEnd());
		if (!FeatureId.IsNone()) SeenFeatureIds.Add(FeatureId);
	}
}

void UFF_BuildingFeatureDiscoverySubsystem::Deinitialize()
{
	OnFeatureSeen.Clear();
	Super::Deinitialize();
}

bool UFF_BuildingFeatureDiscoverySubsystem::MarkFeatureSeen(const FName FeatureId)
{
	if (!IsInGameThread() || FeatureId.IsNone() || SeenFeatureIds.Contains(FeatureId)) return false;
	SeenFeatureIds.Add(FeatureId);
	if (FeatureId == FacilityCodexFeatureId() && GetGameInstance())
		if (auto* Progress = GetGameInstance()->GetSubsystem<UFF_FacilityCodexProgressSubsystem>())
			Progress->MarkIntroductionSeen();
	if (GConfig && !GGameUserSettingsIni.IsEmpty())
	{
		TArray<FString> SavedIds;
		for (const FName Id : SeenFeatureIds) SavedIds.Add(Id.ToString());
		SavedIds.Sort();
		GConfig->SetArray(Section, SeenKey, SavedIds, GGameUserSettingsIni);
		// Share the original facility inspector's learning history: one explanation per player.
		if (FeatureId == LightingControlsFeatureId())
			GConfig->SetBool(LightingSection, LightingSeenKey, true, GGameUserSettingsIni);
		if (FeatureId == FacilityCodexFeatureId())
			GConfig->SetBool(CodexSection, CodexSeenKey, true, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	OnFeatureSeen.Broadcast(FeatureId);
	return true;
}

bool UFF_BuildingFeatureDiscoverySubsystem::HasSeenFeature(const FName FeatureId) const
{
	if (SeenFeatureIds.Contains(FeatureId)) return true;
	bool bSeenInInspector = false;
	if (FeatureId == LightingControlsFeatureId() && GConfig && !GGameUserSettingsIni.IsEmpty())
		GConfig->GetBool(LightingSection, LightingSeenKey, bSeenInInspector, GGameUserSettingsIni);
	if (FeatureId == FacilityCodexFeatureId() && GConfig && !GGameUserSettingsIni.IsEmpty())
		GConfig->GetBool(CodexSection, CodexSeenKey, bSeenInInspector, GGameUserSettingsIni);
	return bSeenInInspector;
}
