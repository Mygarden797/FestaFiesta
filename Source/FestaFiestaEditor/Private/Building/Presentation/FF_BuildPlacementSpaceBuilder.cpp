// Keep placement dimensions and the queue's reserved space visible in the saved HUD.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildTypography.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFBuildPlacementSpace"

namespace FFBuildPlacementSpace
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_PLACEMENT_SPACE");
constexpr const TCHAR* Version = TEXT("FF.BuildPlacementSpace.20261007.v1");

template<class T> T* Find(UWidgetTree& Tree, const TCHAR* Name)
{ return Cast<T>(Tree.FindWidget(Name)); }

void Style(UTextBlock& Text, int32 Size, const FLinearColor& Color, bool bWrap)
{
    // Keep the existing typography role and font resource on every bound label.
    FSlateFontInfo Font = Text.GetFont();
    Font.Size = Size;
    Text.SetFont(Font);
    Text.SetColorAndOpacity(FSlateColor(Color));
    Text.SetShadowOffset(FVector2D::ZeroVector);
    Text.SetAutoWrapText(bWrap);
    Text.SetWrapTextAt(0.f);
    Text.SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    Text.SetTextOverflowPolicy(bWrap ? ETextOverflowPolicy::Clip : ETextOverflowPolicy::Ellipsis);
    Text.SetLineHeightPercentage(1.f);
    Text.SetVisibility(ESlateVisibility::HitTestInvisible);
    Text.bHiddenInDesigner = false;
}

UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
    const FText& Caption, int32 Size, const FLinearColor& Color,
    FFBuildTypography::ERole Role = FFBuildTypography::ERole::Label)
{
    auto* Text = MakeWidget<UTextBlock>(BP, Tree, Name);
    Text->SetFont(FFBuildTypography::Make(Role, Size));
    Text->SetText(Caption);
    Style(*Text, Size, Color, false);
    return Text;
}

void Add(UVerticalBox& Column, UWidget& Child, const FMargin& Padding)
{
    auto* Slot = Column.AddChildToVerticalBox(&Child);
    Slot->SetPadding(Padding);
}

void Legend(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Row,
    const TCHAR* Suffix, const FText& Caption, const FText& Hint,
    const FLinearColor& Fill, const FLinearColor& Ink, bool bLast = false)
{
    auto* Border = MakeWidget<UBorder>(BP, Tree,
        FName(*(FString(TEXT("Border_PlacementSpaceLegend")) + Suffix)));
    Border->SetBrush(FFBuildPearl::Rounded(Fill, 5.f, Ink.CopyWithNewOpacity(.45f), 1.f));
    Border->SetBrushColor(FLinearColor::White);
    Border->SetPadding(FMargin(5.f, 3.f));
    Border->SetHorizontalAlignment(HAlign_Center);
    Border->SetVisibility(ESlateVisibility::Visible);
    Border->SetToolTipText(Hint);
    Border->AddChild(Label(BP, Tree,
        FName(*(FString(TEXT("Text_PlacementSpaceLegend")) + Suffix)), Caption, 12, Ink,
        FFBuildTypography::ERole::Body));
    auto* Slot = Row.AddChildToHorizontalBox(Border);
    Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Slot->SetPadding(FMargin(0.f, 0.f, bLast ? 0.f : 4.f, 0.f));
}

bool Verify(UWidgetBlueprint& BP)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Summary = Find<UVerticalBox>(Tree, TEXT("Column_ClaritySummary"));
    auto* Facts = Find<UVerticalBox>(Tree, TEXT("Panel_PlacementFacts"));
    auto* Space = Find<UBorder>(Tree, TEXT("Border_PlacementSpace"));
    auto* Column = Find<UVerticalBox>(Tree, TEXT("Column_PlacementSpace"));
    auto* Title = Find<UTextBlock>(Tree, TEXT("Text_PlacementSpaceTitle"));
    auto* Footprint = Find<UTextBlock>(Tree, TEXT("Text_Footprint"));
    auto* Queue = Find<UTextBlock>(Tree, TEXT("Text_Queue"));
    auto* Range = Find<UTextBlock>(Tree, TEXT("Text_RangeLegend"));
    auto* Balance = Find<UTextBlock>(Tree, TEXT("Text_Balance"));
    auto* LegendRow = Find<UHorizontalBox>(Tree, TEXT("Row_PlacementSpaceLegend"));
    if (!Summary || !Facts || !Space || !Column || !Title || !Footprint || !Queue ||
        !Range || !Balance || !LegendRow) return false;
    if (Space->GetParent() != Summary || Space->GetContent() != Column ||
        Space->GetVisibility() != ESlateVisibility::SelfHitTestInvisible || Space->bHiddenInDesigner ||
        Facts->GetParent() != Summary || Facts->GetChildrenCount() != 1 ||
        Facts->GetChildAt(0) != Balance || Facts->GetVisibility() != ESlateVisibility::Collapsed ||
        Column->GetChildrenCount() != 5 || Column->GetChildAt(0) != Title ||
        Column->GetChildAt(1) != Footprint || Column->GetChildAt(2) != Queue ||
        Column->GetChildAt(3) != LegendRow || Column->GetChildAt(4) != Range ||
        Title->GetText().ToString() != TEXT("필요한 공간"))
        return false;
    int32 VisibleLegends = 0;
    for (UWidget* Child : LegendRow->GetAllChildren())
        if (Child->GetVisibility() != ESlateVisibility::Collapsed) ++VisibleLegends;
    if (VisibleLegends != 3) return false;
    for (const TCHAR* Name : {TEXT("Border_PlacementSpaceLegendClearance"), TEXT("Text_PlacementSpaceLegendClearance")})
        if (UWidget* Legacy = Tree.FindWidget(Name))
            if (Legacy->GetVisibility() != ESlateVisibility::Collapsed) return false;
    for (auto* Text : {Footprint, Queue, Range})
        if (Text->GetVisibility() != ESlateVisibility::HitTestInvisible || Text->bHiddenInDesigner)
            return false;
    for (const TCHAR* Suffix : {TEXT("Facility"), TEXT("Queue"), TEXT("Blocked")})
        if (!Tree.FindWidget(FName(*(FString(TEXT("Text_PlacementSpaceLegend")) + Suffix)))) return false;
    return Summary->GetChildIndex(Space) == Summary->GetChildIndex(Tree.FindWidget(TEXT("Text_Legend"))) + 1;
}

bool VerifyAudience(UWidgetBlueprint& BP)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Column = Find<UVerticalBox>(Tree, TEXT("Column_PlacementSpace"));
    auto* Audience = Find<UTextBlock>(Tree, TEXT("Text_Audience"));
    auto* FirstRow = Find<UHorizontalBox>(Tree, TEXT("Row_PlacementSpaceLegend"));
    auto* SecondRow = Find<UHorizontalBox>(Tree, TEXT("Row_PlacementBoundaryLegend"));
    if (!Column || !Audience || !FirstRow || !SecondRow || Column->GetChildrenCount() != 7 ||
        Audience->GetParent() != Column || Column->GetChildAt(3) != Audience ||
        Column->GetChildAt(4) != FirstRow || Column->GetChildAt(5) != SecondRow ||
        Audience->GetFont().Size != 16 || Audience->GetVisibility() != ESlateVisibility::HitTestInvisible)
        return false;
    for (const TCHAR* Suffix : {TEXT("Facility"), TEXT("Queue"), TEXT("Audience"), TEXT("Boundary"), TEXT("Blocked")})
        if (!Tree.FindWidget(FName(*(FString(TEXT("Text_PlacementSpaceLegend")) + Suffix)))) return false;
    auto* AudienceLegend = Find<UBorder>(Tree, TEXT("Border_PlacementSpaceLegendAudience"));
    auto* BoundaryLegend = Find<UBorder>(Tree, TEXT("Border_PlacementSpaceLegendBoundary"));
    auto* BlockedLegend = Find<UBorder>(Tree, TEXT("Border_PlacementSpaceLegendBlocked"));
    return AudienceLegend && BoundaryLegend && BlockedLegend && AudienceLegend->GetParent() == FirstRow &&
        BoundaryLegend->GetParent() == SecondRow && BlockedLegend->GetParent() == SecondRow;
}

bool AddAudienceSummary(UWidgetBlueprint& BP)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Column = Find<UVerticalBox>(Tree, TEXT("Column_PlacementSpace"));
    auto* FirstRow = Find<UHorizontalBox>(Tree, TEXT("Row_PlacementSpaceLegend"));
    auto* Blocked = Find<UBorder>(Tree, TEXT("Border_PlacementSpaceLegendBlocked"));
    if (!Column || !FirstRow || !Blocked || Tree.FindWidget(TEXT("Text_Audience")) ||
        Tree.FindWidget(TEXT("Row_PlacementBoundaryLegend"))) return false;
    BP.Modify(); Tree.Modify(); Column->Modify(); FirstRow->Modify(); Blocked->Modify();
    auto* Audience = Label(BP, Tree, TEXT("Text_Audience"),
        LOCTEXT("AudiencePreview", "관객석 범위  28 × 36.5 m · 정원 200명"), 16,
        FFBuildPearl::Color(106, 58, 154), FFBuildTypography::ERole::Body);
    Audience->SetAutoWrapText(true);
    Audience->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
    Audience->SetToolTipText(LOCTEXT("AudienceBoundsHint", "가장자리 통로를 포함한 관객석 전체의 범위입니다. 시설 본체와 별도로 흰색 건축 경계 안에 들어가야 합니다."));
    Add(*Column, *Audience, FMargin(0.f, 0.f, 0.f, 7.f));
    Column->ShiftChild(3, Audience);
    Blocked->RemoveFromParent();
    Legend(BP, Tree, *FirstRow, TEXT("Audience"), LOCTEXT("Audience", "관객석"),
        LOCTEXT("AudienceHint", "보라색 관객석 경계: 관객석과 가장자리 통로가 차지하는 공간"),
        FFBuildPearl::Color(237, 220, 255), FFBuildPearl::Color(106, 58, 154), true);
    auto* SecondRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_PlacementBoundaryLegend"));
    Legend(BP, Tree, *SecondRow, TEXT("Boundary"), LOCTEXT("Boundary", "건축 경계"),
        LOCTEXT("BoundaryHint", "흰색 경계: 모든 시설 영역이 들어가야 하는 건축 가능 범위. 구역 사이 빈틈은 제외됩니다."),
        FFBuildPearl::Color(255, 255, 255), FFBuildPearl::Color(77, 95, 115));
    auto* BlockedSlot = SecondRow->AddChildToHorizontalBox(Blocked);
    BlockedSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    BlockedSlot->SetPadding(FMargin(0.f));
    Blocked->SetToolTipText(LOCTEXT("BoundaryBlockedHint", "붉은 사선: 건축 경계를 넘거나 다른 대상과 겹치는 실제 문제 부분입니다."));
    Add(*Column, *SecondRow, FMargin(0.f, 0.f, 0.f, 5.f));
    Column->ShiftChild(5, SecondRow);
    if (auto* Range = Find<UTextBlock>(Tree, TEXT("Text_RangeLegend")))
    {
        Range->Modify();
        Range->SetText(LOCTEXT("SeparateRangeLegend", "각 범위는 회전 전 가로 × 세로 · 단순 합산 아님"));
    }
    return VerifyAudience(BP);
}

bool Migrate(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    bool bRemovedLegacyClearance = false;
    for (const TCHAR* Name : {TEXT("Border_PlacementSpaceLegendClearance"), TEXT("Text_PlacementSpaceLegendClearance")})
        if (UWidget* Legacy = BP.WidgetTree->FindWidget(Name))
            if (Legacy->GetVisibility() != ESlateVisibility::Collapsed || !Legacy->bHiddenInDesigner)
            {
                BP.Modify(); BP.WidgetTree->Modify(); Legacy->Modify();
                Legacy->SetVisibility(ESlateVisibility::Collapsed);
                Legacy->bHiddenInDesigner = true;
                bRemovedLegacyClearance = true;
            }
    if (auto* Footprint = Find<UTextBlock>(*BP.WidgetTree, TEXT("Text_Footprint")))
    {
        const FString Caption = Footprint->GetText().ToString();
        int32 SafetyLine = INDEX_NONE;
        if (Caption.FindChar(TEXT('\n'), SafetyLine) && Caption.Mid(SafetyLine).Contains(TEXT("안전 여유")))
        {
            BP.Modify(); BP.WidgetTree->Modify(); Footprint->Modify();
            Footprint->SetText(FText::FromString(Caption.Left(SafetyLine)));
            bRemovedLegacyClearance = true;
        }
    }
    if (Metadata.GetValue(&BP, Version) == TEXT("2"))
    {
        if (!VerifyAudience(BP)) return false;
        if (bRemovedLegacyClearance) return SaveNewBlueprint(Prefix, BP);
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: audience and boundary summary is current; no save"), Prefix);
        return true;
    }
    if (Metadata.GetValue(&BP, Version) == TEXT("1"))
    {
        if (!Verify(BP)) return false;
        // Refine an already migrated asset without rebuilding its authored tree.
        // Preserve all copy, colors and bindings; only these three fonts change.
        auto* Title = Find<UTextBlock>(*BP.WidgetTree, TEXT("Text_PlacementSpaceTitle"));
        auto* Footprint = Find<UTextBlock>(*BP.WidgetTree, TEXT("Text_Footprint"));
        auto* Queue = Find<UTextBlock>(*BP.WidgetTree, TEXT("Text_Queue"));
        const FSlateFontInfo TitleFont = FFBuildTypography::Make(FFBuildTypography::ERole::Heading, 16);
        if (bRemovedLegacyClearance || Title->GetFont().Size != 16 || Footprint->GetFont().Size != 16 || Queue->GetFont().Size != 16 ||
            Title->GetFont().FontObject != TitleFont.FontObject || Title->GetFont().TypefaceFontName != TitleFont.TypefaceFontName)
        {
            BP.Modify(); BP.WidgetTree->Modify();
            Title->Modify(); Title->SetFont(TitleFont);
            for (auto* Text : {Footprint, Queue})
            {
                Text->Modify();
                FSlateFontInfo Font = Text->GetFont();
                Font.Size = 16;
                Text->SetFont(Font);
            }
            if (!AddAudienceSummary(BP)) return false;
            Metadata.SetValue(&BP, Version, TEXT("2"));
            return SaveNewBlueprint(Prefix, BP);
        }
        if (!AddAudienceSummary(BP)) return false;
        Metadata.SetValue(&BP, Version, TEXT("2"));
        return SaveNewBlueprint(Prefix, BP);
    }
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Summary = Find<UVerticalBox>(Tree, TEXT("Column_ClaritySummary"));
    auto* Facts = Find<UVerticalBox>(Tree, TEXT("Panel_PlacementFacts"));
    auto* Footprint = Find<UTextBlock>(Tree, TEXT("Text_Footprint"));
    auto* Queue = Find<UTextBlock>(Tree, TEXT("Text_Queue"));
    auto* Range = Find<UTextBlock>(Tree, TEXT("Text_RangeLegend"));
    auto* Balance = Find<UTextBlock>(Tree, TEXT("Text_Balance"));
    auto* Status = Find<UTextBlock>(Tree, TEXT("Text_Legend"));
    auto* Hero = Find<UBorder>(Tree, TEXT("Border_BuildPlacementHero"));
    auto* Thumbnail = Find<USizeBox>(Tree, TEXT("Size_PlacementFacilityThumb"));
    auto* ModeRow = Find<UHorizontalBox>(Tree, TEXT("Row_ClarityMode"));
    if (!Summary || !Facts || !Footprint || !Queue || !Range || !Balance || !Status || !Hero ||
        !Thumbnail || !ModeRow || Facts->GetParent() != Summary || Status->GetParent() != Summary ||
        Footprint->GetParent() != Facts || Queue->GetParent() != Facts || Range->GetParent() != Facts ||
        Facts->GetChildrenCount() != 3 || Balance->GetParent() != Summary)
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: expected current authored placement summary"), Prefix);
        return false;
    }
    for (const TCHAR* Name : {TEXT("Border_PlacementSpace"), TEXT("Column_PlacementSpace"),
        TEXT("Text_PlacementSpaceTitle"), TEXT("Row_PlacementSpaceLegend"),
        TEXT("Border_PlacementSpaceLegendFacility"), TEXT("Text_PlacementSpaceLegendFacility"),
        TEXT("Border_PlacementSpaceLegendQueue"), TEXT("Text_PlacementSpaceLegendQueue"),
        TEXT("Border_PlacementSpaceLegendClearance"), TEXT("Text_PlacementSpaceLegendClearance"),
        TEXT("Border_PlacementSpaceLegendBlocked"), TEXT("Text_PlacementSpaceLegendBlocked")})
        if (Tree.FindWidget(Name))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: unversioned widget %s already exists"), Prefix, Name);
            return false;
        }

    BP.Modify(); Tree.Modify();
    UWidget* Modified[] = {Summary, Facts, Footprint, Queue, Range, Balance, Status, Hero, Thumbnail, ModeRow};
    for (UWidget* Widget : Modified) Widget->Modify();
    Footprint->RemoveFromParent(); Queue->RemoveFromParent(); Range->RemoveFromParent();
    Balance->RemoveFromParent();
    Add(*Facts, *Balance, FMargin(0));
    Facts->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Caption = Find<UTextBlock>(Tree, TEXT("Label_Facts")))
    {
        Caption->SetText(LOCTEXT("FactsCaption", "비용 상세"));
        Style(*Caption, 12, FLinearColor::White, false);
    }

    auto* Space = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PlacementSpace"));
    Space->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(243, 247, 252), 10.f,
        FFBuildPearl::Color(205, 217, 230), 1.f));
    Space->SetBrushColor(FLinearColor::White);
    Space->SetPadding(FMargin(10.f, 8.f));
    Space->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Space->bHiddenInDesigner = false;
    auto* Column = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_PlacementSpace"));
    Space->AddChild(Column);
    const FLinearColor Ink = FFBuildPearl::Color(38, 57, 78);
    const FLinearColor Muted = FFBuildPearl::Color(77, 95, 115);
    Add(*Column, *Label(BP, Tree, TEXT("Text_PlacementSpaceTitle"),
        LOCTEXT("Title", "필요한 공간"), 16, Ink, FFBuildTypography::ERole::Heading), FMargin(0.f, 0.f, 0.f, 5.f));
    Style(*Footprint, 16, Ink, true);
    Style(*Queue, 16, FFBuildPearl::Color(18, 97, 84), true);
    Style(*Range, 12, Muted, true);
    Footprint->SetText(LOCTEXT("FootprintPreview", "시설 크기  4 × 3 m"));
    Queue->SetText(LOCTEXT("QueuePreview", "대기열 범위  2 × 5 m\n기본형 · 대기·이용 6팀\n7 기본 · 8 좌측 · 9 우측 · 0 U자"));
    Range->SetText(LOCTEXT("RangeLegend", "범위는 회전 전 가로 × 세로 기준"));
    Add(*Column, *Footprint, FMargin(0.f, 0.f, 0.f, 5.f));
    Add(*Column, *Queue, FMargin(0.f, 0.f, 0.f, 7.f));
    auto* LegendRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_PlacementSpaceLegend"));
    Legend(BP, Tree, *LegendRow, TEXT("Facility"), LOCTEXT("Facility", "시설"),
        LOCTEXT("FacilityHint", "파란 실선: 시설이 차지하는 공간"),
        FFBuildPearl::Color(220, 235, 255), FFBuildPearl::Color(37, 79, 145));
    Legend(BP, Tree, *LegendRow, TEXT("Queue"), LOCTEXT("Queue", "대기열"),
        LOCTEXT("QueueHint", "청록 선과 화살표: 손님 대기열이 차지하는 공간"),
        FFBuildPearl::Color(207, 243, 235), FFBuildPearl::Color(18, 97, 84));
    Legend(BP, Tree, *LegendRow, TEXT("Blocked"), LOCTEXT("Blocked", "문제 영역"),
        LOCTEXT("BlockedHint", "붉은 사선: 배치를 막는 영역. 정확한 원인은 상태 안내를 확인하세요."),
        FFBuildPearl::Color(255, 227, 224), FFBuildPearl::Color(166, 47, 54), true);
    Add(*Column, *LegendRow, FMargin(0.f, 0.f, 0.f, 5.f));
    Add(*Column, *Range, FMargin(0));
    Add(*Summary, *Space, FMargin(12.f, 5.f, 12.f, 5.f));
    Summary->ShiftChild(Summary->GetChildIndex(Status) + 1, Space);

    // Recover height from decorative preview chrome before exposing five lines
    // of useful space measurements. Existing actions and bound widgets survive.
    Thumbnail->SetWidthOverride(56.f); Thumbnail->SetHeightOverride(48.f);
    Hero->SetPadding(FMargin(14.f, 8.f));
    if (auto* Slot = Cast<UVerticalBoxSlot>(ModeRow->Slot)) Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    if (auto* Name = Find<UTextBlock>(Tree, TEXT("Text_FacilityName")))
        Style(*Name, 20, FLinearColor::White, true);
    for (const TCHAR* Name : {TEXT("Button_Facts"), TEXT("Button_Help")})
        if (auto* Button = Find<UButton>(Tree, Name))
        {
            auto ButtonStyle = Button->GetStyle();
            ButtonStyle.NormalPadding = ButtonStyle.PressedPadding = FMargin(8.f, 5.f);
            Button->SetStyle(ButtonStyle);
        }
    for (const TCHAR* Name : {TEXT("Text_Cost"), TEXT("Text_Legend"), TEXT("Panel_PlacementFacts"),
        TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage")})
        if (auto* Widget = Tree.FindWidget(Name))
            if (auto* Slot = Cast<UVerticalBoxSlot>(Widget->Slot)) Slot->SetPadding(FMargin(15.f, 1.f, 15.f, 3.f));

    if (!Verify(BP)) return false;
    if (!AddAudienceSummary(BP)) return false;
    Metadata.SetValue(&BP, Version, TEXT("2"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Version);
    return false;
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires idle Entry editor"), Prefix);
        return;
    }
    auto* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildToolsPackage);
    if (!BP || !BP->WidgetTree || BP->ParentClass != UFF_BuildToolsWidget::StaticClass() || !Migrate(*BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: saved BuildTools space migration failed"), Prefix);
        return;
    }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS: required space and queue summary visible"), Prefix);
}

FAutoConsoleCommand Command(TEXT("FF.ApplyBuildPlacementSpace"),
    TEXT("Expose footprint, queue bounds and the space legend in the current saved BuildTools WBP only."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
#endif
