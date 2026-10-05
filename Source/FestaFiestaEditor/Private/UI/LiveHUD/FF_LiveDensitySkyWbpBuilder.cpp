// P8 밀집: 고정 요약·히트맵 제어와 스크롤 범례. 저장 후 외형은 Designer가 소유한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LiveDensityLegendRowWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelDensity.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveDensitySky"

namespace FFLiveDensitySkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_DENSITY_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveDensitySky.Version");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelDensity");
	const FString RowFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Rows");
	const FString RowPackage = RowFolder / TEXT("WBP_FF_LiveDensityLegendRow");
	const FString IconPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Density_White_256");

	UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Value, int32 Size = 17, FLinearColor Tint = Ink, bool bVariable = false)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Value, Size, Tint, bVariable);
		Result->SetAutoWrapText(true); return Result;
	}

	void CenterColumn(UHorizontalBox& Parent, UWidget& Child, FMargin Padding = FMargin(0.f), bool bFill = false)
	{
		Column(Parent, Child, Padding, bFill);
		CastChecked<UHorizontalBoxSlot>(Child.Slot)->SetVerticalAlignment(VAlign_Center);
	}

	void StyleValue(UTextBlock& Value, int32 Size, FLinearColor Tint = Ink)
	{
		StyleText(Value, Size, Tint, Size >= 20 ? TEXT("SemiBold") : TEXT("Medium"));
		Value.SetAutoWrapText(true); Value.SetText(LOCTEXT("Unknown", "—"));
	}

	bool BuildRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Row"));
		Size->SetMinDesiredHeight(54.f); Tree.RootWidget = Size;
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Row"), true);
		Frame->SetBrush(Brush(FLinearColor::White, 4.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(12.f, 8.f)); Size->AddChild(Frame);
		UHorizontalBox* Values = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Row")); Frame->AddChild(Values);
		USizeBox* ChipSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_GradeColor")); ChipSize->SetWidthOverride(12.f); ChipSize->SetHeightOverride(30.f);
		UBorder* Chip = MakeWidget<UBorder>(BP, Tree, TEXT("Border_GradeColor"), true); Chip->SetBrush(Brush(FLinearColor::White)); Chip->SetPadding(FMargin(0.f)); ChipSize->AddChild(Chip);
		CenterColumn(*Values, *ChipSize, FMargin(0.f, 0.f, 16.f, 0.f));
		USizeBox* GradeSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Grade")); GradeSize->SetWidthOverride(190.f);
		GradeSize->AddChild(Label(BP, Tree, TEXT("Text_Grade"), LOCTEXT("GradePreview", "등급 · 상태"), 18, Ink, true));
		CenterColumn(*Values, *GradeSize, FMargin(0.f, 0.f, 16.f, 0.f));
		CenterColumn(*Values, *Label(BP, Tree, TEXT("Text_Range"), LOCTEXT("RangePreview", "원본 밀도 구간"), 16, Muted, true), FMargin(0.f, 0.f, 12.f, 0.f), true);
		USizeBox* PeakSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_CurrentPeak")); PeakSize->SetWidthOverride(104.f);
		UBorder* Peak = MakeWidget<UBorder>(BP, Tree, TEXT("Border_CurrentPeak"), true); Peak->SetBrush(Brush(SkyWash, 4.f, 1.f, SkyLine)); Peak->SetPadding(FMargin(8.f, 5.f));
		UTextBlock* PeakText = Label(BP, Tree, TEXT("Text_CurrentPeak"), LOCTEXT("CurrentPeak", "현재 최대"), 14, Ink, true);
		PeakText->SetJustification(ETextJustify::Center); Peak->AddChild(PeakText); PeakSize->AddChild(Peak); CenterColumn(*Values, *PeakSize);
		return true;
	}

	void Metric(UWidgetBlueprint& BP, UWidgetTree& Tree, UWrapBox& Parent, const TCHAR* Key,
		const FText& Caption, UTextBlock& Value, UTextBlock* Note = nullptr)
	{
		const FString Prefix = FString(TEXT("Density_")) + Key;
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(Prefix + TEXT("_Size"))); Size->SetWidthOverride(340.f); Size->SetMinDesiredHeight(132.f);
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, FName(Prefix + TEXT("_Border"))); Frame->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(18.f, 14.f)); Size->AddChild(Frame);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body"))); Frame->AddChild(Body);
		Row(*Body, *Label(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 17, Muted), FMargin(0.f, 0.f, 0.f, 8.f));
		StyleValue(Value, 30); Row(*Body, Value);
		if (Note) { StyleValue(*Note, 16); Row(*Body, *Note, FMargin(0.f, 6.f, 0.f, 0.f)); }
		Parent.AddChild(Size);
	}

	bool BuildPanel(UWidgetBlueprint& BP, UTexture2D& HeaderIcon)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Content")));
		UVerticalBox* Sections = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Sky_VerticalBox_Sections")));
		UHorizontalBox* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Header")));
		USizeBox* HeaderSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Sky_SizeBox_Header")));
		UWidget* Close = Tree.FindWidget(TEXT("Sky_SizeBox_Close"));
		UTextBlock* Title = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PanelTitle")));
		UTextBlock* Peak = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Peak")));
		UTextBlock* PeakGrade = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PeakGrade")));
		UTextBlock* Congested = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Congested")));
		UTextBlock* CellSize = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_CellSize")));
		UTextBlock* Note = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_MissingNote")));
		UTextBlock* ToggleLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ToggleLabel")));
		UButton* Toggle = Cast<UButton>(Tree.FindWidget(TEXT("Button_Toggle")));
		UVerticalBox* Legend = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Box_Legend")));
		UScrollBox* Scroll = Cast<UScrollBox>(Tree.FindWidget(TEXT("Scroll_Density")));
		if (!Content || !Sections || !Header || !HeaderSize || !Close || !Title || !Peak || !PeakGrade || !Congested || !CellSize || !Note || !ToggleLabel || !Toggle || !Legend || !Scroll) return false;
		BP.Modify(); Tree.Modify();
		for (UWidget* Value : TArray<UWidget*>{Peak, PeakGrade, Congested, CellSize, Note, Toggle, Legend, Scroll}) Value->RemoveFromParent();
		// 기존 변수 객체/GUID는 재사용하며 장식은 접어서 보관한다.
		const TArray<UWidget*> OldChildren = Content->GetAllChildren(); Content->ClearChildren();
		const TArray<UWidget*> OldScrollChildren = Scroll->GetAllChildren(); Scroll->ClearChildren();
		UVerticalBox* Legacy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Density_LegacyDecorations")); Legacy->SetVisibility(ESlateVisibility::Collapsed);
		for (UWidget* Old : OldChildren) Legacy->AddChild(Old);
		for (UWidget* Old : OldScrollChildren) Legacy->AddChild(Old);
		Row(*Content, *Legacy);
		Header->ClearChildren(); HeaderSize->SetHeightOverride(80.f);
		USizeBox* IconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Density_SizeBox_TitleIcon")); IconSize->SetWidthOverride(32.f); IconSize->SetHeightOverride(32.f);
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Density_Image_Title")); Icon->SetBrushFromTexture(&HeaderIcon); Icon->SetColorAndOpacity(Ink); IconSize->AddChild(Icon);
		CenterColumn(*Header, *IconSize, FMargin(0.f, 0.f, 14.f, 0.f));
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Density_Heading")); CenterColumn(*Header, *Heading, FMargin(0.f), true);
		StyleText(*Title, 26, Ink, TEXT("SemiBold")); Row(*Heading, *Title);
		Row(*Heading, *Label(BP, Tree, TEXT("Density_Subtitle"), LOCTEXT("Subtitle", "현장의 밀도와 혼잡 구간을 확인하세요."), 15, Muted), FMargin(0.f, 3.f, 0.f, 0.f)); CenterColumn(*Header, *Close);
		UWrapBox* Metrics = MakeWidget<UWrapBox>(BP, Tree, TEXT("Density_Metrics")); Metrics->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		Row(*Content, *Metrics, FMargin(0.f, 0.f, 0.f, 14.f));
		Metric(BP, Tree, *Metrics, TEXT("Peak"), LOCTEXT("Peak", "최대 밀도"), *Peak, PeakGrade);
		Metric(BP, Tree, *Metrics, TEXT("Congested"), LOCTEXT("Congested", "혼잡한 칸"), *Congested);
		UTextBlock* Area = Label(BP, Tree, TEXT("Text_CellArea"), LOCTEXT("AreaPreview", "—"), 16, Ink, true);
		Metric(BP, Tree, *Metrics, TEXT("Cell"), LOCTEXT("Cell", "집계 칸 크기"), *CellSize, Area);
		UHorizontalBox* LegendHeader = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Density_LegendHeading"));
		CenterColumn(*LegendHeader, *Label(BP, Tree, TEXT("Density_LegendTitle"), LOCTEXT("Legend", "밀집 등급"), 22), FMargin(0.f), true);
		CenterColumn(*LegendHeader, *Label(BP, Tree, TEXT("Density_LegendUnit"), LOCTEXT("Unit", "단위: 명/㎡"), 15, Muted));
		Row(*Content, *LegendHeader, FMargin(0.f, 0.f, 0.f, 10.f));
		Scroll->SetAllowOverscroll(false); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f)); Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		Legend->ClearChildren(); CastChecked<UScrollBoxSlot>(Scroll->AddChild(Legend))->SetHorizontalAlignment(HAlign_Fill);
		Row(*Content, *Scroll, FMargin(0.f, 0.f, 0.f, 10.f), true);
		Row(*Content, *Label(BP, Tree, TEXT("Density_LegendHint"), LOCTEXT("LegendHint", "색상과 함께 등급·수치를 확인하세요."), 14, Muted), FMargin(0.f, 0.f, 0.f, 10.f));
		UBorder* Footer = MakeWidget<UBorder>(BP, Tree, TEXT("Density_FooterBorder")); Footer->SetBrush(Brush(SkyWash, 6.f, 1.f, SkyLine)); Footer->SetPadding(FMargin(14.f, 10.f));
		UHorizontalBox* FooterBody = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Density_FooterBody")); Footer->AddChild(FooterBody);
		UVerticalBox* Status = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Density_HeatmapInfo")); CenterColumn(*FooterBody, *Status, FMargin(0.f, 0.f, 18.f, 0.f), true);
		Row(*Status, *Label(BP, Tree, TEXT("Text_HeatmapStatus"), LOCTEXT("HeatmapPreview", "히트맵 상태 확인 중"), 17, Ink, true));
		StyleValue(*Note, 14, Muted); Row(*Status, *Note, FMargin(0.f, 4.f, 0.f, 0.f));
		FButtonStyle ToggleStyle = Toggle->GetStyle(); ToggleStyle.Normal = Brush(Green, 5.f, 1.f, Green);
		ToggleStyle.Hovered = Brush(Color(28, 153, 103), 5.f, 1.5f, Green); ToggleStyle.Pressed = Brush(Color(18, 112, 73), 5.f);
		ToggleStyle.Disabled = Brush(Color(219, 230, 225), 5.f, 1.f, SkyLine); Toggle->SetStyle(ToggleStyle);
		StyleText(*ToggleLabel, 17, FLinearColor::White, TEXT("SemiBold")); ToggleLabel->SetJustification(ETextJustify::Center);
		Toggle->SetBackgroundColor(FLinearColor::White); Toggle->SetColorAndOpacity(FLinearColor::White);
		USizeBox* ToggleSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Density_SizeBox_Toggle")); ToggleSize->SetWidthOverride(190.f); ToggleSize->SetHeightOverride(46.f); ToggleSize->AddChild(Toggle);
		CenterColumn(*FooterBody, *ToggleSize); Row(*Content, *Footer);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1")); return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(IconPackage));
		if (!BP || !BP->WidgetTree || !Icon || !BP->ParentClass->IsChildOf(UFF_LivePanelDensity::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1")) { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer 편집 보존"), LogPrefix); return; }
		if (BP->GetOutermost()->IsDirty()) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 밀집 WBP"), LogPrefix); return; }
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveDensitySky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK) return;
		UWidgetBlueprint* RowBP = EnsureWidgetBlueprint(LogPrefix, RowPackage, RowFolder, *UFF_LiveDensityLegendRowWidget::StaticClass(), BuildRow);
		if (!RowBP || !RowBP->GeneratedClass || !RowBP->GeneratedClass->IsChildOf(UFF_LiveDensityLegendRowWidget::StaticClass())) return;
		if (!BuildPanel(*BP, *Icon) || !SaveNewBlueprint(LogPrefix, *BP))
		{
			BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 제작 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 밀집 요약·범례·히트맵 제어 저장 완료"), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveDensitySkyDesign"), TEXT("P8 밀집 화면과 범례 행을 최초 한 번 제작한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
