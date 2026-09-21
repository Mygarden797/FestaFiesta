// Explicit editor migration: save the requested green skin in the authored facility WBPs.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace FFFacilityAuroraTheme
{
using namespace FFUIWbpBuilder;
static FLinearColor Color(uint8 R,uint8 G,uint8 B){return FLinearColor::FromSRGBColor(FColor(R,G,B));}
static const FLinearColor Ink=Color(0x20,0x4c,0x36), Muted=Color(0x5c,0x75,0x63), Light=Color(0xf5,0xff,0xe8);
static bool Has(const FString& Name,std::initializer_list<const TCHAR*> Keys)
{for(const TCHAR* Key:Keys) if(Name.Contains(Key)) return true;return false;}
static bool IsDanger(const FString& Name)
{return Has(Name,{TEXT("Warning"),TEXT("Reason"),TEXT("Feedback"),TEXT("Remove"),TEXT("Cancel"),TEXT("Required")});}
static bool IsGold(const FString& Name)
{return Name==TEXT("Text_Cost")||Has(Name,{TEXT("Gold"),TEXT("Wallet"),TEXT("SelectedCost"),TEXT("Price"),TEXT("PlacementCost")});}
static bool IsDark(const FString& Name)
{return Name==TEXT("Border_Tools")||Has(Name,{TEXT("PlacementHelp"),TEXT("QuickTools"),TEXT("BuildGuide"),TEXT("CameraHint")});}
static bool IsPrimary(const FString& Name)
{return Has(Name,{TEXT("PlaceSelected"),TEXT("ConfirmPlacement"),TEXT("Button_Upgrade"),TEXT("Button_Move"),
    TEXT("ScheduleProgram"),TEXT("Button_Purchase"),TEXT("Button_Apply"),TEXT("LightingApply"),TEXT("Button_ViewFacility"),TEXT("Button_Inspect"),TEXT("Button_Confirm"),TEXT("Button_StartMinigame")});}
static FSlateBrush MaterialBrush(const TCHAR* Role)
{
    FString Name=FString(TEXT("MI_UI_FacilityAurora_"))+Role;
    FString Path=FString(TEXT("/Game/FestaFiesta/UI/Facility/Aurora/"))+Name+TEXT(".")+Name;
    FSlateBrush Brush; Brush.DrawAs=ESlateBrushDrawType::Image;
    Brush.SetResourceObject(LoadObject<UMaterialInterface>(nullptr,*Path));
    Brush.ImageSize=FVector2D(128,48); Brush.TintColor=FSlateColor(FLinearColor::White);
    return Brush;
}
static bool Apply(UWidgetBlueprint& BP)
{
    const TCHAR* Version=TEXT("FF.FacilityAurora.GreenV7.2.20260920");
    if(BP.GetOutermost()->GetMetaData().GetValue(&BP,Version)==TEXT("1"))return true;
    if(!BP.WidgetTree)return false;
    FSlateBrush Paper=MaterialBrush(TEXT("Paper")),Header=MaterialBrush(TEXT("Header")),Accent=MaterialBrush(TEXT("Accent")),
        Action=MaterialBrush(TEXT("Action")),Dark=MaterialBrush(TEXT("Dark")),Gold=MaterialBrush(TEXT("Gold"));
    for(const FSlateBrush* Brush:{&Paper,&Header,&Accent,&Action,&Dark,&Gold})if(!Brush->GetResourceObject())return false;
    BP.Modify();BP.WidgetTree->Modify();
    const bool Badge=BP.GetName().Contains(TEXT("WorldBadge"));
    BP.WidgetTree->ForEachWidget([&](UWidget* Widget)
    {
        const FString Name=Widget->GetName();
        if(UBorder* Border=Cast<UBorder>(Widget))
        {
            if(Has(Name,{TEXT("Shadow"),TEXT("Swatch"),TEXT("ColorPreview"),TEXT("OriginalColor"),TEXT("CurrentColor"),TEXT("Icon_")}))return;
            // Transparent layout surfaces must not become hit-blocking opaque backdrops.
            const bool Empty=Border->Background.TintColor.GetSpecifiedColor().A*Border->GetBrushColor().A<.01f;
            if(Empty&&!Name.StartsWith(TEXT("Aurora_")))return;
            if(IsDanger(Name))return;
            const FSlateBrush* Brush=&Paper;
            if(IsDark(Name))Brush=&Dark;
            else if(IsGold(Name))Brush=&Gold;
            else if(Has(Name,{TEXT("Header"),TEXT("Available"),TEXT("Scheduled")}))Brush=&Header;
            else if(Has(Name,{TEXT("Aurora_Queue"),TEXT("OperationQueue"),TEXT("Revenue"),TEXT("Total"),TEXT("Program_Card"),
                TEXT("ProgramStatus"),TEXT("SynergySummary"),TEXT("Effect"),TEXT("Selected"),TEXT("Active"),TEXT("Next"),TEXT("Thumbnail")}))Brush=&Accent;
            Border->SetBrush(*Brush);Border->SetBrushColor(FLinearColor::White);
            Border->SetContentColorAndOpacity(FLinearColor::White);
        }
        if(UButton* Button=Cast<UButton>(Widget))
        {
            if(IsDanger(Name))return;
            FButtonStyle Style=Button->GetStyle();
            const bool Tab=Has(Name,{TEXT("Tab"),TEXT("Button_Today"),TEXT("Button_Total"),TEXT("Filter"),TEXT("Button_All"),TEXT("Button_Discovered"),TEXT("Button_Undiscovered"),TEXT("Button_Active")});
            const bool Primary=IsPrimary(Name);
            Style.Normal=Primary?Action:Paper;
            Style.Hovered=Primary?Action:Accent;
            Style.Pressed=Action;
            // Existing native tab presenters disable the current tab to mark its selected state.
            Style.Disabled=(Tab||Primary)?Action:Paper;
            if(!Tab)Style.Disabled.TintColor=FSlateColor(Primary?Color(0x9f,0xad,0x98):Color(0xcc,0xd8,0xc4));
            Button->SetStyle(Style);
            if(auto* BuildAction=Cast<UFF_BuildToolsActionButton>(Button);BuildAction&&BuildAction->bUseSelectionStyle)
            {
                BuildAction->SelectionStyle=Style;
                BuildAction->SelectionStyle.Normal=Action;
                BuildAction->SelectionStyle.Hovered=Action;
                BuildAction->SelectionStyle.Pressed=Action;
            }
        }
        if(auto* Input=Cast<UEditableTextBox>(Widget))
        {
            FEditableTextBoxStyle Style=Input->GetWidgetStyle();
            Style.ForegroundColor=FSlateColor(Ink);
            Style.FocusedForegroundColor=FSlateColor(Ink);
            Style.ReadOnlyForegroundColor=FSlateColor(Muted);
            Style.TextStyle.SetColorAndOpacity(FSlateColor(Ink));
            Input->SetWidgetStyle(Style);
            Input->SetForegroundColor(Ink);
        }
        if(UTextBlock* Text=Cast<UTextBlock>(Widget))
        {
            if(IsDanger(Name)||Name.Contains(TEXT("Hex")))return;
            FLinearColor TextColor=Has(Name,{TEXT("Label"),TEXT("Hint"),TEXT("Caption"),TEXT("Notice")})?Muted:Ink;
            if(IsGold(Name))TextColor=Color(0x6b,0x50,0x1d);
            for(UWidget* Parent=Text->GetParent();Parent;Parent=Parent->GetParent())
            {
                if(IsDark(Parent->GetName())){TextColor=Light;break;}
                if(UButton* Button=Cast<UButton>(Parent))
                {TextColor=IsPrimary(Button->GetName())?Light:Ink;break;}
            }
            Text->SetColorAndOpacity(FSlateColor(TextColor));
            FSlateFontInfo Font=Text->GetFont();
            if(!Badge&&Font.Size<16)Font.Size=16;
            Text->SetFont(Font);
        }
    });
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1"));
    return SaveNewBlueprint(TEXT("FF_FACILITY_AURORA_THEME"),BP);
}
static void Run()
{
    const TCHAR* Paths[]={
        TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel"),
        TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityLightingControls"),
        TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilitySynergyEntry"),
        TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard"),
        TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramPicker"),
        TEXT("/Game/FestaFiesta/UI/FacilityRevenue/WBP_FF_FacilityWorldBadge"),
        TEXT("/Game/FestaFiesta/UI/FacilityRevenue/WBP_FF_FacilityRevenueRanking"),
        TEXT("/Game/FestaFiesta/UI/FacilityRevenue/WBP_FF_FacilityRevenueRankRow"),
        TEXT("/Game/FestaFiesta/UI/FacilityCodex/WBP_FF_FacilityCodexScreen"),
        TEXT("/Game/FestaFiesta/UI/FacilityCodex/WBP_FF_FacilityCodexCard"),
        TEXT("/Game/FestaFiesta/UI/FacilityCodex/WBP_FF_FacilityCodexLauncher"),
        TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild"),
        TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile"),
        TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildTools"),
        TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildSynergy")};
    bool Success=true;
    for(const TCHAR* Path:Paths)
    {
        UWidgetBlueprint* BP=LoadExistingBlueprint(Path);
        const bool Saved=BP&&Apply(*BP);Success&=Saved;
        UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_FACILITY_AURORA_THEME_%s %s"),Saved?TEXT("PASS"):TEXT("FAIL"),Path);
    }
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_FACILITY_AURORA_THEME_COMPLETE_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityAuroraTheme"),
    TEXT("Apply the requested V7 green materials to the explicit facility WBP allowlist once."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
