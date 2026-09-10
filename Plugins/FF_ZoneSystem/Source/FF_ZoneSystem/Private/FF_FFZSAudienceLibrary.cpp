#include "FF_FFZSAudienceLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

#define LOCTEXT_NAMESPACE "FFZSAudienceLibrary"

bool UFFZSAudienceLibrary::BuildSnapshot(const FFFZSAudienceLayout& Layout, const FTransform& Transform,
    FFFZSAudienceSnapshot& OutSnapshot, FText& OutError)
{
    OutSnapshot = FFFZSAudienceSnapshot();
    OutError = FText::GetEmpty();
    TArray<FText> Errors;
    if (!Layout.bEnabled || !Layout.IsValid(Errors) || Transform.ContainsNaN()
        || !Transform.GetScale3D().Equals(FVector::OneVector, .001)
        || !Transform.GetRotation().IsNormalized())
    {
        OutError = Errors.IsEmpty() ? LOCTEXT("InvalidLayout", "관람 공간이 꺼져 있거나 시설 Transform이 유효하지 않습니다. 시설 크기는 Actor Scale이 아닌 PDA로 조정하세요.") : Errors[0];
        return false;
    }
    OutSnapshot.Layout = Layout;
    OutSnapshot.FacilityTransform = Transform;
    OutSnapshot.Capacity = Layout.GetCapacity();
    OutSnapshot.EntryWorldCm = Transform.TransformPosition(Layout.EntryLocalCm);
    OutSnapshot.ExitWorldCm = Transform.TransformPosition(Layout.ExitLocalCm);
    OutSnapshot.FocusWorldCm = Transform.TransformPosition(Layout.FocusLocalCm);
    return true;
}

bool UFFZSAudienceLibrary::GetFacilityAudienceSnapshot(const UObject* WorldContextObject,
    const FGuid FacilityInstanceId, FFFZSAudienceSnapshot& OutSnapshot, FText& OutError)
{
    OutSnapshot = FFFZSAudienceSnapshot();
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    const UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    const AFFZSFacilityActor* Facility = Zones ? Zones->FindFacility(FacilityInstanceId) : nullptr;
    if (!Facility || Facility->GetFacilityState() != EFFZSFacilityState::Active || Facility->NeedsRelocation())
    {
        OutError = LOCTEXT("Unavailable", "운영 중이고 재배치가 필요하지 않은 시설의 Instance ID가 필요합니다.");
        return false;
    }
    if (!BuildSnapshot(Facility->GetFacilitySpec().Audience, Facility->GetActorTransform(), OutSnapshot, OutError)) return false;
    OutSnapshot.FacilityInstanceId = FacilityInstanceId;
    OutSnapshot.WorldRevision = Zones->GetWorldRevision();
    return true;
}

bool UFFZSAudienceLibrary::GetAudienceSlotTransform(const FFFZSAudienceSnapshot& Snapshot,
    const int32 SlotIndex, FTransform& OutTransform)
{
    FVector Local;
    if (Snapshot.Capacity != Snapshot.Layout.GetCapacity()
        || Snapshot.FacilityTransform.ContainsNaN()
        || !Snapshot.Layout.TryGetSlotLocal(SlotIndex, Local)) return false;
    const FVector World = Snapshot.FacilityTransform.TransformPosition(Local);
    const FVector Direction = (Snapshot.FocusWorldCm - World).GetSafeNormal2D();
    if (Direction.IsNearlyZero() || World.ContainsNaN()) return false;
    OutTransform = FTransform(Direction.Rotation(), World);
    return true;
}

bool UFFZSAudienceLibrary::CheckAudienceNavigationBatch(const UObject* WorldContextObject,
    const FFFZSAudienceSnapshot& Snapshot, const int32 FirstSlotIndex, const int32 SlotCount,
    TArray<int32>& OutFailedSlots, FText& OutError)
{
    OutFailedSlots.Reset();
    OutError = FText::GetEmpty();
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    TArray<FText> Errors;
    if (!Nav || !Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)
        || !Snapshot.Layout.bEnabled || !Snapshot.Layout.IsValid(Errors)
        || Snapshot.Capacity != Snapshot.Layout.GetCapacity() || Snapshot.FacilityTransform.ContainsNaN()
        || FirstSlotIndex < 0 || SlotCount < 1 || SlotCount > 64
        || static_cast<int64>(FirstSlotIndex) + SlotCount > Snapshot.Capacity)
    {
        OutError = LOCTEXT("NavInput", "NavMesh와 유효한 관람 데이터가 필요합니다. 한 번에 정원 범위 안의 1~64자리를 검사하세요.");
        return false;
    }
    const double Tolerance = Snapshot.Layout.ArrivalToleranceCm;
    const FVector Extent(Tolerance, Tolerance, 200.);
    const auto Project = [&](const FVector& Point, FNavLocation& Result)
    {
        return Nav->ProjectPointToNavigation(Point, Result, Extent)
            && FVector::DistSquared2D(Point, Result.Location) <= FMath::Square(Tolerance);
    };
    const auto Connected = [&](const FVector& From, const FVector& To)
    {
        if (FVector::DistSquared(From, To) <= 1.) return true;
        const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To);
        return Path && Path->IsValid() && !Path->IsPartial();
    };
    FNavLocation Entry, Exit;
    if (!Project(Snapshot.EntryWorldCm, Entry) || !Project(Snapshot.ExitWorldCm, Exit))
    {
        OutError = LOCTEXT("PortalOffNav", "관람 입구 또는 출구가 NavMesh 바깥입니다. 위치·높이·Collision을 확인하세요.");
        return false;
    }
    for (int32 Index = FirstSlotIndex; Index < FirstSlotIndex + SlotCount; ++Index)
    {
        FTransform Slot;
        FNavLocation Projected;
        if (!GetAudienceSlotTransform(Snapshot, Index, Slot) || !Project(Slot.GetLocation(), Projected)
            || !Connected(Entry.Location, Projected.Location) || !Connected(Projected.Location, Exit.Location))
        {
            OutFailedSlots.Add(Index);
        }
    }
    if (!OutFailedSlots.IsEmpty())
    {
        OutError = LOCTEXT("Disconnected", "일부 관람 자리에 완전한 입퇴장 경로가 없습니다. 실패 자리 번호의 지형·장애물을 확인하세요.");
        return false;
    }
    return true;
}

#undef LOCTEXT_NAMESPACE
