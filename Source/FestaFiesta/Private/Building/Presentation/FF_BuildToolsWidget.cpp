#include "Building/Presentation/FF_BuildToolsWidget.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/Presentation/FF_BuildModeGuide.h"
#include "Building/Presentation/FF_BuildFeedback.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/DefaultValueHelper.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FFBuildTools"

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

void UFF_BuildToolsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    BuildMode = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    Economy = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFF_BuildCatalogEconomyReadModel>() : nullptr;
    if (UFF_BuildCatalogEconomyReadModel* Model = Economy.Get())
    {
        Model->OnGoldChanged.AddUniqueDynamic(this, &ThisClass::HandleGoldChanged);
    }
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->OnBuildToolsChanged.AddUniqueDynamic(this, &ThisClass::RefreshTools);
    }
#define FF_BIND_TOOL(Button, Handler) if (Button) { Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Handler); }
    FF_BIND_TOOL(Button_Grid, HandleGrid)
    FF_BIND_TOOL(Button_Confirm, HandleConfirm)
    FF_BIND_TOOL(Button_Catalog, ReturnToCatalog)
    FF_BIND_TOOL(Button_Details, ToggleDetails)
    FF_BIND_TOOL(Button_Facts, ToggleFacts)
    FF_BIND_TOOL(Button_Help, ToggleHelp)
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

void UFF_BuildToolsWidget::NativeDestruct()
{
    StopPointerFreeRotation();
    if (UFF_BuildCatalogEconomyReadModel* Model = Economy.Get()) { Model->OnGoldChanged.RemoveAll(this); }
    Economy.Reset();
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
    for (const UBorder* Panel : { Border_Tools.Get(), Border_PlacementInfo.Get(), Border_PlacementHelp.Get() })
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
    return IsRadialMenuOpen() || (Panel_Details && Panel_Details->IsVisible());
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
    if (!ViewData.bPreviewActive) return;
    if (UFF_BuildModeSubsystem* System = BuildMode.Get()) System->NotifyBuildToolsInteraction();
    SetHelpExpanded(Border_PlacementHelp && Border_PlacementHelp->GetVisibility() == ESlateVisibility::Collapsed);
    if (auto* System = BuildMode.Get()) System->RefreshBuildToolsOcclusion();
}

void UFF_BuildToolsWidget::SetHelpExpanded(const bool bExpanded)
{
    if (Border_PlacementHelp)
        Border_PlacementHelp->SetVisibility(bExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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

void UFF_BuildToolsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
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
        for (const UBorder* Panel : { Border_Tools.Get(), Border_PlacementInfo.Get(), Border_PlacementHelp.Get() })
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

void UFF_BuildToolsWidget::RefreshTools()
{
    const UFF_BuildModeSubsystem* System = BuildMode.Get();
    const EFF_BuildModeState State = System ? System->GetBuildModeState() : EFF_BuildModeState::Closed;
    const bool bPreview = FFBuildModeModel::IsPreviewActive(State);
    // Also retire the button for older, not-yet-migrated widget assets.
    if (Button_Radial) Button_Radial->SetVisibility(ESlateVisibility::Collapsed);
    if (Panel_Radial) Panel_Radial->SetVisibility(ESlateVisibility::Collapsed);
    const bool bStartingPreview = bPreview && !ViewData.bPreviewActive;
    if (bStartingPreview || !bPreview || (System && System->IsCurrentPlacementValid())) PlacementProblemIndex = 0;
    // The build panel owns the browsing/inspection strip. This surface is for the active
    // placement only and does not cover the teammate's tab/status bar.
    SetVisibility(bPreview ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!bPreview || (System && System->IsInputSuspendedForPauseMenu())) { StopPointerFreeRotation(); }
    if (!bPreview)
    {
        CloseRadialMenu();
        if (Panel_Details) Panel_Details->SetVisibility(ESlateVisibility::Collapsed);
        if (Panel_PlacementFacts) Panel_PlacementFacts->SetVisibility(ESlateVisibility::Collapsed);
        SetHelpExpanded(false);
    }
    // Open once on entry. Cursor, rotation and economy refreshes preserve a user's dismissal.
    else if (bStartingPreview) SetHelpExpanded(true);
    const bool bSession = System && FFBuildModeModel::IsBuildSessionActive(State);
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
        ViewData.QueueText = FText::Format(LOCTEXT("QueueInfo", "대기 정원  {0}명\n동시 이용  {1}명  ·  1인 서비스  {2}초"),
            FText::AsNumber(Queue.MaxWaitingAgents), FText::AsNumber(Queue.ServiceCapacity),
            FText::AsNumber(Queue.ServiceDurationSeconds));
    }
    UFF_BuildCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
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
    if (Text_ControlHelp) Text_ControlHelp->SetText(ViewData.bDragPlacementEnabled
        ? LOCTEXT("DragControls", "좌클릭 드래그  연속 설치\nQ / E  45° 회전 · 드래그 종료\nShift + Q / E  자유 회전\nCtrl / Alt  첫 위치 미세 조정\nG  격자 표시\nB / 우클릭  시설 목록\nW A S D  카메라 이동\nEsc  일시정지")
        : System && System->IsDirectPlacementAdjustment()
        ? LOCTEXT("DirectControls", "화살표 드래그  가로·세로 이동\n원 드래그  회전 · Shift  자유 회전\nEnter / 배치 확정  설치\nT  마우스로 위치 다시 선택\nCtrl  25 cm · Alt  1 cm\nG  격자 표시\nB / 우클릭  시설 목록\nW A S D  카메라 이동 · Esc  일시정지")
        : LOCTEXT("PlacementControls", "좌클릭 / Enter  설치 확정\nT  직접 조정 · 화살표 이동/원 회전\nQ / E  45°씩 회전\nShift + Q / E  자유 회전\nCtrl  25 cm · Alt  1 cm\nG  격자 표시\nB / 우클릭  시설 목록\nW A S D  카메라 이동 · Esc  일시정지"));
    if (Text_ActiveSnap)
    {
        Text_ActiveSnap->SetText(FText::Format(ActiveSnap == PreferredSnap
            ? LOCTEXT("ActiveSnap", "현재 {0}cm") : LOCTEXT("TemporarySnap", "임시 {0}cm 적용 중"), FText::AsNumber(ActiveSnap)));
    }
    if (Text_RotationHint)
    {
        Text_RotationHint->SetText(FText::Format(ViewData.bDragPlacementEnabled
            ? LOCTEXT("DragRotationHint", "Q / E  {0}° 맞춤 · Shift + Q / E  자유 회전 · 회전 시 드래그 종료")
            : LOCTEXT("RotationHint", "Q / E  {0}° 맞춤  ·  Shift + Q / E  누르는 동안 자유 회전"),
            FText::AsNumber(System ? System->GetPlacementRotationSnapDegrees() : 45.0f)));
    }
    // The existing WBP owns this named row; no Blueprint regeneration is needed.
    if (UTextBlock* PrecisionHint = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_PrecisionHint"))))
    {
        PrecisionHint->SetText(ViewData.bDragPlacementEnabled
            ? LOCTEXT("DragPrecisionHint", "Ctrl / Alt: 첫 위치 조정 · 이후 시설 크기 간격 · 좌클릭을 놓으면 정지")
            : LOCTEXT("PrecisionHint", "Ctrl 누르는 동안 25cm  ·  Alt 누르는 동안 1cm  ·  미세 조정 후 이 패널에서 배치를 확정하세요"));
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
                ? LOCTEXT("DragPlacementReady", "배치 가능 · 좌클릭을 누른 채 드래그해 연속 설치")
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
                ? System->IsDirectPlacementAdjustment() ? LOCTEXT("PingPinned", "직접 조정  ·  Enter로 확정")
                    : LOCTEXT("PingReady", "배치 가능  ·  클릭 / Enter") : Guidance.Title);
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
                Label->SetText(bDirect ? LOCTEXT("FollowCursor", "위치 다시 선택  T") : LOCTEXT("AdjustDirect", "직접 조정  T"));
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
        Text_Grid->SetText(bGrid ? LOCTEXT("GridOn", "격자 켜짐") : LOCTEXT("GridOff", "격자 꺼짐"));
    }
    const auto Enable = [](UWidget* Widget, const bool bEnabled)
    {
        if (Widget) { Widget->SetIsEnabled(bEnabled); }
    };
    Enable(Button_Grid, bSession);
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
    SetChoiceState(Button_Confirm, Text_Confirm, bCanConfirm);
    SetChoiceState(Button_SnapMeter, Text_SnapMeter, PreferredSnap == 100);
    SetChoiceState(Button_SnapPrecision, Text_SnapPrecision, PreferredSnap == 25);
    SetChoiceState(Button_SnapFree, Text_SnapFree, PreferredSnap == 1);
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
