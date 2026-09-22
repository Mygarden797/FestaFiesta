// FF_BuildModeClarityInfoBuilder.cpp
// 역할: 시설 정보 화면을 건설 중에 읽기 쉬운 배치로 수정한다.

// Explicit, additive migration for the compact build-mode facility inspector.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace FFBuildModeClarityInfo
{
using namespace FFUIWbpBuilder;
static const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0x24,0x49,0x39));
static const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(0x68,0x7b,0x6d));
static const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(0xf2,0xf6,0xee));
static const FLinearColor Soft = FLinearColor::FromSRGBColor(FColor(0xe6,0xee,0xe0));
static const FLinearColor Line = FLinearColor::FromSRGBColor(FColor(0xc6,0xd6,0xc4));

static FSlateBrush Brush(const FLinearColor Color)
{
    FSlateBrush Result;
    Result.DrawAs = ESlateBrushDrawType::RoundedBox;
    Result.TintColor = Color;
    Result.OutlineSettings = FSlateBrushOutlineSettings(7, FSlateColor(Line), 1);
    return Result;
}
static FSlateBrush Material(const TCHAR* Role, const FLinearColor Fallback)
{
    const FString Name = FString(TEXT("MI_UI_BuildClarity_")) + Role;
    const FString Path = FString(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity/")) + Name + TEXT(".") + Name;
    FSlateBrush Result = Brush(Fallback);
    if (UMaterialInterface* Resource = LoadObject<UMaterialInterface>(nullptr, *Path))
    {
        Result.DrawAs = ESlateBrushDrawType::Image;
        Result.SetResourceObject(Resource);
        Result.ImageSize = FVector2D(456,620);
        Result.TintColor = FLinearColor::White;
    }
    return Result;
}
static void V(UVerticalBox& Parent, UWidget& Child, const FMargin Padding = FMargin(0))
{
    Child.RemoveFromParent();
    auto* Slot = Parent.AddChildToVerticalBox(&Child);
    Slot->SetPadding(Padding);
    Slot->SetHorizontalAlignment(HAlign_Fill);
}
static void Font(UCommonTextBlock* Text, const int32 Size, const bool Strong = false)
{
    if (!Text) return;
    StyleText(*Text, Size, Strong ? Ink : Muted, Strong ? TEXT("SemiBold") : TEXT("Regular"));
    Text->SetAutoWrapText(true);
}
static UCommonTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
    const TCHAR* Copy, const int32 Size, const bool Variable = false)
{
    auto* Text = MakeWidget<UCommonTextBlock>(BP, Tree, Name, Variable);
    Text->SetText(FText::FromString(Copy));
    Font(Text, Size);
    return Text;
}
static UCommonTextBlock* FindText(UWidgetTree& Tree, const TCHAR* Name)
{ return Cast<UCommonTextBlock>(Tree.FindWidget(Name)); }
static void StyleButton(UButton& Button, const bool Primary = false, const bool Tab = false)
{
    FButtonStyle Style = Button.GetStyle();
    Style.Normal = Primary ? Material(TEXT("Action"), Ink) : Brush(Soft);
    Style.Hovered = Brush(FLinearColor::FromSRGBColor(FColor(0xd7,0xe5,0xd1)));
    Style.Pressed = Brush(Ink);
    Style.Disabled = Tab ? Brush(Ink) : Brush(Soft);
    Style.NormalPadding = FMargin(4,7);
    Style.PressedPadding = Style.NormalPadding;
    Button.SetStyle(Style);
    if (auto* Text = Cast<UCommonTextBlock>(Button.GetContent()))
    {
        Font(Text, 15, true);
        Text->SetAutoWrapText(false);
        Text->SetJustification(ETextJustify::Center);
        if (Primary) Text->SetColorAndOpacity(FLinearColor::White);
        if (auto* Slot = Cast<UButtonSlot>(Text->Slot)) Slot->SetPadding(FMargin(2));
    }
}
static UButton* Button(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
    const TCHAR* LabelName, const TCHAR* Copy, const bool Tab = false)
{
    auto* Result = MakeWidget<UButton>(BP, Tree, Name, true);
    if (auto* Focus = FindFProperty<FBoolProperty>(UButton::StaticClass(),TEXT("IsFocusable")))
        Focus->SetPropertyValue_InContainer(Result,false);
    Result->AddChild(Label(BP, Tree, LabelName, Copy, 15));
    StyleButton(*Result, false, Tab);
    return Result;
}
static void H(UHorizontalBox& Parent, UWidget& Child)
{
    Child.RemoveFromParent();
    auto* Slot = Parent.AddChildToHorizontalBox(&Child);
    Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Slot->SetPadding(FMargin(2,0));
    Slot->SetVerticalAlignment(VAlign_Center);
}
static void Pair(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent,
    const TCHAR* Name, const TCHAR* Caption, const bool Hero = false)
{
    auto* Value = FindText(Tree, Name);
    if (!Value) return;
    auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, FName(FString(TEXT("Clarity_Row_"))+Name));
    auto* Key = Label(BP, Tree, FName(FString(TEXT("Clarity_Label_"))+Name), Caption, 14);
    H(*Row, *Key);
    H(*Row, *Value);
    Font(Value, Hero ? 24 : 15, true);
    Value->SetJustification(ETextJustify::Right);
    V(Parent, *Row, FMargin(0,Hero ? 4 : 3,0,Hero ? 8 : 3));
}
static bool Apply(UWidgetBlueprint& BP)
{
    UWidgetTree* Tree = BP.WidgetTree;
    if (!Tree) return false;
    if (Tree->FindWidget(TEXT("Clarity_InfoV1"))) return true;
    auto* Shell = Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_Shell")));
    auto* Navigation = Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_InspectorNavigation")));
    auto* Tabs = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Panel_InspectorTabs")));
    auto* Build = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_BuildSection")));
    auto* Live = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_LiveSection")));
    auto* Revenue = Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_RevenueSection")));
    auto* Synergy = Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_SynergySection")));
    if (!Shell || !Navigation || !Tabs || !Build || !Live || !Revenue || !Synergy ||
        !Tree->FindWidget(TEXT("Panel_PersistentIdentity")) || !Tree->FindWidget(TEXT("Aurora_InfoV7"))) return false;
    for (const TCHAR* Name : {TEXT("Text_QueueCapacityValue"), TEXT("Text_ServiceCapacityValue"),
        TEXT("Text_ServiceDuration"), TEXT("Text_Occupancy"), TEXT("Text_FacilityTodayRevenue")})
        if (!FindText(*Tree,Name)) return false;
    BP.Modify(); Tree->Modify();
    // Preserve original named widgets/bindings in a collapsed archive; no runtime tree edits.
    auto* Retired = MakeWidget<UVerticalBox>(BP,*Tree,TEXT("Clarity_InfoV1"));
    V(*Shell,*Retired); Retired->SetVisibility(ESlateVisibility::Collapsed);
    auto Replace = [&](UBorder& Border,const TCHAR* Name)
    {
        if(UWidget* Old=Border.GetContent()) V(*Retired,*Old);
        auto* Body=MakeWidget<UVerticalBox>(BP,*Tree,Name);
        Border.AddChild(Body); Border.SetPadding(FMargin(10,8));
        Border.SetBrush(Brush(Soft)); Border.SetBrushColor(FLinearColor::White);
        return Body;
    };
    auto* Basics=Replace(*Build,TEXT("Clarity_BasicFacts"));
    Pair(BP,*Tree,*Basics,TEXT("Text_QueueCapacityValue"),TEXT("대기 정원"),true);
    Pair(BP,*Tree,*Basics,TEXT("Text_ServiceCapacityValue"),TEXT("동시 이용"));
    Pair(BP,*Tree,*Basics,TEXT("Text_ServiceDuration"),TEXT("팀당 이용 시간"));
    Pair(BP,*Tree,*Basics,TEXT("Text_Footprint"),TEXT("시설 크기"));
    Pair(BP,*Tree,*Basics,TEXT("Text_Clearance"),TEXT("안전 여유"));
    Pair(BP,*Tree,*Basics,TEXT("Text_InstallCost"),TEXT("설치 비용"));
    Pair(BP,*Tree,*Basics,TEXT("Text_RecoveryCost"),TEXT("철거 회수액"));
    Pair(BP,*Tree,*Basics,TEXT("Text_InstanceLimit"),TEXT("건축 한도"));
    auto* Operation=Replace(*Live,TEXT("Clarity_OperationFacts"));
    if(auto* State=FindText(*Tree,TEXT("Text_OperationState")))
    { Font(State,16,true); V(*Operation,*State,FMargin(0,0,0,8)); }
    Pair(BP,*Tree,*Operation,TEXT("Text_Occupancy"),TEXT("대기·처리 중 팀"),true);
    Pair(BP,*Tree,*Operation,TEXT("Text_WaitingParties"),TEXT("대기·처리 중 일행"));
    Pair(BP,*Tree,*Operation,TEXT("Text_RemainingService"),TEXT("남은 서비스 팀"));
    Pair(BP,*Tree,*Operation,TEXT("Text_ExpectedWait"),TEXT("1인 완료 예상"));
    Pair(BP,*Tree,*Operation,TEXT("Text_Throughput"),TEXT("처리 속도"));
    for(UWidget* Old:Revenue->GetAllChildren()) V(*Retired,*Old);
    auto* Money=MakeWidget<UBorder>(BP,*Tree,TEXT("Clarity_RevenueCard"));
    auto* MoneyBody=MakeWidget<UVerticalBox>(BP,*Tree,TEXT("Clarity_RevenueFacts"));
    Money->SetBrush(Brush(Soft)); Money->SetPadding(FMargin(10,8)); Money->AddChild(MoneyBody);
    V(*Revenue,*Money);
    Pair(BP,*Tree,*MoneyBody,TEXT("Text_FacilityTodayRevenue"),TEXT("오늘 매출"),true);
    Pair(BP,*Tree,*MoneyBody,TEXT("Text_FacilityTotalRevenue"),TEXT("축제 누적"));
    Pair(BP,*Tree,*MoneyBody,TEXT("Text_FacilityRevenueCount"),TEXT("결제 건수"));
    Pair(BP,*Tree,*MoneyBody,TEXT("Text_FacilityRevenueShare"),TEXT("오늘 매출 비중"));
    if(auto* Status=FindText(*Tree,TEXT("Text_FacilityRevenueStatus")))
    { Font(Status,14); V(*MoneyBody,*Status,FMargin(0,6,0,6)); }
    if(auto* Ranking=Cast<UButton>(Tree->FindWidget(TEXT("Button_RevenueRanking"))))
    { StyleButton(*Ranking); V(*Revenue,*Ranking,FMargin(0,10,0,0)); }

    // Keep four short primary tabs; optional tools sit on a separate row instead of shrinking labels.
    if(auto* Text=FindText(*Tree,TEXT("Text_TabOverview"))) Text->SetText(FText::FromString(TEXT("기본")));
    for(UWidget* Child:Tabs->GetAllChildren())
        if(auto* Tab=Cast<UButton>(Child)) StyleButton(*Tab,false,true);
    auto* More=MakeWidget<UHorizontalBox>(BP,*Tree,TEXT("Clarity_SecondaryTabs"));
    H(*More,*Button(BP,*Tree,TEXT("Button_TabRevenue"),TEXT("Clarity_RevenueTabLabel"),TEXT("매출"),true));
    H(*More,*Button(BP,*Tree,TEXT("Button_TabPrograms"),TEXT("Clarity_ProgramTabLabel"),TEXT("프로그램"),true));
    if(auto* Lighting=Cast<UButton>(Tree->FindWidget(TEXT("Button_TabLighting"))))
    { StyleButton(*Lighting,false,true); H(*More,*Lighting); }
    V(*Navigation,*More,FMargin(8,0,8,6));
    auto* Pager=MakeWidget<UHorizontalBox>(BP,*Tree,TEXT("Clarity_SynergyPager"));
    H(*Pager,*Button(BP,*Tree,TEXT("Button_SynergyPrevious"),TEXT("Clarity_PreviousLabel"),TEXT("이전")));
    auto* Page=Label(BP,*Tree,TEXT("Text_SynergyPage"),TEXT("1 / 1"),14,true);
    Page->SetJustification(ETextJustify::Center); H(*Pager,*Page);
    H(*Pager,*Button(BP,*Tree,TEXT("Button_SynergyNext"),TEXT("Clarity_NextLabel"),TEXT("다음")));
    V(*Synergy,*Pager,FMargin(0,8,0,0));
    if(auto* Summary=FindText(*Tree,TEXT("Text_SynergySummary"))) Font(Summary,14);

    for(const TCHAR* Name:{TEXT("Text_FacilityRevenue"),TEXT("Text_FacilityState")})
        if(UWidget* Text=Tree->FindWidget(Name)) V(*Retired,*Text);
    Font(FindText(*Tree,TEXT("Text_FacilityName")),20,true);
    for(const TCHAR* Name:{TEXT("Text_Category"),TEXT("Text_Zone")}) Font(FindText(*Tree,Name),14);
    for(const TCHAR* Name:{TEXT("Upgrade_Card"),TEXT("Program_Card"),TEXT("V2_SynergySummaryCard")})
        if(auto* Card=Cast<UBorder>(Tree->FindWidget(Name)))
        { Card->SetBrush(Brush(Soft)); Card->SetBrushColor(FLinearColor::White); Card->SetPadding(FMargin(10,8)); }
    // The comparison remains a real table, with quieter cells and enough room for the quote.
    for(const TCHAR* Key:{TEXT("Revenue"),TEXT("Time"),TEXT("Throughput"),TEXT("Size")})
        for(const TCHAR* Period:{TEXT("Current"),TEXT("Next")})
        {
            const FString Name=FString(TEXT("Text_Upgrade"))+Period+Key;
            Font(FindText(*Tree,*Name),14,true);
            if(auto* Cell=Cast<UBorder>(Tree->FindWidget(FName(Name+TEXT("_Cell")))))
            { Cell->SetBrush(Brush(Paper));Cell->SetBrushColor(FLinearColor::White);Cell->SetPadding(FMargin(4,3)); }
        }
    for(const TCHAR* Name:{TEXT("Panel_Size"),TEXT("Panel_ShadowSize")})
        if(auto* Size=Cast<USizeBox>(Tree->FindWidget(Name)))
        {
            Size->SetWidthOverride(456); Size->SetHeightOverride(620);
            if(auto* Slot=Cast<UCanvasPanelSlot>(Size->Slot))
            { Slot->SetAnchors(FAnchors(1,0));Slot->SetAlignment(FVector2D(1,0));Slot->SetOffsets(FMargin(-18,18,456,620));Slot->SetAutoSize(false); }
        }
    if(auto* Header=Cast<USizeBox>(Tree->FindWidget(TEXT("Header_Size")))) Header->SetHeightOverride(42);
    if(auto* Thumbnail=Cast<USizeBox>(Tree->FindWidget(TEXT("Thumbnail_Size"))))
    { Thumbnail->SetWidthOverride(66); Thumbnail->SetHeightOverride(68); }
    for(const TCHAR* Name:{TEXT("Border_Panel"),TEXT("Header_Surface"),TEXT("Panel_PersistentIdentity"),TEXT("Body_Surface"),TEXT("Panel_Actions"),TEXT("Panel_UpgradeActions")})
        if(auto* Border=Cast<UBorder>(Tree->FindWidget(Name)))
        {
            Border->SetBrush(FCString::Strcmp(Name,TEXT("Border_Panel"))==0?Material(TEXT("Paper"),Paper):Brush(Paper));
            Border->SetBrushColor(FLinearColor::White);
            if(FCString::Strcmp(Name,TEXT("Body_Surface"))==0) Border->SetPadding(FMargin(10,6));
        }
    if(auto* Scroll=Cast<UScrollBox>(Tree->FindWidget(TEXT("ScrollBox_Details"))))
    { Scroll->SetAnimateWheelScrolling(false); Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed); }
    for(const TCHAR* Name:{TEXT("Button_Move"),TEXT("Button_Remove"),TEXT("Button_Upgrade"),TEXT("Button_ScheduleProgram")})
        if(auto* Action=Cast<UButton>(Tree->FindWidget(Name)))
            StyleButton(*Action,FCString::Strcmp(Name,TEXT("Button_Remove"))!=0);
    return SaveNewBlueprint(TEXT("FF_BUILD_CLARITY_INFO"),BP);
}
static bool RefineFit(UWidgetBlueprint& BP)
{
    const TCHAR* Version=TEXT("FF.BuildClarity.InfoFitV1");
    if(BP.GetOutermost()->GetMetaData().GetValue(&BP,Version)==TEXT("1")) return true;
    UWidgetTree* Tree=BP.WidgetTree;
    if(!Tree || !Tree->FindWidget(TEXT("Clarity_InfoV1"))) return false;
    auto* Quote=Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_UpgradeQuote")));
    auto* Retired=Cast<UVerticalBox>(Tree->FindWidget(TEXT("Clarity_InfoV1")));
    auto* Price=FindText(*Tree,TEXT("Text_UpgradePrice"));
    auto* Balance=FindText(*Tree,TEXT("Text_UpgradeBalance"));
    auto* BalanceLabel=FindText(*Tree,TEXT("Text_UpgradeBalanceLabel"));
    if(!Quote || !Retired || !Price || !Balance || !BalanceLabel) return false;
    BP.Modify(); Tree->Modify();
    // Long localized values receive two-thirds of each line, avoiding an unnecessary
    // second line for a building limit while keeping every value immediately visible.
    Tree->ForEachWidget([&](UWidget* Widget)
    {
        if(auto* Row=Cast<UHorizontalBox>(Widget);Row&&Row->GetName().StartsWith(TEXT("Clarity_Row_")))
        {
            if(Row->GetChildrenCount()!=2) return;
            for(int32 Index=0;Index<2;++Index)
                if(auto* Slot=Cast<UHorizontalBoxSlot>(Row->GetChildAt(Index)->Slot))
                { FSlateChildSize Size(ESlateSizeRule::Fill);Size.Value=Index==0?.34f:.66f;Slot->SetSize(Size); }
            if(auto* Slot=Cast<UVerticalBoxSlot>(Row->Slot)) Slot->SetPadding(FMargin(0,2,0,2));
        }
    });
    for(const TCHAR* Key:{TEXT("Revenue"),TEXT("Time"),TEXT("Throughput"),TEXT("Size")})
    {
        const FString LabelName=FString(TEXT("V2_UpgradeLabel"))+Key;
        if(auto* Text=FindText(*Tree,*LabelName))
        {
            Font(Text,13);
            if(auto* Slot=Cast<UHorizontalBoxSlot>(Text->Slot)) Slot->SetPadding(FMargin(4,0));
        }
        const FString RowName=FString(TEXT("V2_UpgradeRow"))+Key;
        if(auto* Row=Tree->FindWidget(FName(RowName)))
            if(auto* Slot=Cast<UVerticalBoxSlot>(Row->Slot)) Slot->SetPadding(FMargin(0,0,0,2));
        for(const TCHAR* Period:{TEXT("Current"),TEXT("Next")})
        {
            const FString Name=FString(TEXT("Text_Upgrade"))+Period+Key;
            Font(FindText(*Tree,*Name),13,true);
            if(auto* Cell=Cast<UBorder>(Tree->FindWidget(FName(Name+TEXT("_Cell"))))) Cell->SetPadding(FMargin(4,2));
        }
    }
    Font(FindText(*Tree,TEXT("Text_FacilityLevel")),14,true);
    for(const TCHAR* Name:{TEXT("V2_UpgradeMetricCaption"),TEXT("V2_UpgradeCurrentCaption"),TEXT("V2_UpgradeNextCaption")})
    {
        auto* Text=FindText(*Tree,Name);Font(Text,13,true);
        if(Text) if(auto* Slot=Cast<UHorizontalBoxSlot>(Text->Slot)) Slot->SetPadding(FMargin(4,2));
    }
    // The full cost and remaining balance stay beside each other, above the unchanged
    // confirmation action. No quotation information is hidden to make the panel fit.
    for(UWidget* Old:Quote->GetAllChildren()) V(*Retired,*Old);
    auto* QuoteRow=MakeWidget<UHorizontalBox>(BP,*Tree,TEXT("Clarity_UpgradeQuoteRow"));
    auto* CostColumn=MakeWidget<UVerticalBox>(BP,*Tree,TEXT("Clarity_UpgradeCostColumn"));
    auto* BalanceColumn=MakeWidget<UVerticalBox>(BP,*Tree,TEXT("Clarity_UpgradeBalanceColumn"));
    V(*CostColumn,*Label(BP,*Tree,TEXT("Clarity_UpgradeCostCaption"),TEXT("강화 비용"),13));
    Font(Price,15,true);V(*CostColumn,*Price);
    Font(BalanceLabel,13);V(*BalanceColumn,*BalanceLabel);
    Font(Balance,15,true);V(*BalanceColumn,*Balance);
    H(*QuoteRow,*CostColumn);H(*QuoteRow,*BalanceColumn);V(*Quote,*QuoteRow,FMargin(4,2,4,2));
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1"));
    return SaveNewBlueprint(TEXT("FF_BUILD_CLARITY_INFO_FIT"),BP);
}
static bool RefineSpacing(UWidgetBlueprint& BP)
{
    const TCHAR* Version=TEXT("FF.BuildClarity.InfoFitV2");
    if(BP.GetOutermost()->GetMetaData().GetValue(&BP,Version)==TEXT("1")) return true;
    UWidgetTree* Tree=BP.WidgetTree;
    auto* Section=Tree?Tree->FindWidget(TEXT("Panel_UpgradeSection")):nullptr;
    auto* Card=Tree?Cast<UBorder>(Tree->FindWidget(TEXT("Upgrade_Card"))):nullptr;
    auto* SectionSlot=Section?Cast<UVerticalBoxSlot>(Section->Slot):nullptr;
    if(!SectionSlot || !Card || !Tree->FindWidget(TEXT("Clarity_UpgradeQuoteRow"))) return false;
    BP.Modify(); Tree->Modify();
    // The inherited section gutter sat beneath the whole comparison card, producing
    // seven pixels of empty overflow even when every value was already visible.
    // Recover twelve pixels inside the existing 620px panel rather than growing it.
    SectionSlot->SetPadding(FMargin(0));
    Card->SetPadding(FMargin(10,6));
    for(const TCHAR* Name:{TEXT("Text_UpgradePrice"),TEXT("Text_UpgradeBalance")})
        if(auto* Value=FindText(*Tree,Name)) Value->SetJustification(ETextJustify::Left);
    for(const TCHAR* Name:{TEXT("Clarity_UpgradeCostColumn"),TEXT("Clarity_UpgradeBalanceColumn")})
        if(UWidget* Column=Tree->FindWidget(Name))
            if(auto* Slot=Cast<UHorizontalBoxSlot>(Column->Slot)) Slot->SetPadding(FMargin(6,0));
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1"));
    return SaveNewBlueprint(TEXT("FF_BUILD_CLARITY_INFO_SPACING"),BP);
}
static void Run()
{
    auto* BP=LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    const bool Success=BP&&Apply(*BP)&&RefineFit(*BP)&&RefineSpacing(*BP);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_CLARITY_INFO_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClarityInfo"),
    TEXT("Apply compact facility facts, direct revenue/program pages and paginated combinations once, preserving all gameplay bindings."),
    FConsoleCommandDelegate::CreateStatic(&Run));
static void RunRefine()
{
    auto* BP=LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    const bool Success=BP&&RefineFit(*BP)&&RefineSpacing(*BP);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_CLARITY_INFO_FIT_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand RefineCommand(TEXT("FF.RefineBuildModeClarityInfoFit"),
    TEXT("Fit the compact inspector's basic facts and full upgrade quote without scrolling; apply once and preserve Designer edits."),
    FConsoleCommandDelegate::CreateStatic(&RunRefine));
static void RunSpacing()
{
    auto* BP=LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    const bool Success=BP&&RefineSpacing(*BP);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_CLARITY_INFO_SPACING_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand SpacingCommand(TEXT("FF.RefineBuildModeClarityInfoSpacing"),
    TEXT("Remove inherited empty upgrade gutters and align the two quote columns, preserving the compact inspector size."),
    FConsoleCommandDelegate::CreateStatic(&RunSpacing));
}
#endif
