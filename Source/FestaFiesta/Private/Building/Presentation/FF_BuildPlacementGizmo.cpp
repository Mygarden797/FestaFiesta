// FF_BuildPlacementGizmo.cpp
// 역할: 마우스로 잡아 움직이는 시설 이동 화살표와 회전 손잡이를 처리한다.

#include "Building/Presentation/FF_BuildPlacementGizmo.h"

#include "Components/LineBatchComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "SceneManagement.h"
#include "SceneView.h"

namespace FFBuildGizmoMath
{
	bool IntersectGroundPlane(const FVector& RayOrigin, const FVector& RayDirection,
		const FVector& PlanePoint, const FVector& PlaneNormal, FVector& OutPoint)
	{
		if (RayOrigin.ContainsNaN() || RayDirection.ContainsNaN() || PlanePoint.ContainsNaN() ||
			PlaneNormal.ContainsNaN() || RayDirection.IsNearlyZero() || PlaneNormal.IsNearlyZero()) return false;
		const FVector Direction = RayDirection.GetSafeNormal();
		const FVector Normal = PlaneNormal.GetSafeNormal();
		const double Denominator = FVector::DotProduct(Direction, Normal);
		// Reject a grazing ray instead of producing an enormous facility jump.
		if (FMath::Abs(Denominator) < 0.025) return false;
		const double Distance = FVector::DotProduct(PlanePoint - RayOrigin, Normal) / Denominator;
		if (!FMath::IsFinite(Distance) || Distance < 0.0 || Distance > 500000.0) return false;
		OutPoint = RayOrigin + Direction * Distance;
		return !OutPoint.ContainsNaN();
	}

	double DistanceToSegmentSquared(const FVector2D& Point, const FVector2D& Start, const FVector2D& End)
	{
		if (Point.ContainsNaN() || Start.ContainsNaN() || End.ContainsNaN()) return TNumericLimits<double>::Max();
		const FVector2D Segment = End - Start;
		const double LengthSquared = Segment.SizeSquared();
		if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER) return FVector2D::DistSquared(Point, Start);
		const double Alpha = FMath::Clamp(FVector2D::DotProduct(Point - Start, Segment) / LengthSquared, 0.0, 1.0);
		return FVector2D::DistSquared(Point, Start + Segment * Alpha);
	}

	EFF_BuildGizmoHandle HitTest(TConstArrayView<FProjectedSegment> Segments,
		const FVector2D& Pointer, const float TolerancePixels)
	{
		if (Pointer.ContainsNaN() || !FMath::IsFinite(TolerancePixels) || TolerancePixels <= 0.0f)
			return EFF_BuildGizmoHandle::None;
		double BestDistanceSquared = FMath::Square(static_cast<double>(TolerancePixels));
		EFF_BuildGizmoHandle Result = EFF_BuildGizmoHandle::None;
		for (const FProjectedSegment& Segment : Segments)
		{
			if (Segment.Handle == EFF_BuildGizmoHandle::None) continue;
			const double DistanceSquared = DistanceToSegmentSquared(Pointer, Segment.Start, Segment.End);
			// Axes are emitted before the ring and retain ownership at a shared intersection.
			if (DistanceSquared <= BestDistanceSquared &&
				(Result == EFF_BuildGizmoHandle::None || DistanceSquared < BestDistanceSquared - 0.01))
			{
				BestDistanceSquared = DistanceSquared;
				Result = Segment.Handle;
			}
		}
		return Result;
	}

	void BuildGeometry(const FVector& Center, const FVector& GroundNormal,
		const double AxisLength, TArray<FSegment>& OutSegments)
	{
		OutSegments.Reset();
		if (Center.ContainsNaN() || GroundNormal.ContainsNaN() || GroundNormal.Z < 0.25 ||
			!FMath::IsFinite(AxisLength) || AxisLength <= 0.0) return;
		// Both arrows retain their world X/Y meaning on sloping ground.
		const FVector X = FVector(1.0, 0.0, -GroundNormal.X / GroundNormal.Z).GetSafeNormal();
		const FVector Y = FVector(0.0, 1.0, -GroundNormal.Y / GroundNormal.Z).GetSafeNormal();
		OutSegments.Reserve(57);
		const auto Append = [&](const FVector& A, const FVector& B, EFF_BuildGizmoHandle Handle)
		{
			OutSegments.Add({A, B, Handle});
		};
		const auto Arrow = [&](const FVector& Direction, const FVector& Side, EFF_BuildGizmoHandle Handle)
		{
			const FVector Tip = Center + Direction * AxisLength;
			Append(Center + Direction * AxisLength * 0.15, Tip, Handle);
			Append(Tip, Tip - Direction * AxisLength * 0.18 + Side * AxisLength * 0.09, Handle);
			Append(Tip, Tip - Direction * AxisLength * 0.18 - Side * AxisLength * 0.09, Handle);
		};
		Arrow(X, Y, EFF_BuildGizmoHandle::MoveX);
		Arrow(Y, X, EFF_BuildGizmoHandle::MoveY);
		constexpr int32 RingSegments = 48;
		const double Radius = AxisLength * 0.76;
		for (int32 Index = 0; Index < RingSegments; ++Index)
		{
			const double A = 2.0 * UE_DOUBLE_PI * Index / RingSegments;
			const double B = 2.0 * UE_DOUBLE_PI * (Index + 1) / RingSegments;
			Append(Center + Radius * (X * FMath::Cos(A) + Y * FMath::Sin(A)),
				Center + Radius * (X * FMath::Cos(B) + Y * FMath::Sin(B)), EFF_BuildGizmoHandle::RotateYaw);
		}
		// A small directional arrow makes rotation recognizable without a separate label panel.
		const double Angle = UE_DOUBLE_PI * 1.25;
		const FVector Radial = X * FMath::Cos(Angle) + Y * FMath::Sin(Angle);
		const FVector Tangent = -X * FMath::Sin(Angle) + Y * FMath::Cos(Angle);
		const FVector Tip = Center + Radius * Radial;
		Append(Tip, Tip - Tangent * AxisLength * 0.14 + Radial * AxisLength * 0.075, EFF_BuildGizmoHandle::RotateYaw);
		Append(Tip, Tip - Tangent * AxisLength * 0.14 - Radial * AxisLength * 0.075, EFF_BuildGizmoHandle::RotateYaw);
	}

	bool FGesture::Begin(const EFF_BuildGizmoHandle Handle, const FVector& Point,
		const FVector& Center, const FVector& PlaneNormal)
	{
		if (IsActive() || Handle == EFF_BuildGizmoHandle::None || Point.ContainsNaN() ||
			Center.ContainsNaN() || PlaneNormal.ContainsNaN() || PlaneNormal.IsNearlyZero()) return false;
		const FVector Offset = Point - Center;
		if (Handle == EFF_BuildGizmoHandle::RotateYaw && Offset.SizeSquared2D() < 1.0) return false;
		ActiveHandle = Handle;
		StartPoint = Point;
		Pivot = Center;
		Normal = PlaneNormal.GetSafeNormal();
		LastTranslation = FVector::ZeroVector;
		PreviousAngle = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
		TotalYaw = 0.0;
		return true;
	}

	FFF_BuildGizmoDelta FGesture::Update(const FVector& Point)
	{
		FFF_BuildGizmoDelta Result;
		Result.bConsumed = IsActive();
		if (!IsActive() || Point.ContainsNaN()) return Result;
		FVector Translation = FVector::ZeroVector;
		if (ActiveHandle == EFF_BuildGizmoHandle::MoveX) Translation.X = Point.X - StartPoint.X;
		else if (ActiveHandle == EFF_BuildGizmoHandle::MoveY) Translation.Y = Point.Y - StartPoint.Y;
		else
		{
			const FVector Offset = Point - Pivot;
			if (Offset.SizeSquared2D() < 1.0) return Result;
			const double Angle = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
			const double Increment = FMath::FindDeltaAngleDegrees(PreviousAngle, Angle);
			PreviousAngle = Angle;
			TotalYaw += Increment;
			Result.bChanged = !FMath::IsNearlyZero(Increment, 0.0001);
		}
		if (ActiveHandle != EFF_BuildGizmoHandle::RotateYaw)
			Result.bChanged = !Translation.Equals(LastTranslation, 0.0001);
		LastTranslation = Translation;
		Result.WorldTranslation = LastTranslation;
		Result.YawDegrees = static_cast<float>(TotalYaw);
		return Result;
	}

	FFF_BuildGizmoDelta FGesture::End(const bool bCancel)
	{
		FFF_BuildGizmoDelta Result;
		Result.bConsumed = IsActive();
		Result.bEnded = IsActive();
		Result.bCancelled = IsActive() && bCancel;
		Result.WorldTranslation = LastTranslation;
		Result.YawDegrees = static_cast<float>(TotalYaw);
		*this = FGesture();
		return Result;
	}
}

AFF_BuildPlacementGizmo::AFF_BuildPlacementGizmo()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bAllowTickBeforeBeginPlay = true;
	SetActorEnableCollision(false);
	SetReplicates(false);
	Lines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("GizmoLines"));
	SetRootComponent(Lines);
	Lines->SetAbsolute(true, true, true);
	Lines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lines->SetGenerateOverlapEvents(false);
	Lines->SetCanEverAffectNavigation(false);
	Lines->PrimaryComponentTick.bCanEverTick = false;
	Lines->bCalculateAccurateBounds = true;
	Super::SetActorHiddenInGame(true);
}

// 선택한 시설의 위치와 바닥 방향을 받아 조작 손잡이의 기준을 정한다.
void AFF_BuildPlacementGizmo::SetPreview(APlayerController* InController,
	const FTransform& PreviewTransform, const FIntPoint FootprintCm,
	const FVector GroundPoint, const FVector InGroundNormal)
{
	Controller = InController;
	bFrameValid = InController && !PreviewTransform.ContainsNaN() && !GroundPoint.ContainsNaN() &&
		!InGroundNormal.ContainsNaN() && InGroundNormal.Z >= 0.25 && FootprintCm.X > 0 && FootprintCm.Y > 0;
	if (!bFrameValid)
	{
		CancelInteraction();
		FlushPresentation();
		return;
	}
	GroundNormal = InGroundNormal.GetSafeNormal();
	Center = PreviewTransform.GetLocation();
	Center.Z = GroundPoint.Z - (GroundNormal.X * (Center.X - GroundPoint.X) +
		GroundNormal.Y * (Center.Y - GroundPoint.Y)) / GroundNormal.Z + 3.0;
	RefreshGeometry();
}

void AFF_BuildPlacementGizmo::SetGizmoVisible(const bool bVisible)
{
	bRequestedVisible = bVisible;
	SetActorHiddenInGame(!bVisible);
	SetActorTickEnabled(bVisible);
	if (bVisible) RefreshGeometry();
}

void AFF_BuildPlacementGizmo::SetActorHiddenInGame(const bool bNewHidden)
{
	Super::SetActorHiddenInGame(bNewHidden);
	if (bNewHidden)
	{
		CancelInteraction();
		FlushPresentation();
	}
}

void AFF_BuildPlacementGizmo::CancelInteraction()
{
	bPendingCancellation |= Gesture.End(true).bCancelled;
	FlushPresentation();
}

void AFF_BuildPlacementGizmo::FlushPresentation()
{
	ProjectedSegments.Reset();
	HoveredHandle = EFF_BuildGizmoHandle::None;
	if (Lines) Lines->Flush();
}

void AFF_BuildPlacementGizmo::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsGizmoVisible() || !Controller.IsValid() || (GetWorld() && GetWorld()->IsPaused()) ||
		(FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()))
	{
		CancelInteraction();
		FlushPresentation();
		return;
	}
	RefreshGeometry();
}

void AFF_BuildPlacementGizmo::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelInteraction();
	FlushPresentation();
	Controller.Reset();
	Super::EndPlay(EndPlayReason);
}

void AFF_BuildPlacementGizmo::Destroyed()
{
	// Editor-preview worlds can destroy actors before BeginPlay/EndPlay ever run.
	CancelInteraction();
	Super::Destroyed();
}

// 시설 크기와 카메라 화면을 고려하여 이동 화살표·회전 원·클릭 범위를 갱신한다.
void AFF_BuildPlacementGizmo::RefreshGeometry()
{
	APlayerController* PC = Controller.Get();
	if (!IsGizmoVisible() || !PC)
	{
		FlushPresentation();
		return;
	}
	int32 Width = 0;
	int32 Height = 0;
	PC->GetViewportSize(Width, Height);
	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	FSceneViewProjectionData Projection;
	if (!LocalPlayer || !LocalPlayer->ViewportClient ||
		!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection))
	{
		FlushPresentation();
		return;
	}
	// Compute the camera matrices once for all segments, including their hit targets.
	const FMatrix ViewProjection = Projection.ComputeViewProjectionMatrix();
	const auto Project = [&](const FVector& World, FVector2D& Screen)
	{
		return FSceneView::ProjectWorldToScreen(World, Projection.GetConstrainedViewRect(), ViewProjection, Screen) &&
			PC->PostProcessWorldToScreen(World, Screen, false) && !Screen.ContainsNaN();
	};
	FVector2D ScreenCenter;
	if (Width <= 0 || Height <= 0 || !Project(Center, ScreenCenter) ||
		ScreenCenter.ContainsNaN() || ScreenCenter.X < 0 || ScreenCenter.X > Width ||
		ScreenCenter.Y < 0 || ScreenCenter.Y > Height)
	{
		FlushPresentation();
		return;
	}
	const FVector X = FVector(1.0, 0.0, -GroundNormal.X / GroundNormal.Z).GetSafeNormal();
	const FVector Y = FVector(0.0, 1.0, -GroundNormal.Y / GroundNormal.Z).GetSafeNormal();
	FVector2D ScreenX, ScreenY;
	if (!Project(Center + X * 100.0, ScreenX) || !Project(Center + Y * 100.0, ScreenY))
	{
		FlushPresentation();
		return;
	}
	const double PixelsPer100Cm = FMath::Max(FVector2D::Distance(ScreenCenter, ScreenX),
		FVector2D::Distance(ScreenCenter, ScreenY));
	if (!FMath::IsFinite(PixelsPer100Cm) || PixelsPer100Cm < 0.01)
	{
		FlushPresentation();
		return;
	}
	// Projected size, not facility size, controls the small manipulator's footprint.
	const double AxisLength = FMath::Clamp(7200.0 / PixelsPer100Cm, 2.0, 20000.0);
	TArray<FFBuildGizmoMath::FSegment> Geometry;
	FFBuildGizmoMath::BuildGeometry(Center, GroundNormal, AxisLength, Geometry);
	ProjectedSegments.Reset(Geometry.Num());
	TArray<FBatchedLine> RenderLines;
	RenderLines.Reserve(Geometry.Num() * 2);
	for (const FFBuildGizmoMath::FSegment& Segment : Geometry)
	{
		FVector2D A, B;
		if (!Project(Segment.Start, A) || !Project(Segment.End, B)) continue;
		ProjectedSegments.Add({A, B, Segment.Handle});
		FLinearColor Color = Segment.Handle == EFF_BuildGizmoHandle::MoveX ? FLinearColor(0.45f, 0.85f, 0.62f) :
			Segment.Handle == EFF_BuildGizmoHandle::MoveY ? FLinearColor(0.77f, 0.67f, 0.98f) :
			FLinearColor(0.95f, 0.86f, 0.56f);
		const bool bEmphasized = Segment.Handle == Gesture.GetHandle() || Segment.Handle == HoveredHandle;
		if (bEmphasized) Color = FLinearColor(1.0f, 0.98f, 0.87f);
		const float Thickness = bEmphasized ? 3.1f : 2.0f;
		// A dark narrow under-stroke preserves contrast on both pale and dark terrain.
		// Foreground depth priority is still behind UMG, and requires no editor/debug renderer.
		RenderLines.Emplace(Segment.Start, Segment.End, FLinearColor(0.04f, 0.10f, 0.08f),
			0.0f, Thickness + 2.0f, SDPG_Foreground);
		RenderLines.Emplace(Segment.Start, Segment.End, Color, 0.0f, Thickness, SDPG_Foreground);
	}
	Lines->Flush();
	Lines->DrawLines(RenderLines);
}

bool AFF_BuildPlacementGizmo::ResolvePointerPoint(const FVector2D& ScreenPosition, FVector& OutPoint) const
{
	APlayerController* PC = Controller.Get();
	if (!PC || ScreenPosition.ContainsNaN()) return false;
	FVector Origin, Direction;
	if (!PC->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, Origin, Direction)) return false;
	return FFBuildGizmoMath::IntersectGroundPlane(Origin, Direction,
		Gesture.IsActive() ? Gesture.GetPlanePoint() : Center,
		Gesture.IsActive() ? Gesture.GetPlaneNormal() : GroundNormal, OutPoint);
}

// 누르기·끌기·놓기·취소를 이동량 또는 회전량으로 바꿔 건설 시스템에 돌려준다.
FFF_BuildGizmoDelta AFF_BuildPlacementGizmo::HandlePointer(const FFF_BuildGizmoPointerInput& Input)
{
	FFF_BuildGizmoDelta Result;
	if (bPendingCancellation)
	{
		bPendingCancellation = false;
		Result.bConsumed = Result.bEnded = Result.bCancelled = true;
		return Result;
	}
	if (Input.bCancel || Input.bPaused || !IsGizmoVisible() || !Controller.IsValid() ||
		(FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()))
	{
		Result = Gesture.End(true);
		FlushPresentation();
		return Result;
	}
	if (Input.bBlockedByUi)
	{
		// UI owns its pointer immediately; never keep moving a facility behind a button.
		Result = Gesture.End(true);
		HoveredHandle = EFF_BuildGizmoHandle::None;
		return Result;
	}
	RefreshGeometry();
	if (!Gesture.IsActive())
	{
		HoveredHandle = FFBuildGizmoMath::HitTest(ProjectedSegments, Input.ScreenPosition);
		if (!Input.bPrimaryPressed || HoveredHandle == EFF_BuildGizmoHandle::None) return Result;
		FVector Point;
		if (!ResolvePointerPoint(Input.ScreenPosition, Point) ||
			!Gesture.Begin(HoveredHandle, Point, Center, GroundNormal)) return Result;
		Result.bConsumed = Result.bStarted = true;
		// A full click between two game ticks must not leave a stuck drag session.
		if (Input.bPrimaryReleased || !Input.bPrimaryDown)
		{
			const FFF_BuildGizmoDelta End = Gesture.End(false);
			Result.bEnded = End.bEnded;
		}
		return Result;
	}
	FVector Point;
	if (ResolvePointerPoint(Input.ScreenPosition, Point)) Result = Gesture.Update(Point);
	else Result.bConsumed = true;
	if (Input.bPrimaryReleased || !Input.bPrimaryDown)
	{
		const FFF_BuildGizmoDelta End = Gesture.End(false);
		Result.bEnded = End.bEnded;
		Result.WorldTranslation = End.WorldTranslation;
		Result.YawDegrees = End.YawDegrees;
	}
	return Result;
}
