#include "Building/Presentation/FF_BuildToolsWidget.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
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
    if (!Border_Tools || GetVisibility() == ESlateVisibility::Hidden ||
        GetVisibility() == ESlateVisibility::Collapsed)
    {
        return false;
    }
    // Raw BuildMode input runs before Slate's button mouse-up. Geometry also covers that
    // first mouse-down, so clicking a tool never places a building underneath the toolbar.
    return Border_Tools->IsHovered() || (FSlateApplication::IsInitialized() &&
        Border_Tools->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos()));
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
    // The build panel owns the browsing/inspection strip. This surface is for the active
    // placement only and does not cover the teammate's tab/status bar.
    SetVisibility(bPreview ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!bPreview || (System && System->IsInputSuspendedForPauseMenu())) { StopPointerFreeRotation(); }
    const bool bSession = System && FFBuildModeModel::IsBuildSessionActive(State);
    const bool bGrid = System && System->IsPlacementGridVisible();
    const int32 PreferredSnap = System ? System->GetPreferredPositionSnapCm() : 100;
    const int32 ActiveSnap = System ? System->GetActivePositionSnapCm() : PreferredSnap;
    ViewData.bPreviewActive = bPreview;
    ViewData.bMoving = State == EFF_BuildModeState::Moving;
    ViewData.bGridVisible = bGrid;
    ViewData.PreferredSnapCm = PreferredSnap;
    ViewData.ActiveSnapCm = ActiveSnap;
    ViewData.YawDegrees = System ? System->GetPlacementYawDegrees() : 0.0f;
    ViewData.FacilityName = System && !System->GetSelectedFacilityDisplayName().IsEmpty()
        ? System->GetSelectedFacilityDisplayName() : LOCTEXT("NoFacility", "시설을 선택해 주세요");
    ViewData.CostText = ViewData.bMoving ? LOCTEXT("MoveCost", "이동 비용 없음") : FText::GetEmpty();
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
    }
    if (Text_FacilityName) { Text_FacilityName->SetText(ViewData.FacilityName); }
    if (Text_Cost) { Text_Cost->SetText(ViewData.CostText); }
    if (Text_ActiveSnap)
    {
        Text_ActiveSnap->SetText(FText::Format(ActiveSnap == PreferredSnap
            ? LOCTEXT("ActiveSnap", "현재 {0}cm") : LOCTEXT("TemporarySnap", "임시 {0}cm 적용 중"), FText::AsNumber(ActiveSnap)));
    }
    if (Text_RotationHint)
    {
        Text_RotationHint->SetText(FText::Format(LOCTEXT("RotationHint", "Q / E  {0}° 맞춤  ·  Shift + Q / E  누르는 동안 자유 회전"),
            FText::AsNumber(System ? System->GetPlacementRotationSnapDegrees() : 45.0f)));
    }
    if (Text_Mode)
    {
        Text_Mode->SetText(State == EFF_BuildModeState::Moving ? LOCTEXT("ModeMoving", "시설 이동") :
            bPreview ? LOCTEXT("ModePlacement", "시설 배치") : LOCTEXT("ModeOverview", "시설 확인"));
    }
    {
        FText Status = LOCTEXT("OverviewLegend", "시설 클릭: 정보 확인 · 격자: 빈 건축 공간");
        FSlateColor Color = NeutralStatusColor;
        if (bPreview && !System->HasPlacementTarget())
        {
            Status = LOCTEXT("NoTarget", "건축 구역의 바닥 위로 마우스를 옮겨 주세요");
        }
        else if (bPreview && System->IsCurrentPlacementValid())
        {
            Status = FText::Format(LOCTEXT("PlacementReady", "배치 가능 · 현재 {0}cm 맞춤"), FText::AsNumber(ActiveSnap));
            Color = ReadyStatusColor;
        }
        else if (bPreview)
        {
            Status = System->GetPlacementStatusText();
            Color = InvalidStatusColor;
        }
        ViewData.StatusText = Status;
        if (Text_Legend)
        {
            Text_Legend->SetText(Status);
            Text_Legend->SetColorAndOpacity(Color);
            Text_Legend->SetToolTipText(Status);
        }
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
