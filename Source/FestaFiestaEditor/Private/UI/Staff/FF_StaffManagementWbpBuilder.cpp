// 스태프 통합 관리: 저장된 WBP로 시설 배정과 숙련도 목록을 함께 배치한다.
// 기존 고용/승급 위젯 객체와 바인딩은 보관하고, 런타임에서는 데이터만 연결한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
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
#include "UI/LiveHUD/Panels/FF_LivePanelStaff.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelStaff.h"
#include "UI/Staff/FF_StaffPromotionRowWidget.h"
#include "UI/Staff/FF_StaffTrainingWidget.h"
#include "UI/Staff/FF_StaffManagementWidget.h"
#include "UI/Staff/FF_StaffFacilityRowWidget.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFStaffManagementBuilder
{
using namespace FFUIWbpBuilder;
using namespace FFPrepPanelSkyBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_STAFF_MANAGEMENT");
constexpr const TCHAR* Version = TEXT("FF.StaffManagement.Version");
const FString Folder=TEXT("/Game/FestaFiesta/UI/Staff");
const FString ManagementPackage=Folder/TEXT("WBP_FF_StaffManagement");
const FString FacilityRowPackage=Folder/TEXT("WBP_FF_StaffFacilityRow");
const FString TrainingPackage=Folder/TEXT("WBP_FF_StaffTraining");
const FString PromotionPackage=Folder/TEXT("WBP_FF_StaffPromotionRow");
const FString PrepPackage=TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff");
const FString LivePackage=TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff");
const FString PrepHudPackage=TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar");
const FString LiveHudPackage=TEXT("/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen");

template<typename T> T* Get(UWidgetTree& Tree,const TCHAR* Name) { return Cast<T>(Tree.FindWidget(Name)); }
bool Applied(UWidgetBlueprint& BP) { return BP.GetOutermost()->GetMetaData().GetValue(&BP,Version)==TEXT("1"); }
bool Require(UWidgetBlueprint& BP,std::initializer_list<const TCHAR*> Names)
{
    if (!BP.WidgetTree || BP.GetOutermost()->IsDirty()) return false;
    for (const TCHAR* Name:Names) if (!BP.WidgetTree->FindWidget(Name))
    { UE_LOG(LogFFUIWbpBuilder,Error,TEXT("%s_MISSING %s.%s"),Prefix,*BP.GetName(),Name); return false; }
    return true;
}
void Font(UTextBlock* W,int Size=15,FLinearColor Tint=Ink,bool Bold=false)
{
    auto F=W->GetFont(); F.Size=Size; F.TypefaceFontName=Bold?TEXT("Bold"):TEXT("Medium");
    W->SetFont(F); W->SetColorAndOpacity(Tint); W->SetAutoWrapText(false);
    W->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
}
UTextBlock* Label(UWidgetBlueprint& BP,UWidgetTree& T,const TCHAR* Name,const TCHAR* Value,int Size=15,bool Variable=false,FLinearColor Tint=Ink)
{
    auto* W=Text(BP,T,Name,FText::FromString(Value),Size,Tint,Variable);
    W->SetAutoWrapText(false); W->SetVisibility(ESlateVisibility::HitTestInvisible);
    W->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); return W;
}
void V(UVerticalBox* P,UWidget* W,FMargin Padding=FMargin(0),bool Fill=false)
{
    W->RemoveFromParent(); auto* S=P->AddChildToVerticalBox(W); S->SetPadding(Padding);
    S->SetHorizontalAlignment(HAlign_Fill); S->SetSize(FSlateChildSize(Fill?ESlateSizeRule::Fill:ESlateSizeRule::Automatic));
}
void H(UHorizontalBox* P,UWidget* W,FMargin Padding=FMargin(0),float Weight=0)
{
    W->RemoveFromParent(); auto* S=P->AddChildToHorizontalBox(W); S->SetPadding(Padding); S->SetVerticalAlignment(VAlign_Center);
    FSlateChildSize Size(Weight>0?ESlateSizeRule::Fill:ESlateSizeRule::Automatic); Size.Value=Weight; S->SetSize(Size);
}
USizeBox* Size(UWidgetBlueprint& BP,UWidgetTree& T,const TCHAR* Name,UWidget* Child,float Width=0,float Height=0)
{
    auto* W=MakeWidget<USizeBox>(BP,T,Name); if(Width>0)W->SetWidthOverride(Width); if(Height>0)W->SetHeightOverride(Height);
    Child->RemoveFromParent(); W->AddChild(Child); return W;
}
UBorder* Card(UWidgetBlueprint& BP,UWidgetTree& T,const TCHAR* Name,UWidget* Child,FMargin Padding=FMargin(12),FLinearColor Color=Paper)
{
    auto* W=MakeWidget<UBorder>(BP,T,Name); W->SetBrush(Brush(Color,5,1,SkyLine)); W->SetPadding(Padding);
    Child->RemoveFromParent(); W->AddChild(Child); return W;
}
UButton* Action(UWidgetBlueprint& BP,UWidgetTree& T,const TCHAR* Name,const TCHAR* Caption,bool Primary=false,const TCHAR* TextName=nullptr)
{
    const FString LabelName=TextName?FString(TextName):FString(Name)+TEXT("_Label");
    auto* W=Button(BP,T,Name,*Label(BP,T,*LabelName,Caption,15,TextName!=nullptr,Primary?FLinearColor::White:Ink),Primary);
    CastChecked<UTextBlock>(W->GetContent())->SetJustification(ETextJustify::Center); return W;
}
void StyleScroll(UScrollBox* W)
{
    W->SetAllowOverscroll(false); W->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    W->SetScrollbarThickness(FVector2D(6,6)); W->SetClipping(EWidgetClipping::ClipToBounds);
    auto Style=W->GetWidgetBarStyle();
    Style.NormalThumbImage=Brush(Color(130,185,200),3); Style.HoveredThumbImage=Brush(Color(94,161,180),3); Style.DraggedThumbImage=Brush(Ink,3);
    Style.NormalThumbImage.ImageSize=Style.HoveredThumbImage.ImageSize=Style.DraggedThumbImage.ImageSize=FVector2D(6,20);
    Style.VerticalBackgroundImage=Brush(SkyWash,3); Style.VerticalTopSlotImage=Style.VerticalBottomSlotImage=Brush(FLinearColor::Transparent);
    Style.Thickness=6; W->SetWidgetBarStyle(Style);
    auto ScrollStyle=W->GetWidgetStyle(); ScrollStyle.TopShadowBrush=ScrollStyle.BottomShadowBrush=Brush(FLinearColor::Transparent);
    W->SetWidgetStyle(ScrollStyle);
}
UScrollBox* Scroll(UWidgetBlueprint& BP,UWidgetTree& T,const TCHAR* Name,UWidget* Child)
{
    auto* W=MakeWidget<UScrollBox>(BP,T,Name); StyleScroll(W); Child->RemoveFromParent();
    auto* S=CastChecked<UScrollBoxSlot>(W->AddChild(Child)); S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0,0,7,0)); return W;
}
UVerticalBox* ParkRoot(UWidgetBlueprint& BP,const TCHAR* Name)
{
    UWidgetTree& T=*BP.WidgetTree; UWidget* Original=T.RootWidget.Get();
    auto* Root=MakeWidget<UVerticalBox>(BP,T,Name); T.RootWidget=Root;
    Original->SetVisibility(ESlateVisibility::Collapsed); V(Root,Original); return Root;
}
bool Backup(const FString& Package,const FString& Directory)
{
    if(!FPackageName::DoesPackageExist(Package))return true;
    const FString File=FPackageName::LongPackageNameToFilename(Package,TEXT(".uasset"));
    return IFileManager::Get().Copy(*(Directory/FPaths::GetCleanFilename(File)),*File,false,false)==COPY_OK;
}
bool Save(UWidgetBlueprint& BP)
{
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1"));
    if(SaveNewBlueprint(Prefix,BP))return true;
    BP.GetOutermost()->GetMetaData().RemoveValue(&BP,Version); return false;
}
bool BuildFacility(UWidgetBlueprint& BP,UWidgetTree& T)
{
    auto* Rows=MakeWidget<UVerticalBox>(BP,T,TEXT("VerticalBox_Facility"));
    auto* Header=MakeWidget<UHorizontalBox>(BP,T,TEXT("HorizontalBox_FacilityHeader"));
    H(Header,Label(BP,T,TEXT("Text_FacilityName"),TEXT("안내시설"),18,true),FMargin(0,0,10,0),1);
    auto* Capacity=Label(BP,T,TEXT("Text_AssignedCapacity"),TEXT("— / —명"),17,true); Capacity->SetJustification(ETextJustify::Right); H(Header,Capacity);
    V(Rows,Header,FMargin(0,0,0,8));
    auto* Status=Label(BP,T,TEXT("Text_FacilityStatus"),TEXT("배정 상태를 확인하고 있습니다."),14,true,Muted); Status->SetAutoWrapText(true);
    V(Rows,Status,FMargin(0,0,0,10));
    auto* Actions=MakeWidget<UHorizontalBox>(BP,T,TEXT("HorizontalBox_FacilityActions"));
    H(Actions,Size(BP,T,TEXT("SizeBox_Locate"),Action(BP,T,TEXT("Button_Locate"),TEXT("위치 보기")),116,34));
    H(Actions,Label(BP,T,TEXT("Text_ActionSpacer"),TEXT("")),FMargin(0),1);
    H(Actions,Size(BP,T,TEXT("SizeBox_Unassign"),Action(BP,T,TEXT("Button_Unassign"),TEXT("−")),38,34),FMargin(8,0,0,0));
    H(Actions,Size(BP,T,TEXT("SizeBox_Assign"),Action(BP,T,TEXT("Button_Assign"),TEXT("+")),38,34),FMargin(8,0,0,0));
    V(Rows,Actions);
    auto* Columns=MakeWidget<UHorizontalBox>(BP,T,TEXT("HorizontalBox_FacilityContents"));
    auto* Icon=MakeWidget<UImage>(BP,T,TEXT("Image_Facility"));
    auto* Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Facilities/T_FF_LH_Icon_FacilityInfo_White_256.T_FF_LH_Icon_FacilityInfo_White_256"));
    Icon->SetBrushFromTexture(Texture); Icon->SetColorAndOpacity(Ink);
    H(Columns,Size(BP,T,TEXT("SizeBox_FacilityIcon"),Icon,42,42),FMargin(0,0,14,0)); H(Columns,Rows,FMargin(0),1);
    T.RootWidget=Card(BP,T,TEXT("Border_Facility"),Columns,FMargin(14));
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1")); return true;
}
void Metric(UWidgetBlueprint& BP,UWidgetTree& T,UHorizontalBox* Parent,const TCHAR* Key,const TCHAR* Caption,const TCHAR* Value,const TCHAR* Note,bool First=false)
{
    const FString Stem=FString(TEXT("Management_"))+Key;
    auto* Body=MakeWidget<UVerticalBox>(BP,T,FName(Stem+TEXT("_Body")));
    auto* Heading=Label(BP,T,*(FString(TEXT("Text_"))+Key+TEXT("Label")),Caption,15,FString(Key)!=TEXT("Total"),Muted); Heading->SetJustification(ETextJustify::Center);
    auto* Number=Label(BP,T,Value,TEXT("—"),27,true); Number->SetJustification(ETextJustify::Center);
    auto* Detail=Label(BP,T,Note,TEXT(""),12,true,Muted); Detail->SetJustification(ETextJustify::Center);
    V(Body,Heading,FMargin(0,0,0,4)); V(Body,Number); V(Body,Detail,FMargin(0,3,0,0));
    H(Parent,Card(BP,T,*(Stem+TEXT("_Card")),Body,FMargin(12,9),SkyWash),FMargin(First?0:10,0,0,0),1);
}
bool BuildManagement(UWidgetBlueprint& BP,UWidgetTree& T)
{
    auto* Root=MakeWidget<UVerticalBox>(BP,T,TEXT("VerticalBox_Management")); T.RootWidget=Root;
    auto* Intro=MakeWidget<UHorizontalBox>(BP,T,TEXT("HorizontalBox_ManagementIntro"));
    H(Intro,Card(BP,T,TEXT("Border_Phase"),Label(BP,T,TEXT("Text_Phase"),TEXT("셋업"),14,true),FMargin(10,4),SkyWash),FMargin(0,0,12,0));
    H(Intro,Label(BP,T,TEXT("Text_Subtitle"),TEXT("안내시설 배정과 직원 숙련도를 함께 확인하세요."),15,true,Muted),FMargin(0),1); V(Root,Intro,FMargin(0,0,0,12));
    auto* Metrics=MakeWidget<UHorizontalBox>(BP,T,TEXT("HorizontalBox_ManagementMetrics"));
    Metric(BP,T,Metrics,TEXT("Total"),TEXT("전체 인원"),TEXT("Text_TotalStaff"),TEXT("Text_StaffBreakdown"),true);
    Metric(BP,T,Metrics,TEXT("Second"),TEXT("배정 인원"),TEXT("Text_SecondValue"),TEXT("Text_SecondNote"));
    Metric(BP,T,Metrics,TEXT("Third"),TEXT("미배정"),TEXT("Text_ThirdValue"),TEXT("Text_ThirdNote"));
    Metric(BP,T,Metrics,TEXT("Fourth"),TEXT("안내시설 빈자리"),TEXT("Text_FourthValue"),TEXT("Text_FourthNote")); V(Root,Metrics,FMargin(0,0,0,16));
    auto* Columns=MakeWidget<UHorizontalBox>(BP,T,TEXT("HorizontalBox_ManagementColumns"));
    auto* Facilities=MakeWidget<UVerticalBox>(BP,T,TEXT("VerticalBox_Facilities"));
    auto* FacilityHeader=MakeWidget<UHorizontalBox>(BP,T,TEXT("HorizontalBox_FacilitiesHeader"));
    H(FacilityHeader,Label(BP,T,TEXT("Text_FacilitiesTitle"),TEXT("안내시설 배정"),21),FMargin(0,0,8,0),1);
    H(FacilityHeader,Size(BP,T,TEXT("SizeBox_FillVacancies"),Action(BP,T,TEXT("Button_FillVacancies"),TEXT("빈자리 채우기"),true),136,36)); V(Facilities,FacilityHeader,FMargin(0,0,0,12));
    auto* List=MakeWidget<UVerticalBox>(BP,T,TEXT("Box_Facilities"),true);
    V(Facilities,Scroll(BP,T,TEXT("ScrollBox_Facilities"),List),FMargin(0,0,0,10),true);
    auto* Empty=Label(BP,T,TEXT("Text_FacilitiesEmpty"),TEXT("안내시설을 확인하고 있습니다."),15,true,Muted); Empty->SetAutoWrapText(true); V(Facilities,Empty,FMargin(0,0,0,8));
    auto* Guide=Label(BP,T,TEXT("Text_AssignmentGuide"),TEXT("미배정 인원과 안내시설의 빈자리를 확인하세요."),14,true); Guide->SetAutoWrapText(true);
    V(Facilities,Card(BP,T,TEXT("Border_AssignmentGuide"),Guide,FMargin(12,9),Color(255,247,230)));
    auto* Result=Label(BP,T,TEXT("Text_AssignmentResult"),TEXT(""),13,true,Muted); Result->SetAutoWrapText(true); V(Facilities,Result,FMargin(0,7,0,0));
    H(Columns,Facilities,FMargin(0,0,18,0),.43f); auto* Divider=Rule(BP,T,TEXT("Border_ManagementDivider"),true); H(Columns,Divider);
    CastChecked<UHorizontalBoxSlot>(Divider->Slot)->SetVerticalAlignment(VAlign_Fill);
    auto* Training=MakeWidgetOfClass(BP,T,TEXT("Widget_StaffTraining"),LoadGeneratedClass(TrainingPackage)); if(!Training)return false;
    H(Columns,Training,FMargin(18,0,0,0),.57f); CastChecked<UHorizontalBoxSlot>(Training->Slot)->SetVerticalAlignment(VAlign_Fill);
    CastChecked<UHorizontalBoxSlot>(Facilities->Slot)->SetVerticalAlignment(VAlign_Fill);
    V(Root,Columns,FMargin(0,0,0,12),true);
    auto* Incident=MakeWidget<UHorizontalBox>(BP,T,TEXT("HorizontalBox_Incident"));
    H(Incident,Label(BP,T,TEXT("Text_IncidentTitle"),TEXT("사건 대응"),18),FMargin(0,0,18,0));
    H(Incident,Label(BP,T,TEXT("Text_IncidentSummary"),TEXT("현재 사건을 확인하세요."),14,true),FMargin(0,0,12,0),1);
    H(Incident,Size(BP,T,TEXT("SizeBox_ViewIncident"),Action(BP,T,TEXT("Button_ViewIncident"),TEXT("사건 보기")),110,36));
    auto* IncidentGroup=Card(BP,T,TEXT("Group_Incident"),Incident,FMargin(14,9),SkyWash); IncidentGroup->bIsVariable=true;
    V(Root,IncidentGroup,FMargin(0,0,0,8));
    auto* Note=Label(BP,T,TEXT("Text_ManagementNote"),TEXT("출동·복귀 중인 스태프는 복귀 후 배정을 변경할 수 있습니다."),13,true,Muted); Note->SetAutoWrapText(true); V(Root,Note);
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1")); return true;
}
void MigratePromotion(UWidgetBlueprint& BP)
{
    auto& T=*BP.WidgetTree;
    auto* Columns=Get<UHorizontalBox>(T,TEXT("HorizontalBox_StaffRow"));
    auto* Identity=MakeWidget<UVerticalBox>(BP,T,TEXT("Management_StaffIdentity"));
    auto* Name=Get<UTextBlock>(T,TEXT("Text_StaffName")); Font(Name,15,Ink,true); V(Identity,Name);
    auto* Kind=Get<UTextBlock>(T,TEXT("Text_StaffKind")); Font(Kind,12,Muted); V(Identity,Kind,FMargin(0,3,0,0));
    H(Columns,Identity,FMargin(10,7,10,7),.27f); Columns->ShiftChild(0,Identity);
    auto* Level=Get<UTextBlock>(T,TEXT("Text_StaffLevel")); Font(Level,16,Ink,true);
    auto* LevelSize=Size(BP,T,TEXT("Management_Size_Level"),Level,60); H(Columns,LevelSize,FMargin(0,7,8,7)); Columns->ShiftChild(1,LevelSize);
    auto* Assignment=MakeWidget<UVerticalBox>(BP,T,TEXT("Management_StaffAssignment"));
    V(Assignment,Label(BP,T,TEXT("Text_AssignmentFacility"),TEXT("미배정"),14,true));
    V(Assignment,Label(BP,T,TEXT("Text_AssignmentStatus"),TEXT(""),12,true,Muted),FMargin(0,4,0,0));
    H(Columns,Assignment,FMargin(0,7,10,7),.40f); Columns->ShiftChild(2,Assignment);
    auto* ActionColumn=Get<UVerticalBox>(T,TEXT("VerticalBox_PromotionAction"));
    H(Columns,Size(BP,T,TEXT("Management_Size_Promotion"),ActionColumn,126),FMargin(0,7,10,7));
    Font(Get<UTextBlock>(T,TEXT("Text_Promote")),14,Ink,true);
    auto* Status=Get<UTextBlock>(T,TEXT("Text_PromotionStatus")); Font(Status,12,Muted); Status->SetAutoWrapText(true);
    Get<USizeBox>(T,TEXT("SizeBox_Promote"))->SetHeightOverride(32);
    Get<USizeBox>(T,TEXT("SizeBox_StaffRow"))->SetHeightOverride(72);
}
void MigrateTraining(UWidgetBlueprint& BP)
{
    auto& T=*BP.WidgetTree;
    auto* Title=Get<UTextBlock>(T,TEXT("Text_TrainingTitle"));
    auto* Header=Get<UBorder>(T,TEXT("Border_RosterHeader"));
    auto* Roster=Get<USizeBox>(T,TEXT("SizeBox_StaffRoster"));
    auto* Empty=Get<UTextBlock>(T,TEXT("Text_RosterEmpty"));
    auto* Note=Get<UTextBlock>(T,TEXT("Text_RosterNote"));
    auto* Result=Get<UBorder>(T,TEXT("Border_PromotionResult"));
    auto* Root=ParkRoot(BP,TEXT("Management_TrainingRoot"));
    auto* Top=MakeWidget<UHorizontalBox>(BP,T,TEXT("Management_TrainingTop"));
    Title->SetText(FText::FromString(TEXT("직원 · 숙련도"))); Font(Title,21,Ink,true); H(Top,Title,FMargin(0,0,8,0),1);
    H(Top,Size(BP,T,TEXT("Management_Size_AllFilter"),Action(BP,T,TEXT("Button_AllStaff"),TEXT("전체"),false,TEXT("Text_AllStaffFilter")),88,36),FMargin(0,0,6,0));
    H(Top,Size(BP,T,TEXT("Management_Size_UnassignedFilter"),Action(BP,T,TEXT("Button_UnassignedStaff"),TEXT("미배정"),false,TEXT("Text_UnassignedStaffFilter")),96,36));
    V(Root,Top,FMargin(0,0,0,12));
    auto* HeaderRow=Get<UHorizontalBox>(T,TEXT("HorizontalBox_RosterHeader"));
    Get<UTextBlock>(T,TEXT("Text_KindHeader"))->SetVisibility(ESlateVisibility::Collapsed);
    auto* NameHeader=Get<UTextBlock>(T,TEXT("Text_NameHeader")); NameHeader->SetText(FText::FromString(TEXT("직원"))); Font(NameHeader,14,Ink,true);
    H(HeaderRow,NameHeader,FMargin(10,7,10,7),.27f); HeaderRow->ShiftChild(0,NameHeader);
    auto* LevelHeader=Get<UTextBlock>(T,TEXT("Text_LevelHeader")); LevelHeader->SetText(FText::FromString(TEXT("숙련도"))); Font(LevelHeader,14,Ink,true);
    auto* LevelSize=Size(BP,T,TEXT("Management_Size_LevelHeader"),LevelHeader,60); H(HeaderRow,LevelSize,FMargin(0,7,8,7)); HeaderRow->ShiftChild(1,LevelSize);
    auto* Assignment=Label(BP,T,TEXT("Management_Text_AssignmentHeader"),TEXT("배정 / 상태"),14); H(HeaderRow,Assignment,FMargin(0,7,10,7),.40f); HeaderRow->ShiftChild(2,Assignment);
    auto* PromotionHeader=Get<UTextBlock>(T,TEXT("Text_PromotionHeader")); Font(PromotionHeader,14,Ink,true); PromotionHeader->SetJustification(ETextJustify::Center);
    H(HeaderRow,Size(BP,T,TEXT("Management_Size_PromotionHeader"),PromotionHeader,126),FMargin(0,7,10,7)); V(Root,Header,FMargin(0,0,0,8));
    Roster->ClearHeightOverride(); Roster->ClearMaxDesiredHeight(); Roster->ClearMinDesiredHeight();
    StyleScroll(Get<UScrollBox>(T,TEXT("ScrollBox_StaffRoster"))); V(Root,Roster,FMargin(0),true);
    Font(Empty,14,Muted); Empty->SetAutoWrapText(true); V(Root,Empty,FMargin(0,7,0,0));
    Font(Note,13,Muted); Note->SetAutoWrapText(true); V(Root,Note,FMargin(0,10,0,0));
    Font(Get<UTextBlock>(T,TEXT("Text_PromotionResult")),13,Ink); Result->SetPadding(FMargin(10,6)); V(Root,Result,FMargin(0,6,0,0));
}
void Quote(UWidgetBlueprint& BP,UWidgetTree& T,UHorizontalBox* Row,const TCHAR* Stem,const TCHAR* Caption,UTextBlock* Value)
{
    auto* Column=MakeWidget<UVerticalBox>(BP,T,FName(FString(TEXT("Management_Quote_"))+Stem));
    auto* Heading=Label(BP,T,*(FString(TEXT("Management_QuoteLabel_"))+Stem),Caption,13,false,Muted); Heading->SetJustification(ETextJustify::Center);
    Font(Value,22,Ink,true); Value->SetJustification(ETextJustify::Center); V(Column,Heading,FMargin(0,0,0,5));
    if(FString(Stem)==TEXT("Gold"))
    {
        auto* Amount=MakeWidget<UHorizontalBox>(BP,T,TEXT("Management_CurrentGoldAmount"));
        auto* Icon=MakeWidget<UImage>(BP,T,TEXT("Management_CurrentGoldIcon"));
        Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Gold_White_256.T_FF_LH_Icon_Gold_White_256")));
        H(Amount,Size(BP,T,TEXT("Management_CurrentGoldIconSize"),Icon,23,23),FMargin(0,0,7,0)); H(Amount,Value);
        V(Column,Amount); CastChecked<UVerticalBoxSlot>(Amount->Slot)->SetHorizontalAlignment(HAlign_Center);
    }
    else { V(Column,Value); }
    H(Row,Column,FMargin(12,0),1);
}
void MigrateHost(UWidgetBlueprint& BP,bool Prep,UClass* ManagementClass)
{
    auto& T=*BP.WidgetTree;
    auto* Sections=Get<UVerticalBox>(T,Prep?TEXT("VerticalBox_Sections"):TEXT("Sky_VerticalBox_Sections"));
    auto* TabRow=T.FindWidget(TEXT("Staff_TabRow")); auto* Switcher=T.FindWidget(TEXT("Switcher_StaffContent"));
    const int32 Index=Sections->GetChildIndex(TabRow);
    auto* Legacy=MakeWidget<UVerticalBox>(BP,T,TEXT("Management_Legacy")); Legacy->SetVisibility(ESlateVisibility::Collapsed);
    V(Legacy,TabRow); V(Legacy,Switcher); V(Sections,Legacy);
    auto* Management=MakeWidgetOfClass(BP,T,TEXT("Widget_StaffManagement"),ManagementClass); V(Sections,Management,FMargin(24,12,24,14),true); Sections->ShiftChild(Index,Management);
    Get<UTextBlock>(T,TEXT("Text_PanelTitle"))->SetText(FText::FromString(TEXT("스태프")));
    if(!Prep)
    {
        // 공통 위젯이 단계별 설명을 맡는다. 기존 라이브 부제는 숨김 보관한다.
        if(auto* Subtitle=T.FindWidget(TEXT("Staff_Subtitle")))Subtitle->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    auto* Footer=MakeWidget<UVerticalBox>(BP,T,TEXT("Management_HireFooter"));
    auto* QuoteRow=MakeWidget<UHorizontalBox>(BP,T,TEXT("Management_HireQuote"));
    auto* Heading=MakeWidget<UVerticalBox>(BP,T,TEXT("Management_HireHeading"));
    V(Heading,Label(BP,T,TEXT("Management_HireTitle"),TEXT("추가 고용"),21),FMargin(0,0,0,4));
    auto* Unit=Get<UTextBlock>(T,TEXT("Text_HireUnitCost")); Font(Unit,14,Muted); Unit->SetJustification(ETextJustify::Left); V(Heading,Unit);
    V(Heading,Label(BP,T,TEXT("Management_GoldUnit"),TEXT("금액 단위: 금화"),12,false,Muted),FMargin(0,3,0,0)); H(QuoteRow,Heading,FMargin(0,0,20,0));
    auto* Stepper=Get<UHorizontalBox>(T,TEXT("HorizontalBox_Stepper"));
    Get<USizeBox>(T,TEXT("SizeBox_Decrease"))->SetWidthOverride(38); Get<USizeBox>(T,TEXT("SizeBox_Decrease"))->SetHeightOverride(38);
    Get<USizeBox>(T,TEXT("SizeBox_Increase"))->SetWidthOverride(38); Get<USizeBox>(T,TEXT("SizeBox_Increase"))->SetHeightOverride(38);
    // 38px 버튼에서 글자의 기준선이 테두리에 걸리지 않도록 내용 영역을 중앙에 둔다.
    for(const TCHAR* Name:{TEXT("Button_Decrease"),TEXT("Button_Increase")})
    {
        auto* StepButton=Get<UButton>(T,Name);
        auto* Glyph=CastChecked<UTextBlock>(StepButton->GetContent());
        Font(Glyph,20,Ink); Glyph->SetJustification(ETextJustify::Center);
        FButtonStyle StepStyle=StepButton->GetStyle(); StepStyle.NormalPadding=StepStyle.PressedPadding=FMargin(0.f);
        StepButton->SetStyle(StepStyle);
        auto* GlyphSlot=CastChecked<UButtonSlot>(Glyph->Slot); GlyphSlot->SetPadding(FMargin(0.f));
        GlyphSlot->SetHorizontalAlignment(HAlign_Center); GlyphSlot->SetVerticalAlignment(VAlign_Center);
    }
    Font(Get<UTextBlock>(T,TEXT("Text_SelectedCount")),21,Ink,true);
    H(QuoteRow,Size(BP,T,TEXT("Management_Size_Stepper"),Stepper,190),FMargin(0,0,14,0));
    Quote(BP,T,QuoteRow,TEXT("Cost"),TEXT("고용 비용"),Get<UTextBlock>(T,TEXT("Text_HireCostTotal")));
    Quote(BP,T,QuoteRow,TEXT("Gold"),TEXT("보유 금화"),Label(BP,T,TEXT("Text_CurrentGold"),TEXT("—"),22,true));
    Quote(BP,T,QuoteRow,TEXT("After"),TEXT("고용 후 잔액"),Get<UTextBlock>(T,TEXT("Text_GoldAfterValue")));
    // 새 푸터가 아직 루트에 붙지 않았으므로 이동 전에 자식 참조를 보관한다.
    auto* HireLabel=Get<UTextBlock>(T,TEXT("Text_HireAction"));
    auto* Hire=Get<USizeBox>(T,TEXT("SizeBox_HireAction")); Hire->SetMinDesiredWidth(166); Hire->SetMinDesiredHeight(44); H(QuoteRow,Hire,FMargin(18,0,0,0));
    Font(HireLabel,17,FLinearColor::White,true); V(Footer,QuoteRow);
    auto* Notice=Get<UTextBlock>(T,TEXT("Text_DailyNotice")); Font(Notice,12,Muted); Notice->SetAutoWrapText(true); V(Footer,Notice,FMargin(0,7,0,0));
    auto* Block=Get<UTextBlock>(T,TEXT("Text_HireBlockReason")); Font(Block,13,Color(162,96,15)); Block->SetAutoWrapText(true); V(Footer,Block,FMargin(0,5,0,0));
    auto* FooterBorder=Card(BP,T,TEXT("Management_HireCard"),Footer,FMargin(18,12),SkyWash); V(Sections,FooterBorder,FMargin(24,0,24,18));
}
void DefaultText(UWidgetBlueprint& BP,const TCHAR* Name,const TCHAR* Value)
{
    UObject* CDO=BP.GeneratedClass->GetDefaultObject();
    if(auto* Property=FindFProperty<FTextProperty>(CDO->GetClass(),Name))Property->SetPropertyValue_InContainer(CDO,FText::FromString(Value));
}
bool ResizeHud(UWidgetBlueprint& BP,bool Prep)
{
    if(Applied(BP))return true;
    UObject* CDO=BP.GeneratedClass?BP.GeneratedClass->GetDefaultObject():nullptr;
    auto* Property=CDO?FindFProperty<FMapProperty>(CDO->GetClass(),TEXT("PanelLayouts")):nullptr; if(!Property)return false;
    CDO->Modify();
    if(Prep)
    {
        auto* Layouts=Property->ContainerPtrToValuePtr<TMap<EFF_PrepHudTab,FFF_PrepPanelLayout>>(CDO);
        auto* Layout=Layouts->Find(EFF_PrepHudTab::Staff); if(!Layout)return false;
        Layout->Size=FVector2D(1300,800);
    }
    else
    {
        auto* Layouts=Property->ContainerPtrToValuePtr<TMap<EFF_LiveTab,FFF_LivePanelLayout>>(CDO);
        auto* Layout=Layouts->Find(EFF_LiveTab::Staff);
        if(!Layout)
        {
            // 이전 HUD는 기본 크기만 사용해 Staff 항목이 없을 수 있다. 다른 탭은 그대로 둔다.
            FFF_LivePanelLayout NewLayout; NewLayout.Dock=EFF_LivePanelDock::Center;
            NewLayout.Width=1300; NewLayout.Height=800; Layouts->Add(EFF_LiveTab::Staff,NewLayout);
        }
        else { Layout->Width=1300; Layout->Height=800; }
    }
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1")); return SaveAssetPackage(Prefix,BP);
}
bool ValidParent(UWidgetBlueprint* BP,UClass* Parent)
{
    return BP&&BP->WidgetTree&&BP->ParentClass&&BP->ParentClass->IsChildOf(Parent)&&!BP->GetOutermost()->IsDirty();
}
void Apply()
{
    if(GEditor&&GEditor->PlayWorld){UE_LOG(LogFFUIWbpBuilder,Error,TEXT("%s_PIE_RUNNING"),Prefix);return;}
    auto* Prep=LoadExistingBlueprint(PrepPackage); auto* Live=LoadExistingBlueprint(LivePackage);
    auto* Training=LoadExistingBlueprint(TrainingPackage); auto* Promotion=LoadExistingBlueprint(PromotionPackage);
    auto* PrepHud=LoadExistingBlueprint(PrepHudPackage); auto* LiveHud=LoadExistingBlueprint(LiveHudPackage);
    if(!ValidParent(Prep,UFF_PrepPanelStaff::StaticClass())||!ValidParent(Live,UFF_LivePanelStaff::StaticClass())||
        !ValidParent(Training,UFF_StaffTrainingWidget::StaticClass())||!ValidParent(Promotion,UFF_StaffPromotionRowWidget::StaticClass())||
        !ValidParent(PrepHud,UFF_PrepHudBar::StaticClass())||!ValidParent(LiveHud,UFF_LiveHudScreen::StaticClass()))
    {UE_LOG(LogFFUIWbpBuilder,Error,TEXT("%s_CONTRACT_FAIL: 부모/미저장 상태를 확인하세요."),Prefix);return;}
    if(!Require(*Prep,{TEXT("VerticalBox_Sections"),TEXT("Staff_TabRow"),TEXT("Switcher_StaffContent"),TEXT("Widget_StaffTraining"),TEXT("Text_PanelTitle"),TEXT("Text_HireUnitCost"),TEXT("HorizontalBox_Stepper"),TEXT("SizeBox_Decrease"),TEXT("SizeBox_Increase"),TEXT("Text_SelectedCount"),TEXT("Text_HireCostTotal"),TEXT("Text_GoldAfterValue"),TEXT("SizeBox_HireAction"),TEXT("Text_HireAction"),TEXT("Text_DailyNotice"),TEXT("Text_HireBlockReason")})||
        !Require(*Live,{TEXT("Sky_VerticalBox_Sections"),TEXT("Staff_TabRow"),TEXT("Switcher_StaffContent"),TEXT("Widget_StaffTraining"),TEXT("Text_PanelTitle")})||
        !Require(*Training,{TEXT("Text_TrainingTitle"),TEXT("Border_RosterHeader"),TEXT("SizeBox_StaffRoster"),TEXT("Text_RosterEmpty"),TEXT("Text_RosterNote"),TEXT("Border_PromotionResult"),TEXT("HorizontalBox_RosterHeader"),TEXT("Text_NameHeader"),TEXT("Text_KindHeader"),TEXT("Text_LevelHeader"),TEXT("Text_PromotionHeader"),TEXT("ScrollBox_StaffRoster"),TEXT("Text_PromotionResult")})||
        !Require(*Promotion,{TEXT("HorizontalBox_StaffRow"),TEXT("Text_StaffName"),TEXT("Text_StaffKind"),TEXT("Text_StaffLevel"),TEXT("VerticalBox_PromotionAction"),TEXT("Text_Promote"),TEXT("Text_PromotionStatus"),TEXT("SizeBox_Promote"),TEXT("SizeBox_StaffRow")}))return;
    for(const auto& Pair:TArray<TPair<FString,UClass*>>{{ManagementPackage,UFF_StaffManagementWidget::StaticClass()},{FacilityRowPackage,UFF_StaffFacilityRowWidget::StaticClass()}})
    {
        if(auto* Existing=LoadExistingBlueprint(Pair.Key))if(!ValidParent(Existing,Pair.Value)||!Applied(*Existing))
        {UE_LOG(LogFFUIWbpBuilder,Error,TEXT("%s_NEW_ASSET_CONFLICT %s"),Prefix,*Pair.Key);return;}
    }
    const FString BackupDir=FPaths::ProjectSavedDir()/TEXT("UIBackups/StaffManagement")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if(!IFileManager::Get().MakeDirectory(*BackupDir,true))return;
    for(const FString& Path:{PrepPackage,LivePackage,TrainingPackage,PromotionPackage,PrepHudPackage,LiveHudPackage,ManagementPackage,FacilityRowPackage})if(!Backup(Path,BackupDir))
    {UE_LOG(LogFFUIWbpBuilder,Error,TEXT("%s_BACKUP_FAIL %s"),Prefix,*Path);return;}
    if(!Applied(*Promotion))
    {
        Promotion->Modify(); Promotion->WidgetTree->Modify(); MigratePromotion(*Promotion); if(!Save(*Promotion))return;
        auto* CDO=Promotion->GeneratedClass->GetDefaultObject();
        if(auto* Prop=FindFProperty<FBoolProperty>(CDO->GetClass(),TEXT("bCompactManagementRow")))Prop->SetPropertyValue_InContainer(CDO,true);
        if(!SaveAssetPackage(Prefix,*Promotion))return;
    }
    if(!Applied(*Training))
    {
        Training->Modify(); Training->WidgetTree->Modify(); MigrateTraining(*Training); if(!Save(*Training))return;
        DefaultText(*Training,TEXT("RosterNoteText"),TEXT("오늘 고용한 직원은 다음 날부터 승급할 수 있습니다."));
        if(!SaveAssetPackage(Prefix,*Training))return;
    }
    auto* Facility=EnsureWidgetBlueprint(Prefix,FacilityRowPackage,Folder,*UFF_StaffFacilityRowWidget::StaticClass(),BuildFacility);
    if(!Facility||!Facility->GeneratedClass||!Applied(*Facility))return;
    const bool NewManagement=!FPackageName::DoesPackageExist(ManagementPackage);
    auto* Management=EnsureWidgetBlueprint(Prefix,ManagementPackage,Folder,*UFF_StaffManagementWidget::StaticClass(),BuildManagement);
    if(!Management||!Management->GeneratedClass||!Applied(*Management))return;
    if(NewManagement)
    {
        auto* CDO=Management->GeneratedClass->GetDefaultObject<UFF_StaffManagementWidget>();
        CDO->ConfigureFacilityRowDefaults(Facility->GeneratedClass.Get()); if(!SaveAssetPackage(Prefix,*Management))return;
    }
    for(auto* BP:{Prep,Live})if(!Applied(*BP))
    {
        BP->Modify(); BP->WidgetTree->Modify(); MigrateHost(*BP,BP==Prep,Management->GeneratedClass.Get()); if(!Save(*BP))return;
        if(BP==Prep)
        {
            DefaultText(*BP,TEXT("GoldValueTextFormat"),TEXT("{Gold}"));
            DefaultText(*BP,TEXT("HireUnitCostTextFormat"),TEXT("{Count}명 × {UnitCost}"));
            if(!SaveAssetPackage(Prefix,*BP))return;
        }
    }
    if(!ResizeHud(*PrepHud,true)||!ResizeHud(*LiveHud,false))return;
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("%s_APPLIED: 공통 배정·숙련도와 단계별 하단 영역. 백업=%s"),Prefix,*BackupDir);
}
FAutoConsoleCommand Command(TEXT("FF.ApplyStaffManagementDesign"),TEXT("스태프 통합 관리 시안을 기존 고용/승급 연결을 보존하며 한 번 적용한다."),FConsoleCommandDelegate::CreateStatic(&Apply));
}
