// FF_FacilityAuroraInfoBuilder.cpp
// 역할: 시설 정보 화면에 Aurora 디자인을 적용한다.

// Requested V7 facility inspector migration. Existing command/data bindings are retained.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "WidgetBlueprint.h"

namespace FFFacilityAuroraInfo
{
using namespace FFUIWbpBuilder;
static const TCHAR* Package = FFBuildUIAssetPaths::FacilityInfoPanelPackage;
static const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0x20,0x4c,0x36));

static void Add(UVerticalBox& Box, UWidget& Child, FMargin Padding = FMargin(0,0,0,8))
{
    Child.RemoveFromParent();
    Box.AddChildToVerticalBox(&Child)->SetPadding(Padding);
}
static void Font(UCommonTextBlock* Text, int32 Size, bool Strong = false)
{
    if (!Text) return;
    StyleText(*Text, Size, Ink, Strong ? TEXT("SemiBold") : TEXT("Regular"));
    Text->SetAutoWrapText(true);
}
static UCommonTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name,
    const TCHAR* Copy, int32 Size = 17, bool Variable = false)
{
    auto* Text = MakeWidget<UCommonTextBlock>(BP, Tree, Name, Variable);
    Text->SetText(FText::FromString(Copy)); Font(Text, Size);
    return Text;
}
static UCommonTextBlock* Bound(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<UCommonTextBlock>(Tree.FindWidget(Name));
}
static void Pair(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Box,
    const TCHAR* Name, const TCHAR* Caption, int32 Size = 17)
{
    auto* Value = Bound(Tree, Name); if (!Value) return;
    auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, FName(FString(TEXT("Aurora_Row_"))+Name));
    auto* Key = Label(BP,Tree,FName(FString(TEXT("Aurora_Label_"))+Name),Caption,16);
    Row->AddChildToHorizontalBox(Key)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Value->RemoveFromParent(); Font(Value,Size,true);
    Value->SetJustification(ETextJustify::Right);
    Row->AddChildToHorizontalBox(Value)->SetVerticalAlignment(VAlign_Center);
    Add(Box,*Row,FMargin(0,3,0,3));
}
static UVerticalBox* Card(UWidgetBlueprint& BP,UWidgetTree& Tree,UVerticalBox& Parent,
    const TCHAR* Name,const TCHAR* Title)
{
    auto* Border=MakeWidget<UBorder>(BP,Tree,Name);
    auto* Body=MakeWidget<UVerticalBox>(BP,Tree,FName(FString(Name)+TEXT("_Body")));
    Border->SetPadding(FMargin(15,12)); Border->SetBrushColor(FLinearColor::White);
    Border->AddChild(Body); Add(Parent,*Border);
    Add(*Body,*Label(BP,Tree,FName(FString(Name)+TEXT("_Title")),Title,19),FMargin(0,0,0,8));
    return Body;
}
static bool Apply(UWidgetBlueprint& BP)
{
    UWidgetTree* Tree=BP.WidgetTree; if(!Tree) return false;
    if(Tree->FindWidget(TEXT("Aurora_InfoV7"))) return true;
    auto* Shell=Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_Shell")));
    auto* Content=Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_Content")));
    auto* Build=Cast<UBorder>(Tree->FindWidget(TEXT("Panel_BuildSection")));
    auto* Live=Cast<UBorder>(Tree->FindWidget(TEXT("Panel_LiveSection")));
    auto* Revenue=Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_RevenueSection")));
    auto* Identity=Cast<UVerticalBox>(Tree->FindWidget(TEXT("V2_IdentityMeta")));
    if(!Shell||!Content||!Build||!Live||!Revenue||!Identity) return false;
    for(const TCHAR* Name:{TEXT("Text_QueueCapacity"),TEXT("Text_ServiceDuration"),TEXT("Text_Occupancy"),
        TEXT("Text_FacilityTodayRevenue"),TEXT("Text_FacilityTotalRevenue"),TEXT("Text_Throughput")})
        if(!Bound(*Tree,Name)) return false;
    BP.Modify(); Tree->Modify();
    // Preserve legacy named widgets and their bindings in a collapsed branch.
    auto* Retired=MakeWidget<UVerticalBox>(BP,*Tree,TEXT("Aurora_InfoV7"));
    Add(*Shell,*Retired,FMargin(0)); Retired->SetVisibility(ESlateVisibility::Collapsed);
    auto ReplaceBody=[&](UBorder& Border,const TCHAR* Name)
    {
        if(UWidget* Old=Border.GetContent()) Add(*Retired,*Old,FMargin(0));
        auto* Body=MakeWidget<UVerticalBox>(BP,*Tree,Name); Border.AddChild(Body);
        Border.SetPadding(FMargin(0)); return Body;
    };
    auto* BuildBody=ReplaceBody(*Build,TEXT("Aurora_BuildBody"));
    auto* Queue=Card(BP,*Tree,*BuildBody,TEXT("Aurora_QueueHero"),TEXT("대기·이용 정원"));
    auto* Capacity=Label(BP,*Tree,TEXT("Text_QueueCapacityValue"),TEXT("—"),48,true);
    Font(Capacity,48,true); Add(*Queue,*Capacity,FMargin(0,4,0,8));
    Add(*Retired,*Label(BP,*Tree,TEXT("Text_ServiceCapacityValue"),TEXT("—"),18,true));
    Pair(BP,*Tree,*Queue,TEXT("Text_ServiceCapacityValue"),TEXT("동시 이용"));
    Pair(BP,*Tree,*Queue,TEXT("Text_ServiceDuration"),TEXT("팀당 이용 시간"));
    Add(*Queue,*Label(BP,*Tree,TEXT("Aurora_QueueNotice"),TEXT("실시간 대기 현황은 운영 탭에서 확인하세요."),15),FMargin(0,6,0,0));
    auto* Facts=Card(BP,*Tree,*BuildBody,TEXT("Aurora_BuildFacts"),TEXT("배치 정보"));
    Pair(BP,*Tree,*Facts,TEXT("Text_Footprint"),TEXT("시설 크기"));
    Pair(BP,*Tree,*Facts,TEXT("Text_Clearance"),TEXT("안전 여유"));
    Pair(BP,*Tree,*Facts,TEXT("Text_InstallCost"),TEXT("설치 비용"));
    Pair(BP,*Tree,*Facts,TEXT("Text_RecoveryCost"),TEXT("철거 회수액"));
    Pair(BP,*Tree,*Facts,TEXT("Text_InstanceLimit"),TEXT("건축 한도"));
    auto* LiveBody=ReplaceBody(*Live,TEXT("Aurora_LiveBody"));
    auto* Occupancy=Card(BP,*Tree,*LiveBody,TEXT("Aurora_OperationQueueHero"),TEXT("실시간 대기·이용"));
    if(auto* State=Bound(*Tree,TEXT("Text_OperationState"))){Font(State,17,true);Add(*Occupancy,*State);}
    auto* Count=Bound(*Tree,TEXT("Text_Occupancy")); Font(Count,42,true); Add(*Occupancy,*Count);
    Pair(BP,*Tree,*Occupancy,TEXT("Text_WaitingParties"),TEXT("대기·처리 중 일행"));
    Pair(BP,*Tree,*Occupancy,TEXT("Text_RemainingService"),TEXT("남은 서비스 팀"));
    Pair(BP,*Tree,*Occupancy,TEXT("Text_ExpectedWait"),TEXT("1인 이용 완료 예상"));
    Pair(BP,*Tree,*Occupancy,TEXT("Text_Throughput"),TEXT("처리 속도"));
    if(UWidget* Ranking=Tree->FindWidget(TEXT("Button_RevenueRanking"))) Add(*LiveBody,*Ranking);
    // One compact earnings card is shared by overview and operation, using existing live bindings.
    TArray<UWidget*> OldRevenue=Revenue->GetAllChildren();
    for(UWidget* Old:OldRevenue) Add(*Retired,*Old,FMargin(0));
    auto* Money=Card(BP,*Tree,*Revenue,TEXT("Aurora_RevenueSummary"),TEXT("시설 수익"));
    for(const auto& Entry:TArray<TPair<const TCHAR*,const TCHAR*>>{
        {TEXT("Text_FacilityTodayRevenue"),TEXT("오늘 매출")},{TEXT("Text_FacilityTotalRevenue"),TEXT("축제 누적")},
        {TEXT("Text_FacilityRevenueCount"),TEXT("결제 건수")},{TEXT("Text_FacilityRevenueShare"),TEXT("오늘 매출 비중")}})
        Pair(BP,*Tree,*Money,Entry.Key,Entry.Value,FCString::Strcmp(Entry.Key,TEXT("Text_FacilityTodayRevenue"))==0?22:17);
    if(auto* Status=Bound(*Tree,TEXT("Text_FacilityRevenueStatus"))){Font(Status,15);Add(*Money,*Status,FMargin(0,6,0,0));}
    for(const TCHAR* Name:{TEXT("Text_FacilityState"),TEXT("Text_FacilityRevenue")})
        if(auto* Text=Bound(*Tree,Name)){Font(Text,15);Add(*Identity,*Text,FMargin(0,3,0,0));}
    if(UWidget* Summary=Tree->FindWidget(TEXT("Summary_Card"))) Summary->SetVisibility(ESlateVisibility::Collapsed);
    Content->ShiftChild(0,Live); Content->ShiftChild(1,Build); Content->ShiftChild(2,Revenue);
    if(UWidget* Program=Tree->FindWidget(TEXT("Panel_ProgramSection"))) Content->ShiftChild(3,Program);
    for(const TCHAR* Name:{TEXT("Panel_Size"),TEXT("Panel_ShadowSize")})
        if(auto* Size=Cast<USizeBox>(Tree->FindWidget(Name)))
        {
            Size->SetWidthOverride(592); Size->ClearHeightOverride();
            if(auto* Slot=Cast<UCanvasPanelSlot>(Size->Slot))
            {Slot->SetAnchors(FAnchors(1,0,1,1));Slot->SetAlignment(FVector2D(1,0));Slot->SetOffsets(FMargin(-18,18,592,66));Slot->SetAutoSize(false);}
        }
    if(auto* Header=Cast<USizeBox>(Tree->FindWidget(TEXT("Header_Size")))) Header->SetHeightOverride(56);
    if(auto* Thumbnail=Cast<USizeBox>(Tree->FindWidget(TEXT("Thumbnail_Size"))))
    {Thumbnail->SetWidthOverride(84);Thumbnail->SetHeightOverride(78);}
    return SaveNewBlueprint(TEXT("FF_FACILITY_AURORA_INFO"),BP);
}
static void Run()
{
    UWidgetBlueprint* BP=LoadExistingBlueprint(Package);
    const bool Success=BP&&Apply(*BP);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_FACILITY_AURORA_INFO_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityAuroraInfo"),
    TEXT("Apply the requested V7 queue-first facility inspector once, preserving named data/command bindings."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
