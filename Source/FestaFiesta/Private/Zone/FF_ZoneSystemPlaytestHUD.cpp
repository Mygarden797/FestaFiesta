#include "Zone/FF_ZoneSystemPlaytestHUD.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "Zone/FF_ZoneSystemPlaytestSubsystem.h"

// WBP는 레이아웃과 스타일을 소유하고, 이 네이티브 클래스는 상태 동기화와 버튼 동작만 담당한다.
namespace
{
    constexpr float HudRefreshIntervalSeconds = 0.1f;
}

// BindWidgetOptional로 실제 존재하는 버튼만 연결하므로 디자이너가 레이아웃 일부를 생략해도 안전하다.
void UFF_ZoneSystemPlaytestHUD::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (UTextBlock* OverviewLabel = Cast<UTextBlock>(GetWidgetFromName(TEXT("BTN_ReturnToGame_Label"))))
    {
        OverviewLabel->SetText(FText::FromString(TEXT("OVERVIEW (TAB)")));
    }

#define FF_BIND_PLAYTEST_BUTTON(ButtonName, HandlerName) \
    if (ButtonName) \
    { \
        ConfigureButton(ButtonName); \
        ButtonName->OnClicked.AddDynamic(this, &ThisClass::HandlerName); \
    }

    FF_BIND_PLAYTEST_BUTTON(BTN_PresetPrevious, HandlePresetPrevious);
    FF_BIND_PLAYTEST_BUTTON(BTN_PresetNext, HandlePresetNext);
    FF_BIND_PLAYTEST_BUTTON(BTN_Install, HandleInstall);
    FF_BIND_PLAYTEST_BUTTON(BTN_Move, HandleMove);
    FF_BIND_PLAYTEST_BUTTON(BTN_Remove, HandleRemove);
    FF_BIND_PLAYTEST_BUTTON(BTN_Open, HandleOpen);
    FF_BIND_PLAYTEST_BUTTON(BTN_Close, HandleClose);
    FF_BIND_PLAYTEST_BUTTON(BTN_Emergency, HandleEmergency);
    FF_BIND_PLAYTEST_BUTTON(BTN_QueueJoin, HandleQueueJoin);
    FF_BIND_PLAYTEST_BUTTON(BTN_QueueAdvance, HandleQueueAdvance);
    FF_BIND_PLAYTEST_BUTTON(BTN_QueueBlock, HandleQueueBlock);
    FF_BIND_PLAYTEST_BUTTON(BTN_SnapshotSave, HandleSnapshotSave);
    FF_BIND_PLAYTEST_BUTTON(BTN_SnapshotRestore, HandleSnapshotRestore);
    FF_BIND_PLAYTEST_BUTTON(BTN_Reset, HandleReset);
    FF_BIND_PLAYTEST_BUTTON(BTN_Debug, HandleDebug);
    FF_BIND_PLAYTEST_BUTTON(BTN_Help, HandleHelp);
    FF_BIND_PLAYTEST_BUTTON(BTN_ReturnToGame, HandleReturnToGame);

#undef FF_BIND_PLAYTEST_BUTTON

    RefreshDisplay();
}

void UFF_ZoneSystemPlaytestHUD::NativeDestruct()
{
    // PlayerController는 심리스 이동 뒤에도 남을 수 있지만 이 맵 전용 HUD는 사라질 수 있다.
    // 다음 맵에 커서와 GameAndUI 입력 상태가 새지 않도록 반드시 GameOnly로 되돌린다.
    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        PlayerController->bShowMouseCursor = false;
        PlayerController->SetInputMode(FInputModeGameOnly());
    }
    Super::NativeDestruct();
}

void UFF_ZoneSystemPlaytestHUD::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    RefreshElapsed += InDeltaTime;
    if (RefreshElapsed >= HudRefreshIntervalSeconds)
    {
        RefreshElapsed = 0.0f;
        RefreshDisplay();
    }
}

// 0.1초마다 subsystem의 표시 상태를 WBP에 반영하고 Blueprint 확장 이벤트에도 전달한다.
void UFF_ZoneSystemPlaytestHUD::RefreshDisplay()
{
    UWorld* World = GetWorld();
    UFF_ZoneSystemPlaytestSubsystem* Playtest = World
        ? World->GetSubsystem<UFF_ZoneSystemPlaytestSubsystem>()
        : nullptr;
    if (!Playtest)
    {
        return;
    }

    HudState = Playtest->GetHudState();

    if (TXT_Zone) TXT_Zone->SetText(HudState.ZoneText);
    if (TXT_Preset) TXT_Preset->SetText(HudState.PresetText);
    if (TXT_Placement) TXT_Placement->SetText(HudState.PlacementText);
    if (TXT_Validation)
    {
        TXT_Validation->SetText(HudState.ValidationText);
        TXT_Validation->SetColorAndOpacity(HudState.ValidationColor);
    }
    if (TXT_Status)
    {
        TXT_Status->SetText(HudState.StatusText);
        TXT_Status->SetColorAndOpacity(HudState.StatusColor);
    }
    if (TXT_Help)
    {
        TXT_Help->SetText(HudState.HelpText);
        TXT_Help->SetVisibility(HudState.bShowHelp ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (BTN_Install) BTN_Install->SetIsEnabled(HudState.bCanCommitPlacement);
    if (BTN_Move) BTN_Move->SetIsEnabled(HudState.bHasSelectedFacility);
    if (BTN_Remove) BTN_Remove->SetIsEnabled(HudState.bHasSelectedFacility);
    if (BTN_Open) BTN_Open->SetIsEnabled(HudState.bHasSelectedFacility);
    if (BTN_Close) BTN_Close->SetIsEnabled(HudState.bHasSelectedFacility);
    if (BTN_Emergency) BTN_Emergency->SetIsEnabled(HudState.bHasSelectedFacility);
    if (BTN_QueueJoin) BTN_QueueJoin->SetIsEnabled(HudState.bHasSelectedFacility);
    if (BTN_QueueAdvance) BTN_QueueAdvance->SetIsEnabled(HudState.bHasSelectedFacility);
    if (BTN_QueueBlock) BTN_QueueBlock->SetIsEnabled(HudState.bHasSelectedFacility);
    if (BTN_SnapshotRestore) BTN_SnapshotRestore->SetIsEnabled(HudState.bHasSavedSnapshot);

    ReceiveHudStateUpdated(HudState);
}

void UFF_ZoneSystemPlaytestHUD::ConfigureButton(UButton* Button) const
{
    if (!Button)
    {
        return;
    }

    // UE 5.7은 이 작성 속성에 getter만 노출하므로 Slate 생성 전에 리플렉션으로 값을 설정한다.
    // 버튼 클릭 뒤 Tab이 포커스 이동으로 소비되지 않고 관찰/HUD 모드 단축키로 남게 한다.
    if (FBoolProperty* FocusableProperty = FindFProperty<FBoolProperty>(
        UButton::StaticClass(), TEXT("IsFocusable")))
    {
        FocusableProperty->SetPropertyValue_InContainer(Button, false);
    }
}

// 반복되는 버튼 핸들러는 subsystem 호출과 즉시 화면 갱신이라는 공통 형식으로 묶는다.
#define FF_PLAYTEST_ACTION(Name, Call) \
    void UFF_ZoneSystemPlaytestHUD::Name() \
    { \
        if (UWorld* World = GetWorld()) \
        { \
            if (UFF_ZoneSystemPlaytestSubsystem* Playtest = World->GetSubsystem<UFF_ZoneSystemPlaytestSubsystem>()) \
            { \
                Playtest->Call; \
                RefreshDisplay(); \
            } \
        } \
    }

FF_PLAYTEST_ACTION(HandlePresetPrevious, SelectPreviousPreset())
FF_PLAYTEST_ACTION(HandlePresetNext, SelectNextPreset())
FF_PLAYTEST_ACTION(HandleInstall, InstallAtPreview())
FF_PLAYTEST_ACTION(HandleMove, MoveSelectedFacility())
FF_PLAYTEST_ACTION(HandleRemove, RemoveSelectedFacility())
FF_PLAYTEST_ACTION(HandleOpen, OpenSelectedFacility())
FF_PLAYTEST_ACTION(HandleClose, CloseSelectedFacility())
FF_PLAYTEST_ACTION(HandleEmergency, EmergencyCloseSelectedFacility())
FF_PLAYTEST_ACTION(HandleQueueJoin, JoinSelectedQueue())
FF_PLAYTEST_ACTION(HandleQueueAdvance, AdvanceSelectedQueue())
FF_PLAYTEST_ACTION(HandleQueueBlock, ToggleSelectedQueueBlocked())
FF_PLAYTEST_ACTION(HandleSnapshotSave, SavePlaytestSnapshot())
FF_PLAYTEST_ACTION(HandleSnapshotRestore, LoadPlaytestSnapshot())
FF_PLAYTEST_ACTION(HandleReset, ResetPlaytestScenario())
FF_PLAYTEST_ACTION(HandleDebug, ToggleWorldDebug())
FF_PLAYTEST_ACTION(HandleHelp, ToggleHelp())
FF_PLAYTEST_ACTION(HandleReturnToGame, ToggleUiInteractionMode())

#undef FF_PLAYTEST_ACTION



