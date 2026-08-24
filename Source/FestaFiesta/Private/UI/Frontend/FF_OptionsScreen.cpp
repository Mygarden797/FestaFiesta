// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_OptionsScreen.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"
#include "UI/Frontend/FF_OptionRowToggle.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace
{
	// 옵션 분류 여섯 개. 구독과 문구 설정이 같은 목록을 봐야 한다.
	const EFF_OptionCategory AllOptionCategories[] = {
		EFF_OptionCategory::Language,
		EFF_OptionCategory::Accessibility,
		EFF_OptionCategory::Sound,
		EFF_OptionCategory::Graphics,
		EFF_OptionCategory::Controls,
		EFF_OptionCategory::Credits
	};
}

void UFF_OptionsScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// 🔴 구독은 `NativeConstruct` 다. CommonUI 스택이 위젯을 풀에서 재사용하므로
	// `NativeOnInitialized`(인스턴스당 한 번)에 구독하고 `NativeDestruct`(내려갈 때마다)에
	// 해제하면, 뒤로 갔다 다시 들어온 화면은 **버튼이 죽는다.**
	// 버튼이 자기를 넘겨주므로 핸들러 하나로 구분한다.
	for (const EFF_OptionCategory Category : AllOptionCategories)
	{
		if (UFF_CommonButtonBase* Button = GetCategoryButton(Category))
		{
			Button->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleCategoryClicked);
		}
	}

	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleBackClicked);
	}
}

void UFF_OptionsScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 문구는 인스턴스에 한 번만 넣으면 된다.
	for (const EFF_OptionCategory Category : AllOptionCategories)
	{
		if (UFF_CommonButtonBase* Button = GetCategoryButton(Category))
		{
			// enum 의 DisplayName 이 곧 화면 문구다 —
			// WBP 마다 손으로 적으면 분류 이름이 갈라진다.
			Button->SetButtonLabel(UEnum::GetDisplayValueAsText(Category));
		}
		else
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("UFF_OptionsScreen: `%s` 분류 버튼 바인딩이 없다."),
				*UEnum::GetDisplayValueAsText(Category).ToString());
		}
	}

	if (Button_Back)
	{
		Button_Back->SetButtonLabel(LOCTEXT("OptionsBack", "뒤로"));
	}
}

void UFF_OptionsScreen::NativeOnActivated()
{
	// 열 때마다 기본 분류로 돌아간다. 이전 선택을 기억하면 "왜 여기가 열렸지" 가 된다.
	SelectCategory(DefaultCategory);

	// 포커스는 베이스가 NativeGetDesiredFocusTarget 으로 잡는다.
	// 그 함수가 SelectedCategory 를 보므로 반드시 SelectCategory 뒤에 Super 를 부른다.
	Super::NativeOnActivated();
}

void UFF_OptionsScreen::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건.
	for (const EFF_OptionCategory Category : AllOptionCategories)
	{
		if (UFF_CommonButtonBase* Button = GetCategoryButton(Category))
		{
			Button->OnFFButtonClicked.RemoveDynamic(this, &UFF_OptionsScreen::HandleCategoryClicked);
		}
	}

	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.RemoveDynamic(this, &UFF_OptionsScreen::HandleBackClicked);
	}

	Super::NativeDestruct();
}

UFF_CommonButtonBase* UFF_OptionsScreen::GetCategoryButton(EFF_OptionCategory Category) const
{
	switch (Category)
	{
	case EFF_OptionCategory::Language:		return Button_Language;
	case EFF_OptionCategory::Accessibility:	return Button_Accessibility;
	case EFF_OptionCategory::Sound:			return Button_Sound;
	case EFF_OptionCategory::Graphics:		return Button_Graphics;
	case EFF_OptionCategory::Controls:		return Button_Controls;
	case EFF_OptionCategory::Credits:		return Button_Credits;
	}
	return nullptr;
}

UWidget* UFF_OptionsScreen::NativeGetDesiredFocusTarget() const
{
	if (UFF_CommonButtonBase* Selected = GetCategoryButton(SelectedCategory))
	{
		return Selected;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UFF_OptionsScreen::SelectCategory(EFF_OptionCategory Category)
{
	SelectedCategory = Category;

	if (Text_CategoryTitle)
	{
		Text_CategoryTitle->SetText(UEnum::GetDisplayValueAsText(Category));
	}

	RebuildRows();
}

void UFF_OptionsScreen::RebuildRows()
{
	if (VerticalBox_Rows)
	{
		VerticalBox_Rows->ClearChildren();
	}

	// 지금 실제로 동작하는 분류는 접근성 하나다.
	const bool bImplemented = (SelectedCategory == EFF_OptionCategory::Accessibility);

	if (Text_NotImplemented)
	{
		// 만져도 아무 일이 없는 조작 UI 는 고장으로 보인다. 이유를 화면에 적는다.
		Text_NotImplemented->SetText(
			LOCTEXT("OptionsNotImplemented", "반영 사항 없음 — 이 분류는 아직 준비 중입니다"));
		Text_NotImplemented->SetVisibility(
			bImplemented ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (!bImplemented || !VerticalBox_Rows)
	{
		return;
	}

	if (!ToggleRowClass)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_OptionsScreen: ToggleRowClass 가 없어 접근성 설정을 그리지 못했다. "
				 "WBP 에서 WBP_FF_OptionRowToggle 을 지정한다."));
		return;
	}

	const UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this);
	if (!Accessibility)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 접근성 서브시스템을 찾지 못했다. 설정을 그리지 않았다."));
		return;
	}

	UFF_OptionRowToggle* Row = CreateWidget<UFF_OptionRowToggle>(this, ToggleRowClass);
	if (!Row)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 설정 행 생성 실패 (%s)."), *ToggleRowClass->GetName());
		return;
	}

	// 값은 서브시스템이 소유한다. 이 화면은 현재값을 읽어 표시할 뿐이다.
	Row->Configure(
		LOCTEXT("OptionReduceMotion", "움직임 감소"),
		LOCTEXT("OptionReduceMotionDesc", "화면 전환과 패널 애니메이션을 끕니다"),
		Accessibility->IsReduceMotionEnabled());

	Row->OnOptionToggled.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleReduceMotionToggled);
	VerticalBox_Rows->AddChild(Row);
}

void UFF_OptionsScreen::HandleReduceMotionToggled(bool bEnabled)
{
	// 값을 직접 들고 있지 않는다. 소유자에게 요청한다.
	if (UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this))
	{
		Accessibility->SetReduceMotionEnabled(bEnabled);
	}
}

void UFF_OptionsScreen::HandleCategoryClicked(UFF_CommonButtonBase* Button)
{
	for (const EFF_OptionCategory Category : AllOptionCategories)
	{
		if (GetCategoryButton(Category) == Button)
		{
			SelectCategory(Category);
			return;
		}
	}
}

void UFF_OptionsScreen::HandleBackClicked(UFF_CommonButtonBase* Button)
{
	OnBackRequested.Broadcast();
}

#undef LOCTEXT_NAMESPACE
