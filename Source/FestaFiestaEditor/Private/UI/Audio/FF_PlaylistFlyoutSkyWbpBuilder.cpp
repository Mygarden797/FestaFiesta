// 음악 패널/곡 행 최초 생성 도구. Designer 수정은 재실행해도 보존한다.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
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
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/Audio/FF_PlaylistPlayerWidget.h"
#include "UI/Audio/FF_PlaylistTrackRowWidget.h"
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPlaylistFlyoutSky"

namespace FFPlaylistFlyoutSky
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	const TCHAR* LogPrefix = TEXT("FF_PLAYLIST_FLYOUT_SKY");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/Audio");
	const FString PanelPackage = Folder / TEXT("WBP_FF_PlaylistFlyout");
	const FString RowPackage = Folder / TEXT("WBP_FF_PlaylistTrackRow");
	const FString ActionFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions");

	UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Value, int32 FontSize = 18, FLinearColor Tint = Ink, bool bVariable = false)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Value, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(false);
		Result->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Result->SetClipping(EWidgetClipping::ClipToBounds);
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}

	USizeBox* Size(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		UWidget& Child, float Width = 0.f, float Height = 0.f)
	{
		USizeBox* Result = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) { Result->SetWidthOverride(Width); }
		if (Height > 0.f) { Result->SetHeightOverride(Height); }
		Result->AddChild(&Child);
		return Result;
	}

	UImage* ActionImage(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const TCHAR* Action, bool bVariable = false, FLinearColor Tint = Ink)
	{
		UImage* Result = MakeWidget<UImage>(BP, Tree, Name, bVariable);
		Result->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			*MakeObjectPath(ActionFolder / FString::Printf(TEXT("T_FF_LH_Icon_%s_White_256"), Action))));
		Result->SetDesiredSizeOverride(FVector2D(24.f, 24.f));
		Result->SetColorAndOpacity(Tint);
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}

	USizeBox* TransportButton(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		UWidget& Content, float Diameter, bool bPrimary)
	{
		UButton* Result = Button(BP, Tree, Name, Content, bPrimary);
		FButtonStyle Style = Result->GetStyle();
		for (FSlateBrush* Shape : { &Style.Normal, &Style.Hovered, &Style.Pressed, &Style.Disabled })
		{
			Shape->OutlineSettings.CornerRadii = FVector4(Diameter * .5f, Diameter * .5f, Diameter * .5f, Diameter * .5f);
		}
		Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
		Result->SetStyle(Style);
		return Size(BP, Tree, *FString::Printf(TEXT("%s_Size"), Name), *Result, Diameter, Diameter);
	}

	bool BuildRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Track"));
		UButton* Track = Button(BP, Tree, TEXT("Button_Track"), *Content);
		Tree.RootWidget = Size(BP, Tree, TEXT("SizeBox_Track"), *Track, 0.f, 56.f);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Content->Slot)) { Slot->SetHorizontalAlignment(HAlign_Fill); }
		Column(*Content, *Size(BP, Tree, TEXT("SizeBox_Index"),
			*Label(BP, Tree, TEXT("Text_Index"), LOCTEXT("IndexPreview", "01"), 16, Muted, true), 32.f));
		Column(*Content, *Label(BP, Tree, TEXT("Text_Title"), LOCTEXT("TrackPreview", "축제 음악"), 17, Ink, true), FMargin(8.f, 0.f), true);
		Column(*Content, *Size(BP, Tree, TEXT("SizeBox_State"),
			*Label(BP, Tree, TEXT("Text_State"), LOCTEXT("Play", "재생"), 14, Green, true), 62.f));
		for (int32 Index = 0; Index < Content->GetChildrenCount(); ++Index)
		{
			CastChecked<UHorizontalBoxSlot>(Content->GetChildAt(Index)->Slot)->SetVerticalAlignment(VAlign_Center);
		}
		return true;
	}

	bool BuildPanel(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UBorder* Root = MakeWidget<UBorder>(BP, Tree, TEXT("Panel_Player"), true);
		Root->SetBrush(Brush(Paper, 10.f, 1.5f, SkyLine));
		Root->SetPadding(FMargin(20.f));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Tree.RootWidget = Root;
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Sections"));
		Root->AddChild(Sections);

		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Header"));
		Column(*Header, *Label(BP, Tree, TEXT("Text_PanelTitle"), LOCTEXT("Title", "♫  음악"), 28), FMargin(0.f), true);
		UButton* Close = Button(BP, Tree, TEXT("Button_Close"), *CloseIcon(BP, Tree));
		FButtonStyle CloseStyle = Close->GetStyle(); CloseStyle.NormalPadding = CloseStyle.PressedPadding = FMargin(2.f); Close->SetStyle(CloseStyle);
		Close->SetToolTipText(LOCTEXT("Close", "음악 패널 닫기"));
		Column(*Header, *Size(BP, Tree, TEXT("SizeBox_Close"), *Close, 36.f, 36.f));
		Row(*Sections, *Header);
		Row(*Sections, *Label(BP, Tree, TEXT("Text_Subtitle"), LOCTEXT("Subtitle", "축제 플레이리스트"), 16, Muted), FMargin(0.f, 4.f, 0.f, 12.f));
		Row(*Sections, *Rule(BP, Tree, TEXT("Border_HeaderRule")), FMargin(0.f, 0.f, 0.f, 18.f));

		UHorizontalBox* Current = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Current"));
		UBorder* Cover = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Cover"));
		Cover->SetBrush(Brush(Color(231, 246, 251), 8.f, 1.f, SkyLine));
		Cover->SetHorizontalAlignment(HAlign_Center); Cover->SetVerticalAlignment(VAlign_Center);
		Cover->AddChild(Label(BP, Tree, TEXT("Text_CoverNote"), LOCTEXT("CoverNote", "♫"), 58));
		Column(*Current, *Size(BP, Tree, TEXT("SizeBox_Cover"), *Cover, 116.f, 116.f));
		UVerticalBox* Info = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_CurrentInfo"));
		UBorder* State = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PlaybackState"));
		State->SetBrush(Brush(Color(223, 244, 233), 5.f)); State->SetPadding(FMargin(8.f, 4.f));
		State->AddChild(Label(BP, Tree, TEXT("Text_PlaybackState"), LOCTEXT("Playing", "지금 재생 중"), 14, Green, true));
		Row(*Info, *State);
		CastChecked<UVerticalBoxSlot>(State->Slot)->SetHorizontalAlignment(HAlign_Left);
		Row(*Info, *Label(BP, Tree, TEXT("Text_TrackTitle"), LOCTEXT("TrackPreview", "축제 음악"), 23, Ink, true), FMargin(0.f, 10.f, 0.f, 8.f));
		Row(*Info, *Label(BP, Tree, TEXT("Text_TrackIndex"), LOCTEXT("CountPreview", "1 / 8"), 16, Muted, true));
		Column(*Current, *Info, FMargin(18.f, 0.f, 0.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Info->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(*Sections, *Current, FMargin(0.f, 0.f, 0.f, 16.f));

		UProgressBar* Progress = MakeWidget<UProgressBar>(BP, Tree, TEXT("ProgressBar_Track"), true);
		FProgressBarStyle ProgressStyle;
		ProgressStyle.BackgroundImage = Brush(Color(214, 237, 247), 3.f);
		ProgressStyle.FillImage = Brush(FLinearColor::White, 3.f);
		ProgressStyle.MarqueeImage = ProgressStyle.FillImage;
		Progress->SetWidgetStyle(ProgressStyle); Progress->SetFillColorAndOpacity(Green); Progress->SetPercent(.25f);
		Progress->SetVisibility(ESlateVisibility::HitTestInvisible);
		Progress->SetToolTipText(LOCTEXT("ProgressTip", "현재 재생 위치"));
		Row(*Sections, *Size(BP, Tree, TEXT("SizeBox_Progress"), *Progress, 0.f, 6.f));
		UHorizontalBox* Clocks = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Clocks"));
		Column(*Clocks, *Label(BP, Tree, TEXT("Text_Elapsed"), LOCTEXT("Elapsed", "0:00"), 14, Muted, true), FMargin(0.f), true);
		Column(*Clocks, *Label(BP, Tree, TEXT("Text_Duration"), LOCTEXT("Duration", "—"), 14, Muted, true));
		Row(*Sections, *Clocks, FMargin(0.f, 6.f, 0.f, 6.f));

		UHorizontalBox* Controls = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Controls"));
		UImage* Previous = ActionImage(BP, Tree, TEXT("Image_Previous"), TEXT("Next")); Previous->SetRenderTransformAngle(180.f);
		UOverlay* PlayIcons = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_PlayIcons"));
		for (UImage* Icon : { ActionImage(BP, Tree, TEXT("Image_Play"), TEXT("Play"), true, Paper), ActionImage(BP, Tree, TEXT("Image_Pause"), TEXT("Pause"), true, Paper) })
		{
			UOverlaySlot* Slot = PlayIcons->AddChildToOverlay(Icon); Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center);
		}
		Column(*Controls, *TransportButton(BP, Tree, TEXT("Button_Prev"), *Previous, 48.f, false), FMargin(0.f, 0.f, 28.f, 0.f));
		Column(*Controls, *TransportButton(BP, Tree, TEXT("Button_PlayPause"), *PlayIcons, 58.f, true));
		Column(*Controls, *TransportButton(BP, Tree, TEXT("Button_Next"), *ActionImage(BP, Tree, TEXT("Image_Next"), TEXT("Next")), 48.f, false), FMargin(28.f, 0.f, 0.f, 0.f));
		for (int32 Index = 0; Index < Controls->GetChildrenCount(); ++Index) { CastChecked<UHorizontalBoxSlot>(Controls->GetChildAt(Index)->Slot)->SetVerticalAlignment(VAlign_Center); }
		Row(*Sections, *Controls, FMargin(0.f, 0.f, 0.f, 14.f));
		CastChecked<UVerticalBoxSlot>(Controls->Slot)->SetHorizontalAlignment(HAlign_Center);

		UHorizontalBox* Toggles = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Toggles"));
		UButton* Auto = Button(BP, Tree, TEXT("Button_AutoAdvance"), *Label(BP, Tree, TEXT("Text_AutoAdvance"), LOCTEXT("Auto", "자동 다음 곡 켬"), 15, Ink, true));
		UButton* Shuffle = Button(BP, Tree, TEXT("Button_Shuffle"), *Label(BP, Tree, TEXT("Text_Shuffle"), LOCTEXT("Shuffle", "셔플 끔"), 15, Ink, true));
		Auto->SetToolTipText(LOCTEXT("AutoTip", "끄면 지금 곡을 반복합니다.")); Shuffle->SetToolTipText(LOCTEXT("ShuffleTip", "곡 순서를 섞습니다. 지금 곡은 유지됩니다."));
		Column(*Toggles, *Auto, FMargin(0.f, 0.f, 8.f, 0.f), true); Column(*Toggles, *Shuffle, FMargin(0.f), true);
		Row(*Sections, *Size(BP, Tree, TEXT("SizeBox_Toggles"), *Toggles, 0.f, 42.f), FMargin(0.f, 0.f, 0.f, 18.f));
		Row(*Sections, *Rule(BP, Tree, TEXT("Border_ListRule")), FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Sections, *Label(BP, Tree, TEXT("Text_ListTitle"), LOCTEXT("Playlist", "재생 목록"), 18), FMargin(0.f, 0.f, 0.f, 8.f));
		UVerticalBox* List = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Panel_List"), true);
		UScrollBox* Tracks = MakeWidget<UScrollBox>(BP, Tree, TEXT("ScrollBox_Tracks"), true);
		Tracks->SetScrollBarVisibility(ESlateVisibility::Visible);
		Tracks->SetScrollbarThickness(FVector2D(5.f, 5.f));
		Tracks->SetAllowOverscroll(false); Tracks->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Row(*List, *Tracks, FMargin(0.f), true); Row(*Sections, *List, FMargin(0.f), true);
		return true;
	}

	void Apply(bool bRebuild)
	{
		const bool bNewPanel = !LoadExistingBlueprint(PanelPackage);
		UWidgetBlueprint* Track = bRebuild
			? ReplaceWidgetBlueprint(LogPrefix, RowPackage, Folder, *UFF_PlaylistTrackRowWidget::StaticClass(), BuildRow)
			: EnsureWidgetBlueprint(LogPrefix, RowPackage, Folder, *UFF_PlaylistTrackRowWidget::StaticClass(), BuildRow);
		if (!Track || !Track->GeneratedClass) { return; }
		UWidgetBlueprint* Panel = bRebuild
			? ReplaceWidgetBlueprint(LogPrefix, PanelPackage, Folder, *UFF_PlaylistPlayerWidget::StaticClass(), BuildPanel)
			: EnsureWidgetBlueprint(LogPrefix, PanelPackage, Folder, *UFF_PlaylistPlayerWidget::StaticClass(), BuildPanel);
		if (!Panel || !Panel->GeneratedClass) { return; }
		if (bNewPanel || bRebuild)
		{
			CastChecked<UFF_PlaylistPlayerWidget>(Panel->GeneratedClass->GetDefaultObject())->ConfigureFlyoutDefaults(Track->GeneratedClass.Get());
			FKismetEditorUtilities::CompileBlueprint(Panel); SaveAssetPackage(LogPrefix, *Panel);
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 음악 펼침 패널과 곡 행 생성. 기존 미니 플레이어는 보존."), LogPrefix);
	}
	void ApplyDefault() { Apply(false); }
	void Rebuild() { Apply(true); }
	FAutoConsoleCommand ApplyCommand(TEXT("FF.PlaylistFlyoutSky"), TEXT("음악 펼침 패널과 곡 행을 처음 한 번 생성한다."), FConsoleCommandDelegate::CreateStatic(&ApplyDefault));
	FAutoConsoleCommand RebuildCommand(TEXT("FF.PlaylistFlyoutSky.Rebuild"), TEXT("음악 펼침 패널/곡 행의 Designer 배치를 초기화한다."), FConsoleCommandDelegate::CreateStatic(&Rebuild));
}

#undef LOCTEXT_NAMESPACE
#endif
