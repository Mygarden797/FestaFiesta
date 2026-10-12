#include "UI/Core/FF_UIForegroundRules.h"

#include "Audio/FF_AudioEventLink.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UI/LiveHUD/FF_LiveUtilityDock.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"

bool FFUIForegroundRules::HasForegroundWindow(const UObject* ContextObject, bool bIncludeBuildWindows)
{
    const UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
    if (!World) return false;
    const UGameInstance* Game = World->GetGameInstance();
    const UFF_UIFlowSubsystem* Flow = Game ? Game->GetSubsystem<UFF_UIFlowSubsystem>() : nullptr;
    const UFF_UIRootLayout* Root = Flow ? Flow->GetRootLayout() : nullptr;
    if (Root && (Root->HasForegroundStackContent() || Root->IsAtlasTransitionActive())) return true;
    const UFF_FestivalSettingsSubsystem* Settings = UFF_FestivalSettingsSubsystem::Get(World);
    if (Settings && Settings->IsPauseMenuOpen()) return true;
    const UFF_FacilityCodexPresenterSubsystem* Codex = World->GetSubsystem<UFF_FacilityCodexPresenterSubsystem>();
    if (Codex && Codex->IsCodexOpen()) return true;

    // SNS/music are owned by the audio dock, outside HUD tab selection and root stacks.
    const UFF_AudioEventLink* AudioOwner = World->GetSubsystem<UFF_AudioEventLink>();
    const UFF_LiveUtilityDock* UtilityDock = AudioOwner
        ? Cast<UFF_LiveUtilityDock>(AudioOwner->GetPlaylistPlayerWidget()) : nullptr;
    if (UtilityDock && UtilityDock->IsForegroundFlyoutOpen()) return true;

    // Read the HUD owner's selection directly; no per-frame global widget search is needed.
    const UUserWidget* Hud = Flow ? Flow->GetStageHudWidget() : nullptr;
    if (Hud && Hud->IsVisible())
    {
        if (const UFF_PrepHudBar* Prep = Cast<UFF_PrepHudBar>(Hud))
        {
            const FFF_PrepTabSelection Selection = Prep->GetSelection();
            if (Selection.bHasSelection && Selection.Tab != EFF_PrepHudTab::Build) return true;
        }
        if (const UFF_LiveHudScreen* Live = Cast<UFF_LiveHudScreen>(Hud))
        {
            const FFF_LiveTabSelection Selection = Live->GetSelection();
            if (Selection.bHasSelection)
            {
                if (Selection.Tab != EFF_LiveTab::RevenueRanking) return true;
                const UFF_FacilityRevenueRankingSubsystem* Ranking = World->GetSubsystem<UFF_FacilityRevenueRankingSubsystem>();
                // The HUD clears this tab on its next tick after Close/Inspect. It is not a window then.
                if (Ranking && Ranking->IsOpen() && !ContextObject->IsA<UFF_FacilityRevenueRankingPanel>()) return true;
            }
        }
    }
    const UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
    return bIncludeBuildWindows && Build &&
        (Build->GetBuildModeState() == EFF_BuildModeState::CatalogOpen || Build->IsBuildAuxiliaryPanelOpen());
}
