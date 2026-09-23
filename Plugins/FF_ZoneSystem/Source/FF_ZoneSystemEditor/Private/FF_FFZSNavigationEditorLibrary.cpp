// FF_FFZSNavigationEditorLibrary.cpp
// 역할: 바닥 메시가 준비되지 않은 상태로 내비게이션을 만들거나 검사하는 경합을 막는다.
#include "FF_FFZSNavigationEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

bool UFFZSNavigationEditorLibrary::PrepareWorldForNavigation(UWorld* World)
{
    if (!IsValid(World) || World->WorldType != EWorldType::Editor || !World->GetPhysicsScene())
    {
        return false;
    }

    FAssetCompilingManager::Get().FinishAllCompilation();
    World->GetPhysicsScene()->ProcessDeferredCreatePhysicsState();
    World->GetPhysicsScene()->ProcessAsyncPhysicsStateJobs(true);
    World->SendAllEndOfFrameUpdates();
    return true;
}

TArray<FVector> UFFZSNavigationEditorLibrary::GetNavigationBoundaryEdges(UWorld* World)
{
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    const ARecastNavMesh* NavMesh = Navigation
        ? Cast<ARecastNavMesh>(Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
    TArray<FVector> Edges;
    if (NavMesh && NavMesh->HasValidNavmesh())
    {
        FRecastDebugGeometry Geometry;
        Geometry.bGatherNavMeshEdges = true;
        // 빈 TileRef는 모든 타일을 조사하는 엔진 공개 API의 지정 방식이다.
        if (NavMesh->GetDebugGeometryForTile(Geometry, FNavTileRef()))
        {
            Edges = MoveTemp(Geometry.NavMeshEdges);
        }
    }
    return Edges;
}
