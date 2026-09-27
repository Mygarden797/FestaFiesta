// 타이틀과 프런트엔드에만 쓰는 캐주얼 색·도형. 인게임 팔레트는 변경하지 않는다.
// 실행 화면과 에디터 생성기가 같은 스타일을 쓰도록 공개 헤더에서 공유한다.
#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

namespace FFFrontendCasual
{
    inline FLinearColor Hex(const TCHAR* Value) { return FLinearColor::FromSRGBColor(FColor::FromHex(Value)); }
    struct FTokens
    {
        FLinearColor Paper = Hex(TEXT("FAF9F2"));
        FLinearColor PaperSunken = Hex(TEXT("EFF5ED"));
        FLinearColor Ink = Hex(TEXT("123F32"));
        FLinearColor InkMuted = Hex(TEXT("607D71"));
        FLinearColor InkFaint = Hex(TEXT("789084"));
        FLinearColor Line = Hex(TEXT("CEE5D5"));
        FLinearColor LineStrong = Hex(TEXT("9EC8AC"));
        FLinearColor Teal = Hex(TEXT("197B51"));
        FLinearColor TealDeep = Hex(TEXT("105639"));
        FLinearColor TealSoft = Hex(TEXT("DDF0DC"));
        FLinearColor Hover = Hex(TEXT("EBF6E8"));
        FLinearColor Disabled = Hex(TEXT("E7EDE5"));
        FLinearColor White = Hex(TEXT("FFFFFF"));
    };
    inline FTokens Get() { return FTokens(); }
    inline FSlateBrush Rounded(const FLinearColor& Fill, const FLinearColor& Line, float Radius = 20.f, float Width = 1.f)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = Fill;
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
        Brush.OutlineSettings.Color = Line;
        Brush.OutlineSettings.Width = Width;
        return Brush;
    }
}
