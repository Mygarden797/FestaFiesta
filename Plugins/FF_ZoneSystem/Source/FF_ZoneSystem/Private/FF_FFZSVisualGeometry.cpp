// FF_FFZSVisualGeometry.cpp
// 역할: 메시의 중심과 크기를 시설 표시 규격에 맞추는 변환을 계산한다. 실제 점유 규칙은 변경하지 않는다.
#include "FF_FFZSVisualGeometry.h"

#include "Engine/StaticMesh.h"
#include "FF_FFZSTypes.h"

namespace
{
    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    bool IsUsableBounds(const FBox& Bounds)
    {
        if (!Bounds.IsValid || !IsFiniteVector(Bounds.Min) || !IsFiniteVector(Bounds.Max))
        {
            return false;
        }

        const FVector Size = Bounds.GetSize();
        return Size.X > UE_KINDA_SMALL_NUMBER &&
            Size.Y > UE_KINDA_SMALL_NUMBER &&
            Size.Z > UE_KINDA_SMALL_NUMBER;
    }

    FTransform MakeAlignedTransform(
        const FBox& SourceBounds,
        const FRotator& Rotation,
        const FVector& Scale,
        const FVector& UserOffset)
    {
        FTransform Result(Rotation, FVector::ZeroVector, Scale);
        const FBox ScaledRotatedBounds = FFZSVisualGeometry::TransformBounds(SourceBounds, Result);
        const FVector Center = ScaledRotatedBounds.GetCenter();
        Result.SetTranslation(FVector(-Center.X, -Center.Y, -ScaledRotatedBounds.Min.Z) + UserOffset);
        return Result;
    }
}

namespace FFZSVisualGeometry
{
    bool HasUsableSourceBounds(const FBox& SourceBounds)
    {
        return IsUsableBounds(SourceBounds);
    }

    FBox TransformBounds(const FBox& SourceBounds, const FTransform& Transform)
    {
        if (!SourceBounds.IsValid)
        {
            return FBox(EForceInit::ForceInit);
        }

        FBox Result(EForceInit::ForceInit);
        for (int32 X = 0; X < 2; ++X)
        {
            for (int32 Y = 0; Y < 2; ++Y)
            {
                for (int32 Z = 0; Z < 2; ++Z)
                {
                    Result += Transform.TransformPosition(FVector(
                        X == 0 ? SourceBounds.Min.X : SourceBounds.Max.X,
                        Y == 0 ? SourceBounds.Min.Y : SourceBounds.Max.Y,
                        Z == 0 ? SourceBounds.Min.Z : SourceBounds.Max.Z));
                }
            }
        }
        return Result;
    }

    bool IsCardinalYawRotation(const FRotator& Rotation)
    {
        if (!FMath::IsFinite(Rotation.Pitch) ||
            !FMath::IsFinite(Rotation.Yaw) ||
            !FMath::IsFinite(Rotation.Roll))
        {
            return false;
        }

        const double NormalizedPitch = FRotator::NormalizeAxis(Rotation.Pitch);
        const double NormalizedRoll = FRotator::NormalizeAxis(Rotation.Roll);
        const double NormalizedYaw = FRotator::NormalizeAxis(Rotation.Yaw);
        const double NearestQuarterTurn = FMath::RoundToDouble(NormalizedYaw / 90.0) * 90.0;
        return NormalizedPitch == 0.0 &&
            NormalizedRoll == 0.0 &&
            NormalizedYaw == NearestQuarterTurn;
    }

    bool TryResolveVisualTransformFromBounds(
        const FFFZSFacilitySpec& Spec,
        const FBox& SourceBounds,
        FTransform& OutTransform)
    {
        const FTransform SourceSizeTransform(
            Spec.VisualRotation,
            Spec.VisualOffsetCm,
            FVector::OneVector);

        if (Spec.VisualSizingMode == EFFZSVisualSizingMode::SourceMeshSize)
        {
            OutTransform = SourceSizeTransform;
            return true;
        }
        if (!IsUsableBounds(SourceBounds))
        {
            OutTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
            return false;
        }

        const FVector TargetSize(
            static_cast<double>(Spec.FootprintSizeCells.X),
            static_cast<double>(Spec.FootprintSizeCells.Y),
            Spec.PlacementObstacleHeightCm);
        if (!IsFiniteVector(TargetSize) ||
            TargetSize.X <= UE_KINDA_SMALL_NUMBER ||
            TargetSize.Y <= UE_KINDA_SMALL_NUMBER ||
            TargetSize.Z <= UE_KINDA_SMALL_NUMBER)
        {
            OutTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
            return false;
        }

        if (Spec.VisualSizingMode == EFFZSVisualSizingMode::UniformFit ||
            !IsCardinalYawRotation(Spec.VisualRotation))
        {
            const FBox RotatedBounds = TransformBounds(
                SourceBounds,
                FTransform(Spec.VisualRotation));
            const FVector RotatedSize = RotatedBounds.GetSize();
            if (!IsUsableBounds(RotatedBounds))
            {
                OutTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
                return false;
            }

            const double UniformScale = FMath::Min3(
                TargetSize.X / RotatedSize.X,
                TargetSize.Y / RotatedSize.Y,
                TargetSize.Z / RotatedSize.Z);
            OutTransform = MakeAlignedTransform(
                SourceBounds,
                Spec.VisualRotation,
                FVector(UniformScale),
                Spec.VisualOffsetCm);
            return true;
        }

        const FVector SourceSize = SourceBounds.GetSize();
        const int32 QuarterTurns = FMath::RoundToInt(
            FRotator::NormalizeAxis(Spec.VisualRotation.Yaw) / 90.0f);
        const bool bSwapXY = FMath::Abs(QuarterTurns) % 2 == 1;
        const FVector ExactScale(
            (bSwapXY ? TargetSize.Y : TargetSize.X) / SourceSize.X,
            (bSwapXY ? TargetSize.X : TargetSize.Y) / SourceSize.Y,
            TargetSize.Z / SourceSize.Z);
        OutTransform = MakeAlignedTransform(
            SourceBounds,
            Spec.VisualRotation,
            ExactScale,
            Spec.VisualOffsetCm);
        return true;
    }

    FTransform ResolveVisualTransformFromBounds(
        const FFFZSFacilitySpec& Spec,
        const FBox& SourceBounds)
    {
        FTransform Result;
        TryResolveVisualTransformFromBounds(Spec, SourceBounds, Result);
        return Result;
    }

    FTransform ResolveVisualMeshTransform(
        const FFFZSFacilitySpec& Spec,
        const UStaticMesh* Mesh)
    {
        return ResolveVisualTransformFromBounds(
            Spec,
            Mesh ? Mesh->GetBoundingBox() : FBox(EForceInit::ForceInit));
    }
}
