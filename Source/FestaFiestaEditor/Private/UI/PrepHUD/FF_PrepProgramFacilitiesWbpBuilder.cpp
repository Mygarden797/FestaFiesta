// 프로그램 시설 목록을 설치 후에도 유지하고, 설치된 개체를 번호·위치로 고르는 편집 가능한 위젯을 추가한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
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
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCatalogChoice.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramFacilityRow.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepProgramFacilities"

namespace FFPrepProgramFacilitiesBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_PREP_PROGRAM_FACILITIES");
	constexpr const TCHAR* VersionKey = TEXT("FF_PrepProgramFacilitiesVersion");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString PanelPackage = Folder / TEXT("Panels/WBP_FF_PrepPanelProgram");
	const FString ChoicePackage = Folder / TEXT("WBP_FF_PrepProgramCatalogChoice");
	const FString RowPackage = Folder / TEXT("WBP_FF_PrepProgramFacilityRow");
	const FString Icons = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/");

	bool HasVersion(UWidgetBlueprint& BP)
	{
		return BP.GetOutermost()->GetMetaData().GetValue(&BP, VersionKey) == TEXT("1");
	}

	bool SaveVersion(UWidgetBlueprint& BP)
	{
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		if (SaveNewBlueprint(Prefix, BP)) return true;
		BP.GetOutermost()->GetMetaData().RemoveValue(&BP, VersionKey);
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 저장 실패: %s"), Prefix, *BP.GetPathName());
		return false;
	}

	bool BackupPackage(const FString& PackageName, const FString& BackupDirectory)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().Copy(*(BackupDirectory / FPaths::GetCleanFilename(Filename)), *Filename, false, false) == COPY_OK) return true;
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 백업 실패: %s"), Prefix, *Filename);
		return false;
	}

	void BindExisting(UWidgetBlueprint& BP, UWidget& Widget)
	{
		Widget.Modify();
		Widget.bIsVariable = true;
		// 기존 비변수 위젯도 GUID는 이미 등록되어 있을 수 있다. 재등록하면 그래프 참조가 달라진다.
		if (!BP.WidgetVariableNameToGuidMap.Contains(Widget.GetFName())) BP.OnVariableAdded(Widget.GetFName());
	}

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize, FLinearColor Tint, bool bVariable = true)
	{
		auto* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	void CenterChildren(UHorizontalBox& Box)
	{
		for (UWidget* Child : Box.GetAllChildren())
			CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetVerticalAlignment(VAlign_Center);
	}

	UImage* Icon(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, const TCHAR* RelativePath, float Size)
	{
		auto* Result = MakeWidget<UImage>(BP, Tree, Name);
		FSlateBrush ImageBrush;
		ImageBrush.SetResourceObject(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons + RelativePath)));
		ImageBrush.DrawAs = ESlateBrushDrawType::Image;
		ImageBrush.ImageSize = FVector2D(Size);
		Result->SetBrush(ImageBrush); // 브러시 크기를 저장해 NullRHI 제작 후에도 아이콘이 보이게 한다.
		Result->SetColorAndOpacity(Ink);
		return Result;
	}

	bool BuildFacilityRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		auto* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_FacilityRow"));
		Size->SetMinDesiredHeight(66.f);
		Tree.RootWidget = Size;
		auto* Selection = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Selection"), true);
		Selection->SetBrush(Brush(Paper, 6.f, 1.f, SkyLine));
		Selection->SetPadding(FMargin(0.f));
		Size->AddChild(Selection);
		auto* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_FacilityRow"));
		Selection->AddChild(Content);
		auto* SelectContent = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Select"));
		auto* Mark = Text(BP, Tree, TEXT("Text_SelectionMark"), FText::FromString(TEXT("○")), 16, Muted, true);
		Column(*SelectContent, *Mark, FMargin(0.f, 0.f, 7.f, 0.f));
		auto* Copy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_FacilityCopy"));
		Row(*Copy, *Wrapped(BP, Tree, TEXT("Text_FacilityName"), LOCTEXT("FacilityNamePreview", "모래공방 #01"), 15, Ink), FMargin(0.f, 0.f, 0.f, 3.f));
		Row(*Copy, *Wrapped(BP, Tree, TEXT("Text_Location"), LOCTEXT("LocationPreview", "위치 확인"), 12, Muted));
		auto* Status = Wrapped(BP, Tree, TEXT("Text_Status"), LOCTEXT("StatusPreview", "✓ 오늘 편성 완료"), 12, Green);
		Status->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Copy, *Status, FMargin(0.f, 3.f, 0.f, 0.f));
		Column(*SelectContent, *Copy, FMargin(0.f), true);
		CenterChildren(*SelectContent);
		auto* Select = Button(BP, Tree, TEXT("Button_Select"), *SelectContent);
		FButtonStyle SelectStyle = Select->GetStyle();
		SelectStyle.Normal = Brush(FLinearColor::Transparent, 6.f);
		SelectStyle.Hovered = Brush(FLinearColor::Transparent, 6.f);
		SelectStyle.Pressed = Brush(FLinearColor::Transparent, 6.f);
		SelectStyle.Disabled = Brush(FLinearColor::Transparent, 6.f);
		SelectStyle.NormalPadding = SelectStyle.PressedPadding = FMargin(9.f, 8.f);
		Select->SetStyle(SelectStyle);
		CastChecked<UButtonSlot>(SelectContent->Slot)->SetHorizontalAlignment(HAlign_Fill);
		Column(*Content, *Select, FMargin(0.f), true);
		auto* LocateCopy = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Locate"));
		Column(*LocateCopy, *Icon(BP, Tree, TEXT("Image_Locate"), TEXT("Actions/T_FF_LH_Icon_Locate_White_256"), 14.f), FMargin(0.f, 0.f, 3.f, 0.f));
		Column(*LocateCopy, *Text(BP, Tree, TEXT("Text_Locate"), LOCTEXT("Locate", "위치 보기"), 12, Ink));
		CenterChildren(*LocateCopy);
		auto* Locate = Button(BP, Tree, TEXT("Button_Locate"), *LocateCopy);
		FButtonStyle LocateStyle = Locate->GetStyle();
		for (FSlateBrush* State : { &LocateStyle.Normal, &LocateStyle.Hovered, &LocateStyle.Pressed, &LocateStyle.Disabled })
			State->TintColor = FLinearColor::Transparent;
		LocateStyle.NormalPadding = LocateStyle.PressedPadding = FMargin(6.f, 4.f);
		Locate->SetStyle(LocateStyle);
		Locate->SetToolTipText(LOCTEXT("LocateTooltip", "시설 위치를 확인합니다. 프로그램을 다시 열면 선택이 유지됩니다."));
		auto* LocateSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Locate"));
		LocateSize->SetMinDesiredWidth(76.f);
		LocateSize->SetMinDesiredHeight(32.f);
		LocateSize->AddChild(Locate);
		Column(*Content, *LocateSize, FMargin(4.f, 0.f, 8.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(LocateSize->Slot)->SetVerticalAlignment(VAlign_Center);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void AddInstallationStatus(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		auto* Root = CastChecked<UButton>(Tree.FindWidget(TEXT("Button_Root")));
		auto* Copy = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Copy")));
		auto* Label = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_Label")));
		auto* Chevron = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_Chevron")));
		BP.Modify(); Tree.Modify(); Root->Modify(); Copy->Modify(); Label->Modify(); Chevron->Modify();
		FButtonStyle Style = Root->GetStyle();
		// 여백·둥글기·테두리 폭은 보존하고 기본색만 셋업 공통색으로 바꾼다.
		Style.Normal.TintColor = Paper;
		Style.Normal.OutlineSettings.Color = SkyLine;
		Style.Hovered.TintColor = SkyWash;
		Style.Hovered.OutlineSettings.Color = Color(111, 183, 214);
		Style.Pressed.TintColor = Color(229, 245, 252);
		Style.Pressed.OutlineSettings.Color = Color(91, 166, 199);
		Root->SetStyle(Style);
		Chevron->SetColorAndOpacity(Ink);
		const int32 LabelIndex = Copy->GetChildIndex(Label);
		const FMargin LabelPadding = CastChecked<UVerticalBoxSlot>(Label->Slot)->GetPadding();
		Copy->RemoveChild(Label);
		auto* Heading = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_InstallationHeading"));
		Column(*Heading, *Label, FMargin(0.f, 0.f, 8.f, 0.f), true);
		auto* Status = MakeWidget<UBorder>(BP, Tree, TEXT("Border_InstallationStatus"), true);
		Status->SetBrush(Brush(SkyWash, 4.f, 1.f, SkyLine));
		Status->SetPadding(FMargin(7.f, 3.f));
		Status->AddChild(Text(BP, Tree, TEXT("Text_InstallationStatus"), LOCTEXT("InstallationPreview", "미설치"), 12, Muted, true));
		Column(*Heading, *Status);
		CenterChildren(*Heading);
		auto* HeadingSlot = CastChecked<UVerticalBoxSlot>(Copy->InsertChildAt(LabelIndex, Heading));
		HeadingSlot->SetHorizontalAlignment(HAlign_Fill);
		HeadingSlot->SetPadding(LabelPadding);
	}

	void StyleScroll(UScrollBox& Scroll)
	{
		Scroll.SetScrollbarThickness(FVector2D(5.f));
		Scroll.SetScrollbarPadding(FMargin(5.f, 0.f, 0.f, 0.f));
		Scroll.SetAlwaysShowScrollbar(false);
		Scroll.SetAllowOverscroll(false);
		Scroll.SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll.SetClipping(EWidgetClipping::ClipToBounds);
		FScrollBarStyle Bar = Scroll.GetWidgetBarStyle();
		Bar.VerticalBackgroundImage = Brush(Color(234, 241, 242), 2.f);
		Bar.HorizontalBackgroundImage = Brush(Color(234, 241, 242), 2.f);
		for (FSlateBrush* Segment : {&Bar.VerticalTopSlotImage, &Bar.VerticalBottomSlotImage,
			&Bar.HorizontalTopSlotImage, &Bar.HorizontalBottomSlotImage})
			*Segment = Brush(FLinearColor::Transparent);
		Bar.NormalThumbImage = Brush(Color(136, 185, 200), 2.f);
		Bar.HoveredThumbImage = Brush(Color(110, 166, 184), 2.f);
		Bar.DraggedThumbImage = Brush(Color(90, 149, 168), 2.f);
		Scroll.SetWidgetBarStyle(Bar);
		FScrollBoxStyle Style = Scroll.GetWidgetStyle();
		Style.TopShadowBrush = Brush(FLinearColor::Transparent);
		Style.BottomShadowBrush = Brush(FLinearColor::Transparent);
		Scroll.SetWidgetStyle(Style);
	}

	void AddFacilityInstances(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		auto* Guide = CastChecked<UBorder>(Tree.FindWidget(TEXT("Border_FacilityGuide")));
		auto* Title = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_FacilityGuideTitle")));
		auto* GuideIcon = CastChecked<UImage>(Tree.FindWidget(TEXT("Image_FacilityGuideWarning")));
		auto* Facility = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("Box_FacilityRow")));
		auto* Label = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_FacilityLabel")));
		auto* Combo = CastChecked<UComboBoxString>(Tree.FindWidget(TEXT("Combo_Facilities")));
		BP.Modify(); Tree.Modify(); Guide->Modify(); Title->Modify(); GuideIcon->Modify(); Facility->Modify(); Combo->Modify();
		FSlateBrush GuideBrush = Guide->Background;
		GuideBrush.TintColor = SkyWash;
		GuideBrush.OutlineSettings.Color = SkyLine;
		Guide->SetBrush(GuideBrush);
		Title->SetText(LOCTEXT("FacilityGuideTitle", "프로그램 운영 시설"));
		Title->SetColorAndOpacity(Ink);
		FSlateBrush GuideIconBrush = GuideIcon->GetBrush();
		GuideIconBrush.SetResourceObject(LoadObject<UTexture2D>(nullptr,
			*MakeObjectPath(Icons + TEXT("Navigation/T_FF_LH_Icon_Facilities_White_256"))));
		GuideIcon->SetBrush(GuideIconBrush);
		GuideIcon->SetColorAndOpacity(Ink);
		BindExisting(BP, *GuideIcon);
		BindExisting(BP, *Label);
		Combo->SetVisibility(ESlateVisibility::Collapsed); // 기존 데이터 바인딩/디자이너 설정은 남겨 둔다.
		auto* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_FacilityInstances"));
		Size->SetMaxDesiredHeight(240.f);
		auto* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("ScrollBox_FacilityInstances"), true);
		StyleScroll(*Scroll);
		auto* Rows = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_FacilityInstances"), true);
		Scroll->AddChild(Rows);
		Size->AddChild(Scroll);
		Row(*Facility, *Size);
		auto* Empty = Wrapped(BP, Tree, TEXT("Text_FacilityEmpty"), LOCTEXT("Empty", "편성 가능한 시설이 없습니다."), 13, Muted);
		Empty->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Facility, *Empty, FMargin(0.f, 5.f, 0.f, 0.f));
		Row(*Facility, *Wrapped(BP, Tree, TEXT("Text_FacilitySelectionHint"),
			LOCTEXT("FacilitySelectionHint", "같은 시설도 번호와 위치로 구분해 선택하세요."), 12, Muted, false), FMargin(0.f, 7.f, 0.f, 0.f));
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), Prefix); return;
		}
		auto* Panel = LoadExistingBlueprint(PanelPackage);
		auto* Choice = LoadExistingBlueprint(ChoicePackage);
		auto* ExistingRow = LoadExistingBlueprint(RowPackage);
		if (!Panel || !Choice || !Panel->WidgetTree || !Choice->WidgetTree ||
			!Panel->ParentClass || !Panel->ParentClass->IsChildOf(UFF_PrepPanelProgram::StaticClass()) ||
			!Choice->ParentClass || !Choice->ParentClass->IsChildOf(UFF_PrepProgramCatalogChoice::StaticClass()) ||
			(ExistingRow && (!ExistingRow->ParentClass || !ExistingRow->ParentClass->IsChildOf(UFF_PrepProgramFacilityRow::StaticClass()))))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 프로그램 패널/시설 선택 원본 또는 부모 클래스가 다릅니다."), Prefix); return;
		}
		const bool bPanelDone = HasVersion(*Panel), bChoiceDone = HasVersion(*Choice);
		if (bPanelDone && bChoiceDone && ExistingRow)
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 적용된 디자이너 편집을 보존합니다."), Prefix); return;
		}
		UWidgetTree& PanelTree = *Panel->WidgetTree;
		UWidgetTree& ChoiceTree = *Choice->WidgetTree;
		auto* Facility = Cast<UVerticalBox>(PanelTree.FindWidget(TEXT("Box_FacilityRow")));
		auto* Combo = Cast<UComboBoxString>(PanelTree.FindWidget(TEXT("Combo_Facilities")));
		auto* Label = Cast<UTextBlock>(ChoiceTree.FindWidget(TEXT("Text_Label")));
		auto* Copy = Cast<UVerticalBox>(ChoiceTree.FindWidget(TEXT("VerticalBox_Copy")));
		const bool bPanelValid = bPanelDone || (Facility && Combo && Combo->GetParent() == Facility &&
			Cast<UBorder>(PanelTree.FindWidget(TEXT("Border_FacilityGuide"))) &&
			Cast<UTextBlock>(PanelTree.FindWidget(TEXT("Text_FacilityGuideTitle"))) &&
			Cast<UTextBlock>(PanelTree.FindWidget(TEXT("Text_FacilityLabel"))) &&
			Cast<UImage>(PanelTree.FindWidget(TEXT("Image_FacilityGuideWarning"))) &&
			!PanelTree.FindWidget(TEXT("VerticalBox_FacilityInstances")) && !Panel->GetOutermost()->IsDirty());
		const bool bChoiceValid = bChoiceDone || (Label && Copy && Label->GetParent() == Copy &&
			Cast<UButton>(ChoiceTree.FindWidget(TEXT("Button_Root"))) &&
			Cast<UTextBlock>(ChoiceTree.FindWidget(TEXT("Text_Chevron"))) &&
			!ChoiceTree.FindWidget(TEXT("Border_InstallationStatus")) && !Choice->GetOutermost()->IsDirty());
		if (!bPanelValid || !bChoiceValid)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 구조가 다르거나 미저장 변경이 있습니다. 자동 교체하지 않습니다."), Prefix); return;
		}
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/PrepProgramFacilities") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) ||
			(!bPanelDone && !BackupPackage(PanelPackage, Backup)) ||
			(!bChoiceDone && !BackupPackage(ChoicePackage, Backup))) return;
		auto* FacilityRow = EnsureWidgetBlueprint(Prefix, RowPackage, Folder, *UFF_PrepProgramFacilityRow::StaticClass(), BuildFacilityRow);
		if (!FacilityRow || !FacilityRow->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 설치 시설 선택 행 생성 실패."), Prefix); return;
		}
		if (!bChoiceDone)
		{
			AddInstallationStatus(*Choice);
			if (!SaveVersion(*Choice)) return;
		}
		if (!bPanelDone)
		{
			AddFacilityInstances(*Panel);
			if (!SaveVersion(*Panel)) return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 시설 종류별 설치 상태·개별 시설 선택·위치 보기 추가. 백업=%s"), Prefix, *Backup);
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepProgramFacilities"),
		TEXT("프로그램 WBP에 설치 상태와 번호·위치로 구분하는 시설 선택 행을 한 번 추가한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
