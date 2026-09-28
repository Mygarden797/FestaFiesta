// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_UICommonWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 공용 위젯 3종의 WBP 골격을 만드는 에디터 전용 도구다. 콘솔 명령은 `FF.CommonUMG`.
//
// === 이 파일의 계약 ===
// **없는 WBP만 만든다.** 이미 있으면 레이아웃 콜백을 실행하지 않으므로 디자이너에서 손댄
// 계층·색·패딩이 그대로 보존된다. 덮어쓰는 명령은 만들지 않는다.
//
// === 1회용 스캐폴딩이다 ===
// 하는 일은 `계층 · 위젯 이름 · BindWidget 대상 · 틀리기 쉬운 값`까지다.
// **한 번 생성한 뒤 이 파일은 레이아웃의 원본이 아니다.** 여기 적힌 크기는 대략치이고
// 에디터에서 보면서 조절한다. 조절한 값을 여기 되돌려 적지 않는다 — 그러면 손으로 만진 값과
// 갈라져 어느 쪽이 진짜인지 모르는 죽은 문서가 된다.
// Report 는 목업 `manifest.json` 이 좌표 원천이라 예외였고 여기는 그게 없다.
// 근거와 폐기 전례: docs/memory/wbp-codegen-workflow.md
//
// === 색을 여기서도 넣는 이유 ===
// 색의 실제 주인은 각 위젯의 `NativePreConstruct` 다. 여기서 같은 값을 한 번 더 넣는 것은
// 에디터에서 에셋을 열었을 때 바로 보이게 하려는 것뿐이고, **양쪽이 같은 `UFF_UIPalette` 를
// 읽으므로 갈라질 수 없다.** 팔레트를 고치면 둘 다 따라온다.
// 디자이너에서 이 색들을 손으로 덮지 않는다 — 그때부터 제어 경로가 둘이 된다.
//
// === 이미지를 넣지 않는다 ===
// 네모 박스와 브러시 컬러로 모양만 잡는다. 9-slice·Draw As 같은 브러시 세부는
// 실제 텍스처가 들어올 때 정한다. 아이콘 자리는 빈 UImage 로 둔다.

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Widgets/FF_PanelFrame.h"
#include "UI/Widgets/FF_StatCard.h"
#include "UI/Widgets/FF_StateChip.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace FFUICommonWbpBuilder
{
	using namespace FFUIWbpBuilder;

	/** 로그 접두어. 카테고리는 `LogFFUIWbpBuilder` 를 공유한다. */
	const TCHAR* const CommonLogPrefix = TEXT("FF_COMMON_UMG");

	// 생성 대상 경로. 런타임 soft reference·자동화·이 생성기 세 곳이 같은 문자열을 계약으로 쓰므로
	// 이름을 바꾸면 세 곳을 함께 고쳐야 한다.
	// WBP_FF_MetricRow 가 이미 있는 폴더다. 비파괴라 그것은 건드리지 않는다.
	const FString CommonFolder(TEXT("/Game/FestaFiesta/UI/Common"));
	const FString PanelFramePackage(CommonFolder / TEXT("WBP_FF_PanelFrame"));
	const FString StatCardPackage(CommonFolder / TEXT("WBP_FF_StatCard"));
	const FString StateChipPackage(CommonFolder / TEXT("WBP_FF_StateChip"));

	// === 대략 크기 ===
	// 에디터에서 조절할 출발점이다. 픽셀 단위로 맞추지 않는다.
	constexpr float PanelWidth = 420.f;
	constexpr float PanelHeight = 560.f;
	constexpr float HeaderHeight = 44.f;
	constexpr float CloseButtonSize = 32.f;
	constexpr float CardWidth = 240.f;
	constexpr float CardHeight = 132.f;
	constexpr float ChipHeight = 36.f;

	/**
	 * 아이콘 표시 크기. **이 값을 브러시 Image Size 에 반드시 넣는다.**
	 * 텍스처 기본값(2048x512)으로 두면 칩 전체가 부풀어 오른다.
	 * MetricRow 의 구간 아이콘과 같은 24 로 맞춘다.
	 */
	constexpr float IconSize = 24.f;

	/** 브러시의 표시 크기를 박는다. 아이콘을 나중에 꽂아도 크기가 튀지 않는다. */
	void LockImageSize(UImage& Image, const float Size)
	{
		FSlateBrush Brush = Image.GetBrush();
		Brush.SetImageSize(FVector2D(Size, Size));
		Image.SetBrush(Brush);
	}

	// =========================================================================
	// WBP_FF_PanelFrame — 머리(색 바 + 제목 + 닫기) + 본문 스크롤 + 발
	// =========================================================================
	bool BuildPanelFrame(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없어 WBP_FF_PanelFrame 을 만들지 않았다."), CommonLogPrefix);
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UBorder* Frame = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Frame"));
		UVerticalBox* Column = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Root"));
		USizeBox* HeaderBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Header"));
		UHorizontalBox* HeaderRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		USizeBox* CloseBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Close"));
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));

		// bIsVariable = true 인 것만 BindWidget 대상이다.
		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"), true);
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Title"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UBorder* Body = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Body"), true);
		UScrollBox* BodyScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Body"), true);
		UHorizontalBox* Footer = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_Footer"), true);

		if (!Root || !Frame || !Column || !HeaderBox || !HeaderRow || !CloseBox || !CloseGlyph
			|| !Header || !Title || !Close || !Body || !BodyScroll || !Footer)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Root->SetWidthOverride(PanelWidth);
		Root->SetHeightOverride(PanelHeight);
		Root->AddChild(Frame);

		// 틀은 선 색으로 둔다. 패널 바깥 경계다.
		Frame->SetBrushColor(Palette->InGame.Line);
		Frame->SetPadding(FMargin(1.f));
		Frame->AddChild(Column);

		// --- 머리 ---
		// 높이는 SizeBox 로 고정한다. SetDesiredSizeInViewport 는 뷰포트 직접 부착 위젯 전용이라
		// 컨테이너 자식에는 효과가 없다.
		HeaderBox->SetHeightOverride(HeaderHeight);
		HeaderBox->AddChild(Header);
		if (UVerticalBoxSlot* HeaderSlot = Column->AddChildToVerticalBox(HeaderBox))
		{
			HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		Header->SetBrushColor(Palette->InGame.Teal);
		Header->SetPadding(FMargin(14.f, 0.f, 8.f, 0.f));
		Header->AddChild(HeaderRow);

		StyleText(*Title, 20, Palette->InGame.White, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("PanelFrameTitlePlaceholder", "패널 제목"));
		if (UHorizontalBoxSlot* TitleSlot = HeaderRow->AddChildToHorizontalBox(Title))
		{
			TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TitleSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 닫기 글리프는 임시다. 아이콘이 생기면 UImage 로 교체한다.
		StyleText(*CloseGlyph, 20, Palette->InGame.White, TEXT("SemiBold"));
		CloseGlyph->SetText(LOCTEXT("PanelFrameCloseGlyph", "X"));
		Close->AddChild(CloseGlyph);

		CloseBox->SetWidthOverride(CloseButtonSize);
		CloseBox->SetHeightOverride(CloseButtonSize);
		CloseBox->AddChild(Close);
		if (UHorizontalBoxSlot* CloseSlot = HeaderRow->AddChildToHorizontalBox(CloseBox))
		{
			CloseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CloseSlot->SetVerticalAlignment(VAlign_Center);
		}

		// --- 본문 ---
		Body->SetBrushColor(Palette->InGame.Paper);
		Body->SetPadding(FMargin(14.f, 12.f, 14.f, 12.f));
		Body->AddChild(BodyScroll);
		if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(Body))
		{
			BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		// --- 발 ---
		// CTA 가 없는 패널도 있어 기본은 숨김이다. UFF_PanelFrame::SetFooterVisible 로 켠다.
		// 높이를 고정하지 않는다 — 붙는 버튼 크기에 맞춰야 하고, 고정하면 빈 발에도 자리가 남는다.
		// Hidden 이 아니라 Collapsed 다. Hidden 은 자리를 남긴다.
		Footer->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Footer))
		{
			FooterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			FooterSlot->SetPadding(FMargin(14.f, 0.f, 14.f, 12.f));
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_StatCard — 라벨 + 큰 수치 + 보조문구
	// =========================================================================
	bool BuildStatCard(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없어 WBP_FF_StatCard 을 만들지 않았다."), CommonLogPrefix);
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UVerticalBox* Column = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Root"));
		UHorizontalBox* ValueRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Value"));

		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Card"), true);
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* Value = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Value"), true);
		UTextBlock* Caption = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Caption"), true);
		UTextBlock* Provisional = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Provisional"), true);

		if (!Root || !Column || !ValueRow || !Card || !Label || !Value || !Caption || !Provisional)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Root->SetWidthOverride(CardWidth);
		Root->SetHeightOverride(CardHeight);
		Root->AddChild(Card);

		Card->SetBrushColor(Palette->InGame.Paper);
		Card->SetPadding(FMargin(16.f, 14.f, 16.f, 14.f));
		Card->AddChild(Column);

		StyleText(*Label, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		Label->SetText(LOCTEXT("StatCardLabelPlaceholder", "지표 이름"));
		if (UVerticalBoxSlot* LabelSlot = Column->AddChildToVerticalBox(Label))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		// 수치와 `잠정` 배지를 한 줄에 둔다. 배지가 수치 옆에 붙어야 무엇이 잠정인지 드러난다.
		if (UVerticalBoxSlot* ValueRowSlot = Column->AddChildToVerticalBox(ValueRow))
		{
			ValueRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ValueRowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		StyleText(*Value, 32, Palette->InGame.Ink, TEXT("SemiBold"));
		Value->SetText(LOCTEXT("StatCardValuePlaceholder", "0"));
		if (UHorizontalBoxSlot* ValueSlot = ValueRow->AddChildToHorizontalBox(Value))
		{
			ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 문구와 기본 숨김은 UFF_StatCard::NativePreConstruct 가 넣는다. 여기서는 자리만 만든다.
		StyleText(*Provisional, 11, Palette->InGame.Amber, TEXT("Medium"));
		if (UHorizontalBoxSlot* ProvisionalSlot = ValueRow->AddChildToHorizontalBox(Provisional))
		{
			ProvisionalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ProvisionalSlot->SetVerticalAlignment(VAlign_Center);
			ProvisionalSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}

		StyleText(*Caption, 12, Palette->InGame.InkMuted, TEXT("Medium"));
		Caption->SetText(LOCTEXT("StatCardCaptionPlaceholder", "보조 문구"));
		if (UVerticalBoxSlot* CaptionSlot = Column->AddChildToVerticalBox(Caption))
		{
			CaptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_StateChip — 아이콘 + 문구. 색은 셋 중 하나가 아니라 셋과 함께 쓴다 (AT-011)
	// =========================================================================
	bool BuildStateChip(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없어 WBP_FF_StateChip 을 만들지 않았다."), CommonLogPrefix);
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Root"));
		USizeBox* IconBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Icon"));

		UBorder* Chip = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Chip"), true);
		UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Icon"), true);
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);

		if (!Root || !Row || !IconBox || !Chip || !Icon || !Label)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 높이만 고정하고 폭은 문구에 맞춘다. 상태 문구 길이가 제각각이다.
		Root->SetHeightOverride(ChipHeight);
		Root->AddChild(Chip);

		Chip->SetBrushColor(Palette->InGame.PaperSunken);
		Chip->SetPadding(FMargin(10.f, 0.f, 12.f, 0.f));
		Chip->AddChild(Row);

		// 아이콘 텍스처와 틴트는 런타임에 들어온다 (ApplyBand 또는 SetCustomState).
		// 여기서는 자리와 크기만 잡는다. **크기를 안 박으면 텍스처가 꽂히는 순간 칩이 부푼다.**
		LockImageSize(*Icon, IconSize);
		IconBox->SetWidthOverride(IconSize);
		IconBox->SetHeightOverride(IconSize);
		IconBox->AddChild(Icon);
		if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconBox))
		{
			IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}

		// 색은 런타임에 상태색으로 덮인다. 여기 Ink 는 디자이너에서 보이게 하는 출발값이다.
		StyleText(*Label, 14, Palette->InGame.Ink, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("StateChipLabelPlaceholder", "상태"));
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	void RunCommonUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BEGIN: 없는 WBP만 생성한다."), CommonLogPrefix);

		// 셋은 서로 의존하지 않으므로 순서가 자유롭다.
		// Report 생성기처럼 선행 WBP 의 GeneratedClass 를 자식으로 넣는 관계가 없다.
		EnsureWidgetBlueprint(
			CommonLogPrefix,
			PanelFramePackage,
			CommonFolder,
			*UFF_PanelFrame::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildPanelFrame(Blueprint, Tree); });

		EnsureWidgetBlueprint(
			CommonLogPrefix,
			StatCardPackage,
			CommonFolder,
			*UFF_StatCard::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildStatCard(Blueprint, Tree); });

		EnsureWidgetBlueprint(
			CommonLogPrefix,
			StateChipPackage,
			CommonFolder,
			*UFF_StateChip::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildStateChip(Blueprint, Tree); });

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 끝났다. 크기·패딩·폰트 마감은 디자이너에서 한다."), CommonLogPrefix);
	}

	FAutoConsoleCommand CommonUmgCommand(
		TEXT("FF.CommonUMG"),
		TEXT("공용 위젯 WBP 3종(WBP_FF_PanelFrame / WBP_FF_StatCard / WBP_FF_StateChip)을 만든다. "
			 "이미 있는 에셋은 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunCommonUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
