// P4 홍보 전용 제작. 이후의 Designer 편집은 재실행해도 보존한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelPromotion.h"
#include "UI/PrepHUD/Panels/FF_PrepPromotionCard.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepPromotionSky"

namespace FFPrepPromotionSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_PREP_PROMOTION_SKY");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString PanelPackage = Folder / TEXT("Panels/WBP_FF_PrepPanelPromotion");
	const FString CardPackage = Folder / TEXT("WBP_FF_PrepPromotionCard");

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
		Size->SetWidthOverride(240.f); Size->SetMinDesiredHeight(240.f);
		Tree.RootWidget = Size;
		UOverlay* Layers = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Card"));
		Size->AddChild(Layers);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Card"));
		// 비활성 SButton의 자손은 Slate가 다시 흐리게 칠한다. 읽을 정보는 형제 레이어로 둔다.
		Content->SetVisibility(ESlateVisibility::HitTestInvisible);
		UButton* Root = Button(BP, Tree, TEXT("Button_Root"),
			*MakeWidget<USpacer>(BP, Tree, TEXT("Spacer_CardHitArea")));
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = Brush(Paper, 9.f, 1.25f, SkyLine);
		Style.Hovered = Brush(SkyWash, 9.f, 2.f, Color(111, 183, 214));
		Style.Pressed = Brush(Color(229, 245, 252), 9.f, 2.5f, Color(91, 166, 199));
		Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
		Root->SetStyle(Style);
		UOverlaySlot* HitArea = Layers->AddChildToOverlay(Root);
		HitArea->SetHorizontalAlignment(HAlign_Fill); HitArea->SetVerticalAlignment(VAlign_Fill);
		UOverlaySlot* Information = Layers->AddChildToOverlay(Content);
		Information->SetHorizontalAlignment(HAlign_Fill); Information->SetVerticalAlignment(VAlign_Center);
		Information->SetPadding(FMargin(16.f, 18.f));
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_Icon"), true);
		Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
			*MakeObjectPath(TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_SNS_White_256"))));
		Icon->SetDesiredSizeOverride(FVector2D(36.f, 36.f)); Icon->SetColorAndOpacity(Ink);
		USizeBox* IconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_PromotionIcon"));
		IconSize->SetWidthOverride(36.f); IconSize->SetHeightOverride(36.f); IconSize->AddChild(Icon);
		Row(*Content, *IconSize, FMargin(0.f, 0.f, 0.f, 12.f));
		CastChecked<UVerticalBoxSlot>(IconSize->Slot)->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Label = Wrapped(BP, Tree, TEXT("Text_Label"), LOCTEXT("Name", "홍보 등급"), 20);
		Label->SetJustification(ETextJustify::Center); Row(*Content, *Label, FMargin(0.f, 0.f, 0.f, 14.f));
		UTextBlock* Cost = Wrapped(BP, Tree, TEXT("Text_Cost"), LOCTEXT("Cost", "1,400 Gold"), 22);
		Cost->SetJustification(ETextJustify::Center); Row(*Content, *Cost, FMargin(0.f, 0.f, 0.f, 6.f));
		UTextBlock* Effect = Wrapped(BP, Tree, TEXT("Text_Effect"), LOCTEXT("Effect", "예상 방문객 ×1.12"), 15);
		Effect->SetJustification(ETextJustify::Center); Row(*Content, *Effect, FMargin(0.f, 0.f, 0.f, 12.f));
		UTextBlock* Badge = Wrapped(BP, Tree, TEXT("Text_Badge"), FText::GetEmpty(), 15);
		Badge->SetJustification(ETextJustify::Center); Badge->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Content, *Badge);
		return true;
	}

	bool BuildPanel(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		const FPanelFrame Frame = MakeFrame(BP, Tree, LOCTEXT("Title", "홍보"),
			TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_SNS_White_256"));
		Row(*Frame.Body, *Wrapped(BP, Tree, TEXT("Text_OnceNotice"),
			LOCTEXT("Once", "축제당 한 번 구매하며 3일 전체에 적용됩니다. 셋업 중에 선택하세요."), 15, Muted), FMargin(0.f, 0.f, 0.f, 6.f));
		Row(*Frame.Body, *Wrapped(BP, Tree, TEXT("Text_NotAppliedNotice"),
			LOCTEXT("Expected", "예상 방문객에는 반영되며, 실제 입장 인원 연동은 준비 중입니다."), 14, Muted), FMargin(0.f, 0.f, 0.f, 16.f));
		UWrapBox* Cards = MakeWidget<UWrapBox>(BP, Tree, TEXT("Box_PromotionCards"), true);
		Cards->SetInnerSlotPadding(FVector2D(12.f, 12.f)); Row(*Frame.Body, *Cards);

		UBorder* Summary = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PurchaseSummary"));
		Summary->SetBrush(Brush(Paper, 7.f, 1.f, SkyLine)); Summary->SetPadding(FMargin(18.f, 8.f));
		UHorizontalBox* SummaryRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_PurchaseSummary"));
		Summary->AddChild(SummaryRow);
		UVerticalBox* Selected = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_SelectedPromotion"));
		Row(*Selected, *Text(BP, Tree, TEXT("Text_SelectedLabel"), LOCTEXT("SelectedLabel", "홍보 등급"), 14, Muted), FMargin(0.f, 0.f, 0.f, 3.f));
		Row(*Selected, *Wrapped(BP, Tree, TEXT("Text_SelectedPromotion"), LOCTEXT("Select", "홍보 등급을 선택하세요"), 18));
		Column(*SummaryRow, *Selected, FMargin(0.f, 0.f, 20.f, 0.f), true);
		UVerticalBox* Cost = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_PurchaseCost"));
		Row(*Cost, *Text(BP, Tree, TEXT("Text_PurchaseCostLabel"), LOCTEXT("CostLabel", "홍보 비용"), 14, Muted), FMargin(0.f, 0.f, 0.f, 3.f));
		Row(*Cost, *Wrapped(BP, Tree, TEXT("Text_PurchaseCost"), LOCTEXT("EmptyCost", "—"), 20));
		Column(*SummaryRow, *Cost, FMargin(0.f, 0.f, 24.f, 0.f), true);
		Column(*SummaryRow, *Rule(BP, Tree, TEXT("Border_SummaryDivider"), true), FMargin(0.f, 0.f, 24.f, 0.f));
		UVerticalBox* Balance = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_GoldAfter"));
		Row(*Balance, *Text(BP, Tree, TEXT("Text_GoldAfterLabel"), LOCTEXT("BalanceLabel", "구매 후 잔액"), 14, Muted, true), FMargin(0.f, 0.f, 0.f, 3.f));
		Row(*Balance, *Wrapped(BP, Tree, TEXT("Text_GoldAfterValue"), LOCTEXT("EmptyBalance", "—"), 20));
		Column(*SummaryRow, *Balance, FMargin(0.f), true);
		Row(*Frame.Footer, *Summary, FMargin(0.f, 0.f, 0.f, 10.f));
		UHorizontalBox* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Actions"));
		UTextBlock* Notice = Wrapped(BP, Tree, TEXT("Text_BlockReason"), LOCTEXT("Reason", "홍보 등급을 선택하세요."), 15, Color(162, 96, 15));
		Column(*Actions, *Notice, FMargin(0.f, 0.f, 20.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Notice->Slot)->SetVerticalAlignment(VAlign_Center);
		USizeBox* ActionSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_PurchaseAction"));
		ActionSize->SetMinDesiredWidth(188.f); ActionSize->SetMinDesiredHeight(46.f);
		UButton* Purchase = Button(BP, Tree, TEXT("Button_Purchase"), *Text(BP, Tree, TEXT("Text_PurchaseAction"), LOCTEXT("Purchase", "홍보 구매하기"), 19, FLinearColor::White), true);
		Purchase->SetIsEnabled(false); ActionSize->AddChild(Purchase);
		Column(*Actions, *ActionSize); Row(*Frame.Footer, *Actions);
		return true;
	}

	void Apply()
	{
		if (UWidgetBlueprint* Existing = LoadExistingBlueprint(PanelPackage))
		{
			if (Existing->WidgetTree && Existing->WidgetTree->FindWidget(TEXT("Border_PurchaseSummary")))
			{
				UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_PREP_PROMOTION_SKY_REUSE: 이미 적용됨. Designer 편집 보존."));
				return;
			}
		}
		UWidgetBlueprint* CardBP = ReplaceWidgetBlueprint(LogPrefix, CardPackage, Folder, *UFF_PrepPromotionCard::StaticClass(), BuildCard);
		if (!CardBP) { return; }
		if (UFF_PrepPromotionCard* Defaults = Cast<UFF_PrepPromotionCard>(CardBP->GeneratedClass->GetDefaultObject()))
		{
			Defaults->SetContentColors(Ink, Ink);
			FKismetEditorUtilities::CompileBlueprint(CardBP);
			SaveAssetPackage(LogPrefix, *CardBP);
		}
		if (ReplaceWidgetBlueprint(LogPrefix, PanelPackage, Folder / TEXT("Panels"), *UFF_PrepPanelPromotion::StaticClass(), BuildPanel))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_PREP_PROMOTION_SKY_APPLIED: 홍보 비교 카드와 구매 요약 적용."));
		}
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepPromotionSkyDesign"),
		TEXT("P4 홍보 등급 비교 카드와 구매 요약 디자인을 최초 한 번 적용한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
