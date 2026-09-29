// 게임 화면과 에디터 제작 도구가 함께 사용하는 공통 규칙이다.
// FF_BuildTypography.h
// 역할: 제목·본문·라벨·숫자의 공통 글꼴을 선택하고 로드한 글꼴을 유지한다.

// Fonts shared by construction's authored widgets and its native fallback UI.
#pragma once

#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/StrongObjectPtr.h"

namespace FFBuildTypography
{
enum class ERole : uint8 { Heading, Body, Label, Number };

inline const TCHAR* Path(ERole Role)
{
    switch (Role)
    {
    case ERole::Heading: return TEXT("/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Bold_Font.NEXON_Lv2_Gothic_OTF_Bold_Font");
    case ERole::Label: return TEXT("/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Medium_Font.NEXON_Lv2_Gothic_OTF_Medium_Font");
    case ERole::Number: return TEXT("/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Bold_Font.NEXON_Lv2_Gothic_OTF_Bold_Font");
    default: return TEXT("/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Font.NEXON_Lv2_Gothic_OTF_Font");
    }
}

inline FName Typeface(const UFont& Font)
{
    const FCompositeFont* Composite = Font.GetCompositeFont();
    return Composite && !Composite->DefaultTypeface.Fonts.IsEmpty()
        ? Composite->DefaultTypeface.Fonts[0].Name : NAME_None;
}

inline UFont* Font(ERole Role)
{
    // Slate fallback text does not own a UObject property. Keep its fonts alive,
    // including when a catalog is rebuilt after a garbage collection.
    static TStrongObjectPtr<UFont> Fonts[4];
    const int32 Index = static_cast<int32>(Role);
    if (!Fonts[Index].IsValid()) Fonts[Index].Reset(LoadObject<UFont>(nullptr, Path(Role)));
    return Fonts[Index].Get();
}

inline FSlateFontInfo Make(ERole Role, int32 Size)
{
    UFont* Resource = Font(Role);
    return FSlateFontInfo(Resource, Size, Resource ? Typeface(*Resource) : NAME_None);
}
}
