#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FF_ZoneSystemPlaytestOverviewPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class USceneComponent;
class USpringArmComponent;

/**
 * ZoneTestMap에서만 사용하는 충돌 없는 전체 관찰 카메라 Pawn이다.
 * 시설이나 지형에 막히지 않고 Zone 위를 이동하며 확대, 축소와 시점 회전을 담당한다.
 */
UCLASS(NotBlueprintable, Transient)
class FESTAFIESTA_API AFF_ZoneSystemPlaytestOverviewPawn final : public APawn
{
    GENERATED_BODY()

public:
    AFF_ZoneSystemPlaytestOverviewPawn();

    virtual void Tick(float DeltaSeconds) override;

    /** Zone 중심과 경계 크기를 받아 처음 한 화면에 들어오는 카메라 구도를 설정한다. */
    void InitializeOverview(const FVector& FocusLocation, float SuggestedCameraDistance);

    /** Tab으로 HUD 조작 모드에 들어갈 때 카메라 이동 입력을 일시 정지한다. */
    void SetUiInteractionMode(bool bEnabled);
    bool IsUiInteractionMode() const { return bUiInteractionMode; }

    /** 헤드리스 스모크 테스트에서 하향 관찰 카메라가 실제로 활성화됐는지 확인한다. */
    bool HasActiveOverviewCamera() const;

private:
    UPROPERTY(VisibleAnywhere, Category = "Zone Playtest")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Zone Playtest")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, Category = "Zone Playtest")
    TObjectPtr<UCameraComponent> OverviewCamera;

    UPROPERTY(VisibleAnywhere, Category = "Zone Playtest")
    TObjectPtr<UFloatingPawnMovement> FloatingMovement;

    // Home 키로 전체 보기 구도를 복원할 때 사용하는 최초 중심과 거리다.
    FVector InitialFocusLocation = FVector::ZeroVector;
    float InitialCameraDistance = 12000.0f;
    bool bUiInteractionMode = false;

    void ResetOverview();
    void AdjustZoom(float ZoomMultiplier);
};

