// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_PlaylistPlayerWbpBuilder.cpp
// 역할: 축제 플레이리스트 미니 플레이어의 편집 가능한 화면 틀을 만든다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR

// 비파괴 WBP 스캐폴더. 콘솔 명령은 `FF.PlaylistPlayerUMG` 다.
// 이미 있으면 손대지 않는다. 디자이너에서 옮기고 꾸민 것이 그대로 남는다.
// 처음부터 다시 만들려면 에셋을 지우고 명령을 다시 실행한다.
//
// 색은 일시정지·설정 화면과 같은 LightTeal 팔레트(FFPauseOptionsLightTeal)를 쓴다.
// 면은 9-slice 텍스처 대신 단색 둥근 면이다. 작은 크기에서 텍스처 테두리가 늘어나 깨졌다.
// 디자이너 수정을 버리고 다시 만들려면 `FF.PlaylistPlayerUMG.Rebuild` 를 쓴다.

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Audio/FF_PlaylistPlayerWidget.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPlaylistPlayerWbpBuilder"

namespace FFPlaylistPlayerWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const PlaylistLogPrefix = TEXT("FF_PLAYLIST_UMG");
	const FString PlaylistFolder(TEXT("/Game/FestaFiesta/UI/Audio"));
	const FString PlaylistPlayerPackage(PlaylistFolder / TEXT("WBP_FF_PlaylistPlayer"));

	// 1920x1080 기준. 화면 배율은 UIScaleRule 이 처리한다.
	struct FPlayerMetrics
	{
		static constexpr float Width = 420.f;
		static constexpr float LeftMargin = 24.f;
		// 준비·운영 HUD 의 아래 띠(약 58~68) 위로 띄운다.
		static constexpr float BottomOffset = 84.f;
		static constexpr float BarRadius = 10.f;
		static constexpr float IconSize = 30.f;
		static constexpr float ProgressHeight = 3.f;
		static constexpr float ListMaxHeight = 280.f;
	};

	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, const float Radius,
		const FLinearColor& Outline = FLinearColor::Transparent, const float OutlineWidth = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	/* 단색 둥근 버튼 모양.

	   일시정지 화면의 9-slice 버튼 텍스처는 큰 메뉴 버튼 크기에 맞춘 것이라, 이 플레이어처럼 작은
	   버튼에 쓰면 테두리가 두껍게 늘어나 깨져 보였다. 그래서 같은 팔레트 색을 단색 둥근 면으로 쓴다.
	*/
	FButtonStyle MakeFlatButtonStyle(const FFPauseOptionsLightTeal::FTokens& Tokens, const bool bPrimary,
		const float Radius, const FMargin& Padding)
	{
		FButtonStyle Style;
		if (bPrimary)
		{
			Style.Normal = MakeRoundedBrush(Tokens.Teal, Radius);
			Style.Hovered = MakeRoundedBrush(Tokens.TealDeep, Radius);
			Style.Pressed = MakeRoundedBrush(Tokens.Ink, Radius);
		}
		else
		{
			Style.Normal = MakeRoundedBrush(FLinearColor::Transparent, Radius);
			Style.Hovered = MakeRoundedBrush(Tokens.Hover, Radius, Tokens.Line, 1.f);
			Style.Pressed = MakeRoundedBrush(Tokens.TealSoft, Radius);
		}
		Style.Disabled = MakeRoundedBrush(Tokens.Disabled, Radius);
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
			Block->SetClipping(EWidgetClipping::ClipToBounds);
			Block->SetAutoWrapText(false);
			Block->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		}
		return Block;
	}

	// 정사각 아이콘 버튼. 기호 글자 하나를 가운데 둔다.
	USizeBox* MakeIconButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const FName ButtonName, const FName TextName, const FText& Glyph,
		const FFPauseOptionsLightTeal::FTokens& Tokens, const bool bPrimary, const bool bTextVariable)
	{
		USizeBox* Size = MakeWidget<USizeBox>(Blueprint, Tree, FName(*(ButtonName.ToString() + TEXT("_Size"))));
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UTextBlock* Text = MakeText(Blueprint, Tree, TextName, Glyph, 11,
			bPrimary ? Tokens.White : Tokens.TealDeep, TEXT("SemiBold"), bTextVariable);
		if (!Size || !Button || !Text)
		{
			return nullptr;
		}
		Text->SetJustification(ETextJustify::Center);
		Text->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
		Button->SetStyle(MakeFlatButtonStyle(Tokens, bPrimary, FPlayerMetrics::IconSize * 0.5f, FMargin(0.f)));
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f));
		}
		Size->SetWidthOverride(FPlayerMetrics::IconSize);
		Size->SetHeightOverride(FPlayerMetrics::IconSize);
		Size->AddChild(Button);
		return Size;
	}

	// 글자 버튼. 목록 열기·자동 다음 곡·셔플에 쓴다.
	UButton* MakeTextButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const FName ButtonName, const FName TextName, const FText& Label,
		const FFPauseOptionsLightTeal::FTokens& Tokens, const int32 FontSize)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UTextBlock* Text = MakeText(Blueprint, Tree, TextName, Label, FontSize, Tokens.TealDeep, TEXT("SemiBold"), true);
		if (!Button || !Text)
		{
			return nullptr;
		}
		Text->SetJustification(ETextJustify::Center);
		FButtonStyle Style = MakeFlatButtonStyle(Tokens, false, 6.f, FMargin(10.f, 5.f));
		Style.Normal = MakeRoundedBrush(Tokens.PaperSunken, 6.f, Tokens.Line, 1.f);
		Button->SetStyle(Style);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}

	void AddToRow(UHorizontalBox& Row, UWidget* Child, const ESlateSizeRule::Type Rule,
		const FMargin& Padding = FMargin(0.f))
	{
		if (!Child)
		{
			return;
		}
		if (UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(Child))
		{
			Slot->SetSize(FSlateChildSize(Rule));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(Padding);
		}
	}

	void AddToColumn(UVerticalBox& Column, UWidget* Child, const FMargin& Padding = FMargin(0.f))
	{
		if (!Child)
		{
			return;
		}
		if (UVerticalBoxSlot* Slot = Column.AddChildToVerticalBox(Child))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetPadding(Padding);
		}
	}

	/* 화면 왼쪽 아래, HUD 아래 띠 바로 위에 한 줄 바로 둔다.

	   ┌ 목록(펼쳤을 때만) ───────────────┐   ← 위로 펼쳐진다
	   │ [자동 다음 곡] [셔플]             │
	   │ 1. 곡 이름 ...                    │
	   └───────────────────────────────────┘
	   ┌───────────────────────────────────┐
	   │ (♪) 곡 이름          ◀◀  ||  ▶▶  목록 │
	   │     2 / 14 · 0:04 / 3:31          │
	   │ ━━━━━━━━━━──────────────────────── │
	   └───────────────────────────────────┘
	*/
	bool BuildPlaylistPlayer(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);

		// ── 뼈대 ─────────────────────────────────────────────────────────────
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		USizeBox* PlayerSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Panel_Player"), true);
		UVerticalBox* PlayerColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Player"));
		if (!Root || !PlayerSize || !PlayerColumn)
		{
			return false;
		}
		Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// 왼쪽 아래 기준으로 붙인다. 높이가 자라면(목록을 펼치면) 위로 늘어난다.
		if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(PlayerSize))
		{
			Slot->SetAnchors(FAnchors(0.f, 1.f));
			Slot->SetAlignment(FVector2D(0.f, 1.f));
			Slot->SetAutoSize(true);
			Slot->SetPosition(FVector2D(FPlayerMetrics::LeftMargin, -FPlayerMetrics::BottomOffset));
		}
		PlayerSize->SetWidthOverride(FPlayerMetrics::Width);
		PlayerSize->AddChild(PlayerColumn);

		const FSlateBrush CardBrush = MakeRoundedBrush(
			FFPauseOptionsLightTeal::WithAlpha(Tokens.PaperStrong, 0.96f), FPlayerMetrics::BarRadius, Tokens.Line, 1.f);

		// ── 펼침: 자동 다음 곡·셔플 + 곡 목록 (바 위) ───────────────────────
		UBorder* ListPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_List"), true);
		UVerticalBox* ListColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_List"));
		UHorizontalBox* Toggles = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Toggles"));
		UButton* AutoAdvance = MakeTextButton(Blueprint, Tree, TEXT("Button_AutoAdvance"),
			TEXT("Text_AutoAdvance"), LOCTEXT("AutoOn", "자동 다음 곡 켬"), Tokens, 12);
		UButton* Shuffle = MakeTextButton(Blueprint, Tree, TEXT("Button_Shuffle"),
			TEXT("Text_Shuffle"), LOCTEXT("ShuffleOff", "셔플 끔"), Tokens, 12);
		USizeBox* ListSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Tracks"));
		UScrollBox* Tracks = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Tracks"), true);
		if (!ListPanel || !ListColumn || !Toggles || !AutoAdvance || !Shuffle || !ListSize || !Tracks)
		{
			return false;
		}
		ListPanel->SetBrush(CardBrush);
		ListPanel->SetPadding(FMargin(10.f));
		ListPanel->SetContent(ListColumn);
		// 처음엔 접혀 있다. 위젯이 목록 버튼으로 펼친다.
		ListPanel->SetVisibility(ESlateVisibility::Collapsed);

		AddToRow(*Toggles, AutoAdvance, ESlateSizeRule::Fill, FMargin(0.f, 0.f, 6.f, 0.f));
		AddToRow(*Toggles, Shuffle, ESlateSizeRule::Fill);

		ListSize->SetMaxDesiredHeight(FPlayerMetrics::ListMaxHeight);
		ListSize->AddChild(Tracks);
		Tracks->SetScrollBarVisibility(ESlateVisibility::Visible);

		AddToColumn(*ListColumn, Toggles);
		AddToColumn(*ListColumn, ListSize, FMargin(0.f, 8.f, 0.f, 0.f));
		AddToColumn(*PlayerColumn, ListPanel, FMargin(0.f, 0.f, 0.f, 6.f));

		// ── 한 줄 바 ─────────────────────────────────────────────────────────
		UBorder* Bar = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Bar"));
		UVerticalBox* BarColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Bar"));
		UHorizontalBox* BarRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Bar"));

		USizeBox* IconSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Icon"));
		UBorder* Icon = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Icon"));
		UTextBlock* IconGlyph = MakeText(Blueprint, Tree, TEXT("Text_Icon"),
			LOCTEXT("IconGlyph", "♪"), 14, Tokens.White, TEXT("SemiBold"));

		UVerticalBox* TitleColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Title"));
		UTextBlock* Title = MakeText(Blueprint, Tree, TEXT("Text_TrackTitle"),
			LOCTEXT("TitlePreview", "See You Again Next Time"), 13, Tokens.Ink, TEXT("SemiBold"), true);
		UTextBlock* Meta = MakeText(Blueprint, Tree, TEXT("Text_TrackMeta"),
			LOCTEXT("MetaPreview", "2 / 14  ·  0:04 / 3:31"), 10, Tokens.InkMuted, TEXT("Medium"), true);

		USizeBox* Prev = MakeIconButton(Blueprint, Tree, TEXT("Button_Prev"), TEXT("Text_Prev"),
			LOCTEXT("PrevGlyph", "◀◀"), Tokens, false, false);
		USizeBox* PlayPause = MakeIconButton(Blueprint, Tree, TEXT("Button_PlayPause"), TEXT("Text_PlayPause"),
			LOCTEXT("PauseGlyph", "||"), Tokens, true, true);
		USizeBox* Next = MakeIconButton(Blueprint, Tree, TEXT("Button_Next"), TEXT("Text_Next"),
			LOCTEXT("NextGlyph", "▶▶"), Tokens, false, false);
		UButton* ToggleList = MakeTextButton(Blueprint, Tree, TEXT("Button_ToggleList"),
			TEXT("Text_ToggleList"), LOCTEXT("ListOpen", "목록"), Tokens, 11);

		USizeBox* ProgressSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Progress"));
		UProgressBar* Progress = MakeWidget<UProgressBar>(Blueprint, Tree, TEXT("ProgressBar_Track"), true);

		if (!Bar || !BarColumn || !BarRow || !IconSize || !Icon || !IconGlyph || !TitleColumn || !Title
			|| !Meta || !Prev || !PlayPause || !Next || !ToggleList || !ProgressSize || !Progress)
		{
			return false;
		}

		Bar->SetBrush(CardBrush);
		Bar->SetPadding(FMargin(8.f, 6.f, 8.f, 6.f));
		Bar->SetContent(BarColumn);

		// 왼쪽 원형 음표. 플레이리스트라는 것을 글 없이 알린다.
		Icon->SetBrush(MakeRoundedBrush(Tokens.Teal, FPlayerMetrics::IconSize * 0.5f));
		Icon->SetHorizontalAlignment(HAlign_Center);
		Icon->SetVerticalAlignment(VAlign_Center);
		Icon->SetContent(IconGlyph);
		IconGlyph->SetJustification(ETextJustify::Center);
		IconGlyph->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
		IconSize->SetWidthOverride(FPlayerMetrics::IconSize);
		IconSize->SetHeightOverride(FPlayerMetrics::IconSize);
		IconSize->AddChild(Icon);

		AddToColumn(*TitleColumn, Title);
		AddToColumn(*TitleColumn, Meta, FMargin(0.f, 1.f, 0.f, 0.f));

		AddToRow(*BarRow, IconSize, ESlateSizeRule::Automatic);
		AddToRow(*BarRow, TitleColumn, ESlateSizeRule::Fill, FMargin(10.f, 0.f, 8.f, 0.f));
		AddToRow(*BarRow, Prev, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 2.f, 0.f));
		AddToRow(*BarRow, PlayPause, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 2.f, 0.f));
		AddToRow(*BarRow, Next, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 8.f, 0.f));
		AddToRow(*BarRow, ToggleList, ESlateSizeRule::Automatic);

		FProgressBarStyle ProgressStyle;
		ProgressStyle.BackgroundImage = MakeRoundedBrush(Tokens.Line, 1.5f);
		ProgressStyle.FillImage = MakeRoundedBrush(FLinearColor::White, 1.5f);
		ProgressStyle.MarqueeImage = ProgressStyle.FillImage;
		Progress->SetWidgetStyle(ProgressStyle);
		Progress->SetFillColorAndOpacity(Tokens.Teal);
		Progress->SetPercent(0.2f);
		ProgressSize->SetHeightOverride(FPlayerMetrics::ProgressHeight);
		ProgressSize->AddChild(Progress);

		AddToColumn(*BarColumn, BarRow);
		AddToColumn(*BarColumn, ProgressSize, FMargin(2.f, 6.f, 2.f, 0.f));
		AddToColumn(*PlayerColumn, Bar);

		return true;
	}

	void RunPlaylistPlayerUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: 없는 플레이리스트 플레이어 WBP 만 만든다."), PlaylistLogPrefix);
		EnsureWidgetBlueprint(PlaylistLogPrefix, PlaylistPlayerPackage, PlaylistFolder,
			*UFF_PlaylistPlayerWidget::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPlaylistPlayer(Blueprint, Tree);
			});
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 위치·크기 마감은 WBP 에서 비파괴로 조정한다."), PlaylistLogPrefix);
	}

	void RebuildPlaylistPlayerUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Warning,
			TEXT("%s_REBUILD: 플레이리스트 플레이어 WBP 의 레이아웃을 처음부터 다시 만든다. 디자이너 수정은 사라진다."),
			PlaylistLogPrefix);
		ReplaceWidgetBlueprint(PlaylistLogPrefix, PlaylistPlayerPackage, PlaylistFolder,
			*UFF_PlaylistPlayerWidget::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPlaylistPlayer(Blueprint, Tree);
			});
	}

	FAutoConsoleCommand PlaylistPlayerUmgCommand(
		TEXT("FF.PlaylistPlayerUMG"),
		TEXT("축제 플레이리스트 미니 플레이어 WBP 를 만든다. 이미 있으면 덮어쓰지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunPlaylistPlayerUmg));

	// 🔴 파괴적이다. 이름에 Rebuild 를 박아 실수로 부르지 않게 한다.
	FAutoConsoleCommand PlaylistPlayerUmgRebuildCommand(
		TEXT("FF.PlaylistPlayerUMG.Rebuild"),
		TEXT("플레이리스트 플레이어 WBP 레이아웃을 처음부터 다시 만든다. 디자이너에서 바꾼 배치는 사라진다."),
		FConsoleCommandDelegate::CreateStatic(&RebuildPlaylistPlayerUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
