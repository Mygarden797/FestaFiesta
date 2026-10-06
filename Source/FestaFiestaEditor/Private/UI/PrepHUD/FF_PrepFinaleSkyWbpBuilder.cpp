// 피날레 최초 제작 도구. 기존 에셋의 Designer/Class Defaults 편집은 재실행해도 보존한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelFinale.h"
#include "UI/PrepHUD/Panels/FF_PrepFinaleCard.h"
#include "UI/Frontend/FF_FinaleConfirmDialog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepFinaleSky"

namespace FFPrepFinaleSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_PREP_FINALE_SKY");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString PanelPackage = Folder / TEXT("Panels/WBP_FF_PrepPanelFinale");
	const FString CardPackage = Folder / TEXT("WBP_FF_PrepFinaleCard");
	const FString DialogPackage = Folder / TEXT("Finale/WBP_FF_FinaleConfirmDialog");
	/** 사용자 제공 피날레 원본(SourceArt/UI/Finale/finale-*.png) 크기. 그림 칸 비율의 기준이다. */
	const FVector2D FinalePreviewSourceSize(1024.f, 800.f);

	/** 이미 만든 카드·확인 창의 그림 브러시 크기가 0×0 이면 원본 크기로 바로잡는다. 다른 편집은 그대로 둔다. */
	void FixPreviewBrushSize()
	{
		if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PREVIEW_FAIL: PIE 종료 후 실행하세요."), LogPrefix); return; }
		for (const FString& Package : { CardPackage, DialogPackage })
		{
			UWidgetBlueprint* BP = LoadExistingBlueprint(Package);
			UImage* Picture = BP && BP->WidgetTree ? Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("Image_Preview"))) : nullptr;
			if (!Picture) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PREVIEW_FAIL: 그림 칸 없음 %s"), LogPrefix, *Package); return; }
			if (!Picture->GetBrush().GetImageSize().IsNearlyZero())
			{
				UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PREVIEW_REUSE: %s 크기 %s"), LogPrefix, *Package, *Picture->GetBrush().GetImageSize().ToString());
				continue;
			}
			if (BP->GetOutermost()->IsDirty()) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PREVIEW_FAIL: 미저장 WBP 보존 %s"), LogPrefix, *Package); return; }
			const FString File = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
			const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/FinalePreviewSize") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
			if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK)
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PREVIEW_FAIL: 백업 실패 %s"), LogPrefix, *Package); return;
			}
			BP->Modify(); Picture->Modify();
			FSlateBrush PictureBrush = Picture->GetBrush();
			PictureBrush.SetImageSize(FinalePreviewSourceSize);
			Picture->SetBrush(PictureBrush);
			if (!SaveNewBlueprint(LogPrefix, *BP)) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PREVIEW_FAIL: 저장 실패 %s 백업=%s"), LogPrefix, *Package, *Backup); return; }
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PREVIEW_APPLIED: %s 그림 크기 0×0 → %s, 백업=%s"), LogPrefix, *Package, *FinalePreviewSourceSize.ToString(), *Backup);
		}
	}
	FAutoConsoleCommand FixPreviewCommand(TEXT("FF.FixFinalePreviewBrushSize"),
		TEXT("피날레 카드·확인 창의 그림 크기가 0×0 으로 저장된 것을 원본 크기로 바로잡는다."),
		FConsoleCommandDelegate::CreateStatic(&FixPreviewBrushSize));

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize = 16, FLinearColor Tint = Ink, bool bVariable = true)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	UBorder* Surface(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		FMargin Padding, FLinearColor Fill = Paper)
	{
		UBorder* Result = MakeWidget<UBorder>(BP, Tree, Name);
		Result->SetBrush(Brush(Fill, 8.f, 1.f, SkyLine));
		Result->SetPadding(Padding);
		return Result;
	}

	USizeBox* Preview(UWidgetBlueprint& BP, UWidgetTree& Tree, float Width, float Height)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Preview"));
		Size->SetWidthOverride(Width); Size->SetHeightOverride(Height);
		UScaleBox* Scale = MakeWidget<UScaleBox>(BP, Tree, TEXT("ScaleBox_Preview"));
		Scale->SetStretch(EStretch::ScaleToFit);
		Size->AddChild(Scale);
		UImage* Picture = MakeWidget<UImage>(BP, Tree, TEXT("Image_Preview"), true);
		// "텍스처 크기에 맞춤"(bMatchSize)은 렌더링 없는 생성 실행에서 0×0 으로 저장된다. 원본 크기를 직접 넣는다.
		FSlateBrush PictureBrush;
		PictureBrush.SetResourceObject(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Folder / TEXT("Finale/Textures/T_FF_Finale_C"))));
		PictureBrush.SetImageSize(FinalePreviewSourceSize);
		Picture->SetBrush(PictureBrush);
		Scale->AddChild(Picture);
		return Size;
	}

	bool BuildCard(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Card"));
		Size->SetWidthOverride(300.f); Size->SetMinDesiredHeight(360.f);
		Tree.RootWidget = Size;
		UOverlay* Layers = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Card"));
		Size->AddChild(Layers);
		// 잠긴 카드도 내용을 읽을 수 있도록 버튼과 정보 레이어를 분리한다.
		UButton* Hit = Button(BP, Tree, TEXT("Button_Root"), *MakeWidget<USpacer>(BP, Tree, TEXT("Spacer_HitArea")));
		FButtonStyle Style = Hit->GetStyle();
		Style.Normal = Brush(Paper, 8.f, 1.2f, SkyLine);
		Style.Hovered = Brush(SkyWash, 8.f, 2.f, Color(93, 177, 213));
		Style.Pressed = Brush(Color(231, 247, 251), 8.f, 2.5f, Color(31, 158, 212));
		Style.Disabled = Style.Normal;
		Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
		Hit->SetStyle(Style);
		UOverlaySlot* HitSlot = Layers->AddChildToOverlay(Hit);
		HitSlot->SetHorizontalAlignment(HAlign_Fill); HitSlot->SetVerticalAlignment(VAlign_Fill);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Content"));
		Content->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* ContentSlot = Layers->AddChildToOverlay(Content);
		ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(FMargin(6.f));
		UOverlay* Art = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Preview"));
		Row(*Content, *Art);
		Art->AddChildToOverlay(Preview(BP, Tree, 288.f, 225.f));
		UBorder* Grade = Surface(BP, Tree, TEXT("Border_Grade"), FMargin(9.f, 3.f));
		Grade->AddChild(Text(BP, Tree, TEXT("Text_Grade"), LOCTEXT("GradePreview", "C"), 23, Ink, true));
		UOverlaySlot* GradeSlot = Art->AddChildToOverlay(Grade);
		GradeSlot->SetHorizontalAlignment(HAlign_Left); GradeSlot->SetVerticalAlignment(VAlign_Top); GradeSlot->SetPadding(FMargin(8.f));
		UBorder* BadgeBorder = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Badge"), true);
		BadgeBorder->SetBrush(Brush(Paper, 12.f, 1.f, SkyLine)); BadgeBorder->SetPadding(FMargin(9.f, 5.f));
		UTextBlock* Badge = Wrapped(BP, Tree, TEXT("Text_Badge"), FText::GetEmpty(), 14);
		BadgeBorder->AddChild(Badge); BadgeBorder->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* BadgeSlot = Art->AddChildToOverlay(BadgeBorder);
		BadgeSlot->SetHorizontalAlignment(HAlign_Right); BadgeSlot->SetVerticalAlignment(VAlign_Top); BadgeSlot->SetPadding(FMargin(8.f));
		for (const auto& Item : TArray<TTuple<FName, FText, int32>>{
			{TEXT("Text_Label"), LOCTEXT("CardLabel", "피날레 C"), 20},
			{TEXT("Text_Description"), LOCTEXT("Description", "단발 불꽃"), 15},
			{TEXT("Text_Cost"), LOCTEXT("CostPreview", "—"), 25},
			{TEXT("Text_Bonus"), LOCTEXT("BonusPreview", "평가 만족도 —"), 15}})
		{
			UTextBlock* Label = Wrapped(BP, Tree, *Item.Get<0>().ToString(), Item.Get<1>(), Item.Get<2>());
			Label->SetJustification(ETextJustify::Center);
			Row(*Content, *Label, FMargin(8.f, 5.f, 8.f, 0.f));
		}
		return true;
	}

	void SummaryItem(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Parent,
		const TCHAR* Prefix, const TCHAR* ValueName, const FText& Label, const TCHAR* LabelName = nullptr)
	{
		UVerticalBox* Stack = MakeWidget<UVerticalBox>(BP, Tree, FName(FString(TEXT("VerticalBox_")) + Prefix));
		UTextBlock* Caption = Text(BP, Tree, LabelName ? LabelName : *(FString(TEXT("Text_")) + Prefix + TEXT("Caption")), Label, 15, Muted, LabelName != nullptr);
		Caption->SetJustification(ETextJustify::Center);
		Row(*Stack, *Caption, FMargin(0.f, 0.f, 0.f, 4.f));
		UTextBlock* Value = Wrapped(BP, Tree, ValueName, LOCTEXT("Missing", "—"), 25);
		Value->SetJustification(ETextJustify::Center); Row(*Stack, *Value);
		Column(Parent, *Stack, FMargin(10.f, 0.f), true);
	}

	bool BuildPanel(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		const FPanelFrame Frame = MakeFrame(BP, Tree, LOCTEXT("Title", "피날레"), TEXT("/Game/FestaFiesta/UI/Frontend/Art/T_FF_Spark"));
		// 기존 700 높이에서도 카드와 고정 액션 영역을 함께 읽을 수 있도록 본문 여백만 줄인다.
		if (UWidget* Scroll = Tree.FindWidget(TEXT("ScrollBox_Body")))
		{
			CastChecked<UVerticalBoxSlot>(Scroll->Slot)->SetPadding(FMargin(24.f, 10.f));
		}
		UHorizontalBox* Subheading = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Subheading"));
		Column(*Subheading, *Wrapped(BP, Tree, TEXT("Text_StageNotice"), LOCTEXT("Stage", "마지막 날 아침 점검에서 구매할 수 있습니다."), 16, Muted), FMargin(0.f), true);
		Column(*Subheading, *Text(BP, Tree, TEXT("Text_Unit"), LOCTEXT("Unit", "금액 단위: 금화"), 14, Muted));
		Row(*Frame.Body, *Subheading, FMargin(0.f, 0.f, 0.f, 12.f));
		UBorder* Once = Surface(BP, Tree, TEXT("Border_OnceNotice"), FMargin(14.f, 9.f), SkyWash);
		Once->AddChild(Wrapped(BP, Tree, TEXT("Text_OnceNotice"), LOCTEXT("Once", "축제당 1회 구매 · 구매 후 변경 및 취소 불가"), 16));
		Row(*Frame.Body, *Once, FMargin(0.f, 0.f, 0.f, 14.f));
		UWrapBox* Cards = MakeWidget<UWrapBox>(BP, Tree, TEXT("Box_FinaleCards"), true);
		Cards->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		Row(*Frame.Body, *Cards);
		UBorder* Summary = Surface(BP, Tree, TEXT("Border_PurchaseSummary"), FMargin(12.f, 10.f));
		UHorizontalBox* SummaryRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Summary")); Summary->AddChild(SummaryRow);
		SummaryItem(BP, Tree, *SummaryRow, TEXT("Selected"), TEXT("Text_SelectedFinale"), LOCTEXT("Selected", "선택한 피날레"));
		Column(*SummaryRow, *Rule(BP, Tree, TEXT("Rule_Cost"), true));
		SummaryItem(BP, Tree, *SummaryRow, TEXT("Cost"), TEXT("Text_PurchaseCost"), LOCTEXT("Cost", "구매 비용"));
		Column(*SummaryRow, *Rule(BP, Tree, TEXT("Rule_Balance"), true));
		SummaryItem(BP, Tree, *SummaryRow, TEXT("Balance"), TEXT("Text_GoldAfterValue"), LOCTEXT("Balance", "구매 후 잔액"), TEXT("Text_GoldAfterLabel"));
		Row(*Frame.Footer, *Summary, FMargin(0.f, 0.f, 0.f, 12.f));
		UHorizontalBox* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Actions"));
		UTextBlock* Reason = Wrapped(BP, Tree, TEXT("Text_BlockReason"), LOCTEXT("Pick", "피날레 등급을 선택하세요."), 16, Muted);
		Column(*Actions, *Reason, FMargin(0.f, 0.f, 16.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Reason->Slot)->SetVerticalAlignment(VAlign_Center);
		USizeBox* LaterSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Later"));
		LaterSize->SetWidthOverride(140.f); LaterSize->SetHeightOverride(46.f);
		LaterSize->AddChild(Button(BP, Tree, TEXT("Button_Later"), *Text(BP, Tree, TEXT("Text_Later"), LOCTEXT("Later", "나중에"), 18)));
		Column(*Actions, *LaterSize, FMargin(0.f, 0.f, 12.f, 0.f));
		USizeBox* BuySize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Purchase"));
		BuySize->SetMinDesiredWidth(220.f); BuySize->SetHeightOverride(46.f);
		UButton* Buy = Button(BP, Tree, TEXT("Button_Purchase"), *Text(BP, Tree, TEXT("Text_PurchaseLabel"), LOCTEXT("Buy", "피날레 구매하기"), 18, Paper, true), true);
		Buy->SetIsEnabled(false); BuySize->AddChild(Buy); Column(*Actions, *BuySize); Row(*Frame.Footer, *Actions);
		return true;
	}

	bool BuildDialog(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass* CancelClass, UClass* AcceptClass)
	{
		UOverlay* Root = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Root")); Tree.RootWidget = Root;
		UBorder* Scrim = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Scrim"));
		FLinearColor ScrimColor = Color(17, 37, 45); ScrimColor.A = .42f;
		Scrim->SetBrush(Brush(ScrimColor)); Scrim->SetPadding(FMargin(0.f));
		UOverlaySlot* ScrimSlot = Root->AddChildToOverlay(Scrim);
		ScrimSlot->SetHorizontalAlignment(HAlign_Fill); ScrimSlot->SetVerticalAlignment(VAlign_Fill);
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Dialog")); Size->SetWidthOverride(760.f);
		UOverlaySlot* DialogSlot = Root->AddChildToOverlay(Size);
		DialogSlot->SetHorizontalAlignment(HAlign_Center); DialogSlot->SetVerticalAlignment(VAlign_Center);
		UBorder* Dialog = Surface(BP, Tree, TEXT("Border_Dialog"), FMargin(24.f)); Size->AddChild(Dialog);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Dialog")); Dialog->AddChild(Content);
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Header"));
		Column(*Header, *Wrapped(BP, Tree, TEXT("Text_Title"), LOCTEXT("ConfirmTitle", "피날레를 구매할까요?"), 27), FMargin(0.f, 0.f, 16.f, 0.f), true);
		USizeBox* CloseSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Close")); CloseSize->SetWidthOverride(36.f); CloseSize->SetHeightOverride(36.f);
		CloseSize->AddChild(Button(BP, Tree, TEXT("Button_Close"), *CloseIcon(BP, Tree))); Column(*Header, *CloseSize);
		Row(*Content, *Header, FMargin(0.f, 0.f, 0.f, 20.f));
		UBorder* Selection = Surface(BP, Tree, TEXT("Border_Selection"), FMargin(10.f));
		UHorizontalBox* SelectionRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Selection")); Selection->AddChild(SelectionRow);
		Column(*SelectionRow, *Preview(BP, Tree, 340.f, 265.625f), FMargin(0.f, 0.f, 20.f, 0.f));
		UVerticalBox* Details = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_SelectionDetails"));
		Row(*Details, *Wrapped(BP, Tree, TEXT("Text_Grade"), LOCTEXT("ConfirmGrade", "피날레 등급"), 24), FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Details, *Wrapped(BP, Tree, TEXT("Text_Bonus"), LOCTEXT("ConfirmBonus", "평가 만족도 —"), 20, Muted));
		Column(*SelectionRow, *Details, FMargin(0.f), true); CastChecked<UHorizontalBoxSlot>(Details->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(*Content, *Selection);
		Row(*Content, *Text(BP, Tree, TEXT("Text_Unit"), LOCTEXT("Unit", "금액 단위: 금화"), 14, Muted), FMargin(0.f, 12.f, 0.f, 6.f));
		UBorder* Quote = Surface(BP, Tree, TEXT("Border_Quote"), FMargin(12.f, 10.f));
		UHorizontalBox* QuoteRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Quote")); Quote->AddChild(QuoteRow);
		SummaryItem(BP, Tree, *QuoteRow, TEXT("Cost"), TEXT("Text_Cost"), LOCTEXT("Cost", "구매 비용"));
		Column(*QuoteRow, *Rule(BP, Tree, TEXT("Rule_Balance"), true));
		SummaryItem(BP, Tree, *QuoteRow, TEXT("Balance"), TEXT("Text_GoldAfter"), LOCTEXT("Balance", "구매 후 잔액"));
		Row(*Content, *Quote, FMargin(0.f, 0.f, 0.f, 12.f));
		UBorder* Warning = Surface(BP, Tree, TEXT("Border_Warning"), FMargin(14.f, 10.f), Color(255, 249, 230));
		Warning->AddChild(Wrapped(BP, Tree, TEXT("Text_Body"), LOCTEXT("ConfirmBody", "축제당 한 번만 구매할 수 있습니다.\n구매 후에는 변경하거나 취소할 수 없습니다."), 16, Color(131, 90, 28)));
		Row(*Content, *Warning, FMargin(0.f, 0.f, 0.f, 10.f));
		UTextBlock* Reason = Wrapped(BP, Tree, TEXT("Text_BlockReason"), FText::GetEmpty(), 16, Color(162, 96, 15));
		Reason->SetVisibility(ESlateVisibility::Collapsed); Row(*Content, *Reason, FMargin(0.f, 0.f, 0.f, 10.f));
		UHorizontalBox* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Actions"));
		UFF_CommonButtonBase* Cancel = Cast<UFF_CommonButtonBase>(MakeWidgetOfClass(BP, Tree, TEXT("Button_Cancel"), CancelClass));
		UFF_CommonButtonBase* Accept = Cast<UFF_CommonButtonBase>(MakeWidgetOfClass(BP, Tree, TEXT("Button_Confirm"), AcceptClass));
		if (!Cancel || !Accept) { return false; }
		Cancel->ApplyGeneratedButtonContent(LOCTEXT("Back", "돌아가기"), FSlateBrush(), FVector2D::ZeroVector);
		Accept->ApplyGeneratedButtonContent(LOCTEXT("Confirm", "구매 확정"), FSlateBrush(), FVector2D::ZeroVector);
		Column(*Actions, *Cancel, FMargin(0.f, 0.f, 12.f, 0.f), true); Column(*Actions, *Accept, FMargin(0.f), true);
		Row(*Content, *Actions);
		return true;
	}

	void Apply()
	{
		UClass* CancelClass = LoadClass<UFF_CommonButtonBase>(nullptr, *(Folder / TEXT("WBP_FF_PrepConfirmSkyCancel.WBP_FF_PrepConfirmSkyCancel_C")));
		UClass* AcceptClass = LoadClass<UFF_CommonButtonBase>(nullptr, *(Folder / TEXT("WBP_FF_PrepConfirmSkyAccept.WBP_FF_PrepConfirmSkyAccept_C")));
		if (!CancelClass || !AcceptClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 셋업 확인 버튼 에셋이 필요합니다."), LogPrefix); return;
		}
		const bool bNewCard = !LoadExistingBlueprint(CardPackage);
		UWidgetBlueprint* Card = EnsureWidgetBlueprint(LogPrefix, CardPackage, Folder, *UFF_PrepFinaleCard::StaticClass(), BuildCard);
		if (!Card || !Card->GeneratedClass) { return; }
		if (bNewCard)
		{
			CastChecked<UFF_PrepFinaleCard>(Card->GeneratedClass->GetDefaultObject())->SetContentColors(Ink, Ink);
			FKismetEditorUtilities::CompileBlueprint(Card); SaveAssetPackage(LogPrefix, *Card);
		}
		UWidgetBlueprint* Dialog = EnsureWidgetBlueprint(LogPrefix, DialogPackage, Folder / TEXT("Finale"), *UFF_FinaleConfirmDialog::StaticClass(),
			[CancelClass, AcceptClass](UWidgetBlueprint& BP, UWidgetTree& Tree) { return BuildDialog(BP, Tree, CancelClass, AcceptClass); });
		if (!Dialog || !Dialog->GeneratedClass) { return; }
		const bool bNewPanel = !LoadExistingBlueprint(PanelPackage);
		UWidgetBlueprint* Panel = EnsureWidgetBlueprint(LogPrefix, PanelPackage, Folder / TEXT("Panels"), *UFF_PrepPanelFinale::StaticClass(), BuildPanel);
		if (!Panel || !Panel->GeneratedClass) { return; }
		if (bNewPanel)
		{
			TMap<EFF_FinaleGrade, FFF_FinaleTierPresentation> Presentations;
			const EFF_FinaleGrade Grades[] = { EFF_FinaleGrade::C, EFF_FinaleGrade::B, EFF_FinaleGrade::A, EFF_FinaleGrade::S };
			const TCHAR* Names[] = { TEXT("C"), TEXT("B"), TEXT("A"), TEXT("S") };
			const FText Descriptions[] = { LOCTEXT("DescC", "단발 불꽃"), LOCTEXT("DescB", "묶음 불꽃"), LOCTEXT("DescA", "일렬 · 부채형"), LOCTEXT("DescS", "대형 혼합 연출") };
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Grades); ++Index)
			{
				FFF_FinaleTierPresentation Presentation;
				Presentation.PreviewTexture = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Folder / TEXT("Finale/Textures/T_FF_Finale_") + Names[Index]));
				Presentation.Description = Descriptions[Index]; Presentations.Add(Grades[Index], Presentation);
			}
			CastChecked<UFF_PrepPanelFinale>(Panel->GeneratedClass->GetDefaultObject())->ConfigurePresentationDefaults(Card->GeneratedClass.Get(), Dialog->GeneratedClass.Get(), Presentations);
			FKismetEditorUtilities::CompileBlueprint(Panel); SaveAssetPackage(LogPrefix, *Panel);
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 피날레 선택·확인 패널 연결. 기존 편집값 보존."), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepFinaleSkyDesign"), TEXT("피날레 선택 및 구매 확인 화면을 최초 한 번 생성한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
