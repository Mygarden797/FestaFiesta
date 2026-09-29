// FF_BuildToolsWidget.cpp
// 역할: 건설 도구 버튼과 각도 입력을 건설 상태 및 명령에 연결한다.

#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "FF_FFZSWorldSubsystem.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/Presentation/FF_BuildModeGuide.h"
#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"
#include "Building/Presentation/FF_BuildFeedback.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/DefaultValueHelper.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UI/Settings/FF_KeyBindingRuntimeSubsystem.h"
#include "TimerManager.h"
#include "UI/Settings/FF_RuntimeSettingsSubsystem.h"

#define LOCTEXT_NAMESPACE "FFBuildTools"

FVector2D FFBuildToolsModel::ResolveHelpVerticalBounds(const float ContentHeight, const float AvailableBottom)
{
    const float Bottom = FMath::Max(0.f, AvailableBottom);
    const float MinimumTop = FMath::Min(104.f, Bottom);
    const float Height = FMath::Min(FMath::Max(0.f, ContentHeight), Bottom - MinimumTop);
    return FVector2D(FMath::Max(MinimumTop, FMath::Min(525.f, Bottom - Height)), Height);
}

// 입력한 각도가 0~360의 유한한 숫자인지 확인한다. 잘못된 입력은 이전 값을 유지한다.
bool FFBuildToolsModel::TryParseYawText(const FString& Text, float& OutYawDegrees)
{
    const FString Trimmed = Text.TrimStartAndEnd();
    if (Trimmed.IsEmpty() || Trimmed.Len() > 16)
    {
        return false;
    }
    bool bHasDigit = false;
    bool bHasDecimalPoint = false;
    for (const TCHAR Character : Trimmed)
    {
        if (Character >= TEXT('0') && Character <= TEXT('9'))
        {
            bHasDigit = true;
        }
        else if (Character == TEXT('.') && !bHasDecimalPoint)
        {
            bHasDecimalPoint = true;
        }
        else
        {
            return false;
        }
    }
    float Parsed = 0.0f;
    if (!bHasDigit || !FDefaultValueHelper::ParseFloat(Trimmed, Parsed) || !FMath::IsFinite(Parsed) ||
        Parsed < 0.0f || Parsed > 360.0f)
    {
        return false;
    }
    OutYawDegrees = FMath::Fmod(Parsed, 360.0f);
    return true;
}

UFF_BuildToolsActionButton::UFF_BuildToolsActionButton()
{
    InitIsFocusable(false);
}

// 선택 상태의 스타일만 적용하고 해제 시 디자이너가 지정한 원래 모양을 복원한다.
void UFF_BuildToolsActionButton::ApplySelection(const bool bSelected, UTextBlock* Label)
{
    if (!bUseSelectionStyle) { return; }
    if (bHasRestingStyle && bAppliedSelected == bSelected) { return; }
    if (!bHasRestingStyle)
    {
        RestingStyle = GetStyle();
        RestingTextColor = Label ? Label->GetColorAndOpacity() : FSlateColor::UseForeground();
        bHasRestingStyle = true;
    }
    SetStyle(bSelected ? SelectionStyle : RestingStyle);
    if (Label) { Label->SetColorAndOpacity(bSelected ? SelectionTextColor : RestingTextColor); }
    bAppliedSelected = bSelected;
}

// 건설 상태·잔액 변경 알림과 화면 버튼을 연결하고 처음 값을 표시한다.
void UFF_BuildToolsWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (!IsDesignTime() || !bPreviewDesignerPage) return;
    auto Show = [this](const TCHAR* Name, bool bVisible)
    {
        if (auto* Widget = GetWidgetFromName(Name))
        {
#if WITH_EDITOR
            Widget->bHiddenInDesigner = !bVisible;
#endif
            Widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        }
    };
    const bool bPlacement = DesignerPage == EFF_BuildToolsDesignerPage::Placement;
    Show(TEXT("Border_BuildTopBar"), true);
    Show(TEXT("Border_BuildBack"), true);
    Show(TEXT("Border_PlacementInfo"), bPlacement);
    Show(TEXT("Border_Tools"), bPlacement);
    Show(TEXT("Panel_BuildSettings"), DesignerPage == EFF_BuildToolsDesignerPage::Settings);
    Show(TEXT("Border_PlacementHelp"), bDesignerShowHelp && DesignerPage != EFF_BuildToolsDesignerPage::Settings);
    Show(TEXT("Row_BuildHelpQueue"), bPlacement);
    Show(TEXT("Border_BuildToolbarGold"), false);
}

void UFF_BuildToolsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    // These bounds retain their height when their panel is collapsed. Let only
    // visible descendants receive input so the world/catalog remains clickable.
    if (!Size_BuildSettings)
        Size_BuildSettings = Cast<USizeBox>(GetWidgetFromName(TEXT("Size_BuildSettings")));
    if (Size_BuildSettings) Size_BuildSettings->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (!Size_BuildWorldHelp)
        Size_BuildWorldHelp = Cast<USizeBox>(GetWidgetFromName(TEXT("Size_BuildWorldHelp")));
    if (Size_BuildWorldHelp) Size_BuildWorldHelp->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (Size_BuildWorldHelp)
        if (auto* HelpCanvasSlot = Cast<UCanvasPanelSlot>(Size_BuildWorldHelp->Slot))
            AuthoredHelpPosition = HelpCanvasSlot->GetPosition();
    if (Size_BuildSettings && Size_BuildSettings->IsHeightOverride())
        AuthoredSettingsHeight = Size_BuildSettings->GetHeightOverride();
    BuildMode = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    Economy = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFF_BuildCatalogEconomyReadModel>() : nullptr;
    CatalogModel = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
    if (UFF_BuildCatalogSubsystem* Catalog = CatalogModel.Get())
    {
        Catalog->OnCatalogThumbnailsChanged.AddUniqueDynamic(this, &ThisClass::RefreshTools);
        Catalog->AcquireThumbnailPreload();
    }
    if (UFF_BuildCatalogEconomyReadModel* Model = Economy.Get())
    {
        Model->OnGoldChanged.AddUniqueDynamic(this, &ThisClass::HandleGoldChanged);
    }
    if (UFF_RuntimeSettingsSubsystem* Settings = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UFF_RuntimeSettingsSubsystem>() : nullptr)
    {
        Settings->OnSettingsChanged.AddUniqueDynamic(this, &ThisClass::RefreshTools);
    }
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->OnBuildToolsChanged.AddUniqueDynamic(this, &ThisClass::RefreshTools);
    }
#define FF_BIND_TOOL(Button, Handler) if (Button) { Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Handler); }
    FF_BIND_TOOL(Button_Grid, HandleGrid)
    FF_BIND_TOOL(Button_MultiSelect, HandleMultiSelect)
    FF_BIND_TOOL(Button_SnapCycle, HandleSnapCycle)
    FF_BIND_TOOL(Button_ToolbarRanges, HandleFacilityRanges)
    FF_BIND_TOOL(Button_ToolbarCatalog, ReturnToCatalog)
    FF_BIND_TOOL(Button_ToolbarHelp, ToggleHelp)
    FF_BIND_TOOL(Button_Confirm, HandleConfirm)
    FF_BIND_TOOL(Button_Catalog, ReturnToCatalog)
    FF_BIND_TOOL(Button_Details, ToggleDetails)
    FF_BIND_TOOL(Button_BuildSettings, ToggleBuildSettings)
    FF_BIND_TOOL(Button_BuildSettingsClose, ToggleBuildSettings)
    FF_BIND_TOOL(Button_BuildBack, HandleBuildBack)
    FF_BIND_TOOL(Button_SettingsGrid, HandleGrid)
    FF_BIND_TOOL(Button_SettingsSnapMeter, HandleSnapMeter)
    FF_BIND_TOOL(Button_SettingsSnapPrecision, HandleSnapPrecision)
    FF_BIND_TOOL(Button_SettingsSnapFree, HandleSnapFree)
    FF_BIND_TOOL(Button_SettingsDirectAdjustment, ToggleDirectAdjustment)
    FF_BIND_TOOL(Button_SettingsFacilityRanges, HandleFacilityRanges)
    FF_BIND_TOOL(Button_SettingsSynergyPreview, HandleSynergyPreview)
    FF_BIND_TOOL(Button_SettingsZoomToCursor, HandleZoomToCursor)
    FF_BIND_TOOL(Button_SettingsEdgeScroll, HandleEdgeScroll)
    FF_BIND_TOOL(Button_Facts, ToggleFacts)
    FF_BIND_TOOL(Button_Help, ToggleHelp)
    FF_BIND_TOOL(Button_BuildHelpUndo, HandleUndoConstruction)
    FF_BIND_TOOL(Button_BuildHelpRedo, HandleRedoConstruction)
    FF_BIND_TOOL(Button_DirectAdjustment, ToggleDirectAdjustment)
    FF_BIND_TOOL(Button_FindNearby, FindNearbyPlacement)
    FF_BIND_TOOL(Button_NextPlacementProblem, ShowNextPlacementProblem)
    FF_BIND_TOOL(Button_SnapMeter, HandleSnapMeter)
    FF_BIND_TOOL(Button_SnapPrecision, HandleSnapPrecision)
    FF_BIND_TOOL(Button_SnapFree, HandleSnapFree)
    FF_BIND_TOOL(Button_RotateLeft, HandleRotateLeft)
    FF_BIND_TOOL(Button_RotateRight, HandleRotateRight)
    FF_BIND_TOOL(Button_NudgeLeft, HandleNudgeLeft)
    FF_BIND_TOOL(Button_NudgeUp, HandleNudgeUp)
    FF_BIND_TOOL(Button_NudgeDown, HandleNudgeDown)
    FF_BIND_TOOL(Button_NudgeRight, HandleNudgeRight)
#undef FF_BIND_TOOL
    if (Button_FreeRotateLeft)
    {
        Button_FreeRotateLeft->OnPressed.AddUniqueDynamic(this, &ThisClass::HandleFreeRotateLeftPressed);
        Button_FreeRotateLeft->OnReleased.AddUniqueDynamic(this, &ThisClass::StopPointerFreeRotation);
        Button_FreeRotateLeft->OnUnhovered.AddUniqueDynamic(this, &ThisClass::StopPointerFreeRotation);
    }
    if (Button_FreeRotateRight)
    {
        Button_FreeRotateRight->OnPressed.AddUniqueDynamic(this, &ThisClass::HandleFreeRotateRightPressed);
        Button_FreeRotateRight->OnReleased.AddUniqueDynamic(this, &ThisClass::StopPointerFreeRotation);
        Button_FreeRotateRight->OnUnhovered.AddUniqueDynamic(this, &ThisClass::StopPointerFreeRotation);
    }
    if (Editable_Angle)
    {
        Editable_Angle->SetSelectAllTextWhenFocused(true);
        Editable_Angle->SetClearKeyboardFocusOnCommit(false);
        Editable_Angle->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleAngleCommitted);
    }
    RefreshTools();
}

// 회전 타이머와 외부 알림 연결을 해제해 닫힌 화면에 작업이 남지 않게 한다.
void UFF_BuildToolsWidget::NativeDestruct()
{
    StopPointerFreeRotation();
    if (UFF_BuildCatalogEconomyReadModel* Model = Economy.Get()) { Model->OnGoldChanged.RemoveAll(this); }
    if (UFF_RuntimeSettingsSubsystem* Settings = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UFF_RuntimeSettingsSubsystem>() : nullptr)
    {
        Settings->OnSettingsChanged.RemoveAll(this);
    }
    Economy.Reset();
    if (UFF_BuildCatalogSubsystem* Catalog = CatalogModel.Get())
    {
        Catalog->OnCatalogThumbnailsChanged.RemoveAll(this);
        Catalog->ReleaseThumbnailPreload();
    }
    CatalogModel.Reset();
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->OnBuildToolsChanged.RemoveAll(this);
    }
    BuildMode.Reset();
    Super::NativeDestruct();
}

bool UFF_BuildToolsWidget::IsPointerOverTools() const
{
    if (GetVisibility() == ESlateVisibility::Hidden ||
        GetVisibility() == ESlateVisibility::Collapsed)
    {
        return false;
    }
    // Raw BuildMode input runs before Slate's button mouse-up. Geometry also covers that
    // first mouse-down, so clicking a tool never places a building underneath the toolbar.
    for (const UBorder* Panel : { Border_Tools.Get(), Border_PlacementInfo.Get(),
        Border_BuildTopBar.Get(), Border_BuildBack.Get(), Border_BuildToolbarGold.Get(), Panel_BuildSettings.Get(),
        Border_PlacementHelp.Get() })
    {
        if (Panel && Panel->IsVisible() && (Panel->IsHovered() || (FSlateApplication::IsInitialized() &&
            Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos())))) return true;
    }
    return false;
}

bool UFF_BuildToolsWidget::IsRadialMenuOpen() const
{
    return false;
}

bool UFF_BuildToolsWidget::IsAuxiliaryPanelOpen() const
{
    return IsRadialMenuOpen() || (Panel_Details && Panel_Details->IsVisible()) ||
        (Panel_BuildSettings && Panel_BuildSettings->IsVisible());
}

bool UFF_BuildToolsWidget::IsBuildSettingsOpen() const
{
    return Panel_BuildSettings && Panel_BuildSettings->IsVisible();
}

void UFF_BuildToolsWidget::CloseRadialMenu()
{
    if (Panel_Radial) Panel_Radial->SetVisibility(ESlateVisibility::Collapsed);
    if (Button_Radial) Button_Radial->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* System = BuildMode.Get()) System->RefreshBuildToolsOcclusion();
}

void UFF_BuildToolsWidget::ToggleRadialMenu()
{
    // Keep old Blueprint callers loadable, but the removed R menu cannot reopen.
    CloseRadialMenu();
}

void UFF_BuildToolsWidget::ToggleFacts()
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get()) System->NotifyBuildToolsInteraction();
    if (Panel_PlacementFacts)
        Panel_PlacementFacts->SetVisibility(Panel_PlacementFacts->GetVisibility() == ESlateVisibility::Collapsed
            ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UFF_BuildToolsWidget::ToggleHelp()
{
    UFF_BuildModeSubsystem* System = BuildMode.Get();
    const bool bSession = System && FFBuildModeModel::IsBuildSessionActive(System->GetBuildModeState());
    if (!bSession && !ViewData.bPreviewActive) return;
    if (bSession) System->NotifyBuildToolsInteraction();
    SetHelpExpanded(Border_PlacementHelp && Border_PlacementHelp->GetVisibility() == ESlateVisibility::Collapsed);
    SetChoiceState(Button_ToolbarHelp, Cast<UTextBlock>(Button_ToolbarHelp ? Button_ToolbarHelp->GetContent() : nullptr),
        Border_PlacementHelp && Border_PlacementHelp->IsVisible());
    if (bSession) System->RefreshBuildToolsOcclusion();
}

void UFF_BuildToolsWidget::SetHelpExpanded(const bool bExpanded)
{
    if (Border_PlacementHelp)
        Border_PlacementHelp->SetVisibility(bExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (Button_Help)
        if (auto* Label = Cast<UTextBlock>(Button_Help->GetContent()))
            Label->SetText(bExpanded ? LOCTEXT("ClosePlacementHelp", "안내 닫기") : LOCTEXT("OpenPlacementHelp", "조작 안내"));
}

void UFF_BuildToolsWidget::ToggleDirectAdjustment()
{
    if (auto* System = BuildMode.Get()) System->ToggleDirectPlacementAdjustment();
}

void UFF_BuildToolsWidget::FindNearbyPlacement()
{
    if (auto* System = BuildMode.Get()) System->FindNearbyPlacement();
}

void UFF_BuildToolsWidget::ShowNextPlacementProblem()
{
    if (auto* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        const int32 Count = System->GetPlacementGuidance().ProblemCount;
        PlacementProblemIndex = Count > 0 ? (PlacementProblemIndex + 1) % Count : 0;
        RefreshTools();
    }
}

// 작은 안내 표시의 화면 위치를 갱신한다. 시설 배치 판정은 건설 시스템이 담당한다.
void UFF_BuildToolsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (IsDesignTime()) return;
    RefreshEditingHelp();
    if (Size_BuildWorldHelp && Border_PlacementHelp && Border_PlacementHelp->IsVisible())
    {
        // Use the painted catalog edge in the same logical coordinate system.
        // The old fixed 170px reservation was shorter than the 326px catalog itself.
        if (auto* HelpSlot = Cast<UCanvasPanelSlot>(Size_BuildWorldHelp->Slot))
        {
            const float Height = Geometry.GetLocalSize().Y;
            if (Height <= 0.f) return;
            const bool bCatalogOpen = BuildMode.IsValid() &&
                BuildMode->GetBuildModeState() == EFF_BuildModeState::CatalogOpen;
            float Bottom = Height - 100.f;
            if (bCatalogOpen)
            {
                // Conservative first-frame clearance before the catalog has Slate geometry.
                Bottom = Height - 450.f;
                const double Time = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
                if (!CatalogHelpAvoidanceSurface.IsValid() && Time >= NextCatalogHelpSearchTime)
                {
                    NextCatalogHelpSearchTime = Time + .5;
                    TArray<UUserWidget*> Panels;
                    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Panels,
                        UFF_PrepPanelBuild::StaticClass(), false);
                    for (UUserWidget* Panel : Panels)
                        if (Panel && Panel->GetOwningPlayer() == GetOwningPlayer() && Panel->IsVisible())
                            if (UWidget* Surface = Panel->GetWidgetFromName(TEXT("Border_PanelRoot")))
                            {
                                CatalogHelpAvoidanceSurface = Surface;
                                break;
                            }
                }
                if (const UWidget* Surface = CatalogHelpAvoidanceSurface.Get(); Surface && Surface->IsVisible())
                {
                    const FGeometry& Bounds = Surface->GetCachedGeometry();
                    if (Bounds.GetLocalSize().Y > 0.f)
                        Bottom = Geometry.AbsoluteToLocal(Bounds.LocalToAbsolute(FVector2D::ZeroVector)).Y - 16.f;
                }
            }
            if (BuildMode.IsValid() && BuildMode->IsMultiSelectionActive())
            {
                // Keep the same guide available during group edits, above the saved group panel.
                Bottom = FMath::Min(Bottom, Height - 450.f);
                const double Time = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
                if (!GroupHelpAvoidanceSurface.IsValid() && Time >= NextGroupHelpSearchTime)
                {
                    NextGroupHelpSearchTime = Time + .5;
                    TArray<UUserWidget*> Panels;
                    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Panels,
                        UFF_BuildMultiSelectionWidget::StaticClass(), false);
                    for (UUserWidget* Panel : Panels)
                        if (Panel && Panel->GetOwningPlayer() == GetOwningPlayer() && Panel->IsVisible())
                            if (UWidget* Surface = Panel->GetWidgetFromName(TEXT("Border_MultiSelection")))
                            {
                                GroupHelpAvoidanceSurface = Surface;
                                break;
                            }
                }
                if (const UWidget* Surface = GroupHelpAvoidanceSurface.Get(); Surface && Surface->IsVisible())
                {
                    const FGeometry& Bounds = Surface->GetCachedGeometry();
                    if (Bounds.GetLocalSize().Y > 0.f)
                        Bottom = Geometry.AbsoluteToLocal(Bounds.LocalToAbsolute(FVector2D::ZeroVector)).Y - 16.f;
                }
            }
            const float ContentHeight = Column_BuildIconHelp
                ? Column_BuildIconHelp->GetDesiredSize().Y + 24.f
                : Size_BuildWorldHelp->GetDesiredSize().Y;
            // The Designer owns the preferred position. Only move upward/shrink when
            // another visible panel or the viewport would cover the guide.
            const float MinimumTop = FMath::Min3(104.f, float(AuthoredHelpPosition.Y), FMath::Max(0.f, Bottom));
            const float FittedHeight = FMath::Min(FMath::Max(0.f, ContentHeight), FMath::Max(0.f, Bottom - MinimumTop));
            const FVector2D Placement(FMath::Clamp(float(AuthoredHelpPosition.Y), MinimumTop,
                FMath::Max(MinimumTop, Bottom - FittedHeight)), FittedHeight);
            const float HelpHeight = Placement.Y;
            if (!Size_BuildWorldHelp->IsHeightOverride() ||
                !FMath::IsNearlyEqual(Size_BuildWorldHelp->GetHeightOverride(), HelpHeight, .5f))
                Size_BuildWorldHelp->SetHeightOverride(HelpHeight);
            const float Top = Placement.X;
            if (!FMath::IsNearlyEqual(HelpSlot->GetPosition().Y, Top, .5f))
                HelpSlot->SetPosition(FVector2D(AuthoredHelpPosition.X, Top));
        }
    }
    if (Size_BuildSettings)
    {
        const float ViewportHeight = Geometry.GetLocalSize().Y;
        if (ViewportHeight > 0.f)
        {
            const auto* SettingsSlot = Cast<UCanvasPanelSlot>(Size_BuildSettings->Slot);
            const float AuthoredTop = SettingsSlot ? float(SettingsSlot->GetPosition().Y) : 101.f;
            const float Height = FMath::Min(AuthoredSettingsHeight, FMath::Max(100.f, ViewportHeight - AuthoredTop - 25.f));
            if (!FMath::IsNearlyEqual(Size_BuildSettings->GetHeightOverride(), Height, .5f))
                Size_BuildSettings->SetHeightOverride(Height);
        }
    }
    if (!Border_ContextPing) return;
    auto* PingSlot = Cast<UCanvasPanelSlot>(Border_ContextPing->Slot);
    FVector Target;
    FVector2D Projected;
    const UFF_BuildModeSubsystem* System = BuildMode.Get();
    const bool bShow = PingSlot && System && !System->IsInputSuspendedForPauseMenu() &&
        System->GetPlacementMarkerLocation(Target) && !IsRadialMenuOpen() && !IsPointerOverTools() &&
        UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Target, Projected, true);
    Border_ContextPing->SetVisibility(ESlateVisibility::Collapsed);
    if (!bShow) return;
    const FGeometry Screen = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(GetOwningPlayer());
    Projected = Geometry.AbsoluteToLocal(Screen.LocalToAbsolute(Projected));
    const FVector2D Size = Geometry.GetLocalSize();
    // The ping yields to every persistent surface. If no free spot exists it stays hidden;
    // the same status remains readable in the summary, never covering another control.
    const FVector2D PingSize = System->IsCurrentPlacementValid() ? FVector2D(280, 40) : FVector2D(360, 60);
    PingSlot->SetSize(PingSize);
    const float Gap = System->IsDirectPlacementAdjustment() ? 100.f : 52.f;
    const FVector2D Offsets[] = { FVector2D(Gap, -PingSize.Y - 24), FVector2D(-PingSize.X - Gap, -PingSize.Y - 24),
        FVector2D(Gap, 64), FVector2D(-PingSize.X - Gap, 64) };
    for (const FVector2D& Offset : Offsets)
    {
        const FVector2D Pos = Projected + Offset;
        if (Pos.X < 24 || Pos.Y < 40 || Pos.X + PingSize.X > Size.X - 24 || Pos.Y + PingSize.Y > Size.Y - 100) continue;
        const FSlateRect Candidate(Pos.X, Pos.Y, Pos.X + PingSize.X, Pos.Y + PingSize.Y);
        // Reserve the top-right combination preview and the bottom HUD in every state.
        if (Candidate.IntersectionWith(FSlateRect(Size.X - 490, 36, Size.X, 490)).GetArea() > 0) continue;
        bool bOccupied = false;
        for (const UBorder* Panel : { Border_Tools.Get(), Border_PlacementInfo.Get(),
            Border_BuildTopBar.Get(), Border_BuildBack.Get(), Border_BuildToolbarGold.Get(), Panel_BuildSettings.Get() })
        {
            if (!Panel || !Panel->IsVisible()) continue;
            const FGeometry& Bounds = Panel->GetCachedGeometry();
            const FVector2D Min = Geometry.AbsoluteToLocal(Bounds.LocalToAbsolute(FVector2D::ZeroVector)) - FVector2D(12);
            const FVector2D Max = Geometry.AbsoluteToLocal(Bounds.LocalToAbsolute(Bounds.GetLocalSize())) + FVector2D(12);
            if (Candidate.IntersectionWith(FSlateRect(Min.X, Min.Y, Max.X, Max.Y)).GetArea() > 0) { bOccupied = true; break; }
        }
        if (bOccupied) continue;
        PingSlot->SetPosition(Pos);
        Border_ContextPing->SetVisibility(ESlateVisibility::HitTestInvisible);
        break;
    }
}

void UFF_BuildToolsWidget::ToggleDetails()
{
    if (!Panel_Details) return;
    if (UFF_BuildModeSubsystem* System = BuildMode.Get()) System->NotifyBuildToolsInteraction();
    const bool bClosing = Panel_Details->GetVisibility() != ESlateVisibility::Collapsed;
    CloseRadialMenu();
    if (bClosing)
    {
        if (!ApplyPendingAngle()) return;
        StopPointerFreeRotation();
        if (Editable_Angle && Editable_Angle->HasKeyboardFocus())
            FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
    Panel_Details->SetVisibility(bClosing ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    if (auto* System = BuildMode.Get()) System->RefreshBuildToolsOcclusion();
    if (Button_Details)
        if (UTextBlock* Label = Cast<UTextBlock>(Button_Details->GetContent()))
            Label->SetText(bClosing ? LOCTEXT("DetailsOpen", "세부 조정 +") : LOCTEXT("DetailsClose", "세부 조정 −"));
}

void UFF_BuildToolsWidget::ToggleBuildSettings()
{
    UFF_BuildModeSubsystem* System = BuildMode.Get();
    if (!System || !Panel_BuildSettings ||
        !FFBuildModeModel::IsBuildSessionActive(System->GetBuildModeState())) return;
    System->NotifyBuildToolsInteraction();
    const bool bClosing = Panel_BuildSettings->IsVisible();
    if (bClosing && !ApplyPendingAngle())
    {
        // Closing the drawer must still work when an unfinished angle is invalid.
        // Discard that edit and restore the current placement angle.
        if (Editable_Angle) Editable_Angle->SetError(FText::GetEmpty());
        RefreshAngleText();
    }
    if (bClosing)
    {
        StopPointerFreeRotation();
        if (Editable_Angle && Editable_Angle->HasKeyboardFocus())
            FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
    if (!bClosing)
    {
        bRestoreHelpAfterSettings = Border_PlacementHelp && Border_PlacementHelp->IsVisible();
        SetHelpExpanded(false);
    }
    Panel_BuildSettings->SetVisibility(bClosing ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (Border_PlacementInfo)
        Border_PlacementInfo->SetVisibility(ViewData.bPreviewActive && bClosing
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bClosing)
    {
        if (bRestoreHelpAfterSettings) SetHelpExpanded(true);
        bRestoreHelpAfterSettings = false;
    }
    SetChoiceState(Button_BuildSettings,
        Cast<UTextBlock>(Button_BuildSettings ? Button_BuildSettings->GetContent() : nullptr), !bClosing);
    System->RefreshBuildToolsOcclusion();
}

bool UFF_BuildToolsWidget::HandleToolbarKey(const FKey& Key)
{
    // Compatibility entry point only: toolbar actions are activated by buttons.
    return false;
}

void UFF_BuildToolsWidget::HandleMultiSelect()
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
        System->ToggleMultiSelection();
}

void UFF_BuildToolsWidget::HandleSnapCycle()
{
    if (const UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        const int32 Current = System->GetPreferredPositionSnapCm();
        SetSnap(Current == 100 ? 25 : Current == 25 ? 1 : 100);
    }
}

void UFF_BuildToolsWidget::HandleBuildBack()
{
    UFF_BuildModeSubsystem* System = BuildMode.Get();
    if (!System) return;
    if (Panel_BuildSettings && Panel_BuildSettings->IsVisible())
    {
        ToggleBuildSettings();
        return;
    }
    System->NotifyBuildToolsInteraction();
    switch (System->GetBuildModeState())
    {
    case EFF_BuildModeState::Placing:
        System->ReturnToCatalog();
        break;
    case EFF_BuildModeState::Moving:
    case EFF_BuildModeState::CatalogOpen:
        System->EnterBuildMode();
        break;
    case EFF_BuildModeState::BuildMode:
        System->ExitBuildMode();
        break;
    default:
        break;
    }
}

void UFF_BuildToolsWidget::HandleFacilityRanges()
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        System->SetFacilityRangesVisible(!System->AreFacilityRangesVisible());
    }
}

void UFF_BuildToolsWidget::HandleSynergyPreview()
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        System->SetSynergyPreviewVisible(!System->IsSynergyPreviewVisible());
    }
}

void UFF_BuildToolsWidget::HandleZoomToCursor()
{
    if (UFF_RuntimeSettingsSubsystem* Settings = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UFF_RuntimeSettingsSubsystem>() : nullptr)
    {
        Settings->SetZoomToCursorEnabled(!Settings->IsZoomToCursorEnabled());
        RefreshTools();
    }
}

void UFF_BuildToolsWidget::HandleEdgeScroll()
{
    if (UFF_RuntimeSettingsSubsystem* Settings = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UFF_RuntimeSettingsSubsystem>() : nullptr)
    {
        Settings->SetEdgeScrollEnabled(!Settings->IsEdgeScrollEnabled());
        RefreshTools();
    }
}

bool UFF_BuildToolsWidget::IsEditingAngle() const
{
    return Editable_Angle && Editable_Angle->GetIsEnabled() &&
        (Editable_Angle->HasKeyboardFocus() || Editable_Angle->HasFocusedDescendants());
}

FReply UFF_BuildToolsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (IsEditingAngle() && KeyEvent.GetKey() == EKeys::Escape)
    {
        if (UFF_BuildModeSubsystem* System = BuildMode.Get())
        {
            System->NotifyBuildToolsInteraction();
        }
        Editable_Angle->SetError(FText::GetEmpty());
        RefreshAngleText();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

void UFF_BuildToolsWidget::SetChoiceState(UButton* Button, UTextBlock* Label, const bool bSelected)
{
    if (UFF_BuildToolsActionButton* Styled = Cast<UFF_BuildToolsActionButton>(Button))
    { Styled->ApplySelection(bSelected, Label); }
}

void UFF_BuildToolsWidget::RefreshAngleText()
{
    if (Editable_Angle)
    {
        const UFF_BuildModeSubsystem* System = BuildMode.Get();
        Editable_Angle->SetText(FText::FromString(FString::Printf(TEXT("%.2f"),
            System ? System->GetPlacementYawDegrees() : 0.0f)));
    }
}

bool UFF_BuildToolsWidget::ApplyAngleText(const FText& Text)
{
    UFF_BuildModeSubsystem* System = BuildMode.Get();
    if (!System)
    {
        return false;
    }
    float Yaw = 0.0f;
    const bool bValid = FFBuildToolsModel::TryParseYawText(Text.ToString(), Yaw);
    if (Editable_Angle)
    {
        Editable_Angle->SetError(bValid ? FText::GetEmpty() :
            LOCTEXT("InvalidAngle", "0~360 사이의 숫자를 입력해 주세요."));
    }
    if (!bValid)
    {
        return false;
    }
    System->SetPlacementYawDegrees(Yaw);
    RefreshAngleText();
    return true;
}

bool UFF_BuildToolsWidget::ApplyPendingAngle()
{
    // Action buttons deliberately do not steal focus. Apply the current editor buffer here,
    // because clicking Confirm/Rotate need not produce a text-focus-loss commit first.
    return !IsEditingAngle() || ApplyAngleText(Editable_Angle->GetText());
}

// 현재 시설·가격·잔액·배치 가능 여부를 한 번 읽어 버튼과 안내 문구에 반영한다.
void UFF_BuildToolsWidget::RefreshTools()
{
    const UFF_BuildModeSubsystem* System = BuildMode.Get();
    const EFF_BuildModeState State = System ? System->GetBuildModeState() : EFF_BuildModeState::Closed;
    const bool bPreview = FFBuildModeModel::IsPreviewActive(State);
    // Also retire the button for older, not-yet-migrated widget assets.
    if (Button_Radial) Button_Radial->SetVisibility(ESlateVisibility::Collapsed);
    if (Panel_Radial) Panel_Radial->SetVisibility(ESlateVisibility::Collapsed);
    // The placement workspace no longer has a duplicate wallet at the top.
    // Collapse the whole surface so legacy saved widgets also release its hit area.
    if (Border_BuildToolbarGold) Border_BuildToolbarGold->SetVisibility(ESlateVisibility::Collapsed);
    const bool bStartingPreview = bPreview && !ViewData.bPreviewActive;
    if (bStartingPreview || !bPreview || (System && System->IsCurrentPlacementValid())) PlacementProblemIndex = 0;
    // The compact top bar also serves catalog browsing. Placement summary and confirm
    // controls remain visible only while a world preview is active.
    const bool bSession = System && FFBuildModeModel::IsBuildSessionActive(State);
    SetVisibility(bSession && !System->IsInputSuspendedForPauseMenu()
        ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (Label_BuildMode)
        Label_BuildMode->SetText(System && System->IsMultiSelectionActive()
            ? LOCTEXT("BuildModeMultiSelectionTitle", "다중 선택")
            : LOCTEXT("BuildModeTitle", "건설"));
    if (Border_BuildTopBar)
        Border_BuildTopBar->SetVisibility(bSession ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Border_BuildBack)
        Border_BuildBack->SetVisibility(bSession ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Text_BuildBack)
        Text_BuildBack->SetText(LOCTEXT("BackGlyph", "←"));
    if (Button_BuildBack)
        Button_BuildBack->SetToolTipText(State == EFF_BuildModeState::CatalogOpen
            ? LOCTEXT("BackToWorld", "건축 화면으로")
            : State == EFF_BuildModeState::BuildMode
            ? LOCTEXT("BackToFestival", "축제로 돌아가기")
            : LOCTEXT("BackToCatalog", "시설 목록으로"));
    if (Border_Tools)
        Border_Tools->SetVisibility(bPreview ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Border_PlacementInfo)
        Border_PlacementInfo->SetVisibility(bPreview && !(Panel_BuildSettings && Panel_BuildSettings->IsVisible())
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (!bPreview || (System && System->IsInputSuspendedForPauseMenu())) { StopPointerFreeRotation(); }
    if (!bPreview)
    {
        CloseRadialMenu();
        if (Panel_Details) Panel_Details->SetVisibility(ESlateVisibility::Collapsed);
        if (!bSession && Panel_BuildSettings) Panel_BuildSettings->SetVisibility(ESlateVisibility::Collapsed);
        if (!bSession) bRestoreHelpAfterSettings = false;
        if (Panel_PlacementFacts) Panel_PlacementFacts->SetVisibility(ESlateVisibility::Collapsed);
        if (!bSession) SetHelpExpanded(false);
    }
    // Open once on entry. Cursor, rotation and economy refreshes preserve a user's dismissal.
    else if (bStartingPreview)
    {
        if (Panel_BuildSettings && Panel_BuildSettings->IsVisible()) bRestoreHelpAfterSettings = true;
        else SetHelpExpanded(true);
    }
    const bool bGrid = System && System->IsPlacementGridVisible();
    const int32 PreferredSnap = System ? System->GetPreferredPositionSnapCm() : 100;
    const int32 ActiveSnap = System ? System->GetActivePositionSnapCm() : PreferredSnap;
    ViewData.bPreviewActive = bPreview;
    ViewData.bMoving = State == EFF_BuildModeState::Moving;
    ViewData.bDragPlacementEnabled = System && System->IsDragPlacementEnabled() && !System->IsDirectPlacementAdjustment();
    ViewData.bGridVisible = bGrid;
    ViewData.PreferredSnapCm = PreferredSnap;
    ViewData.ActiveSnapCm = ActiveSnap;
    ViewData.YawDegrees = System ? System->GetPlacementYawDegrees() : 0.0f;
    ViewData.FacilityName = System && !System->GetSelectedFacilityDisplayName().IsEmpty()
        ? System->GetSelectedFacilityDisplayName() : LOCTEXT("NoFacility", "시설을 선택해 주세요");
    ViewData.CostText = ViewData.bMoving ? LOCTEXT("MoveCost", "이동 비용 없음") : FText::GetEmpty();
    ViewData.BalanceText = LOCTEXT("BalanceUnknown", "보유 골드 확인 중");
    ViewData.FootprintText = FText::GetEmpty();
    ViewData.QueueText = FText::GetEmpty();
    if (System && bPreview)
    {
        const FFFZSFacilitySpec& Spec = System->GetSelectedFacilitySpec();
        const FFFZSQueueRules Queue = System->GetSelectedFacilityQueueRules();
        ViewData.FootprintText = FText::Format(LOCTEXT("FootprintInfo", "시설 크기  {0} × {1} m\n안전 여유  {2} m"),
            FText::AsNumber(Spec.FootprintSizeCells.X / 100.0), FText::AsNumber(Spec.FootprintSizeCells.Y / 100.0),
            FText::AsNumber(Spec.ClearanceCells / 100.0));
        ViewData.QueueText = FText::Format(LOCTEXT("QueueInfo", "대기·이용 정원  {0}팀\n동시 이용  {1}팀  ·  팀당 이용  {2}초"),
            FText::AsNumber(Queue.MaxWaitingAgents), FText::AsNumber(Queue.ServiceCapacity),
            FText::AsNumber(Queue.ServiceDurationSeconds));
    }
    UFF_BuildCatalogSubsystem* Catalog = CatalogModel.Get();
    FFF_BuildCatalogEntry Entry;
    if (System && Catalog && Catalog->FindEntry(System->GetSelectedFacilityDefinitionId(), Entry))
    {
        ViewData.FacilityName = Entry.DisplayName;
        FString Failure;
        const bool bCostKnown = Economy.IsValid() && Economy->TryProjectFacilityCosts(Entry, Entry.Definition.Get(), Failure);
        ViewData.CostText = ViewData.bMoving ? LOCTEXT("MoveCost", "이동 비용 없음") : bCostKnown
            ? FText::Format(LOCTEXT("InstallCost", "설치 {0} Gold"), FText::AsNumber(Entry.InstallCost))
            : LOCTEXT("UnknownCost", "설치 비용 확인 필요");
        if (Economy.IsValid() && Economy->HasAuthoritativeGold())
        {
            ViewData.BalanceText = bCostKnown || ViewData.bMoving
                ? FText::Format(LOCTEXT("PlacementBalance", "보유 {0} Gold\n확정 후 {1} Gold"),
                    FText::AsNumber(Economy->GetGold()), FText::AsNumber(Economy->GetGold() - (ViewData.bMoving ? 0 : Entry.InstallCost)))
                : FText::Format(LOCTEXT("CurrentBalanceOnly", "보유 {0} Gold"), FText::AsNumber(Economy->GetGold()));
        }
    }
    if (Text_FacilityName) { Text_FacilityName->SetText(ViewData.FacilityName); }
    if (Text_Cost) { Text_Cost->SetText(ViewData.CostText); }
    if (Text_Balance) Text_Balance->SetText(ViewData.BalanceText);
    if (Text_Footprint) Text_Footprint->SetText(ViewData.FootprintText);
    if (Text_Queue) Text_Queue->SetText(ViewData.QueueText);
    // The saved WBP keeps this named legacy binding collapsed for older
    // Blueprint references. The visible guide pairs physical keycaps with actions.
    if (Text_ControlHelp) Text_ControlHelp->SetText(FText::GetEmpty());
    RefreshEditingHelp();
    const bool bDirectHelp = System && System->IsDirectPlacementAdjustment();
    if (Text_BuildHelpPrimary)
        Text_BuildHelpPrimary->SetText(!bPreview
            ? LOCTEXT("IconHelpInspect", "시설 클릭해 정보 보기")
            : bDirectHelp ? LOCTEXT("KeyHelpGizmoDrag", "핸들을 드래그해 조정")
            : ViewData.bDragPlacementEnabled ? LOCTEXT("IconHelpDrag", "드래그로 연속 설치")
            : LOCTEXT("IconHelpPlace", "클릭해 시설 배치"));
    if (Text_BuildHelpRotate)
        Text_BuildHelpRotate->SetText(ViewData.bDragPlacementEnabled
            ? LOCTEXT("IconHelpEndDrag", "회전하면 연속 배치 종료")
            : LOCTEXT("IconHelpRotateFacility", "시설 방향 회전"));
    if (Text_BuildHelpMove)
        Text_BuildHelpMove->SetText(LOCTEXT("IconHelpMoveCamera", "카메라 이동"));
    if (Text_BuildHelpPrecision)
        Text_BuildHelpPrecision->SetText(LOCTEXT("KeyHelpPrecisionHold", "누르는 동안 정밀 맞춤"));
    // Camera and option keys are user-remappable; show the saved input, not a stale default.
    if (ULocalPlayer* Player = GetOwningLocalPlayer())
        if (const auto* Bindings = Player->GetSubsystem<UFF_KeyBindingRuntimeSubsystem>())
        {
            const TPair<const TCHAR*, const TCHAR*> Keys[] = {
                {TEXT("W"), TEXT("FF.Camera.Move.Forward")}, {TEXT("A"), TEXT("FF.Camera.Move.Left")},
                {TEXT("S"), TEXT("FF.Camera.Move.Backward")}, {TEXT("D"), TEXT("FF.Camera.Move.Right")},
                {TEXT("Esc"), TEXT("FF.System.Options.Primary")}};
            for (const auto& Key : Keys)
                if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(
                    FName(*(FString(TEXT("Text_BuildHelpKey_")) + Key.Key)))))
                {
                    const FKey Current = Bindings->GetCurrentKey(FName(Key.Value));
                    Text->SetText(Current == EKeys::Escape ? FText::FromString(TEXT("Esc")) : Current.GetDisplayName());
                }
        }
    if (Row_BuildHelpRotate)
        Row_BuildHelpRotate->SetVisibility(bPreview
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Row_BuildHelpPrecision)
        Row_BuildHelpPrecision->SetVisibility(bPreview
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Text_ActiveSnap)
    {
        Text_ActiveSnap->SetText(FText::Format(ActiveSnap == PreferredSnap
            ? LOCTEXT("ActiveSnap", "현재 {0}cm") : LOCTEXT("TemporarySnap", "임시 {0}cm 적용 중"), FText::AsNumber(ActiveSnap)));
    }
    if (Button_Catalog)
        if (auto* CatalogLabel = Cast<UTextBlock>(Button_Catalog->GetContent()))
            CatalogLabel->SetText(LOCTEXT("PlacementCatalogAction", "시설 목록"));
    if (Text_RotationHint)
    {
        Text_RotationHint->SetText(FText::Format(ViewData.bDragPlacementEnabled
            ? LOCTEXT("DragRotationHint", "{0}°씩 회전 · 회전 시 연속 배치 종료")
            : LOCTEXT("RotationHint", "{0}°씩 회전 · 자유 회전 지원"),
            FText::AsNumber(System ? System->GetPlacementRotationSnapDegrees() : 45.0f)));
    }
    // The existing WBP owns this named row; no Blueprint regeneration is needed.
    if (UTextBlock* PrecisionHint = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_PrecisionHint"))))
    {
        PrecisionHint->SetText(ViewData.bDragPlacementEnabled
            ? LOCTEXT("DragPrecisionHint", "첫 위치를 정밀 조정한 뒤 연속 배치하세요")
            : LOCTEXT("PrecisionHint", "정밀 조정 후 배치 확정 버튼을 누르세요"));
    }
    if (Text_Mode)
    {
        Text_Mode->SetText(State == EFF_BuildModeState::Moving ? LOCTEXT("ModeMoving", "시설 이동") :
            ViewData.bDragPlacementEnabled ? LOCTEXT("ModeDragPlacement", "시설 연속 배치") :
            bPreview ? LOCTEXT("ModePlacement", "시설 배치") : LOCTEXT("ModeOverview", "시설 확인"));
        Text_Mode->SetToolTipText(bPreview
            ? UFF_BuildModeGuide::MakePlacementHelpText(ViewData.bMoving, ViewData.bDragPlacementEnabled)
            : UFF_BuildModeGuide::MakeBuildOverviewHelpText());
    }
    {
        const FFF_BuildPlacementGuidance Guidance = System ? System->GetPlacementGuidance(PlacementProblemIndex) : FFF_BuildPlacementGuidance{};
        PlacementProblemIndex = Guidance.ProblemIndex;
        FText Status = LOCTEXT("OverviewLegend", "시설 클릭: 정보 확인 · 격자: 빈 건축 공간");
        FSlateColor Color = NeutralStatusColor;
        if (bPreview && !System->HasPlacementTarget())
        {
            Status = LOCTEXT("NoTarget", "건축 구역의 바닥 위로 마우스를 옮겨 주세요");
        }
        else if (bPreview && System->IsCurrentPlacementValid())
        {
            Status = ViewData.bDragPlacementEnabled
                ? LOCTEXT("DragPlacementReady", "배치 가능 · 드래그로 연속 설치")
                : FText::Format(LOCTEXT("PlacementReady", "배치 가능 · 현재 {0}cm 맞춤"), FText::AsNumber(ActiveSnap));
            Color = ReadyStatusColor;
        }
        else if (bPreview)
        {
            Status = Guidance.ProblemCount > 1
                ? FText::Format(LOCTEXT("MultipleProblems", "{0}  ·  {1}/{2}"), Guidance.Title,
                    FText::AsNumber(Guidance.ProblemIndex + 1), FText::AsNumber(Guidance.ProblemCount)) : Guidance.Title;
            Color = InvalidStatusColor;
        }
        ViewData.StatusText = Status;
        if (Text_ContextPing)
        {
            Text_ContextPing->SetText(System && System->IsCurrentPlacementValid()
                ? System->IsDirectPlacementAdjustment() ? LOCTEXT("PingPinned", "위치 조정 완료 · 배치 확정")
                    : LOCTEXT("PingReady", "배치 가능 · 눌러서 설치") : Guidance.Title);
            Text_ContextPing->SetColorAndOpacity(Color);
        }
        if (Text_Legend)
        {
            Text_Legend->SetText(Status);
            Text_Legend->SetColorAndOpacity(Color);
            Text_Legend->SetToolTipText(Status);
        }
        const auto SetOptional = [](UTextBlock* Text, const FText& Value)
        {
            if (!Text) return;
            Text->SetText(Value);
            Text->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        };
        SetOptional(Text_PlacementAdvice, bPreview && (!System->IsCurrentPlacementValid() || System->IsDirectPlacementAdjustment()) ? Guidance.Advice : FText::GetEmpty());
        SetOptional(Text_PlacementObstruction, Guidance.Obstruction);
        SetOptional(Text_AssistanceMessage, System ? System->GetPlacementAssistanceMessage() : FText::GetEmpty());
        if (Button_DirectAdjustment)
        {
            Button_DirectAdjustment->SetIsEnabled(bPreview && System->HasPlacementTarget());
            if (auto* Label = Cast<UTextBlock>(Button_DirectAdjustment->GetContent()))
            {
                const bool bDirect = System && System->IsDirectPlacementAdjustment();
                Label->SetText(bDirect ? LOCTEXT("FollowCursor", "위치 다시 선택") : LOCTEXT("AdjustDirect", "직접 조정"));
                SetChoiceState(Button_DirectAdjustment, Label, bDirect);
            }
        }
        if (Button_FindNearby)
        {
            const bool bFinding = System && System->IsFindingNearbyPlacement();
            Button_FindNearby->SetVisibility(Guidance.bCanFindNearby || bFinding ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            Button_FindNearby->SetIsEnabled(!bFinding);
            if (auto* Label = Cast<UTextBlock>(Button_FindNearby->GetContent()))
                Label->SetText(bFinding ? LOCTEXT("FindingNearby", "찾는 중…") : LOCTEXT("FindNearby", "가까운 빈자리"));
        }
        if (Button_NextPlacementProblem)
            Button_NextPlacementProblem->SetVisibility(Guidance.ProblemCount > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (Text_Grid)
    {
        Text_Grid->SetText(FText::GetEmpty()); // The visible F2 icon is a bounded texture.
    }
    if (Image_PlacementFacility)
    {
        UTexture2D* Thumbnail = bPreview ? Entry.Thumbnail.Get() : nullptr;
        Image_PlacementFacility->SetBrushFromTexture(Thumbnail, false);
        Image_PlacementFacility->SetVisibility(Thumbnail
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    const auto Enable = [](UWidget* Widget, const bool bEnabled)
    {
        if (Widget) { Widget->SetIsEnabled(bEnabled); }
    };
    Enable(Button_Grid, bSession);
    Enable(Button_MultiSelect, bSession);
    Enable(Button_SnapCycle, bSession);
    Enable(Button_ToolbarRanges, bSession);
    Enable(Button_ToolbarCatalog, bSession);
    Enable(Button_ToolbarHelp, bSession);
    Enable(Button_BuildBack, bSession);
    Enable(Button_BuildSettings, bSession);
    Enable(Button_BuildSettingsClose, bSession);
    Enable(Button_SettingsGrid, bSession);
    Enable(Button_SettingsFacilityRanges, bSession);
    Enable(Button_SettingsSynergyPreview, bSession);
    Enable(Button_SettingsDirectAdjustment, bPreview && System->HasPlacementTarget());
    const UFF_RuntimeSettingsSubsystem* RuntimeSettings = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UFF_RuntimeSettingsSubsystem>() : nullptr;
    Enable(Button_SettingsZoomToCursor, bSession && RuntimeSettings);
    Enable(Button_SettingsEdgeScroll, bSession && RuntimeSettings);
    const bool bCanConfirm = bPreview && System->HasPlacementTarget() && System->IsCurrentPlacementValid();
    ViewData.bCanConfirm = bCanConfirm;
    Enable(Button_Confirm, bCanConfirm);
    if (Text_Confirm)
    {
        Text_Confirm->SetText(State == EFF_BuildModeState::Moving
            ? LOCTEXT("ConfirmMove", "이동 확정") : LOCTEXT("ConfirmInstall", "배치 확정"));
    }
    Enable(Button_SnapMeter, bSession);
    Enable(Button_SnapPrecision, bSession);
    Enable(Button_SnapFree, bSession);
    Enable(Button_SettingsSnapMeter, bSession);
    Enable(Button_SettingsSnapPrecision, bSession);
    Enable(Button_SettingsSnapFree, bSession);
    Enable(Button_RotateLeft, bPreview);
    Enable(Button_RotateRight, bPreview);
    Enable(Button_FreeRotateLeft, bPreview);
    Enable(Button_FreeRotateRight, bPreview);
    Enable(Button_Catalog, bSession);
    Enable(Editable_Angle, bPreview);
    Enable(Button_NudgeLeft, bPreview);
    Enable(Button_NudgeUp, bPreview);
    Enable(Button_NudgeDown, bPreview);
    Enable(Button_NudgeRight, bPreview);
    SetChoiceState(Button_Grid, Text_Grid, bGrid);
    if (Button_Grid) Button_Grid->SetToolTipText(bGrid
        ? LOCTEXT("GridDisableTip", "건축 격자 숨기기")
        : LOCTEXT("GridEnableTip", "건축 격자 표시"));
    if (Button_MultiSelect)
    {
        const int32 Count = System ? System->GetMultiSelectionCount() : 0;
        Button_MultiSelect->SetToolTipText(Count > 0
            ? FText::Format(LOCTEXT("MultiSelectCount", "다중 선택 · {0}개 선택"), FText::AsNumber(Count))
            : LOCTEXT("MultiSelectTip", "시설 다중 선택"));
    }
    SetChoiceState(Button_MultiSelect, Text_MultiSelect, System && System->IsMultiSelectionActive());
    if (Button_SnapCycle) Button_SnapCycle->SetToolTipText(FText::Format(
        LOCTEXT("SnapCycleTip", "위치 간격 {0}cm · 클릭해 변경"), FText::AsNumber(PreferredSnap)));
    SetChoiceState(Button_SnapCycle, Text_SnapCycle, PreferredSnap != 100);
    if (Text_SettingsGrid)
        Text_SettingsGrid->SetText(bGrid ? LOCTEXT("SettingsGridOn", "켜짐") : LOCTEXT("SettingsGridOff", "꺼짐"));
    SetChoiceState(Button_SettingsGrid, Text_SettingsGrid, bGrid);
    const bool bRanges = System && System->AreFacilityRangesVisible();
    if (Text_SettingsFacilityRanges)
        Text_SettingsFacilityRanges->SetText(bRanges
            ? LOCTEXT("SettingsRangesOn", "켜짐") : LOCTEXT("SettingsRangesOff", "꺼짐"));
    SetChoiceState(Button_SettingsFacilityRanges, Text_SettingsFacilityRanges, bRanges);
    SetChoiceState(Button_ToolbarRanges, Text_ToolbarRanges, bRanges);
    const bool bSynergy = System && System->IsSynergyPreviewVisible();
    if (Text_SettingsSynergyPreview)
        Text_SettingsSynergyPreview->SetText(bSynergy
            ? LOCTEXT("SettingsSynergyOn", "켜짐") : LOCTEXT("SettingsSynergyOff", "꺼짐"));
    SetChoiceState(Button_SettingsSynergyPreview, Text_SettingsSynergyPreview, bSynergy);
    const bool bDirect = System && System->IsDirectPlacementAdjustment();
    if (Text_SettingsDirectAdjustment)
        Text_SettingsDirectAdjustment->SetText(bDirect
            ? LOCTEXT("SettingsDirectOn", "켜짐") : LOCTEXT("SettingsDirectOff", "꺼짐"));
    SetChoiceState(Button_SettingsDirectAdjustment, Text_SettingsDirectAdjustment, bDirect);
    const bool bZoom = RuntimeSettings && RuntimeSettings->IsZoomToCursorEnabled();
    if (Text_SettingsZoomToCursor)
        Text_SettingsZoomToCursor->SetText(bZoom
            ? LOCTEXT("SettingsZoomOn", "켜짐") : LOCTEXT("SettingsZoomOff", "꺼짐"));
    SetChoiceState(Button_SettingsZoomToCursor, Text_SettingsZoomToCursor, bZoom);
    const bool bEdge = RuntimeSettings && RuntimeSettings->IsEdgeScrollEnabled();
    if (Text_SettingsEdgeScroll)
        Text_SettingsEdgeScroll->SetText(bEdge
            ? LOCTEXT("SettingsEdgeOn", "켜짐") : LOCTEXT("SettingsEdgeOff", "꺼짐"));
    SetChoiceState(Button_SettingsEdgeScroll, Text_SettingsEdgeScroll, bEdge);
    SetChoiceState(Button_BuildSettings, Cast<UTextBlock>(Button_BuildSettings ? Button_BuildSettings->GetContent() : nullptr),
        Panel_BuildSettings && Panel_BuildSettings->IsVisible());
    SetChoiceState(Button_ToolbarCatalog, Cast<UTextBlock>(Button_ToolbarCatalog ? Button_ToolbarCatalog->GetContent() : nullptr),
        State == EFF_BuildModeState::CatalogOpen);
    SetChoiceState(Button_ToolbarHelp, Cast<UTextBlock>(Button_ToolbarHelp ? Button_ToolbarHelp->GetContent() : nullptr),
        Border_PlacementHelp && Border_PlacementHelp->IsVisible());
    SetChoiceState(Button_Confirm, Text_Confirm, bCanConfirm);
    SetChoiceState(Button_SnapMeter, Text_SnapMeter, PreferredSnap == 100);
    SetChoiceState(Button_SnapPrecision, Text_SnapPrecision, PreferredSnap == 25);
    SetChoiceState(Button_SnapFree, Text_SnapFree, PreferredSnap == 1);
    SetChoiceState(Button_SettingsSnapMeter, Text_SettingsSnapMeter, PreferredSnap == 100);
    SetChoiceState(Button_SettingsSnapPrecision, Text_SettingsSnapPrecision, PreferredSnap == 25);
    SetChoiceState(Button_SettingsSnapFree, Text_SettingsSnapFree, PreferredSnap == 1);
    if (!IsEditingAngle())
    {
        RefreshAngleText();
    }
    BP_OnToolsStateChanged(ViewData);
}

void UFF_BuildToolsWidget::HandleGrid()
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        System->SetPlacementGridVisible(!System->IsPlacementGridVisible());
        FFBuildFeedback::Pulse(GetOwningPlayer());
        CloseRadialMenu();
    }
}

// 입력 중인 각도를 먼저 반영한 다음 건설 시스템에 배치 확정을 요청한다.
void UFF_BuildToolsWidget::HandleConfirm()
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        if (!ApplyPendingAngle())
        {
            return;
        }
        if (FFBuildModeModel::IsPreviewActive(System->GetBuildModeState()) &&
            System->HasPlacementTarget() && System->IsCurrentPlacementValid())
        {
            System->CommitPlacement();
        }
    }
}

void UFF_BuildToolsWidget::SetSnap(const int32 StepCm)
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        System->SetPreferredPositionSnapCm(StepCm);
        FFBuildFeedback::Pulse(GetOwningPlayer());
        CloseRadialMenu();
    }
}

void UFF_BuildToolsWidget::RotateToNextSnap(const int32 Direction)
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        if (!ApplyPendingAngle())
        {
            return;
        }
        System->SetPlacementYawDegrees(FFBuildModeModel::ResolveNextSnappedYawDegrees(
            System->GetPlacementYawDegrees(), System->GetPlacementRotationSnapDegrees(), Direction));
        RefreshAngleText();
        FFBuildFeedback::Pulse(GetOwningPlayer());
        CloseRadialMenu();
    }
}

void UFF_BuildToolsWidget::Nudge(const FIntPoint Direction)
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        System->NudgePlacement(Direction);
    }
}

// 아래 짧은 함수들은 각 버튼을 공통 위치 맞춤·회전·이동 처리에 연결한다.
void UFF_BuildToolsWidget::HandleSnapMeter() { SetSnap(100); }
void UFF_BuildToolsWidget::HandleSnapPrecision() { SetSnap(25); }
void UFF_BuildToolsWidget::HandleSnapFree() { SetSnap(1); }
void UFF_BuildToolsWidget::HandleRotateLeft() { RotateToNextSnap(-1); }
void UFF_BuildToolsWidget::HandleRotateRight() { RotateToNextSnap(1); }
void UFF_BuildToolsWidget::HandleNudgeLeft() { Nudge(FIntPoint(-1, 0)); }
void UFF_BuildToolsWidget::HandleNudgeUp() { Nudge(FIntPoint(0, 1)); }
void UFF_BuildToolsWidget::HandleNudgeDown() { Nudge(FIntPoint(0, -1)); }
void UFF_BuildToolsWidget::HandleNudgeRight() { Nudge(FIntPoint(1, 0)); }

void UFF_BuildToolsWidget::ConfirmPlacement() { HandleConfirm(); }
void UFF_BuildToolsWidget::ToggleGrid() { HandleGrid(); }
void UFF_BuildToolsWidget::SelectSnapStep(const int32 StepCm) { SetSnap(StepCm); }
void UFF_BuildToolsWidget::MoveInZoneAxes(const FIntPoint Direction) { Nudge(Direction); }
bool UFF_BuildToolsWidget::ApplyYawText(const FText& Text)
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get()) { System->NotifyBuildToolsInteraction(); }
    return ApplyAngleText(Text);
}
void UFF_BuildToolsWidget::RefreshEditingHelp()
{
    const UFF_BuildModeSubsystem* System = BuildMode.Get();
    const UFFZSWorldSubsystem* Zones = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    const bool bCanInteract = System && !System->IsInputSuspendedForPauseMenu() &&
        FFBuildModeModel::IsBuildSessionActive(System->GetBuildModeState());
    if (Button_BuildHelpUndo) Button_BuildHelpUndo->SetIsEnabled(bCanInteract && Zones && Zones->CanUndoConstruction());
    if (Button_BuildHelpRedo) Button_BuildHelpRedo->SetIsEnabled(bCanInteract && Zones && Zones->CanRedoConstruction());
    const bool bQueueAvailable = bCanInteract && System->CanChooseQueueLayout();
    if (Row_BuildHelpQueue)
        Row_BuildHelpQueue->SetVisibility(bQueueAvailable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Text_BuildHelpQueue) Text_BuildHelpQueue->SetText(LOCTEXT("QueueShapeAction", "대기열 모양 선택"));
    if (Text_BuildHelpQueueHint && bQueueAvailable)
    {
        const FText Hint = FText::Format(LOCTEXT("QueueShapeKeysAndCurrent", "7 기본 · 8 좌측 · 9 우측 · 0 U자\n{0}"),
            System->GetQueueLayoutKeyHint());
        if (!Text_BuildHelpQueueHint->GetText().EqualTo(Hint)) Text_BuildHelpQueueHint->SetText(Hint);
    }
}

void UFF_BuildToolsWidget::HandleUndoConstruction()
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get()) System->UndoBuildEdit();
    RefreshTools();
}

void UFF_BuildToolsWidget::HandleRedoConstruction()
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get()) System->RedoBuildEdit();
    RefreshTools();
}

void UFF_BuildToolsWidget::HandleGoldChanged(const int32 Gold) { RefreshTools(); }

void UFF_BuildToolsWidget::ReturnToCatalog()
{
    StopPointerFreeRotation();
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        System->ReturnToCatalog();
    }
}

void UFF_BuildToolsWidget::HandleFreeRotateLeftPressed() { StartPointerFreeRotation(-1); }
void UFF_BuildToolsWidget::HandleFreeRotateRightPressed() { StartPointerFreeRotation(1); }

// 회전 버튼을 누르는 동안만 주기적으로 각도를 바꾸도록 시작한다.
void UFF_BuildToolsWidget::StartPointerFreeRotation(const int32 Direction)
{
    StopPointerFreeRotation();
    UFF_BuildModeSubsystem* System = BuildMode.Get();
    if (!System || !GetWorld() || !FFBuildModeModel::IsPreviewActive(System->GetBuildModeState()) ||
        System->IsInputSuspendedForPauseMenu() || !ApplyPendingAngle()) { return; }
    System->NotifyBuildToolsInteraction();
    PointerRotationDirection = Direction;
    LastPointerRotationTime = FPlatformTime::Seconds();
    // A timer exists only for a held pointer button. Keyboard gestures remain owned by
    // BuildMode; both routes use its configured speed and authoritative rotation command.
    GetWorld()->GetTimerManager().SetTimer(FreeRotationTimer, this, &ThisClass::AdvancePointerFreeRotation, 1.0f / 60.0f, true);
}

void UFF_BuildToolsWidget::AdvancePointerFreeRotation()
{
    UFF_BuildModeSubsystem* System = BuildMode.Get();
    const UButton* Held = PointerRotationDirection < 0 ? Button_FreeRotateLeft : Button_FreeRotateRight;
    if (!System || !Held || !Held->IsPressed() || !Held->IsHovered() || !FSlateApplication::IsInitialized() ||
        !FSlateApplication::Get().IsActive() || System->IsInputSuspendedForPauseMenu() ||
        !FFBuildModeModel::IsPreviewActive(System->GetBuildModeState()))
    {
        StopPointerFreeRotation();
        return;
    }
    const double Now = FPlatformTime::Seconds();
    const float Delta = static_cast<float>(FMath::Clamp(Now - LastPointerRotationTime, 0.0, 0.05));
    LastPointerRotationTime = Now;
    System->RotatePlacementDegrees(PointerRotationDirection * System->GetFreePlacementRotationSpeedDegreesPerSecond() * Delta);
    RefreshAngleText();
}

// 회전 버튼을 놓거나 화면을 닫으면 반복 회전을 즉시 멈춘다.
void UFF_BuildToolsWidget::StopPointerFreeRotation()
{
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(FreeRotationTimer); }
    PointerRotationDirection = 0;
}

void UFF_BuildToolsWidget::HandleAngleCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
    }
    if (CommitMethod == ETextCommit::OnCleared)
    {
        if (Editable_Angle)
        {
            Editable_Angle->SetError(FText::GetEmpty());
        }
        RefreshAngleText();
        return;
    }
    ApplyAngleText(Text);
}

#undef LOCTEXT_NAMESPACE
