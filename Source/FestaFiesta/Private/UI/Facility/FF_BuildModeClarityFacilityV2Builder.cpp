// Explicit V2 authoring pass. Existing controls and transaction bindings are retained.
#if WITH_EDITOR
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace FFBuildClarityFacilityV2
{
using namespace FFUIWbpBuilder;
static const FLinearColor Ink=FLinearColor::FromSRGBColor(FColor(43,72,59));
static const FLinearColor Muted=FLinearColor::FromSRGBColor(FColor(95,115,102));
static const FLinearColor Paper=FLinearColor::FromSRGBColor(FColor(242,247,238));
static const FLinearColor Violet=FLinearColor::FromSRGBColor(FColor(98,83,119));
static FSlateBrush Material(const TCHAR* Role)
{
    FSlateBrush B;B.DrawAs=ESlateBrushDrawType::RoundedBox;B.TintColor=Paper;
    B.OutlineSettings=FSlateBrushOutlineSettings(8,FSlateColor(FLinearColor::FromSRGBColor(FColor(202,217,200))),.7f);
    const FString Name=FString(TEXT("MI_UI_BuildClarity_"))+Role;
    const FString Path=FString(TEXT("/Game/FestaFiesta/UI/Building/Clarity/"))+Name+TEXT(".")+Name;
    if(auto* M=LoadObject<UMaterialInterface>(nullptr,*Path))
    { B.DrawAs=ESlateBrushDrawType::Image;B.SetResourceObject(M);B.TintColor=FLinearColor::White; }
    return B;
}
template<class T> static T* Find(UWidgetTree& Tree,const TCHAR* Name){return Cast<T>(Tree.FindWidget(Name));}
static void Variable(UWidgetBlueprint& BP,UWidget* W)
{
    if(!W)return;
    W->bIsVariable=true;
    // Existing Designer nodes already have GUIDs even when they were not exposed
    // as variables. Promotion must retain those references and register only new names.
    if(!BP.WidgetVariableNameToGuidMap.Contains(W->GetFName()))BP.OnVariableAdded(W->GetFName());
}
static void V(UVerticalBox& Parent,UWidget& W,const FMargin Padding=FMargin(0))
{ W.RemoveFromParent();auto* S=Parent.AddChildToVerticalBox(&W);S->SetPadding(Padding);S->SetHorizontalAlignment(HAlign_Fill); }
static void H(UHorizontalBox& Parent,UWidget& W,bool Fill=true,float Gap=0)
{ W.RemoveFromParent();auto* S=Parent.AddChildToHorizontalBox(&W);S->SetSize(FSlateChildSize(Fill?ESlateSizeRule::Fill:ESlateSizeRule::Automatic));S->SetPadding(FMargin(0,0,Gap,0));S->SetVerticalAlignment(VAlign_Center); }
static void Font(UTextBlock* T,int32 Size,bool Strong=false,FLinearColor Color=Ink)
{ if(!T)return;StyleText(*T,Size,Color,Strong?TEXT("SemiBold"):TEXT("Regular"));T->SetAutoWrapText(true);T->SetLineHeightPercentage(1.f);T->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping); }
static UCommonTextBlock* Text(UWidgetBlueprint& BP,UWidgetTree& Tree,FName Name,const TCHAR* Copy,int32 Size=14,bool Bound=false)
{ auto* W=MakeWidget<UCommonTextBlock>(BP,Tree,Name,Bound);W->SetText(FText::FromString(Copy));Font(W,Size);return W; }
static void StyleButton(UButton* W,bool Primary=false,const TCHAR* Role=TEXT("Header"))
{
    if(!W)return;FButtonStyle S=W->GetStyle();
    S.Normal=Material(Primary?TEXT("Action"):Role);S.Hovered=Material(Primary?TEXT("Dark"):TEXT("Paper"));
    S.Pressed=Material(Primary?TEXT("Dark"):Role);S.Disabled=Material(W->GetName().Contains(TEXT("Tab"))?TEXT("Action"):(Primary?TEXT("Header"):TEXT("Paper")));
    S.NormalPadding=S.PressedPadding=FMargin(6,6);W->SetStyle(S);
    if(auto* L=Cast<UTextBlock>(W->GetContent()))
    { Font(L,14,true,Primary?FLinearColor::White:Ink);L->SetAutoWrapText(false);L->SetJustification(ETextJustify::Center);
      if(auto* Slot=Cast<UButtonSlot>(L->Slot))Slot->SetPadding(FMargin(2)); }
}
static UButton* Button(UWidgetBlueprint& BP,UWidgetTree& Tree,const TCHAR* Name,const TCHAR* Label,const TCHAR* Copy)
{
    auto* B=MakeWidget<UButton>(BP,Tree,Name,true);B->AddChild(Text(BP,Tree,Label,Copy,14,true));StyleButton(B);
    if(auto* P=FindFProperty<FBoolProperty>(UButton::StaticClass(),TEXT("IsFocusable")))P->SetPropertyValue_InContainer(B,false);
    return B;
}
static bool Inspector(UWidgetBlueprint& BP)
{
    UWidgetTree& T=*BP.WidgetTree;
    if(T.FindWidget(TEXT("ClarityV2_Info")))return true;
    auto* Shell=Find<UVerticalBox>(T,TEXT("Panel_Shell"));
    if(!Shell||!T.FindWidget(TEXT("Clarity_InfoV1")))return false;
    BP.Modify();T.Modify();
    auto* Marker=MakeWidget<UVerticalBox>(BP,T,TEXT("ClarityV2_Info"));V(*Shell,*Marker);Marker->SetVisibility(ESlateVisibility::Collapsed);
    for(const TCHAR* Name:{TEXT("Border_Panel"),TEXT("Panel_PersistentIdentity")})
        if(auto* B=Find<UBorder>(T,Name)){B->SetBrush(Material(TEXT("Paper")));B->SetBrushColor(FLinearColor::White);}
    if(auto* B=Find<UBorder>(T,TEXT("Header_Surface"))){B->SetBrush(Material(TEXT("Header")));B->SetBrushColor(FLinearColor::White);}
    if(auto* B=Find<UBorder>(T,TEXT("Program_Card"))){B->SetBrush(Material(TEXT("Program")));B->SetBrushColor(FLinearColor::White);}
    if(auto* B=Find<UButton>(T,TEXT("Button_TabPrograms")))StyleButton(B,false,TEXT("Program"));
    if(auto* L=Find<UTextBlock>(T,TEXT("Text_TabLighting")))L->SetText(FText::FromString(TEXT("조명")));
    for(const TCHAR* Name:{TEXT("Button_LightingMinimize"),TEXT("Button_Close")})StyleButton(Find<UButton>(T,Name));
    Font(Find<UTextBlock>(T,TEXT("Text_LightingCompactSummary")),14);
    for(const TCHAR* Name:{TEXT("Text_ProgramStatus"),TEXT("Text_ScheduledProgramSummary"),TEXT("Text_ProgramScheduleReason")})Font(Find<UTextBlock>(T,Name),14);
    return SaveNewBlueprint(TEXT("FF_CLARITY_V2_INFO"),BP);
}
static bool Lighting(UWidgetBlueprint& BP)
{
    UWidgetTree& T=*BP.WidgetTree;if(T.FindWidget(TEXT("ClarityV2_Lighting")))return true;
    auto* Root=Cast<UVerticalBox>(T.RootWidget);auto* Wheel=Find<USizeBox>(T,TEXT("Aurora_ColorPickerSize"));
    if(!Root||!Wheel||!T.FindWidget(TEXT("Input_LightingH"))||!T.FindWidget(TEXT("Button_LightingApply")))return false;
    BP.Modify();T.Modify();
    auto* Archive=MakeWidget<UVerticalBox>(BP,T,TEXT("ClarityV2_Lighting"));
    for(UWidget* Old:Root->GetAllChildren())V(*Archive,*Old);
    V(*Root,*Archive);Archive->SetVisibility(ESlateVisibility::Collapsed);
    auto* Header=MakeWidget<UHorizontalBox>(BP,T,TEXT("ClarityV2_LightingHeader"));
    auto* Title=Find<UCommonTextBlock>(T,TEXT("Text_LightingTitle"));Title->SetText(FText::FromString(TEXT("조명 · 실시간 미리보기")));Font(Title,14,true);
    H(*Header,*Title);H(*Header,*Button(BP,T,TEXT("Button_LightingPrecision"),TEXT("Text_LightingPrecision"),TEXT("정밀 값")),false,6);
    auto* Toggle=Find<UButton>(T,TEXT("Button_LightingToggle"));StyleButton(Toggle);H(*Header,*Toggle,false);V(*Root,*Header,FMargin(0,0,0,10));
    auto* Modes=MakeWidget<UOverlay>(BP,T,TEXT("ClarityV2_ColorModes"));
    auto* ModeSize=MakeWidget<USizeBox>(BP,T,TEXT("ClarityV2_ColorModeSize"));ModeSize->SetHeightOverride(200);ModeSize->AddChild(Modes);
    auto* Basic=MakeWidget<UHorizontalBox>(BP,T,TEXT("Panel_LightingBasic"),true);
    Wheel->SetWidthOverride(190);Wheel->SetHeightOverride(190);H(*Basic,*Wheel,false,14);
    auto* Presets=MakeWidget<UVerticalBox>(BP,T,TEXT("ClarityV2_Presets"));
    V(*Presets,*Text(BP,T,TEXT("ClarityV2_PresetCaption"),TEXT("색상 프리셋"),13),FMargin(0,0,0,6));
    for(const TCHAR* Name:{TEXT("Button_LightingOriginal"),TEXT("Button_LightingTeal"),TEXT("Button_LightingSunset"),TEXT("Button_LightingViolet")})
    {
        auto* B=Find<UButton>(T,Name);StyleButton(B);V(*Presets,*B,FMargin(0,0,0,5));
        auto* Label=B->GetContent();Label->RemoveFromParent();
        auto* Row=MakeWidget<UHorizontalBox>(BP,T,FName(*(B->GetName()+TEXT("_SwatchRow"))));
        auto* Swatch=MakeWidget<UBorder>(BP,T,FName(*(B->GetName()+TEXT("_Swatch"))));
        FSlateBrush SwatchBrush;SwatchBrush.DrawAs=ESlateBrushDrawType::RoundedBox;
        SwatchBrush.OutlineSettings=FSlateBrushOutlineSettings(4,FSlateColor(Ink),.5f);
        SwatchBrush.TintColor=FLinearColor::FromSRGBColor(FColor(222,228,218));
        if(B->GetName().Contains(TEXT("Teal")))SwatchBrush.TintColor=FLinearColor::FromSRGBColor(FColor(0x56,0xDC,0xE7));
        if(B->GetName().Contains(TEXT("Sunset")))SwatchBrush.TintColor=FLinearColor::FromSRGBColor(FColor(0xFF,0xB5,0x6B));
        if(B->GetName().Contains(TEXT("Violet")))SwatchBrush.TintColor=FLinearColor::FromSRGBColor(FColor(0xBC,0xA2,0xFF));
        Swatch->SetBrush(SwatchBrush);Swatch->SetBrushColor(FLinearColor::White);
        auto* SwatchSize=MakeWidget<USizeBox>(BP,T,FName(*(B->GetName()+TEXT("_SwatchSize"))));
        SwatchSize->SetWidthOverride(14);SwatchSize->SetHeightOverride(14);SwatchSize->AddChild(Swatch);
        H(*Row,*SwatchSize,false,8);H(*Row,*Label);B->AddChild(Row);
    }
    H(*Basic,*Presets);auto* BasicSlot=Modes->AddChildToOverlay(Basic);BasicSlot->SetHorizontalAlignment(HAlign_Fill);BasicSlot->SetVerticalAlignment(VAlign_Center);
    auto* Precise=MakeWidget<UHorizontalBox>(BP,T,TEXT("Panel_LightingPrecision"),true);Precise->SetVisibility(ESlateVisibility::Collapsed);
    for(int32 Group=0;Group<2;++Group)
    {
        auto* Column=MakeWidget<UVerticalBox>(BP,T,FName(*FString::Printf(TEXT("ClarityV2_Channels%d"),Group)));
        V(*Column,*Text(BP,T,FName(*FString::Printf(TEXT("ClarityV2_ChannelsTitle%d"),Group)),Group==0?TEXT("HSV"):TEXT("RGB"),14),FMargin(0,0,0,8));
        const TCHAR* Inputs[]={TEXT("Input_LightingH"),TEXT("Input_LightingS"),TEXT("Input_LightingV"),TEXT("Input_LightingR"),TEXT("Input_LightingG"),TEXT("Input_LightingB")};
        const TCHAR* Captions[]={TEXT("색조 0–360"),TEXT("채도 0–100"),TEXT("명도 0–100"),TEXT("빨강 0–255"),TEXT("초록 0–255"),TEXT("파랑 0–255")};
        for(int32 RowIndex=0;RowIndex<3;++RowIndex)
        {
            const int32 Index=Group*3+RowIndex;
            auto* Row=MakeWidget<UHorizontalBox>(BP,T,FName(*FString::Printf(TEXT("ClarityV2_Channel%d"),Index)));
            H(*Row,*Text(BP,T,FName(*FString::Printf(TEXT("ClarityV2_ChannelLabel%d"),Index)),Captions[Index],13));
            H(*Row,*T.FindWidget(Inputs[Index]),false);V(*Column,*Row,FMargin(0,0,0,8));
        }
        H(*Precise,*Column,true,Group==0?14:0);
    }
    auto* PreciseSlot=Modes->AddChildToOverlay(Precise);PreciseSlot->SetHorizontalAlignment(HAlign_Fill);PreciseSlot->SetVerticalAlignment(VAlign_Center);
    V(*Root,*ModeSize,FMargin(0,0,0,10));
    auto* Hex=Find<UHorizontalBox>(T,TEXT("Aurora_HexRow"));V(*Root,*Hex,FMargin(0,0,0,5));
    Font(Find<UTextBlock>(T,TEXT("Aurora_HexLabel")),13);Font(Find<UTextBlock>(T,TEXT("Text_LightingColorStatus")),13,false,Muted);
    V(*Root,*T.FindWidget(TEXT("Text_LightingColorStatus")),FMargin(0,0,0,12));
    V(*Root,*T.FindWidget(TEXT("Aurora_BrightnessRow")),FMargin(0,0,0,4));
    Font(Find<UTextBlock>(T,TEXT("Text_LightingBrightnessLabel")),14);Font(Find<UTextBlock>(T,TEXT("Text_LightingBrightness")),16,true);
    if(auto* Brightness=Find<UHorizontalBox>(T,TEXT("Aurora_BrightnessRow")))
    {
        Find<UTextBlock>(T,TEXT("Text_LightingBrightnessLabel"))->SetText(FText::FromString(TEXT("밝기 0–500%")));
        if(auto* S=Cast<UHorizontalBoxSlot>(T.FindWidget(TEXT("Text_LightingBrightness"))->Slot))S->SetPadding(FMargin(0,0,8,0));
        const TCHAR* Names[]={TEXT("Button_LightingBrightnessLow"),TEXT("Button_LightingBrightnessNormal"),TEXT("Button_LightingBrightnessHigh")};
        const TCHAR* Copies[]={TEXT("50%"),TEXT("100%"),TEXT("150%")};
        for(int32 Index=0;Index<3;++Index)
        {
            const FString Label=FString(Names[Index])+TEXT("_Label");auto* B=Button(BP,T,Names[Index],*Label,Copies[Index]);
            auto S=B->GetStyle();S.NormalPadding=S.PressedPadding=FMargin(4,3);B->SetStyle(S);
            Font(Cast<UTextBlock>(B->GetContent()),12);H(*Brightness,*B,false,Index==2?0:4);
        }
    }
    V(*Root,*T.FindWidget(TEXT("Slider_LightingBrightness")),FMargin(0,0,0,14));
    auto* Footer=MakeWidget<UHorizontalBox>(BP,T,TEXT("ClarityV2_LightingActions"));
    const TCHAR* Names[]={TEXT("Button_LightingReset"),TEXT("Button_LightingCancel"),TEXT("Button_LightingObserve"),TEXT("Button_LightingApply")};
    const TCHAR* Labels[]={TEXT("기본값"),TEXT("취소"),TEXT("장면 확인"),TEXT("적용")};
    for(int32 Index=0;Index<4;++Index)
    {auto* B=Find<UButton>(T,Names[Index]);StyleButton(B,Index==3);if(auto* L=Cast<UTextBlock>(B->GetContent()))L->SetText(FText::FromString(Labels[Index]));H(*Footer,*B,true,Index==3?0:6);}
    V(*Root,*Footer);
    T.ForEachWidget([&](UWidget* W)
    {
        if(auto* Input=Cast<UEditableTextBox>(W))
        {
            auto S=Input->GetWidgetStyle();auto F=S.TextStyle.Font;F.Size=14;S.TextStyle.SetFont(F);S.Padding=FMargin(6,4);
            S.BackgroundImageNormal=Material(TEXT("Paper"));S.BackgroundImageFocused=Material(TEXT("Header"));S.BackgroundImageHovered=Material(TEXT("Paper"));
            S.ForegroundColor=Ink;S.FocusedForegroundColor=Ink;Input->SetWidgetStyle(S);Input->SetMinDesiredWidth(62);
        }
    });
    return SaveNewBlueprint(TEXT("FF_CLARITY_V2_LIGHTING"),BP);
}
static bool ProgramCard(UWidgetBlueprint& BP)
{
    const TCHAR* Version=TEXT("FF.BuildClarity.ProgramCardV2");if(BP.GetOutermost()->GetMetaData().GetValue(&BP,Version)==TEXT("1"))return true;
    UWidgetTree& T=*BP.WidgetTree;auto* Select=Find<UButton>(T,TEXT("Button_Select"));if(!Select)return false;
    BP.Modify();T.Modify();StyleButton(Select,false,TEXT("Program"));
    for(const TCHAR* Name:{TEXT("Text_ProgramName"),TEXT("Text_ProgramMeta"),TEXT("Text_ProgramAvailability")})
    {
        auto* W=Find<UTextBlock>(T,Name);if(!W)return false;
        const bool NameRow=FCString::Strcmp(Name,TEXT("Text_ProgramName"))==0;
        Font(W,NameRow?16:12,NameRow,NameRow?Ink:Muted);W->SetWrapTextAt(380);
        if(auto* S=Cast<UVerticalBoxSlot>(W->Slot))S->SetPadding(FMargin(8,NameRow?6:0,8,4));
    }
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1"));return SaveNewBlueprint(TEXT("FF_CLARITY_V2_PROGRAM_CARD"),BP);
}
static bool ProgramPickerPriceFit(UWidgetBlueprint& BP)
{
    const TCHAR* Version=TEXT("FF.BuildClarity.ProgramPickerPriceV3");
    if(BP.GetOutermost()->GetMetaData().GetValue(&BP,Version)==TEXT("1"))return true;
    auto* Price=Find<UTextBlock>(*BP.WidgetTree,TEXT("Text_PurchaseCost"));if(!Price)return false;
    BP.Modify();Price->Modify();
    // Auto-sized quote values must keep the amount and currency together.
    Price->SetAutoWrapText(false);Price->SetWrapTextAt(0);Price->SetJustification(ETextJustify::Right);
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1"));
    return SaveNewBlueprint(TEXT("FF_CLARITY_V3_PROGRAM_PRICE"),BP);
}
static bool ProgramPicker(UWidgetBlueprint& BP)
{
    UWidgetTree& T=*BP.WidgetTree;if(T.FindWidget(TEXT("Button_BackToPrograms")))return ProgramPickerPriceFit(BP);
    auto* Picker=Find<UBorder>(T,TEXT("Border_Picker"));auto* Header=Find<UHorizontalBox>(T,TEXT("Panel_Header"));
    auto* ListSize=Find<USizeBox>(T,TEXT("Size_ProgramList"));if(!Picker||!Header||!ListSize||!T.FindWidget(TEXT("Panel_Preview")))return false;
    BP.Modify();T.Modify();
    if(auto* S=Cast<UCanvasPanelSlot>(Picker->Slot))
    {S->SetAnchors(FAnchors(1,0));S->SetAlignment(FVector2D(1,0));S->SetOffsets(FMargin(-18,18,456,620));S->SetAutoSize(false);}
    Picker->SetBrush(Material(TEXT("Paper")));Picker->SetBrushColor(FLinearColor::White);
    for(const TCHAR* Name:{TEXT("Size_ProgramList"),TEXT("Border_Preview"),TEXT("Text_Instruction")})Variable(BP,T.FindWidget(Name));
    auto* Back=Button(BP,T,TEXT("Button_BackToPrograms"),TEXT("Text_BackToPrograms"),TEXT("‹ 목록"));H(*Header,*Back,false,8);Header->ShiftChild(0,Back);Back->SetVisibility(ESlateVisibility::Collapsed);
    for(const TCHAR* Name:{TEXT("Border_Header"),TEXT("Border_Body"),TEXT("Border_Preview"),TEXT("Border_Footer"),TEXT("Border_ProgramList")})
        if(auto* B=Find<UBorder>(T,Name))
        {B->SetBrush(Material(FCString::Strcmp(Name,TEXT("Border_Header"))==0||FCString::Strcmp(Name,TEXT("Border_Preview"))==0?TEXT("Program"):TEXT("Paper")));B->SetBrushColor(FLinearColor::White);B->SetPadding(FMargin(10,8));}
    Font(Find<UTextBlock>(T,TEXT("Text_Title")),19,true);Find<UTextBlock>(T,TEXT("Text_Title"))->SetText(FText::FromString(TEXT("프로그램 편성")));
    Font(Find<UTextBlock>(T,TEXT("Text_Eyebrow")),11,false,Muted);
    Font(Find<UTextBlock>(T,TEXT("Text_FacilityName")),14,true);
    Font(Find<UTextBlock>(T,TEXT("Text_PreviewName")),18,true);
    Font(Find<UTextBlock>(T,TEXT("Text_PurchaseCost")),19,true,Violet);
    for(const TCHAR* Name:{TEXT("Text_PreviewTime"),TEXT("Text_PreviewDescription"),TEXT("Text_PreviewAvailability"),TEXT("Text_ResourcePreview"),TEXT("Text_Feedback"),TEXT("Text_Instruction")})Font(Find<UTextBlock>(T,Name),13,false,Muted);
    for(const TCHAR* Name:{TEXT("Text_RequiredStaff"),TEXT("Text_Capacity"),TEXT("Text_TicketFee")})Font(Find<UTextBlock>(T,Name),15,true);
    for(const TCHAR* Name:{TEXT("Text_MaxRevenue"),TEXT("Text_NoRefundNotice"),TEXT("Text_PurchaseCostLabel")})Font(Find<UTextBlock>(T,Name),12,false,Muted);
    for(const TCHAR* Name:{TEXT("Button_Close"),TEXT("Button_Confirm"),TEXT("Button_ReturnToFacility")})StyleButton(Find<UButton>(T,Name),FCString::Strcmp(Name,TEXT("Button_Close"))!=0);
    for(const TCHAR* Name:{TEXT("Text_PurchaseCompleteTitle"),TEXT("Text_ReceiptProgramName")})Font(Find<UTextBlock>(T,Name),18,true);
    for(const TCHAR* Name:{TEXT("Text_PurchaseReceipt"),TEXT("Text_ReceiptResources")})Font(Find<UTextBlock>(T,Name),14);
    ListSize->ClearMaxDesiredHeight();ListSize->SetHeightOverride(324);
    if(auto* Preview=Find<UVerticalBox>(T,TEXT("Panel_Preview")))
        for(UWidget* W:Preview->GetAllChildren())if(auto* S=Cast<UVerticalBoxSlot>(W->Slot))S->SetPadding(FMargin(0,0,0,5));
    for(const TCHAR* Name:{TEXT("Text_RequiredStaff_Card"),TEXT("Text_Capacity_Card"),TEXT("Text_TicketFee_Card")})
        if(auto* B=Find<UBorder>(T,Name)){B->SetPadding(FMargin(6,4));B->SetBrush(Material(TEXT("Paper")));B->SetBrushColor(FLinearColor::White);}
    for(const TCHAR* Name:{TEXT("Text_RequiredStaff_Label"),TEXT("Text_Capacity_Label"),TEXT("Text_TicketFee_Label")})Font(Find<UTextBlock>(T,Name),12,false,Muted);
    for(const TCHAR* Name:{TEXT("ScrollBox_PurchaseDetails"),TEXT("ScrollBox_Programs"),TEXT("ScrollBox_PurchaseReceipt")})
        if(auto* Scroll=Find<UScrollBox>(T,Name)){Scroll->SetAnimateWheelScrolling(false);Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);}
    return ProgramPickerPriceFit(BP);
}
static void Run()
{
    struct FTarget{const TCHAR* Path;bool(*Apply)(UWidgetBlueprint&);};
    const FTarget Targets[]={
        {TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityLightingControls"),Lighting},
        {TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel"),Inspector},
        {TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard"),ProgramCard},
        {TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramPicker"),ProgramPicker}};
    bool Success=true;
    for(const auto& Target:Targets)
    {auto* BP=LoadExistingBlueprint(Target.Path);const bool Result=BP&&BP->WidgetTree&&Target.Apply(*BP);Success&=Result;UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_CLARITY_FACILITY_V2_%s %s"),Result?TEXT("PASS"):TEXT("FAIL"),Target.Path);}
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_CLARITY_FACILITY_V2_COMPLETE_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClarityFacilityV2"),
    TEXT("Refine saved facility, compact lighting and program purchase widgets to V2 without replacing transaction bindings."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFClarityVariableGuidPreservationTest,
    "FestaFiesta.UI.FacilityInfo.ClarityVariableGuidPreservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFFClarityVariableGuidPreservationTest::RunTest(const FString&)
{
    auto* BP=NewObject<UWidgetBlueprint>();
    BP->WidgetTree=NewObject<UWidgetTree>(BP);
    auto* Existing=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ExistingDesignerNode"));
    BP->OnVariableAdded(Existing->GetFName());
    const FGuid Original=BP->WidgetVariableNameToGuidMap.FindChecked(Existing->GetFName());
    FFBuildClarityFacilityV2::Variable(*BP,Existing);
    FFBuildClarityFacilityV2::Variable(*BP,Existing);
    TestTrue(TEXT("Promoted Designer node is bindable"),Existing->bIsVariable);
    TestEqual(TEXT("Repeated promotion preserves the Designer GUID"),BP->WidgetVariableNameToGuidMap.FindChecked(Existing->GetFName()),Original);
    auto* NewNode=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("UnregisteredNode"));
    FFBuildClarityFacilityV2::Variable(*BP,NewNode);
    TestTrue(TEXT("A previously unregistered node receives a valid GUID"),BP->WidgetVariableNameToGuidMap.FindChecked(NewNode->GetFName()).IsValid());
    return true;
}
#endif
#endif
