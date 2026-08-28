// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Systems/FF_CrowdPathFollow.h"

namespace CrowdPathFollow
{
	int32 AdvanceToReachableWaypoint(const TArray<FVector>& Points, int32 NextIndex,
	                                 const FVector& Position, float AcceptanceRadiusUU)
	{
		if (Points.Num() == 0)
		{
			return 0;
		}

		int32 Index = FMath::Max(NextIndex, 0);

		const float AcceptanceSq = FMath::Square(FMath::Max(AcceptanceRadiusUU, 0.f));

		while (Points.IsValidIndex(Index)
			&& FVector::DistSquared2D(Position, Points[Index]) <= AcceptanceSq)
		{
			++Index;
		}

		return Index;
	}
}
