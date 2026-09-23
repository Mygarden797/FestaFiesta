// FF_FFZSNavigationEditorLibrary.h
// 역할: NavMesh 제작 전에 에셋 컴파일과 물리 등록 완료를 보장한다.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_FFZSNavigationEditorLibrary.generated.h"

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
};
