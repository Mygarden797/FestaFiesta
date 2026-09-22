// FF_FFZSAudienceLayout.cpp
// 역할: 관객이 설 자리를 행과 열로 만들고 관람 영역에 맞는 위치와 수용 인원을 계산한다.
#include "FF_FFZSAudienceLayout.h"

#define LOCTEXT_NAMESPACE "FFZSAudienceLayout"

int32 FFFZSAudienceLayout::GetCapacity() const
{
    if (!bEnabled || Columns < 1 || Rows < 1 || Columns > MaxDimension || Rows > MaxDimension)
    {
        return 0;
    }
    const int64 Count = static_cast<int64>(Columns) * Rows;
    return Count <= MaxSlots ? static_cast<int32>(Count) : 0;
}

bool FFFZSAudienceLayout::TryGetSlotLocal(const int32 SlotIndex, FVector& OutPosition) const
{
    if (SlotIndex < 0 || SlotIndex >= GetCapacity() || FirstSlotLocalCm.ContainsNaN()
        || !FMath::IsFinite(ColumnSpacingCm) || ColumnSpacingCm <= 0.f
        || !FMath::IsFinite(RowSpacingCm) || RowSpacingCm <= 0.f
        || !FMath::IsFinite(CenterAisleWidthCm) || CenterAisleWidthCm < 0.f)
    {
        return false;
    }
    const int32 Column = SlotIndex % Columns;
    const double Aisle = Columns > 1 && Column >= (Columns + 1) / 2 ? CenterAisleWidthCm : 0.;
    const FVector Result = FirstSlotLocalCm + FVector(
        static_cast<double>(Column) * ColumnSpacingCm + Aisle,
        static_cast<double>(SlotIndex / Columns) * RowSpacingCm, 0.);
    if (Result.ContainsNaN()) return false;
    OutPosition = Result;
    return true;
}

bool FFFZSAudienceLayout::TryGetLocalBounds(FBox2D& OutBounds) const
{
    FVector Last;
    if (!FMath::IsFinite(PerimeterMarginCm) || PerimeterMarginCm <= 0.f
        || !TryGetSlotLocal(GetCapacity() - 1, Last)) return false;
    const FBox2D Bounds(FVector2D(FirstSlotLocalCm) - FVector2D(PerimeterMarginCm),
        FVector2D(Last) + FVector2D(PerimeterMarginCm));
    // Keep subsequent cm quantization/area arithmetic bounded even for malformed assets.
    if (!Bounds.bIsValid || Bounds.Min.GetAbsMax() > 1000000. || Bounds.Max.GetAbsMax() > 1000000.) return false;
    OutBounds = Bounds;
    return true;
}

bool FFFZSAudienceLayout::IsValid(TArray<FText>& OutErrors) const
{
    if (!bEnabled) return true; // Legacy assets and saves retain their original behaviour.
    const int32 Before = OutErrors.Num();
    if (!FMath::IsFinite(AreaLimitM2) || AreaLimitM2 < 0.f || AreaLimitM2 > MaxAreaLimitM2)
    {
        OutErrors.Add(LOCTEXT("AreaLimit", "Audience Area Limit은 0~50,000m²의 유한한 값이어야 합니다. 0은 기존 공통 한도를 사용합니다."));
    }
    FBox2D Bounds;
    if (GetCapacity() == 0 || !TryGetLocalBounds(Bounds))
    {
        OutErrors.Add(LOCTEXT("InvalidGrid", "Audience: 행·열·간격·좌표를 확인하세요. 정원은 1~8192명이며 한 방향은 최대 512자리입니다."));
        return false;
    }
    const double Envelope = static_cast<double>(AgentRadiusCm) + ArrivalToleranceCm;
    if (!FMath::IsFinite(AgentRadiusCm) || AgentRadiusCm <= 0.f
        || !FMath::IsFinite(ArrivalToleranceCm) || ArrivalToleranceCm <= 0.f
        || ColumnSpacingCm < 2. * Envelope || RowSpacingCm < 2. * Envelope
        || PerimeterMarginCm < 2. * Envelope)
    {
        OutErrors.Add(LOCTEXT("Spacing", "Audience: 자리 간격과 가장자리 여백은 2 × (NPC Radius + Arrival Tolerance) 이상이어야 합니다."));
        return false;
    }
    FVector Last;
    TryGetSlotLocal(GetCapacity() - 1, Last);
    const FBox2D SeatEnvelope(FVector2D(FirstSlotLocalCm) - FVector2D(Envelope),
        FVector2D(Last) + FVector2D(Envelope));
    const FBox2D WalkablePerimeter(Bounds.Min + FVector2D(Envelope), Bounds.Max - FVector2D(Envelope));
    const auto IsPortalValid = [&](const FVector& Point)
    {
        return !Point.ContainsNaN() && FMath::IsNearlyEqual(Point.Z, FirstSlotLocalCm.Z, .1)
            && WalkablePerimeter.IsInsideOrOn(FVector2D(Point))
            && !SeatEnvelope.IsInsideOrOn(FVector2D(Point));
    };
    if (!IsPortalValid(EntryLocalCm) || !IsPortalValid(ExitLocalCm)
        || FVector::DistSquared2D(EntryLocalCm, ExitLocalCm) < FMath::Square(2. * Envelope))
    {
        OutErrors.Add(LOCTEXT("Portals", "Audience: 입구·출구를 같은 바닥 높이의 가장자리 통로 안에 서로 떨어뜨려 놓으세요. 자리 배열 내부에는 놓을 수 없습니다."));
    }
    if (FocusLocalCm.ContainsNaN() || FMath::Abs(FocusLocalCm.Z) > 1000000.
        || FMath::Abs(FirstSlotLocalCm.Z) > 1000000.
        || SeatEnvelope.IsInsideOrOn(FVector2D(FocusLocalCm)))
    {
        OutErrors.Add(LOCTEXT("Focus", "Audience: 바라볼 지점은 관람 자리 배열 밖의 유효한 무대 좌표여야 합니다."));
    }
    return OutErrors.Num() == Before;
}

void FFFZSAudienceLayout::ApplySizeMultiplier(const float Multiplier)
{
    if (!bEnabled) return;
    FirstSlotLocalCm *= Multiplier;
    ColumnSpacingCm *= Multiplier;
    RowSpacingCm *= Multiplier;
    CenterAisleWidthCm *= Multiplier;
    PerimeterMarginCm *= Multiplier;
    EntryLocalCm *= Multiplier;
    ExitLocalCm *= Multiplier;
    FocusLocalCm *= Multiplier;
    // NPC size and arrival tolerance do not grow when the facility is upgraded.
}

bool FFFZSAudienceLayout::TryCenterAndArrangePortals()
{
    FFFZSAudienceLayout Candidate = *this;
    FVector Last;
    if (!Candidate.TryGetSlotLocal(Candidate.GetCapacity() - 1, Last)) return false;
    Candidate.FirstSlotLocalCm.X = -(Last.X - Candidate.FirstSlotLocalCm.X) * .5;
    if (!Candidate.TryGetSlotLocal(Candidate.GetCapacity() - 1, Last)) return false;
    const double HalfMargin = Candidate.PerimeterMarginCm * .5;
    Candidate.EntryLocalCm = FVector(Candidate.FirstSlotLocalCm.X - HalfMargin, Last.Y + HalfMargin, Last.Z);
    Candidate.ExitLocalCm = FVector(Last.X + HalfMargin, Last.Y + HalfMargin, Last.Z);
    TArray<FText> Errors;
    if (!Candidate.IsValid(Errors)) return false;
    *this = Candidate;
    return true;
}

#undef LOCTEXT_NAMESPACE
