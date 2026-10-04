// P2 운영 화면 제작. 실행 중에는 저장된 Designer 스타일을 덮어쓰지 않는다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
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
#include "UI/LiveHUD/Panels/FF_LivePanelOperations.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveOperationsSky"

namespace FFLiveOperationsSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_OPERATIONS_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveOperationsSky.Version");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelOperations");
	const FString Icons = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

	USizeBox* Icon(UWidgetBlueprint& BP, UWidgetTree& Tree, const FString& Name,
		const FString& RelativePath, float Size = 28.f, bool bOriginalColor = false)
	{
		USizeBox* Box = MakeWidget<USizeBox>(BP, Tree, FName(Name + TEXT("_Size")));
		Box->SetWidthOverride(Size); Box->SetHeightOverride(Size);
		UImage* Image = MakeWidget<UImage>(BP, Tree, FName(Name));
		Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons / RelativePath)));
		Image->SetColorAndOpacity(bOriginalColor ? FLinearColor::White : Ink);
		Box->AddChild(Image);
		return Box;
	}

	UVerticalBox* Card(UWidgetBlueprint& BP, UWidgetTree& Tree, UWrapBox& Parent,
		const TCHAR* Key, const FText& Caption, const TCHAR* IconPath, UTextBlock& Value)
	{
		const FString Prefix = FString(TEXT("Ops_")) + Key;
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(Prefix + TEXT("_Size")));
		Size->SetWidthOverride(250.f); Size->SetMinDesiredHeight(144.f);
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, FName(Prefix + TEXT("_Border")));
		Frame->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(18.f, 14.f));
		Size->AddChild(Frame); Parent.AddChild(Size);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body")));
		Frame->AddChild(Body);
		UHorizontalBox* Heading = MakeWidget<UHorizontalBox>(BP, Tree, FName(Prefix + TEXT("_Heading")));
		Column(*Heading, *Icon(BP, Tree, Prefix + TEXT("_Icon"), IconPath, 28.f, FString(Key) == TEXT("Gold")), FMargin(0.f, 0.f, 12.f, 0.f));
		UTextBlock* Label = Text(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 17, Ink);
		Column(*Heading, *Label, FMargin(0.f), true);
		CastChecked<UHorizontalBoxSlot>(Label->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(*Body, *Heading, FMargin(0.f, 0.f, 0.f, 8.f));
		StyleText(Value, 30, Ink, TEXT("SemiBold"));
		Value.SetAutoWrapText(true);
		Row(*Body, Value);
		return Body;
	}

	bool Build(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Content")));
		UHorizontalBox* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Header")));
		USizeBox* HeaderSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Sky_SizeBox_Header")));
		UWidget* Close = Tree.FindWidget(TEXT("Sky_SizeBox_Close"));
		UTextBlock* Title = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PanelTitle")));
		UTextBlock* Revenue = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_TodayRevenue")));
		UTextBlock* Gold = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Gold")));
		UTextBlock* Satisfaction = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Satisfaction")));
		UTextBlock* Safety = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Safety")));
		UTextBlock* Goal = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_GoalProgress")));
		UTextBlock* Detail = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_GoalDetail")));
		UProgressBar* Progress = Cast<UProgressBar>(Tree.FindWidget(TEXT("Bar_GoalProgress")));
		UTextBlock* Notice = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_DummyNotice")));
		UWidget* Warning = Tree.FindWidget(TEXT("Box_Warning"));
		if (!Content || !Header || !HeaderSize || !Close || !Title || !Revenue || !Gold || !Satisfaction
			|| !Safety || !Goal || !Detail || !Progress || !Notice || !Warning) return false;

		BP.Modify(); Tree.Modify();
		// 기존 바인딩 대상과 GUID를 유지한다. 이전 장식은 숨긴 보관함으로 옮긴다.
		for (UWidget* Value : TArray<UWidget*>{Revenue, Gold, Satisfaction, Safety, Goal, Detail, Progress, Notice, Warning})
		{
			Value->RemoveFromParent();
		}
		const TArray<UWidget*> OldChildren = Content->GetAllChildren();
		Content->ClearChildren();
		UVerticalBox* Legacy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Ops_LegacyDecorations"));
		Legacy->SetVisibility(ESlateVisibility::Collapsed);
		for (UWidget* Old : OldChildren) Legacy->AddChild(Old);
		Row(*Content, *Legacy);

		Header->ClearChildren(); HeaderSize->SetHeightOverride(78.f);
		USizeBox* PanelIcon = Icon(BP, Tree, TEXT("Ops_PanelIcon"), TEXT("Navigation/T_FF_LH_Icon_Operations_White_256"));
		Column(*Header, *PanelIcon, FMargin(0.f, 0.f, 14.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(PanelIcon->Slot)->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Ops_Heading"));
		Column(*Header, *Heading, FMargin(0.f), true);
		CastChecked<UHorizontalBoxSlot>(Heading->Slot)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox* TitleRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Ops_TitleRow"));
		Row(*Heading, *TitleRow);
		StyleText(*Title, 26, Ink, TEXT("SemiBold"));
		Column(*TitleRow, *Title, FMargin(0.f, 0.f, 16.f, 0.f));
		UBorder* Day = MakeWidget<UBorder>(BP, Tree, TEXT("Ops_DayChip"));
		Day->SetBrush(Brush(SkyWash, 12.f, 1.f, SkyLine)); Day->SetPadding(FMargin(12.f, 3.f));
		Day->AddChild(Text(BP, Tree, TEXT("Text_Day"), LOCTEXT("Day", "1일차"), 16, Ink, true));
		Column(*TitleRow, *Day);
		CastChecked<UHorizontalBoxSlot>(Day->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(*Heading, *Text(BP, Tree, TEXT("Ops_Subtitle"), LOCTEXT("Subtitle", "축제 운영 현황을 한눈에 확인하세요."), 15, Muted), FMargin(0.f, 2.f, 0.f, 0.f));
		Column(*Header, *Close);
		CastChecked<UHorizontalBoxSlot>(Close->Slot)->SetVerticalAlignment(VAlign_Center);

		UWrapBox* Metrics = MakeWidget<UWrapBox>(BP, Tree, TEXT("Ops_Metrics"));
		Metrics->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		Row(*Content, *Metrics, FMargin(0.f, 0.f, 0.f, 16.f));
		Revenue->SetText(LOCTEXT("EmptyRevenue", "—")); Gold->SetText(LOCTEXT("EmptyGold", "—"));
		Satisfaction->SetText(LOCTEXT("EmptySatisfaction", "—")); Safety->SetText(LOCTEXT("EmptySafety", "—"));
		Card(BP, Tree, *Metrics, TEXT("Revenue"), LOCTEXT("Revenue", "오늘 매출"), TEXT("Metrics/T_FF_LH_Icon_Revenue_White_256"), *Revenue);
		Card(BP, Tree, *Metrics, TEXT("Gold"), LOCTEXT("Gold", "보유 금화"), TEXT("Metrics/T_FF_LH_Icon_Gold_White_256"), *Gold);
		UVerticalBox* SatisfactionCard = Card(BP, Tree, *Metrics, TEXT("Satisfaction"), LOCTEXT("Satisfaction", "관객 만족도"), TEXT("Metrics/T_FF_LH_Icon_Satisfaction_White_256"), *Satisfaction);
		Row(*SatisfactionCard, *Text(BP, Tree, TEXT("Text_SatisfactionNote"), LOCTEXT("SatisfactionNote", "현재 만족도"), 15, Muted, true));
		UVerticalBox* SafetyCard = Card(BP, Tree, *Metrics, TEXT("Safety"), LOCTEXT("Safety", "안전"), TEXT("Metrics/T_FF_LH_Icon_Safety_White_256"), *Safety);
		Row(*SafetyCard, *Text(BP, Tree, TEXT("Ops_SafetyNote"), LOCTEXT("SafetyNote", "집계 준비 중"), 15, Muted));

		UBorder* GoalFrame = MakeWidget<UBorder>(BP, Tree, TEXT("Ops_GoalBorder"));
		GoalFrame->SetBrush(Brush(Paper, 7.f, 1.f, SkyLine)); GoalFrame->SetPadding(FMargin(18.f, 14.f));
		UVerticalBox* GoalBody = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Ops_GoalBody")); GoalFrame->AddChild(GoalBody);
		Row(*Content, *GoalFrame, FMargin(0.f, 0.f, 0.f, 16.f));
		Row(*GoalBody, *Text(BP, Tree, TEXT("Ops_GoalTitle"), LOCTEXT("GoalTitle", "일일 목표"), 22, Ink));
		Row(*GoalBody, *Rule(BP, Tree, TEXT("Ops_GoalDivider")), FMargin(0.f, 8.f, 0.f, 10.f));
		UHorizontalBox* GoalNumbers = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Ops_GoalNumbers"));
		StyleText(*Goal, 23, Ink, TEXT("SemiBold")); Goal->SetAutoWrapText(true);
		Goal->SetText(LOCTEXT("UnknownGoal", "집계 준비 중"));
		Column(*GoalNumbers, *Goal, FMargin(0.f, 0.f, 12.f, 0.f), true);
		Column(*GoalNumbers, *Text(BP, Tree, TEXT("Text_GoalPercent"), LOCTEXT("UnknownPercent", "—"), 21, Ink, true));
		Row(*GoalBody, *GoalNumbers, FMargin(0.f, 0.f, 0.f, 12.f));
		USizeBox* ProgressSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Ops_GoalProgressSize"));
		ProgressSize->SetHeightOverride(18.f); ProgressSize->AddChild(Progress);
		FProgressBarStyle ProgressStyle = Progress->GetWidgetStyle();
		ProgressStyle.BackgroundImage = Brush(SkyWash, 8.f, 1.f, SkyLine);
		ProgressStyle.FillImage = Brush(FLinearColor::White, 8.f);
		Progress->SetWidgetStyle(ProgressStyle); Progress->SetFillColorAndOpacity(Green);
		Progress->SetPercent(0.f); Progress->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Row(*GoalBody, *ProgressSize, FMargin(0.f, 0.f, 0.f, 10.f));
		StyleText(*Detail, 17, Muted, TEXT("Medium")); Detail->SetAutoWrapText(true);
		Detail->SetText(LOCTEXT("UnknownDetail", "오늘 입장객과 목표를 확인하고 있습니다."));
		Row(*GoalBody, *Detail);
		Row(*Content, *Warning); Warning->SetVisibility(ESlateVisibility::Collapsed);

		UBorder* Footer = MakeWidget<UBorder>(BP, Tree, TEXT("Ops_InfoBorder"));
		Footer->SetBrush(Brush(SkyWash, 6.f, 1.f, SkyLine)); Footer->SetPadding(FMargin(16.f, 10.f));
		StyleText(*Notice, 16, Muted, TEXT("Medium")); Notice->SetAutoWrapText(true);
		Notice->SetText(LOCTEXT("Notice", "관객 현황과 대기열은 각 탭에서 확인할 수 있습니다."));
		Footer->AddChild(Notice); Row(*Content, *Footer);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->ParentClass->IsChildOf(UFF_LivePanelOperations::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1"))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer 편집 보존"), LogPrefix); return;
		}
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveOperationsSky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK) return;
		if (!Build(*BP) || !SaveNewBlueprint(LogPrefix, *BP))
		{
			BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 운영 패널 적용 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 운영 패널 저장 완료"), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveOperationsSkyDesign"),
		TEXT("P2 운영 패널의 카드와 목표 영역을 최초 한 번 적용한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
