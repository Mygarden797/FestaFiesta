#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_BuildPlacementGizmo.generated.h"

class APlayerController;
class ULineBatchComponent;

enum class EFF_BuildGizmoHandle : uint8
{
	None,
	MoveX,
	MoveY,
	RotateYaw
};

/** The build subsystem retains input ownership. No Slate capture or authority calls occur here. */
struct FFF_BuildGizmoPointerInput
{
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	bool bPrimaryPressed = false;
	bool bPrimaryDown = false;
	bool bPrimaryReleased = false;
	bool bCancel = false;
	bool bBlockedByUi = false;
	bool bPaused = false;
};

/** Absolute change since bStarted, not an increment. The caller owns snapping and validation. */
struct FFF_BuildGizmoDelta
{
	FVector WorldTranslation = FVector::ZeroVector;
	float YawDegrees = 0.0f;
	bool bConsumed = false;
	bool bStarted = false;
	bool bEnded = false;
	bool bCancelled = false;
	bool bChanged = false;
};

namespace FFBuildGizmoMath
{
	struct FSegment
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		EFF_BuildGizmoHandle Handle = EFF_BuildGizmoHandle::None;
	};

	struct FProjectedSegment
	{
		FVector2D Start = FVector2D::ZeroVector;
		FVector2D End = FVector2D::ZeroVector;
		EFF_BuildGizmoHandle Handle = EFF_BuildGizmoHandle::None;
	};

	FESTAFIESTA_API bool IntersectGroundPlane(const FVector& RayOrigin, const FVector& RayDirection,
		const FVector& PlanePoint, const FVector& PlaneNormal, FVector& OutPoint);
	FESTAFIESTA_API double DistanceToSegmentSquared(const FVector2D& Point,
		const FVector2D& Start, const FVector2D& End);
	FESTAFIESTA_API EFF_BuildGizmoHandle HitTest(TConstArrayView<FProjectedSegment> Segments,
		const FVector2D& Pointer, float TolerancePixels = 11.0f);
	FESTAFIESTA_API void BuildGeometry(const FVector& Center, const FVector& GroundNormal,
		double AxisLength, TArray<FSegment>& OutSegments);

	/** Pinned plane/center keep changing preview transforms from feeding back into pointer math. */
	struct FESTAFIESTA_API FGesture
	{
		bool Begin(EFF_BuildGizmoHandle Handle, const FVector& Point, const FVector& Center,
			const FVector& PlaneNormal);
		FFF_BuildGizmoDelta Update(const FVector& Point);
		FFF_BuildGizmoDelta End(bool bCancel);
		bool IsActive() const { return ActiveHandle != EFF_BuildGizmoHandle::None; }
		EFF_BuildGizmoHandle GetHandle() const { return ActiveHandle; }
		const FVector& GetPlanePoint() const { return StartPoint; }
		const FVector& GetPlaneNormal() const { return Normal; }

	private:
		EFF_BuildGizmoHandle ActiveHandle = EFF_BuildGizmoHandle::None;
		FVector StartPoint = FVector::ZeroVector;
		FVector Pivot = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		FVector LastTranslation = FVector::ZeroVector;
		double PreviousAngle = 0.0;
		double TotalYaw = 0.0;
	};
}

/** Small shipping-visible, collision-free XY/yaw handles for an explicitly pinned preview. */
UCLASS(Transient, NotBlueprintable)
class FESTAFIESTA_API AFF_BuildPlacementGizmo : public AActor
{
	GENERATED_BODY()

public:
	AFF_BuildPlacementGizmo();
	// PlacementTransform is already centered by ZoneSystem; do not add another footprint offset.
	void SetPreview(APlayerController* InController, const FTransform& PreviewTransform,
		FIntPoint FootprintCm, FVector GroundPoint, FVector GroundNormal);
	void SetGizmoVisible(bool bVisible);
	FFF_BuildGizmoDelta HandlePointer(const FFF_BuildGizmoPointerInput& Input);
	void CancelInteraction();
	bool IsInteracting() const { return Gesture.IsActive(); }
	EFF_BuildGizmoHandle GetActiveHandle() const { return Gesture.GetHandle(); }
	bool IsGizmoVisible() const { return bRequestedVisible && bFrameValid && !IsHidden(); }
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void SetActorHiddenInGame(bool bNewHidden) override;

private:
	friend class FFF_BuildPlacementGizmoLifecycleTest;
	void RefreshGeometry();
	void FlushPresentation();
	bool ResolvePointerPoint(const FVector2D& ScreenPosition, FVector& OutPoint) const;

	UPROPERTY(VisibleAnywhere, Transient)
	TObjectPtr<ULineBatchComponent> Lines;
	TWeakObjectPtr<APlayerController> Controller;
	TArray<FFBuildGizmoMath::FProjectedSegment> ProjectedSegments;
	FFBuildGizmoMath::FGesture Gesture;
	FVector Center = FVector::ZeroVector;
	FVector GroundNormal = FVector::UpVector;
	EFF_BuildGizmoHandle HoveredHandle = EFF_BuildGizmoHandle::None;
	bool bRequestedVisible = false;
	bool bFrameValid = false;
	bool bPendingCancellation = false;
};
