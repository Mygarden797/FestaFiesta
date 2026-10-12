// SNS 도크 패널 최초 제작. 기존 중앙 패널과 게시물 행 에셋은 수정하지 않는다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveSnsFlyout"

namespace FFLiveSnsFlyoutBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_SNS_FLYOUT");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels");
	const FString Package = Folder / TEXT("WBP_FF_LiveSnsFlyout");
	const FString EntryPackage = Folder / TEXT("WBP_FF_LiveSnsPostEntry");
	const FString IconPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_SNS_White_256");

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Content, int32 Size = 16, FLinearColor Tint = Ink, bool bVariable = false)
	{
		UTextBlock* Label = Text(BP, Tree, Name, Content, Size, Tint, bVariable);
		Label->SetAutoWrapText(true);
		return Label;
	}

	void CenterColumn(UHorizontalBox& Parent, UWidget& Child, FMargin Padding = FMargin(0.f), bool bFill = false)
	{
		Column(Parent, Child, Padding, bFill);
		CastChecked<UHorizontalBoxSlot>(Child.Slot)->SetVerticalAlignment(VAlign_Center);
	}

	bool Build(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UBorder* Root = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PanelRoot"));
		Root->SetBrush(Brush(Paper, 10.f, 1.5f, SkyLine));
		Root->SetPadding(FMargin(18.f, 14.f));
		Root->SetClipping(EWidgetClipping::ClipToBounds);
		Tree.RootWidget = Root;
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Sections"));
		Root->AddChild(Sections);

		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Header"));
		USizeBox* HeaderSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Header"));
		HeaderSize->SetHeightOverride(62.f); HeaderSize->AddChild(Header);
		USizeBox* IconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_TitleIcon"));
		IconSize->SetWidthOverride(28.f); IconSize->SetHeightOverride(28.f);
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_TitleIcon"));
		Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(IconPackage)));
		Icon->SetColorAndOpacity(Ink); IconSize->AddChild(Icon);
		CenterColumn(*Header, *IconSize, FMargin(0.f, 0.f, 12.f, 0.f));
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Heading"));
		Row(*Heading, *Text(BP, Tree, TEXT("Text_PanelTitle"), LOCTEXT("Title", "SNS"), 25, Ink, true));
		Row(*Heading, *Wrapped(BP, Tree, TEXT("Text_Subtitle"), LOCTEXT("Subtitle", "관객들이 전하는 축제 이야기"), 14, Muted), FMargin(0.f, 3.f, 0.f, 0.f));
		CenterColumn(*Header, *Heading, FMargin(0.f, 0.f, 8.f, 0.f), true);
		UButton* Close = Button(BP, Tree, TEXT("Button_Close"), *CloseIcon(BP, Tree));
		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.NormalPadding = CloseStyle.PressedPadding = FMargin(0.f); Close->SetStyle(CloseStyle);
		USizeBox* CloseSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Close"));
		CloseSize->SetWidthOverride(32.f); CloseSize->SetHeightOverride(32.f); CloseSize->AddChild(Close);
		CenterColumn(*Header, *CloseSize);
		Row(*Sections, *HeaderSize, FMargin(0.f, 0.f, 0.f, 8.f));
		Row(*Sections, *Rule(BP, Tree, TEXT("Rule_Header")), FMargin(0.f, 0.f, 0.f, 12.f));

		// 고정 폭 세 개를 쌓지 않고 남은 폭을 균등 분할한다. 작은 도크에서도 우측 빈틈이 없다.
		UHorizontalBox* Filters = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Filters"));
		const TCHAR* Suffixes[] = { TEXT("All"), TEXT("Positive"), TEXT("Negative") };
		const FText Labels[] = { LOCTEXT("All", "전체"), LOCTEXT("Positive", "긍정"), LOCTEXT("Negative", "아쉬움") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Suffixes); ++Index)
		{
			UTextBlock* Label = Wrapped(BP, Tree, *(FString(TEXT("Text_Filter")) + Suffixes[Index]), Labels[Index], 16, Ink, true);
			UButton* Filter = Button(BP, Tree, *(FString(TEXT("Button_Filter")) + Suffixes[Index]), *Label);
			// 가운데 칸의 자동 줄바꿈은 직전 폭에서 줄이 바뀐다. 버튼 폭을 쓰고 글자로 가운데 맞춘다.
			CastChecked<UButtonSlot>(Label->Slot)->SetHorizontalAlignment(HAlign_Fill);
			Label->SetJustification(ETextJustify::Center);
			FButtonStyle FilterStyle = Filter->GetStyle();
			FilterStyle.Normal = Brush(FLinearColor::White, 6.f, 1.f, SkyLine);
			FilterStyle.Hovered = Brush(FLinearColor::White, 6.f, 2.f, Color(100, 177, 210));
			FilterStyle.Pressed = Brush(FLinearColor::White, 6.f, 2.f, Ink);
			FilterStyle.NormalPadding = FilterStyle.PressedPadding = FMargin(8.f, 4.f); Filter->SetStyle(FilterStyle);
			USizeBox* FilterSize = MakeWidget<USizeBox>(BP, Tree, FName(FString(TEXT("SizeBox_Filter")) + Suffixes[Index]));
			FilterSize->SetHeightOverride(38.f); FilterSize->AddChild(Filter);
			Column(*Filters, *FilterSize, FMargin(0.f, 0.f, Index < 2 ? 8.f : 0.f, 0.f), true);
		}
		Row(*Sections, *Filters, FMargin(0.f, 0.f, 0.f, 8.f));
		Row(*Sections, *Wrapped(BP, Tree, TEXT("Text_Count"), LOCTEXT("Count", "최근 0개 · 최신순"), 14, Muted, true), FMargin(0.f, 0.f, 0.f, 8.f));
		USizeBox* NewSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_NewPosts"));
		NewSize->SetHeightOverride(36.f);
		UButton* NewPosts = Button(BP, Tree, TEXT("Button_NewPosts"),
			*Text(BP, Tree, TEXT("Text_NewPosts"), LOCTEXT("NewPosts", "↑ 새 글 보기"), 15, Ink, true));
		NewPosts->SetVisibility(ESlateVisibility::Hidden); NewSize->AddChild(NewPosts);
		Row(*Sections, *NewSize, FMargin(0.f, 0.f, 0.f, 10.f));

		UOverlay* Feed = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Feed"));
		UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_Posts"), true);
		Scroll->SetAllowOverscroll(false); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetScrollbarThickness(FVector2D(4.f, 4.f));
		Scroll->SetScrollbarPadding(FMargin(6.f, 0.f, 0.f, 0.f));
		Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		UVerticalBox* Posts = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_Posts"), true);
		UScrollBoxSlot* PostsSlot = CastChecked<UScrollBoxSlot>(Scroll->AddChild(Posts));
		PostsSlot->SetPadding(FMargin(0.f)); PostsSlot->SetHorizontalAlignment(HAlign_Fill);
		UOverlaySlot* ScrollSlot = Feed->AddChildToOverlay(Scroll);
		ScrollSlot->SetHorizontalAlignment(HAlign_Fill); ScrollSlot->SetVerticalAlignment(VAlign_Fill);
		UTextBlock* Empty = Wrapped(BP, Tree, TEXT("Text_Empty"), LOCTEXT("Empty", "아직 올라온 반응이 없습니다."), 17, Muted, true);
		UOverlaySlot* EmptySlot = Feed->AddChildToOverlay(Empty);
		EmptySlot->SetPadding(FMargin(12.f, 28.f)); EmptySlot->SetHorizontalAlignment(HAlign_Fill); EmptySlot->SetVerticalAlignment(VAlign_Top);
		Empty->SetJustification(ETextJustify::Center); Empty->SetVisibility(ESlateVisibility::HitTestInvisible);
		Row(*Sections, *Feed, FMargin(0.f, 0.f, 0.f, 10.f), true);
		Row(*Sections, *Rule(BP, Tree, TEXT("Rule_Footer")), FMargin(0.f, 0.f, 0.f, 8.f));
		UTextBlock* Missing = Wrapped(BP, Tree, TEXT("Text_MissingNote"), FText::GetEmpty(), 14, Muted, true);
		Missing->SetVisibility(ESlateVisibility::Collapsed); Row(*Sections, *Missing, FMargin(0.f, 0.f, 0.f, 4.f));
		Row(*Sections, *Wrapped(BP, Tree, TEXT("Text_ReadPositionNote"), LOCTEXT("ReadPosition", "새 글이 올라와도 읽던 위치는 유지됩니다."), 13, Muted));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) { return; }
		UClass* EntryClass = LoadGeneratedClass(EntryPackage);
		if (!EntryClass || !EntryClass->IsChildOf(UFF_LiveSnsPostEntry::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 SNS 게시물 행 에셋이 필요합니다."), LogPrefix); return;
		}
		const bool bNewPanel = !LoadExistingBlueprint(Package);
		UWidgetBlueprint* Panel = EnsureWidgetBlueprint(LogPrefix, Package, Folder, *UFF_LivePanelSNS::StaticClass(), Build);
		if (!Panel || !Panel->GeneratedClass) { return; }
		if (bNewPanel)
		{
			CastChecked<UFF_LivePanelSNS>(Panel->GeneratedClass->GetDefaultObject())->ConfigureFlyoutDefaults(EntryClass);
			FKismetEditorUtilities::CompileBlueprint(Panel);
			if (!SaveAssetPackage(LogPrefix, *Panel)) { return; }
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: SNS 플라이아웃 제작 완료. 기존 패널·행·Designer 편집 보존."), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.LiveSnsFlyoutSky"), TEXT("왼쪽 위 SNS 도크 패널을 최초 한 번 제작한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
