#include "Building/Presentation/FF_BuildSynergyWidget.h"

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/Synergy/FF_FacilitySynergySubsystem.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/Data/FF_UIPalette.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "FFBuildSynergy"

UFF_BuildSynergyWidget::UFF_BuildSynergyWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> Font(
        TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard"));
    PresentationFont = Font.Object;
    const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
    ActiveTextColor = FSlateColor(Palette.Info);
    InactiveTextColor = FSlateColor(Palette.Ink);
    ActiveLineColor = Palette.Info;
    InactiveLineColor = Palette.Amber;
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::Collapsed);
    // Camera movement only reprojects at most 49 range points + 3 markers; it never runs a world query.
    ForceVolatile(true);
}

TSharedRef<SWidget> UFF_BuildSynergyWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) { BuildNativeTree(); }
    ResolveBindings();
    return Super::RebuildWidget();
}

void UFF_BuildSynergyWidget::ResolveBindings()
{
    // BindWidget handles authored WBP instances. Name resolution also supports the native
    // missing-asset fallback; neither path ever rebuilds a designer's existing tree.
    if (!WidgetTree) { return; }
    Border_Synergy = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_Synergy")));
    Text_SynergyLegend = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SynergyLegend")));
    Text_SynergyFooter = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SynergyFooter")));
    Text_SynergyTitle_1 = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SynergyTitle_1")));
    Text_SynergyTitle_2 = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SynergyTitle_2")));
    Text_SynergyTitle_3 = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SynergyTitle_3")));
    Text_SynergyDetail_1 = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SynergyDetail_1")));
    Text_SynergyDetail_2 = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SynergyDetail_2")));
    Text_SynergyDetail_3 = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_SynergyDetail_3")));
    CardPanel = Border_Synergy;
    Legend = Text_SynergyLegend;
    Footer = Text_SynergyFooter;
    RowTitles = { Text_SynergyTitle_1, Text_SynergyTitle_2, Text_SynergyTitle_3 };
    RowDetails = { Text_SynergyDetail_1, Text_SynergyDetail_2, Text_SynergyDetail_3 };
}

bool UFF_BuildSynergyWidget::HasRequiredBindings() const
{
    return CardPanel && Legend && Footer && RowTitles.Num() == MaxVisibleRows &&
        RowDetails.Num() == MaxVisibleRows &&
        !RowTitles.Contains(nullptr) && !RowDetails.Contains(nullptr);
}

void UFF_BuildSynergyWidget::BuildNativeTree()
{
    const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(344.0f);
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Synergy"));
    CardPanel = Panel;
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Palette.Paper);
    Brush.OutlineSettings = FSlateBrushOutlineSettings(10.0f, FSlateColor(Palette.LineStrong), 1.0f);
    Panel->SetBrush(Brush);
    Panel->SetBrushColor(FLinearColor::White);
    Panel->SetPadding(FMargin(14.0f, 8.0f));
    Size->AddChild(Panel);
    UCanvasPanelSlot* Position = Canvas->AddChildToCanvas(Size);
    Position->SetAnchors(FAnchors(1.0f, 0.0f));
    Position->SetAlignment(FVector2D(1.0, 0.0));
    Position->SetPosition(FVector2D(-22.0, 96.0));
    Position->SetAutoSize(true);
    UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->AddChild(Rows);

    auto AddText = [&](const FText& Text, const int32 FontSize, const FLinearColor Color,
        const bool bBold, const float BottomPadding, const FName Name = NAME_None)
    {
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        FSlateFontInfo Font = Label->GetFont();
        if (PresentationFont)
        {
            Font.FontObject = PresentationFont;
            Font.TypefaceFontName = bBold ? TEXT("SemiBold") : TEXT("Regular");
        }
        Font.Size = FontSize;
        Label->SetFont(Font);
        Label->SetColorAndOpacity(FSlateColor(Color));
        Label->SetText(Text);
        Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
        Rows->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
        return Label;
    };

    AddText(LOCTEXT("Title", "함께 두면 생기는 수익"), 16, Palette.Ink, true, 4.0f);
    Legend = AddText(LOCTEXT("Legend", "실선: 효과 적용 · 점선: 미적용 · 원: 1번 거리"),
        11, Palette.InkMuted, false, 4.0f, TEXT("Text_SynergyLegend"));
    RowTitles.Reset();
    RowDetails.Reset();
    for (int32 Index = 0; Index < MaxVisibleRows; ++Index)
    {
        RowTitles.Add(AddText(FText::GetEmpty(), 13, Palette.Ink, true, 0.0f,
            FName(*FString::Printf(TEXT("Text_SynergyTitle_%d"), Index + 1))));
        RowDetails.Add(AddText(FText::GetEmpty(), 11, Palette.InkMuted, false, 4.0f,
            FName(*FString::Printf(TEXT("Text_SynergyDetail_%d"), Index + 1))));
    }
    Footer = AddText(FText::GetEmpty(), 10, Palette.InkMuted, false, 0.0f, TEXT("Text_SynergyFooter"));
}

FText UFF_BuildSynergyWidget::ResolveFacilityName(const FName DefinitionId) const
{
    const UFF_BuildCatalogSubsystem* Catalog = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
    return Catalog ? Catalog->ResolveFacilityDisplayName(DefinitionId)
        : LOCTEXT("FacilityNameLoading", "시설 이름 불러오는 중");
}

void UFF_BuildSynergyWidget::PresentPreview(
    const FFF_FacilitySynergyPreview& Preview, const FVector& CandidateLocation)
{
    if (!Preview.bRulesAvailable || Preview.Entries.IsEmpty()) { ClearPreview(); return; }
    if (!HasRequiredBindings()) { TakeWidget(); }
    if (!ensureMsgf(HasRequiredBindings(), TEXT("Build synergy WBP requires the named Border_Synergy and Text_Synergy* bindings.")))
    { ClearPreview(); return; }

    PreviewData = Preview;
    CandidateWorldLocation = CandidateLocation + FVector(0.0, 0.0, 20.0);
    Markers.Reset(MaxVisibleRows);
    RangePoints.Reset(CircleSegments + 1);
    const int32 VisibleCount = FMath::Min(MaxVisibleRows, Preview.Entries.Num());
    for (int32 Index = 0; Index < MaxVisibleRows; ++Index)
    {
        const bool bVisible = Index < VisibleCount;
        RowTitles[Index]->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        RowDetails[Index]->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bVisible) { continue; }
        const FFF_FacilitySynergyPreviewEntry& Entry = Preview.Entries[Index];
        const bool bActive = Entry.bInRange && Entry.AppliedBonusPercent > 0.0f;
        const FText Counterpart = ResolveFacilityName(Entry.bCandidateIsTarget
            ? Entry.PartnerFacilityId : Entry.TargetFacilityId);
        const float Delta = Entry.ProposedTotalBonusPercent - Entry.CurrentTotalBonusPercent;
        const bool bUnchangedBonus = FMath::IsNearlyZero(Delta) && Entry.AppliedBonusPercent > 0.0f;
        const FText Status = Delta < -UE_SMALL_NUMBER ? LOCTEXT("Decreases", "이동 시 보너스 감소")
            : bUnchangedBonus ? (Entry.bInRange ? LOCTEXT("AlreadyActive", "기존 유지 · 추가 없음")
                : LOCTEXT("OtherPartnerActive", "거리 초과 · 기존 유지"))
            : !Entry.bInRange ? LOCTEXT("OutOfRange", "거리 초과")
            : bActive ? LOCTEXT("Active", "조합 성립") : LOCTEXT("Limited", "중복 / 상한 적용");
        const FText RecipeTitle = Entry.DisplayName.IsEmpty()
            ? FText::Format(LOCTEXT("UnnamedRecipe", "{0} + {1}"),
                ResolveFacilityName(Entry.TargetFacilityId), ResolveFacilityName(Entry.PartnerFacilityId))
            : Entry.DisplayName;
        RowTitles[Index]->SetText(FText::Format(LOCTEXT("RowTitle", "{0}. {1}"),
            FText::AsNumber(Index + 1), RecipeTitle));
        if (bApplyStateTextColors)
        { RowTitles[Index]->SetColorAndOpacity(bActive ? ActiveTextColor : InactiveTextColor); }

        FNumberFormattingOptions NumberFormat;
        NumberFormat.SetMaximumFractionalDigits(1);
        auto Number = [&NumberFormat](const float Value) { return FText::AsNumber(Value, &NumberFormat); };
        const FText Progress = Entry.bHasPartner
            ? FText::Format(LOCTEXT("Progress", "{0} · 현재 {1}m · 적용 +{2}%"),
                Status, Number(Entry.DistanceMeters), Number(Entry.AppliedBonusPercent))
            : FText::Format(LOCTEXT("Missing", "필요 시설 없음 · 설정 보너스 +{0}%"), Number(Entry.ConfiguredBonusPercent));
        // The authoritative capped/grouped total is shown. Existing support may keep the
        // total unchanged even when this candidate satisfies a rule; never claim extra income then.
        RowDetails[Index]->SetText(FText::Format(
            LOCTEXT("RowDetail", "함께: {0} · {1}m 이내\n{2}\n{3} 조합 보너스: +{4}% → +{5}%"),
            Counterpart, Number(Entry.MaxDistanceMeters), Progress,
            ResolveFacilityName(Entry.TargetFacilityId),
            Number(Entry.CurrentTotalBonusPercent), Number(Entry.ProposedTotalBonusPercent)));

        if (Entry.bHasPartner)
        {
            FFF_BuildSynergyMarker& Marker = Markers.AddDefaulted_GetRef();
            Marker.WorldLocation = (Entry.bCandidateIsTarget ? Entry.PartnerWorldLocation : Entry.TargetWorldLocation)
                + FVector(0.0, 0.0, 20.0);
            Marker.bActive = bActive;
            Marker.RowNumber = Index + 1;
        }
        if (Index == 0)
        {
            bFirstRangeActive = bActive;
            const double RadiusCm = Entry.MaxDistanceMeters * 100.0;
            for (int32 Segment = 0; Segment <= CircleSegments; ++Segment)
            {
                const double Angle = 2.0 * UE_PI * Segment / CircleSegments;
                RangePoints.Add(CandidateWorldLocation + FVector(
                    RadiusCm * FMath::Cos(Angle), RadiusCm * FMath::Sin(Angle), 0.0));
            }
        }
    }
    // Heading and legend are static Designer copy, never overwritten during runtime refresh.
    const int32 Remaining = Preview.Entries.Num() - VisibleCount;
    Footer->SetText(Remaining > 0
        ? FText::Format(LOCTEXT("MoreRules", "다른 조합 {0}개 · 중심 간 수평 거리"), FText::AsNumber(Remaining))
        : LOCTEXT("Footer", "중심 간 수평 거리 · 확정 후 수입에 반영"));
    SetVisibility(ESlateVisibility::HitTestInvisible);
    InvalidateLayoutAndVolatility();
    BP_OnPreviewChanged(PreviewData);
}

void UFF_BuildSynergyWidget::ClearPreview()
{
    const bool bHadPreview = PreviewData.bRulesAvailable;
    PreviewData = FFF_FacilitySynergyPreview();
    Markers.Reset();
    RangePoints.Reset();
    SetVisibility(ESlateVisibility::Collapsed);
    if (bHadPreview) { BP_OnPreviewCleared(); }
}

int32 UFF_BuildSynergyWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
    const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    APlayerController* Controller = GetOwningPlayer();
    if (!Controller || RangePoints.IsEmpty()) { return BaseLayer; }

    const FGeometry ScreenGeometry = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(Controller);
    const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
    auto Project = [&](const FVector& World, FVector2f& Out)
    {
        FVector2D Position;
        if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, World, Position, true))
        { return false; }
        Out = FVector2f(AllottedGeometry.AbsoluteToLocal(ScreenGeometry.LocalToAbsolute(Position)));
        return FMath::IsFinite(Out.X) && FMath::IsFinite(Out.Y);
    };
    const FGeometry PanelGeometry = CardPanel ? CardPanel->GetCachedGeometry() : FGeometry();
    const FVector2D PanelMin = AllottedGeometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(FVector2D::ZeroVector));
    const FVector2D PanelMax = AllottedGeometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize()));
    const FBox2D PanelBounds(PanelMin - FVector2D(3.0), PanelMax + FVector2D(3.0));
    TArray<FVector2f> SegmentPoints;
    SegmentPoints.SetNumUninitialized(2);
    auto DrawSegment = [&](const FVector2f A, const FVector2f B, const FLinearColor Color, const float Thickness)
    {
        SegmentPoints[0] = A;
        SegmentPoints[1] = B;
        FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(),
            SegmentPoints, ESlateDrawEffect::None, Color * Tint, true, Thickness);
    };
    auto Line = [&](const FVector2f A, const FVector2f B, const FLinearColor Color, const float Thickness)
    {
        // NativePaint runs above the child tree. Cut line segments at the opaque card,
        // keeping connection/range strokes out of all text, including when the camera moves.
        float Enter = 0.0f;
        float Leave = 1.0f;
        const FVector2f Delta = B - A;
        bool bIntersects = true;
        for (int32 Axis = 0; Axis < 2; ++Axis)
        {
            if (FMath::IsNearlyZero(Delta[Axis]))
            {
                if (A[Axis] < PanelBounds.Min[Axis] || A[Axis] > PanelBounds.Max[Axis])
                { bIntersects = false; break; }
            }
            else
            {
                float First = (PanelBounds.Min[Axis] - A[Axis]) / Delta[Axis];
                float Last = (PanelBounds.Max[Axis] - A[Axis]) / Delta[Axis];
                if (First > Last) { Swap(First, Last); }
                Enter = FMath::Max(Enter, First);
                Leave = FMath::Min(Leave, Last);
                if (Enter > Leave) { bIntersects = false; break; }
            }
        }
        if (!bIntersects) { DrawSegment(A, B, Color, Thickness); return; }
        if (Enter > 0.0f) { DrawSegment(A, FMath::Lerp(A, B, Enter), Color, Thickness); }
        if (Leave < 1.0f) { DrawSegment(FMath::Lerp(A, B, Leave), B, Color, Thickness); }
    };
    // Bound clipping to this player's viewport. Hidden/behind-camera points break an arc;
    // no world ray casts or occlusion tests are performed for this diagram overlay.
    OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
    FVector2f Previous = FVector2f::ZeroVector;
    bool bPreviousVisible = false;
    FLinearColor RangeColor = bFirstRangeActive ? ActiveLineColor : InactiveLineColor;
    RangeColor.A *= FMath::Clamp(RangeOpacity, 0.0f, 1.0f);
    for (int32 Index = 0; Index < RangePoints.Num(); ++Index)
    {
        FVector2f Point = FVector2f::ZeroVector;
        const bool bVisible = Project(RangePoints[Index], Point);
        if (bPreviousVisible && bVisible && (bFirstRangeActive || Index % 2 == 0))
        { Line(Previous, Point, RangeColor, FMath::Clamp(RangeLineThickness, 0.5f, 12.0f)); }
        Previous = Point;
        bPreviousVisible = bVisible;
    }
    FVector2f Candidate;
    if (Project(CandidateWorldLocation, Candidate))
    {
        for (const FFF_BuildSynergyMarker& Marker : Markers)
        {
            FVector2f Partner;
            if (!Project(Marker.WorldLocation, Partner)) { continue; }
            const FLinearColor Color = Marker.bActive ? ActiveLineColor : InactiveLineColor;
            if (Marker.bActive) { Line(Candidate, Partner, Color, FMath::Clamp(ActiveLineThickness, 0.5f, 12.0f)); }
            else
            {
                // Fixed segment budget keeps extremely distant off-screen targets inexpensive.
                constexpr int32 DashSegments = 24;
                for (int32 Index = 0; Index < DashSegments; Index += 2)
                {
                    Line(FMath::Lerp(Candidate, Partner, static_cast<float>(Index) / DashSegments),
                        FMath::Lerp(Candidate, Partner, static_cast<float>(Index + 1) / DashSegments), Color,
                        FMath::Clamp(InactiveLineThickness, 0.5f, 12.0f));
                }
            }
            if (PanelBounds.ExpandBy(48.0).IsInsideOrOn(FVector2D(Partner))) { continue; }
            const float Radius = FMath::Clamp(MarkerRadius, 4.0f, 48.0f);
            const FVector2f Corners[] = { Partner + FVector2f(0.0f, -Radius),
                Partner + FVector2f(Radius, 0.0f), Partner + FVector2f(0.0f, Radius),
                Partner + FVector2f(-Radius, 0.0f) };
            for (int32 Index = 0; Index < 4; ++Index)
            { Line(Corners[Index], Corners[(Index + 1) % 4], Color, FMath::Clamp(MarkerThickness, 0.5f, 12.0f)); }
            const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::Clamp(MarkerFontSize, 8, 32));
            FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2,
                AllottedGeometry.ToPaintGeometry(FVector2D(28.0, 22.0),
                    FSlateLayoutTransform(FVector2D(Partner.X + 18.0f, Partner.Y - 12.0f))),
                FString::FromInt(Marker.RowNumber), Font, ESlateDrawEffect::None, Color * Tint);
        }
    }
    OutDrawElements.PopClip();
    return BaseLayer + 2;
}

#undef LOCTEXT_NAMESPACE
