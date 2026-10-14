#include "MapOverview/FF_MapOverviewGameBridgeSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Core/FF_UIRootLayout.h"

namespace
{
    struct FToolbarMount
    {
        TWeakObjectPtr<UUserWidget> Tools;
        TWeakObjectPtr<UHorizontalBox> Row;
        TWeakObjectPtr<USizeBox> Bounds;
        float OriginalWidth = 0.f;
        float AddedWidth = 0.f;
    };
    TMap<TWeakObjectPtr<UUserWidget>, FToolbarMount> ToolbarMounts;

    struct FToolbarSlot
    {
        UWidget* Widget = nullptr;
        FMargin Padding;
        FSlateChildSize Size;
        EHorizontalAlignment Horizontal = HAlign_Fill;
        EVerticalAlignment Vertical = VAlign_Fill;

        explicit FToolbarSlot(UWidget& InWidget)
            : Widget(&InWidget)
        {
            const auto* Slot = CastChecked<UHorizontalBoxSlot>(InWidget.Slot);
            Padding = Slot->GetPadding(); Size = Slot->GetSize();
            Horizontal = Slot->GetHorizontalAlignment(); Vertical = Slot->GetVerticalAlignment();
        }
        void Apply(UHorizontalBoxSlot& Slot) const
        {
            Slot.SetPadding(Padding); Slot.SetSize(Size);
            Slot.SetHorizontalAlignment(Horizontal); Slot.SetVerticalAlignment(Vertical);
        }
    };

    bool ContainsNestedUserWidget(UWidget& Widget)
    {
        if (Widget.IsA<UUserWidget>()) return true;
        TArray<UWidget*> Children;
        UWidgetTree::GetChildWidgets(&Widget, Children);
        return Children.ContainsByPredicate([](const UWidget* Child) { return Child && Child->IsA<UUserWidget>(); });
    }

    UFF_UIFlowSubsystem* FlowFor(UWorld* World)
    {
        return World && World->GetGameInstance()
            ? World->GetGameInstance()->GetSubsystem<UFF_UIFlowSubsystem>() : nullptr;
    }
    AFF_OverviewCameraPawn* CameraFor(APlayerController* Controller)
    {
        if (const auto* Festival = Cast<AFF_FestivalPlayerController>(Controller))
            return Festival->GetOverviewCameraPawn();
        return Controller ? Cast<AFF_OverviewCameraPawn>(Controller->GetPawn()) : nullptr;
    }
}

bool FFMapOverviewToolbar::AttachAfterGridToRow(UUserWidget& Tools, UUserWidget& Addon)
{
    for (auto It = ToolbarMounts.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || !It.Value().Tools.IsValid()) It.RemoveCurrent();
    auto* Row = Cast<UHorizontalBox>(Tools.GetWidgetFromName(TEXT("Row_BuildReferenceTopBar")));
    auto* Grid = Cast<UButton>(Tools.GetWidgetFromName(TEXT("Button_Grid")));
    auto* Bounds = Cast<USizeBox>(Tools.GetWidgetFromName(TEXT("Size_BuildTopBar")));
    if (!Row || !Grid || !Bounds || !Bounds->IsWidthOverride() || Bounds->GetWidthOverride() <= 0.f) return false;
    if (const FToolbarMount* Existing = ToolbarMounts.Find(&Addon))
    {
        if (Existing->Tools.Get() == &Tools && Existing->Row.Get() == Row && Addon.GetParent() == Row) return true;
        if (Existing->Tools.IsValid()) DetachFromRow(*Existing->Tools.Get(), Addon);
        else ToolbarMounts.Remove(&Addon);
    }
    if (Addon.GetParent() || &Tools == &Addon) return false;
    UWidget* GridCell = Grid;
    while (GridCell->GetParent() && GridCell->GetParent() != Row) GridCell = GridCell->GetParent();
    const int32 GridIndex = Row->GetChildIndex(GridCell);
    const auto* GridSlot = Cast<UHorizontalBoxSlot>(GridCell->Slot);
    if (GridIndex == INDEX_NONE || !GridSlot) return false;
    const UWidget* RowAncestor = Row;
    while (RowAncestor && RowAncestor != Bounds) RowAncestor = RowAncestor->GetParent();
    if (!RowAncestor) return false;

    TArray<FToolbarSlot> Tail;
    for (int32 Index = GridIndex + 1; Index < Row->GetChildrenCount(); ++Index)
    {
        UWidget* Child = Row->GetChildAt(Index);
        // Removing a native cell rebuilds its Slate nodes, but an arbitrary nested WBP
        // may run NativeDestruct and lose custom bindings. Reject before any mutation.
        if (!Child || !Cast<UHorizontalBoxSlot>(Child->Slot) || ContainsNestedUserWidget(*Child))
        {
            UE_LOG(LogTemp, Display, TEXT("MapOverview: unsupported nested toolbar widget; original toolbar preserved."));
            return false;
        }
        Tail.Emplace(*Child);
    }
    const TSharedRef<SWidget> AddonSlate = Addon.TakeWidget();
    Addon.ForceLayoutPrepass();
    double Width = Addon.GetDesiredSize().X;
    if (!FMath::IsFinite(Width) || Width <= 0.0)
    {
        if (const auto* Cell = Cast<USizeBox>(GridCell); Cell && Cell->IsWidthOverride()) Width = Cell->GetWidthOverride();
    }
    if (!FMath::IsFinite(Width) || Width <= 0.0) return false;
    const FToolbarSlot GridLayout(*GridCell);
    FToolbarMount Mount;
    Mount.Tools = &Tools; Mount.Row = Row; Mount.Bounds = Bounds;
    Mount.OriginalWidth = Bounds->GetWidthOverride();
    Mount.AddedWidth = static_cast<float>(Width) + GridLayout.Padding.Left + GridLayout.Padding.Right;
    // InsertChildAt only changes UObject order in UE 5.7. Remove/reappend the
    // native tail so OnSlotRemoved/Added update the live SHorizontalBox as well.
    for (const FToolbarSlot& Saved : Tail) Row->RemoveChild(Saved.Widget);
    auto* AddonSlot = Row->AddChildToHorizontalBox(&Addon);
    GridLayout.Apply(*AddonSlot);
    AddonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    for (const FToolbarSlot& Saved : Tail) Saved.Apply(*Row->AddChildToHorizontalBox(Saved.Widget));
    Bounds->SetWidthOverride(Mount.OriginalWidth + Mount.AddedWidth);
    ToolbarMounts.Add(&Addon, Mount);
    return true;
}

void FFMapOverviewToolbar::DetachFromRow(UUserWidget& Tools, UUserWidget& Addon)
{
    const FToolbarMount* Found = ToolbarMounts.Find(&Addon);
    if (!Found || Found->Tools.Get() != &Tools) return;
    const FToolbarMount Mount = *Found;
    ToolbarMounts.Remove(&Addon);
    if (Mount.Row.IsValid() && Addon.GetParent() == Mount.Row.Get()) Mount.Row->RemoveChild(&Addon);
    if (Mount.Bounds.IsValid())
    {
        USizeBox* Bounds = Mount.Bounds.Get();
        const float CurrentWidth = Bounds->GetWidthOverride();
        // Preserve any independent runtime width edit made while the addon was mounted.
        Bounds->SetWidthOverride(FMath::IsNearlyEqual(CurrentWidth, Mount.OriginalWidth + Mount.AddedWidth)
            ? Mount.OriginalWidth : FMath::Max(0.f, CurrentWidth - Mount.AddedWidth));
    }
}

void UFF_MapOverviewGameBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    IModularFeatures::Get().RegisterModularFeature(FeatureName(), this);
}

void UFF_MapOverviewGameBridgeSubsystem::Deinitialize()
{
    for (const auto& Pair : ToolbarAddons)
        if (UUserWidget* Addon = Pair.Value.Get())
            if (const auto* Tools = ToolbarCache.Find(Pair.Key); Tools && Tools->IsValid())
                FFMapOverviewToolbar::DetachFromRow(*Tools->Get(), *Addon);
    ToolbarCache.Reset(); ToolbarAddons.Reset();
    IModularFeatures::Get().UnregisterModularFeature(FeatureName(), this);
    PreviewCache.Reset();
    PreviousViewModes.Reset();
    MapViewTargets.Reset();
    MapViewCharacterPawns.Reset();
    Super::Deinitialize();
}

bool UFF_MapOverviewGameBridgeSubsystem::IsHudContextReady(UWorld* World) const
{
    const auto* Flow = FlowFor(World);
    return Flow && Flow->GetMode() == EFF_UIFlowMode::Festival &&
        IsValid(Flow->GetStageHudWidget()) && IsValid(Flow->GetRootLayout());
}

bool UFF_MapOverviewGameBridgeSubsystem::IsInputBlocked(UWorld* World) const
{
    const auto* Flow = FlowFor(World);
    const auto* Layout = Flow ? Flow->GetRootLayout() : nullptr;
    const auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    const auto* PC = World ? Cast<AFF_FestivalPlayerController>(World->GetFirstPlayerController()) : nullptr;
    return !IsHudContextReady(World) || (World && World->IsPaused()) ||
        (Layout && (Layout->HasForegroundStackContent() || Layout->IsAtlasTransitionActive())) ||
        (Build && Build->IsInputSuspendedForPauseMenu()) || (PC && PC->IsInScriptedView());
}

bool UFF_MapOverviewGameBridgeSubsystem::MountHudWidget(UWorld* World, UUserWidget* Widget)
{
    if (!IsHudContextReady(World) || !Widget) return false;
    if (Widget->IsInViewport()) return true;
    Widget->RemoveFromParent();
    // Authored HUD layers may be positioned/scaled for the lower resource bar.
    // A separate local-player overlay guarantees viewport-relative top-right anchors.
    // The presenter collapses this overlay whenever the existing modal/foreground owner is active.
    return Widget->AddToPlayerScreen(2);
}

bool UFF_MapOverviewGameBridgeSubsystem::MountWholeGridToolbar(UWorld* World, UUserWidget* Widget)
{
    if (!World || !IsValid(Widget) || Widget->GetWorld() != World) return false;
    for (auto It = ToolbarCache.CreateIterator(); It; ++It)
        if (!It.Key().IsValid()) { ToolbarAddons.Remove(It.Key()); It.RemoveCurrent(); }
    if (const auto* Old = ToolbarAddons.Find(World); Old && Old->IsValid() && Old->Get() != Widget)
        UnmountWholeGridToolbar(World, Old->Get());
    TWeakObjectPtr<UUserWidget>& Cached = ToolbarCache.FindOrAdd(World);
    APlayerController* Owner = Widget->GetOwningPlayer();
    if (!Owner) return false;
    if (Cached.IsValid() && (!Cached->IsInViewport() || Cached->GetWorld() != World || Cached->GetOwningPlayer() != Owner))
    {
        FFMapOverviewToolbar::DetachFromRow(*Cached.Get(), *Widget);
        Cached.Reset();
    }
    if (!Cached.IsValid())
    {
        TArray<UUserWidget*> Candidates;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Candidates, UFF_BuildToolsWidget::StaticClass(), true);
        for (UUserWidget* Candidate : Candidates)
            if (Candidate->GetOwningPlayer() == Owner) { Cached = Candidate; break; }
    }
    if (!Cached.IsValid() || !FFMapOverviewToolbar::AttachAfterGridToRow(*Cached.Get(), *Widget)) return false;
    ToolbarAddons.Add(World, Widget);
    return true;
}

void UFF_MapOverviewGameBridgeSubsystem::UnmountWholeGridToolbar(UWorld* World, UUserWidget* Widget)
{
    if (!Widget) return;
    if (const auto* Tools = ToolbarCache.Find(World); Tools && Tools->IsValid())
        FFMapOverviewToolbar::DetachFromRow(*Tools->Get(), *Widget);
    if (const auto* Addon = ToolbarAddons.Find(World); Addon && Addon->Get() == Widget)
        ToolbarAddons.Remove(World);
}

bool UFF_MapOverviewGameBridgeSubsystem::IsBuildSessionActive(UWorld* World) const
{
    const auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    return Build && FFBuildModeModel::IsBuildSessionActive(Build->GetBuildModeState());
}

bool UFF_MapOverviewGameBridgeSubsystem::IsGridVisible(UWorld* World) const
{
    const auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    return Build && Build->IsPlacementGridVisible();
}

void UFF_MapOverviewGameBridgeSubsystem::SetGridVisible(UWorld* World, const bool bVisible)
{
    if (auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr)
        Build->SetPlacementGridVisible(bVisible);
}

int32 UFF_MapOverviewGameBridgeSubsystem::GetVisualGridStepCm(UWorld* World) const
{
    const auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    return Build ? FMath::Max(25, Build->GetActivePositionSnapCm()) : 100;
}

bool UFF_MapOverviewGameBridgeSubsystem::TryGetPlacementContext(UWorld* World, FFFZSCommand& OutCommand)
{
    OutCommand = FFFZSCommand();
    const auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    if (!Build || !FFBuildModeModel::IsPreviewActive(Build->GetBuildModeState()) || !Build->HasPlacementTarget())
        return false;
    for (auto It = PreviewCache.CreateIterator(); It; ++It)
        if (!It.Key().IsValid()) It.RemoveCurrent();
    TWeakObjectPtr<AFFZSPlacementPreviewActor>& Cached = PreviewCache.FindOrAdd(World);
    if (!Cached.IsValid())
    {
        for (TActorIterator<AFFZSPlacementPreviewActor> It(World); It; ++It)
        {
            if (!It->IsHidden() && !It->PreviewCommand.FacilitySpec.FacilityDefinitionId.IsNone())
            {
                Cached = *It;
                break;
            }
        }
    }
    if (!Cached.IsValid()) return false;
    OutCommand = Cached->PreviewCommand;
    return !OutCommand.ZoneId.IsNone();
}

void UFF_MapOverviewGameBridgeSubsystem::NotifyMapInteraction(UWorld* World)
{
    if (auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr)
        Build->NotifyBuildToolsInteraction();
    StopCameraMotion(World ? World->GetFirstPlayerController() : nullptr);
}

bool UFF_MapOverviewGameBridgeSubsystem::TryGetCameraFocus(APlayerController* Controller, FVector& OutFocus) const
{
    if (const auto* Camera = CameraFor(Controller))
    {
        OutFocus = Camera->GetCameraState().FocusLocation;
        return !OutFocus.ContainsNaN();
    }
    return false;
}

void UFF_MapOverviewGameBridgeSubsystem::StopCameraMotion(APlayerController* Controller)
{
    if (auto* Camera = CameraFor(Controller)) Camera->StopCameraMotion();
}

bool UFF_MapOverviewGameBridgeSubsystem::TryGetPlayerMapLocation(APlayerController* Controller, FVector& OutLocation) const
{
    if (!Controller) return false;
    const AFF_OverviewCameraPawn* Camera = CameraFor(Controller);
    const APawn* PlayerPawn = Controller->GetPawn();
    if (IsValid(PlayerPawn) && PlayerPawn != Camera)
    {
        OutLocation = PlayerPawn->GetActorLocation();
        return !OutLocation.ContainsNaN();
    }
    if (const auto* Festival = Cast<AFF_FestivalPlayerController>(Controller))
    {
        const uint8* PreviousMode = PreviousViewModes.Find(Controller);
        const auto* Target = MapViewTargets.Find(Controller);
        const auto* Character = MapViewCharacterPawns.Find(Controller);
        if (Festival->GetViewMode() == EFF_CameraViewMode::Actor && Target && Target->IsValid() &&
            Festival->GetViewTarget() == Target->Get() && PreviousMode &&
            *PreviousMode == static_cast<uint8>(EFF_CameraViewMode::Character) && Character && Character->IsValid())
        {
            OutLocation = Character->Get()->GetActorLocation();
            return !OutLocation.ContainsNaN();
        }
    }
    // Overview is the player's controllable position in construction mode. Whole-map
    // view keeps this focus rather than making the local minimap jump to the venue center.
    return TryGetCameraFocus(Controller, OutLocation);
}

bool UFF_MapOverviewGameBridgeSubsystem::FocusMapPoint(APlayerController* Controller, const FVector& Location)
{
    if (auto* Camera = CameraFor(Controller); Camera && !Location.ContainsNaN())
    {
        if (auto* Festival = Cast<AFF_FestivalPlayerController>(Controller);
            Festival && Festival->GetViewMode() != EFF_CameraViewMode::Overview)
            Festival->SwitchToOverview(0.0f);
        Camera->FocusOnLocation(Location);
        return true;
    }
    return false;
}

bool UFF_MapOverviewGameBridgeSubsystem::EnterMapView(APlayerController* Controller, AActor* Target)
{
    if (!IsValid(Target)) return false;
    if (auto* Festival = Cast<AFF_FestivalPlayerController>(Controller))
    {
        const uint8 PreviousMode = static_cast<uint8>(Festival->GetViewMode());
        const TWeakObjectPtr<APawn> PreviousCharacter = Festival->GetViewMode() == EFF_CameraViewMode::Character
            ? Festival->GetPawn() : nullptr;
        if (Festival->GetViewMode() == EFF_CameraViewMode::Character)
        {
            if (!Festival->GetOverviewCameraPawn()) return false;
            Festival->SwitchToOverview(0.0f);
        }
        if (!Festival->SwitchToActorView(Target, 0.0f)) return false;
        PreviousViewModes.Add(Controller, PreviousMode);
        MapViewTargets.Add(Controller, Target);
        MapViewCharacterPawns.Add(Controller, PreviousCharacter);
        return true;
    }
    return false;
}

bool UFF_MapOverviewGameBridgeSubsystem::ExitMapView(APlayerController* Controller, AActor* PreviousTarget)
{
    if (auto* Festival = Cast<AFF_FestivalPlayerController>(Controller))
    {
        uint8 PreviousMode = static_cast<uint8>(EFF_CameraViewMode::Overview);
        PreviousViewModes.RemoveAndCopyValue(Controller, PreviousMode);
        MapViewTargets.Remove(Controller);
        MapViewCharacterPawns.Remove(Controller);
        if (!IsValid(PreviousTarget) || PreviousTarget == Festival->GetOverviewCameraPawn())
        {
            Festival->SwitchToOverview(0.0f);
            return Festival->GetOverviewCameraPawn() != nullptr;
        }
        if (PreviousMode == static_cast<uint8>(EFF_CameraViewMode::Character))
        {
            if (auto* Character = Cast<APawn>(PreviousTarget)) return Festival->PossessCharacter(Character, 0.0f);
            Festival->SwitchToOverview(0.0f);
            return Festival->GetOverviewCameraPawn() != nullptr;
        }
        if (PreviousMode == static_cast<uint8>(EFF_CameraViewMode::Overview))
        {
            Festival->SwitchToOverview(0.0f);
            return true;
        }
        return IsValid(PreviousTarget) && Festival->SwitchToActorView(PreviousTarget, 0.0f);
    }
    return false;
}
