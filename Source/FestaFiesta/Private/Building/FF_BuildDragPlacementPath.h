// FF_BuildDragPlacementPath.h

#pragma once

#include "CoreMinimal.h"

/**
 * Constant-space, incremental rasterization on the facility's footprint lattice.
 * The caller owns world/lattice conversion, cursor-sample queues and its per-tick budget.
 * Reset starts on an already processed cell; TryStep emits one cardinal neighbor only.
 * Diagonal corner ties use a canonical direction so retracing visits the same tiles.
 */
class FFF_BuildDragPlacementPath final
{
public:
	void Reset(const FIntPoint& StartCell)
	{
		CurrentCell = StartCell;
		TargetCell = StartCell;
		DeltaX = DeltaY = CrossingError = 0;
		StepX = StepY = 0;
	}

	/** Retarget from the last emitted cell. Repeating the current target preserves progress. */
	void SetTarget(const FIntPoint& InTargetCell)
	{
		if (TargetCell == InTargetCell)
		{
			return;
		}

		TargetCell = InTargetCell;
		const int64 SignedX = static_cast<int64>(TargetCell.X) - CurrentCell.X;
		const int64 SignedY = static_cast<int64>(TargetCell.Y) - CurrentCell.Y;
		DeltaX = SignedX < 0 ? -SignedX : SignedX;
		DeltaY = SignedY < 0 ? -SignedY : SignedY;
		StepX = SignedX < 0 ? -1 : SignedX > 0 ? 1 : 0;
		StepY = SignedY < 0 ? -1 : SignedY > 0 ? 1 : 0;

		// Difference between the next X/Y grid-boundary crossing times, scaled
		// by 2*DeltaX*DeltaY. Incrementing it avoids overflowing cross products
		// even for opposite int32 limits, and keeps each emitted cell O(1).
		CrossingError = DeltaY - DeltaX;
	}

	/** O(1), no allocations. Does not modify OutCell when the segment is complete. */
	bool TryStep(FIntPoint& OutCell)
	{
		if (!HasPendingCells())
		{
			return false;
		}

		// Use the same side of a corner when walking the segment backwards.
		// An unconditional X-first tie would widen a diagonal road on retrace.
		const bool bCrossXFirst = CrossingError < 0 || (CrossingError == 0 && StepX > 0);
		if (CurrentCell.X != TargetCell.X
			&& (CurrentCell.Y == TargetCell.Y || bCrossXFirst))
		{
			CurrentCell.X += StepX;
			CrossingError += 2 * DeltaY;
		}
		else
		{
			CurrentCell.Y += StepY;
			CrossingError -= 2 * DeltaX;
		}

		OutCell = CurrentCell;
		return true;
	}

	bool HasPendingCells() const { return CurrentCell != TargetCell; }
	const FIntPoint& GetCurrentCell() const { return CurrentCell; }
	const FIntPoint& GetTargetCell() const { return TargetCell; }

private:
	FIntPoint CurrentCell = FIntPoint::ZeroValue;
	FIntPoint TargetCell = FIntPoint::ZeroValue;
	int64 DeltaX = 0;
	int64 DeltaY = 0;
	int64 CrossingError = 0;
	int32 StepX = 0;
	int32 StepY = 0;
};
