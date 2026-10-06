// SNS·음악 펼침 패널을 2026-10-07 승인 시안으로 다시 짠다. 도크 버튼(아이콘)은 건드리지 않는다.
//
// - 대상: WBP_FF_LiveSnsFlyout · WBP_FF_PlaylistFlyout · WBP_FF_PlaylistTrackRow, 새 WBP_FF_LiveSnsFlyoutPostRow.
//   기존 카드형 SNS 행(WBP_FF_LiveSnsPostEntry)과 하단 SNS 패널은 그대로 둔다.
// - 처음 한 번만 적용한다. 새 구조(밑줄·한 곡 반복 아이콘·행 바탕)가 이미 있으면 건너뛰어 이후 Designer 편집을 지킨다.
// - 트리를 갈기 전에 원본을 Saved/UIBackups/LiveFlyoutRedesign 에 복사한다. 바인딩 이름은 C++ 그대로다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/Audio/FF_PlaylistPlayerWidget.h"
#include "UI/Audio/FF_PlaylistTrackRowWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveFlyoutRedesign"

namespace FFLiveFlyoutRedesign
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_FLYOUT_REDESIGN");
	const FString SnsFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels");
	const FString SnsPackage = SnsFolder / TEXT("WBP_FF_LiveSnsFlyout");
	const FString SnsRowPackage = SnsFolder / TEXT("WBP_FF_LiveSnsFlyoutPostRow");
	const FString AudioFolder = TEXT("/Game/FestaFiesta/UI/Audio");
	const FString MusicPackage = AudioFolder / TEXT("WBP_FF_PlaylistFlyout");
	const FString TrackRowPackage = AudioFolder / TEXT("WBP_FF_PlaylistTrackRow");
	const FString IconRoot = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

	// 승인안 색. 흰 바탕 · 아주 연한 하늘색 외곽선 · 짙은 초록 글자.
	const FLinearColor White = FLinearColor::White;
	const FLinearColor Outline = Color(204, 230, 240);
	const FLinearColor Divider = Color(228, 238, 240);
	const FLinearColor Hint = Color(118, 140, 133);
	const FLinearColor Mint = Color(222, 244, 234);
	const FLinearColor MintLine = Color(186, 228, 208);
	const FLinearColor MintInk = Color(20, 112, 74);
	const FLinearColor SkyHover = Color(234, 245, 250);
	const FLinearColor ProgressTrack = Color(222, 237, 243);
	const FLinearColor AvatarFill = Color(236, 243, 244);

	UTexture2D* Icon(const TCHAR* Group, const TCHAR* Key)
	{
		const FString Package = IconRoot / Group / FString::Printf(TEXT("T_FF_LH_Icon_%s_White_256"), Key);
		UTexture2D* Result = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Package));
		if (!Result) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 아이콘이 없다: %s"), LogPrefix, *Package); }
		return Result;
	}

	UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, const FText& Preview,
		int32 Size, FLinearColor Tint, const TCHAR* Typeface = TEXT("Regular"), bool bVariable = true)
	{
		UTextBlock* Result = MakeWidget<UTextBlock>(BP, Tree, Name, bVariable);
		StyleText(*Result, Size, Tint, Typeface);
		Result->SetText(Preview);
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}

	USizeBox* Sized(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name, UWidget& Child, float Width = 0.f, float Height = 0.f)
	{
		USizeBox* Result = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) { Result->SetWidthOverride(Width); }
		if (Height > 0.f) { Result->SetHeightOverride(Height); }
		Result->AddChild(&Child);
		return Result;
	}

	/** 텍스처 아이콘. 크기는 감싼 SizeBox 가 정한다(생성 실행에서 텍스처 크기 맞춤은 0×0 이 된다 — 안티패턴 23). */
	USizeBox* IconBox(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UTexture2D* Texture,
		float Size, FLinearColor Tint, bool bVariable = false)
	{
		UImage* Image = MakeWidget<UImage>(BP, Tree, Name, bVariable);
		FSlateBrush ImageBrush;
		ImageBrush.SetResourceObject(Texture);
		ImageBrush.ImageSize = FVector2D(Size, Size);
		Image->SetBrush(ImageBrush);
		Image->SetColorAndOpacity(Tint);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Sized(BP, Tree, FName(FString(TEXT("SizeBox_")) + FString(Name).RightChop(6)), *Image, Size, Size);
	}

	/** 단색 면(구분선·강조선·점). 텍스처 없이 둥근 상자로 그린다. */
	UImage* Fill(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, FLinearColor Tint, float Radius = 0.f, bool bVariable = true)
	{
		UImage* Result = MakeWidget<UImage>(BP, Tree, Name, bVariable);
		Result->SetBrush(Brush(Tint, Radius));
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}

	void Align(UWidget& Child, EHorizontalAlignment H, EVerticalAlignment V)
	{
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Child.Slot)) { HSlot->SetHorizontalAlignment(H); HSlot->SetVerticalAlignment(V); }
		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Child.Slot)) { VSlot->SetHorizontalAlignment(H); VSlot->SetVerticalAlignment(V); }
		if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(Child.Slot)) { OSlot->SetHorizontalAlignment(H); OSlot->SetVerticalAlignment(V); }
		if (UButtonSlot* BSlot = Cast<UButtonSlot>(Child.Slot)) { BSlot->SetHorizontalAlignment(H); BSlot->SetVerticalAlignment(V); }
	}

	UOverlaySlot* Layer(UOverlay& Parent, UWidget& Child, EHorizontalAlignment H, EVerticalAlignment V, FMargin Padding = FMargin(0.f))
	{
		UOverlaySlot* Result = Parent.AddChildToOverlay(&Child);
		Result->SetHorizontalAlignment(H); Result->SetVerticalAlignment(V); Result->SetPadding(Padding);
		return Result;
	}

	/** 평소엔 그리지 않고 마우스·누름에만 옅게 반응하는 버튼. 내용은 칸 전체를 쓴다(안티패턴 20). */
	UButton* QuietButton(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& Content, float Radius)
	{
		UButton* Result = MakeWidget<UButton>(BP, Tree, Name, true);
		FButtonStyle Style = Result->GetStyle();
		Style.Normal = FSlateNoResource();
		Style.Hovered = Brush(SkyHover, Radius);
		Style.Pressed = Brush(Color(220, 238, 246), Radius);
		Style.Disabled = FSlateNoResource();
		Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
		Result->SetStyle(Style);
		Result->AddChild(&Content);
		Align(Content, HAlign_Fill, VAlign_Fill);
		CastChecked<UButtonSlot>(Content.Slot)->SetPadding(FMargin(0.f));
		return Result;
	}

	/** 오른쪽 위 닫기. 연한 외곽선의 작은 사각 버튼, 아이콘은 가운데. */
	USizeBox* CloseButton(UWidgetBlueprint& BP, UWidgetTree& Tree, const FText& Tooltip)
	{
		USizeBox* IconSize = CloseIcon(BP, Tree);
		IconSize->SetWidthOverride(14.f); IconSize->SetHeightOverride(14.f);
		UButton* Close = MakeWidget<UButton>(BP, Tree, TEXT("Button_Close"), true);
		FButtonStyle Style = Close->GetStyle();
		Style.Normal = Brush(White, 7.f, 1.f, Outline);
		Style.Hovered = Brush(SkyHover, 7.f, 1.f, Color(150, 200, 220));
		Style.Pressed = Brush(Color(220, 238, 246), 7.f, 1.f, Ink);
		Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
		Close->SetStyle(Style);
		Close->SetToolTipText(Tooltip);
		Close->AddChild(IconSize);
		Align(*IconSize, HAlign_Center, VAlign_Center);
		return Sized(BP, Tree, TEXT("SizeBox_Close"), *Close, 30.f, 30.f);
	}

	USizeBox* Line(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, bool bVariable = false)
	{
		return Sized(BP, Tree, FName(FString(Name) + TEXT("_Size")), *Fill(BP, Tree, Name, Divider, 0.f, bVariable), 0.f, 1.f);
	}

	UBorder* PanelRoot(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, bool bVariable)
	{
		UBorder* Root = MakeWidget<UBorder>(BP, Tree, Name, bVariable);
		Root->SetBrush(Brush(White, 12.f, 1.f, Outline));
		Root->SetPadding(FMargin(18.f, 16.f, 18.f, 12.f));
		Root->SetClipping(EWidgetClipping::ClipToBounds);
		Tree.RootWidget = Root;
		return Root;
	}

	UScrollBox* List(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name)
	{
		UScrollBox* Result = MakeWidget<UScrollBox>(BP, Tree, Name, true);
		Result->SetAllowOverscroll(false);
		Result->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Result->SetClipping(EWidgetClipping::ClipToBounds);
		// 막대 모양·폭·여백은 패널 Class Defaults 의 Scroll Style 이 정한다.
		return Result;
	}

	// ── SNS ───────────────────────────────────────────────────────────────

	bool BuildSnsRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UVerticalBox* Root = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Root"));
		Tree.RootWidget = Root;
		UBorder* Card = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Card"), true);
		Card->SetBrush(FSlateNoResource());
		Card->SetPadding(FMargin(2.f, 11.f, 4.f, 11.f));
		Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBox* Post = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Post"));
		Card->AddChild(Post);

		UBorder* Avatar = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Avatar"));
		Avatar->SetBrush(Brush(AvatarFill, 17.f));
		Avatar->SetHorizontalAlignment(HAlign_Center); Avatar->SetVerticalAlignment(VAlign_Center);
		Avatar->AddChild(IconBox(BP, Tree, TEXT("Image_Avatar"), Icon(TEXT("AudienceGroups"), TEXT("GroupSolo")), 17.f, Hint));
		USizeBox* AvatarSize = Sized(BP, Tree, TEXT("SizeBox_AvatarCircle"), *Avatar, 34.f, 34.f);
		Column(*Post, *AvatarSize, FMargin(0.f, 0.f, 12.f, 0.f));
		Align(*AvatarSize, HAlign_Left, VAlign_Center);

		UVerticalBox* Texts = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Text"));
		UHorizontalBox* Meta = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Meta"));
		UTextBlock* Author = Label(BP, Tree, TEXT("Text_Author"), LOCTEXT("Author", "방문객 #0000"), 13, Ink, TEXT("SemiBold"));
		Author->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Column(*Meta, *Author, FMargin(0.f, 0.f, 8.f, 0.f), true);
		Align(*Author, HAlign_Fill, VAlign_Center);
		USizeBox* Mood = IconBox(BP, Tree, TEXT("Image_Sentiment"), Icon(TEXT("Social"), TEXT("SentimentPositive")), 15.f, Green, true);
		Column(*Meta, *Mood, FMargin(0.f, 0.f, 4.f, 0.f));
		Align(*Mood, HAlign_Left, VAlign_Center);
		UTextBlock* MoodText = Label(BP, Tree, TEXT("Text_Sentiment"), LOCTEXT("Positive", "긍정"), 12, Green, TEXT("Medium"));
		Column(*Meta, *MoodText);
		Align(*MoodText, HAlign_Left, VAlign_Center);
		Row(*Texts, *Meta);
		UTextBlock* Body = Label(BP, Tree, TEXT("Text_Body"), LOCTEXT("Body", "관객의 반응이 여기에 표시됩니다."), 14, Ink);
		Body->SetAutoWrapText(true);
		Row(*Texts, *Body, FMargin(0.f, 3.f, 0.f, 0.f));
		Column(*Post, *Texts, FMargin(0.f), true);
		Align(*Texts, HAlign_Fill, VAlign_Center);

		Row(*Root, *Card);
		Row(*Root, *Line(BP, Tree, TEXT("Image_Divider")));
		return true;
	}

	bool BuildSns(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UBorder* Root = PanelRoot(BP, Tree, TEXT("Border_PanelRoot"), false);
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Sections"));
		Root->AddChild(Sections);

		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Header"));
		USizeBox* TitleIcon = IconBox(BP, Tree, TEXT("Image_TitleIcon"), Icon(TEXT("Navigation"), TEXT("SNS")), 26.f, Ink);
		Column(*Header, *TitleIcon, FMargin(0.f, 0.f, 12.f, 0.f));
		Align(*TitleIcon, HAlign_Left, VAlign_Center);
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Heading"));
		Row(*Heading, *Label(BP, Tree, TEXT("Text_PanelTitle"), LOCTEXT("SnsTitle", "SNS"), 21, Ink, TEXT("SemiBold")));
		UTextBlock* Subtitle = Label(BP, Tree, TEXT("Text_Subtitle"), LOCTEXT("SnsSubtitle", "관객들이 전하는 축제 이야기"), 13, Muted);
		Subtitle->SetAutoWrapText(true);
		Row(*Heading, *Subtitle, FMargin(0.f, 1.f, 0.f, 0.f));
		Column(*Header, *Heading, FMargin(0.f, 0.f, 8.f, 0.f), true);
		Align(*Heading, HAlign_Fill, VAlign_Center);
		USizeBox* Close = CloseButton(BP, Tree, LOCTEXT("SnsClose", "SNS 닫기"));
		Column(*Header, *Close);
		Align(*Close, HAlign_Right, VAlign_Center);
		Row(*Sections, *Header, FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Sections, *Line(BP, Tree, TEXT("Rule_Header")), FMargin(0.f, 0.f, 0.f, 2.f));

		// 큰 사각 버튼 대신 탭. 선택한 탭만 밑줄과 굵은 글자(런타임이 Class Defaults 값으로 바꾼다).
		UHorizontalBox* Filters = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Filters"));
		const TCHAR* Suffixes[] = { TEXT("All"), TEXT("Positive"), TEXT("Negative") };
		const FText Names[] = { LOCTEXT("All", "전체"), LOCTEXT("PositiveTab", "긍정"), LOCTEXT("Negative", "아쉬움") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Suffixes); ++Index)
		{
			const FString Key = Suffixes[Index];
			UVerticalBox* Tab = MakeWidget<UVerticalBox>(BP, Tree, FName(TEXT("VerticalBox_Filter") + Key));
			UTextBlock* TabText = Label(BP, Tree, *(TEXT("Text_Filter") + Key), Names[Index], 15, Index == 0 ? MintInk : Muted,
				Index == 0 ? TEXT("SemiBold") : TEXT("Medium"));
			TabText->SetJustification(ETextJustify::Center);
			Row(*Tab, *TabText, FMargin(0.f), true);
			Align(*TabText, HAlign_Fill, VAlign_Center);
			UImage* Underline = Fill(BP, Tree, *(TEXT("Underline_Filter") + Key), Green, 1.5f);
			if (Index > 0) { Underline->SetVisibility(ESlateVisibility::Hidden); }
			Row(*Tab, *Sized(BP, Tree, FName(TEXT("SizeBox_Underline") + Key), *Underline, 0.f, 3.f), FMargin(14.f, 0.f));
			UButton* TabButton = QuietButton(BP, Tree, *(TEXT("Button_Filter") + Key), *Tab, 6.f);
			Column(*Filters, *Sized(BP, Tree, FName(TEXT("SizeBox_Filter") + Key), *TabButton, 0.f, 40.f), FMargin(0.f), true);
		}
		Row(*Sections, *Filters, FMargin(0.f, 0.f, 0.f, 8.f));
		Row(*Sections, *Label(BP, Tree, TEXT("Text_Count"), LOCTEXT("Count", "최근 0개 · 최신순"), 12, Muted), FMargin(0.f, 0.f, 0.f, 6.f));

		// 새 글 안내: 얇은 민트 버튼. 새 글이 없을 땐 숨기되 자리를 지켜 목록이 밀리지 않는다.
		UButton* NewPosts = MakeWidget<UButton>(BP, Tree, TEXT("Button_NewPosts"), true);
		FButtonStyle NewStyle = NewPosts->GetStyle();
		NewStyle.Normal = Brush(Mint, 7.f, 1.f, MintLine);
		NewStyle.Hovered = Brush(Color(210, 239, 225), 7.f, 1.f, Color(140, 206, 175));
		NewStyle.Pressed = Brush(Color(196, 233, 215), 7.f, 1.f, Green);
		NewStyle.NormalPadding = NewStyle.PressedPadding = FMargin(8.f, 0.f);
		NewPosts->SetStyle(NewStyle);
		UTextBlock* NewText = Label(BP, Tree, TEXT("Text_NewPosts"), LOCTEXT("NewPosts", "↑ 새 글 0개 보기"), 13, MintInk, TEXT("SemiBold"));
		NewText->SetJustification(ETextJustify::Center);
		NewPosts->AddChild(NewText);
		Align(*NewText, HAlign_Fill, VAlign_Center);
		NewPosts->SetVisibility(ESlateVisibility::Hidden);
		Row(*Sections, *Sized(BP, Tree, TEXT("SizeBox_NewPosts"), *NewPosts, 0.f, 32.f), FMargin(0.f, 0.f, 0.f, 4.f));

		UOverlay* Feed = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Feed"));
		UScrollBox* Scroll = List(BP, Tree, TEXT("Scroll_Posts"));
		UVerticalBox* Posts = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_Posts"), true);
		UScrollBoxSlot* PostsSlot = CastChecked<UScrollBoxSlot>(Scroll->AddChild(Posts));
		PostsSlot->SetPadding(FMargin(0.f)); PostsSlot->SetHorizontalAlignment(HAlign_Fill);
		Layer(*Feed, *Scroll, HAlign_Fill, VAlign_Fill);
		UTextBlock* Empty = Label(BP, Tree, TEXT("Text_Empty"), LOCTEXT("Empty", "아직 올라온 반응이 없습니다."), 14, Muted);
		Empty->SetAutoWrapText(true); Empty->SetJustification(ETextJustify::Center);
		Layer(*Feed, *Empty, HAlign_Fill, VAlign_Top, FMargin(12.f, 28.f));
		Row(*Sections, *Feed, FMargin(0.f), true);

		Row(*Sections, *Line(BP, Tree, TEXT("Rule_Footer")), FMargin(0.f, 0.f, 0.f, 8.f));
		UTextBlock* Missing = Label(BP, Tree, TEXT("Text_MissingNote"), FText::GetEmpty(), 12, Muted);
		Missing->SetAutoWrapText(true); Missing->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Sections, *Missing, FMargin(0.f, 0.f, 0.f, 4.f));
		UTextBlock* ReadNote = Label(BP, Tree, TEXT("Text_ReadPositionNote"), LOCTEXT("ReadPosition", "새 글이 와도 읽던 위치는 유지돼요."), 12, Hint);
		ReadNote->SetAutoWrapText(true);
		Row(*Sections, *ReadNote);
		return true;
	}

	// ── 음악 ─────────────────────────────────────────────────────────────

	bool BuildTrackRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UVerticalBox* Root = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Root"));
		Tree.RootWidget = Root;

		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Track"));
		// 번호 칸: 번호와 이퀄라이저가 같은 자리에 겹친다.
		UOverlay* IndexCell = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Index"));
		Layer(*IndexCell, *Label(BP, Tree, TEXT("Text_Index"), LOCTEXT("IndexPreview", "01"), 13, Muted, TEXT("Medium")), HAlign_Left, VAlign_Center);
		USizeBox* Equalizer = IconBox(BP, Tree, TEXT("Image_Equalizer"), Icon(TEXT("Actions"), TEXT("Equalizer")), 14.f, Green, true);
		Equalizer->GetContent()->SetVisibility(ESlateVisibility::Hidden);
		Layer(*IndexCell, *Equalizer, HAlign_Left, VAlign_Center, FMargin(1.f, 0.f, 0.f, 0.f));
		USizeBox* IndexSize = Sized(BP, Tree, TEXT("SizeBox_Index"), *IndexCell, 30.f);
		Column(*Content, *IndexSize);
		Align(*IndexSize, HAlign_Left, VAlign_Center);
		UTextBlock* Title = Label(BP, Tree, TEXT("Text_Title"), LOCTEXT("TrackPreview", "축제 음악"), 14, Ink, TEXT("Medium"));
		Title->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Title->SetClipping(EWidgetClipping::ClipToBounds);
		Column(*Content, *Title, FMargin(2.f, 0.f, 8.f, 0.f), true);
		Align(*Title, HAlign_Fill, VAlign_Center);
		// 상태 칸: `재생 중` 과 재생 삼각형이 같은 자리에 겹친다. 폭을 미리 잡아 제목이 흔들리지 않는다.
		UOverlay* StateCell = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_State"));
		UTextBlock* State = Label(BP, Tree, TEXT("Text_State"), LOCTEXT("Playing", "재생 중"), 12, MintInk, TEXT("Medium"));
		State->SetVisibility(ESlateVisibility::Hidden);
		Layer(*StateCell, *State, HAlign_Right, VAlign_Center);
		USizeBox* PlayHint = IconBox(BP, Tree, TEXT("Image_PlayHint"), Icon(TEXT("Actions"), TEXT("PlayFill")), 12.f, Ink, true);
		PlayHint->GetContent()->SetVisibility(ESlateVisibility::Hidden);
		Layer(*StateCell, *PlayHint, HAlign_Right, VAlign_Center, FMargin(0.f, 0.f, 2.f, 0.f));
		USizeBox* StateSize = Sized(BP, Tree, TEXT("SizeBox_State"), *StateCell, 54.f);
		Column(*Content, *StateSize);
		Align(*StateSize, HAlign_Right, VAlign_Center);

		UOverlay* RowLayers = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Row"));
		Layer(*RowLayers, *Content, HAlign_Fill, VAlign_Fill, FMargin(14.f, 0.f, 12.f, 0.f));
		UImage* Accent = Fill(BP, Tree, TEXT("Image_Accent"), Green, 1.5f);
		Accent->SetVisibility(ESlateVisibility::Hidden);
		Layer(*RowLayers, *Sized(BP, Tree, TEXT("SizeBox_Accent"), *Accent, 3.f), HAlign_Left, VAlign_Fill);
		UBorder* Background = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Row"), true);
		// 런타임은 색만 바꾼다(평소 투명 · 호버 하늘 · 지금 곡 민트). 둥글기는 여기서 고친다.
		Background->SetBrush(Brush(White, 5.f));
		Background->SetBrushColor(FLinearColor::Transparent);
		Background->SetPadding(FMargin(0.f));
		Background->SetHorizontalAlignment(HAlign_Fill); Background->SetVerticalAlignment(VAlign_Fill);
		Background->AddChild(RowLayers);

		UButton* Track = MakeWidget<UButton>(BP, Tree, TEXT("Button_Track"), true);
		FButtonStyle Style = Track->GetStyle();
		Style.Normal = Style.Hovered = Style.Pressed = Style.Disabled = FSlateNoResource();
		Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
		Track->SetStyle(Style);
		Track->AddChild(Background);
		Align(*Background, HAlign_Fill, VAlign_Fill);
		Row(*Root, *Sized(BP, Tree, TEXT("SizeBox_Track"), *Track, 0.f, 42.f));
		Row(*Root, *Line(BP, Tree, TEXT("Image_Divider"), true), FMargin(12.f, 1.f, 6.f, 1.f));
		return true;
	}

	/** 아이콘 하나와 아래 작은 점을 가진 토글 버튼(셔플·반복). 점 자리는 늘 남는다. */
	USizeBox* ToggleButton(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* ButtonName, const TCHAR* DotName, UWidget& IconContent)
	{
		UVerticalBox* Stack = MakeWidget<UVerticalBox>(BP, Tree, FName(FString(TEXT("VerticalBox_")) + FString(ButtonName).RightChop(7)));
		Row(*Stack, IconContent, FMargin(0.f, 0.f, 0.f, 3.f));
		Align(IconContent, HAlign_Center, VAlign_Top);
		UImage* Dot = Fill(BP, Tree, DotName, Green, 2.5f);
		Dot->SetVisibility(ESlateVisibility::Hidden);
		USizeBox* DotSize = Sized(BP, Tree, FName(FString(DotName) + TEXT("_Size")), *Dot, 5.f, 5.f);
		Row(*Stack, *DotSize);
		Align(*DotSize, HAlign_Center, VAlign_Top);
		UButton* Toggle = QuietButton(BP, Tree, ButtonName, *Stack, 8.f);
		// 아이콘 중심을 옆 이전·다음 아이콘과 같은 높이에 맞춘다(점은 그 아래).
		CastChecked<UButtonSlot>(Stack->Slot)->SetPadding(FMargin(0.f, 11.f, 0.f, 0.f));
		Align(*Stack, HAlign_Center, VAlign_Top);
		return Sized(BP, Tree, FName(FString(ButtonName) + TEXT("_Size")), *Toggle, 42.f, 44.f);
	}

	USizeBox* TransportButton(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& IconContent)
	{
		UButton* Result = QuietButton(BP, Tree, Name, IconContent, 21.f);
		Align(IconContent, HAlign_Center, VAlign_Center);
		return Sized(BP, Tree, FName(FString(Name) + TEXT("_Size")), *Result, 42.f, 42.f);
	}

	bool BuildMusic(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UBorder* Root = PanelRoot(BP, Tree, TEXT("Panel_Player"), true);
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Sections"));
		Root->AddChild(Sections);

		UTexture2D* Note = Icon(TEXT("Navigation"), TEXT("Music"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Header"));
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Heading"));
		UHorizontalBox* TitleRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Title"));
		USizeBox* TitleIcon = IconBox(BP, Tree, TEXT("Image_TitleIcon"), Note, 22.f, Ink);
		Column(*TitleRow, *TitleIcon, FMargin(0.f, 0.f, 8.f, 0.f));
		Align(*TitleIcon, HAlign_Left, VAlign_Center);
		UTextBlock* Title = Label(BP, Tree, TEXT("Text_PanelTitle"), LOCTEXT("MusicTitle", "음악"), 21, Ink, TEXT("SemiBold"));
		Column(*TitleRow, *Title, FMargin(0.f), true);
		Align(*Title, HAlign_Fill, VAlign_Center);
		Row(*Heading, *TitleRow);
		Row(*Heading, *Label(BP, Tree, TEXT("Text_Subtitle"), LOCTEXT("MusicSubtitle", "축제 플레이리스트"), 13, Muted), FMargin(0.f, 1.f, 0.f, 0.f));
		Column(*Header, *Heading, FMargin(0.f, 0.f, 8.f, 0.f), true);
		Align(*Heading, HAlign_Fill, VAlign_Center);
		USizeBox* Close = CloseButton(BP, Tree, LOCTEXT("MusicClose", "음악 패널 닫기"));
		Column(*Header, *Close);
		Align(*Close, HAlign_Right, VAlign_Center);
		Row(*Sections, *Header, FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Sections, *Line(BP, Tree, TEXT("Border_HeaderRule")), FMargin(0.f, 0.f, 0.f, 14.f));

		// 지금 곡: 작은 음표 타일 + 상태·제목(두 줄까지)·곡 번호.
		UHorizontalBox* Current = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Current"));
		UBorder* Cover = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Cover"));
		Cover->SetBrush(Brush(Color(232, 245, 251), 8.f, 1.f, Outline));
		Cover->SetHorizontalAlignment(HAlign_Center); Cover->SetVerticalAlignment(VAlign_Center);
		Cover->AddChild(IconBox(BP, Tree, TEXT("Image_CoverIcon"), Note, 34.f, Ink));
		USizeBox* CoverSize = Sized(BP, Tree, TEXT("SizeBox_Cover"), *Cover, 72.f, 72.f);
		Column(*Current, *CoverSize, FMargin(0.f, 0.f, 14.f, 0.f));
		Align(*CoverSize, HAlign_Left, VAlign_Center);
		UVerticalBox* Info = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_CurrentInfo"));
		UBorder* Chip = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PlaybackState"));
		Chip->SetBrush(Brush(Mint, 4.f)); Chip->SetPadding(FMargin(7.f, 2.f));
		Chip->AddChild(Label(BP, Tree, TEXT("Text_PlaybackState"), LOCTEXT("PlayingState", "지금 재생 중"), 12, MintInk, TEXT("Medium")));
		Row(*Info, *Chip);
		Align(*Chip, HAlign_Left, VAlign_Top);
		UTextBlock* TrackTitle = Label(BP, Tree, TEXT("Text_TrackTitle"), LOCTEXT("TrackTitlePreview", "축제 음악"), 17, Ink, TEXT("SemiBold"));
		TrackTitle->SetAutoWrapText(true);
		TrackTitle->SetTextOverflowPolicy(ETextOverflowPolicy::MultilineEllipsis);
		TrackTitle->SetClipping(EWidgetClipping::ClipToBounds);
		TrackTitle->SetVisibility(ESlateVisibility::Visible); // 잘린 제목은 툴팁으로 본다.
		USizeBox* TitleSize = Sized(BP, Tree, TEXT("SizeBox_TrackTitle"), *TrackTitle);
		TitleSize->SetMaxDesiredHeight(48.f); // 두 줄까지
		Row(*Info, *TitleSize, FMargin(0.f, 5.f, 0.f, 3.f));
		Row(*Info, *Label(BP, Tree, TEXT("Text_TrackIndex"), LOCTEXT("CountPreview", "1 / 8"), 12, Muted));
		Column(*Current, *Info, FMargin(0.f), true);
		Align(*Info, HAlign_Fill, VAlign_Center);
		Row(*Sections, *Current, FMargin(0.f, 0.f, 0.f, 14.f));

		UProgressBar* Progress = MakeWidget<UProgressBar>(BP, Tree, TEXT("ProgressBar_Track"), true);
		FProgressBarStyle ProgressStyle;
		ProgressStyle.BackgroundImage = Brush(ProgressTrack, 2.f);
		ProgressStyle.FillImage = Brush(White, 2.f);
		ProgressStyle.MarqueeImage = ProgressStyle.FillImage;
		Progress->SetWidgetStyle(ProgressStyle); Progress->SetFillColorAndOpacity(Green); Progress->SetPercent(.25f);
		Progress->SetVisibility(ESlateVisibility::Visible);
		Progress->SetToolTipText(LOCTEXT("ProgressTip", "현재 재생 위치"));
		Row(*Sections, *Sized(BP, Tree, TEXT("SizeBox_Progress"), *Progress, 0.f, 4.f));
		UHorizontalBox* Clocks = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Clocks"));
		Column(*Clocks, *Label(BP, Tree, TEXT("Text_Elapsed"), LOCTEXT("Elapsed", "0:00"), 12, Muted), FMargin(0.f), true);
		Column(*Clocks, *Label(BP, Tree, TEXT("Text_Duration"), LOCTEXT("Duration", "—"), 12, Muted));
		Row(*Sections, *Clocks, FMargin(0.f, 6.f, 0.f, 6.f));

		// 재생 조작 한 줄: 셔플 / 이전 / 재생·일시정지 / 다음 / 반복. 다섯 칸을 균등하게 나눈다.
		UHorizontalBox* Controls = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Controls"));
		USizeBox* Shuffle = ToggleButton(BP, Tree, TEXT("Button_Shuffle"), TEXT("Dot_Shuffle"),
			*IconBox(BP, Tree, TEXT("Image_Shuffle"), Icon(TEXT("Actions"), TEXT("Shuffle")), 22.f, Hint, true));
		USizeBox* Previous = TransportButton(BP, Tree, TEXT("Button_Prev"),
			*IconBox(BP, Tree, TEXT("Image_Previous"), Icon(TEXT("Actions"), TEXT("PreviousFill")), 20.f, Ink));
		UOverlay* PlayIcons = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_PlayIcons"));
		Layer(*PlayIcons, *IconBox(BP, Tree, TEXT("Image_Play"), Icon(TEXT("Actions"), TEXT("PlayFill")), 22.f, White, true), HAlign_Center, VAlign_Center, FMargin(2.f, 0.f, 0.f, 0.f));
		// FindWidget 은 루트에 붙은 위젯만 찾는다. 아직 붙이기 전이므로 만든 포인터를 그대로 쓴다.
		USizeBox* PauseBox = IconBox(BP, Tree, TEXT("Image_Pause"), Icon(TEXT("Actions"), TEXT("PauseFill")), 20.f, White, true);
		PauseBox->GetContent()->SetVisibility(ESlateVisibility::Collapsed);
		Layer(*PlayIcons, *PauseBox, HAlign_Center, VAlign_Center);
		UButton* Play = MakeWidget<UButton>(BP, Tree, TEXT("Button_PlayPause"), true);
		FButtonStyle PlayStyle = Play->GetStyle();
		PlayStyle.Normal = Brush(Green, 26.f);
		PlayStyle.Hovered = Brush(Color(21, 122, 80), 26.f);
		PlayStyle.Pressed = Brush(Ink, 26.f);
		PlayStyle.Disabled = Brush(Color(160, 186, 176), 26.f);
		PlayStyle.NormalPadding = PlayStyle.PressedPadding = FMargin(0.f);
		Play->SetStyle(PlayStyle);
		Play->SetToolTipText(LOCTEXT("PlayTip", "재생"));
		Play->AddChild(PlayIcons);
		Align(*PlayIcons, HAlign_Center, VAlign_Center);
		USizeBox* PlaySize = Sized(BP, Tree, TEXT("Button_PlayPause_Size"), *Play, 52.f, 52.f);
		USizeBox* Next = TransportButton(BP, Tree, TEXT("Button_Next"),
			*IconBox(BP, Tree, TEXT("Image_Next"), Icon(TEXT("Actions"), TEXT("NextFill")), 20.f, Ink));
		UOverlay* RepeatIcons = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_RepeatIcons"));
		Layer(*RepeatIcons, *IconBox(BP, Tree, TEXT("Image_Repeat"), Icon(TEXT("Actions"), TEXT("Repeat")), 22.f, Hint, true), HAlign_Center, VAlign_Center);
		USizeBox* RepeatOne = IconBox(BP, Tree, TEXT("Image_RepeatOne"), Icon(TEXT("Actions"), TEXT("RepeatOne")), 22.f, Green, true);
		RepeatOne->GetContent()->SetVisibility(ESlateVisibility::Collapsed);
		Layer(*RepeatIcons, *RepeatOne, HAlign_Center, VAlign_Center);
		USizeBox* Repeat = ToggleButton(BP, Tree, TEXT("Button_AutoAdvance"), TEXT("Dot_Repeat"), *RepeatIcons);
		Previous->GetContent()->SetToolTipText(LOCTEXT("PrevTip", "이전 곡"));
		Next->GetContent()->SetToolTipText(LOCTEXT("NextTip", "다음 곡"));
		for (USizeBox* Control : { Shuffle, Previous, PlaySize, Next, Repeat })
		{
			Column(*Controls, *Control, FMargin(0.f), true);
			Align(*Control, HAlign_Center, VAlign_Center);
		}
		Row(*Sections, *Controls);
		UTextBlock* ControlHint = Label(BP, Tree, TEXT("Text_ControlHint"), FText::GetEmpty(), 12, Muted);
		ControlHint->SetAutoWrapText(true); ControlHint->SetJustification(ETextJustify::Center);
		ControlHint->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Sections, *ControlHint, FMargin(0.f, 4.f, 0.f, 0.f));

		Row(*Sections, *Line(BP, Tree, TEXT("Border_ListRule")), FMargin(0.f, 12.f, 0.f, 12.f));
		UHorizontalBox* ListHeader = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_ListHeader"));
		UTextBlock* ListTitle = Label(BP, Tree, TEXT("Text_ListTitle"), LOCTEXT("Playlist", "재생 목록"), 15, Ink, TEXT("SemiBold"));
		Column(*ListHeader, *ListTitle, FMargin(0.f), true);
		Align(*ListTitle, HAlign_Fill, VAlign_Center);
		UTextBlock* ListCount = Label(BP, Tree, TEXT("Text_ListCount"), LOCTEXT("ListCountPreview", "0곡"), 12, Muted);
		Column(*ListHeader, *ListCount);
		Align(*ListCount, HAlign_Right, VAlign_Center);
		Row(*Sections, *ListHeader, FMargin(0.f, 0.f, 0.f, 6.f));
		UVerticalBox* ListPanel = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Panel_List"), true);
		Row(*ListPanel, *List(BP, Tree, TEXT("ScrollBox_Tracks")), FMargin(0.f), true);
		Row(*Sections, *ListPanel, FMargin(0.f), true);
		return true;
	}

	// ── 적용 ─────────────────────────────────────────────────────────────

	bool Backup(const FString& Package)
	{
		const FString File = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
		if (!IFileManager::Get().FileExists(*File)) { return true; }
		const FString Folder = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveFlyoutRedesign") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
		const bool bOk = IFileManager::Get().MakeDirectory(*Folder, true)
			&& IFileManager::Get().Copy(*(Folder / FPaths::GetCleanFilename(File)), *File) == COPY_OK;
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BACKUP: %s → %s (%s)"), LogPrefix, *Package, *Folder, bOk ? TEXT("ok") : TEXT("FAIL"));
		return bOk;
	}

	/** 이미 새 구조면 손대지 않는다. 그래야 다시 실행해도 Designer 편집이 남는다. */
	UWidgetBlueprint* Redesign(const FString& Package, const FString& Folder, UClass& Parent, const TCHAR* Marker,
		FFUIWbpBuilder::FBuildLayout&& Build, bool& bOutChanged)
	{
		bOutChanged = false;
		if (UWidgetBlueprint* Existing = LoadExistingBlueprint(Package))
		{
			if (Existing->WidgetTree && Existing->WidgetTree->FindWidget(Marker))
			{
				UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: %s 는 이미 새 시안이다. Designer 편집 보존"), LogPrefix, *Package);
				return Existing;
			}
			if (Existing->GetOutermost()->IsDirty())
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 저장하지 않은 편집이 있어 손대지 않았다: %s"), LogPrefix, *Package);
				return nullptr;
			}
			if (!Backup(Package)) { return nullptr; }
		}
		bOutChanged = true;
		return ReplaceWidgetBlueprint(LogPrefix, Package, Folder, Parent, MoveTemp(Build));
	}

	/**
	 * 바인딩·컴파일 확인. 이름마다 위젯이 있고 C++ 속성 형식과 맞는지, 블루프린트가 오류 없이 컴파일됐는지 본다.
	 * 결과는 `..._VERIFY` 줄로 남긴다(실패면 Error).
	 */
	bool Verify(UWidgetBlueprint& BP, std::initializer_list<const TCHAR*> Names)
	{
		bool bOk = BP.Status == BS_UpToDate || BP.Status == BS_UpToDateWithWarnings;
		UClass* Native = BP.ParentClass;
		for (TFieldIterator<FObjectProperty> It(Native); It; ++It)
		{
			if (!It->HasMetaData(TEXT("BindWidget")) && !It->HasMetaData(TEXT("BindWidgetOptional"))) { continue; }
			const UWidget* Found = BP.WidgetTree ? BP.WidgetTree->FindWidget(It->GetFName()) : nullptr;
			if (Found && !Found->IsA(It->PropertyClass))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_VERIFY_TYPE: %s.%s 는 %s 여야 한다(%s)"), LogPrefix, *BP.GetName(),
					*It->GetName(), *It->PropertyClass->GetName(), *Found->GetClass()->GetName());
				bOk = false;
			}
			if (!Found && It->HasMetaData(TEXT("BindWidget")))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_VERIFY_MISSING_REQUIRED: %s.%s"), LogPrefix, *BP.GetName(), *It->GetName());
				bOk = false;
			}
		}
		for (const TCHAR* Name : Names)
		{
			const UWidget* Found = BP.WidgetTree ? BP.WidgetTree->FindWidget(Name) : nullptr;
			if (!Found || !Found->bIsVariable)
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_VERIFY_MISSING: %s.%s (%s)"), LogPrefix, *BP.GetName(), Name,
					Found ? TEXT("변수 아님") : TEXT("없음"));
				bOk = false;
			}
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_VERIFY %s: %s (status=%d, 확인 이름 %d개)"), LogPrefix, *BP.GetName(),
			bOk ? TEXT("OK") : TEXT("FAIL"), static_cast<int32>(BP.Status.GetValue()), static_cast<int32>(Names.size()));
		return bOk;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), LogPrefix); return; }
		for (const TCHAR* Needed : { TEXT("Shuffle"), TEXT("Repeat"), TEXT("RepeatOne"), TEXT("Equalizer"), TEXT("PlayFill"), TEXT("PauseFill"), TEXT("NextFill"), TEXT("PreviousFill") })
		{
			if (!Icon(TEXT("Actions"), Needed)) { return; }
		}

		bool bChanged = false;
		UWidgetBlueprint* SnsRow = Redesign(SnsRowPackage, SnsFolder, *UFF_LiveSnsPostEntry::StaticClass(), TEXT("Image_Avatar"), BuildSnsRow, bChanged);
		if (!SnsRow || !SnsRow->GeneratedClass) { return; }
		if (bChanged)
		{
			const TMap<EFF_LiveSnsSentiment, UTexture2D*> Moods = {
				{ EFF_LiveSnsSentiment::Positive, Icon(TEXT("Social"), TEXT("SentimentPositive")) },
				{ EFF_LiveSnsSentiment::Negative, Icon(TEXT("Social"), TEXT("SentimentNegative")) },
				{ EFF_LiveSnsSentiment::Neutral, Icon(TEXT("Social"), TEXT("SentimentNeutral")) } };
			CastChecked<UFF_LiveSnsPostEntry>(SnsRow->GeneratedClass->GetDefaultObject())->ConfigureIconRowDefaults(Moods, Green, Color(122, 136, 132), Muted);
			FKismetEditorUtilities::CompileBlueprint(SnsRow);
			if (!SaveAssetPackage(LogPrefix, *SnsRow)) { return; }
		}

		UWidgetBlueprint* Sns = Redesign(SnsPackage, SnsFolder, *UFF_LivePanelSNS::StaticClass(), TEXT("Underline_FilterAll"), BuildSns, bChanged);
		if (!Sns || !Sns->GeneratedClass) { return; }
		if (bChanged)
		{
			UFF_LivePanelSNS* Defaults = CastChecked<UFF_LivePanelSNS>(Sns->GeneratedClass->GetDefaultObject());
			Defaults->ConfigureFlyoutDefaults(SnsRow->GeneratedClass.Get());
			Defaults->ConfigureTabFilterDefaults(MintInk, Muted);
			FKismetEditorUtilities::CompileBlueprint(Sns);
			if (!SaveAssetPackage(LogPrefix, *Sns)) { return; }
		}

		UWidgetBlueprint* TrackRow = Redesign(TrackRowPackage, AudioFolder, *UFF_PlaylistTrackRowWidget::StaticClass(), TEXT("Border_Row"), BuildTrackRow, bChanged);
		if (!TrackRow || !TrackRow->GeneratedClass) { return; }
		UWidgetBlueprint* Music = Redesign(MusicPackage, AudioFolder, *UFF_PlaylistPlayerWidget::StaticClass(), TEXT("Image_RepeatOne"), BuildMusic, bChanged);
		if (!Music || !Music->GeneratedClass) { return; }
		if (bChanged)
		{
			UFF_PlaylistPlayerWidget* Defaults = CastChecked<UFF_PlaylistPlayerWidget>(Music->GeneratedClass->GetDefaultObject());
			Defaults->ConfigureFlyoutDefaults(TrackRow->GeneratedClass.Get());
			Defaults->ConfigureIconToggleDefaults();
			FKismetEditorUtilities::CompileBlueprint(Music);
			if (!SaveAssetPackage(LogPrefix, *Music)) { return; }
		}
		bool bVerified = Verify(*SnsRow, { TEXT("Text_Author"), TEXT("Text_Body"), TEXT("Text_Sentiment"), TEXT("Image_Sentiment"), TEXT("Border_Card") });
		bVerified &= Verify(*Sns, { TEXT("Text_PanelTitle"), TEXT("Button_Close"), TEXT("Button_FilterAll"), TEXT("Button_FilterPositive"),
				TEXT("Button_FilterNegative"), TEXT("Text_FilterAll"), TEXT("Text_FilterPositive"), TEXT("Text_FilterNegative"),
				TEXT("Underline_FilterAll"), TEXT("Underline_FilterPositive"), TEXT("Underline_FilterNegative"), TEXT("Text_Count"),
				TEXT("Button_NewPosts"), TEXT("Text_NewPosts"), TEXT("Scroll_Posts"), TEXT("Box_Posts"), TEXT("Text_Empty"), TEXT("Text_MissingNote") });
		bVerified &= Verify(*TrackRow, { TEXT("Button_Track"), TEXT("Border_Row"), TEXT("Image_Accent"), TEXT("Text_Index"), TEXT("Image_Equalizer"),
				TEXT("Text_Title"), TEXT("Text_State"), TEXT("Image_PlayHint"), TEXT("Image_Divider") });
		bVerified &= Verify(*Music, { TEXT("Panel_Player"), TEXT("Button_Close"), TEXT("Text_PlaybackState"), TEXT("Text_TrackTitle"), TEXT("Text_TrackIndex"),
				TEXT("ProgressBar_Track"), TEXT("Text_Elapsed"), TEXT("Text_Duration"), TEXT("Button_Shuffle"), TEXT("Image_Shuffle"), TEXT("Dot_Shuffle"),
				TEXT("Button_Prev"), TEXT("Button_PlayPause"), TEXT("Image_Play"), TEXT("Image_Pause"), TEXT("Button_Next"), TEXT("Button_AutoAdvance"),
				TEXT("Image_Repeat"), TEXT("Image_RepeatOne"), TEXT("Dot_Repeat"), TEXT("Text_ControlHint"), TEXT("Text_ListCount"), TEXT("Panel_List"),
				TEXT("ScrollBox_Tracks") });
		// Class Defaults 연결 확인(보호 속성이라 이름으로 읽는다).
		const auto ClassOf = [](UObject* Defaults, const TCHAR* Name) -> UClass*
		{
			const FClassProperty* Property = FindFProperty<FClassProperty>(Defaults->GetClass(), Name);
			return Property ? Cast<UClass>(Property->GetObjectPropertyValue_InContainer(Defaults)) : nullptr;
		};
		const auto FlagOf = [](UObject* Defaults, const TCHAR* Name)
		{
			const FBoolProperty* Property = FindFProperty<FBoolProperty>(Defaults->GetClass(), Name);
			return Property && Property->GetPropertyValue_InContainer(Defaults);
		};
		UObject* SnsDefaults = Sns->GeneratedClass->GetDefaultObject();
		UObject* MusicDefaults = Music->GeneratedClass->GetDefaultObject();
		const bool bDefaultsOk = ClassOf(SnsDefaults, TEXT("PostEntryClass")) == SnsRow->GeneratedClass
			&& !FlagOf(SnsDefaults, TEXT("bTintFilterBackground")) && FlagOf(SnsDefaults, TEXT("bPreserveFeedWhileClosed"))
			&& ClassOf(MusicDefaults, TEXT("TrackRowWidgetClass")) == TrackRow->GeneratedClass
			&& FlagOf(MusicDefaults, TEXT("bHostedByHud")) && !FlagOf(MusicDefaults, TEXT("bShowToggleStateByButtonStyle"));
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_VERIFY_DEFAULTS: %s"), LogPrefix, bDefaultsOk ? TEXT("OK") : TEXT("FAIL"));
		if (!bVerified || !bDefaultsOk) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 바인딩·컴파일·기본값 확인 실패"), LogPrefix); return; }
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: SNS 탭·구분선 행, 음악 조작 한 줄·곡 행 상태. 도크 버튼·하단 HUD·기존 SNS 패널은 그대로"), LogPrefix);
	}

	/**
	 * SNS 새 글 자동 표시(2026-10-07 사용자 결정)에 맞춘 작은 정리. 트리를 갈지 않고 필요한 곳만 바꾼다.
	 * - 글 행: 이름 바로 뒤에 빨간 점 `Image_NewDot`. 이름 칸은 글자 폭만큼, 남는 폭은 빈칸이 채운다.
	 * - 패널: `새 글 N개 보기` 버튼을 기본 접힘으로 두고 아래 여백을 없앤다(런타임도 자동 표시면 접는다).
	 * 이미 점이 있으면 손대지 않는다.
	 */
	void ApplyNewDot()
	{
		if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_NEWDOT_FAIL: PIE 종료 후 실행하세요."), LogPrefix); return; }
		UWidgetBlueprint* Row = LoadExistingBlueprint(SnsRowPackage);
		UWidgetBlueprint* Panel = LoadExistingBlueprint(SnsPackage);
		if (!Row || !Row->WidgetTree || !Panel || !Panel->WidgetTree)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_NEWDOT_FAIL: SNS 글 행·패널이 없다. 먼저 FF.ApplyLiveFlyoutRedesign"), LogPrefix); return;
		}
		if (Row->WidgetTree->FindWidget(TEXT("Image_NewDot")))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_NEWDOT_REUSE: 이미 적용됨. Designer 편집 보존"), LogPrefix);
		}
		else
		{
			UHorizontalBox* Meta = Cast<UHorizontalBox>(Row->WidgetTree->FindWidget(TEXT("HorizontalBox_Meta")));
			UTextBlock* Author = Cast<UTextBlock>(Row->WidgetTree->FindWidget(TEXT("Text_Author")));
			if (!Meta || !Author || Author->GetParent() != Meta || Row->GetOutermost()->IsDirty() || Panel->GetOutermost()->IsDirty())
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_NEWDOT_FAIL: 글 행 구조가 예상과 다르거나 저장 안 한 편집이 있다."), LogPrefix); return;
			}
			if (!Backup(SnsRowPackage)) { return; }
			Row->Modify(); Row->WidgetTree->Modify(); Meta->Modify(); Author->Modify();
			const int32 AuthorIndex = Meta->GetChildIndex(Author);
			UHorizontalBoxSlot* AuthorSlot = CastChecked<UHorizontalBoxSlot>(Author->Slot);
			AuthorSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			AuthorSlot->SetPadding(FMargin(0.f));
			UImage* Dot = Fill(*Row, *Row->WidgetTree, TEXT("Image_NewDot"), Color(226, 74, 60), 3.5f);
			USizeBox* DotSize = Sized(*Row, *Row->WidgetTree, TEXT("SizeBox_NewDot"), *Dot, 7.f, 7.f);
			UHorizontalBoxSlot* DotSlot = CastChecked<UHorizontalBoxSlot>(Meta->InsertChildAt(AuthorIndex + 1, DotSize));
			DotSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			DotSlot->SetHorizontalAlignment(HAlign_Left); DotSlot->SetVerticalAlignment(VAlign_Center);
			USpacer* Gap = MakeWidget<USpacer>(*Row, *Row->WidgetTree, TEXT("Spacer_Meta"));
			UHorizontalBoxSlot* GapSlot = CastChecked<UHorizontalBoxSlot>(Meta->InsertChildAt(AuthorIndex + 2, Gap));
			GapSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			GapSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			if (!SaveNewBlueprint(LogPrefix, *Row)) { return; }

			UButton* NewPosts = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("Button_NewPosts")));
			USizeBox* NewPostsSize = Cast<USizeBox>(Panel->WidgetTree->FindWidget(TEXT("SizeBox_NewPosts")));
			if (NewPosts && NewPostsSize)
			{
				if (!Backup(SnsPackage)) { return; }
				Panel->Modify(); NewPosts->Modify(); NewPostsSize->Modify();
				NewPosts->SetVisibility(ESlateVisibility::Collapsed);
				if (UVerticalBoxSlot* SizeSlot = Cast<UVerticalBoxSlot>(NewPostsSize->Slot)) { SizeSlot->SetPadding(FMargin(0.f)); }
				if (!SaveNewBlueprint(LogPrefix, *Panel)) { return; }
			}
		}
		bool bVerified = Verify(*Row, { TEXT("Text_Author"), TEXT("Text_Body"), TEXT("Text_Sentiment"), TEXT("Image_Sentiment"), TEXT("Border_Card"), TEXT("Image_NewDot") });
		bVerified &= Verify(*Panel, { TEXT("Button_NewPosts"), TEXT("Scroll_Posts"), TEXT("Box_Posts"), TEXT("Text_Count") });
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_NEWDOT_%s: 이름 옆 새 글 점, 새 글 보기 버튼 접음"), LogPrefix, bVerified ? TEXT("APPLIED") : TEXT("VERIFY_FAIL"));
	}

	FAutoConsoleCommand NewDotCommand(TEXT("FF.ApplyLiveSnsNewDot"),
		TEXT("SNS 글 행에 새 글 빨간 점을 넣고 새 글 보기 버튼을 접는다. 이미 있으면 건너뛴다."),
		FConsoleCommandDelegate::CreateStatic(&ApplyNewDot));

	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveFlyoutRedesign"),
		TEXT("SNS·음악 펼침 패널을 2026-10-07 승인안으로 다시 짠다. 원본을 백업하고, 이미 새 구조면 건너뛴다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
