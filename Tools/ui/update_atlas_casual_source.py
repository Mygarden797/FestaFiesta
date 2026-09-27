from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=root/'Source/FestaFiestaEditor/Private/UI/Atlas/FF_AtlasWbpBuilder.cpp'
s=p.read_text(encoding='utf-8-sig').replace('#include "UI/Data/FF_UIPalette.h"','#include "UI/Data/FF_UIPalette.h"\n#include "UI/Frontend/FF_FrontendCasualBuilder.h"')
start=s.index('\tbool BuildAtlasGlobeSetupLayout(')
end=s.index('\n\t/**',start)
s=s[:start]+'''	bool BuildCasualCountryRow(UWidgetBlueprint& Blueprint,UWidgetTree& Tree)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};
        auto* Root=B.Make<USizeBox>(TEXT("SizeBox_Root"),true);Root->SetHeightOverride(54);Tree.RootWidget=Root;
        auto* Overlay=B.Make<UOverlay>(TEXT("Overlay_Root"));Root->AddChild(Overlay);
        auto* Ring=B.Make<UImage>(TEXT("Image_FocusRing"),true);Ring->SetBrush(FFFrontendCasual::Rounded(FLinearColor::Transparent,B.T.Teal,16,3));Ring->SetVisibility(ESlateVisibility::Hidden);
        auto* RS=Overlay->AddChildToOverlay(Ring);RS->SetHorizontalAlignment(HAlign_Fill);RS->SetVerticalAlignment(VAlign_Fill);
        auto* Row=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Content"));auto* OS=Overlay->AddChildToOverlay(Row);OS->SetHorizontalAlignment(HAlign_Fill);OS->SetVerticalAlignment(VAlign_Center);OS->SetPadding(FMargin(18,0));
        auto* Flag=B.Icon(TEXT("Image_CountryFlag"),TEXT("Flag_KR"),40,true);B.H(Row,B.Sized(TEXT("SizeBox_Flag"),Flag,42,30),18);
        auto* Label=B.Make<UCommonTextBlock>(TEXT("Text_Label"),true);
        Label->SetStyle(LoadClass<UCommonTextStyle>(nullptr,TEXT("/Game/FestaFiesta/UI/Styles/CTS_FF_FrontendInk.CTS_FF_FrontendInk_C")));
        Label->SetText(LOCTEXT("CountryRowPlaceholder","대한민국"));B.H(Row,Label,12,true);
        B.H(Row,B.Sized(TEXT("SizeBox_Status"),B.Text(TEXT("Text_PinBadge"),LOCTEXT("CountryStatusPlaceholder","준비 중"),16,true,true),104,0),14);
        auto* Arrow=B.Icon(TEXT("Image_Chevron"),TEXT("Chevron"),16,true);Arrow->SetColorAndOpacity(B.T.Ink);B.H(Row,Arrow);
        return true;
    }

    bool BuildAtlasGlobeSetupLayout(UWidgetBlueprint& Blueprint,UWidgetTree& Tree,
        UClass* RowClass,UClass* NavButtonClass,UClass* PrimaryButtonClass)
    {
        if(!RowClass || !NavButtonClass || !PrimaryButtonClass)return false;
        FFFrontendCasual::FBuilder B{Blueprint,Tree};auto* Root=B.Make<UCanvasPanel>(TEXT("CanvasPanel_Root"));Tree.RootWidget=Root;
        auto* Drag=B.Make<UBorder>(TEXT("Border_DragSurface"),true);Drag->SetBrushColor(FLinearColor::Transparent);
        StretchOnCanvas(*Root,*Drag);auto* Pins=B.Make<UCanvasPanel>(TEXT("CanvasPanel_Pins"),true);Pins->SetVisibility(ESlateVisibility::SelfHitTestInvisible);StretchOnCanvas(*Root,*Pins);
        auto* Column=B.Make<UVerticalBox>(TEXT("VerticalBox_LeftColumn"));B.SidePanel(Root,Column,620,56);
        B.Heading(Column,LOCTEXT("AtlasCountriesHeading","어느 나라로 떠날까요?"),LOCTEXT("AtlasCountriesSubheading","축제를 열고 싶은 나라를 선택하세요!"),29);
        auto* Countries=B.Make<UScrollBox>(TEXT("ScrollBox_Countries"),true);Countries->SetConsumeMouseWheel(EConsumeMouseWheel::Always);Countries->SetClipping(EWidgetClipping::ClipToBounds);
        B.V(Column,Countries,16,true);
        auto* Details=B.Make<UScrollBox>(TEXT("ScrollBox_CountryDetails"),true);Details->SetVisibility(ESlateVisibility::Collapsed);Details->SetClipping(EWidgetClipping::ClipToBounds);B.V(Column,Details,14);
        auto* Content=B.Make<UVerticalBox>(TEXT("VerticalBox_Panel"));auto* Panel=B.Panel(TEXT("Border_Panel"),Content,18,22,false,true);Details->AddChild(Panel);
        auto* Info=B.Make<UHorizontalBox>(TEXT("HorizontalBox_CountryInfo"));
        auto* Flag=B.Icon(TEXT("Image_CountryFlag"),TEXT("Flag_KR"),72,true);B.H(Info,B.Sized(TEXT("SizeBox_CountryFlag"),Flag,98,66),20);
        auto* Text=B.Make<UVerticalBox>(TEXT("VerticalBox_CountryInfo"));
        B.V(Text,B.Text(TEXT("Text_CountryName"),LOCTEXT("AtlasCountryPlaceholder","대한민국"),26,false,true),6);
        B.V(Text,B.Text(TEXT("Text_FestivalCount"),FText::GetEmpty(),17,true,true));B.H(Info,Text,0,true);B.V(Content,Info,14);
        auto* Lock=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Lock"));
        auto* LockIcon=B.Icon(TEXT("Image_LockedIcon"),TEXT("Lock"),20,true);LockIcon->SetColorAndOpacity(B.T.InkMuted);B.H(Lock,LockIcon,10);
        B.H(Lock,B.Text(TEXT("Text_LockReason"),FText::GetEmpty(),16,true,true),0,true);B.V(Content,Lock,8);
        B.V(Content,B.Button(TEXT("Button_SelectCountry"),PrimaryButtonClass,LOCTEXT("AtlasGlobeSelectCountry","축제 지도 보기"),518,56,TEXT("Map")),8);
        B.V(Content,B.Button(TEXT("Button_ClosePanel"),NavButtonClass,LOCTEXT("AtlasGlobeClosePanel","닫기"),518,50,TEXT("Folder")));
        B.V(Column,B.Text(TEXT("Text_ControlHints"),LOCTEXT("AtlasGlobeHintPlaceholder","드래그: 회전   국가 목록 / 조형물: 살펴보기\\nEsc: 뒤로"),14,true,true));
        return true;
    }
'''+s[end:]
start=s.index('\t\t// 나라 행은 이미')
end=s.index('\n\t\tUWidgetBlueprint* Globe',start)
s=s[:start]+'''		const FString RowPackage=AtlasFolder/TEXT("WBP_FF_AtlasCountryRow");
        auto* RowBP=ReplaceWidgetBlueprint(AtlasLogPrefix,RowPackage,AtlasFolder,*UFF_AtlasPinButton::StaticClass(),
            [](UWidgetBlueprint& BP,UWidgetTree& Tree){return BuildCasualCountryRow(BP,Tree);});
        if(!RowBP || !RowBP->GeneratedClass)return;
        auto* RowDefaults=RowBP->GeneratedClass->GetDefaultObject<UFF_AtlasPinButton>();
        RowBP->Modify();RowDefaults->Modify();
        const auto Ink=FFFrontendCasual::Get().Ink;
        RowDefaults->ApplyGeneratedButtonDefaults(LoadClass<UCommonButtonStyle>(nullptr,
            TEXT("/Game/FestaFiesta/UI/Styles/CBS_FF_FrontendNav.CBS_FF_FrontendNav_C")),Ink,Ink,Ink,false,false,false);
        RowDefaults->SetIsSelectable(true);RowDefaults->SetIsToggleable(true);SaveAssetPackage(AtlasLogPrefix,*RowBP);
        UClass* RowClass=RowBP->GeneratedClass;
'''+s[end:]
s=s.replace('// `PinButtonClass` 는 CDO 프로퍼티라 컴파일 뒤에 넣는다. 비면 목록이 하나도 안 뜬다.', '''// UI 국기는 CDO가 강하게 참조해서 쿠킹에도 포함한다.
        auto* Defaults=Globe->GeneratedClass->GetDefaultObject<UFF_AtlasGlobeScreen>();
        Defaults->Modify();
        for(const TCHAR* Code : {TEXT("KR"),TEXT("BR"),TEXT("DE"),TEXT("JP"),TEXT("US"),TEXT("ES"),TEXT("TH"),TEXT("AU")})
            Defaults->CountryFlagTextures.Add(FName(Code),LoadObject<UTexture2D>(nullptr,*MakeObjectPath(FFFrontendCasual::ArtPackage(FString(TEXT("Flag_"))+Code))));
        // PinButtonClass는 나라 목록에만 쓰고 지구 액터는 변경하지 않는다.''')
s=s.replace('셋업 스타일 왼쪽 패널','캐주얼 스타일 왼쪽 패널').replace('single tall LightTeal left panel','Clean Casual left panel')
p.write_text(s,encoding='utf8')
p=root/'Source/FestaFiesta/Private/UI/Atlas/FF_AtlasGlobeScreen.cpp'
s=p.read_text(encoding='utf-8-sig')
needle='\t\tPin->SetPinData(State.CountryId, Title, Badge, State.bSelectable, LockReason);'
s=s.replace(needle,needle+'''
        if(UImage* Flag=Cast<UImage>(Pin->GetWidgetFromName(TEXT("Image_CountryFlag"))))
        {
            const auto* Texture=CountryFlagTextures.Find(State.CountryId);
            Flag->SetBrushFromTexture(Texture?Texture->Get():nullptr);
            Flag->SetVisibility(Texture && *Texture?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
        }
        Pin->SetIsSelectable(true);Pin->SetIsToggleable(true);
''')
s=s.replace('if (Pair.Value) { Pair.Value->SetRenderScale(Pair.Key == PanelCountryId ? FVector2D(1.02, 1.02) : FVector2D(1, 1)); }','if (Pair.Value) { Pair.Value->SetIsSelected(Pair.Key == PanelCountryId, false); }')
s=s.replace('void UFF_AtlasGlobeScreen::RefreshCountryPanel()\n{','''void UFF_AtlasGlobeScreen::RefreshCountryPanel()
{
    if(UImage* Flag=Cast<UImage>(GetWidgetFromName(TEXT("Image_CountryFlag"))))
    {
        const auto* Texture=CountryFlagTextures.Find(PanelCountryId);
        Flag->SetBrushFromTexture(Texture?Texture->Get():nullptr);
        Flag->SetVisibility(Texture && *Texture?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    }
''')
p.write_text(s,encoding='utf8')
