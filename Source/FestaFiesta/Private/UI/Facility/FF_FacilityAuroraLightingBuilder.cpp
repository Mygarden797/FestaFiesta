// FF_FacilityAuroraLightingBuilder.cpp
// 역할: 시설 조명 편집 화면에 Aurora 디자인을 적용한다.

// Explicit editor migration for the approved green aurora lighting inspector.
#if WITH_EDITOR
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "UI/Facility/FF_FacilityColorPicker.h"
#include "UI/Facility/FF_FacilityLightingControls.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Data/FF_UIPalette.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFFacilityAuroraLighting
{
    using namespace FFUIWbpBuilder;
    const FString LightingPath=FFBuildUIAssetPaths::FacilityLightingControlsPackage;
    const FString PanelPath=FFBuildUIAssetPaths::FacilityInfoPanelPackage;
    const TCHAR* LogPrefix=TEXT("FF_AURORA_LIGHTING");
    FLinearColor Green() { return FLinearColor::FromSRGBColor(FColor(0x24,0x55,0x43)); }
    FLinearColor Paper() { return FLinearColor::FromSRGBColor(FColor(0xED,0xF5,0xEE)); }
    UCommonTextBlock* Text(UWidgetBlueprint& B,UWidgetTree& T,const TCHAR* Name,const TCHAR* Value,int32 Size=19,bool Variable=false)
    {
        auto* W=MakeWidget<UCommonTextBlock>(B,T,FName(Name),Variable);
        W->SetText(FText::FromString(Value)); StyleText(*W,Size,Green(),TEXT("Medium"));
        W->SetAutoWrapText(true); return W;
    }
    void V(UVerticalBox& Parent,UWidget& Child,float Bottom=9)
    { Child.RemoveFromParent(); auto* S=Parent.AddChildToVerticalBox(&Child); S->SetPadding(FMargin(0,0,0,Bottom)); S->SetHorizontalAlignment(HAlign_Fill); }
    void H(UHorizontalBox& Parent,UWidget& Child,bool Fill=true,float Right=8)
    { Child.RemoveFromParent(); auto* S=Parent.AddChildToHorizontalBox(&Child); S->SetPadding(FMargin(0,0,Right,0)); S->SetSize(FSlateChildSize(Fill?ESlateSizeRule::Fill:ESlateSizeRule::Automatic)); S->SetVerticalAlignment(VAlign_Center); }
    UButton* Button(UWidgetBlueprint& B,UWidgetTree& T,const TCHAR* Name,const TCHAR* LabelName,const TCHAR* Label,bool Primary=false)
    {
        auto* W=MakeWidget<UButton>(B,T,FName(Name),true);
        FButtonStyle Style=W->GetStyle();
        auto Brush=[&](FLinearColor Color){ FSlateBrush X; X.DrawAs=ESlateBrushDrawType::RoundedBox; X.TintColor=Color; X.OutlineSettings=FSlateBrushOutlineSettings(7,FSlateColor(Green()),1); return X; };
        Style.SetNormal(Brush(Primary?Green():Paper())); Style.SetHovered(Brush(FLinearColor(.32f,.57f,.40f)));
        Style.SetPressed(Brush(Green())); Style.SetDisabled(Brush(Green()));
        W->SetStyle(Style);
        if(FBoolProperty* Focus=FindFProperty<FBoolProperty>(UButton::StaticClass(),TEXT("IsFocusable"))) Focus->SetPropertyValue_InContainer(W,false);
        auto* L=Text(B,T,LabelName,Label,18,true); if(Primary)L->SetColorAndOpacity(FLinearColor::White);
        W->AddChild(L); if(auto* S=Cast<UButtonSlot>(L->Slot)){S->SetPadding(FMargin(12,9));S->SetHorizontalAlignment(HAlign_Center);}
        return W;
    }
    UEditableTextBox* Input(UWidgetBlueprint& B,UWidgetTree& T,const TCHAR* Name,const TCHAR* Hint)
    {
        auto* W=MakeWidget<UEditableTextBox>(B,T,FName(Name),true);
        W->SetHintText(FText::FromString(Hint)); W->SetMinDesiredWidth(66);
        FEditableTextBoxStyle Style=W->GetWidgetStyle(); Style.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),18)); W->SetWidgetStyle(Style);
        W->SetSelectAllTextWhenFocused(true); W->SetClearKeyboardFocusOnCommit(false); return W;
    }

    bool RefineLightingTab(UWidgetBlueprint& Panel)
    {
        const TCHAR* Key=TEXT("FF.FacilityAurora.LightingTabFit.20260920");
        if(Panel.GetOutermost()->GetMetaData().GetValue(&Panel,Key)==TEXT("1"))return false;
        auto* Tab=Cast<UButton>(Panel.WidgetTree->FindWidget(TEXT("Button_TabLighting")));
        auto* Label=Cast<UCommonTextBlock>(Panel.WidgetTree->FindWidget(TEXT("Text_TabLighting")));
        if(!Tab || !Label)return false;
        Label->SetAutoWrapText(false); StyleText(*Label,16,Green(),TEXT("Medium"));
        FButtonStyle Style=Tab->GetStyle(); Style.NormalPadding=FMargin(0); Style.PressedPadding=FMargin(0); Tab->SetStyle(Style);
        if(auto* Slot=Cast<UButtonSlot>(Label->Slot)) Slot->SetPadding(FMargin(6,9));
        Panel.GetOutermost()->GetMetaData().SetValue(&Panel,Key,TEXT("1"));
        return true;
    }

    bool BuildLighting(UWidgetBlueprint& B,UWidgetTree& T)
    {
        auto* Root=MakeWidget<UVerticalBox>(B,T,TEXT("Aurora_LightingV7")); T.RootWidget=Root;
        auto* Header=MakeWidget<UHorizontalBox>(B,T,TEXT("Aurora_LightingHeader"));
        H(*Header,*Text(B,T,TEXT("Text_LightingTitle"),TEXT("조명 편집 · 실시간 미리보기"),22));
        H(*Header,*Button(B,T,TEXT("Button_LightingToggle"),TEXT("Text_LightingToggle"),TEXT("켜짐 · 끄기")),false,0); V(*Root,*Header,12);
        auto* ColorRow=MakeWidget<UHorizontalBox>(B,T,TEXT("Aurora_ColorRow"));
        auto* WheelSize=MakeWidget<USizeBox>(B,T,TEXT("Aurora_ColorPickerSize")); WheelSize->SetWidthOverride(290); WheelSize->SetHeightOverride(290);
        WheelSize->AddChild(MakeWidget<UFF_FacilityColorPicker>(B,T,TEXT("ColorPicker_Lighting"),true)); H(*ColorRow,*WheelSize,false,18);
        auto* Channels=MakeWidget<UVerticalBox>(B,T,TEXT("Aurora_Channels"));
        const TCHAR* Names[]={TEXT("Input_LightingH"),TEXT("Input_LightingS"),TEXT("Input_LightingV"),TEXT("Input_LightingR"),TEXT("Input_LightingG"),TEXT("Input_LightingB")};
        const TCHAR* Labels[]={TEXT("색조 H  0~360°"),TEXT("채도 S  0~100%"),TEXT("명도 V  0~100%"),TEXT("빨강 R  0~255"),TEXT("초록 G  0~255"),TEXT("파랑 B  0~255")};
        for(int32 I=0;I<6;++I)
        {
            auto* Row=MakeWidget<UHorizontalBox>(B,T,FName(*FString::Printf(TEXT("Aurora_ChannelRow%d"),I)));
            H(*Row,*Text(B,T,*FString::Printf(TEXT("Aurora_ChannelLabel%d"),I),Labels[I],17));
            H(*Row,*Input(B,T,Names[I],TEXT("0")),false,0); V(*Channels,*Row,8);
        }
        H(*ColorRow,*Channels,true,0); V(*Root,*ColorRow,10);
        auto* Hex=MakeWidget<UHorizontalBox>(B,T,TEXT("Aurora_HexRow"));
        H(*Hex,*Text(B,T,TEXT("Aurora_HexLabel"),TEXT("HEX"),18),false,16);
        H(*Hex,*Input(B,T,TEXT("Input_LightingColor"),TEXT("#56DCE7")),true,0); V(*Root,*Hex,8);
        V(*Root,*Text(B,T,TEXT("Text_LightingColorStatus"),TEXT("원래 조명 색상 사용 중"),17,true),10);
        auto* Brightness=MakeWidget<UHorizontalBox>(B,T,TEXT("Aurora_BrightnessRow"));
        H(*Brightness,*Text(B,T,TEXT("Text_LightingBrightnessLabel"),TEXT("조명 밝기  0~500%"),19));
        H(*Brightness,*Text(B,T,TEXT("Text_LightingBrightness"),TEXT("100%"),22,true),false,0); V(*Root,*Brightness,2);
        auto* Slider=MakeWidget<USlider>(B,T,TEXT("Slider_LightingBrightness"),true);
        Slider->SetMinValue(0); Slider->SetMaxValue(5); Slider->SetStepSize(.05f); Slider->SetValue(1); Slider->MouseUsesStep=true;
        Slider->SetSliderBarColor(Green()); Slider->SetSliderHandleColor(Green()); V(*Root,*Slider,13);
        auto* Presets=MakeWidget<UHorizontalBox>(B,T,TEXT("Lighting_ColorPresets"));
        const TCHAR* Buttons[]={TEXT("Button_LightingOriginal"),TEXT("Button_LightingTeal"),TEXT("Button_LightingSunset"),TEXT("Button_LightingViolet")};
        const TCHAR* PresetLabels[]={TEXT("원래 색"),TEXT("청록"),TEXT("노을"),TEXT("보라")};
        for(int32 I=0;I<4;++I) H(*Presets,*Button(B,T,Buttons[I],*FString::Printf(TEXT("Text_LightingPreset%d"),I),PresetLabels[I]),true,I==3?0:7);
        V(*Root,*Presets,10);
        V(*Root,*Button(B,T,TEXT("Button_LightingObserve"),TEXT("Text_LightingObserve"),TEXT("장면 확인 · 접어서 카메라 이동")),10);
        auto* Footer=MakeWidget<UHorizontalBox>(B,T,TEXT("Aurora_LightingFooter"));
        H(*Footer,*Button(B,T,TEXT("Button_LightingReset"),TEXT("Text_LightingReset"),TEXT("기본값")));
        H(*Footer,*Button(B,T,TEXT("Button_LightingCancel"),TEXT("Text_LightingCancel"),TEXT("취소")));
        H(*Footer,*Button(B,T,TEXT("Button_LightingApply"),TEXT("Text_LightingApply"),TEXT("설정 적용"),true),true,0); V(*Root,*Footer,0);
        return true;
    }

    void Apply()
    {
        UWidgetBlueprint* Lighting=LoadExistingBlueprint(LightingPath);
        UWidgetBlueprint* Panel=LoadExistingBlueprint(PanelPath);
        if(!Lighting || !Panel || !Panel->WidgetTree || !Lighting->ParentClass->IsChildOf(UFF_FacilityLightingControls::StaticClass()))
        { UE_LOG(LogFFUIWbpBuilder,Error,TEXT("FF_AURORA_LIGHTING_FAIL: required authored WBPs are missing.")); return; }
        // Explicit Apply command updates this small child WBP once. Subsequent runs preserve designer edits.
        if(!Lighting->WidgetTree->FindWidget(TEXT("Aurora_LightingV7")))
        {
            if(!ResetWidgetTree(LogPrefix,*Lighting) || !BuildLighting(*Lighting,*Lighting->WidgetTree) || !SaveNewBlueprint(LogPrefix,*Lighting)) return;
        }
        UWidgetTree& T=*Panel->WidgetTree;
        if(T.FindWidget(TEXT("Button_TabLighting")))
        { UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_AURORA_LIGHTING_REUSE: designer layout preserved.")); return; }
        auto* Tabs=Cast<UHorizontalBox>(T.FindWidget(TEXT("Panel_InspectorTabs")));
        auto* Header=Cast<UHorizontalBox>(T.FindWidget(TEXT("Header_Row")));
        auto* Shell=Cast<UVerticalBox>(T.FindWidget(TEXT("Panel_Shell")));
        if(!Tabs || !Header || !Shell || !T.FindWidget(TEXT("Widget_LightingControls")))
        { UE_LOG(LogFFUIWbpBuilder,Error,TEXT("FF_AURORA_LIGHTING_FAIL: inspector contract mismatch.")); return; }
        Panel->Modify(); T.Modify();
        H(*Tabs,*Button(*Panel,T,TEXT("Button_TabLighting"),TEXT("Text_TabLighting"),TEXT("조명 편집")),true,0);
        auto* Minimize=Button(*Panel,T,TEXT("Button_LightingMinimize"),TEXT("Text_LightingMinimize"),TEXT("접기"));
        H(*Header,*Minimize,false,8); Header->ShiftChild(FMath::Max(0,Header->GetChildrenCount()-2),Minimize);
        Minimize->SetVisibility(ESlateVisibility::Collapsed);
        auto* Compact=MakeWidget<UBorder>(*Panel,T,TEXT("Panel_LightingCompact"),true);
        Compact->SetPadding(FMargin(18)); Compact->SetBrushColor(Paper());
        Compact->AddChild(Text(*Panel,T,TEXT("Text_LightingCompactSummary"),TEXT("원래 조명 · 밝기 100% · 켜짐"),18,true));
        V(*Shell,*Compact,0); Compact->SetVisibility(ESlateVisibility::Collapsed);
        RefineLightingTab(*Panel);
        if(!SaveNewBlueprint(LogPrefix,*Panel)) return;
        UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_AURORA_LIGHTING_SUCCESS: HSV/RGB/HEX, preview transaction, integrated tab and compact observation controls authored."));
    }
    FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityAuroraLighting"),
        TEXT("Apply approved V7 lighting controls and inspector tab to authored WBPs."),FConsoleCommandDelegate::CreateStatic(&Apply));
    void Refine()
    {
        auto* Panel=LoadExistingBlueprint(PanelPath);
        if(Panel && RefineLightingTab(*Panel))SaveNewBlueprint(LogPrefix,*Panel);
    }
    FAutoConsoleCommand RefineCommand(TEXT("FF.RefineFacilityAuroraLighting"),
        TEXT("Correct the V7 lighting tab to a single line without replacing authored layouts."),FConsoleCommandDelegate::CreateStatic(&Refine));
}
#endif
