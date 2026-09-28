// FF_FacilityColorPicker.cpp
// 역할: 시설 조명 색을 고르는 색상 선택기를 그리며 마우스 입력을 색으로 변환한다.

#include "UI/Facility/FF_FacilityColorPicker.h"

#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

class SFFFacilityColorPicker : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SFFFacilityColorPicker) {} SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    FFF_OnLightingColorPicked OnColorPicked;
    FSimpleMulticastDelegate OnInteractionStarted;
    void SetColor(FLinearColor Linear)
    {
        const FColor RGB = Linear.ToFColorSRGB();
        const FLinearColor NewHSV = FLinearColor(RGB.R / 255.f, RGB.G / 255.f, RGB.B / 255.f).LinearRGBToHSV();
        if (NewHSV.G > 0.0001f) Hue = NewHSV.R;
        Saturation = NewHSV.G; Value = NewHSV.B;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(194,194); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        const float Size = FMath::Min(G.GetLocalSize().X, G.GetLocalSize().Y);
        const FVector2D Center = G.GetLocalSize() * .5f;
        const float Radius = Size * .45f;
        for (int32 I=0; I<120; ++I)
        {
            const float A = (I-.16f) * 2.f * PI / 120.f, B = (I+1.16f) * 2.f * PI / 120.f;
            TArray<FVector2D> Points{Center + FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius,
                Center + FVector2D(FMath::Cos(B),FMath::Sin(B))*Radius};
            FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,
                FLinearColor(I*3.f,1,1).HSVToLinearRGB(),false,Size*.08f);
        }
        const float Side = Size*.55f;
        const FVector2D Corner = Center - FVector2D(Side*.5f,Side*.5f);
        const FPaintGeometry Square = G.ToPaintGeometry(FVector2D(Side,Side),FSlateLayoutTransform(Corner));
        TArray<FSlateGradientStop> Horizontal{
            FSlateGradientStop(FVector2D(0,0),FLinearColor::White),
            FSlateGradientStop(FVector2D(Side,0),FLinearColor(Hue,1,1).HSVToLinearRGB())};
        // Slate names the orientation of each gradient STOP, rather than its direction.
        FSlateDrawElement::MakeGradient(Out,Layer+1,Square,Horizontal,Orient_Vertical);
        TArray<FSlateGradientStop> Vertical{
            FSlateGradientStop(FVector2D(0,0),FLinearColor(0,0,0,0)),
            FSlateGradientStop(FVector2D(0,Side),FLinearColor::Black)};
        FSlateDrawElement::MakeGradient(Out,Layer+2,Square,Vertical,Orient_Horizontal);
        auto Ring = [&](FVector2D Position)
        {
            TArray<FVector2D> Points;
            for(int32 I=0;I<=24;++I) { const float A=I*2.f*PI/24.f; Points.Add(Position+FVector2D(FMath::Cos(A),FMath::Sin(A))*5.f); }
            FSlateDrawElement::MakeLines(Out,Layer+3,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor::Black,true,4);
            FSlateDrawElement::MakeLines(Out,Layer+4,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor::White,true,2);
        };
        Ring(Corner + FVector2D(Saturation,1-Value)*Side);
        Ring(Center + FVector2D(FMath::Cos(FMath::DegreesToRadians(Hue)),FMath::Sin(FMath::DegreesToRadians(Hue)))*Radius);
        return Layer+4;
    }
    virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override
    {
        if(E.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Unhandled();
        const float Size=FMath::Min(G.GetLocalSize().X,G.GetLocalSize().Y);
        const FVector2D D=G.AbsoluteToLocal(E.GetScreenSpacePosition())-G.GetLocalSize()*.5f;
        if(FMath::Abs(D.X)<=Size*.275f && FMath::Abs(D.Y)<=Size*.275f) DragMode=2;
        else if(D.Size()>=Size*.36f && D.Size()<=Size*.5f) DragMode=1;
        else return FReply::Handled();
        OnInteractionStarted.Broadcast(); Update(G,E);
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }
    virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E) override
    { if(HasMouseCapture()){ Update(G,E); return FReply::Handled(); } return FReply::Unhandled(); }
    virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent& E) override
    { if(E.GetEffectingButton()==EKeys::LeftMouseButton && HasMouseCapture()){DragMode=0;return FReply::Handled().ReleaseMouseCapture();}return FReply::Unhandled(); }
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { DragMode=0; }
private:
    void Update(const FGeometry& G,const FPointerEvent& E)
    {
        const float Size=FMath::Min(G.GetLocalSize().X,G.GetLocalSize().Y);
        const FVector2D D=G.AbsoluteToLocal(E.GetScreenSpacePosition())-G.GetLocalSize()*.5f;
        if(DragMode==1) Hue=FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(D.Y,D.X))+360.f,360.f);
        if(DragMode==2) { Saturation=FMath::Clamp(D.X/(Size*.55f)+.5f,0.f,1.f); Value=FMath::Clamp(.5f-D.Y/(Size*.55f),0.f,1.f); }
        const FLinearColor RGB=FLinearColor(Hue,Saturation,Value).HSVToLinearRGB();
        const FColor SRGB(FMath::RoundToInt(RGB.R*255),FMath::RoundToInt(RGB.G*255),FMath::RoundToInt(RGB.B*255));
        OnColorPicked.Broadcast(FLinearColor::FromSRGBColor(SRGB));
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    float Hue=0, Saturation=0, Value=1;
    int32 DragMode=0;
};

UFF_FacilityColorPicker::UFF_FacilityColorPicker(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UFF_FacilityColorPicker::SetColor(FLinearColor Color)
{
    CurrentColor=Color;
    // UWidget already weakly caches the leaf returned by RebuildWidget. Keeping another
    // owning Slate pointer in a template UWidget is unnecessary during WBP duplication.
    const TSharedPtr<SWidget> Cached=GetCachedWidget();
    if(Cached && Cached->GetType()==FName(TEXT("SFFFacilityColorPicker")))
        StaticCastSharedPtr<SFFFacilityColorPicker>(Cached)->SetColor(Color);
}
TSharedRef<SWidget> UFF_FacilityColorPicker::RebuildWidget()
{
    checkf(GetClass()->GetPropertiesSize()>=sizeof(UFF_FacilityColorPicker),TEXT("Color picker native class layout is stale; perform a full editor rebuild."));
    TSharedRef<SFFFacilityColorPicker> Leaf=SNew(SFFFacilityColorPicker);
    Leaf->SetColor(CurrentColor);
    Leaf->OnColorPicked.AddWeakLambda(this,[this](FLinearColor Color){ CurrentColor=Color; OnColorPicked.Broadcast(Color); });
    Leaf->OnInteractionStarted.AddWeakLambda(this,[this](){ OnInteractionStarted.Broadcast(); });
    return Leaf;
}
