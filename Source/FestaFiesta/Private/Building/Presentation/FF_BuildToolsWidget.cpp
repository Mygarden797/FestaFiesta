#include "Building/Presentation/FF_BuildToolsWidget.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/DefaultValueHelper.h"
#include "UI/Data/FF_UIPalette.h"

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

void UFF_BuildToolsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    BuildMode = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->OnBuildToolsChanged.AddUniqueDynamic(this, &ThisClass::RefreshTools);
    }
#define FF_BIND_TOOL(Button, Handler) if (Button) { Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Handler); }
    FF_BIND_TOOL(Button_Grid, HandleGrid)
    FF_BIND_TOOL(Button_Confirm, HandleConfirm)
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
    const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
    if (Button)
    {
        FButtonStyle Style = Button->GetStyle();
        Style.Normal.TintColor = FSlateColor(bSelected ? Palette.Teal : Palette.PaperSunken);
        Style.Hovered.TintColor = FSlateColor(bSelected ? Palette.TealDeep : Palette.Hover);
        Style.Pressed.TintColor = FSlateColor(bSelected ? Palette.TealDeep : Palette.TealSoft);
        Button->SetStyle(Style);
    }
    if (Label)
    {
        Label->SetColorAndOpacity(FSlateColor(bSelected ? Palette.White : Palette.Ink));
    }
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
    // The shared catalog occupies the lower 320 px. It owns that browsing surface;
    // restore this compact toolbar when the player returns to the world or selects a facility.
    SetVisibility(State == EFF_BuildModeState::CatalogOpen
        ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    const bool bPreview = FFBuildModeModel::IsPreviewActive(State);
    const bool bSession = System && FFBuildModeModel::IsBuildSessionActive(State);
    const bool bGrid = System && System->IsPlacementGridVisible();
    const int32 PreferredSnap = System ? System->GetPreferredPositionSnapCm() : 100;
    const int32 ActiveSnap = System ? System->GetActivePositionSnapCm() : PreferredSnap;
    if (Text_Mode)
    {
        Text_Mode->SetText(State == EFF_BuildModeState::Moving ? LOCTEXT("ModeMoving", "시설 이동") :
            bPreview ? LOCTEXT("ModePlacement", "시설 배치") : LOCTEXT("ModeOverview", "시설 확인"));
    }
    if (Text_Legend)
    {
        const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
        FText Status = LOCTEXT("OverviewLegend", "시설 클릭: 정보 확인 · 격자: 빈 건축 공간");
        FLinearColor Color = Palette.InkMuted;
        if (bPreview && !System->HasPlacementTarget())
        {
            Status = LOCTEXT("NoTarget", "건축 구역의 바닥 위로 마우스를 옮겨 주세요");
        }
        else if (bPreview && System->IsCurrentPlacementValid())
        {
            Status = FText::Format(LOCTEXT("PlacementReady", "배치 가능 · 현재 {0}cm 맞춤"), FText::AsNumber(ActiveSnap));
            Color = Palette.TealDeep;
        }
        else if (bPreview)
        {
            Status = System->GetPlacementStatusText();
            Color = Palette.Danger;
        }
        Text_Legend->SetText(Status);
        Text_Legend->SetColorAndOpacity(FSlateColor(Color));
        Text_Legend->SetToolTipText(Status);
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
    Enable(Button_Confirm, bCanConfirm);
    if (Text_Confirm)
    {
        Text_Confirm->SetText(State == EFF_BuildModeState::Moving
            ? LOCTEXT("ConfirmMove", "이동 확정") : LOCTEXT("ConfirmInstall", "설치"));
    }
    Enable(Button_SnapMeter, bSession);
    Enable(Button_SnapPrecision, bSession);
    Enable(Button_SnapFree, bSession);
    Enable(Button_RotateLeft, bPreview);
    Enable(Button_RotateRight, bPreview);
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

void UFF_BuildToolsWidget::Rotate(const float Degrees)
{
    if (UFF_BuildModeSubsystem* System = BuildMode.Get())
    {
        System->NotifyBuildToolsInteraction();
        if (!ApplyPendingAngle())
        {
            return;
        }
        System->RotatePlacementDegrees(Degrees);
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
void UFF_BuildToolsWidget::HandleRotateLeft() { Rotate(-45.0f); }
void UFF_BuildToolsWidget::HandleRotateRight() { Rotate(45.0f); }
void UFF_BuildToolsWidget::HandleNudgeLeft() { Nudge(FIntPoint(-1, 0)); }
void UFF_BuildToolsWidget::HandleNudgeUp() { Nudge(FIntPoint(0, 1)); }
void UFF_BuildToolsWidget::HandleNudgeDown() { Nudge(FIntPoint(0, -1)); }
void UFF_BuildToolsWidget::HandleNudgeRight() { Nudge(FIntPoint(1, 0)); }

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
