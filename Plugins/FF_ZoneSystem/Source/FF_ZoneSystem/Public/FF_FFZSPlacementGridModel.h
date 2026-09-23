// FF_FFZSPlacementGridModel.h
// 역할: 화면 격자 간격과 위치 정렬을 계산한다. 화면 선 간격과 저장용 1cm 좌표를 구분한다.
#pragma once
#include "CoreMinimal.h"

// 권위 1cm 저장 격자와 별개인 사용자용 로컬 격자의 순수 계산 규칙이다.
// Actor/World 없이 자동화 테스트할 수 있어 음수 좌표, 잘못된 설정과 대형 footprint 회귀를 막는다.
namespace FFZSPlacementGridModel
{
    constexpr int32 DefaultVisualGridStepCm = 100;
    constexpr int32 MinimumVisualGridStepCm = 25;
    constexpr int32 MaximumVisualGridStepCm = 1000;

    inline int32 SanitizeGridStepCm(const int32 StepCm)
    {
        return StepCm >= MinimumVisualGridStepCm && StepCm <= MaximumVisualGridStepCm
            ? StepCm
            : DefaultVisualGridStepCm;
    }

    inline double AlignDownToGrid(const double CoordinateCm, const int32 StepCm)
    {
        const int32 SafeStepCm = SanitizeGridStepCm(StepCm);
        return FMath::FloorToDouble(CoordinateCm / SafeStepCm) * SafeStepCm;
    }

    inline double AlignUpToGrid(const double CoordinateCm, const int32 StepCm)
    {
        const int32 SafeStepCm = SanitizeGridStepCm(StepCm);
        return FMath::CeilToDouble(CoordinateCm / SafeStepCm) * SafeStepCm;
    }

    inline float ResolveGridHalfExtentCm(
        const int32 FootprintAxisCm,
        const float PaddingCm,
        const float MaximumRadiusCm,
        const int32 StepCm)
    {
        const int32 SafeStepCm = SanitizeGridStepCm(StepCm);
        const float SafePaddingCm = FMath::IsFinite(PaddingCm) ? FMath::Max(0.0f, PaddingCm) : 600.0f;
        const float SafeMaximumRadiusCm = FMath::IsFinite(MaximumRadiusCm) && MaximumRadiusCm >= SafeStepCm
            ? MaximumRadiusCm
            : 3600.0f;
        const float FootprintHalfExtentCm = FMath::Max(0, FootprintAxisCm) * 0.5f;
        const float MinimumHalfExtentCm = FMath::Max(
            FootprintHalfExtentCm,
            SafeStepCm * 2.0f);
        const float DesiredHalfExtentCm = FMath::Max(
            FootprintHalfExtentCm + SafePaddingCm,
            MinimumHalfExtentCm);
        // 반경 제한은 주변 여백을 줄이는 소프트 제한이다. 대형 시설 자체를 잘라 내면 가장
        // 정렬이 어려운 외곽을 볼 수 없으므로 footprint 반경만큼은 항상 보장한다.
        return FMath::Max(MinimumHalfExtentCm, FMath::Min(DesiredHalfExtentCm, SafeMaximumRadiusCm));
    }

    inline bool IsMajorGridCoordinate(
        const double CoordinateCm,
        const int32 MajorIntervalCm)
    {
        if (MajorIntervalCm <= 0 || !FMath::IsFinite(CoordinateCm))
        {
            return false;
        }
        const double Remainder = FMath::Abs(FMath::Fmod(CoordinateCm, MajorIntervalCm));
        return Remainder <= 0.01 || FMath::Abs(Remainder - MajorIntervalCm) <= 0.01;
    }

    inline float ResolveEdgeOpacity(
        const double CoordinateCm,
        const double CenterCm,
        const double HalfExtentCm)
    {
        if (!FMath::IsFinite(CoordinateCm) || !FMath::IsFinite(CenterCm) ||
            !FMath::IsFinite(HalfExtentCm) || HalfExtentCm <= UE_SMALL_NUMBER)
        {
            return 0.25f;
        }
        const double NormalizedDistance = FMath::Clamp(
            FMath::Abs(CoordinateCm - CenterCm) / HalfExtentCm,
            0.0,
            1.0);
        if (NormalizedDistance <= 0.65)
        {
            return 1.0f;
        }
        const float FadeAlpha = static_cast<float>((NormalizedDistance - 0.65) / 0.35);
        return FMath::Lerp(1.0f, 0.25f, FadeAlpha);
    }
}

