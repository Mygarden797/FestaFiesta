// 에디터 전용 모듈: 제작 도구와 그 검증 코드를 게임 실행 코드에서 분리한다.
// 프런트엔드 재생성 도구의 공통 조립 함수. 에디터에서 각 위젯을 그대로 편집할 수 있다.
#pragma once
#if WITH_EDITOR
#include "UI/Frontend/FF_FrontendCasualStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FF_CommonButtonBase.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace FFFrontendCasual
{
    inline FString ArtPackage(const FString& Name) { return TEXT("/Game/FestaFiesta/UI/Frontend/Art/T_FF_") + Name; }
    inline FSlateBrush Art(const FString& Name, FVector2D Size = FVector2D(32,32))
    {
        FSlateBrush B;
        B.SetResourceObject(LoadObject<UTexture2D>(nullptr, *FFUIWbpBuilder::MakeObjectPath(ArtPackage(Name))));
        B.ImageSize = Size;
        return B;
    }
    struct FBuilder
    {
        UWidgetBlueprint& BP;
        UWidgetTree& Tree;
        FTokens T;
        template<class W> W* Make(const FName Name, bool Bound = false) { return FFUIWbpBuilder::MakeWidget<W>(BP,Tree,Name,Bound); }
        UTextBlock* Text(const FName Name, const FText& Value, int Size=18, bool Muted=false, bool Bound=false)
        {
            auto* W=Make<UTextBlock>(Name,Bound);
            FFUIWbpBuilder::StyleText(*W,Size,Muted?T.InkMuted:T.Ink,Size>=20?FName(TEXT("SemiBold")):FName(TEXT("Medium")));
            W->SetText(Value); W->SetAutoWrapText(true); W->SetVisibility(ESlateVisibility::HitTestInvisible); return W;
        }
        UBorder* Panel(const FName Name, UWidget* Child, float Padding=20, float Radius=22, bool Sunken=false, bool Bound=false)
        {
            auto* W=Make<UBorder>(Name,Bound); W->SetBrush(Rounded(Sunken?T.PaperSunken:T.Paper,T.Line,Radius));
            W->SetPadding(FMargin(Padding)); W->SetHorizontalAlignment(HAlign_Fill); W->SetVerticalAlignment(VAlign_Fill);
            W->AddChild(Child); return W;
        }
        void V(UVerticalBox* Parent,UWidget* Child,float Bottom=0,bool Fill=false)
        {
            auto* S=Parent->AddChildToVerticalBox(Child); S->SetSize(FSlateChildSize(Fill?ESlateSizeRule::Fill:ESlateSizeRule::Automatic));
            S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0,0,0,Bottom));
        }
        void H(UHorizontalBox* Parent,UWidget* Child,float Right=0,bool Fill=false)
        {
            auto* S=Parent->AddChildToHorizontalBox(Child); S->SetSize(FSlateChildSize(Fill?ESlateSizeRule::Fill:ESlateSizeRule::Automatic));
            S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0,0,Right,0));
        }
        USizeBox* Sized(FName Name,UWidget* Child,float Width,float Height)
        {
            auto* W=Make<USizeBox>(Name); if(Width>0)W->SetWidthOverride(Width); if(Height>0)W->SetHeightOverride(Height);
            W->AddChild(Child);return W;
        }
        UImage* Icon(FName Name,const FString& Asset,float Size=32,bool Bound=false)
        {
            auto* W=Make<UImage>(Name,Bound);W->SetBrush(Art(Asset,FVector2D(Size,Size)));
            W->SetVisibility(ESlateVisibility::HitTestInvisible);return W;
        }
        UFF_CommonButtonBase* Button(FName Name,UClass* Class,const FText& Label,float Width,float Height=60,const FString& IconName=TEXT(""))
        {
            auto* W=CastChecked<UFF_CommonButtonBase>(FFUIWbpBuilder::MakeWidgetOfClass(BP,Tree,Name,Class));
            W->ApplyGeneratedButtonContent(Label,IconName.IsEmpty()?FSlateBrush():Art(IconName),FVector2D(Width,Height));
            // 이미 로드된 부모 WBP의 오래된 템플릿 값이 자식 인스턴스에 남지 않게 현재 스타일을 명시한다.
            const auto* CDO=Class->GetDefaultObject<UFF_CommonButtonBase>();
            if(CDO->GetStyle()) W->SetStyle(CDO->GetStyle()->GetClass());
            return W;
        }
        void SidePanel(UCanvasPanel* Canvas,UWidget* Child,float Width=620,float X=64)
        {
            auto* Shadow=Make<UBorder>(TEXT("Border_LeftPanelShadow"));
            Shadow->SetBrush(Rounded(Hex(TEXT("B3CCC0")),FLinearColor::Transparent,32));
            Shadow->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* SS=Canvas->AddChildToCanvas(Shadow);SS->SetAnchors(FAnchors(0,0.05f,0,0.95f));
            SS->SetOffsets(FMargin(X+3,6,Width, -6));SS->SetZOrder(4);
            auto* P=Panel(TEXT("Border_LeftPanel"),Child,30,32);
            auto* S=Canvas->AddChildToCanvas(P);S->SetAnchors(FAnchors(0,0.05f,0,0.95f));
            S->SetOffsets(FMargin(X,0,Width,0));S->SetZOrder(5);
        }
        void Heading(UVerticalBox* Parent,const FText& Title,const FText& Subtitle,int Size=34)
        {
            auto* R=Make<UHorizontalBox>(TEXT("HorizontalBox_Heading"));
            auto* C=Make<UVerticalBox>(TEXT("VerticalBox_Heading"));
            V(C,Text(TEXT("Text_ScreenTitle"),Title,Size),8);
            V(C,Text(TEXT("Text_Subtitle"),Subtitle,16,true));
            H(R,C,12,true);
            auto* Logo=Make<UImage>(TEXT("Image_BrandMark"));
            Logo->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/FestaFiesta/UI/Title/CleanCasual/Brand/T_FF_Title_CasualBright_Logo_512.T_FF_Title_CasualBright_Logo_512")));
            Logo->SetOpacity(0.6f);Logo->SetVisibility(ESlateVisibility::HitTestInvisible);
            H(R,Sized(TEXT("SizeBox_BrandMark"),Logo,76,76));V(Parent,R,26);
        }
    };
}
#endif
