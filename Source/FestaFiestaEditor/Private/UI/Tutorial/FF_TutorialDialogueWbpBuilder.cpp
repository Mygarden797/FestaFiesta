// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_TutorialDialogueWbpBuilder.cpp
// 역할: 튜토리얼 대사창 · 선택지 버튼 · 건너뛰기 확인 창을 게임 UI(LightTeal) 모양으로 만든다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR

/* 콘솔 명령
     FF.TutorialDialogueUMG          없는 WBP 만 만든다. 이미 있으면 손대지 않는다.
     FF.TutorialDialogueUMG.Rebuild  세 WBP 의 레이아웃을 처음부터 다시 만든다. 디자이너에서 바꾼 배치는 사라진다.

   ─── 왜 생성기로 만드는가 ───────────────────────────────────────────────────
   처음 대사창은 디자이너에서 손으로 짜서 기본 글꼴(Roboto)과 반투명 회색 면이었다. 게임 UI 는 LightTeal 팔레트
   (UFF_UIPalette)와 Pretendard 로 맞춰져 있어 대사창만 따로 놀았다(2026-10-06 요청 "지금 게임 UI 에 맞는 디자인").
   색과 모양을 팔레트 토큰에서 가져와 만들면 팔레트가 바뀌어도 다시 만들기만 하면 따라간다.
   만드는 방식은 플레이리스트 플레이어(FF_PlaylistPlayerWbpBuilder)와 같다 — 단색 둥근 면, 같은 버튼 모양.

   ─── 화면 (1920x1080 기준) ──────────────────────────────────────────────────
                                                         [건너뛰기 ›]
          ┌ 초상화 ┐
          │        │
     ┌────┴────────┴──────────────────────────────────────────────┐
     │ (비서)                                                       │  ← 화자 이름표(청록)
     │ 오셨네요. 기다리고 있었습니다.                               │  ← 대사
     │ [› 선택지 1]                                                 │  ← 선택지 줄에서만
     │                                                     다음 ›   │  ← 넘길 수 있을 때만
     └──────────────────────────────────────────────────────────────┘

   ─── 안 하는 것 ─────────────────────────────────────────────────────────────
   글자가 한 자씩 나오는 효과나 애니메이션은 넣지 않는다. 동작은 C++(UFF_DialogueBoxWidget)이 그대로 한다.
   위젯 이름은 C++ 의 BindWidgetOptional 과 1:1 이다. 이름을 바꾸면 그쪽도 같이 바꾼다.
*/

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Tutorial/FF_DialogueBoxWidget.h"
#include "Tutorial/FF_SkipConfirmWidget.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaTutorialDialogueWbpBuilder"

namespace FFTutorialDialogueWbpBuilder
{
	using namespace FFUIWbpBuilder;
	using FTokens = FFPauseOptionsLightTeal::FTokens;

	const TCHAR* const DialogueLogPrefix = TEXT("FF_TUTORIAL_DIALOGUE_UMG");
	const FString TutorialFolder(TEXT("/Game/FestaFiesta/Tutorial"));
	const FString DialogueBoxPackage(TutorialFolder / TEXT("WBP_DialogueBoxWidget"));
	const FString ChoiceButtonPackage(TutorialFolder / TEXT("WBP_DialogueChoiceButton"));
	const FString SkipConfirmPackage(TutorialFolder / TEXT("WBP_SkipConfirm"));

	// 1920x1080 기준. 화면 배율은 UIScaleRule 이 처리한다.
	struct FMetrics
	{
		static constexpr float CardWidth = 1320.f;
		static constexpr float CardMinHeight = 200.f;
		static constexpr float CardRadius = 14.f;

		/* 화면 아래에서 띄우는 높이. 준비 · 운영 HUD 의 아래 띠(메뉴 44 + 상태 52 + 여백, 약 110)보다 위다.

		   대사창은 HUD 아래에 그려진다(튜토리얼 설정 DialogueBoxZOrder=-100 — HUD 를 눌러야 하는 줄이 있다).
		   띠와 겹치면 대사의 아랫줄이 HUD 에 가려진다.
		*/
		static constexpr float CardBottom = 130.f;

		// 대사창 안에서만 클릭을 받는 버튼의 높이. 대사가 길어 카드가 커져도 덮이게 넉넉히 잡는다.
		static constexpr float BoxClickHeight = 260.f;

		// 초상화 원본은 832x1216 이다. 같은 비율로 줄인다. 아래쪽은 대사창에 가려진다.
		static constexpr float PortraitWidth = 560.f;
		static constexpr float PortraitHeight = 818.f;
		static constexpr float PortraitLeft = 170.f;

		static constexpr float SkipMargin = 32.f;
		// 안내 문장 첫 줄("…더 이상 나오지 않습니다.")이 한 줄에 들어가는 폭.
		static constexpr float ConfirmWidth = 620.f;
		static constexpr float ConfirmRadius = 12.f;
		static constexpr float ConfirmButtonHeight = 50.f;
	};

	enum class EButtonLook : uint8
	{
		Primary,    // 청록 면. 되돌릴 수 없는 결정(건너뛰기 확정).
		Secondary,  // 종이 면 + 선. 취소 · 선택지 · 건너뛰기 열기.
		Invisible   // 아무것도 그리지 않는다. 클릭 영역(다음 줄로).
	};

	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, const FVector4& Radii,
		const FLinearColor& Outline = FLinearColor::Transparent, const float OutlineWidth = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = Radii;
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, const float Radius,
		const FLinearColor& Outline = FLinearColor::Transparent, const float OutlineWidth = 0.f)
	{
		return MakeRoundedBrush(Fill, FVector4(Radius, Radius, Radius, Radius), Outline, OutlineWidth);
	}

	FButtonStyle MakeButtonStyle(const FTokens& Tokens, const EButtonLook Look, const float Radius, const FMargin& Padding)
	{
		FButtonStyle Style;
		switch (Look)
		{
		case EButtonLook::Primary:
			Style.Normal = MakeRoundedBrush(Tokens.Teal, Radius);
			Style.Hovered = MakeRoundedBrush(Tokens.TealDeep, Radius);
			Style.Pressed = MakeRoundedBrush(Tokens.Ink, Radius);
			Style.Disabled = MakeRoundedBrush(Tokens.Disabled, Radius);
			break;

		case EButtonLook::Secondary:
			Style.Normal = MakeRoundedBrush(Tokens.PaperStrong, Radius, Tokens.Line, 1.5f);
			Style.Hovered = MakeRoundedBrush(Tokens.Hover, Radius, Tokens.TealDeep, 1.5f);
			Style.Pressed = MakeRoundedBrush(Tokens.TealSoft, Radius, Tokens.TealDeep, 1.5f);
			Style.Disabled = MakeRoundedBrush(Tokens.Disabled, Radius, Tokens.Line, 1.f);
			break;

		case EButtonLook::Invisible:
		{
			FSlateBrush Nothing;
			Nothing.DrawAs = ESlateBrushDrawType::NoDrawType;
			Style.Normal = Nothing;
			Style.Hovered = Nothing;
			Style.Pressed = Nothing;
			Style.Disabled = Nothing;
			break;
		}
		}
		Style.NormalPadding = Padding;
		Style.PressedPadding = Padding;
		return Style;
	}

	UTextBlock* MakeText(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FName Name,
		const FText& Text, const int32 Size, const FLinearColor& Color, const FName Typeface,
		const bool bVariable = false)
	{
		UTextBlock* Block = MakeWidget<UTextBlock>(Blueprint, Tree, Name, bVariable);
		if (Block)
		{
			StyleText(*Block, Size, Color, Typeface);
			Block->SetText(Text);
			// 글자는 클릭을 먹지 않는다. 클릭은 버튼이 받는다.
			Block->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		return Block;
	}

	void AddToRow(UHorizontalBox& Row, UWidget* Child, const ESlateSizeRule::Type Rule,
		const FMargin& Padding = FMargin(0.f), const EVerticalAlignment VAlign = VAlign_Center)
	{
		if (!Child)
		{
			return;
		}
		if (UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(Child))
		{
			Slot->SetSize(FSlateChildSize(Rule));
			Slot->SetVerticalAlignment(VAlign);
			Slot->SetPadding(Padding);
		}
	}

	void AddToColumn(UVerticalBox& Column, UWidget* Child, const FMargin& Padding = FMargin(0.f),
		const EHorizontalAlignment HAlign = HAlign_Fill)
	{
		if (!Child)
		{
			return;
		}
		if (UVerticalBoxSlot* Slot = Column.AddChildToVerticalBox(Child))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign);
			Slot->SetPadding(Padding);
		}
	}

	void SetButtonContent(UButton& Button, UWidget& Content, const EHorizontalAlignment HAlign = HAlign_Center)
	{
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button.AddChild(&Content)))
		{
			Slot->SetHorizontalAlignment(HAlign);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	}

	// 캔버스에 앵커 · 정렬 · 위치로 붙인다. 크기를 주지 않으면 내용 크기를 따른다.
	UCanvasPanelSlot* Anchor(UCanvasPanel& Canvas, UWidget& Child, const FAnchors& Anchors, const FVector2D& Alignment,
		const FVector2D& Position, const FVector2D& Size = FVector2D::ZeroVector, const int32 ZOrder = 0)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Child);
		if (!Slot)
		{
			return nullptr;
		}
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		if (Size.IsNearlyZero())
		{
			Slot->SetAutoSize(true);
		}
		else
		{
			Slot->SetSize(Size);
		}
		Slot->SetZOrder(ZOrder);
		return Slot;
	}

	bool ResolveTokens(FTokens& OutTokens)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: UI 팔레트를 찾지 못했다."), DialogueLogPrefix);
			return false;
		}
		OutTokens = FFPauseOptionsLightTeal::Get(*Palette);
		return true;
	}

	/* 선택지 버튼 하나. 대사창의 ChoiceContainer 에 줄줄이 붙는다.

	   [ ›  잘 부탁드립니다.                                ]
	   바깥 테두리 아래 여백이 버튼 사이 간격이다. 담는 쪽(세로 상자)이 간격을 모르기 때문이다.
	*/
	bool BuildChoiceButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		FTokens Tokens;
		if (!ResolveTokens(Tokens))
		{
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ChoiceRoot"));
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, TEXT("ChoiceButton"), true);
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Choice"));
		UTextBlock* Mark = MakeText(Blueprint, Tree, TEXT("Text_ChoiceMark"),
			LOCTEXT("ChoiceMark", "›"), 24, Tokens.Teal, TEXT("SemiBold"));
		UTextBlock* Label = MakeText(Blueprint, Tree, TEXT("ChoiceText"),
			LOCTEXT("ChoicePreview", "잘 부탁드립니다."), 20, Tokens.Ink, TEXT("Medium"), true);
		if (!Root || !Button || !Row || !Mark || !Label)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeRoundedBrush(FLinearColor::Transparent, 0.f));
		Root->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Root->SetContent(Button);

		Button->SetStyle(MakeButtonStyle(Tokens, EButtonLook::Secondary, 10.f, FMargin(22.f, 12.f)));
		SetButtonContent(*Button, *Row, HAlign_Fill);

		Label->SetAutoWrapText(true);
		AddToRow(*Row, Mark, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 14.f, 2.f));
		AddToRow(*Row, Label, ESlateSizeRule::Fill);
		return true;
	}

	/* 대사창. 화면 전체에 붙고, 보이는 것은 아래 가운데 카드 · 왼쪽 초상화 · 오른쪽 위 건너뛰기다.

	   그리는 순서(ZOrder): 배경 그림 0 → 초상화 1 → 카드 2 → 전체 클릭 3 → 카드 클릭 4 → 건너뛰기 5.
	   클릭 버튼을 카드 위에 둔다. 대사 줄에서는 카드 어디를 눌러도 넘어가고, 선택지 줄에서는 C++ 가
	   클릭 버튼을 숨겨 카드 안의 선택지가 눌린다. 카드 자체는 클릭을 먹지 않아(SelfHitTestInvisible)
	   조작을 기다리는 줄에서 카드 뒤의 화면을 누를 수 있다.
	*/
	bool BuildDialogueBox(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		FTokens Tokens;
		if (!ResolveTokens(Tokens))
		{
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UImage* Background = MakeWidget<UImage>(Blueprint, Tree, TEXT("BackgroundImage"), true);
		UImage* Portrait = MakeWidget<UImage>(Blueprint, Tree, TEXT("PortraitImage"), true);

		USizeBox* CardSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Panel_Dialogue"));
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Card"));
		UVerticalBox* CardColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Card"));
		UHorizontalBox* SpeakerRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Speaker"));
		UBorder* SpeakerPlate = MakeWidget<UBorder>(Blueprint, Tree, TEXT("SpeakerPlate"), true);
		UTextBlock* Speaker = MakeText(Blueprint, Tree, TEXT("SpeakerText"),
			LOCTEXT("SpeakerPreview", "비서"), 19, Tokens.White, TEXT("SemiBold"), true);
		UTextBlock* Body = MakeText(Blueprint, Tree, TEXT("BodyText"),
			LOCTEXT("BodyPreview", "오셨네요. 기다리고 있었습니다."), 24, Tokens.Ink, TEXT("Medium"), true);
		UVerticalBox* Choices = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("ChoiceContainer"), true);
		USpacer* HintPush = MakeWidget<USpacer>(Blueprint, Tree, TEXT("Spacer_HintPush"));
		UHorizontalBox* Hint = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("AdvanceHint"), true);
		UTextBlock* HintText = MakeText(Blueprint, Tree, TEXT("Text_AdvanceHint"),
			LOCTEXT("AdvanceHint", "다음"), 15, Tokens.InkMuted, TEXT("SemiBold"));
		UTextBlock* HintMark = MakeText(Blueprint, Tree, TEXT("Text_AdvanceMark"),
			LOCTEXT("AdvanceMark", "›"), 20, Tokens.Teal, TEXT("SemiBold"));

		UButton* Advance = MakeWidget<UButton>(Blueprint, Tree, TEXT("AdvanceButton"), true);
		UButton* BoxAdvance = MakeWidget<UButton>(Blueprint, Tree, TEXT("BoxAdvanceButton"), true);

		UButton* Skip = MakeWidget<UButton>(Blueprint, Tree, TEXT("SkipButton"), true);
		UHorizontalBox* SkipRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Skip"));
		UTextBlock* SkipText = MakeText(Blueprint, Tree, TEXT("Text_Skip"),
			LOCTEXT("SkipLabel", "건너뛰기"), 16, Tokens.TealDeep, TEXT("SemiBold"));
		UTextBlock* SkipMark = MakeText(Blueprint, Tree, TEXT("Text_SkipMark"),
			LOCTEXT("SkipMark", "›"), 20, Tokens.TealDeep, TEXT("SemiBold"));

		if (!Root || !Background || !Portrait || !CardSize || !Card || !CardColumn || !SpeakerRow || !SpeakerPlate
			|| !Speaker || !Body || !Choices || !HintPush || !Hint || !HintText || !HintMark || !Advance || !BoxAdvance
			|| !Skip || !SkipRow || !SkipText || !SkipMark)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// ── 배경 그림 · 초상화 ───────────────────────────────────────────────
		// 둘 다 C++ 가 줄마다 켜고 끈다. 처음에는 접어 둔다.
		Background->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* Slot = StretchOnCanvas(*Root, *Background))
		{
			Slot->SetZOrder(0);
		}

		Portrait->SetVisibility(ESlateVisibility::Collapsed);
		Anchor(*Root, *Portrait, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(FMetrics::PortraitLeft, 0.f),
			FVector2D(FMetrics::PortraitWidth, FMetrics::PortraitHeight), 1);

		// ── 카드 ─────────────────────────────────────────────────────────────
		CardSize->SetWidthOverride(FMetrics::CardWidth);
		CardSize->SetMinDesiredHeight(FMetrics::CardMinHeight);
		CardSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CardSize->AddChild(Card);
		Anchor(*Root, *CardSize, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -FMetrics::CardBottom),
			FVector2D::ZeroVector, 2);

		Card->SetBrush(MakeRoundedBrush(Tokens.PaperStrong, FMetrics::CardRadius, Tokens.Line, 1.5f));
		Card->SetPadding(FMargin(40.f, 24.f, 40.f, 20.f));
		Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Card->SetContent(CardColumn);

		// 화자 이름표. 청록 알약 모양에 흰 글자. 화자가 없는 줄에서는 C++ 가 이름표째 숨긴다.
		SpeakerPlate->SetBrush(MakeRoundedBrush(Tokens.Teal, 17.f));
		SpeakerPlate->SetPadding(FMargin(18.f, 5.f, 18.f, 6.f));
		SpeakerPlate->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SpeakerPlate->SetContent(Speaker);
		AddToRow(*SpeakerRow, SpeakerPlate, ESlateSizeRule::Automatic);

		Body->SetAutoWrapText(true);
		Body->SetLineHeightPercentage(1.15f);

		Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddToRow(*Hint, HintText, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 6.f, 0.f));
		AddToRow(*Hint, HintMark, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 0.f, 2.f));

		Choices->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		AddToColumn(*CardColumn, SpeakerRow, FMargin(0.f, 0.f, 0.f, 12.f), HAlign_Left);
		AddToColumn(*CardColumn, Body);
		AddToColumn(*CardColumn, Choices, FMargin(0.f, 12.f, 0.f, 0.f));

		// 남는 높이를 빈 칸이 먹어 "다음" 이 카드 오른쪽 아래 구석에 붙는다. 대사가 짧아도 자리가 같다.
		if (UVerticalBoxSlot* Slot = CardColumn->AddChildToVerticalBox(HintPush))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		AddToColumn(*CardColumn, Hint, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Right);

		// ── 클릭 영역 ────────────────────────────────────────────────────────
		// 아무것도 그리지 않는다. 어느 쪽을 켤지는 줄마다 C++ 가 정한다(ApplyClickArea).
		Advance->SetStyle(MakeButtonStyle(Tokens, EButtonLook::Invisible, 0.f, FMargin(0.f)));
		Advance->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* Slot = StretchOnCanvas(*Root, *Advance))
		{
			Slot->SetZOrder(3);
		}

		BoxAdvance->SetStyle(MakeButtonStyle(Tokens, EButtonLook::Invisible, 0.f, FMargin(0.f)));
		BoxAdvance->SetVisibility(ESlateVisibility::Collapsed);
		Anchor(*Root, *BoxAdvance, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -FMetrics::CardBottom),
			FVector2D(FMetrics::CardWidth, FMetrics::BoxClickHeight), 4);

		// ── 건너뛰기 ─────────────────────────────────────────────────────────
		Skip->SetStyle(MakeButtonStyle(Tokens, EButtonLook::Secondary, 20.f, FMargin(20.f, 8.f, 16.f, 8.f)));
		AddToRow(*SkipRow, SkipText, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 8.f, 0.f));
		AddToRow(*SkipRow, SkipMark, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 0.f, 2.f));
		SetButtonContent(*Skip, *SkipRow);
		Anchor(*Root, *Skip, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-FMetrics::SkipMargin, FMetrics::SkipMargin),
			FVector2D::ZeroVector, 5);
		return true;
	}

	/* 건너뛰기 확인 창. 화면을 어둡게 덮고 가운데 카드를 띄운다.

	   ┌ 튜토리얼 건너뛰기 ───────────────┐  ← 청록 머리 띠
	   │ 튜토리얼을 건너뛰면 ...           │
	   │ [  취소  ]   [  건너뛰기  ]       │  ← 되돌릴 수 없는 쪽이 주 버튼
	   └───────────────────────────────────┘
	   어두운 막(Dimmer)은 클릭을 먹는다. 창이 떠 있는 동안 뒤의 대사창이 눌리면 안 된다.
	*/
	bool BuildSkipConfirm(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		FTokens Tokens;
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!ResolveTokens(Tokens) || !Palette)
		{
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UBorder* Dimmer = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Dimmer"));
		USizeBox* Window = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SkipWindow"));
		UVerticalBox* WindowColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Window"));
		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UTextBlock* Title = MakeText(Blueprint, Tree, TEXT("Text_Title"),
			LOCTEXT("ConfirmTitle", "튜토리얼 건너뛰기"), 22, Tokens.White, TEXT("SemiBold"));
		UBorder* Body = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Body"));
		UVerticalBox* BodyColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Body"));
		UTextBlock* Message = MakeText(Blueprint, Tree, TEXT("Text_Message"),
			LOCTEXT("ConfirmMessage", "튜토리얼을 건너뛰면 조작 가이드가 더 이상 나오지 않습니다.\n계속하시겠습니까?"),
			18, Tokens.Ink, TEXT("Regular"));
		UHorizontalBox* Buttons = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Buttons"));
		USizeBox* CancelSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Cancel"));
		UButton* Cancel = MakeWidget<UButton>(Blueprint, Tree, TEXT("CancelButton"), true);
		UTextBlock* CancelText = MakeText(Blueprint, Tree, TEXT("Text_Cancel"),
			LOCTEXT("ConfirmCancel", "취소"), 18, Tokens.TealDeep, TEXT("SemiBold"));
		USizeBox* SkipSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Skip"));
		UButton* Skip = MakeWidget<UButton>(Blueprint, Tree, TEXT("SkipButton"), true);
		UTextBlock* SkipText = MakeText(Blueprint, Tree, TEXT("Text_Skip"),
			LOCTEXT("ConfirmSkip", "건너뛰기"), 18, Tokens.White, TEXT("SemiBold"));

		if (!Root || !Dimmer || !Window || !WindowColumn || !Header || !Title || !Body || !BodyColumn || !Message
			|| !Buttons || !CancelSize || !Cancel || !CancelText || !SkipSize || !Skip || !SkipText)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 시간이 멈추는 화면에 쓰는 팔레트의 스크림 그대로다(UFF_UIPalette::InGame.Scrim 주석).
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(Palette->InGame.Scrim);
		Dimmer->SetBrush(DimBrush);
		Dimmer->SetVisibility(ESlateVisibility::Visible);
		StretchOnCanvas(*Root, *Dimmer);

		Window->SetWidthOverride(FMetrics::ConfirmWidth);
		Window->AddChild(WindowColumn);
		Anchor(*Root, *Window, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D::ZeroVector, 1);

		const float Radius = FMetrics::ConfirmRadius;
		Header->SetBrush(MakeRoundedBrush(Tokens.Teal, FVector4(Radius, Radius, 0.f, 0.f)));
		Header->SetPadding(FMargin(28.f, 16.f, 28.f, 16.f));
		Header->SetContent(Title);

		Body->SetBrush(MakeRoundedBrush(Tokens.PaperStrong, FVector4(0.f, 0.f, Radius, Radius)));
		Body->SetPadding(FMargin(28.f, 22.f, 28.f, 24.f));
		Body->SetContent(BodyColumn);
		Message->SetAutoWrapText(true);
		Message->SetLineHeightPercentage(1.15f);

		Cancel->SetStyle(MakeButtonStyle(Tokens, EButtonLook::Secondary, 8.f, FMargin(16.f, 0.f)));
		SetButtonContent(*Cancel, *CancelText);
		CancelSize->SetHeightOverride(FMetrics::ConfirmButtonHeight);
		CancelSize->AddChild(Cancel);

		Skip->SetStyle(MakeButtonStyle(Tokens, EButtonLook::Primary, 8.f, FMargin(16.f, 0.f)));
		SetButtonContent(*Skip, *SkipText);
		SkipSize->SetHeightOverride(FMetrics::ConfirmButtonHeight);
		SkipSize->AddChild(Skip);

		AddToRow(*Buttons, CancelSize, ESlateSizeRule::Fill, FMargin(0.f, 0.f, 6.f, 0.f));
		AddToRow(*Buttons, SkipSize, ESlateSizeRule::Fill, FMargin(6.f, 0.f, 0.f, 0.f));

		AddToColumn(*BodyColumn, Message);
		AddToColumn(*BodyColumn, Buttons, FMargin(0.f, 22.f, 0.f, 0.f));
		AddToColumn(*WindowColumn, Header);
		AddToColumn(*WindowColumn, Body);
		return true;
	}

	/* 대사창 CDO 의 선택지 버튼 클래스를 확인한다.

	   레이아웃만 바꾸면 기본값은 남지만, 처음 만드는 경우에는 비어 있다. 비어 있으면 선택지가 아예 안 뜬다.
	*/
	void EnsureChoiceButtonClass(UWidgetBlueprint* DialogueBox)
	{
		UClass* ChoiceClass = LoadGeneratedClass(ChoiceButtonPackage);
		UClass* BoxClass = DialogueBox ? DialogueBox->GeneratedClass.Get() : nullptr;
		FClassProperty* Property = FindFProperty<FClassProperty>(UFF_DialogueBoxWidget::StaticClass(), TEXT("ChoiceButtonClass"));
		if (!ChoiceClass || !BoxClass || !Property)
		{
			UE_LOG(LogFFUIWbpBuilder, Warning,
				TEXT("%s_WARN: 선택지 버튼 클래스를 확인하지 못했다. WBP_DialogueBoxWidget 의 Choice Button Class 를 볼 것."),
				DialogueLogPrefix);
			return;
		}

		UObject* Defaults = BoxClass->GetDefaultObject();
		if (Property->GetPropertyValue_InContainer(Defaults) == ChoiceClass)
		{
			return;
		}

		Property->SetPropertyValue_InContainer(Defaults, ChoiceClass);
		SaveAssetPackage(DialogueLogPrefix, *DialogueBox);
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s: 선택지 버튼 클래스를 %s 로 맞췄다."),
			DialogueLogPrefix, *ChoiceClass->GetName());
	}

	// 선택지 버튼을 먼저 만든다. 대사창 CDO 가 그 클래스를 가리킨다.
	void BuildAll(const bool bReplace)
	{
		auto Make = [bReplace](const FString& Package, UClass& Parent, TFunction<bool(UWidgetBlueprint&, UWidgetTree&)> Layout)
		{
			return bReplace
				? ReplaceWidgetBlueprint(DialogueLogPrefix, Package, TutorialFolder, Parent, MoveTemp(Layout))
				: EnsureWidgetBlueprint(DialogueLogPrefix, Package, TutorialFolder, Parent, MoveTemp(Layout));
		};

		Make(ChoiceButtonPackage, *UFF_DialogueChoiceButton::StaticClass(), &BuildChoiceButton);
		Make(SkipConfirmPackage, *UFF_SkipConfirmWidget::StaticClass(), &BuildSkipConfirm);
		UWidgetBlueprint* DialogueBox = Make(DialogueBoxPackage, *UFF_DialogueBoxWidget::StaticClass(), &BuildDialogueBox);
		EnsureChoiceButtonClass(DialogueBox);
	}

	void RunTutorialDialogueUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BEGIN: 없는 튜토리얼 대사 WBP 만 만든다."), DialogueLogPrefix);
		BuildAll(false);
	}

	void RebuildTutorialDialogueUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Warning,
			TEXT("%s_REBUILD: 대사창 · 선택지 버튼 · 건너뛰기 확인 창의 레이아웃을 처음부터 다시 만든다. 디자이너 수정은 사라진다."),
			DialogueLogPrefix);
		BuildAll(true);
	}

	FAutoConsoleCommand TutorialDialogueUmgCommand(
		TEXT("FF.TutorialDialogueUMG"),
		TEXT("튜토리얼 대사창 · 선택지 버튼 · 건너뛰기 확인 창 WBP 를 만든다. 이미 있으면 덮어쓰지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunTutorialDialogueUmg));

	// 파괴적이다. 이름에 Rebuild 를 박아 실수로 부르지 않게 한다.
	FAutoConsoleCommand TutorialDialogueUmgRebuildCommand(
		TEXT("FF.TutorialDialogueUMG.Rebuild"),
		TEXT("튜토리얼 대사 WBP 세 개의 레이아웃을 게임 UI 모양으로 처음부터 다시 만든다. 디자이너에서 바꾼 배치는 사라진다."),
		FConsoleCommandDelegate::CreateStatic(&RebuildTutorialDialogueUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
