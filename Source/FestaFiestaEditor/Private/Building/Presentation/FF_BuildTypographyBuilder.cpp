// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildTypographyBuilder.cpp
// 역할: 저장된 건설 관련 화면의 글꼴만 공통 규칙으로 맞추고 결과를 검사한다.

// Explicit font-only migration: authored layout, materials, bindings and actions survive.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildTypography.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableText.h"
#include "Components/RichTextBlock.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFBuildTypographyBuilder
{
using namespace FFUIWbpBuilder;
using namespace FFBuildTypography;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_TYPOGRAPHY");
constexpr const TCHAR* Version = TEXT("FF.BuildMode.Typography20260921.1");
// Includes the current construction flow, its supported catalog fallback, and
// shared HUD / inspector surfaces reachable while constructing. Other prep
// panels and the separately redesigned codex screen/cards are intentionally absent.
const TCHAR* Paths[] = {
    FFBuildUIAssetPaths::BuildPanelPackage,
    FFBuildUIAssetPaths::BuildTilePackage,
    FFBuildUIAssetPaths::CategoryChoicePackage,
    FFBuildUIAssetPaths::BuildToolsPackage,
    FFBuildUIAssetPaths::BuildSynergyPackage,
    FFBuildUIAssetPaths::BuildModeGuidePackage,
    FFBuildUIAssetPaths::FacilityCostTooltipPackage,
    FFBuildUIAssetPaths::PlacementFailureFeedbackPackage,
    FFBuildUIAssetPaths::CatalogScreenPackage,
    FFBuildUIAssetPaths::FacilityCardPackage,
    FFBuildUIAssetPaths::CategoryButtonPackage,
    FFBuildUIAssetPaths::MinorSectionPackage,
    FFBuildUIAssetPaths::TopButtonPackage,
    FFBuildUIAssetPaths::FacilityInfoPanelPackage,
    FFBuildUIAssetPaths::FacilityLightingControlsPackage,
    FFBuildUIAssetPaths::FacilitySynergyEntryPackage,
    FFBuildUIAssetPaths::FacilityPaidProgramCardPackage,
    FFBuildUIAssetPaths::FacilityPaidProgramPickerPackage,
    FFBuildUIAssetPaths::FacilityWorldBadgePackage,
    FFBuildUIAssetPaths::FacilityRevenueRankingPackage,
    FFBuildUIAssetPaths::FacilityRevenueRankRowPackage,
    FFBuildUIAssetPaths::PrepHudBarPackage,
    FFBuildUIAssetPaths::PrepHudTabButtonPackage,
    FFBuildUIAssetPaths::PrepChoiceButtonPackage,
    FFBuildUIAssetPaths::FacilityCodexLauncherPackage,
    FFBuildUIAssetPaths::FacilityCodexCatalogLauncherPackage
};

static bool Contains(const FString& Value, std::initializer_list<const TCHAR*> Tokens)
{
    for (const TCHAR* Token : Tokens) if (Value.Contains(Token)) return true;
    return false;
}

static bool IsSymbol(const UWidget& Widget)
{
    const FString Name = Widget.GetName();
    if (Contains(Name, {TEXT("Icon"), TEXT("Glyph"), TEXT("Symbol")})) return true;
    if (Name == TEXT("Text_Favorite") || Name == TEXT("Text_Plus") || Name == TEXT("Text_Check")) return true;
    const UTextBlock* Label = Cast<UTextBlock>(&Widget);
    const FString Copy = Label ? Label->GetText().ToString().TrimStartAndEnd() : FString();
    // Do not treat an empty text or a data placeholder (such as an em dash) as
    // an icon: it can be replaced by Korean text by a runtime presenter.
    for (const TCHAR* Glyph : {TEXT("✓"), TEXT("✔"), TEXT("★"), TEXT("☆"), TEXT("→"), TEXT("←"),
        TEXT("‹"), TEXT("›"), TEXT("×"), TEXT("+"), TEXT("−"), TEXT("●"), TEXT("○")})
        if (Copy == Glyph) return true;
    return false;
}

static ERole Role(const UWidget& Widget, float Size)
{
    const FString Name = Widget.GetName();
    if ((Contains(Name, {TEXT("Title"), TEXT("Heading")}) && Size >= 16) ||
        ((Name == TEXT("Text_SelectedFacility") || Name == TEXT("Text_FacilityName")) && Size >= 20))
        return ERole::Heading;
    if (Contains(Name, {TEXT("Cost"), TEXT("Gold"), TEXT("Balance"), TEXT("Amount"), TEXT("Value"),
        TEXT("Revenue"), TEXT("Count"), TEXT("Price"), TEXT("PageIndex")})) return ERole::Number;
    if (Contains(Name, {TEXT("Hint"), TEXT("Legend"), TEXT("Description"), TEXT("Detail"), TEXT("Help"),
        TEXT("Instruction"), TEXT("Reason"), TEXT("Notice"), TEXT("Caption"), TEXT("Subtitle")})) return ERole::Body;
    return ERole::Label;
}

static const TCHAR* RoleName(ERole Value)
{
    switch (Value) { case ERole::Heading: return TEXT("heading"); case ERole::Body: return TEXT("body");
        case ERole::Number: return TEXT("number"); default: return TEXT("label"); }
}

static bool ReadFont(UWidget& Widget, FSlateFontInfo& Result)
{
    if (auto* Text = Cast<UTextBlock>(&Widget)) Result = Text->GetFont();
    else if (auto* Input = Cast<UEditableTextBox>(&Widget)) Result = Input->GetWidgetStyle().TextStyle.Font;
    else if (auto* Editable = Cast<UEditableText>(&Widget)) Result = Editable->GetFont();
    else if (auto* Multiline = Cast<UMultiLineEditableText>(&Widget)) Result = Multiline->GetFont();
    else if (auto* Rich = Cast<URichTextBlock>(&Widget)) Result = Rich->GetDefaultTextStyle().Font;
    else if (auto* Spin = Cast<USpinBox>(&Widget)) Result = Spin->GetFont();
    else return false;
    return true;
}

static void ApplyFont(UWidget& Widget, const FSlateFontInfo& FontInfo)
{
    if (auto* Text = Cast<UTextBlock>(&Widget))
    {
        // CommonTextBlock re-applies its style on every SynchronizeProperties.
        // Freeze its existing appearance before setting an authored local font.
        if (auto* Common = Cast<UCommonTextBlock>(Text)) Common->SetStyle(nullptr);
        Text->SetFont(FontInfo);
        Text->SetLineHeightPercentage(1.f);
    }
    else if (auto* Input = Cast<UEditableTextBox>(&Widget))
    {
        auto Style = Input->GetWidgetStyle(); Style.TextStyle.SetFont(FontInfo); Input->SetWidgetStyle(Style);
    }
    else if (auto* Editable = Cast<UEditableText>(&Widget)) Editable->SetFont(FontInfo);
    else if (auto* Multiline = Cast<UMultiLineEditableText>(&Widget)) Multiline->SetFont(FontInfo);
    else if (auto* Rich = Cast<URichTextBlock>(&Widget)) Rich->SetDefaultFont(FontInfo);
    else if (auto* Spin = Cast<USpinBox>(&Widget)) Spin->SetFont(FontInfo);
}

static FString Quoted(const FString& Value)
{
    FString Result = TEXT("\"");
    for (TCHAR C : Value)
    {
        if (C == '\\' || C == '"') { Result += '\\'; Result += C; }
        else if (C < 0x20) Result += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(C));
        else Result += C;
    }
    return Result + TEXT("\"");
}

static bool RunAudit(const TArray<UWidgetBlueprint*>& Blueprints)
{
    int32 TextCount = 0, FontCount = 0, SymbolCount = 0;
    bool bValid = true;
    FString Rows;
    FString FontRows;
    for (ERole Value : {ERole::Heading, ERole::Body, ERole::Label, ERole::Number})
    {
        UFont* Resource = Font(Value);
        FString Faces;
        for (const FTypefaceEntry& Entry : Resource->GetCompositeFont()->DefaultTypeface.Fonts)
        {
            if (!Faces.IsEmpty()) Faces += TEXT(",");
            Faces += Quoted(Entry.Name.ToString());
        }
        if (!FontRows.IsEmpty()) FontRows += TEXT(",\n");
        FontRows += FString::Printf(TEXT("    {\"role\":%s,\"asset\":%s,\"selected_typeface\":%s,\"available_typefaces\":[%s]}"),
            *Quoted(RoleName(Value)), *Quoted(Resource->GetPathName()), *Quoted(Typeface(*Resource).ToString()), *Faces);
    }
    for (UWidgetBlueprint* BP : Blueprints)
    {
        TArray<UWidget*> Widgets;
        BP->WidgetTree->GetAllWidgets(Widgets);
        Widgets.Sort([](const UWidget& A, const UWidget& B) { return A.GetName() < B.GetName(); });
        int32 LocalCount = 0;
        for (UWidget* Widget : Widgets)
        {
            FSlateFontInfo Info;
            if (!ReadFont(*Widget, Info)) continue;
            ++TextCount; ++LocalCount;
            const bool bSymbol = IsSymbol(*Widget);
            const ERole ExpectedRole = Role(*Widget, Info.Size);
            const bool bCorrect = bSymbol || (Info.FontObject == Font(ExpectedRole) &&
                Info.TypefaceFontName == Typeface(*Font(ExpectedRole)));
            bValid &= bCorrect;
            if (bSymbol) ++SymbolCount; else ++FontCount;
            if (!Rows.IsEmpty()) Rows += TEXT(",\n");
            Rows += FString::Printf(TEXT("    {\"asset\":%s,\"widget\":%s,\"class\":%s,\"role\":%s,\"font\":%s,\"typeface\":%s,\"size\":%g,\"valid\":%s}"),
                *Quoted(BP->GetOutermost()->GetName()), *Quoted(Widget->GetName()), *Quoted(Widget->GetClass()->GetName()),
                *Quoted(bSymbol ? TEXT("preserved_symbol") : RoleName(ExpectedRole)), *Quoted(GetPathNameSafe(Info.FontObject)),
                *Quoted(Info.TypefaceFontName.ToString()), Info.Size, bCorrect ? TEXT("true") : TEXT("false"));
        }
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_ASSET %s text_widgets=%d"), Prefix, *BP->GetOutermost()->GetName(), LocalCount);
    }
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/BuildTypography");
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Json = FString::Printf(TEXT("{\n  \"status\":\"%s\",\n  \"assets\":%d,\n  \"text_widgets\":%d,\n  \"font_updated\":%d,\n  \"symbols_preserved\":%d,\n  \"fonts\":[\n%s\n  ],\n  \"widgets\":[\n%s\n  ]\n}\n"),
        bValid ? TEXT("PASS") : TEXT("FAIL"), Blueprints.Num(), TextCount, FontCount, SymbolCount, *FontRows, *Rows);
    const bool bWritten = FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("typography-audit.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_AUDIT_%s assets=%d text_widgets=%d fonts=%d symbols=%d"),
        Prefix, bValid && bWritten ? TEXT("PASS") : TEXT("FAIL"), Blueprints.Num(), TextCount, FontCount, SymbolCount);
    return bValid && bWritten;
}

static bool LoadInputs(TArray<UWidgetBlueprint*>& Result)
{
    for (ERole Value : {ERole::Heading, ERole::Body, ERole::Label, ERole::Number})
        if (!Font(Value) || Typeface(*Font(Value)).IsNone())
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL missing font/typeface: %s"), Prefix, Path(Value));
            return false;
        }
    for (const TCHAR* Package : Paths)
    {
        UWidgetBlueprint* BP = LoadExistingBlueprint(Package);
        if (!BP || !BP->WidgetTree)
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL missing widget tree: %s"), Prefix, Package);
            return false;
        }
        Result.Add(BP);
    }
    return true;
}

static void Audit()
{
    TArray<UWidgetBlueprint*> Blueprints;
    if (LoadInputs(Blueprints)) RunAudit(Blueprints);
}

static void Run()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL requires idle Entry editor."), Prefix);
        return;
    }
    TArray<UWidgetBlueprint*> Blueprints;
    if (!LoadInputs(Blueprints)) return;
    for (UWidgetBlueprint* BP : Blueprints)
    {
        auto& Metadata = BP->GetOutermost()->GetMetaData();
        if (Metadata.GetValue(BP, Version) == TEXT("1")) continue;
        BP->Modify(); BP->WidgetTree->Modify();
        TArray<UWidget*> Widgets;
        BP->WidgetTree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets)
        {
            FSlateFontInfo Info;
            if (!ReadFont(*Widget, Info) || IsSymbol(*Widget)) continue;
            const ERole FontRole = Role(*Widget, Info.Size);
            Info.FontObject = Font(FontRole);
            Info.TypefaceFontName = Typeface(*Font(FontRole));
            // Keep established wrapping / fixed-height contracts. Only tiny
            // general labels receive a readability floor; world badges stay compact.
            const bool bWorldBadge = BP->GetName() == TEXT("WBP_FF_FacilityWorldBadge");
            Info.Size = FMath::Max(Info.Size, bWorldBadge ? 10.f : 12.f);
            Info.LetterSpacing = 0;
            Widget->Modify();
            ApplyFont(*Widget, Info);
        }
        Metadata.SetValue(BP, Version, TEXT("1"));
        if (!SaveNewBlueprint(Prefix, *BP))
        {
            Metadata.RemoveValue(BP, Version);
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL saving %s"), Prefix, *BP->GetName());
            return;
        }
    }
    const bool bValid = RunAudit(Blueprints);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix, bValid ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand ApplyCommand(TEXT("FF.ApplyBuildModeTypography"),
    TEXT("Apply local NEXON typography to the exact construction UI allowlist once; preserve icon fonts and layout."),
    FConsoleCommandDelegate::CreateStatic(&Run));
static FAutoConsoleCommand AuditCommand(TEXT("FF.AuditBuildModeTypography"),
    TEXT("Audit every construction text widget and write Saved/Automation/BuildTypography/typography-audit.json."),
    FConsoleCommandDelegate::CreateStatic(&Audit));
}
#endif
