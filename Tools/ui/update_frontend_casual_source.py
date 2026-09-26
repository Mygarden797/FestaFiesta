"""캐주얼 프런트엔드 생성기 이행. 한 번 실행하는 소스 편집 도구."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=root/'Source/FestaFiestaEditor/Private/UI/Frontend/FF_FrontendSetupStyleWbpBuilder.cpp'
s=p.read_text(encoding='utf-8-sig')
start=s.index('// 타이틀 옵션 화면')
end=s.index('#include "AssetToolsModule.h"')
s=s[:start]+'''// 타이틀의 크림·민트·숲색을 플레이 허브·옵션·저장 목록으로 확장한다.
// FF.ApplyFrontendSetupStyle은 기존 프런트엔드 에셋을 재생성한다.
// 인게임 ESC·준비·라이브 HUD와 지구 씬에는 쓰지 않는다.

'''+s[end:]
s=s.replace('#include "UI/Core/FF_PauseOptionsLightTealStyle.h"','#include "UI/Frontend/FF_FrontendCasualBuilder.h"\n#include "UI/Frontend/FF_SaveListScreen.h"\n#include "UI/Frontend/FF_SaveSlotCard.h"\n#include "UObject/UnrealType.h"')
s=s.replace('FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette)','FFFrontendCasual::FTokens Tokens = FFFrontendCasual::Get()')
for a,b in [('HeaderHeight = 88','HeaderHeight = 126'),('FooterHeight = 104','FooterHeight = 112'),('CategoryHeight = 56','CategoryHeight = 64'),('CategoryGap = 8','CategoryGap = 12'),('SidePanelWidth = 460','SidePanelWidth = 620'),('ActionHeight = 56','ActionHeight = 60')]:s=s.replace(a,b)
s=s.replace('Font.Size = 17','Font.Size = 20')
start=s.index('\t\tif (bPrimary)',s.index('bool ApplyButtonStyle'))
end=s.index('\n\t\t// 상태별 글자',start)
s=s[:start]+'''		const auto Surface = [&](const FLinearColor& Fill, const FLinearColor& Line, float Width=1.5f)
        { return FFFrontendCasual::Rounded(Fill,Line,18.f,Width); };
        Style->NormalBase = Surface(bPrimary?Tokens.Teal:Tokens.Paper,Tokens.LineStrong);
        Style->NormalHovered = Surface(bPrimary?Tokens.TealDeep:Tokens.Hover,Tokens.Teal,2.f);
        Style->NormalPressed = Surface(bPrimary?Tokens.TealDeep:Tokens.TealSoft,Tokens.Teal,3.f);
        Style->SelectedBase = Surface(bPrimary?Tokens.TealDeep:Tokens.TealSoft,Tokens.Teal,2.5f);
        Style->SelectedHovered = Style->SelectedBase;
        Style->SelectedPressed = Style->NormalPressed;
        Style->Disabled = Surface(Tokens.Disabled,Tokens.Line);
'''+s[end:]
start=s.index('\tbool BuildButtonLayout(')
end=s.index('\n\t// =========================================================================',start)
s=s[:start]+'''	bool BuildButtonLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
        const TSubclassOf<UCommonTextStyle>& TextStyleClass, const bool bPrimary)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};
        auto* Root=B.Make<USizeBox>(TEXT("SizeBox_Root"),true);
        auto* Overlay=B.Make<UOverlay>(TEXT("Overlay_Root"));
        Root->SetWidthOverride(Layout::RailWidth);Root->SetHeightOverride(Layout::CategoryHeight);
        Root->AddChild(Overlay);Tree.RootWidget=Root;
        // 中身を塗りつぶさず、フォーカスは輪郭だけで示す。
        auto* Ring=B.Make<UImage>(TEXT("Image_FocusRing"),true);
        Ring->SetBrush(FFFrontendCasual::Rounded(FLinearColor::Transparent,
            bPrimary?B.T.LineStrong:B.T.Teal,18,3));
        Ring->SetVisibility(ESlateVisibility::Hidden);
        auto* RS=Overlay->AddChildToOverlay(Ring);RS->SetHorizontalAlignment(HAlign_Fill);RS->SetVerticalAlignment(VAlign_Fill);
        auto* Row=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Content"));
        auto* OS=Overlay->AddChildToOverlay(Row);OS->SetHorizontalAlignment(HAlign_Fill);OS->SetVerticalAlignment(VAlign_Center);OS->SetPadding(FMargin(22,0));
        auto* Icon=B.Icon(TEXT("Image_Icon"),TEXT("Spark"),28,true);
        B.H(Row,B.Sized(TEXT("SizeBox_Icon"),Icon,32,32),16);
        auto* Label=B.Make<UCommonTextBlock>(TEXT("Text_Label"),true);
        Label->SetStyle(TextStyleClass);Label->SetText(LOCTEXT("FrontendButtonPlaceholder","선택"));
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);B.H(Row,Label,12,true);
        auto* Chevron=B.Icon(TEXT("Image_Chevron"),TEXT("Chevron"),18,true);
        B.H(Row,B.Sized(TEXT("SizeBox_Chevron"),Chevron,18,18));
        return true;
    }
'''+s[end:]
s=s.replace('// 中身を塗りつぶさず、フォーカスは輪郭だけで示す.','// 내부를 덮지 않고 포커스는 테두리로만 나타낸다.')
s=s.replace('// 中身を塗りつぶさず、フォーカスは輪郭だけで示す。','// 내부를 덮지 않고 포커스는 테두리로만 나타낸다.')
# 전체 화면 구조는 유지하고 화면 전용 팔레트를 쓴다.
s=s.replace('MakeFlatBrush(Palette->InGame.Paper)','MakeFlatBrush(Tokens.PaperSunken)')
s=s.replace('MakeFlatBrush(Palette->InGame.Teal)','MakeFlatBrush(Tokens.Paper)')
s=s.replace('StyleText(*ScreenTitle, 28, Tokens.White','StyleText(*ScreenTitle, 34, Tokens.Ink')
s=s.replace('MakeRoundedBrush(Palette->InGame.PaperSunken, Palette->InGame.Line, 8.f)','MakeRoundedBrush(Tokens.Paper, Tokens.Line, 28.f)')
s=s.replace('MakeFlatBrush(Palette->InGame.StatusBar)','MakeFlatBrush(Tokens.PaperSunken)')
s=s.replace('StyleText(*CategoryTitle, 24','StyleText(*CategoryTitle, 28')
s=s.replace('ApplyHint->SetAutoWrapText(false)','ApplyHint->SetAutoWrapText(true)')
s=s.replace('LOCTEXT("OptionsApplyHintPlaceholder", "바꾼 값이 있으면 여기 안내가 나온다")','LOCTEXT("OptionsApplyHintPlaceholder", "설정을 바꾼 뒤 적용해 주세요")')
# 카테고리 아이콘은 인스턴스 데이터다.
s=s.replace('FSlateBrush(),\n\t\t\t\tFVector2D(Layout::RailWidth', 'FFFrontendCasual::Art(TArray<FString>{TEXT("Language"),TEXT("Accessibility"),TEXT("Sound"),TEXT("Graphics"),TEXT("Controls"),TEXT("Info")}[Index]),\n\t\t\t\tFVector2D(Layout::RailWidth')
s=s.replace('LOCTEXT("OptionsApply", "적용"), FSlateBrush(),','LOCTEXT("OptionsApply", "적용"), FFFrontendCasual::Art(TEXT("Check")),')
s=s.replace('LOCTEXT("OptionsBack", "뒤로"), FSlateBrush(),','LOCTEXT("OptionsBack", "뒤로"), FFFrontendCasual::Art(TEXT("Back")),')
# 머리 띠에 보조 설명과 브랜드 마크를 더한다.
anchor='\t\tif (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(HeaderBox))'
s=s.replace(anchor,'''		FFFrontendCasual::FBuilder HeaderBuilder{Blueprint,Tree};
        HeaderBuilder.H(HeaderRow,HeaderBuilder.Text(TEXT("Text_OptionsSubtitle"),
            LOCTEXT("OptionsSubtitle","나에게 맞는 축제 여행을 준비하세요"),18,true),24);
        auto* Brand=HeaderBuilder.Make<UImage>(TEXT("Image_BrandMark"));
        Brand->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/FestaFiesta/UI/Title/CleanCasual/Brand/T_FF_Title_CasualBright_Logo_512.T_FF_Title_CasualBright_Logo_512")));
        Brand->SetOpacity(0.55f);HeaderBuilder.H(HeaderRow,HeaderBuilder.Sized(TEXT("SizeBox_BrandMark"),Brand,70,70));
'''+anchor)
start=s.index('\tbool BuildPlayHubLayout(')
end=s.index('\n\t// =========================================================================',start)
s=s[:start]+'''	bool BuildPlayHubLayout(UWidgetBlueprint& Blueprint,UWidgetTree& Tree,UClass* ButtonClass,UClass* PrimaryButtonClass)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};
        auto* Root=B.Make<UCanvasPanel>(TEXT("CanvasPanel_Content"));Tree.RootWidget=Root;
        auto* Column=B.Make<UVerticalBox>(TEXT("VerticalBox_Column"));B.SidePanel(Root,Column);
        B.Heading(Column,LOCTEXT("PlayHubTitle","플레이"),LOCTEXT("PlayHubSubtitle","세상의 축제를 만들어보세요!"),44);
        auto* Scroll=B.Make<UScrollBox>(TEXT("ScrollBox_Content"));
        auto* Content=B.Make<UVerticalBox>(TEXT("VerticalBox_Content"));Scroll->AddChild(Content);B.V(Column,Scroll,20,true);
        auto* Continue=B.Make<UVerticalBox>(TEXT("VerticalBox_Continue"));
        B.V(Continue,B.Button(TEXT("Button_Continue"),ButtonClass,LOCTEXT("PlayHubContinue","최근 축제 계속하기"),504,62,TEXT("Continue")),8);
        B.V(Continue,B.Text(TEXT("Text_ContinueHelp"),LOCTEXT("ContinueHelp","최근에 플레이한 기록을 이어갑니다."),16,true),12);
        auto* Lock=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Locked"));
        auto* LockIcon=B.Icon(TEXT("Image_LockedIcon"),TEXT("Clock"),28,true);LockIcon->SetColorAndOpacity(B.T.InkMuted);
        B.H(Lock,LockIcon,14);B.H(Lock,B.Text(TEXT("Text_LockedNotice"),LOCTEXT("PlayHubNoRecord","기록 없음"),18,true,true),0,true);
        B.V(Continue,B.Panel(TEXT("Border_ContinueStatus"),Lock,16,16,true));
        B.V(Content,B.Panel(TEXT("Border_ContinueCard"),Continue,18,24),18);
        B.V(Content,B.Button(TEXT("Button_NewFestival"),PrimaryButtonClass,LOCTEXT("PlayHubNewFestival","새 축제 시작"),552,94,TEXT("Plus")),8);
        B.V(Content,B.Text(TEXT("Text_NewFestivalHelp"),LOCTEXT("NewFestivalHelp","새로운 축제를 만들러 떠나볼까요?"),16,true),20);
        auto* Saved=B.Make<UVerticalBox>(TEXT("VerticalBox_Saved"));
        B.V(Saved,B.Button(TEXT("Button_SaveList"),ButtonClass,LOCTEXT("PlayHubSaveList","저장 기록 선택"),504,62,TEXT("Save")),10);
        B.V(Saved,B.Text(TEXT("Text_SaveHelp"),LOCTEXT("SaveHelp","저장한 축제 기록을 찾아볼 수 있습니다."),16,true),12);
        auto* Empty=B.Make<UVerticalBox>(TEXT("VerticalBox_Empty"));
        auto* Folder=B.Icon(TEXT("Image_EmptyFolder"),TEXT("Folder"),44);Folder->SetColorAndOpacity(B.T.InkFaint);
        auto* FS=Empty->AddChildToVerticalBox(Folder);FS->SetHorizontalAlignment(HAlign_Center);FS->SetPadding(FMargin(0,4,0,10));
        auto* EmptyText=B.Text(TEXT("Text_RecentEmpty"),LOCTEXT("PlayHubRecentEmpty","저장 기록이 없습니다"),18,true,true);EmptyText->SetJustification(ETextJustify::Center);
        B.V(Empty,EmptyText);B.V(Saved,B.Panel(TEXT("Border_SaveEmpty"),Empty,18,18,true));
        auto* Recent=B.Make<UVerticalBox>(TEXT("Panel_Recent"),true);B.V(Saved,Recent);
        B.V(Content,B.Panel(TEXT("Border_SaveCard"),Saved,18,24));
        B.V(Column,B.Button(TEXT("Button_Back"),ButtonClass,LOCTEXT("PlayHubBack","뒤로"),552,62,TEXT("Back")));
        return true;
    }

    bool BuildSaveListCasual(UWidgetBlueprint& Blueprint,UWidgetTree& Tree,UClass* ButtonClass)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};auto* Root=B.Make<UCanvasPanel>(TEXT("CanvasPanel_Root"));Tree.RootWidget=Root;
        auto* Column=B.Make<UVerticalBox>(TEXT("VerticalBox_Column"));B.SidePanel(Root,Column,820);
        B.Heading(Column,LOCTEXT("SaveListScreenTitle","저장 기록"),LOCTEXT("SaveListSubtitle","다시 만나고 싶은 축제를 찾아보세요."));
        B.V(Column,B.Text(TEXT("Text_SaveSystemNotice"),FText::GetEmpty(),16,true,true),18);
        auto* Scroll=B.Make<UScrollBox>(TEXT("ScrollBox_Saves"));auto* Content=B.Make<UVerticalBox>(TEXT("VerticalBox_Content"));Scroll->AddChild(Content);B.V(Column,Scroll,20,true);
        for(bool Auto : {true,false})
        {
            auto* Group=B.Make<UVerticalBox>(Auto?TEXT("Panel_AutoGroup"):TEXT("Panel_ManualGroup"),true);
            B.V(Group,B.Text(Auto?TEXT("Text_AutoHeader"):TEXT("Text_ManualHeader"),Auto?LOCTEXT("SaveAuto","자동 저장"):LOCTEXT("SaveManual","수동 저장"),24),14);
            B.V(Group,B.Make<UVerticalBox>(Auto?TEXT("VerticalBox_Auto"):TEXT("VerticalBox_Manual"),true));B.V(Content,Group,22);
        }
        B.V(Content,B.Text(TEXT("Text_Empty"),LOCTEXT("SaveListEmpty","저장 기록이 없습니다"),22,true,true));
        B.V(Column,B.Button(TEXT("Button_Back"),ButtonClass,LOCTEXT("SaveListBack","뒤로"),760,62,TEXT("Back")));return true;
    }

    bool BuildSaveCardCasual(UWidgetBlueprint& Blueprint,UWidgetTree& Tree)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};auto* Root=B.Make<UOverlay>(TEXT("Overlay_Root"));Tree.RootWidget=Root;
        auto* Row=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Content"));auto* Info=B.Make<UVerticalBox>(TEXT("VerticalBox_Info"));
        B.V(Info,B.Text(TEXT("Text_FestivalName"),LOCTEXT("SaveCardNamePlaceholder","축제 이름"),24,false,true),5);
        B.V(Info,B.Text(TEXT("Text_Region"),FText::GetEmpty(),16,true,true),4);
        B.V(Info,B.Text(TEXT("Text_DayStage"),FText::GetEmpty(),16,true,true));
        B.H(Row,B.Icon(TEXT("Image_KindIcon"),TEXT("Save"),32,true),20);B.H(Row,Info,20,true);
        auto* Meta=B.Make<UVerticalBox>(TEXT("VerticalBox_Meta"));B.V(Meta,B.Text(TEXT("Text_Kind"),FText::GetEmpty(),16,false,true),6);
        B.V(Meta,B.Text(TEXT("Text_SavedAt"),FText::GetEmpty(),14,true,true));B.H(Row,Meta);
        auto* S=Root->AddChildToOverlay(B.Panel(TEXT("Border_Card"),Row,22,22));S->SetHorizontalAlignment(HAlign_Fill);S->SetPadding(FMargin(0,0,0,12));
        auto* Ring=B.Make<UImage>(TEXT("Image_FocusRing"),true);Ring->SetBrush(FFFrontendCasual::Rounded(FLinearColor::Transparent,B.T.Teal,22,3));Ring->SetVisibility(ESlateVisibility::Hidden);
        auto* R=Root->AddChildToOverlay(Ring);R->SetHorizontalAlignment(HAlign_Fill);R->SetVerticalAlignment(VAlign_Fill);R->SetPadding(FMargin(0,0,0,12));return true;
    }
'''+s[end:]
s=s.replace('Palette->InGame.Ink, InkTextStyle','FFFrontendCasual::Get().Ink, InkTextStyle').replace('Palette->InGame.White, OnTealTextStyle','FFFrontendCasual::Get().White, OnTealTextStyle')
s=s.replace('NavStyle, Palette->InGame.Ink, false','NavStyle, FFFrontendCasual::Get().Ink, false').replace('PrimaryStyle, Palette->InGame.White, true','PrimaryStyle, FFFrontendCasual::Get().White, true')
s=s.replace('ButtonCDO->ApplyGeneratedButtonDefaults(', 'ButtonBlueprint->Modify();\n                ButtonCDO->Modify();\n                ButtonCDO->ApplyGeneratedButtonDefaults(')
s=s.replace('Palette->InGame.InkFaint,','FFFrontendCasual::Get().InkMuted,')
s=s.replace('if (!SaveAssetPackage(LogPrefix, *ButtonBlueprint))','if (FFloatProperty* Opacity=FindFProperty<FFloatProperty>(ButtonCDO->GetClass(),TEXT("DisabledOpacity")))\n                    Opacity->SetPropertyValue_InContainer(ButtonCDO,1.f);\n                if (!SaveAssetPackage(LogPrefix, *ButtonBlueprint))')
marker='\t\tUE_LOG(LogFFUIWbpBuilder, Display,\n\t\t\tTEXT("%s_DONE:'
i=s.index(marker)
s=s[:i]+'''		const FString CardPackage=FrontendFolder/TEXT("WBP_FF_SaveSlotCard");
        UWidgetBlueprint* Card=ReplaceWidgetBlueprint(LogPrefix,CardPackage,FrontendFolder,*UFF_SaveSlotCard::StaticClass(),
            [](UWidgetBlueprint& BP,UWidgetTree& Tree){return BuildSaveCardCasual(BP,Tree);});
        if(!Card || !Card->GeneratedClass)return;
        UWidgetBlueprint* Saves=ReplaceWidgetBlueprint(LogPrefix,FrontendFolder/TEXT("WBP_FF_SaveList"),FrontendFolder,*UFF_SaveListScreen::StaticClass(),
            [NavButtonClass](UWidgetBlueprint& BP,UWidgetTree& Tree){return BuildSaveListCasual(BP,Tree,NavButtonClass);});
        if(!Saves || !Saves->GeneratedClass)return;
        UObject* Defaults=Saves->GeneratedClass->GetDefaultObject();Defaults->Modify();
        if(auto* Property=FindFProperty<FClassProperty>(Defaults->GetClass(),TEXT("CardWidgetClass")))
            Property->SetObjectPropertyValue_InContainer(Defaults,Card->GeneratedClass);
        SaveAssetPackage(LogPrefix,*Saves);
'''+s[i:]
s=s.replace('셋업 스타일','캐주얼 스타일').replace('LightTeal setup-style','Clean Casual frontend')
p.write_text(s,encoding='utf-8')
