// FF_BuildDragPlacementPath.h
// 역할: 드래그 경로의 격자 좌표를 빠짐없이 순서대로 생성한다.

#pragma once

#include "CoreMinimal.h"

/**
 * 마우스로 그린 선을 시설 크기의 격자 한 칸씩 따라가며 설치 위치를 만든다.
 * 경로 전체를 저장하지 않으므로 길이가 늘어도 이 계산기의 메모리 사용량은 같다.
 * 실제 월드 좌표 변환과 프레임당 작업량 제한은 호출하는 쪽에서 관리한다.
 * Reset의 시작 칸은 이미 처리한 칸이다. TryStep은 상하좌우 이웃 한 칸만 반환한다.
 * 대각선을 되짚어도 같은 칸을 지나도록 모서리를 통과하는 방향을 일정하게 정한다.
 */
class FFF_BuildDragPlacementPath final
{
public:
	/** 같은 가로줄·세로줄을 계속 전진할 때만 경로를 합친다. 꺾거나 되돌아가는 지점은 남긴다. */
	static bool CanExtendCardinalSegment(const FIntPoint& Start, const FIntPoint& End, const FIntPoint& Next)
	{
		if (Start.Y == End.Y && End.Y == Next.Y)
		{
			return (Start.X < End.X && End.X < Next.X) || (Start.X > End.X && End.X > Next.X);
		}
		if (Start.X == End.X && End.X == Next.X)
		{
			return (Start.Y < End.Y && End.Y < Next.Y) || (Start.Y > End.Y && End.Y > Next.Y);
		}
		return false;
	}

	void Reset(const FIntPoint& StartCell)
	{
		CurrentCell = StartCell;
		TargetCell = StartCell;
		DeltaX = DeltaY = CrossingError = 0;
		StepX = StepY = 0;
	}

	/** 마지막으로 반환한 칸에서 새 목적지로 이어 간다. 같은 목적지는 진행 상태를 초기화하지 않는다. */
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

		// 다음 가로·세로 경계 중 어느 쪽을 먼저 만나는지 나타내는 누적 차이다.
		// 큰 좌표끼리 곱하지 않고 차이를 더하고 빼므로 정수 범위를 넘지 않으며,
		// 경로 길이와 관계없이 다음 칸 하나를 일정한 계산량으로 구한다.
		CrossingError = DeltaY - DeltaX;
	}

	/** 메모리 할당 없이 다음 칸을 반환한다. 도착한 상태면 OutCell을 바꾸지 않고 false를 반환한다. */
	bool TryStep(FIntPoint& OutCell)
	{
		if (!HasPendingCells())
		{
			return false;
		}

		// 모서리에 정확히 걸려도 왕복할 때 같은 쪽 칸을 선택한다.
		// 항상 가로 이동을 먼저 하면 되돌아갈 때 다른 칸을 지나 도로 폭이 넓어진다.
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
