// P6 SNS 패널·글 카드만 제작한다. 기존 필터·새 글 보류·스크롤 객체를 유지한다.
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
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveSnsSky"

namespace FFLiveSnsSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_SNS_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveSnsSky.Version");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelSNS");
	const FString EntryPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsPostEntry");
	const FString Icons = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

	void CenterColumn(UHorizontalBox& Parent, UWidget& Child, FMargin Padding = FMargin(0.f), bool bFill = false)
	{
		Column(Parent, Child, Padding, bFill);
		CastChecked<UHorizontalBoxSlot>(Child.Slot)->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* Icon(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UTexture2D& Texture, float Extent)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(FString(Name) + TEXT("_Size")));
		Size->SetWidthOverride(Extent); Size->SetHeightOverride(Extent);
		UImage* Image = MakeWidget<UImage>(BP, Tree, Name);
		Image->SetBrushFromTexture(&Texture); Image->SetColorAndOpacity(Ink); Size->AddChild(Image);
		return Size;
	}

	void StyleControl(UButton& Control, UTextBlock& Label, bool bFilter)
	{
		FButtonStyle Style = Control.GetStyle();
		// 필터 선택색은 CDO 색과 곱해지므로 기본 면을 흰색으로 둔다.
		Style.Normal = Brush(bFilter ? FLinearColor::White : SkyWash, 6.f, 1.f, SkyLine);
		Style.Hovered = Brush(FLinearColor::White, 6.f, 2.f, Color(100, 177, 210));
		Style.Pressed = Brush(FLinearColor::White, 6.f, 2.f, Ink);
		Style.Disabled = Brush(SkyWash, 6.f, 1.f, SkyLine);
		Style.NormalPadding = Style.PressedPadding = FMargin(14.f, 5.f);
		Control.SetStyle(Style); Control.SetBackgroundColor(FLinearColor::White);
		StyleText(Label, 17, Ink, TEXT("SemiBold")); Label.SetAutoWrapText(true);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Label.Slot))
		{
			ContentSlot->SetPadding(FMargin(0.f));
			ContentSlot->SetHorizontalAlignment(HAlign_Center); ContentSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	bool BuildEntry(UWidgetBlueprint& BP, UTexture2D& Person)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		UBorder* Card = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Card")));
		UBorder* Sentiment = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Sentiment")));
		USizeBox* Avatar = Cast<USizeBox>(Tree.FindWidget(TEXT("SizeBox_Avatar")));
		UOverlay* AvatarParts = Cast<UOverlay>(Tree.FindWidget(TEXT("Overlay_Avatar")));
		UTextBlock* Author = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Author")));
		UTextBlock* Body = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Body")));
		UTextBlock* Label = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Sentiment")));
		UTextBlock* Symbol = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_SentimentSymbol")));
		if (!Card || !Sentiment || !Avatar || !AvatarParts || !Author || !Body || !Label || !Symbol || Tree.RootWidget != Card) return false;
		BP.Modify(); Tree.Modify();
		USizeBox* CardSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Sns_SizeBox_Card"));
		CardSize->SetMinDesiredHeight(100.f); Tree.RootWidget = CardSize; CardSize->AddChild(Card);
		Card->SetBrush(Brush(Paper, 7.f, 1.f, SkyLine)); Card->SetBrushColor(FLinearColor::White);
		Card->SetPadding(FMargin(20.f, 16.f));
		Avatar->SetWidthOverride(56.f); Avatar->SetHeightOverride(56.f);
		if (UHorizontalBoxSlot* AvatarSlot = Cast<UHorizontalBoxSlot>(Avatar->Slot)) AvatarSlot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		// 기존 실루엣 객체/GUID는 접어 보관하고, 공용 인원 아이콘을 명확히 표시한다.
		for (UWidget* Part : AvatarParts->GetAllChildren()) Part->SetVisibility(ESlateVisibility::Collapsed);
		UBorder* AvatarFrame = MakeWidget<UBorder>(BP, Tree, TEXT("Sns_Border_Avatar"));
		AvatarFrame->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); AvatarFrame->SetPadding(FMargin(10.f));
		AvatarFrame->AddChild(Icon(BP, Tree, TEXT("Sns_Image_Avatar"), Person, 36.f));
		UOverlaySlot* AvatarFrameSlot = AvatarParts->AddChildToOverlay(AvatarFrame);
		AvatarFrameSlot->SetHorizontalAlignment(HAlign_Fill); AvatarFrameSlot->SetVerticalAlignment(VAlign_Fill);
		StyleText(*Author, 20, Ink, TEXT("SemiBold")); Author->SetAutoWrapText(true);
		StyleText(*Body, 18, Ink, TEXT("Medium")); Body->SetAutoWrapText(true);
		StyleText(*Label, 15, Ink, TEXT("Medium")); StyleText(*Symbol, 17, Ink, TEXT("SemiBold"));
		Sentiment->SetBrush(Brush(FLinearColor::White, 14.f)); Sentiment->SetPadding(FMargin(12.f, 5.f));
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	bool BuildPanel(UWidgetBlueprint& BP, UTexture2D& SnsIcon)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Content")));
		UHorizontalBox* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Header")));
		USizeBox* HeaderSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Sky_SizeBox_Header")));
		UWidget* Close = Tree.FindWidget(TEXT("Sky_SizeBox_Close"));
		UTextBlock* Title = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PanelTitle")));
		UTextBlock* Subtitle = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Subtitle")));
		UTextBlock* Count = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Count")));
		UTextBlock* NewText = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_NewPosts")));
		UButton* NewPosts = Cast<UButton>(Tree.FindWidget(TEXT("Button_NewPosts")));
		UOverlay* Feed = Cast<UOverlay>(Tree.FindWidget(TEXT("Overlay_Feed")));
		UScrollBox* Scroll = Cast<UScrollBox>(Tree.FindWidget(TEXT("Scroll_Posts")));
		UTextBlock* Empty = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Empty")));
		UTextBlock* Missing = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_MissingNote")));
		UTextBlock* ReadNote = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ReadPositionNote")));
		TArray<UButton*> Filters; TArray<UTextBlock*> Labels;
		for (const TCHAR* Suffix : { TEXT("All"), TEXT("Positive"), TEXT("Negative") })
		{
			Filters.Add(Cast<UButton>(Tree.FindWidget(FName(FString(TEXT("Button_Filter")) + Suffix))));
			Labels.Add(Cast<UTextBlock>(Tree.FindWidget(FName(FString(TEXT("Text_Filter")) + Suffix))));
		}
		if (!Content || !Header || !HeaderSize || !Close || !Title || !Subtitle || !Count || !NewText || !NewPosts || !Feed || !Scroll || !Empty || !Missing || !ReadNote || Filters.Contains(nullptr) || Labels.Contains(nullptr)) return false;
		BP.Modify(); Tree.Modify();
		for (UWidget* Widget : TArray<UWidget*>{Subtitle, Count, NewPosts, Feed, Missing, ReadNote}) Widget->RemoveFromParent();
		for (UButton* Filter : Filters) Filter->RemoveFromParent();
		const TArray<UWidget*> OldChildren = Content->GetAllChildren(); Content->ClearChildren();
		UVerticalBox* Legacy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Sns_LegacyDecorations")); Legacy->SetVisibility(ESlateVisibility::Collapsed);
		for (UWidget* Old : OldChildren) Legacy->AddChild(Old);
		Row(*Content, *Legacy);
		Header->ClearChildren(); HeaderSize->SetHeightOverride(84.f);
		CenterColumn(*Header, *Icon(BP, Tree, TEXT("Sns_Image_Title"), SnsIcon, 32.f), FMargin(0.f, 0.f, 14.f, 0.f));
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Sns_Heading"));
		StyleText(*Title, 26, Ink, TEXT("SemiBold")); Row(*Heading, *Title);
		StyleText(*Subtitle, 16, Muted, TEXT("Medium")); Subtitle->SetAutoWrapText(true); Row(*Heading, *Subtitle, FMargin(0.f, 3.f, 0.f, 0.f));
		CenterColumn(*Header, *Heading, FMargin(0.f), true); CenterColumn(*Header, *Close);
		UHorizontalBox* Toolbar = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Sns_Toolbar"));
		for (int32 Index = 0; Index < Filters.Num(); ++Index)
		{
			StyleControl(*Filters[Index], *Labels[Index], true);
			USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(FString(TEXT("Sns_SizeBox_Filter")) + FString::FromInt(Index)));
			Size->SetWidthOverride(132.f); Size->SetHeightOverride(44.f); Size->AddChild(Filters[Index]);
			CenterColumn(*Toolbar, *Size, FMargin(0.f, 0.f, 10.f, 0.f));
		}
		StyleText(*Count, 16, Muted, TEXT("Medium")); Count->SetJustification(ETextJustify::Right); Count->SetAutoWrapText(true);
		CenterColumn(*Toolbar, *Count, FMargin(12.f, 0.f, 0.f, 0.f), true); Row(*Content, *Toolbar, FMargin(0.f, 0.f, 0.f, 10.f));
		StyleControl(*NewPosts, *NewText, false); NewPosts->SetVisibility(ESlateVisibility::Hidden);
		USizeBox* NewSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Sns_SizeBox_NewPosts"));
		NewSize->SetHeightOverride(38.f); NewSize->AddChild(NewPosts); Row(*Content, *NewSize, FMargin(0.f, 0.f, 0.f, 10.f));
		Scroll->SetAllowOverscroll(false); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f)); Scroll->SetScrollbarPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		StyleText(*Empty, 18, Muted, TEXT("Medium")); Empty->SetAutoWrapText(true);
		Row(*Content, *Feed, FMargin(0.f, 0.f, 0.f, 12.f), true);
		Row(*Content, *Rule(BP, Tree, TEXT("Sns_FooterRule")), FMargin(0.f, 0.f, 0.f, 8.f));
		StyleText(*Missing, 14, Muted, TEXT("Medium")); Missing->SetAutoWrapText(true); Row(*Content, *Missing, FMargin(0.f, 0.f, 0.f, 4.f));
		StyleText(*ReadNote, 14, Muted, TEXT("Medium")); ReadNote->SetAutoWrapText(true); Row(*Content, *ReadNote);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		UWidgetBlueprint* Panel = LoadExistingBlueprint(PanelPackage);
		UWidgetBlueprint* Entry = LoadExistingBlueprint(EntryPackage);
		UTexture2D* Person = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons / TEXT("Metrics/T_FF_LH_Icon_StaffCount_White_256")));
		UTexture2D* Sns = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons / TEXT("Navigation/T_FF_LH_Icon_SNS_White_256")));
		if (!Panel || !Entry || !Person || !Sns || !Panel->WidgetTree || !Entry->WidgetTree || !Panel->ParentClass->IsChildOf(UFF_LivePanelSNS::StaticClass()) || !Entry->ParentClass->IsChildOf(UFF_LiveSnsPostEntry::StaticClass())) return;
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveSnsSky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true)) return;
		for (UWidgetBlueprint* BP : { Entry, Panel })
		{
			if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1")) continue;
			if (BP->GetOutermost()->IsDirty()) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 에셋 %s"), LogPrefix, *BP->GetName()); return; }
			const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), TEXT(".uasset"));
			if (IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK) return;
		}
		for (UWidgetBlueprint* BP : { Entry, Panel })
		{
			if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1")) continue;
			if (!(BP == Entry ? BuildEntry(*BP, *Person) : BuildPanel(*BP, *Sns)) || !SaveNewBlueprint(LogPrefix, *BP))
			{
				BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 제작 실패. 백업=%s"), LogPrefix, *BP->GetName(), *Backup); return;
			}
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: SNS 패널·글 카드 저장 완료, 재실행 시 Designer 편집 보존"), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveSnsSkyDesign"), TEXT("P6 SNS 패널과 글 카드를 최초 한 번 제작한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
