// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Lane/FF_CrowdLaneBake.h"

namespace CrowdLaneBake
{
	namespace
	{
		float PerpDistance2D(const FVector& P, const FVector& A, const FVector& B)
		{
			const FVector2D AP(P.X - A.X, P.Y - A.Y);
			const FVector2D AB(B.X - A.X, B.Y - A.Y);

			const float LenSq = static_cast<float>(AB.SizeSquared());
			if (LenSq <= KINDA_SMALL_NUMBER)
			{
				return static_cast<float>(AP.Size());
			}

			const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(AP, AB)) / LenSq, 0.f, 1.f);
			return static_cast<float>((AP - AB * T).Size());
		}
	}

	void SimplifyPolyline(const TArray<FVector>& In, float ToleranceCm, TArray<FVector>& Out)
	{
		Out.Reset();

		if (In.Num() <= 2)
		{
			Out = In;
			return;
		}

		const float Tolerance = FMath::Max(ToleranceCm, 0.f);
		const int32 Last = In.Num() - 1;

		TArray<bool> Keep;
		Keep.Init(false, In.Num());
		Keep[0] = true;
		Keep[Last] = true;

		TArray<TPair<int32, int32>> Pending;

		// 닫힌 레인은 양 끝점이 같아 코드가 전체를 한 점으로 접는다. 가장 먼 정점을 세 번째 앵커로 박는다.
		const bool bClosed = In[0].Equals(In[Last], 1.f) && In.Num() >= 4;
		if (bClosed)
		{
			int32 Farthest = 1;
			float FarthestDistSq = -1.f;
			for (int32 Index = 1; Index < Last; ++Index)
			{
				const float DistSq = static_cast<float>(FVector::DistSquared2D(In[Index], In[0]));
				if (DistSq > FarthestDistSq)
				{
					FarthestDistSq = DistSq;
					Farthest = Index;
				}
			}

			Keep[Farthest] = true;
			Pending.Emplace(0, Farthest);
			Pending.Emplace(Farthest, Last);
		}
		else
		{
			Pending.Emplace(0, Last);
		}

		while (Pending.Num() > 0)
		{
			const TPair<int32, int32> Range = Pending.Pop();

			int32 SplitIndex = INDEX_NONE;
			float SplitDist = -1.f;

			for (int32 Index = Range.Key + 1; Index < Range.Value; ++Index)
			{
				const float Dist = PerpDistance2D(In[Index], In[Range.Key], In[Range.Value]);
				if (Dist > SplitDist)
				{
					SplitDist = Dist;
					SplitIndex = Index;
				}
			}

			if (SplitIndex != INDEX_NONE && SplitDist > Tolerance)
			{
				Keep[SplitIndex] = true;
				Pending.Emplace(Range.Key, SplitIndex);
				Pending.Emplace(SplitIndex, Range.Value);
			}
		}

		Out.Reserve(In.Num());
		for (int32 Index = 0; Index < In.Num(); ++Index)
		{
			if (Keep[Index])
			{
				Out.Add(In[Index]);
			}
		}
	}

	void ResamplePolyline(const TArray<FVector>& In, float SpacingCm, TArray<FVector>& Out)
	{
		Out.Reset();

		if (In.Num() == 0)
		{
			return;
		}

		const float Spacing = FMath::Max(SpacingCm, 1.f);

		Out.Add(In[0]);

		for (int32 Index = 1; Index < In.Num(); ++Index)
		{
			const FVector& A = In[Index - 1];
			const FVector& B = In[Index];

			const float SegmentLength = static_cast<float>(FVector::Dist2D(A, B));
			const int32 Divisions = FMath::Max(1, FMath::CeilToInt(SegmentLength / Spacing));

			for (int32 Step = 1; Step <= Divisions; ++Step)
			{
				Out.Add(FMath::Lerp(A, B, static_cast<float>(Step) / static_cast<float>(Divisions)));
			}
		}
	}

	void EmitSegments(const TArray<FVector>& Simplified, const FBakeParams& Params, TArray<FLaneSegment>& Out)
	{
		Out.Reset();

		if (Simplified.Num() < 2)
		{
			return;
		}

		const float HalfWidth = FMath::Max(Params.LaneHalfWidthCm, 1.f);
		const float MinLength = FMath::Max(Params.MinSegmentLengthRatio, 0.f) * HalfWidth;
		const float MaxDeviation = 0.5f * HalfWidth;
		const float Overlap = FMath::Max(Params.CornerOverlapCm, 0.f);

		auto Emit = [&Out, HalfWidth, Overlap](const FVector& A, const FVector& B)
		{
			FVector Delta = B - A;
			Delta.Z = 0.f;

			const float Length = static_cast<float>(Delta.Size());
			if (Length <= KINDA_SMALL_NUMBER)
			{
				return;
			}

			FLaneSegment Segment;
			Segment.CenterCm = (A + B) * 0.5f;
			Segment.YawDegrees = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(Delta.Y, Delta.X)));
			Segment.HalfLengthCm = Length * 0.5f + Overlap;
			Segment.HalfWidthCm = HalfWidth;

			Out.Add(Segment);
		};

		int32 Anchor = 0;

		for (int32 Index = 1; Index < Simplified.Num(); ++Index)
		{
			const bool bLast = (Index == Simplified.Num() - 1);

			float Deviation = 0.f;
			for (int32 Inner = Anchor + 1; Inner < Index; ++Inner)
			{
				Deviation = FMath::Max(Deviation,
					PerpDistance2D(Simplified[Inner], Simplified[Anchor], Simplified[Index]));
			}

			if (Deviation > MaxDeviation && Index > Anchor + 1)
			{
				Emit(Simplified[Anchor], Simplified[Index - 1]);
				Anchor = Index - 1;
				--Index;
				continue;
			}

			const float ChordLength = static_cast<float>(FVector::Dist2D(Simplified[Anchor], Simplified[Index]));
			if (!bLast && ChordLength < MinLength)
			{
				continue;
			}

			Emit(Simplified[Anchor], Simplified[Index]);
			Anchor = Index;
		}
	}

	bool ValidateWidthInvariant(float LaneHalfWidthCm, float ProtectedWidthCm, float AgentRadiusCm,
	                            FString& OutError)
	{
		OutError.Reset();

		if (LaneHalfWidthCm <= 0.f)
		{
			OutError = FString::Printf(TEXT("LaneHalfWidthCm은 0보다 커야 한다 (현재 %.1f)"), LaneHalfWidthCm);
			return false;
		}

		if (AgentRadiusCm < 0.f)
		{
			OutError = FString::Printf(TEXT("AgentRadiusCm은 음수일 수 없다 (현재 %.1f)"), AgentRadiusCm);
			return false;
		}

		const float RequiredCm = 2.f * LaneHalfWidthCm + 2.f * (AgentRadiusCm + WallClearanceCm);

		if (ProtectedWidthCm + KINDA_SMALL_NUMBER < RequiredCm)
		{
			OutError = FString::Printf(
				TEXT("보호 구역 폭이 부족하다: %.1fcm < 필요 %.1fcm (= 2 x %.1f + 2 x (%.1f + %.1f))"),
				ProtectedWidthCm, RequiredCm, LaneHalfWidthCm, AgentRadiusCm, WallClearanceCm);
			return false;
		}

		return true;
	}
}
