// FF_FFZSNavigationEditorLibrary.h
// 역할: NavMesh 제작 전에 에셋 컴파일과 물리 등록 완료를 보장한다.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_FFZSNavigationEditorLibrary.generated.h"

class UStaticMesh;

UCLASS()
class FF_ZONESYSTEMEDITOR_API UFFZSNavigationEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 맵 로딩 직후에도 바닥 메시의 컴파일·물리 등록을 마친 뒤 내비게이션을 만들게 한다.
    // 대기 시간 추측 없이 엔진의 완료 API를 사용하며 에디터 월드에서만 실행한다.
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Editor|Navigation")
    static bool PrepareWorldForNavigation(UWorld* World);

    // 연결이 끊긴 경계를 조사하는 읽기 전용 데이터다. 연속된 두 점이 경계 선분 하나다.
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Editor|Navigation")
    static TArray<FVector> GetNavigationBoundaryEdges(UWorld* World);

    // ini 문자열이 아닌 현재 NavData의 실제 A* 탐색 한도를 읽는다.
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Editor|Navigation")
    static int32 GetNavigationQueryNodeLimit(UWorld* World);

    // 에디터 메모리에서만 충돌 캐시를 재생성한다. 에셋·맵을 저장하지 않는다.
    // Python에서 원래 BodySetup 속성을 복원한 뒤에도 같은 경로로 캐시를 갱신한다.
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Editor|Navigation")
    static bool RefreshStaticMeshCollisionForNavigation(UWorld* World, UStaticMesh* Mesh);

    // 단일 convex가 계단·통로를 덮는지 비교하는 에디터 실험용이다.
    // 실제 삼각형 충돌로 전환하며, 선택하면 기존 단순 충돌도 메모리에서 제거한다.
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Editor|Navigation")
    static bool ApplyTriangleCollisionForNavigation(UWorld* World, UStaticMesh* Mesh, bool bRemoveSimpleCollision = true);
};
