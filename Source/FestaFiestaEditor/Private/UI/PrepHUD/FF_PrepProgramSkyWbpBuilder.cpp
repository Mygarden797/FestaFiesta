// P3 프로그램 전용 제작. 이후의 Designer 편집은 재실행해도 보존한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCard.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepProgramSky"

namespace FFPrepProgramSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_PREP_PROGRAM_SKY");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString PanelPackage = Folder / TEXT("Panels/WBP_FF_PrepPanelProgram");
	const FString CardPackage = Folder / TEXT("WBP_FF_PrepProgramCard");
	const FString ChoicePackage = Folder / TEXT("WBP_FF_PrepProgramFacilityChoice");

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize = 16, FLinearColor Tint = Ink, bool bVariable = true)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	bool BuildCard(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Card"));
		Size->SetWidthOverride(300.f);
		Size->SetMinDesiredHeight(300.f);
		Tree.RootWidget = Size;
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Card"));
		UButton* Root = Button(BP, Tree, TEXT("Button_Root"), *Content);
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = Brush(Paper, 9.f, 1.25f, SkyLine);
		Style.Hovered = Brush(SkyWash, 9.f, 2.f, Color(111, 183, 214));
		Style.Pressed = Brush(Color(229, 245, 252), 9.f, 2.5f, Color(91, 166, 199));
		Style.NormalPadding = Style.PressedPadding = FMargin(20.f, 16.f);
		Root->SetStyle(Style);
		CastChecked<UButtonSlot>(Content->Slot)->SetHorizontalAlignment(HAlign_Fill);
		CastChecked<UButtonSlot>(Content->Slot)->SetVerticalAlignment(VAlign_Fill);
		Size->AddChild(Root);
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_Icon"), true);
		Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			*MakeObjectPath(TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Needs/T_FF_LH_Icon_NeedEntertainment_White_256"))));
		Icon->SetDesiredSizeOverride(FVector2D(32.f, 32.f));
		Icon->SetColorAndOpacity(Ink);
		USizeBox* IconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_ProgramIcon"));
		IconSize->SetWidthOverride(28.f); IconSize->SetHeightOverride(28.f); IconSize->AddChild(Icon);
		Row(*Content, *IconSize, FMargin(0.f, 0.f, 0.f, 8.f));
		CastChecked<UVerticalBoxSlot>(IconSize->Slot)->SetHorizontalAlignment(HAlign_Left);
		Row(*Content, *Wrapped(BP, Tree, TEXT("Text_Label"), LOCTEXT("CardName", "프로그램 이름"), 20), FMargin(0.f, 0.f, 0.f, 6.f));
		Row(*Content, *Text(BP, Tree, TEXT("Text_TimeRange"), LOCTEXT("TimePreview", "18:30 ~ 19:00"), 16, Ink, true), FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Content, *Rule(BP, Tree, TEXT("Border_CardDivider")), FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Content, *Wrapped(BP, Tree, TEXT("Text_Capacity"), LOCTEXT("CapacityPreview", "정원 12명 · 시설별 매출")), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Content, *Text(BP, Tree, TEXT("Text_Cost"), LOCTEXT("CostPreview", "200 Gold"), 22, Ink, true), FMargin(0.f, 0.f, 0.f, 10.f));
		UTextBlock* Badge = Wrapped(BP, Tree, TEXT("Text_Badge"), FText::GetEmpty(), 15);
		Badge->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Content, *Badge);
		return true;
	}

	bool BuildChoice(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Choice"));
		Size->SetMinDesiredHeight(38.f);
		Tree.RootWidget = Size;
		UTextBlock* Label = Wrapped(BP, Tree, TEXT("Text_Label"), LOCTEXT("CatalogPreview", "운영 시설 · 건축목록에서 보기"));
		Size->AddChild(Button(BP, Tree, TEXT("Button_Root"), *Label));
		// 가운데 칸에 자동 줄바꿈 글자를 두면 직전 폭에서 줄이 바뀐다. 버튼 폭을 쓰고 글자로 가운데 맞춘다.
		CastChecked<UButtonSlot>(Label->Slot)->SetHorizontalAlignment(HAlign_Fill);
		Label->SetJustification(ETextJustify::Center);
		return true;
	}

	UComboBoxString* FacilityCombo(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UComboBoxString* Combo = MakeWidget<UComboBoxString>(BP, Tree, TEXT("Combo_Facilities"), true);
		Combo->SetContentPadding(FMargin(10.f, 8.f));
		Combo->SetMaxListHeight(220.f);
		FComboBoxStyle Style = Combo->GetWidgetStyle();
		FButtonStyle& ButtonStyle = Style.ComboButtonStyle.ButtonStyle;
		ButtonStyle.Normal = Brush(Paper, 6.f, 1.f, SkyLine);
		ButtonStyle.Hovered = Brush(SkyWash, 6.f, 2.f, Color(111, 183, 214));
		ButtonStyle.Pressed = Brush(Color(229, 245, 252), 6.f, 2.f, Color(91, 166, 199));
		ButtonStyle.Disabled = Brush(SkyWash, 6.f, 1.f, SkyLine);
		ButtonStyle.NormalPadding = ButtonStyle.PressedPadding = FMargin(2.f);
		Style.ComboButtonStyle.MenuBorderBrush = Brush(Paper, 6.f, 1.f, SkyLine);
		Combo->SetWidgetStyle(Style);
		FTableRowStyle ItemStyle = Combo->GetItemStyle();
		ItemStyle.SetTextColor(Ink).SetSelectedTextColor(Ink)
			.SetActiveBrush(Brush(Color(221, 239, 246)))
			.SetInactiveBrush(Brush(Color(221, 239, 246)))
			.SetEvenRowBackgroundBrush(Brush(Paper)).SetOddRowBackgroundBrush(Brush(Paper));
		Combo->SetItemStyle(ItemStyle);
		// UE5.7의 생성 시 전용 속성. 런타임 재지정 없이 Designer 값으로 저장한다.
		FSlateFontInfo Font = Combo->GetFont();
		Font.FontObject = LoadObject<UFont>(nullptr, *MakeObjectPath(GetUiFontPackage()));
		Font.TypefaceFontName = TEXT("Medium"); Font.Size = 16;
		FStructProperty* FontProperty = FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("Font"));
		FStructProperty* ColorProperty = FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("ForegroundColor"));
		check(FontProperty && ColorProperty);
		*FontProperty->ContainerPtrToValuePtr<FSlateFontInfo>(Combo) = Font;
		*ColorProperty->ContainerPtrToValuePtr<FSlateColor>(Combo) = FSlateColor(Ink);
		Combo->AddOption(TEXT("운영 시설을 선택하세요"));
		Combo->SetSelectedIndex(0);
		return Combo;
	}

	bool BuildPanel(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		const FPanelFrame Frame = MakeFrame(BP, Tree, LOCTEXT("Title", "프로그램"),
			TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Clock_White_256"));
		UHorizontalBox* Intro = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Instruction"));
		Column(*Intro, *Wrapped(BP, Tree, TEXT("Text_Instruction"), LOCTEXT("Instruction", "프로그램을 고른 뒤 운영할 시설을 선택하세요."), 17, Muted, false), FMargin(0.f, 0.f, 16.f, 0.f), true);
		UBorder* Day = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Day"));
		Day->SetBrush(Brush(SkyWash, 5.f, 1.f, SkyLine)); Day->SetPadding(FMargin(12.f, 3.f));
		Day->AddChild(Text(BP, Tree, TEXT("Text_Day"), LOCTEXT("DayPreview", "1일차"), 16, Ink, true));
		Column(*Intro, *Day);
		Row(*Frame.Body, *Intro, FMargin(0.f, 0.f, 0.f, 16.f));
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_ProgramColumns"));
		UVerticalBox* Catalog = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Catalog"));
		Column(*Columns, *Catalog, FMargin(0.f, 0.f, 20.f, 0.f), true);
		UTextBlock* Empty = Wrapped(BP, Tree, TEXT("Text_EmptyCatalog"), LOCTEXT("EmptyPreview", "편성할 수 있는 프로그램이 없습니다."), 17, Muted);
		Empty->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Catalog, *Empty);
		UWrapBox* Cards = MakeWidget<UWrapBox>(BP, Tree, TEXT("Box_ProgramCards"), true);
		Cards->SetInnerSlotPadding(FVector2D(16.f, 16.f));
		Row(*Catalog, *Cards);
		USizeBox* SummarySize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Summary"));
		SummarySize->SetWidthOverride(340.f);
		UBorder* Summary = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Summary"));
		Summary->SetBrush(Brush(SkyWash, 8.f, 1.f, SkyLine)); Summary->SetPadding(FMargin(14.f));
		UVerticalBox* Quote = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Summary"));
		Summary->AddChild(Quote); SummarySize->AddChild(Summary); Column(*Columns, *SummarySize);
		Row(*Quote, *Text(BP, Tree, TEXT("Text_SummaryTitle"), LOCTEXT("Summary", "편성 요약"), 21), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Quote, *Text(BP, Tree, TEXT("Text_SelectedLabel"), LOCTEXT("SelectedLabel", "선택한 프로그램"), 15, Muted), FMargin(0.f, 0.f, 0.f, 2.f));
		Row(*Quote, *Wrapped(BP, Tree, TEXT("Text_SelectedProgram"), LOCTEXT("SelectedPreview", "프로그램을 선택하세요"), 19), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Quote, *Rule(BP, Tree, TEXT("Border_SummaryDivider")), FMargin(0.f, 0.f, 0.f, 10.f));
		UHorizontalBox* Cost = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_ProgramCost"));
		Column(*Cost, *Text(BP, Tree, TEXT("Text_CostLabel"), LOCTEXT("CostLabel", "편성 비용"), 15, Muted), FMargin(0.f), true);
		Column(*Cost, *Wrapped(BP, Tree, TEXT("Text_CostPreview"), LOCTEXT("QuotePreview", "200 Gold"), 18));
		Row(*Quote, *Cost, FMargin(0.f, 0.f, 0.f, 4.f));
		Row(*Quote, *Wrapped(BP, Tree, TEXT("Text_GoldAfter"), LOCTEXT("GoldPreview", "편성 후 잔액 9,500 Gold"), 15, Muted), FMargin(0.f, 0.f, 0.f, 6.f));
		Row(*Quote, *Wrapped(BP, Tree, TEXT("Text_RevenuePreview"), LOCTEXT("RevenuePreview", "시설 선택 후 매출을 확인할 수 있습니다."), 15, Muted), FMargin(0.f, 0.f, 0.f, 8.f));
		UVerticalBox* Facility = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_FacilityRow"), true);
		Row(*Facility, *Text(BP, Tree, TEXT("Text_FacilityLabel"), LOCTEXT("Facility", "운영 시설"), 16), FMargin(0.f, 0.f, 0.f, 4.f));
		Row(*Facility, *FacilityCombo(BP, Tree)); Row(*Quote, *Facility);
		UTextBlock* Notice = Wrapped(BP, Tree, TEXT("Text_FacilityNotice"), FText::GetEmpty(), 16, Muted);
		Notice->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Frame.Body, *Notice, FMargin(0.f, 0.f, 0.f, 8.f));
		UVerticalBox* Choices = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_CatalogChoices"), true);
		Choices->SetVisibility(ESlateVisibility::Collapsed); Row(*Frame.Body, *Choices, FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Frame.Body, *Columns);
		UTextBlock* Reason = Wrapped(BP, Tree, TEXT("Text_BlockReason"), LOCTEXT("ReasonPreview", "편성할 프로그램을 고르세요."), 15, Color(162, 96, 15));
		Row(*Frame.Footer, *Reason, FMargin(0.f, 0.f, 0.f, 8.f));
		UHorizontalBox* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Actions"));
		UTextBlock* Daily = Wrapped(BP, Tree, TEXT("Text_DailyNotice"), LOCTEXT("Daily", "프로그램은 하루 동안 운영되며, 다음 날 다시 편성합니다."), 15, Muted);
		Column(*Actions, *Daily, FMargin(0.f, 0.f, 20.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Daily->Slot)->SetVerticalAlignment(VAlign_Center);
		USizeBox* ActionSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_ScheduleAction"));
		ActionSize->SetMinDesiredWidth(168.f); ActionSize->SetMinDesiredHeight(46.f);
		UButton* Schedule = Button(BP, Tree, TEXT("Button_Schedule"), *Text(BP, Tree, TEXT("Text_ScheduleAction"), LOCTEXT("Schedule", "편성하기"), 19, FLinearColor::White), true);
		Schedule->SetIsEnabled(false); ActionSize->AddChild(Schedule);
		Column(*Actions, *ActionSize); Row(*Frame.Footer, *Actions);
		return true;
	}

	void Apply()
	{
		if (UWidgetBlueprint* Existing = LoadExistingBlueprint(PanelPackage))
		{
			if (Existing->WidgetTree && Existing->WidgetTree->FindWidget(TEXT("Combo_Facilities")))
			{
				UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_PREP_PROGRAM_SKY_REUSE: 이미 적용됨. Designer 편집 보존."));
				return;
			}
		}
		UWidgetBlueprint* CardBP = ReplaceWidgetBlueprint(LogPrefix, CardPackage, Folder, *UFF_PrepProgramCard::StaticClass(), BuildCard);
		UWidgetBlueprint* ChoiceBP = EnsureWidgetBlueprint(LogPrefix, ChoicePackage, Folder, *UFF_PrepChoiceButton::StaticClass(), BuildChoice);
		if (!CardBP || !ChoiceBP) { return; }
		for (UWidgetBlueprint* BP : {CardBP, ChoiceBP})
		{
			if (UFF_PrepChoiceButton* Defaults = Cast<UFF_PrepChoiceButton>(BP->GeneratedClass->GetDefaultObject()))
			{
				Defaults->SetContentColors(Ink, Ink);
				// UMG 인스턴스 템플릿에도 CDO 색 변경을 반영한다.
				FKismetEditorUtilities::CompileBlueprint(BP);
				SaveAssetPackage(LogPrefix, *BP);
			}
		}
		if (ReplaceWidgetBlueprint(LogPrefix, PanelPackage, Folder / TEXT("Panels"), *UFF_PrepPanelProgram::StaticClass(), BuildPanel))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_PREP_PROGRAM_SKY_APPLIED: 프로그램/카드/시설 안내 적용."));
		}
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepProgramSkyDesign"),
		TEXT("P3 프로그램 카드와 시설 선택·편성 요약 디자인을 최초 한 번 적용한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
