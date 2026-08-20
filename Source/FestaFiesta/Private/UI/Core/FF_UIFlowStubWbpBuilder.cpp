// FF_UIFlowStubWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 흐름 스텁 화면의 WBP 를 만드는 에디터 전용 도구다. 콘솔 명령은 `FF.FlowStubUMG`.
//
// **없는 WBP만 만든다.** 규칙과 함정은 FF_UIWbpBuilderCommon.h 상단에 있다.
// 이 화면은 U2~U7 이 실제 화면으로 갈아끼울 자리이므로 마감할 필요가 없다.

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIFlowStubScreen.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace FFUIFlowStubWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const StubLogPrefix = TEXT("FF_FLOWSTUB_UMG");

	const FString FlowFolder(TEXT("/Game/FestaFiesta/UI/Flow"));
	const FString StubPackage(FlowFolder / TEXT("WBP_FF_FlowStub"));

	// 대략 크기. 에디터에서 조절할 출발점이다.
	constexpr float CardWidth = 520.f;
	constexpr float CardHeight = 240.f;
	constexpr float ButtonHeight = 52.f;

	bool BuildFlowStub(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없어 WBP_FF_FlowStub 을 만들지 않았다."), StubLogPrefix);
			return false;
		}

		// 루트를 Overlay 로 둔다. 흐름 서브시스템이 HUD 레이어 슬롯을 Fill 로 주므로
		// 이 Overlay 가 화면 전체를 받고, 그 안에서 카드를 가운데로 놓는다.
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		USizeBox* CardBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Card"));
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Card"));
		UVerticalBox* Column = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Root"));
		USizeBox* ButtonBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Button"));
		UTextBlock* AdvanceLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_AdvanceLabel"));

		// bIsVariable = true 인 것만 BindWidget 대상이다.
		UTextBlock* StageName = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_StageName"), true);
		UTextBlock* Hint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Hint"), true);
		UTextBlock* Clock = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Clock"), true);
		UButton* Advance = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Advance"), true);

		if (!Root || !CardBox || !Card || !Column || !ButtonBox || !AdvanceLabel
			|| !StageName || !Hint || !Clock || !Advance)
		{
			return false;
		}

		Tree.RootWidget = Root;

		CardBox->SetWidthOverride(CardWidth);
		CardBox->SetHeightOverride(CardHeight);
		if (UOverlaySlot* CardSlot = Root->AddChildToOverlay(CardBox))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Center);
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}
		CardBox->AddChild(Card);

		Card->SetBrushColor(Palette->InGame.Paper);
		Card->SetPadding(FMargin(28.f, 24.f, 28.f, 24.f));
		Card->AddChild(Column);

		StyleText(*StageName, 32, Palette->InGame.Ink, TEXT("SemiBold"));
		StageName->SetText(LOCTEXT("FlowStubStagePlaceholder", "단계 이름"));
		if (UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(StageName))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		StyleText(*Hint, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		Hint->SetText(LOCTEXT("FlowStubHintPlaceholder", "Day 1 · 임시 화면입니다"));
		if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(Hint))
		{
			HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			HintSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		// 이 숫자가 줄어드는지가 "Live 중 시계가 계속 가는가" 확인 지점이다.
		StyleText(*Clock, 20, Palette->InGame.Amber, TEXT("SemiBold"));
		Clock->SetText(LOCTEXT("FlowStubClockPlaceholder", "남은 시간 --초"));
		if (UVerticalBoxSlot* ClockSlot = Column->AddChildToVerticalBox(Clock))
		{
			ClockSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ClockSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 12.f));
		}

		StyleText(*AdvanceLabel, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		AdvanceLabel->SetText(LOCTEXT("FlowStubAdvance", "다음 단계"));
		Advance->AddChild(AdvanceLabel);

		ButtonBox->SetHeightOverride(ButtonHeight);
		ButtonBox->AddChild(Advance);
		if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(ButtonBox))
		{
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		return true;
	}

	void RunFlowStubUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BEGIN: 없는 WBP만 생성한다."), StubLogPrefix);

		EnsureWidgetBlueprint(
			StubLogPrefix,
			StubPackage,
			FlowFolder,
			*UFF_UIFlowStubScreen::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildFlowStub(Blueprint, Tree); });

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 끝났다. Project Settings > Game > FestaFiesta UI Flow 의 "
				 "Stage Screens 에 이 WBP 를 넣는다."), StubLogPrefix);
	}

	FAutoConsoleCommand FlowStubUmgCommand(
		TEXT("FF.FlowStubUMG"),
		TEXT("흐름 스텁 화면(WBP_FF_FlowStub)을 만든다. 이미 있는 에셋은 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunFlowStubUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
