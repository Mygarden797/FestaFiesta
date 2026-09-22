#include "Building/Presentation/FF_BuildPlacementGuidance.h"

#define LOCTEXT_NAMESPACE "FFBuildPlacementGuidance"

FText FFBuildPlacementAssistance::ReasonTitle(const EFFZSPlacementFailure Reason)
{
    switch (Reason)
    {
    case EFFZSPlacementFailure::PlacementCollision: return LOCTEXT("Collision", "시설 또는 장애물과 겹쳐요");
    case EFFZSPlacementFailure::InsufficientClearance: return LOCTEXT("Clearance", "시설 주변 여유 공간이 부족해요");
    case EFFZSPlacementFailure::QueueFootprintBlocked: return LOCTEXT("Queue", "대기 줄이 들어갈 공간이 없어요");
    case EFFZSPlacementFailure::OutsideZone:
    case EFFZSPlacementFailure::ZoneNotFound: return LOCTEXT("Outside", "건축 구역을 벗어났어요");
    case EFFZSPlacementFailure::ProtectedRouteBlocked: return LOCTEXT("Route", "비워 두어야 할 구역과 겹쳐요");
    case EFFZSPlacementFailure::AccessibilityRouteBlocked: return LOCTEXT("Accessible", "이동 통로를 막고 있어요");
    case EFFZSPlacementFailure::RequiredRouteDisconnected: return LOCTEXT("Entry", "손님이 입구까지 갈 수 없어요");
    case EFFZSPlacementFailure::InvalidGround: return LOCTEXT("Ground", "시설을 놓을 바닥을 찾지 못했어요");
    case EFFZSPlacementFailure::ExcessSlope: return LOCTEXT("Slope", "바닥의 경사가 너무 커요");
    case EFFZSPlacementFailure::HeightVarianceExceeded: return LOCTEXT("Uneven", "시설 아래 바닥이 고르지 않아요");
    case EFFZSPlacementFailure::CategoryNotAllowed: return LOCTEXT("Category", "이 구역에 놓을 수 없는 시설이에요");
    case EFFZSPlacementFailure::InsufficientFunds: return LOCTEXT("Gold", "설치할 골드가 부족해요");
    case EFFZSPlacementFailure::BuildLimitReached: return LOCTEXT("Limit", "이 시설의 설치 한도에 도달했어요");
    case EFFZSPlacementFailure::WrongStage: return LOCTEXT("Stage", "지금은 시설을 배치할 수 없어요");
    case EFFZSPlacementFailure::FacilityOccupied:
    case EFFZSPlacementFailure::InvalidFacilityState: return LOCTEXT("Occupied", "이용 중인 시설이에요");
    case EFFZSPlacementFailure::NavigationUnavailable:
    case EFFZSPlacementFailure::NavRebuildTimeout: return LOCTEXT("Navigation", "손님의 이동 경로를 확인하고 있어요");
    case EFFZSPlacementFailure::StaleWorldRevision: return LOCTEXT("Changed", "주변 시설의 상태가 바뀌었어요");
    default: return LOCTEXT("Unavailable", "현재 위치에 배치할 수 없어요");
    }
}

bool FFBuildPlacementAssistance::CanResolveByMoving(const TArray<EFFZSPlacementFailure>& Reasons)
{
    if (Reasons.IsEmpty()) return false;
    for (const auto Reason : Reasons)
    {
        switch (Reason)
        {
        case EFFZSPlacementFailure::OutsideZone:
        case EFFZSPlacementFailure::InvalidGround:
        case EFFZSPlacementFailure::ExcessSlope:
        case EFFZSPlacementFailure::HeightVarianceExceeded:
        case EFFZSPlacementFailure::PlacementCollision:
        case EFFZSPlacementFailure::InsufficientClearance:
        case EFFZSPlacementFailure::QueueFootprintBlocked:
        case EFFZSPlacementFailure::ProtectedRouteBlocked:
        case EFFZSPlacementFailure::AccessibilityRouteBlocked:
        case EFFZSPlacementFailure::RequiredRouteDisconnected: break;
        default: return false;
        }
    }
    return true;
}

TArray<FIntPoint> FFBuildPlacementAssistance::MakeNearbyOffsets(const int32 SnapCm)
{
    const int32 Step = FMath::Clamp(SnapCm, 1, 100);
    TArray<FIntPoint> Result;
    for (const int32 Radius : {25, 50, 100, 200, 300, 400, 600})
    {
        const int32 R = FMath::Max(Step, FMath::RoundToInt(static_cast<double>(Radius) / Step) * Step);
        for (const FIntPoint Direction : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1),
            FIntPoint(1, 1), FIntPoint(-1, 1), FIntPoint(1, -1), FIntPoint(-1, -1)})
        {
            const FIntPoint Offset = Direction * R;
            if (static_cast<int64>(Offset.X) * Offset.X + static_cast<int64>(Offset.Y) * Offset.Y <= 600 * 600)
                Result.AddUnique(Offset);
        }
    }
    Result.StableSort([](const FIntPoint& A, const FIntPoint& B) { return A.SizeSquared() < B.SizeSquared(); });
    return Result;
}
#undef LOCTEXT_NAMESPACE
