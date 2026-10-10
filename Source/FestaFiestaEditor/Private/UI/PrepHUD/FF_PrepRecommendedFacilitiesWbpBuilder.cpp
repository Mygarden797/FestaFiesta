// 기존 점검창에 권장 시설 안내만 추가한다. 최초 저장 후 디자인은 WBP가 소유한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
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
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UI/PrepHUD/Panels/FF_PrepRecommendedFacilityRow.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaRecommendedFacilities"

namespace FFPrepRecommendedBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_PREP_RECOMMENDED");
	constexpr const TCHAR* VersionKey = TEXT("FF_PrepRecommendedFacilitiesVersion");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels");
	const FString PanelPackage = Folder / TEXT("WBP_FF_PrepPanelCheck");
	const FString RowPackage = Folder / TEXT("WBP_FF_PrepRecommendedFacilityRow");
	const FString Icons = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/");
	const FLinearColor Amber = Color(196, 113, 12), AmberLine = Color(238, 207, 155), AmberWash = Color(255, 250, 238);

	USizeBox* Sized(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, UWidget& Child, float Width, float Height)
	{
		auto* Box = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) Box->SetWidthOverride(Width);
		if (Height > 0.f) Box->SetMinDesiredHeight(Height);
		Box->AddChild(&Child);
		return Box;
	}

	FSlateBrush IconBrush(const TCHAR* Path)
	{
		FSlateBrush Result;
		const FString Package = FString(Path).StartsWith(TEXT("/Game/")) ? FString(Path) : Icons + Path;
		Result.SetResourceObject(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Package)));
		Result.ImageSize = FVector2D(32.f);
		Result.DrawAs = ESlateBrushDrawType::Image;
		Result.TintColor = Ink;
		return Result;
	}

	bool MakeRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		auto* Root = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Row"));
		Tree.RootWidget = Root;
		auto* Line = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Separator"));
		Line->SetBrush(Brush(AmberLine)); Line->SetPadding(FMargin(0.f));
		Row(*Root, *Sized(BP, Tree, TEXT("SizeBox_Separator"), *Line, 0.f, 1.f));
		auto* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Content"));
		Row(*Root, *Sized(BP, Tree, TEXT("SizeBox_Row"), *Content, 0.f, 78.f), FMargin(18.f, 8.f));
		auto* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_Icon"), true);
		Icon->SetBrush(IconBrush(TEXT("Needs/T_FF_LH_Icon_NeedFood_White_256")));
		Column(*Content, *Sized(BP, Tree, TEXT("SizeBox_Icon"), *Icon, 30.f, 30.f), FMargin(0.f, 0.f, 18.f, 0.f));
		auto* Copy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Copy"));
		auto* Title = Text(BP, Tree, TEXT("Text_Title"), LOCTEXT("RowTitle", "음식 시설 없음"), 18, Ink, true);
		Title->SetAutoWrapText(true); StyleText(*Title, 18, Ink, TEXT("Bold"));
		Row(*Copy, *Title, FMargin(0.f, 0.f, 0.f, 4.f));
		auto* Description = Text(BP, Tree, TEXT("Text_Description"), LOCTEXT("RowDescription", "관객이 음식을 사 먹을 곳이 없습니다."), 15, Muted, true);
		Description->SetAutoWrapText(true); Row(*Copy, *Description);
		Column(*Content, *Copy, FMargin(0.f, 0.f, 16.f, 0.f), true);
		auto* Action = Text(BP, Tree, TEXT("Text_Action"), LOCTEXT("RowAction", "음식 배치하기 ›"), 17, Ink, true);
		Action->SetJustification(ETextJustify::Center);
		Column(*Content, *Sized(BP, Tree, TEXT("SizeBox_Action"), *Button(BP, Tree, TEXT("Button_Place"), *Action), 220.f, 44.f));
		for (UWidget* Child : Content->GetAllChildren()) CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetVerticalAlignment(VAlign_Center);
		return true;
	}

	struct FEntry
	{
		EFF_PrepRecommendedFacilityKind Kind;
		const TCHAR* Name;
		FText Title, Description, Action;
		const TCHAR* Icon;
	};

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		auto* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->ParentClass->IsChildOf(UFF_PrepPanelCheck::StaticClass())) return;
		UPackage* Package = BP->GetOutermost();
		if (Package->GetMetaData().GetValue(BP, VersionKey) == TEXT("1"))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 기존 디자인을 보존합니다."), Prefix); return;
		}
		UWidgetTree& Tree = *BP->WidgetTree;
		auto* Body = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Body")));
		auto* Footer = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Footer")));
		if (!Body || !Footer || Footer->GetChildrenCount() < 2 || Package->IsDirty() || Tree.FindWidget(TEXT("Border_RecommendedFacilities")))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 점검 원본 구조 또는 미저장 상태를 확인하세요."), Prefix); return;
		}
		const FString Source = FPackageName::LongPackageNameToFilename(PanelPackage, FPackageName::GetAssetPackageExtension());
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/PrepRecommended") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(Source)), *Source, false, false) != COPY_OK) return;
		auto* RowBP = EnsureWidgetBlueprint(Prefix, RowPackage, Folder, *UFF_PrepRecommendedFacilityRow::StaticClass(), MakeRow);
		if (!RowBP || !RowBP->GeneratedClass) return;
		BP->Modify(); Tree.Modify(); Body->Modify(); Footer->Modify();
		auto* Warning = MakeWidget<UBorder>(*BP, Tree, TEXT("Border_RecommendedFacilities"), true);
		Warning->SetBrush(Brush(AmberWash, 6.f, 1.f, AmberLine)); Warning->SetPadding(FMargin(1.f));
		auto* WarningBody = MakeWidget<UVerticalBox>(*BP, Tree, TEXT("VerticalBox_RecommendedContent"));
		Warning->AddChild(WarningBody);
		Row(*Body, *Warning, FMargin(0.f, 18.f, 0.f, 0.f));
		auto* Heading = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("HorizontalBox_RecommendedHeading"));
		auto* WarningIcon = MakeWidget<UImage>(*BP, Tree, TEXT("Image_RecommendedWarning"));
		auto WarningBrush = IconBrush(TEXT("Status/T_FF_LH_Icon_StatusWarning_White_256")); WarningBrush.TintColor = Amber;
		WarningIcon->SetBrush(WarningBrush);
		Column(*Heading, *Sized(*BP, Tree, TEXT("SizeBox_WarningIcon"), *WarningIcon, 28.f, 28.f), FMargin(0.f, 0.f, 12.f, 0.f));
		Column(*Heading, *Text(*BP, Tree, TEXT("Text_RecommendedHeading"), LOCTEXT("Heading", "개장 전 확인"), 22), FMargin(0.f, 0.f, 20.f, 0.f));
		Column(*Heading, *Text(*BP, Tree, TEXT("Text_RecommendedCount"), LOCTEXT("Count", "권장 시설 8종 없음"), 16, Amber, true));
		for (UWidget* Child : Heading->GetAllChildren()) CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(*WarningBody, *Heading, FMargin(18.f, 16.f, 18.f, 8.f));
		auto* Hint = Text(*BP, Tree, TEXT("Text_RecommendedHint"), LOCTEXT("Hint", "아래 시설이 없는 상태입니다. 이대로 운영을 시작할까요?"), 16, Muted);
		Hint->SetAutoWrapText(true); Row(*WarningBody, *Hint, FMargin(58.f, 0.f, 18.f, 16.f));
		auto* Rows = MakeWidget<UVerticalBox>(*BP, Tree, TEXT("VerticalBox_RecommendedRows"), true);
		Row(*WarningBody, *Rows);
		const FEntry Entries[] = {
			{ EFF_PrepRecommendedFacilityKind::Lighting, TEXT("Lighting"), LOCTEXT("LightingTitle", "조명 시설 없음"), LOCTEXT("LightingDescription", "밤이 되면 축제장이 매우 어두워집니다."), LOCTEXT("LightingAction", "조명 배치하기 ›"), TEXT("/Game/FestaFiesta/UI/PrepHUD/Icons/T_FF_PrepRecommended_Lighting") },
			{ EFF_PrepRecommendedFacilityKind::Food, TEXT("Food"), LOCTEXT("FoodTitle", "음식 시설 없음"), LOCTEXT("FoodDescription", "관객이 음식을 사 먹을 곳이 없습니다."), LOCTEXT("FoodAction", "음식 배치하기 ›"), TEXT("Facilities/T_FF_LH_Icon_FacilityFood_White_256") },
			{ EFF_PrepRecommendedFacilityKind::Toilet, TEXT("Toilet"), LOCTEXT("ToiletTitle", "화장실 없음"), LOCTEXT("ToiletDescription", "관객이 이용할 화장실이 없습니다."), LOCTEXT("ToiletAction", "화장실 배치하기 ›"), TEXT("Needs/T_FF_LH_Icon_NeedRestroom_White_256") },
			{ EFF_PrepRecommendedFacilityKind::Entertainment, TEXT("Entertainment"), LOCTEXT("EntertainmentTitle", "즐길거리 시설 없음"), LOCTEXT("EntertainmentDescription", "메인 공연 외에 즐길 시설이 없습니다."), LOCTEXT("EntertainmentAction", "즐길거리 배치하기 ›"), TEXT("Needs/T_FF_LH_Icon_NeedEntertainment_White_256") },
			{ EFF_PrepRecommendedFacilityKind::Rest, TEXT("Rest"), LOCTEXT("RestTitle", "휴게 시설 없음"), LOCTEXT("RestDescription", "쉴 수 있는 좌석과 그늘 공간이 없습니다."), LOCTEXT("RestAction", "휴게 시설 배치하기 ›"), TEXT("Needs/T_FF_LH_Icon_NeedRest_White_256") },
			{ EFF_PrepRecommendedFacilityKind::Drink, TEXT("Drink"), LOCTEXT("DrinkTitle", "음료 시설 없음"), LOCTEXT("DrinkDescription", "음료를 판매하는 시설이 없습니다."), LOCTEXT("DrinkAction", "음료 배치하기 ›"), TEXT("Facilities/T_FF_LH_Icon_FacilityDrink_White_256") },
			{ EFF_PrepRecommendedFacilityKind::FirstAid, TEXT("FirstAid"), LOCTEXT("FirstAidTitle", "응급 시설 없음"), LOCTEXT("FirstAidDescription", "응급처치 시설이 배치되어 있지 않습니다."), LOCTEXT("FirstAidAction", "응급 시설 배치하기 ›"), TEXT("/Game/FestaFiesta/UI/PrepHUD/Icons/T_FF_PrepRecommended_FirstAid") },
			{ EFF_PrepRecommendedFacilityKind::FireSafety, TEXT("FireSafety"), LOCTEXT("FireSafetyTitle", "소방 시설 없음"), LOCTEXT("FireSafetyDescription", "소방 장비와 소방 시설이 배치되어 있지 않습니다."), LOCTEXT("FireSafetyAction", "소방 시설 배치하기 ›"), TEXT("/Game/FestaFiesta/UI/PrepHUD/Icons/T_FF_PrepRecommended_FireSafety") }
		};
		for (const auto& Entry : Entries)
		{
			auto* Item = CastChecked<UFF_PrepRecommendedFacilityRow>(MakeWidgetOfClass(*BP, Tree, FName(FString(TEXT("RecommendedRow_")) + Entry.Name), RowBP->GeneratedClass));
			Item->Kind = Entry.Kind; Item->TitleText = Entry.Title; Item->DescriptionText = Entry.Description;
			Item->ActionText = Entry.Action; Item->IconBrush = IconBrush(Entry.Icon);
			Row(*Rows, *Item);
		}
		auto* Summary = Text(*BP, Tree, TEXT("Text_RecommendedSummary"), LOCTEXT("Summary", "⚠ 권장 시설 8종이 없습니다"), 16, Amber, true);
		Summary->SetAutoWrapText(true);
		// 원래 사유와 실행 버튼 슬롯은 보존하고 그 사이에 새 경고 한 줄만 넣는다.
		auto* Slot = CastChecked<UVerticalBoxSlot>(Footer->InsertChildAt(1, Summary));
		Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f));
		Package->GetMetaData().SetValue(BP, VersionKey, TEXT("1"));
		if (!SaveNewBlueprint(Prefix, *BP)) { Package->GetMetaData().RemoveValue(BP, VersionKey); return; }
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 권장 시설 8종 추가. 백업=%s"), Prefix, *Backup);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepRecommendedFacilities"),
		TEXT("점검 WBP에 권장 시설 안내를 한 번 추가한다. 기존 편집값을 보존한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
