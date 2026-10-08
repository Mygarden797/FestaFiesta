// FF_PauseHubScreen.cpp
// 역할: 일시정지 메뉴에서 재개·설정·도움말·나가기 항목을 연결한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Pause/FF_PauseHubScreen.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPauseHubScreen"

namespace
{
	FText FormatOptionKeyHint(const TArray<FKey>& Keys, const FText& Suffix)
	{
		TArray<FString> KeyLabels;
		for (const FKey& Key : Keys)
		{
			if (Key.IsValid())
			{
				KeyLabels.AddUnique(Key.GetDisplayName(false).ToString());
			}
		}
		return KeyLabels.IsEmpty()
			? Suffix
			: FText::FromString(FString::Printf(
				TEXT("%s  %s"), *FString::Join(KeyLabels, TEXT(" · ")), *Suffix.ToString()));
	}

	void BindButton(UButton* Button, UObject* Object, const FName FunctionName)
	{
		if (Button)
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Object, FunctionName);
			Button->OnClicked.AddUnique(Delegate);
		}
	}

	void UnbindButton(UButton* Button, UObject* Object)
	{
		if (Button)
		{
			Button->OnClicked.RemoveAll(Object);
		}
	}
}

void UFF_PauseHubScreen::NativeConstruct()
{
	Super::NativeConstruct();

	BindButton(Button_Resume, this, GET_FUNCTION_NAME_CHECKED(UFF_PauseHubScreen, HandleResumeClicked));
	BindButton(Button_Close, this, GET_FUNCTION_NAME_CHECKED(UFF_PauseHubScreen, HandleResumeClicked));
	BindButton(Button_Settings, this, GET_FUNCTION_NAME_CHECKED(UFF_PauseHubScreen, HandleSettingsClicked));
	BindButton(Button_Keyboard, this, GET_FUNCTION_NAME_CHECKED(UFF_PauseHubScreen, HandleKeyboardClicked));
	BindButton(Button_Account, this, GET_FUNCTION_NAME_CHECKED(UFF_PauseHubScreen, HandleAccountClicked));
	BindButton(Button_ReturnToAtlas, this, GET_FUNCTION_NAME_CHECKED(UFF_PauseHubScreen, HandleReturnToAtlasClicked));
	BindButton(Button_Exit, this, GET_FUNCTION_NAME_CHECKED(UFF_PauseHubScreen, HandleExitClicked));

	SetContextCopy(EFF_PauseHubAction::Resume);
	RefreshFestivalContext();
	RefreshOptionKeyHints();
}

void UFF_PauseHubScreen::NativeDestruct()
{
	UnbindButton(Button_Resume, this);
	UnbindButton(Button_Close, this);
	UnbindButton(Button_Settings, this);
	UnbindButton(Button_Keyboard, this);
	UnbindButton(Button_Account, this);
	UnbindButton(Button_ReturnToAtlas, this);
	UnbindButton(Button_Exit, this);
	OnReleased.Broadcast();
	Super::NativeDestruct();
}

void UFF_PauseHubScreen::NativeOnActivated()
{
	RefreshFestivalContext();
	RefreshOptionKeyHints();
	SetContextCopy(EFF_PauseHubAction::Resume);
	Super::NativeOnActivated();
}

UWidget* UFF_PauseHubScreen::NativeGetDesiredFocusTarget() const
{
	return Button_Resume ? Button_Resume : Super::NativeGetDesiredFocusTarget();
}

FReply UFF_PauseHubScreen::NativeOnKeyDown(
	const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const UFF_FestivalSettingsDeveloperSettings* Settings =
		UFF_FestivalSettingsDeveloperSettings::Get();
	if (!InKeyEvent.IsRepeat() && Settings && Settings->OpenCloseKeys.Contains(InKeyEvent.GetKey()))
	{
		RequestAction(EFF_PauseHubAction::Resume);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UFF_PauseHubScreen::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFocusPresentation();
	RefreshOptionKeyHints();
}

void UFF_PauseHubScreen::RefreshOptionKeyHints()
{
	const UFF_FestivalSettingsDeveloperSettings* Settings =
		UFF_FestivalSettingsDeveloperSettings::Get();
	if (!Settings || Settings->OpenCloseKeys.IsEmpty()
		|| PresentedOpenCloseKeys == Settings->OpenCloseKeys)
	{
		return;
	}

	PresentedOpenCloseKeys = Settings->OpenCloseKeys;
	const FText Header = FormatOptionKeyHint(
		PresentedOpenCloseKeys, LOCTEXT("ResumeHintSuffix", "계속 플레이"));
	const FText Footer = FormatOptionKeyHint(
		PresentedOpenCloseKeys, LOCTEXT("BackHintSuffix", "계속 플레이"));
	const FText Guide = FText::Format(
		LOCTEXT("DynamicGuideFormat", "↑ ↓  메뉴 이동    Enter  선택\n{0}"), Footer);
	if (Text_HeaderHint)
	{
		Text_HeaderHint->SetText(Header);
		Text_HeaderHint->SetToolTipText(Header);
	}
	if (Text_KeyHint)
	{
		Text_KeyHint->SetText(Footer);
		Text_KeyHint->SetToolTipText(Footer);
	}
	if (Text_GuideBody)
	{
		Text_GuideBody->SetText(Guide);
		Text_GuideBody->SetToolTipText(Guide);
	}
}

bool UFF_PauseHubScreen::RequestAction(const EFF_PauseHubAction Action)
{
	if (LastActionRequestFrame == GFrameCounter)
	{
		return false;
	}
	LastActionRequestFrame = GFrameCounter;
	OnActionRequested.Broadcast(Action);
	return true;
}

void UFF_PauseHubScreen::RefreshFestivalContext()
{
	UGameInstance* GameInstance = GetGameInstance();
	const UFF_UIFlowSubsystem* Flow = GameInstance
		? GameInstance->GetSubsystem<UFF_UIFlowSubsystem>() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	const UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	const UFF_CrowdSimulationSubsystem* Crowd = GetWorld()
		? GetWorld()->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;

	if (Text_FestivalName)
	{
		const FText FestivalName = Flow ? Flow->GetActiveFestivalDisplayName() : FText::GetEmpty();
		const FText Value = FestivalName.IsEmpty()
			? LOCTEXT("FestivalFallback", "진행 중인 축제") : FestivalName;
		Text_FestivalName->SetText(Value);
		Text_FestivalName->SetToolTipText(Value);
	}
	if (Text_DayStage)
	{
		const FText Value = Stage
			? FText::Format(LOCTEXT("DayStageFormat", "DAY {0}  ·  {1}"),
				FText::AsNumber(Stage->CurrentDay), Stage->GetCurrentStageDisplayName())
			: LOCTEXT("DayStageFallback", "축제 운영 정보 준비 중");
		Text_DayStage->SetText(Value);
		Text_DayStage->SetToolTipText(Value);
	}
	if (Text_Clock)
	{
		const FString Clock = Stage ? Stage->GetCurrentLiveClockString() : FString();
		const FText Value = !Clock.IsEmpty()
			? FText::Format(LOCTEXT("ClockFormat", "현장 시각  {0}"), FText::FromString(Clock))
			: LOCTEXT("ClockFallback", "현재 단계가 일시정지되었습니다");
		Text_Clock->SetText(Value);
		Text_Clock->SetToolTipText(Value);
	}
	if (Text_Gold)
	{
		const FText Value = Progression
			? FText::Format(LOCTEXT("GoldFormat", "{0} Gold"), FText::AsNumber(Progression->GetGold()))
			: LOCTEXT("GoldFallback", "—");
		Text_Gold->SetText(Value);
		Text_Gold->SetToolTipText(Value);
	}
	if (Text_Visitors)
	{
		const FText Value = Crowd
			? FText::Format(LOCTEXT("VisitorsFormat", "{0}명"), FText::AsNumber(Crowd->GetActiveAgentCount()))
			: LOCTEXT("VisitorsFallback", "—");
		Text_Visitors->SetText(Value);
		Text_Visitors->SetToolTipText(Value);
	}
	if (Text_Congestion)
	{
		const int32 CongestedCells = Crowd ? Crowd->GetCongestedCellCount() : INDEX_NONE;
		const FText Value = CongestedCells < 0
			? LOCTEXT("CongestionFallback", "정보 준비 중")
			: (CongestedCells == 0
				? LOCTEXT("CongestionClear", "혼잡 없음")
				: FText::Format(LOCTEXT("CongestionFormat", "혼잡 구역 {0}"), FText::AsNumber(CongestedCells)));
		Text_Congestion->SetText(Value);
		Text_Congestion->SetToolTipText(Value);
	}
}

void UFF_PauseHubScreen::RefreshFocusPresentation()
{
	const struct FButtonAction
	{
		UButton* Button;
		UImage* FocusRing;
		EFF_PauseHubAction Action;
	} Buttons[] = {
		{ Button_Resume, Image_FocusResume, EFF_PauseHubAction::Resume },
		{ Button_Settings, Image_FocusSettings, EFF_PauseHubAction::SystemSettings },
		{ Button_Keyboard, Image_FocusKeyboard, EFF_PauseHubAction::KeyboardGuide },
		{ Button_Account, Image_FocusAccount, EFF_PauseHubAction::Account },
		{ Button_ReturnToAtlas, Image_FocusReturnToAtlas, EFF_PauseHubAction::ReturnToAtlas },
		{ Button_Exit, Image_FocusExit, EFF_PauseHubAction::ExitGame }
	};
	TOptional<EFF_PauseHubAction> HoveredAction;
	TOptional<EFF_PauseHubAction> FocusedAction;

	for (const FButtonAction& Entry : Buttons)
	{
		if (!Entry.Button)
		{
			continue;
		}
		const bool bFocused = Entry.Button->HasAnyUserFocus();
		const bool bHovered = Entry.Button->IsHovered();
		if (Entry.FocusRing)
		{
			Entry.FocusRing->SetVisibility(
				bFocused ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (bHovered && !HoveredAction.IsSet())
		{
			HoveredAction = Entry.Action;
		}
		if (bFocused && !FocusedAction.IsSet())
		{
			FocusedAction = Entry.Action;
		}
	}

	const TOptional<EFF_PauseHubAction>& NextAction = HoveredAction.IsSet()
		? HoveredAction : FocusedAction;
	if (NextAction.IsSet() && PresentedAction != NextAction.GetValue())
	{
		SetContextCopy(NextAction.GetValue());
	}
}

void UFF_PauseHubScreen::SetContextCopy(const EFF_PauseHubAction Action)
{
	PresentedAction = Action;
	FText Eyebrow;
	FText Title;
	FText Description;
	switch (Action)
	{
	case EFF_PauseHubAction::SystemSettings:
		Eyebrow = LOCTEXT("SettingsEyebrow", "환경 설정");
		Title = LOCTEXT("SettingsTitle", "시스템 설정");
		Description = LOCTEXT("SettingsDescription",
			"소리, 카메라 움직임과 화면 표시를 취향에 맞게 조절하세요.");
		break;
	case EFF_PauseHubAction::KeyboardGuide:
		Eyebrow = LOCTEXT("KeyboardEyebrow", "조작 안내");
		Title = LOCTEXT("KeyboardTitle", "조작 가이드");
		Description = LOCTEXT("KeyboardDescription",
			"카메라 이동부터 시설 건설까지, 플레이에 필요한 조작 키를 확인하세요.");
		break;
	case EFF_PauseHubAction::Account:
		Eyebrow = LOCTEXT("AccountEyebrow", "내 정보");
		Title = LOCTEXT("AccountTitle", "계정 정보");
		Description = LOCTEXT("AccountDescription",
			"프로필과 저장 방식, 게임 버전을 확인하세요.");
		break;
	case EFF_PauseHubAction::ReturnToAtlas:
		Eyebrow = LOCTEXT("ReturnToAtlasEyebrow", "축제 선택");
		Title = LOCTEXT("ReturnToAtlasTitle", "축제 선택으로");
		Description = LOCTEXT("ReturnToAtlasDescription",
			"현재 플레이를 마치고 축제를 선택하는 화면으로 이동합니다.");
		break;
	case EFF_PauseHubAction::ExitGame:
		Eyebrow = LOCTEXT("ExitEyebrow", "종료 안내");
		Title = LOCTEXT("ExitTitle", "게임 종료");
		Description = LOCTEXT("ExitDescription",
			"게임을 종료하기 전에 한 번 더 확인합니다.");
		break;
	case EFF_PauseHubAction::Resume:
	default:
		Eyebrow = LOCTEXT("ResumeEyebrow", "축제 운영");
		Title = LOCTEXT("ResumeTitle", "계속하기");
		Description = LOCTEXT("ResumeDescription", "메뉴를 닫고 진행 중인 축제로 돌아갑니다.");
		break;
	}
	if (Text_SelectionEyebrow)
	{
		Text_SelectionEyebrow->SetText(Eyebrow);
		Text_SelectionEyebrow->SetToolTipText(Eyebrow);
	}
	if (Text_SelectionTitle)
	{
		Text_SelectionTitle->SetText(Title);
		Text_SelectionTitle->SetToolTipText(Title);
	}
	if (Text_SelectionDescription)
	{
		Text_SelectionDescription->SetText(Description);
		Text_SelectionDescription->SetToolTipText(Description);
	}
}

void UFF_PauseHubScreen::HandleResumeClicked()
{
	RequestAction(EFF_PauseHubAction::Resume);
}

void UFF_PauseHubScreen::HandleSettingsClicked()
{
	RequestAction(EFF_PauseHubAction::SystemSettings);
}

void UFF_PauseHubScreen::HandleKeyboardClicked()
{
	RequestAction(EFF_PauseHubAction::KeyboardGuide);
}

void UFF_PauseHubScreen::HandleAccountClicked()
{
	RequestAction(EFF_PauseHubAction::Account);
}

void UFF_PauseHubScreen::HandleExitClicked()
{
	RequestAction(EFF_PauseHubAction::ExitGame);
}

void UFF_PauseHubScreen::HandleReturnToAtlasClicked()
{
	RequestAction(EFF_PauseHubAction::ReturnToAtlas);
}

#undef LOCTEXT_NAMESPACE
