// 프로그램 창의 기존 디자이너 편집을 보존하면서 시설 안내와 편성 피드백만 추가한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCard.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCatalogChoice.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepProgramFeedback"

namespace FFPrepProgramFeedbackBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_PREP_PROGRAM_FEEDBACK");
	constexpr const TCHAR* VersionKey = TEXT("FF_PrepProgramFeedbackVersion");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString PanelPackage = Folder / TEXT("Panels/WBP_FF_PrepPanelProgram");
	const FString CardPackage = Folder / TEXT("WBP_FF_PrepProgramCard");
	const FString ChoicePackage = Folder / TEXT("WBP_FF_PrepProgramCatalogChoice");
	const FString Icons = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/");
	const FLinearColor Amber = Color(184, 111, 20), AmberLine = Color(225, 181, 110);
	const FLinearColor AmberWash = Color(255, 249, 236), Mint = Color(232, 247, 238);
	const FLinearColor MintLine = Color(151, 202, 172);

	USizeBox* Sized(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, UWidget& Child, float Width, float Height)
	{
		auto* Result = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) Result->SetWidthOverride(Width);
		if (Height > 0.f) Result->SetMinDesiredHeight(Height);
		Result->AddChild(&Child);
		return Result;
	}

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize, FLinearColor Tint, bool bVariable = true)
	{
		auto* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	UImage* Icon(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, const TCHAR* RelativePath,
		FLinearColor Tint, bool bVariable = false)
	{
		auto* Result = MakeWidget<UImage>(BP, Tree, Name, bVariable);
		Result->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons + RelativePath)));
		Result->SetDesiredSizeOverride(FVector2D(24.f));
		Result->SetColorAndOpacity(Tint);
		return Result;
	}

	void CenterChildren(UHorizontalBox& Box)
	{
		for (UWidget* Child : Box.GetAllChildren())
			CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetVerticalAlignment(VAlign_Center);
	}

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

	bool BuildCatalogChoice(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		auto* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Choice"));
		Size->SetMinDesiredHeight(66.f); // 폭은 부모의 2열 그리드가 결정한다.
		Tree.RootWidget = Size;
		auto* Overlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Choice"));
		Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Size->AddChild(Overlay);
		auto* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Content"));
		auto* Root = Button(BP, Tree, TEXT("Button_Root"), *Content);
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = Brush(Paper, 7.f, 1.25f, AmberLine);
		Style.Hovered = Brush(AmberWash, 7.f, 2.f, Amber);
		Style.Pressed = Brush(Color(255, 240, 209), 7.f, 2.f, Amber);
		Style.NormalPadding = Style.PressedPadding = FMargin(14.f, 10.f);
		Root->SetStyle(Style);
		CastChecked<UButtonSlot>(Content->Slot)->SetHorizontalAlignment(HAlign_Fill);
		auto* RootSlot = Overlay->AddChildToOverlay(Root);
		RootSlot->SetHorizontalAlignment(HAlign_Fill);
		RootSlot->SetVerticalAlignment(VAlign_Fill);
		auto* FacilityIcon = Icon(BP, Tree, TEXT("Image_Icon"), TEXT("Navigation/T_FF_LH_Icon_Facilities_White_256"), Ink, true);
		Column(*Content, *Sized(BP, Tree, TEXT("SizeBox_Icon"), *FacilityIcon, 26.f, 26.f), FMargin(0.f, 0.f, 12.f, 0.f));
		auto* Copy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Copy"));
		Row(*Copy, *Wrapped(BP, Tree, TEXT("Text_Label"), LOCTEXT("FacilityPreview", "나만의 부채 공방"), 17, Ink), FMargin(0.f, 0.f, 0.f, 3.f));
		Row(*Copy, *Text(BP, Tree, TEXT("Text_Subtitle"), LOCTEXT("CatalogSubtitle", "건축목록에서 보기"), 13, Muted, true));
		Column(*Content, *Copy, FMargin(0.f, 0.f, 10.f, 0.f), true);
		Column(*Content, *Text(BP, Tree, TEXT("Text_Chevron"), FText::FromString(TEXT("›")), 25, Amber, true));
		CenterChildren(*Content);
		auto* Attention = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Attention"), true);
		Attention->SetBrush(Brush(FLinearColor::Transparent, 7.f, 2.5f, Amber));
		Attention->SetPadding(FMargin(0.f));
		Attention->SetVisibility(ESlateVisibility::HitTestInvisible);
		Attention->SetRenderOpacity(0.f);
		auto* AttentionSlot = Overlay->AddChildToOverlay(Attention);
		AttentionSlot->SetHorizontalAlignment(HAlign_Fill);
		AttentionSlot->SetVerticalAlignment(VAlign_Fill);
		return true;
	}

	void AddCardBadge(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		auto* Content = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Card")));
		BP.Modify(); Tree.Modify(); Content->Modify();
		auto* Badge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_CompletionBadge"), true);
		Badge->SetBrush(Brush(Mint, 5.f, 1.f, MintLine));
		Badge->SetPadding(FMargin(9.f, 6.f));
		Badge->SetVisibility(ESlateVisibility::Collapsed);
		Badge->AddChild(Wrapped(BP, Tree, TEXT("Text_CompletionBadge"), LOCTEXT("CompleteBadge", "✓ 오늘 1곳 편성 완료"), 14, Green));
		Row(*Content, *Badge, FMargin(0.f, 8.f, 0.f, 0.f));
	}

	bool AddFacilityGuide(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		auto* Body = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Body")));
		auto* Notice = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_FacilityNotice")));
		auto* OldChoices = CastChecked<UPanelWidget>(Tree.FindWidget(TEXT("Box_CatalogChoices")));
		const int32 InsertIndex = Body->GetChildIndex(Notice);
		Notice->Modify(); OldChoices->Modify(); Body->Modify();
		Body->RemoveChild(Notice);
		Body->RemoveChild(OldChoices);
		// 빈 구형 컨테이너만 교체한다. UObject까지 트리 밖으로 옮겨 같은 이름의 잔여 오브젝트를 남기지 않는다.
		if (!OldChoices->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 빈 구형 시설 컨테이너 이동에 실패해 패널을 저장하지 않습니다."), Prefix);
			return false;
		}
		auto* Guide = MakeWidget<UBorder>(BP, Tree, TEXT("Border_FacilityGuide"), true);
		Guide->SetBrush(Brush(AmberWash, 8.f, 1.f, AmberLine));
		Guide->SetPadding(FMargin(14.f, 12.f));
		Guide->SetVisibility(ESlateVisibility::Collapsed);
		auto* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_FacilityGuide"));
		Guide->AddChild(Content);
		auto* Heading = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_FacilityGuideHeading"));
		Column(*Heading, *Sized(BP, Tree, TEXT("SizeBox_FacilityGuideIcon"),
			*Icon(BP, Tree, TEXT("Image_FacilityGuideWarning"), TEXT("Status/T_FF_LH_Icon_StatusWarning_White_256"), Amber), 22.f, 22.f), FMargin(0.f, 0.f, 9.f, 0.f));
		Column(*Heading, *Wrapped(BP, Tree, TEXT("Text_FacilityGuideTitle"), LOCTEXT("FacilityGuideTitle", "프로그램을 운영할 시설이 필요합니다"), 18, Amber), FMargin(0.f), true);
		CenterChildren(*Heading);
		Row(*Content, *Heading, FMargin(0.f, 0.f, 0.f, 5.f));
		Row(*Content, *Notice, FMargin(31.f, 0.f, 0.f, 8.f));
		// UE 5.7 OnVariableAdded는 기존 이름을 다시 등록하면 ensure 후 GUID까지 바꾼다.
		// 위젯 타입만 교체하는 경우 기존 GUID를 그대로 이어받아 그래프 참조를 보존한다.
		const FName ChoicesName(TEXT("Box_CatalogChoices"));
		auto* Choices = Tree.ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), ChoicesName);
		Choices->bIsVariable = true;
		if (!BP.WidgetVariableNameToGuidMap.Contains(ChoicesName)) BP.OnVariableAdded(ChoicesName);
		Choices->SetSlotPadding(FMargin(4.f));
		Choices->SetMinDesiredSlotHeight(66.f);
		Choices->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Content, *Choices);
		auto* GuideSlot = CastChecked<UVerticalBoxSlot>(Body->InsertChildAt(InsertIndex, Guide));
		GuideSlot->SetHorizontalAlignment(HAlign_Fill);
		GuideSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		return true;
	}

	void AddCompletionFeedback(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		auto* Footer = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Footer")));
		auto* Summary = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Summary")));
		Footer->Modify(); Summary->Modify();
		auto* Completion = Wrapped(BP, Tree, TEXT("Text_ProgramCompletion"),
			LOCTEXT("CompletionPreview", "✓ 바다향기 공방 편성 완료 · 다른 프로그램도 편성할 수 있습니다."), 15, Green);
		Completion->SetVisibility(ESlateVisibility::Collapsed);
		CastChecked<UVerticalBoxSlot>(Footer->InsertChildAt(0, Completion))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		auto* Status = Wrapped(BP, Tree, TEXT("Text_SummaryStatus"), LOCTEXT("SummaryStatus", "✓ 오늘 1곳 편성 완료"), 14, Green);
		Status->SetVisibility(ESlateVisibility::Collapsed);
		CastChecked<UVerticalBoxSlot>(Summary->InsertChildAt(1, Status))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		auto* Action = Tree.FindWidget(TEXT("Text_ScheduleAction"));
		Action->Modify(); Action->bIsVariable = true;
		if (!BP.WidgetVariableNameToGuidMap.Contains(Action->GetFName())) BP.OnVariableAdded(Action->GetFName());

		// 기존 루트 이하 슬롯은 건드리지 않는다. 새 캔버스는 크기를 강제하지 않고 부모 패널의 영역을 따른다.
		UWidget* OriginalRoot = Tree.RootWidget;
		auto* Overlay = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_ProgramFeedback"));
		Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Tree.RootWidget = Overlay;
		StretchOnCanvas(*Overlay, *OriginalRoot);
		auto* Toast = MakeWidget<UBorder>(BP, Tree, TEXT("Border_ScheduleToast"), true);
		Toast->SetBrush(Brush(Mint, 8.f, 1.25f, MintLine));
		Toast->SetPadding(FMargin(14.f, 12.f));
		Toast->SetVisibility(ESlateVisibility::Collapsed);
		auto* ToastBody = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_ScheduleToast"));
		Toast->AddChild(ToastBody);
		auto* ToastCopy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_ScheduleToastCopy"));
		Row(*ToastCopy, *Wrapped(BP, Tree, TEXT("Text_ScheduleToastTitle"), LOCTEXT("ToastTitle", "✓ 바다향기 공방 편성 완료"), 17, Green), FMargin(0.f, 0.f, 0.f, 5.f));
		Row(*ToastCopy, *Wrapped(BP, Tree, TEXT("Text_ScheduleToastDetail"), LOCTEXT("ToastDetail", "나만의 부채 공방 · 18:30 ~ 19:00"), 14, Ink));
		Column(*ToastBody, *ToastCopy, FMargin(0.f, 0.f, 10.f, 0.f), true);
		auto* CloseIcon = Icon(BP, Tree, TEXT("Image_DismissScheduleToast"), TEXT("Actions/T_FF_LH_Icon_Close_White_256"), Muted);
		auto* Close = Button(BP, Tree, TEXT("Button_DismissScheduleToast"),
			*Sized(BP, Tree, TEXT("SizeBox_DismissScheduleToastIcon"), *CloseIcon, 14.f, 14.f));
		Close->SetToolTipText(LOCTEXT("Dismiss", "편성 알림 닫기"));
		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.Normal = Brush(FLinearColor::Transparent);
		CloseStyle.Hovered = Brush(Color(215, 237, 223), 4.f);
		CloseStyle.Pressed = Brush(Color(197, 224, 206), 4.f);
		CloseStyle.NormalPadding = CloseStyle.PressedPadding = FMargin(4.f);
		Close->SetStyle(CloseStyle);
		Column(*ToastBody, *Sized(BP, Tree, TEXT("SizeBox_DismissScheduleToast"), *Close, 24.f, 24.f));
		CastChecked<UHorizontalBoxSlot>(Close->GetParent()->Slot)->SetVerticalAlignment(VAlign_Top);
		auto* ToastSize = Sized(BP, Tree, TEXT("SizeBox_ScheduleToast"), *Toast, 340.f, 0.f);
		ToastSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* ToastSlot = Overlay->AddChildToCanvas(ToastSize);
		ToastSlot->SetAnchors(FAnchors(1.f, 0.f));
		ToastSlot->SetAlignment(FVector2D(1.f, 0.f));
		ToastSlot->SetOffsets(FMargin(-24.f, 84.f, 0.f, 0.f));
		ToastSlot->SetAutoSize(true);
		ToastSlot->SetZOrder(10);
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), Prefix); return;
		}
		auto* Panel = LoadExistingBlueprint(PanelPackage);
		auto* Card = LoadExistingBlueprint(CardPackage);
		if (!Panel || !Card || !Panel->WidgetTree || !Card->WidgetTree ||
			!Panel->ParentClass->IsChildOf(UFF_PrepPanelProgram::StaticClass()) ||
			!Card->ParentClass->IsChildOf(UFF_PrepProgramCard::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 프로그램 패널/카드의 원본 또는 부모 클래스가 다릅니다."), Prefix); return;
		}
		const bool bPanelDone = HasVersion(*Panel), bCardDone = HasVersion(*Card);
		if (bPanelDone && bCardDone)
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 기존 디자이너 편집을 보존합니다."), Prefix); return;
		}
		UWidgetTree& Tree = *Panel->WidgetTree;
		auto* Body = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Body")));
		auto* Notice = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_FacilityNotice")));
		auto* Choices = Cast<UPanelWidget>(Tree.FindWidget(TEXT("Box_CatalogChoices")));
		const bool bPanelValid = bPanelDone || (Tree.RootWidget && Body && Notice && Notice->GetParent() == Body && Choices &&
			Choices->GetParent() == Body && Choices->GetChildrenCount() == 0 &&
			Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Footer"))) &&
			Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Summary"))) &&
			Tree.FindWidget(TEXT("Text_ScheduleAction")) && !Tree.FindWidget(TEXT("Border_FacilityGuide")) &&
			!Tree.FindWidget(TEXT("Canvas_ProgramFeedback")) && !Panel->GetOutermost()->IsDirty());
		const bool bCardValid = bCardDone || (Cast<UVerticalBox>(Card->WidgetTree->FindWidget(TEXT("VerticalBox_Card"))) &&
			!Card->WidgetTree->FindWidget(TEXT("Border_CompletionBadge")) && !Card->GetOutermost()->IsDirty());
		if (!bPanelValid || !bCardValid)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 구조가 다르거나 미저장 변경이 있습니다. 자동 교체하지 않습니다."), Prefix); return;
		}
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/PrepProgramFeedback") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) ||
			(!bPanelDone && !BackupPackage(PanelPackage, Backup)) || (!bCardDone && !BackupPackage(CardPackage, Backup))) return;
		const bool bChoiceExisted = LoadExistingBlueprint(ChoicePackage) != nullptr;
		auto* Choice = EnsureWidgetBlueprint(Prefix, ChoicePackage, Folder, *UFF_PrepProgramCatalogChoice::StaticClass(), BuildCatalogChoice);
		if (!Choice || !Choice->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 시설 바로가기 위젯 생성 실패."), Prefix); return;
		}
		if (!bChoiceExisted)
		{
			auto* Defaults = CastChecked<UFF_PrepProgramCatalogChoice>(Choice->GeneratedClass->GetDefaultObject());
			Defaults->SetContentColors(Ink, Ink);
			if (!SaveAssetPackage(Prefix, *Choice)) return;
		}
		if (!bCardDone)
		{
			AddCardBadge(*Card);
			if (!SaveVersion(*Card)) return;
		}
		if (!bPanelDone)
		{
			Panel->Modify(); Tree.Modify();
			if (!AddFacilityGuide(*Panel)) return;
			AddCompletionFeedback(*Panel);
			if (!SaveVersion(*Panel)) return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 시설 바로가기 강조·개별 편성 완료 표시 추가. 백업=%s"), Prefix, *Backup);
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepProgramFeedback"),
		TEXT("기존 프로그램 WBP에 시설 안내와 개별 편성 완료 피드백을 한 번 추가한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
