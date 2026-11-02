// FF_FFZSNavigationEditorLibrary.cpp
// 역할: 바닥 메시가 준비되지 않은 상태로 내비게이션을 만들거나 검사하는 경합을 막는다.
#include "FF_FFZSNavigationEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/UObjectIterator.h"
#include "AI/Navigation/NavQueryFilter.h"

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

int32 UFFZSNavigationEditorLibrary::GetNavigationQueryNodeLimit(UWorld* World)
{
    UNavigationSystemV1* Navigation = World
        ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    const ANavigationData* Data = Navigation
        ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
    return Data && Data->GetDefaultQueryFilter().IsValid()
        ? static_cast<int32>(Data->GetDefaultQueryFilter()->GetMaxSearchNodes()) : -1;
}

bool UFFZSNavigationEditorLibrary::RefreshStaticMeshCollisionForNavigation(UWorld* World, UStaticMesh* Mesh)
{
    if (!IsValid(Mesh) || !PrepareWorldForNavigation(World) || !Mesh->GetBodySetup())
    {
        return false;
    }

    UBodySetup* Body = Mesh->GetBodySetup();
    Body->InvalidatePhysicsData();
    Body->CreatePhysicsMeshes();
    // 재바인딩만으로는 기존 UNavCollision의 cooked convex 캐시가 유지된다.
    Mesh->CreateNavCollision(true);
    for (UStaticMeshComponent* Component : TObjectRange<UStaticMeshComponent>())
    {
        if (Component->GetWorld() == World && Component->GetStaticMesh() == Mesh && Component->IsRegistered())
        {
            Component->RecreatePhysicsState();
            Component->UpdateBounds();
            FNavigationSystem::UpdateComponentData(*Component);
        }
    }
    return PrepareWorldForNavigation(World)
        && (Body->GetCollisionTraceFlag() != CTF_UseComplexAsSimple || !Body->TriMeshGeometries.IsEmpty());
}

bool UFFZSNavigationEditorLibrary::ApplyTriangleCollisionForNavigation(UWorld* World, UStaticMesh* Mesh,
    bool bRemoveSimpleCollision)
{
    if (!IsValid(Mesh) || !PrepareWorldForNavigation(World) || !Mesh->GetBodySetup())
    {
        return false;
    }
    // ComplexAsSimple은 시뮬레이션 강체에 사용할 수 없다. 정적인 레벨 지형만 허용한다.
    for (const UStaticMeshComponent* Component : TObjectRange<UStaticMeshComponent>())
    {
        if (Component->GetWorld() == World && Component->GetStaticMesh() == Mesh
            && Component->IsSimulatingPhysics())
        {
            return false;
        }
    }
    UBodySetup* Body = Mesh->GetBodySetup();
    Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
    if (bRemoveSimpleCollision)
    {
        // Recast는 complex 모드에서도 남은 AggGeom 요소를 함께 내보낸다.
        Body->AggGeom.EmptyElements();
    }
    return RefreshStaticMeshCollisionForNavigation(World, Mesh);
}
