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
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelPromotion.h"
#include "UI/PrepHUD/Panels/FF_PrepPromotionCard.h"
#include "UObject/UnrealType.h"

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
		Size->SetWidthOverride(240.f); Size->SetMinDesiredHeight(320.f);
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
		UTextBlock* Buzz = Wrapped(BP, Tree, TEXT("Text_Buzz"), LOCTEXT("Buzz", "화제성 +0"), 15);
		Buzz->SetJustification(ETextJustify::Center); Row(*Content, *Buzz, FMargin(0.f, 0.f, 0.f, 8.f));
		UTextBlock* Forecast = Wrapped(BP, Tree, TEXT("Text_Forecast"),
			LOCTEXT("Forecast", "예상 방문객 확인 중\n현재 입장료 기준"), 15);
		Forecast->SetJustification(ETextJustify::Center); Row(*Content, *Forecast, FMargin(0.f, 0.f, 0.f, 12.f));
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
			LOCTEXT("Expected", "홍보 효과는 예상 방문객에 반영됩니다. 실제 입장 인원은 운영 상황에 따라 달라집니다."), 14, Muted), FMargin(0.f, 0.f, 0.f, 16.f));
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

	// UE 5.7 Python에는 OnVariableAdded가 노출되지 않는다. 추가한 두 행만 등록하고 기존 GUID는 보존한다.
	void RegisterEffectWidgets()
	{
		UWidgetBlueprint* BP = LoadExistingBlueprint(CardPackage);
		if (!BP || !BP->WidgetTree) return;
		for (const FName Name : { FName(TEXT("Text_Buzz")), FName(TEXT("Text_Forecast")) })
		{
			if (BP->WidgetTree->FindWidget(Name) && !BP->WidgetVariableNameToGuidMap.Contains(Name))
			{
				BP->OnVariableAdded(Name);
			}
		}
	}
	FAutoConsoleCommand RegisterEffectsCommand(TEXT("FF.RegisterPrepPromotionEffectWidgets"),
		TEXT("홍보 카드에 추가한 화제성·예측 행의 편집기 GUID만 등록한다. 기존 레이아웃을 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&RegisterEffectWidgets));

	// 전체 재생성 대신 홍보 두 에셋의 지정 행만 옮긴다. HUD 크기·도킹과 기존 위젯 GUID는 건드리지 않는다.
	void ApplyCompactDesign()
	{
		constexpr const TCHAR* CompactPrefix = TEXT("FF_PREP_PROMOTION_COMPACT");
		UWidgetBlueprint* PanelBP = LoadExistingBlueprint(PanelPackage);
		UWidgetBlueprint* CardBP = LoadExistingBlueprint(CardPackage);
		if (!PanelBP || !PanelBP->WidgetTree || !PanelBP->GeneratedClass
			|| !CardBP || !CardBP->WidgetTree || !CardBP->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 홍보 패널과 카드가 모두 필요합니다."), CompactPrefix);
			return;
		}
		UWidgetTree& PanelTree = *PanelBP->WidgetTree;
		UWidgetTree& CardTree = *CardBP->WidgetTree;
		UObject* Defaults = PanelBP->GeneratedClass->GetDefaultObject();
		FBoolProperty* CompactProperty = FindFProperty<FBoolProperty>(UFF_PrepPanelPromotion::StaticClass(), TEXT("bUseCompactLayout"));
		FTextProperty* OnceProperty = FindFProperty<FTextProperty>(UFF_PrepPanelPromotion::StaticClass(), TEXT("OnceNoticeText"));
		FTextProperty* NoticeProperty = FindFProperty<FTextProperty>(UFF_PrepPanelPromotion::StaticClass(), TEXT("NotAppliedNoticeText"));
		auto* Body = Cast<UVerticalBox>(PanelTree.FindWidget(TEXT("VerticalBox_Body")));
		auto* Once = Cast<UTextBlock>(PanelTree.FindWidget(TEXT("Text_OnceNotice")));
		auto* Notice = Cast<UTextBlock>(PanelTree.FindWidget(TEXT("Text_NotAppliedNotice")));
		auto* Cards = Cast<UWrapBox>(PanelTree.FindWidget(TEXT("Box_PromotionCards")));
		auto* Footer = Cast<UVerticalBox>(PanelTree.FindWidget(TEXT("VerticalBox_Footer")));
		auto* Summary = Cast<UBorder>(PanelTree.FindWidget(TEXT("Border_PurchaseSummary")));
		auto* SummaryRow = Cast<UHorizontalBox>(PanelTree.FindWidget(TEXT("HorizontalBox_PurchaseSummary")));
		auto* Selected = Cast<UVerticalBox>(PanelTree.FindWidget(TEXT("VerticalBox_SelectedPromotion")));
		auto* Cost = Cast<UVerticalBox>(PanelTree.FindWidget(TEXT("VerticalBox_PurchaseCost")));
		auto* Divider = Cast<USizeBox>(PanelTree.FindWidget(TEXT("Border_SummaryDivider_Size")));
		auto* Balance = Cast<UVerticalBox>(PanelTree.FindWidget(TEXT("VerticalBox_GoldAfter")));
		auto* Actions = Cast<UHorizontalBox>(PanelTree.FindWidget(TEXT("HorizontalBox_Actions")));
		auto* BlockReason = Cast<UTextBlock>(PanelTree.FindWidget(TEXT("Text_BlockReason")));
		auto* ActionSize = Cast<USizeBox>(PanelTree.FindWidget(TEXT("SizeBox_PurchaseAction")));
		auto* Purchase = Cast<UButton>(PanelTree.FindWidget(TEXT("Button_Purchase")));
		auto* PurchaseText = Cast<UTextBlock>(PanelTree.FindWidget(TEXT("Text_PurchaseAction")));
		auto* CardColumn = Cast<UVerticalBox>(CardTree.FindWidget(TEXT("VerticalBox_Card")));
		auto* Effect = Cast<UTextBlock>(CardTree.FindWidget(TEXT("Text_Effect")));
		auto* Forecast = Cast<UTextBlock>(CardTree.FindWidget(TEXT("Text_Forecast")));
		auto* Buzz = Cast<UTextBlock>(CardTree.FindWidget(TEXT("Text_Buzz")));
		if (!Defaults || !CompactProperty || !OnceProperty || !NoticeProperty
			|| !Body || !Once || !Notice || !Cards || !Footer || !Summary || !SummaryRow
			|| !Selected || !Cost || !Divider || !Balance || !Actions || !BlockReason
			|| !ActionSize || !Purchase || !PurchaseText || !CardColumn || !Effect || !Forecast || !Buzz)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 두 홍보 에셋의 필수 위젯 또는 C++ 바인딩이 누락되어 변경하지 않았습니다."), CompactPrefix);
			return;
		}

		// 양쪽 저장 상태를 함께 확인한다. 부분 적용을 완성된 상태로 오인하지 않는다.
		UWidget* ExistingBasis = PanelTree.FindWidget(TEXT("Text_ForecastBasis"));
		if (ExistingBasis || CompactProperty->GetPropertyValue_InContainer(Defaults))
		{
			const bool bComplete = Cast<UTextBlock>(ExistingBasis)
				&& CompactProperty->GetPropertyValue_InContainer(Defaults)
				&& ExistingBasis->GetParent() == Body && ActionSize->GetParent() == SummaryRow
				&& Effect->GetVisibility() == ESlateVisibility::Collapsed
				&& Forecast->GetParent() == CardColumn && Buzz->GetParent() == CardColumn
				&& CardColumn->GetChildIndex(Forecast) < CardColumn->GetChildIndex(Buzz);
			if (bComplete)
			{
				UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 이미 적용됨. 이후 Designer 편집을 보존합니다."), CompactPrefix);
			}
			else
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 부분 적용 상태입니다. 두 에셋의 백업을 확인하세요. 변경하지 않았습니다."), CompactPrefix);
			}
			return;
		}

		const bool bExpectedParents = Once->GetParent() == Body && Notice->GetParent() == Body && Cards->GetParent() == Body
			&& Summary->GetParent() == Footer && Actions->GetParent() == Footer && SummaryRow->GetParent() == Summary
			&& Selected->GetParent() == SummaryRow && Cost->GetParent() == SummaryRow
			&& Divider->GetParent() == SummaryRow && Balance->GetParent() == SummaryRow
			&& BlockReason->GetParent() == Actions && ActionSize->GetParent() == Actions
			&& Purchase->GetParent() == ActionSize && PurchaseText->GetParent() == Purchase
			&& Effect->GetParent() == CardColumn && Forecast->GetParent() == CardColumn && Buzz->GetParent() == CardColumn;
		auto* OldActionSlot = Cast<UHorizontalBoxSlot>(ActionSize->Slot);
		auto* BalanceSlot = Cast<UHorizontalBoxSlot>(Balance->Slot);
		auto* OnceSlot = Cast<UVerticalBoxSlot>(Once->Slot);
		auto* NoticeSlot = Cast<UVerticalBoxSlot>(Notice->Slot);
		if (!bExpectedParents || !OldActionSlot || !BalanceSlot || !OnceSlot || !NoticeSlot)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 계층이 달라 자동으로 옮기지 않았습니다."), CompactPrefix);
			return;
		}

		// 위 검증이 모두 끝난 뒤에만 수정한다. 재정렬은 ShiftChild로 원래 슬롯을 유지한다.
		PanelBP->Modify(); PanelTree.Modify(); CardBP->Modify(); CardTree.Modify();
		PanelTree.ForEachWidget([](UWidget* Widget) { Widget->Modify(); if (Widget->Slot) Widget->Slot->Modify(); });
		CardTree.ForEachWidget([](UWidget* Widget) { Widget->Modify(); if (Widget->Slot) Widget->Slot->Modify(); });
		const FText CompactOnce = LOCTEXT("CompactOnce", "축제당 1회 · 셋업에서 구매 · 3일간 적용");
		const FText CompactNotice = LOCTEXT("CompactForecastNotice", "예상 인원이며 실제 입장객은 달라질 수 있습니다.");
		Once->SetText(CompactOnce);
		Notice->SetText(CompactNotice);
		UTextBlock* Basis = MakeWidget<UTextBlock>(*PanelBP, PanelTree, TEXT("Text_ForecastBasis"), true);
		FSlateFontInfo BasisFont = Once->GetFont(); BasisFont.Size = 16;
		Basis->SetFont(BasisFont); Basis->SetColorAndOpacity(FSlateColor(Ink));
		Basis->SetAutoWrapText(true);
		Basis->SetText(LOCTEXT("CompactBasisPreview", "예상 방문객 · 현재 입장료 기준"));
		Row(*Body, *Basis, FMargin(0.f, 0.f, 0.f, 12.f));
		Body->ShiftChild(0, Once); Body->ShiftChild(1, Basis); Body->ShiftChild(2, Cards); Body->ShiftChild(3, Notice);
		FMargin OncePadding = OnceSlot->GetPadding(); OncePadding.Bottom = 16.f; OnceSlot->SetPadding(OncePadding);
		FMargin NoticePadding = NoticeSlot->GetPadding(); NoticePadding.Top = 12.f; NoticePadding.Bottom = 0.f; NoticeSlot->SetPadding(NoticePadding);

		Selected->SetVisibility(ESlateVisibility::Collapsed);
		Cost->SetVisibility(ESlateVisibility::Collapsed);
		Divider->SetVisibility(ESlateVisibility::Collapsed);
		Summary->SetBrushColor(FLinearColor::Transparent);
		// 같은 HorizontalBoxSlot을 템플릿으로 복사해 사용자가 조정한 버튼 여백·정렬을 보존한다.
		auto* ActionSlot = CastChecked<UHorizontalBoxSlot>(SummaryRow->AddChild(ActionSize, OldActionSlot));
		ActionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BalanceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ActionSize->SetMinDesiredWidth(FMath::Max(280.f, ActionSize->GetMinDesiredWidth()));
		Footer->ShiftChild(0, Actions);
		PurchaseText->bIsVariable = true;
		if (!PanelBP->WidgetVariableNameToGuidMap.Contains(PurchaseText->GetFName()))
		{
			PanelBP->OnVariableAdded(PurchaseText->GetFName());
		}

		Effect->SetVisibility(ESlateVisibility::Collapsed);
		CardColumn->ShiftChild(CardColumn->GetChildIndex(Buzz), Forecast);
		FSlateFontInfo ForecastFont = Forecast->GetFont(); ForecastFont.Size = 28;
		Forecast->SetFont(ForecastFont);
		Forecast->SetText(LOCTEXT("CompactVisitorsPreview", "—명"));
		FSlateFontInfo BuzzFont = Buzz->GetFont(); BuzzFont.Size = 15; Buzz->SetFont(BuzzFont);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CardBP);
		FKismetEditorUtilities::CompileBlueprint(CardBP);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(PanelBP);
		FKismetEditorUtilities::CompileBlueprint(PanelBP);
		if (CardBP->Status == BS_Error || PanelBP->Status == BS_Error || !PanelBP->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 홍보 위젯 컴파일 실패. 두 에셋을 저장하지 않았습니다."), CompactPrefix);
			return;
		}
		Defaults = PanelBP->GeneratedClass->GetDefaultObject();
		Defaults->Modify();
		CompactProperty->SetPropertyValue_InContainer(Defaults, true);
		OnceProperty->SetPropertyValue_InContainer(Defaults, CompactOnce);
		NoticeProperty->SetPropertyValue_InContainer(Defaults, CompactNotice);
		if (!SaveAssetPackage(CompactPrefix, *CardBP) || !SaveAssetPackage(CompactPrefix, *PanelBP))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 홍보 에셋 저장 실패. 두 에셋의 백업을 확인하세요."), CompactPrefix);
			return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 비교 수치와 잔액·구매 버튼을 간소화했습니다. HUD 치수와 기존 위젯 스타일은 보존했습니다."), CompactPrefix);
	}
	FAutoConsoleCommand CompactCommand(TEXT("FF.ApplyPrepPromotionCompactDesign"),
		TEXT("기존 홍보 패널·카드의 중복 설명을 줄인다. 전체 HUD와 사용자 치수는 재생성하지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&ApplyCompactDesign));
}

#undef LOCTEXT_NAMESPACE
