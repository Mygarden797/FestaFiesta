// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"

/**
 * 해상도 목록 생성과 선택에 필요한 플랫폼 비의존 정책.
 *
 * 장치 조회와 UGameUserSettings 변경은 화면이 담당하고, 이곳은 입력 배열만으로 결과가 결정된다.
 * 따라서 패키징 환경에서만 달라지기 쉬운 해상도 조회 결과를 자동화 테스트로 재현할 수 있다.
 */
namespace FF::SettingsPolicy
{
	namespace Private
	{
		inline bool IsPositiveResolution(const FIntPoint& Resolution)
		{
			return Resolution.X > 0 && Resolution.Y > 0;
		}

		inline bool MeetsMinimumResolution(
			const FIntPoint& Resolution,
			const int32 MinimumWidth,
			const int32 MinimumHeight)
		{
			return IsPositiveResolution(Resolution)
				&& Resolution.X >= FMath::Max(1, MinimumWidth)
				&& Resolution.Y >= FMath::Max(1, MinimumHeight);
		}

		inline bool FitsDesktop(const FIntPoint& Resolution, const FIntPoint& DesktopResolution)
		{
			// 일부 플랫폼은 데스크톱 크기를 조회하지 못한다. 그때는 이미 플랫폼이 검출한 후보를
			// 버리지 않는다. 유효한 데스크톱 값이 있을 때만 상한으로 사용한다.
			return !IsPositiveResolution(DesktopResolution)
				|| (Resolution.X <= DesktopResolution.X && Resolution.Y <= DesktopResolution.Y);
		}

		inline void AddSafeResolutions(
			TArray<FIntPoint>& OutResolutions,
			const TArray<FIntPoint>& Candidates,
			const int32 MinimumWidth,
			const int32 MinimumHeight,
			const FIntPoint& DesktopResolution,
			const bool bConstrainToDesktop)
		{
			for (const FIntPoint& Candidate : Candidates)
			{
				if (MeetsMinimumResolution(Candidate, MinimumWidth, MinimumHeight)
					&& (!bConstrainToDesktop || FitsDesktop(Candidate, DesktopResolution)))
				{
					OutResolutions.AddUnique(Candidate);
				}
			}
		}

		inline void SortResolutions(TArray<FIntPoint>& Resolutions)
		{
			Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
			{
				const int64 PixelsA = static_cast<int64>(A.X) * A.Y;
				const int64 PixelsB = static_cast<int64>(B.X) * B.Y;
				if (PixelsA != PixelsB)
				{
					return PixelsA < PixelsB;
				}
				if (A.X != B.X)
				{
					return A.X < B.X;
				}
				return A.Y < B.Y;
			});
		}
	}

	/**
	 * 창 모드별로 사용자에게 제시할 안전한 해상도 목록을 만든다.
	 *
	 * CurrentResolution은 목록에 강제로 넣지 않는다. 저장된 비표준 값 때문에 검출 목록이 오염되는
	 * 문제를 막고, 호출부가 FindClosestResolutionIndex로 가장 가까운 실제 후보를 선택하게 하기 위함이다.
	 */
	inline TArray<FIntPoint> BuildResolutionOptions(
		const EWindowMode::Type WindowMode,
		const TArray<FIntPoint>& DetectedFullscreenResolutions,
		const TArray<FIntPoint>& DetectedWindowedResolutions,
		const TArray<FIntPoint>& FallbackResolutions,
		const FIntPoint& DesktopResolution,
		const FIntPoint& CurrentResolution,
		const int32 MinimumWidth,
		const int32 MinimumHeight)
	{
		// 이 값은 의도적으로 후보 소스가 아니다. 함수 인자에 두어 호출부가 현재값을 명시적으로
		// 전달하게 하고, 바로 아래 FindClosestResolutionIndex에 같은 값을 사용하도록 한다.
		static_cast<void>(CurrentResolution);

		TArray<FIntPoint> Result;

		if (WindowMode == EWindowMode::WindowedFullscreen)
		{
			// Borderless는 OS 데스크톱 크기로 동작한다. 다른 크기를 보여주면 선택값과 실제 적용값이 다르다.
			if (Private::IsPositiveResolution(DesktopResolution))
			{
				Result.Add(DesktopResolution);
			}
			return Result;
		}

		if (WindowMode == EWindowMode::Fullscreen)
		{
			// 드라이버가 확인한 모드가 있으면 fallback이나 저장된 비표준 값을 섞지 않는다.
			Private::AddSafeResolutions(Result, DetectedFullscreenResolutions,
				MinimumWidth, MinimumHeight, DesktopResolution, false);

			if (Result.IsEmpty())
			{
				// 검출 실패 때만 보수적인 fallback을 사용한다. 드라이버 검증을 받지 않은 크기는
				// 데스크톱보다 크지 않게 제한하고, 실제 데스크톱 크기는 최소값과 무관하게 보장한다.
				Private::AddSafeResolutions(Result, FallbackResolutions,
					MinimumWidth, MinimumHeight, DesktopResolution, true);
				if (Private::IsPositiveResolution(DesktopResolution))
				{
					Result.AddUnique(DesktopResolution);
				}
			}
		}
		else
		{
			// 창 모드는 플랫폼이 계산한 '편리한 크기'를 우선한다. 화면 밖으로 잘리는 크기는 제외한다.
			Private::AddSafeResolutions(Result, DetectedWindowedResolutions,
				MinimumWidth, MinimumHeight, DesktopResolution, true);

			if (Result.IsEmpty())
			{
				Private::AddSafeResolutions(Result, FallbackResolutions,
					MinimumWidth, MinimumHeight, DesktopResolution, true);
			}

			// 플랫폼과 fallback 모두 비어 있어도 설정 UI가 선택 불능이 되지 않게 마지막 안전값을 둔다.
			if (Result.IsEmpty() && Private::IsPositiveResolution(DesktopResolution))
			{
				Result.Add(DesktopResolution);
			}
		}

		Private::SortResolutions(Result);
		return Result;
	}

	/**
	 * 원하는 크기가 목록에 없을 때 가로·세로 차이가 가장 작은 후보의 인덱스를 돌려준다.
	 * 동률이면 픽셀 수가 작은 후보를 택해 화면 밖으로 잘릴 위험과 적용 비용을 낮춘다.
	 */
	inline int32 FindClosestResolutionIndex(
		const TArray<FIntPoint>& ResolutionOptions,
		const FIntPoint& DesiredResolution)
	{
		int32 BestIndex = INDEX_NONE;
		double BestDistanceSquared = TNumericLimits<double>::Max();
		int64 BestPixelCount = MAX_int64;
		FIntPoint BestResolution(MAX_int32, MAX_int32);

		for (int32 Index = 0; Index < ResolutionOptions.Num(); ++Index)
		{
			const FIntPoint& Candidate = ResolutionOptions[Index];
			if (!Private::IsPositiveResolution(Candidate))
			{
				continue;
			}

			const double DeltaWidth = static_cast<double>(Candidate.X) - DesiredResolution.X;
			const double DeltaHeight = static_cast<double>(Candidate.Y) - DesiredResolution.Y;
			const double DistanceSquared = DeltaWidth * DeltaWidth + DeltaHeight * DeltaHeight;
			const int64 PixelCount = static_cast<int64>(Candidate.X) * Candidate.Y;

			const bool bCloser = DistanceSquared < BestDistanceSquared;
			const bool bSaferTie = DistanceSquared == BestDistanceSquared
				&& (PixelCount < BestPixelCount
					|| (PixelCount == BestPixelCount
						&& (Candidate.X < BestResolution.X
							|| (Candidate.X == BestResolution.X && Candidate.Y < BestResolution.Y))));

			if (bCloser || bSaferTie)
			{
				BestIndex = Index;
				BestDistanceSquared = DistanceSquared;
				BestPixelCount = PixelCount;
				BestResolution = Candidate;
			}
		}

		return BestIndex;
	}
}
