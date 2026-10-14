// Explicit loaded-world/selection scope capture and read-only before/after preview.
// Geometry snapshots and transactions live in FFZASelectedWeldSession. Scope remains fixed
// until recaptured; no unloaded actors are loaded and actor selection is never changed.
#include "Weld/FFZASelectedWeldTool.h"
#include "Weld/FFZASelectedWeldSession.h"

#include "FF_FFZSZoneActor.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Debug/DebugDrawService.h"
#include "Editor.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Misc/TransactionObjectEvent.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "FFZASelectedWeld"

namespace
{
    constexpr int32 MaximumPreviewSegments = 4096;
    constexpr int32 MaximumDiagnosticRows = 512;

    struct FZoneOption
    {
        FText Label;
        FString ActorPath;
        FText Detail;
    };

    struct FPreviewSegment
    {
        FVector A;
        FVector B;
        FLinearColor Color;
        float Thickness = 1.0f;
    };

    struct FDiagnosticOption
    {
        FText Label;
        FLinearColor Color;
    };

    bool IsFinitePreviewPoint(const FVector& Point)
    {
        return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y) && FMath::IsFinite(Point.Z);
    }

    // Diagnostic points are XY geometry. Reconstruct preview height from the snapshot only;
    // no landscape traces, world queries, or edits are needed for residual visualization.
    FVector AtBoundaryHeight(const FVector2D& Point, const TArray<FVector>& Ring)
    {
        double BestDistanceSquared = TNumericLimits<double>::Max();
        double Height = 0.0;
        for (int32 Index = 0; Index < Ring.Num(); ++Index)
        {
            const FVector& A = Ring[Index];
            const FVector& B = Ring[(Index + 1) % Ring.Num()];
            if (!IsFinitePreviewPoint(A) || !IsFinitePreviewPoint(B)) { continue; }
            const FVector2D A2(A.X, A.Y), B2(B.X, B.Y);
            const FVector2D Delta = B2 - A2;
            const double Alpha = Delta.SizeSquared() > UE_SMALL_NUMBER
                ? FMath::Clamp(FVector2D::DotProduct(Point - A2, Delta) / Delta.SizeSquared(), 0.0, 1.0) : 0.0;
            const double DistanceSquared = (Point - (A2 + Delta * Alpha)).SizeSquared();
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                Height = FMath::Lerp(A.Z, B.Z, Alpha);
            }
        }
        return FVector(Point.X, Point.Y, Height);
    }

    TWeakPtr<SWindow> ActiveWindow;
    class SSelectedWeldPanel;
    TWeakPtr<SSelectedWeldPanel> ActivePanel;
    // Only successful sessions are retained. Actor references in a session remain weak.
    // This also refreshes derived presentation after undo when the tool window is closed.
    TArray<TSharedPtr<FFFZASelectedWeldSession>> AppliedSessions;
    TSet<TWeakObjectPtr<UWorld>> UndoRedoDirtyWorlds;
    TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UWorld>> AppliedGeometryWorlds;

    void CollectGeometryDependencies(AFFZSGameplayZone& Zone, TSet<TWeakObjectPtr<UObject>>& OutObjects)
    {
        OutObjects.Add(&Zone);
        for (USceneComponent* Source : { Zone.GetRootComponent(), static_cast<USceneComponent*>(Zone.BoundarySpline.Get()) })
        {
            for (USceneComponent* Component = Source; Component; Component = Component->GetAttachParent())
            {
                if (OutObjects.Contains(Component)) { break; }
                OutObjects.Add(Component);
                if (AActor* Owner = Component->GetOwner()) { OutObjects.Add(Owner); }
            }
        }
    }

    void RememberAppliedGeometry(const FFFZASelectedWeldSession& Session)
    {
        TSet<TWeakObjectPtr<UObject>> Dependencies;
        for (const FFFZASelectedWeldZone& Row : Session.GetZones())
        {
            if (AFFZSGameplayZone* Zone = Row.Zone.Get()) { CollectGeometryDependencies(*Zone, Dependencies); }
        }
        for (const TWeakObjectPtr<UObject>& Object : Dependencies)
        {
            AppliedGeometryWorlds.Add(Object, Session.GetWorld());
        }
    }

    bool IsEditableWorld(const UWorld* World)
    {
        return World && World->WorldType == EWorldType::Editor && GEditor && !GEditor->PlayWorld;
    }

    void CaptureSelectedZones(TArray<AFFZSGameplayZone*>& OutZones, int32& OutIgnoredActors)
    {
        OutIgnoredActors = 0;
        if (!GEditor || !GEditor->GetSelectedActors())
        {
            return;
        }
        for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
        {
            if (AFFZSGameplayZone* Zone = Cast<AFFZSGameplayZone>(*It))
            {
                OutZones.Add(Zone);
            }
            else
            {
                ++OutIgnoredActors;
            }
        }
    }

    void CaptureLoadedWorldZones(UWorld* World, TArray<AFFZSGameplayZone*>& OutZones)
    {
        OutZones.Reset();
        if (!World) { return; }
        TSet<AFFZSGameplayZone*> Seen;
        // Explicit flags include invisible but loaded levels. The default OnlyActiveLevels
        // filter would omit those Zones despite describing this scope as the loaded world.
        for (TActorIterator<AFFZSGameplayZone> It(World, AFFZSGameplayZone::StaticClass(), EActorIteratorFlags::SkipPendingKill); It; ++It)
        {
            AFFZSGameplayZone* Zone = *It;
            if (IsValid(Zone) && !Zone->IsActorBeingDestroyed() && Zone->GetWorld() == World && !Seen.Contains(Zone))
            {
                Seen.Add(Zone); OutZones.Add(Zone);
            }
        }
    }

    bool CanOpenZoneWeld()
    {
        return GEditor && IsEditableWorld(GEditor->GetEditorWorldContext().World());
    }

    void RedrawPreview()
    {
        if (GEditor)
        {
            GEditor->RedrawLevelEditingViewports(false);
        }
    }

    class SSelectedWeldPanel final : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SSelectedWeldPanel) : _ExpectedWorld(nullptr), _ToleranceCm(50.0f) {}
            SLATE_ARGUMENT(UWorld*, ExpectedWorld)
            SLATE_ARGUMENT(float, ToleranceCm)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            ExpectedWorld = InArgs._ExpectedWorld;
            ToleranceCm = FMath::IsFinite(InArgs._ToleranceCm)
                ? FMath::Max(0.0f, InArgs._ToleranceCm) : 50.0f;
            CaptureScope();

            ChildSlot
            [
                SNew(SBorder)
                .Padding(16.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("Instructions", "현재 편집 월드에 로드된 전체 Zone을 자동으로 연결합니다. 공통 경계의 점 이동이 어려운 좁거나 오목한 구간은 틈 영역을 이웃 Zone에 편입해 복구합니다. 기준 Zone 지정은 필요하지 않습니다. 최종 경계 검사를 통과한 결과를 한 번에 적용합니다. 로드되지 않은 Zone은 포함하지 않습니다."))
                        .AutoWrapText(true)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Text(this, &SSelectedWeldPanel::GetScopeText)
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        .VAlign(VAlign_Center).Padding(0, 0, 12, 0)
                        [
                            SNew(SCheckBox)
                            .IsChecked(this, &SSelectedWeldPanel::GetSelectedOnly)
                            .IsEnabled(this, &SSelectedWeldPanel::CanRecapture)
                            .OnCheckStateChanged(this, &SSelectedWeldPanel::OnScopeChanged)
                            [ SNew(STextBlock).Text(LOCTEXT("SelectedOnly", "선택한 Zone만")) ]
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton)
                            .Text(this, &SSelectedWeldPanel::GetRecaptureText)
                            .ToolTipText(LOCTEXT("RecaptureTooltip", "현재 범위의 Zone을 다시 읽어 고정된 작업 목록으로 가져옵니다. 미리보기는 초기화되며 Zone이나 레벨을 변경하지 않습니다."))
                            .IsEnabled(this, &SSelectedWeldPanel::CanRecapture)
                            .OnClicked(this, &SSelectedWeldPanel::OnRecapture)
                        ]
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 0, 0, 10)
                    [
                        SAssignNew(ZoneList, SListView<TSharedPtr<FZoneOption>>)
                        .ListItemsSource(&ZoneRows)
                        .SelectionMode(ESelectionMode::None)
                        .OnGenerateRow(this, &SSelectedWeldPanel::GenerateZoneRow)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                        [
                            SNew(STextBlock).Text(LOCTEXT("Tolerance", "Weld Tolerance Cm"))
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SBox).WidthOverride(100.0f)
                            [
                                SNew(SSpinBox<float>)
                                .MinValue(0.0f).MinSliderValue(0.0f).MaxSliderValue(200.0f)
                                .Delta(1.0f)
                                .Value(this, &SSelectedWeldPanel::GetTolerance)
                                .OnValueChanged(this, &SSelectedWeldPanel::OnToleranceChanged)
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SNew(STextBlock).Text(this, &SSelectedWeldPanel::GetDiagnosticText).AutoWrapText(true)
                    ]
                    + SVerticalBox::Slot().MaxHeight(180.0f).AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SAssignNew(DiagnosticList, SListView<TSharedPtr<FDiagnosticOption>>)
                        .ListItemsSource(&DiagnosticRows)
                        .SelectionMode(ESelectionMode::None)
                        .Visibility_Lambda([this] { return DiagnosticRows.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
                        .OnGenerateRow(this, &SSelectedWeldPanel::GenerateDiagnosticRow)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SNew(SCheckBox)
                        .IsChecked(this, &SSelectedWeldPanel::GetShowPreview)
                        .OnCheckStateChanged(this, &SSelectedWeldPanel::OnShowPreviewChanged)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("PreviewLegend", "뷰포트: 이전 경계 빨강 · 공통 경계 결과 청록 · 남은 틈 자홍 · 겹침 보라 · 거부 빨강 (Composite Editor Primitives 켜기)"))
                            .AutoWrapText(true)
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SNew(STextBlock).Text(this, &SSelectedWeldPanel::GetStatusText).AutoWrapText(true)
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                        [
                            SNew(SButton).Text(LOCTEXT("Preview", "미리보기"))
                            .IsEnabled(this, &SSelectedWeldPanel::CanPreview)
                            .OnClicked(this, &SSelectedWeldPanel::OnPreview)
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton).Text(LOCTEXT("Apply", "자동 복구 후 적용"))
                            .ToolTipText(LOCTEXT("ApplyTooltip", "틈과 겹침을 자동 복구하고 최종 검사에서 모두 해결된 결과만 한 번의 Undo 작업으로 적용합니다. 미리보기가 없으면 클릭 시 계산합니다."))
                            .IsEnabled(this, &SSelectedWeldPanel::CanApply)
                            .OnClicked(this, &SSelectedWeldPanel::OnApply)
                        ]
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SNew(STextBlock).Text(LOCTEXT("ScopeNotice", "  목록 밖 Zone은 수정하지 않습니다."))
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton).Text(LOCTEXT("Close", "닫기"))
                            .OnClicked(this, &SSelectedWeldPanel::OnClose)
                        ]
                    ]
                ]
            ];
            DrawHandle = UDebugDrawService::Register(TEXT("CompositeEditorPrimitives"),
                FDebugDrawDelegate::CreateSP(this, &SSelectedWeldPanel::DrawPreview));
            ObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddSP(
                this, &SSelectedWeldPanel::OnScopedObjectModified);
            if (GEngine)
            {
                ActorAddedHandle = GEngine->OnLevelActorAdded().AddSP(this, &SSelectedWeldPanel::OnWorldZoneListChanged);
                ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddSP(this, &SSelectedWeldPanel::OnWorldZoneListChanged);
            }
        }

        virtual ~SSelectedWeldPanel() override
        {
            StopPreviewDrawing();
        }

        void SetWindow(const TSharedRef<SWindow>& InWindow)
        {
            Window = InWindow;
        }

        void StopPreviewDrawing()
        {
            FCoreUObjectDelegates::OnObjectModified.Remove(ObjectModifiedHandle);
            ObjectModifiedHandle.Reset();
            if (GEngine)
            {
                GEngine->OnLevelActorAdded().Remove(ActorAddedHandle);
                GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
            }
            ActorAddedHandle.Reset(); ActorDeletedHandle.Reset();
            if (DrawHandle.IsValid())
            {
                UDebugDrawService::Unregister(DrawHandle);
                DrawHandle.Reset();
            }
            PreviewSegments.Reset();
            WatchedObjects.Reset();
            RedrawPreview();
        }

        void HandleUndoRedo()
        {
            InvalidatePreview(LOCTEXT("UndoInvalidated", "Undo/Redo 이후에는 경계 미리보기를 다시 생성하세요."));
        }

    private:
        void CaptureScope()
        {
            if (GEditor) { ExpectedWorld = GEditor->GetEditorWorldContext().World(); }
            TArray<AFFZSGameplayZone*> Zones;
            IgnoredActorCount = 0;
            if (bSelectedOnly)
            {
                CaptureSelectedZones(Zones, IgnoredActorCount);
            }
            else
            {
                // TActorIterator only visits loaded actors. It does not request World
                // Partition cells, change selection, or load an asset/level package.
                CaptureLoadedWorldZones(ExpectedWorld.Get(), Zones);
            }
            Session = MakeShared<FFFZASelectedWeldSession>(Zones);
            bScopeNeedsRecapture = false;
            PreviewSegments.Reset();
            DiagnosticRows.Reset();
            DiagnosticText = FText::GetEmpty();
            bHasDisplayedResult = false;
            if (DiagnosticList) { DiagnosticList->RequestListRefresh(); }
            bPreviewTruncated = false;
            RebuildRows();
            CacheWatchedObjects();
            StatusText = SupportedZoneCount >= 2
                ? LOCTEXT("ReadyAutomatic", "미리보기에서 공통 경계를 확인하거나 '자동 복구 후 적용'을 누르세요.")
                : LOCTEXT("NeedTwoZones", "작업 범위에 닫힌 Linear 경계를 가진 Zone이 두 개 이상 필요합니다. 범위를 확인하고 다시 가져오세요.");
            RedrawPreview();
        }

        void CacheWatchedObjects()
        {
            WatchedObjects.Reset();
            if (!Session) { return; }
            for (const FFFZASelectedWeldZone& Row : Session->GetZones())
            {
                if (AFFZSGameplayZone* Zone = Row.Zone.Get())
                {
                    CollectGeometryDependencies(*Zone, WatchedObjects);
                }
            }
        }

        void OnScopedObjectModified(UObject* Object)
        {
            if (!bApplying && Session && (Session->HasPreview() || bHasDisplayedResult) && WatchedObjects.Contains(Object))
            {
                InvalidatePreview(LOCTEXT("ObjectChanged", "작업 목록의 Zone 또는 Spline이 변경되었습니다. 미리보기를 다시 실행하세요."));
            }
        }

        void OnWorldZoneListChanged(AActor* Actor)
        {
            if (bSelectedOnly || bApplying || !Session || !Actor ||
                !Actor->IsA<AFFZSGameplayZone>() || Actor->GetWorld() != ExpectedWorld.Get()) { return; }
            // A new/deleted Zone changes what "전체" means. Do not silently reuse a fixed
            // scope and omit new actors during automatic Apply; explicit recapture is required.
            bScopeNeedsRecapture = true;
            InvalidatePreview(LOCTEXT("WorldScopeChanged", "월드의 Zone이 추가되거나 삭제되었습니다. '현재 월드 다시 가져오기'로 전체 목록을 갱신하세요."));
        }

        bool CheckLoadedWorldScope()
        {
            if (bSelectedOnly) { return true; }
            if (bScopeNeedsRecapture || !Session) { return false; }
            // Editor add/delete events do not cover every World Partition actor load/unload
            // or streamed-level change. Recheck membership only on Preview/Apply actions,
            // before geometry work or Modify(), never in Slate frame attributes/drawing.
            TArray<AFFZSGameplayZone*> CurrentZones;
            CaptureLoadedWorldZones(ExpectedWorld.Get(), CurrentZones);
            TSet<AFFZSGameplayZone*> CapturedZones;
            for (const FFFZASelectedWeldZone& Row : Session->GetZones())
            {
                if (AFFZSGameplayZone* Zone = Row.Zone.Get()) { CapturedZones.Add(Zone); }
            }
            bool bMatches = CurrentZones.Num() == Session->GetZones().Num() && CapturedZones.Num() == CurrentZones.Num();
            if (bMatches)
            {
                for (AFFZSGameplayZone* Zone : CurrentZones)
                {
                    if (!CapturedZones.Contains(Zone)) { bMatches = false; break; }
                }
            }
            if (bMatches) { return true; }
            bScopeNeedsRecapture = true;
            InvalidatePreview(LOCTEXT("LoadedWorldScopeChanged", "로드된 Zone 목록이 변경되었습니다. '현재 월드 다시 가져오기'로 전체 목록을 갱신한 뒤 실행하세요."));
            return false;
        }

        void RebuildRows()
        {
            TArray<TSharedPtr<FZoneOption>> PreviousRows = MoveTemp(ZoneRows);
            ZoneRows.Reset();
            SupportedZoneCount = 0;
            if (!Session)
            {
                return;
            }
            const TArray<FFFZASelectedWeldZone>& Zones = Session->GetZones();
            for (int32 Index = 0; Index < Zones.Num(); ++Index)
            {
                const FFFZASelectedWeldZone& Zone = Zones[Index];
                // Keep option identities stable when the same captured scope is re-previewed.
                TSharedPtr<FZoneOption> Row = PreviousRows.IsValidIndex(Index)
                    && PreviousRows[Index]->ActorPath == Zone.ActorPath
                    ? PreviousRows[Index] : MakeShared<FZoneOption>();
                Row->ActorPath = Zone.ActorPath;
                Row->Label = FText::FromString(FString::Printf(TEXT("%s  [Zone ID: %s]"),
                    *Zone.Label, *Zone.ZoneId.ToString()));
                FString Detail;
                if (!Zone.bSupported)
                {
                    Detail = FString::Printf(TEXT("지원하지 않음: %s"), *Zone.Issue);
                }
                else
                {
                    ++SupportedZoneCount;
                    if (!Session->GetStats().Groups.IsEmpty() || Session->HasPreview())
                    {
                        if (Session->GetStats().Groups.IsValidIndex(Zone.GroupIndex))
                        {
                            const FFZALayoutOps::FWeldGroup& Group = Session->GetStats().Groups[Zone.GroupIndex];
                            Detail = FString::Printf(TEXT("묶음 %d"), Zone.GroupIndex + 1);
                            Detail += TEXT(" · 공통 경계 자동 연결");
                            if (Group.PolygonIndices.Num() == 1) { Detail += TEXT(" · 독립 Zone"); }
                            Detail += TEXT(" · ");
                        }
                        if (Zone.bInvalidInput) { Detail += TEXT("잘못된 다각형 · 적용 불가"); }
                        else if (Zone.bRejected) { Detail += TEXT("안전 검사에서 변경 거부"); }
                        else { Detail += Zone.bChanged ? (Session->HasPreview() ? TEXT("변경 예정") : TEXT("적용 결과")) : TEXT("변경 없음"); }
                        if (Zone.bChanged)
                        {
                            Detail += FString::Printf(TEXT(" · 이동 %d / 추가 %d / 제거 %d / 최대 %.2f cm"),
                                Zone.MovedPointCount, Zone.InsertedPointCount, Zone.RemovedPointCount, Zone.MaximumMoveCm);
                        }
                        if (Zone.ResidualGapCount > 0 || Zone.ResidualOverlapCount > 0)
                        {
                            Detail += FString::Printf(TEXT(" · %s틈 %d쌍 / 겹침 %d쌍"),
                                Session->GetDiagnostics().bTruncated ? TEXT("표시된 ") : TEXT("남은 "),
                                Zone.ResidualGapCount, Zone.ResidualOverlapCount);
                        }
                        if (!Zone.Issue.IsEmpty()) { Detail += TEXT(" · ") + Zone.Issue; }
                    }
                    else
                    {
                        Detail = TEXT("미리보기 대기");
                    }
                }
                Row->Detail = FText::FromString(Detail);
                ZoneRows.Add(Row);
            }
            ScopeText = bSelectedOnly
                ? FText::Format(LOCTEXT("SelectedScopeCount", "고정된 선택 목록: Zone {0}개 · 지원 {1}개 · Zone 외 선택 제외 {2}개"),
                    FText::AsNumber(ZoneRows.Num()), FText::AsNumber(SupportedZoneCount), FText::AsNumber(IgnoredActorCount))
                : FText::Format(LOCTEXT("WorldScopeCount", "현재 월드의 로드된 전체 Zone: {0}개 · 지원 {1}개"),
                    FText::AsNumber(ZoneRows.Num()), FText::AsNumber(SupportedZoneCount));
            if (ZoneList) { ZoneList->RequestListRefresh(); }
        }

        TSharedRef<ITableRow> GenerateZoneRow(TSharedPtr<FZoneOption> Row, const TSharedRef<STableViewBase>& OwnerTable)
        {
            return SNew(STableRow<TSharedPtr<FZoneOption>>, OwnerTable)
                .Padding(FMargin(6.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock).Text_Lambda([Row] { return Row->Label; }).ToolTipText(FText::FromString(Row->ActorPath))
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock).Text_Lambda([Row] { return Row->Detail; }).AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                ];
        }

        TSharedRef<ITableRow> GenerateDiagnosticRow(TSharedPtr<FDiagnosticOption> Row, const TSharedRef<STableViewBase>& OwnerTable)
        {
            return SNew(STableRow<TSharedPtr<FDiagnosticOption>>, OwnerTable)
                .Padding(FMargin(6.0f, 3.0f))
                [
                    SNew(STextBlock).Text(Row->Label).AutoWrapText(true).ColorAndOpacity(Row->Color)
                ];
        }

        FText GetScopeText() const
        {
            return ScopeText;
        }

        FText GetDiagnosticText() const { return DiagnosticText; }

        FText GetRecaptureText() const
        {
            return bSelectedOnly ? LOCTEXT("RecaptureSelected", "현재 선택 다시 가져오기") : LOCTEXT("RecaptureWorld", "현재 월드 다시 가져오기");
        }

        FText GetStatusText() const
        {
            if (GEditor && GEditor->GetEditorWorldContext().World() != ExpectedWorld.Get())
            {
                return LOCTEXT("WorldChanged", "편집 월드가 변경되었습니다. 다시 가져오기로 작업 목록을 교체하세요.");
            }
            return StatusText;
        }
        float GetTolerance() const { return ToleranceCm; }
        ECheckBoxState GetShowPreview() const { return bShowPreview ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
        ECheckBoxState GetSelectedOnly() const { return bSelectedOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

        bool CanRecapture() const { return GEditor && IsEditableWorld(GEditor->GetEditorWorldContext().World()); }
        bool CanPreview() const
        {
            return CanRecapture() && Session && Session->GetWorld() == ExpectedWorld.Get()
                && GEditor->GetEditorWorldContext().World() == ExpectedWorld.Get()
                && SupportedZoneCount >= 2 && !bScopeNeedsRecapture
                && FMath::IsFinite(ToleranceCm) && ToleranceCm > 0.0f;
        }
        // Applying can calculate a fresh repair without a separate preview click. The session
        // still enforces the full completion gate before any actor or spline Modify().
        bool CanApply() const { return CanPreview(); }

        void InvalidatePreview(const FText& Message)
        {
            if (Session) { Session->ClearPreview(); }
            PreviewSegments.Reset();
            DiagnosticRows.Reset();
            DiagnosticText = FText::GetEmpty();
            bHasDisplayedResult = false;
            if (DiagnosticList) { DiagnosticList->RequestListRefresh(); }
            bPreviewTruncated = false;
            StatusText = Message;
            RebuildRows();
            CacheWatchedObjects();
            RedrawPreview();
        }

        void OnScopeChanged(ECheckBoxState NewState)
        {
            const bool bNewSelectedOnly = NewState == ECheckBoxState::Checked;
            if (bSelectedOnly == bNewSelectedOnly) { return; }
            bSelectedOnly = bNewSelectedOnly;
            CaptureScope();
        }

        void OnToleranceChanged(float NewToleranceCm)
        {
            if (!FMath::IsFinite(NewToleranceCm) || ToleranceCm == NewToleranceCm) { return; }
            ToleranceCm = FMath::Max(0.0f, NewToleranceCm);
            InvalidatePreview(LOCTEXT("ToleranceChanged", "허용 거리를 변경했습니다. 미리보기를 다시 실행하세요."));
        }

        void OnShowPreviewChanged(ECheckBoxState NewState)
        {
            bShowPreview = NewState == ECheckBoxState::Checked;
            RedrawPreview();
        }

        FReply OnRecapture()
        {
            CaptureScope();
            return FReply::Handled();
        }

        FReply OnPreview()
        {
            if (!CanPreview()) { return FReply::Handled(); }
            if (!CheckLoadedWorldScope()) { return FReply::Handled(); }
            FText Error;
            const bool bBuilt = Session->BuildPreview(ToleranceCm, Error);
            if (!bBuilt && !Session->HasPreview())
            {
                PreviewSegments.Reset();
                DiagnosticRows.Reset();
                DiagnosticText = FText::GetEmpty();
                bHasDisplayedResult = false;
                StatusText = Error;
            }
            else
            {
                CacheDiagnosticRows();
                CachePreviewSegments();
                bHasDisplayedResult = true;
                StatusText = bBuilt ? Session->GetStatusText() : Error;
                if (bPreviewTruncated)
                {
                    StatusText = FText::Format(LOCTEXT("PreviewCapped", "{0}\n뷰포트 표시는 성능을 위해 최대 {1}개 선분으로 제한됩니다. 적용 계산에는 모든 경계가 포함됩니다."),
                        StatusText, FText::AsNumber(MaximumPreviewSegments));
                }
            }
            if (DiagnosticList) { DiagnosticList->RequestListRefresh(); }
            RebuildRows();
            CacheWatchedObjects();
            RedrawPreview();
            return FReply::Handled();
        }

        FReply OnApply()
        {
            if (!CanApply()) { return FReply::Handled(); }
            if (!CheckLoadedWorldScope()) { return FReply::Handled(); }
            FText Message;
            bool bPrepared = true;
            if (!Session->HasPreview())
            {
                bPrepared = Session->BuildPreview(ToleranceCm, Message);
            }
            // Modify() broadcasts object notifications before the write. Suppress only our
            // own transaction; external edits invalidate the visible preview immediately.
            TGuardValue<bool> ApplyingGuard(bApplying, true);
            if (bPrepared && Session->Apply(Message))
            {
                // Retain just the most recent session per world: derived data refresh is world-wide
                // and retaining every historical scope would repeat the same rebuild on Undo.
                const UWorld* AppliedWorld = Session->GetWorld();
                AppliedSessions.RemoveAll([AppliedWorld](const TSharedPtr<FFFZASelectedWeldSession>& Previous)
                {
                    return !Previous || !Previous->GetWorld() || Previous->GetWorld() == AppliedWorld;
                });
                AppliedSessions.Add(Session);
                RememberAppliedGeometry(*Session);
                // Keep the inspected residuals as the last applied result; the earlier authored
                // rings are no longer pending changes and must not be drawn after this commit.
                CacheDiagnosticRows();
                CachePreviewSegments(false);
                bHasDisplayedResult = true;
            }
            else if (Session->HasPreview())
            {
                // Failed automatic repair remains a read-only diagnostic. It did not alter the
                // level, and its unresolved spans must stay visible so the failure is actionable.
                CacheDiagnosticRows();
                CachePreviewSegments();
                bHasDisplayedResult = true;
            }
            else
            {
                PreviewSegments.Reset();
                DiagnosticRows.Reset();
                DiagnosticText = FText::GetEmpty();
                bHasDisplayedResult = false;
                bPreviewTruncated = false;
            }
            StatusText = Message;
            if (bPreviewTruncated)
            {
                StatusText = FText::Format(LOCTEXT("AppliedPreviewCapped", "{0}\n뷰포트 표시는 최대 {1}개 선분으로 제한됩니다."),
                    StatusText, FText::AsNumber(MaximumPreviewSegments));
            }
            if (DiagnosticList) { DiagnosticList->RequestListRefresh(); }
            RebuildRows();
            CacheWatchedObjects();
            RedrawPreview();
            return FReply::Handled();
        }

        FReply OnClose()
        {
            StopPreviewDrawing();
            if (TSharedPtr<SWindow> PinnedWindow = Window.Pin()) { PinnedWindow->RequestDestroyWindow(); }
            return FReply::Handled();
        }

        void CacheDiagnosticRows()
        {
            DiagnosticRows.Reset();
            const FFZAWeldDiagnostics::FResult& Diagnostics = Session->GetDiagnostics();
            const TArray<FFFZASelectedWeldZone>& Zones = Session->GetZones();
            DiagnosticRows.Reserve(FMath::Min(MaximumDiagnosticRows, Diagnostics.Issues.Num()));
            const auto ZoneLabel = [&Zones](int32 Index)
            {
                return Zones.IsValidIndex(Index) ? Zones[Index].Label : FString(TEXT("알 수 없는 Zone"));
            };
            for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Diagnostics.Issues)
            {
                if (DiagnosticRows.Num() == MaximumDiagnosticRows) { break; }
                TSharedPtr<FDiagnosticOption> Row = MakeShared<FDiagnosticOption>();
                FString Label;
                switch (Issue.Type)
                {
                case FFZAWeldDiagnostics::EIssueType::Gap:
                    Label = FString::Printf(TEXT("남은 틈 · %s ↔ %s · 최대 %.2f cm"),
                        *ZoneLabel(Issue.PolygonIndexA), *ZoneLabel(Issue.PolygonIndexB), Issue.MaximumGapCm);
                    Row->Color = FLinearColor(1.0f, 0.35f, 0.8f);
                    break;
                case FFZAWeldDiagnostics::EIssueType::Overlap:
                    // Each retained span carries the pair's total area, so never sum row areas.
                    Label = FString::Printf(TEXT("겹침 · %s ↔ %s · %.4f m² (Zone 쌍 면적)"),
                        *ZoneLabel(Issue.PolygonIndexA), *ZoneLabel(Issue.PolygonIndexB), Issue.OverlapAreaCm2 / 10000.0);
                    Row->Color = FLinearColor(0.75f, 0.45f, 1.0f);
                    break;
                case FFZAWeldDiagnostics::EIssueType::Rejected:
                    Label = FString::Printf(TEXT("거부 · %s · 변경이 거부된 Zone의 원래 경계 구간"), *ZoneLabel(Issue.PolygonIndexA));
                    Row->Color = FLinearColor(1.0f, 0.3f, 0.3f);
                    break;
                }
                if (Issue.Type != FFZAWeldDiagnostics::EIssueType::Rejected && Issue.bRelatedToRejection)
                {
                    Label += TEXT(" · 거부된 변경과 관련");
                }
                Row->Label = FText::FromString(Label);
                DiagnosticRows.Add(Row);
            }
            DiagnosticText = FText::FromString(FString::Printf(
                TEXT("자동 연결 결과: 남은 틈 %d쌍 · 겹침 %d쌍 · 거부 Zone %d개\n틈 복구·검사 범위 %.2f cm (Weld Tolerance의 2배). 목록의 유효한 XY 경계를 함께 움직여 공통 경계를 만들고 겹침을 정리합니다. 모두 해결된 결과만 적용합니다. 겹침 면적은 같은 Zone 쌍의 각 구간에 반복 표시됩니다."),
                Diagnostics.GapPairCount, Diagnostics.OverlapPairCount, Diagnostics.RejectedPolygonCount, Diagnostics.GapInspectionCm));
            if (!Session->HasPreview())
            {
                DiagnosticText = FText::Format(LOCTEXT("AppliedDiagnostics", "마지막 적용 결과 · {0}"), DiagnosticText);
            }
            if (Diagnostics.bTruncated || Diagnostics.Issues.Num() > MaximumDiagnosticRows)
            {
                DiagnosticText = FText::Format(LOCTEXT("DiagnosticsCapped", "{0}\n진단 구간은 최대 {1}개 표시하며, 일부 구간은 생략됩니다. 전체 쌍 집계는 유지되며 Zone별 수는 표시된 구간 기준입니다."),
                    DiagnosticText, FText::AsNumber(MaximumDiagnosticRows));
            }
        }

        void CachePreviewSegments(bool bIncludeBefore = true)
        {
            PreviewSegments.Reset();
            PreviewSegments.Reserve(MaximumPreviewSegments);
            bPreviewTruncated = false;
            DiagnosticSegmentCount = 0;
            const auto AppendSegment = [this](const FVector& A, const FVector& B, const FLinearColor& Color, float Thickness)
            {
                if (!IsFinitePreviewPoint(A) || !IsFinitePreviewPoint(B)) { return; }
                if (PreviewSegments.Num() == MaximumPreviewSegments)
                {
                    bPreviewTruncated = true;
                    return;
                }
                PreviewSegments.Add({ A, B, Color, Thickness });
            };
            const auto AppendRing = [this, &AppendSegment](const TArray<FVector>& Ring, const FLinearColor& Color, float Thickness)
            {
                if (Ring.Num() < 3) { return; }
                for (int32 Index = 0; Index < Ring.Num(); ++Index)
                {
                    if (PreviewSegments.Num() == MaximumPreviewSegments)
                    {
                        bPreviewTruncated = true;
                        return;
                    }
                    AppendSegment(Ring[Index], Ring[(Index + 1) % Ring.Num()], Color, Thickness);
                }
            };

            // Residuals first: a large selection must not hide the spans that still need review.
            const TArray<FFFZASelectedWeldZone>& Zones = Session->GetZones();
            for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Session->GetDiagnostics().Issues)
            {
                if (PreviewSegments.Num() == MaximumPreviewSegments) { bPreviewTruncated = true; break; }
                if (!Zones.IsValidIndex(Issue.PolygonIndexA)) { continue; }
                const TArray<FVector>& RingA = Zones[Issue.PolygonIndexA].After;
                const FVector A = AtBoundaryHeight(Issue.StartA, RingA);
                const FVector B = AtBoundaryHeight(Issue.EndA, RingA);
                if (Issue.Type == FFZAWeldDiagnostics::EIssueType::Gap)
                {
                    if (!Zones.IsValidIndex(Issue.PolygonIndexB)) { continue; }
                    const TArray<FVector>& RingB = Zones[Issue.PolygonIndexB].After;
                    const FVector C = AtBoundaryHeight(Issue.StartB, RingB);
                    const FVector D = AtBoundaryHeight(Issue.EndB, RingB);
                    const FLinearColor Color(1.0f, 0.1f, 0.8f);
                    AppendSegment(A, B, Color, 3.0f);
                    AppendSegment(C, D, Color, 3.0f);
                    AppendSegment((A + B) * 0.5, (C + D) * 0.5, Color, 2.0f);
                }
                else
                {
                    AppendSegment(A, B, Issue.Type == FFZAWeldDiagnostics::EIssueType::Overlap
                        ? FLinearColor(0.75f, 0.2f, 1.0f) : FLinearColor(1.0f, 0.1f, 0.1f), 3.0f);
                }
            }
            DiagnosticSegmentCount = PreviewSegments.Num();
            for (const FFFZASelectedWeldZone& Zone : Zones)
            {
                if (PreviewSegments.Num() == MaximumPreviewSegments) { bPreviewTruncated = true; break; }
                if (Zone.bChanged)
                {
                    if (bIncludeBefore) { AppendRing(Zone.Before, FLinearColor(1.0f, 0.2f, 0.2f), 1.5f); }
                    AppendRing(Zone.After, FLinearColor(0.0f, 0.9f, 1.0f), 2.0f);
                }
            }
        }

        void DrawPreview(UCanvas* Canvas, APlayerController*)
        {
            if (!bShowPreview || PreviewSegments.IsEmpty() || !Canvas || !Canvas->Canvas || !Canvas->SceneView
                || !Canvas->SceneView->Family || !Canvas->SceneView->Family->Scene
                || Canvas->SceneView->Family->Scene->GetWorld() != ExpectedWorld.Get())
            {
                return;
            }
            // A bounded projection pass over the cached preview. No actor/world scan or weld work.
            const auto DrawSegment = [Canvas](const FPreviewSegment& Segment)
            {
                if (Canvas->SceneView->WorldToScreen(Segment.A).W <= 0.0
                    || Canvas->SceneView->WorldToScreen(Segment.B).W <= 0.0) { return; }
                const FVector A = Canvas->Project(Segment.A, false);
                const FVector B = Canvas->Project(Segment.B, false);
                FCanvasLineItem Line(FVector2D(A.X, A.Y), FVector2D(B.X, B.Y));
                Line.SetColor(Segment.Color);
                Line.LineThickness = Segment.Thickness;
                Canvas->Canvas->DrawItem(Line);
            };
            // Cache residuals first to prioritize them at the cap, but draw them last so the
            // ordinary result outlines cannot cover the remaining problem markers.
            for (int32 Index = DiagnosticSegmentCount; Index < PreviewSegments.Num(); ++Index) { DrawSegment(PreviewSegments[Index]); }
            for (int32 Index = 0; Index < DiagnosticSegmentCount; ++Index) { DrawSegment(PreviewSegments[Index]); }
        }

        TWeakObjectPtr<UWorld> ExpectedWorld;
        TWeakPtr<SWindow> Window;
        TSharedPtr<FFFZASelectedWeldSession> Session;
        TSharedPtr<SListView<TSharedPtr<FZoneOption>>> ZoneList;
        TSharedPtr<SListView<TSharedPtr<FDiagnosticOption>>> DiagnosticList;
        TArray<TSharedPtr<FZoneOption>> ZoneRows;
        TArray<FPreviewSegment> PreviewSegments;
        TArray<TSharedPtr<FDiagnosticOption>> DiagnosticRows;
        FDelegateHandle DrawHandle;
        FDelegateHandle ObjectModifiedHandle;
        FDelegateHandle ActorAddedHandle;
        FDelegateHandle ActorDeletedHandle;
        TSet<TWeakObjectPtr<UObject>> WatchedObjects;
        FText StatusText;
        FText ScopeText;
        FText DiagnosticText;
        int32 IgnoredActorCount = 0;
        int32 SupportedZoneCount = 0;
        int32 DiagnosticSegmentCount = 0;
        float ToleranceCm = 50.0f;
        bool bShowPreview = true;
        bool bPreviewTruncated = false;
        bool bHasDisplayedResult = false;
        bool bApplying = false;
        bool bSelectedOnly = false;
        bool bScopeNeedsRecapture = false;
    };
}

void FFZASelectedWeldTool::Open(UWorld* ExpectedWorld, float InitialToleranceCm)
{
    if (!IsEditableWorld(ExpectedWorld) || !FSlateApplication::IsInitialized()) { return; }
    if (TSharedPtr<SWindow> Window = ActiveWindow.Pin())
    {
        Window->BringToFront(true);
        return;
    }
    TSharedRef<SSelectedWeldPanel> Panel = SNew(SSelectedWeldPanel)
        .ExpectedWorld(ExpectedWorld).ToleranceCm(InitialToleranceCm);
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("WindowTitle", "Zone 경계 자동 연결"))
        .ClientSize(FVector2D(960, 720))
        .SupportsMaximize(true).SupportsMinimize(false)
        [ Panel ];
    Panel->SetWindow(Window);
    ActiveWindow = Window;
    ActivePanel = Panel;
    Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([](const TSharedRef<SWindow>&)
    {
        if (TSharedPtr<SSelectedWeldPanel> Panel = ActivePanel.Pin()) { Panel->StopPreviewDrawing(); }
        ActivePanel.Reset();
        ActiveWindow.Reset();
    }));
    FSlateApplication::Get().AddWindow(Window);
}

void FFZASelectedWeldTool::RegisterMenus(void* Owner)
{
    FToolMenuOwnerScoped OwnerScoped(Owner);
    // UE 5.7 uses a separate context menu for Scene Outliner actor selection.
    for (const FName MenuName : { FName(TEXT("LevelEditor.ActorContextMenu")), FName(TEXT("LevelEditor.SceneOutlinerContextMenu")) })
    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
        Menu->FindOrAddSection(TEXT("FFZoneAuthoring"), LOCTEXT("MenuSection", "Zone Authoring"))
            .AddMenuEntry(TEXT("FFZASelectedBoundaryWeld"), LOCTEXT("MenuEntry", "Zone 경계 자동 연결"),
                LOCTEXT("MenuTooltip", "현재 월드에 로드된 전체 Zone의 공통 경계를 자동으로 만들고 틈과 겹침을 정리합니다. 선택한 Zone만 처리하는 범위도 고를 수 있습니다."),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Edit")),
                FUIAction(FExecuteAction::CreateLambda([]
                {
                    FFZASelectedWeldTool::Open(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr, 50.0f);
                }), FCanExecuteAction::CreateStatic(&CanOpenZoneWeld)));
    }
}

void FFZASelectedWeldTool::HandleUndoRedo()
{
    AppliedSessions.RemoveAll([](const TSharedPtr<FFFZASelectedWeldSession>& Session)
    {
        return !Session || !Session->GetWorld();
    });
    for (auto It = AppliedGeometryWorlds.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || !It.Value().IsValid()) { It.RemoveCurrent(); }
    }
    for (const TSharedPtr<FFFZASelectedWeldSession>& Session : AppliedSessions)
    {
        if (UndoRedoDirtyWorlds.Contains(Session->GetWorld()))
        {
            Session->RefreshDerivedData();
            RememberAppliedGeometry(*Session);
        }
    }
    UndoRedoDirtyWorlds.Reset();
    if (TSharedPtr<SSelectedWeldPanel> Panel = ActivePanel.Pin()) { Panel->HandleUndoRedo(); }
}

void FFZASelectedWeldTool::HandleObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
    if (!Object || AppliedSessions.IsEmpty() || Event.GetEventType() != ETransactionObjectEventType::UndoRedo) { return; }

    const TWeakObjectPtr<UWorld>* DependencyWorld = AppliedGeometryWorlds.Find(Object);
    UWorld* ChangedWorld = DependencyWorld ? DependencyWorld->Get() : nullptr;
    if (!ChangedWorld)
    {
        AFFZSGameplayZone* Zone = Cast<AFFZSGameplayZone>(Object);
        if (!Zone)
        {
            const USceneComponent* Component = Cast<USceneComponent>(Object);
            Zone = Component ? Cast<AFFZSGameplayZone>(Component->GetOwner()) : nullptr;
            if (!Zone || (Component != Zone->BoundarySpline && Component != Zone->GetRootComponent())) { return; }
        }
        ChangedWorld = Zone->GetWorld();
    }

    // Only source geometry, its ownership/lifetime, or opaque serialized geometry changes
    // invalidate derived data. A label/name-only rename and unrelated asset Undo do not.
    bool bGeometryChanged = Event.HasPendingKillChange() || Event.HasOuterChange() || Event.HasNonPropertyChanges(true);
    if (!bGeometryChanged)
    {
        static const TSet<FName> GeometryProperties = []
        {
            TSet<FName> Properties =
            {
                TEXT("bClosedLoop"), TEXT("bLoopPositionOverride"), TEXT("LoopPosition"),
                TEXT("RelativeLocation"), TEXT("RelativeRotation"), TEXT("RelativeScale3D"),
                TEXT("bAbsoluteLocation"), TEXT("bAbsoluteRotation"), TEXT("bAbsoluteScale"),
                TEXT("AttachParent"), TEXT("AttachSocketName"),
                TEXT("BoundarySpline"), TEXT("SceneRoot"), TEXT("RootComponent"), TEXT("ZoneId"), TEXT("VerticalExtentCm")
            };
            // UE 5.7 can serialize either the new Spline or legacy SplineCurves backing store.
            Properties.Append(USplineComponent::GetSplinePropertyNames());
            return Properties;
        }();
        for (const FName ChangedProperty : Event.GetChangedProperties())
        {
            if (GeometryProperties.Contains(ChangedProperty)) { bGeometryChanged = true; break; }
        }
    }
    if (bGeometryChanged)
    {
        // Most recent session per world can rebuild any older weld scope in that same world.
        // The set coalesces all transacted spline/actor notifications into one rebuild.
        UndoRedoDirtyWorlds.Add(ChangedWorld);
    }
}

void FFZASelectedWeldTool::Shutdown()
{
    if (TSharedPtr<SSelectedWeldPanel> Panel = ActivePanel.Pin()) { Panel->StopPreviewDrawing(); }
    if (TSharedPtr<SWindow> Window = ActiveWindow.Pin())
    {
        Window->SetOnWindowClosed(FOnWindowClosed());
        Window->SetContent(SNullWidget::NullWidget);
        if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().DestroyWindowImmediately(Window.ToSharedRef()); }
    }
    ActiveWindow.Reset();
    ActivePanel.Reset();
    AppliedSessions.Reset();
    UndoRedoDirtyWorlds.Reset();
    AppliedGeometryWorlds.Reset();
}

#undef LOCTEXT_NAMESPACE
