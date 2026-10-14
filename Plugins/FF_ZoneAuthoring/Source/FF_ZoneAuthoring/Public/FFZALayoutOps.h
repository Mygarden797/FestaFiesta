// FFZALayoutOps.h
// 역할: 여러 Zone 경계를 한꺼번에 다듬는 순수 계산(레이아웃 매니저 버튼이 사용).
#pragma once

#include "CoreMinimal.h"

namespace FFZALayoutOps
{
    struct FF_ZONEAUTHORING_API FWeldGroup
    {
        // Original caller-array indices. Includes singletons and rejected members.
        TArray<int32> PolygonIndices;
        int32 ReferencePolygonIndex = INDEX_NONE;
        bool bUsesManualReference = false;
    };

    struct FF_ZONEAUTHORING_API FWeldStats
    {
        int32 MovedVertexCount = 0;
        int32 InsertedVertexCount = 0;
        // Boolean overlap repair may remove original corners. Ordinary welding leaves this zero.
        int32 RemovedVertexCount = 0;
        int32 ChangedPolygonCount = 0;
        int32 RejectedPolygonCount = 0;
        int32 InvalidInputPolygonCount = 0;
        // Largest XY displacement from authored geometry, including inserted seam points.
        double MaximumMoveCm = 0.0;

        // Indices always refer to the caller's original array, including selected-mode diagnostics.
        TArray<int32> RejectedPolygonIndices;
        TArray<int32> InvalidInputPolygonIndices;
        TArray<int32> DisconnectedPolygonIndices;
        bool bInvalidReference = false;

        // Grouped mode processes the manual reference's component first, then all remaining
        // eligible components with a deterministic automatic reference. Empty in legacy modes.
        TArray<FWeldGroup> Groups;

        // Output point keys into each ORIGINAL closed ring. Retained points keep integer indices;
        // inserts on edge i have key i + alpha, including the closing edge. Preserve/interpolate
        // spline height and other attributes using these keys, without nearest-edge guesses.
        TArray<TArray<double>> SourcePointKeys;
    };

    // Match nearby overlapping edges with opposing polygon interiors, independent of winding.
    // Earlier rings keep their shape. Later rings follow reference corners; both sides gain exact
    // shared nodes at partial endpoints and T junctions. Invalid/collapsed/self-intersecting proposals
    // are rejected atomically. OutChangedPolygons includes reference rings that only gained nodes.
    // The return value remains MOVED EXISTING vertices; stats reports inserted vertices separately.
    FF_ZONEAUTHORING_API int32 WeldPolygons(TArray<TArray<FVector2D>>& InOutPolygons, double ToleranceCm,
        TArray<bool>& OutChangedPolygons, FWeldStats* OutStats = nullptr);

    // Caller supplies ONLY the selected rings and explicitly chooses the fixed reference. Process
    // its connected component in deterministic geometric order; disconnected rings stay untouched.
    // Previously accepted reference geometry stays fixed (only exact shared nodes may be inserted).
    // The manual reference is the first anchor; other accepted neighbours own nearer exposed spans.
    // Conflicting/overlapping proposals are
    // rejected; invalid reference indices or geometry perform no changes. All output indices and
    // provenance remain aligned with the original input order, regardless of traversal order.
    FF_ZONEAUTHORING_API int32 WeldSelectedPolygons(TArray<TArray<FVector2D>>& InOutPolygons,
        int32 ReferencePolygonIndex, double ToleranceCm, TArray<bool>& OutChangedPolygons,
        FWeldStats* OutStats = nullptr);

    // Process EVERY connected component of the selected rings. Connectivity requires actual
    // opposing, overlapping edge spans within the tolerance; nearby bounds alone do not connect.
    // The preferred reference stays fixed in its component. Each other component fixes its
    // largest-area ring (geometric tie-breaks), including unchanged singleton components.
    // Invalid preferred references abort without changes; other invalid rings are excluded.
    // Rejected targets never become anchors, and cannot affect independent components.
    // Output provenance and diagnostics use the caller's original array order.
    FF_ZONEAUTHORING_API int32 WeldSelectedPolygonGroups(TArray<TArray<FVector2D>>& InOutPolygons,
        int32 PreferredReferencePolygonIndex, double ToleranceCm, TArray<bool>& OutChangedPolygons,
        FWeldStats* OutStats = nullptr);
}
