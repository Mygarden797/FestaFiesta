// A fixed captured-Zone snapshot, read-only shared-seam preview, and atomic editor writeback.
// Slate owns this session. No selection queries, actor creation, or asset saves occur here.
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "FFZALayoutOps.h"
#include "FFZAWeldDiagnostics.h"

class AFFZSGameplayZone;
class USplineComponent;
class ULevel;
class UWorld;

// Plain geometry state shared by the apply gate and headless tests. Exact comparisons reject
// even small edits after a preview; the weld search tolerance never weakens this freshness check.
struct FFFZASelectedWeldGeometrySnapshot
{
    FTransform ActorTransform = FTransform::Identity;
    FTransform SplineTransform = FTransform::Identity;
    TArray<FVector> Points;
    TArray<uint8> PointTypes;
    bool bClosedLoop = false;

    bool Matches(const FFFZASelectedWeldGeometrySnapshot& Other) const;
};

struct FFFZASelectedWeldZone
{
    TWeakObjectPtr<AFFZSGameplayZone> Zone;
    FString Label;
    FString ActorPath;
    FName ZoneId;
    bool bSupported = false;
    bool bChanged = false;
    bool bReference = false;
    bool bAutoReference = false;
    int32 GroupIndex = INDEX_NONE;
    bool bDisconnected = false;
    bool bRejected = false;
    bool bInvalidInput = false;
    FString Issue;
    TArray<FVector> Before;
    TArray<FVector> After;
    TArray<double> SourcePointKeys;
    int32 MovedPointCount = 0;
    int32 InsertedPointCount = 0;
    int32 RemovedPointCount = 0;
    double MaximumMoveCm = 0.0;
    int32 ResidualGapCount = 0;
    int32 ResidualOverlapCount = 0;
};

class FFFZASelectedWeldSession
{
public:
    explicit FFFZASelectedWeldSession(const TArray<AFFZSGameplayZone*>& SelectedZones,
        float InAnalysisCellSizeCm = 100.0f);

    const TArray<FFFZASelectedWeldZone>& GetZones() const { return Zones; }
    UWorld* GetWorld() const;
    int32 GetReferenceIndex() const { return ReferenceIndex; }
    bool HasPreview() const { return bHasPreview; }
    // A cached predicate for Slate attributes. Deep snapshot checks run only on Preview/Apply.
    bool CanApply() const
    {
        return bHasPreview && (Stats.ChangedPolygonCount > 0 || Diagnostics.GapPairCount > 0 ||
            Diagnostics.OverlapPairCount > 0 || Stats.RejectedPolygonCount > 0 || Stats.InvalidInputPolygonCount > 0 ||
            (!bAutomaticRepairComplete && !RepairFailureReason.IsEmpty()));
    }
    const FFZALayoutOps::FWeldStats& GetStats() const { return Stats; }
    const FFZAWeldDiagnostics::FResult& GetDiagnostics() const { return Diagnostics; }
    const FText& GetStatusText() const { return StatusText; }
    bool IsAutomaticRepairComplete() const { return bAutomaticRepairComplete; }
    bool WasRepairRadiusExpanded() const { return bExpandedRepairRadius; }
    bool WasGapAreaRecoveryUsed() const { return bGapAreaRecoveryUsed; }
    double GetRepairRadiusCm() const { return RepairRadiusCm; }
    const FString& GetRepairFailureReason() const { return RepairFailureReason; }

    bool BuildPreview(int32 InReferenceIndex, double ToleranceCm, FText& OutError);
    // Default world-wide workflow: no reference actor is required or held fixed.
    bool BuildPreview(double ToleranceCm, FText& OutError);
    bool Apply(FText& OutMessage);
    void ClearPreview();

    // Refreshes existing generated representations; never creates or saves a level/asset.
    // The editor module also calls this after undo/redo of a successful application.
    void RefreshDerivedData() const;

private:
    struct FSplineSnapshot
    {
        TWeakObjectPtr<USplineComponent> Spline;
        TWeakObjectPtr<ULevel> Level;
        FFFZASelectedWeldGeometrySnapshot Geometry;
    };

    bool CheckEditorWorld(FText& OutError) const;
    bool BuildPreviewInternal(int32 InReferenceIndex, double ToleranceCm, FText& OutError, bool bAutomatic);
    void CaptureGeometry();
    bool CheckSnapshotCurrent(FText& OutError) const;

    TWeakObjectPtr<UWorld> World;
    TArray<FFFZASelectedWeldZone> Zones;
    TArray<FSplineSnapshot> Snapshots;
    FFZALayoutOps::FWeldStats Stats;
    FFZAWeldDiagnostics::FResult Diagnostics;
    FText StatusText;
    int32 ReferenceIndex = INDEX_NONE;
    float AnalysisCellSizeCm = 100.0f;
    bool bHasPreview = false;
    bool bAutomaticRepairComplete = false;
    bool bExpandedRepairRadius = false;
    bool bGapAreaRecoveryUsed = false;
    double RepairRadiusCm = 0.0;
    FString RepairFailureReason;
};
