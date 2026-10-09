// FFZAZoneAuthoringSubsystem.cpp
// 역할: 구획/구역 레지스트리, 구획 잠금 상태, 배치 정책 판정, Zone 셀 지도.
#include "FFZAZoneAuthoringSubsystem.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSOrientedGeometry.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "FF_ZoneAuthoringModule.h"
#include "FFZADistrict.h"
#include "FFZALayerPolicy.h"
#include "FFZAPlot.h"
#include "FFZAPolygonMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZAZoneAuthoringSubsystem)

#define LOCTEXT_NAMESPACE "FFZAZoneAuthoringSubsystem"

namespace
{
    // 이보다 작은 겹침(cm²)은 부동소수 오차로 보고 무시한다. 1 cm² = FF_ZoneSystem 셀 1개.
    constexpr double MinBlockingOverlapAreaCm2 = 1.0;

    bool IsSpatialCommand(const EFFZSCommandType Type)
    {
        return Type == EFFZSCommandType::Install || Type == EFFZSCommandType::Move || Type == EFFZSCommandType::Upgrade;
    }

    FFFZSPolicyDecision Allow()
    {
        return FFFZSPolicyDecision();
    }

    FFFZSPolicyDecision Reject(const EFFZSPlacementFailure Reason, const FText& Message)
    {
        FFFZSPolicyDecision Decision;
        Decision.bAllowed = false;
        Decision.FailureReason = Reason;
        Decision.Message = Message;
        return Decision;
    }

    FText GetCategoryText(const EFFZSFacilityCategory Category)
    {
        return StaticEnum<EFFZSFacilityCategory>()->GetDisplayNameTextByValue(static_cast<int64>(Category));
    }
}

// ---------------------------------------------------------------------------------------------
// 수명주기
// ---------------------------------------------------------------------------------------------

void UFFZAZoneAuthoringSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // FF_ZoneSystem 서브시스템이 먼저 준비돼야 정책을 등록할 수 있다.
    Collection.InitializeDependency<UFFZSWorldSubsystem>();
    Super::Initialize(Collection);
    // 레벨 스트리밍으로 Zone이 들어오거나 나가면 좌표→Zone 지도를 다시 만든다.
    LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UFFZAZoneAuthoringSubsystem::HandleLevelChanged);
    LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UFFZAZoneAuthoringSubsystem::HandleLevelChanged);
}

void UFFZAZoneAuthoringSubsystem::HandleLevelChanged(ULevel* Level, UWorld* InWorld)
{
    if (InWorld == GetWorld())
    {
        bCellMapDirty = true;
    }
}

void UFFZAZoneAuthoringSubsystem::Deinitialize()
{
    FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
    FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
    if (bPolicyRegistered && LayerPolicy)
    {
        if (UWorld* World = GetWorld())
        {
            if (UFFZSWorldSubsystem* ZoneSystem = World->GetSubsystem<UFFZSWorldSubsystem>())
            {
                ZoneSystem->UnregisterExternalPolicy(LayerPolicy);
            }
        }
    }
    bPolicyRegistered = false;
    Plots.Reset();
    Districts.Reset();
    Super::Deinitialize();
}

void UFFZAZoneAuthoringSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    // 레벨 로드 중 등록이 거절됐다면(명령 실행 중 등) 여기서 한 번 더 시도한다.
    EnsurePolicyRegistered();
    bCellMapDirty = true;
}

bool UFFZAZoneAuthoringSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    // FF_ZoneSystem 서브시스템과 같은 월드 종류를 지원한다(에디터에서도 규칙·오버레이가 동작).
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE ||
        WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview ||
        WorldType == EWorldType::GamePreview;
}

// ---------------------------------------------------------------------------------------------
// 레지스트리
// ---------------------------------------------------------------------------------------------

void UFFZAZoneAuthoringSubsystem::RegisterPlot(AFFZAPlot* Plot)
{
    if (!IsValid(Plot))
    {
        return;
    }
    Plots.AddUnique(Plot);
    EnsurePolicyRegistered();
    NotifyLayersChanged();
}

void UFFZAZoneAuthoringSubsystem::UnregisterPlot(AFFZAPlot* Plot)
{
    const int32 Removed = Plots.Remove(Plot) +
        Plots.RemoveAll([](const TWeakObjectPtr<AFFZAPlot>& Item) { return !Item.IsValid(); });
    if (Removed > 0)
    {
        UnregisterPolicyIfUnused();
        NotifyLayersChanged();
    }
}

void UFFZAZoneAuthoringSubsystem::RegisterDistrict(AFFZADistrict* District)
{
    if (!IsValid(District))
    {
        return;
    }
    Districts.AddUnique(District);
    EnsurePolicyRegistered();
    NotifyLayersChanged();
}

void UFFZAZoneAuthoringSubsystem::UnregisterDistrict(AFFZADistrict* District)
{
    const int32 Removed = Districts.Remove(District) +
        Districts.RemoveAll([](const TWeakObjectPtr<AFFZADistrict>& Item) { return !Item.IsValid(); });
    if (Removed > 0)
    {
        UnregisterPolicyIfUnused();
        NotifyLayersChanged();
    }
}

TArray<AFFZAPlot*> UFFZAZoneAuthoringSubsystem::GetPlots() const
{
    TArray<AFFZAPlot*> Result;
    for (const TWeakObjectPtr<AFFZAPlot>& Plot : Plots)
    {
        if (AFFZAPlot* Valid = Plot.Get())
        {
            Result.Add(Valid);
        }
    }
    return Result;
}

TArray<AFFZADistrict*> UFFZAZoneAuthoringSubsystem::GetDistricts() const
{
    TArray<AFFZADistrict*> Result;
    for (const TWeakObjectPtr<AFFZADistrict>& District : Districts)
    {
        if (AFFZADistrict* Valid = District.Get())
        {
            Result.Add(Valid);
        }
    }
    return Result;
}

AFFZAPlot* UFFZAZoneAuthoringSubsystem::FindPlot(const FName PlotId) const
{
    if (PlotId.IsNone())
    {
        return nullptr;
    }
    for (const TWeakObjectPtr<AFFZAPlot>& Plot : Plots)
    {
        if (Plot.IsValid() && Plot->AreaId == PlotId)
        {
            return Plot.Get();
        }
    }
    return nullptr;
}

void UFFZAZoneAuthoringSubsystem::NotifyLayersChanged()
{
    OnAuthoringLayersChanged.Broadcast();
}

// ---------------------------------------------------------------------------------------------
// 정책 등록: 구획/구역이 있을 때만 등록해 다른 월드(테스트 등)에는 영향을 주지 않는다.
// ---------------------------------------------------------------------------------------------

void UFFZAZoneAuthoringSubsystem::EnsurePolicyRegistered()
{
    if (bPolicyRegistered || (Plots.IsEmpty() && Districts.IsEmpty()))
    {
        return;
    }
    UWorld* World = GetWorld();
    UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!ZoneSystem)
    {
        return;
    }
    if (!LayerPolicy)
    {
        LayerPolicy = NewObject<UFFZALayerPolicy>(this);
        LayerPolicy->SetOwner(this);
    }
    bPolicyRegistered = ZoneSystem->RegisterExternalPolicy(LayerPolicy);
    if (!bPolicyRegistered)
    {
        // 명령 실행/스냅샷 복원 중에는 등록이 거절된다. 다음 프레임에 다시 시도한다.
        UE_LOG(LogFFZoneAuthoring, Verbose, TEXT("구획/구역 정책 등록이 지금은 거절되었습니다. 다음 프레임에 다시 시도합니다."));
        SchedulePolicyRegistrationRetry();
    }
}

void UFFZAZoneAuthoringSubsystem::SchedulePolicyRegistrationRetry()
{
    UWorld* World = GetWorld();
    if (bPolicyRetryScheduled || !World)
    {
        return;
    }
    bPolicyRetryScheduled = true;
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        bPolicyRetryScheduled = false;
        EnsurePolicyRegistered();
    }));
}

void UFFZAZoneAuthoringSubsystem::UnregisterPolicyIfUnused()
{
    if (!bPolicyRegistered || !Plots.IsEmpty() || !Districts.IsEmpty())
    {
        return;
    }
    UWorld* World = GetWorld();
    if (UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr)
    {
        if (ZoneSystem->UnregisterExternalPolicy(LayerPolicy))
        {
            bPolicyRegistered = false;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// 구획 잠금 상태
// ---------------------------------------------------------------------------------------------

bool UFFZAZoneAuthoringSubsystem::IsPlotActorUnlocked(const AFFZAPlot& Plot) const
{
    if (!Plot.AreaId.IsNone())
    {
        if (const bool* Override = PlotUnlockOverrides.Find(Plot.AreaId))
        {
            return *Override;
        }
    }
    return Plot.bStartsUnlocked;
}

bool UFFZAZoneAuthoringSubsystem::IsPlotUnlocked(const FName PlotId) const
{
    if (const bool* Override = PlotUnlockOverrides.Find(PlotId))
    {
        return *Override;
    }
    const AFFZAPlot* Plot = FindPlot(PlotId);
    return Plot ? Plot->bStartsUnlocked : false;
}

bool UFFZAZoneAuthoringSubsystem::UnlockPlot(const FName PlotId)
{
    if (PlotId.IsNone() || IsPlotUnlocked(PlotId))
    {
        return false;
    }
    PlotUnlockOverrides.Add(PlotId, true);
    OnPlotStateChanged.Broadcast(PlotId, true);
    NotifyLayersChanged();
    return true;
}

bool UFFZAZoneAuthoringSubsystem::LockPlot(const FName PlotId)
{
    if (PlotId.IsNone() || !IsPlotUnlocked(PlotId))
    {
        return false;
    }
    PlotUnlockOverrides.Add(PlotId, false);
    OnPlotStateChanged.Broadcast(PlotId, false);
    NotifyLayersChanged();
    return true;
}

TArray<FName> UFFZAZoneAuthoringSubsystem::GetUnlockedPlotIds() const
{
    TArray<FName> Result;
    for (const TWeakObjectPtr<AFFZAPlot>& Plot : Plots)
    {
        if (Plot.IsValid() && !Plot->AreaId.IsNone() && IsPlotActorUnlocked(*Plot))
        {
            Result.AddUnique(Plot->AreaId);
        }
    }
    // 아직 레벨에 없는(스트리밍 전) 구획의 해제 기록도 잃지 않는다.
    for (const TPair<FName, bool>& Pair : PlotUnlockOverrides)
    {
        if (Pair.Value)
        {
            Result.AddUnique(Pair.Key);
        }
    }
    Result.Sort(FNameLexicalLess());
    return Result;
}

void UFFZAZoneAuthoringSubsystem::SetUnlockedPlotIds(const TArray<FName>& PlotIds)
{
    // 바뀐 구획만 알리기 위해 이전 상태를 기억한다.
    TMap<FName, bool> Previous;
    for (const TWeakObjectPtr<AFFZAPlot>& Plot : Plots)
    {
        if (Plot.IsValid() && !Plot->AreaId.IsNone())
        {
            Previous.Add(Plot->AreaId, IsPlotActorUnlocked(*Plot));
        }
    }
    for (const TPair<FName, bool>& Pair : PlotUnlockOverrides)
    {
        Previous.FindOrAdd(Pair.Key) = Pair.Value;
    }

    PlotUnlockOverrides.Reset();
    for (const TWeakObjectPtr<AFFZAPlot>& Plot : Plots)
    {
        if (Plot.IsValid() && !Plot->AreaId.IsNone())
        {
            PlotUnlockOverrides.Add(Plot->AreaId, false);
        }
    }
    for (const FName PlotId : PlotIds)
    {
        if (!PlotId.IsNone())
        {
            PlotUnlockOverrides.Add(PlotId, true);
        }
    }
    // 리스너가 UnlockPlot 등을 호출해도 안전하도록 복사본을 순회하고, 실제로 바뀐 것만 알린다.
    const TMap<FName, bool> Snapshot = PlotUnlockOverrides;
    for (const TPair<FName, bool>& Pair : Snapshot)
    {
        const bool* Before = Previous.Find(Pair.Key);
        if (!Before || *Before != Pair.Value)
        {
            OnPlotStateChanged.Broadcast(Pair.Key, Pair.Value);
        }
    }
    NotifyLayersChanged();
}

// ---------------------------------------------------------------------------------------------
// 배치 판정
// ---------------------------------------------------------------------------------------------

FFFZSPolicyDecision UFFZAZoneAuthoringSubsystem::EvaluatePlacement(const FFFZSPolicyContext& Context) const
{
    if (!IsSpatialCommand(Context.Command.Type) || (Plots.IsEmpty() && Districts.IsEmpty()))
    {
        return Allow();
    }

    // FF_ZoneSystem이 계산한 "실제로 차지할 영역"(본체+관람+대기열)을 그대로 쓴다. 규칙을 다시 계산하지 않는다.
    const TArray<FFFZSOrientedRect>& LocalRects = Context.Validation.ReservedOrientedRects.IsEmpty()
        ? Context.Validation.FootprintOrientedRects
        : Context.Validation.ReservedOrientedRects;
    if (LocalRects.IsEmpty())
    {
        return Allow();
    }

    const UWorld* World = GetWorld();
    const UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    // 강화(Upgrade) 등 명령에 ZoneId가 비어 있으면 기존 시설이 속한 Zone을 쓴다.
    const AFFZSFacilityActor* ExistingFacility = Context.FacilityActor.Get();
    const FName ZoneId = !Context.Command.ZoneId.IsNone() || !ExistingFacility
        ? Context.Command.ZoneId
        : ExistingFacility->GetZoneId();
    const AFFZSGameplayZone* Zone = ZoneSystem ? ZoneSystem->FindZone(ZoneId) : nullptr;
    if (!Zone)
    {
        // Zone을 모르면 FF_ZoneSystem 자체 검증이 거부한다. 여기서는 판단을 보류한다.
        return Allow();
    }

    // Zone 로컬 사각형 → 월드 4꼭짓점(cm). Zone은 Scale 1·Yaw만 쓰므로 위치/회전만 적용한다.
    const FTransform ZoneTransform = Zone->GetActorTransform();
    const double ZoneYaw = ZoneTransform.Rotator().Yaw;
    TArray<TArray<FVector2D>> WorldRects;
    WorldRects.Reserve(LocalRects.Num());
    for (const FFFZSOrientedRect& Local : LocalRects)
    {
        FFFZSOrientedRect WorldRect = Local;
        const FVector Center = ZoneTransform.TransformPosition(FVector(Local.CenterCell.X, Local.CenterCell.Y, 0.0));
        WorldRect.CenterCell = FVector2D(Center.X, Center.Y);
        WorldRect.YawDegrees = static_cast<float>(FRotator::NormalizeAxis(Local.YawDegrees + ZoneYaw));
        const FFZSOrientedGeometry::FCornerArray Corners = FFZSOrientedGeometry::GetCorners(WorldRect);
        TArray<FVector2D>& Out = WorldRects.AddDefaulted_GetRef();
        for (int32 Index = 0; Index < FFZSOrientedGeometry::CornerCount; ++Index)
        {
            Out.Add(Corners[Index]);
        }
    }

    const auto OverlapsPolygon = [&WorldRects](const TArray<FVector2D>& Polygon)
    {
        for (const TArray<FVector2D>& Rect : WorldRects)
        {
            if (FFZAPolygonMath::IntersectionAreaWithConvex(Polygon, Rect) > MinBlockingOverlapAreaCm2)
            {
                return true;
            }
        }
        return false;
    };

    // 1) 잠긴 구획
    for (const TWeakObjectPtr<AFFZAPlot>& WeakPlot : Plots)
    {
        const AFFZAPlot* Plot = WeakPlot.Get();
        if (!Plot || IsPlotActorUnlocked(*Plot))
        {
            continue;
        }
        if (OverlapsPolygon(Plot->GetWorldPolygon()))
        {
            return Reject(EFFZSPlacementFailure::ExternalPolicyRejected, FText::Format(
                LOCTEXT("LockedPlot", "잠긴 구획 '{0}'에는 지을 수 없습니다. 구획을 먼저 해제하세요."),
                Plot->GetResolvedDisplayName()));
        }
    }

    // 2) 구역 정책
    // 이동/강화는 기존 시설의 종류를, 설치는 명령에 담긴 사양의 종류를 쓴다.
    const EFFZSFacilityCategory Category = ExistingFacility
        ? ExistingFacility->GetFacilitySpec().Category
        : Context.Command.FacilitySpec.Category;
    for (const TWeakObjectPtr<AFFZADistrict>& WeakDistrict : Districts)
    {
        const AFFZADistrict* District = WeakDistrict.Get();
        if (!District || District->IsCategoryAllowed(Category))
        {
            continue;
        }
        if (OverlapsPolygon(District->GetWorldPolygon()))
        {
            return Reject(EFFZSPlacementFailure::CategoryNotAllowed, FText::Format(
                LOCTEXT("DistrictCategory", "'{0}' 구역에는 {1} 시설을 지을 수 없습니다."),
                District->GetResolvedDisplayName(), GetCategoryText(Category)));
        }
    }
    return Allow();
}

bool UFFZAZoneAuthoringSubsystem::CanBuildAtWorldLocation(const FVector& WorldLocation,
    const EFFZSFacilityCategory Category, FText& OutReason) const
{
    OutReason = FText::GetEmpty();
    for (const TWeakObjectPtr<AFFZAPlot>& WeakPlot : Plots)
    {
        const AFFZAPlot* Plot = WeakPlot.Get();
        if (Plot && !IsPlotActorUnlocked(*Plot) && Plot->ContainsWorldPoint(WorldLocation))
        {
            OutReason = FText::Format(LOCTEXT("HoverLockedPlot", "잠긴 구획: {0}"), Plot->GetResolvedDisplayName());
            return false;
        }
    }
    for (const TWeakObjectPtr<AFFZADistrict>& WeakDistrict : Districts)
    {
        const AFFZADistrict* District = WeakDistrict.Get();
        if (District && !District->IsCategoryAllowed(Category) && District->ContainsWorldPoint(WorldLocation))
        {
            OutReason = FText::Format(LOCTEXT("HoverDistrict", "'{0}' 구역에서 허용되지 않는 시설"), District->GetResolvedDisplayName());
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Zone 셀 지도
// ---------------------------------------------------------------------------------------------

void UFFZAZoneAuthoringSubsystem::GetZoneWorldPolygon(const AFFZSGameplayZone& Zone, TArray<FVector2D>& OutPolygon)
{
    const TArray<FVector2D> Local = Zone.GetBoundaryPolygonLocalCm();
    const FTransform Transform = Zone.GetActorTransform();
    OutPolygon.Reset(Local.Num());
    for (const FVector2D& Point : Local)
    {
        const FVector World = Transform.TransformPosition(FVector(Point.X, Point.Y, 0.0));
        OutPolygon.Add(FVector2D(World.X, World.Y));
    }
}

void UFFZAZoneAuthoringSubsystem::CollectWorldZones(const UWorld& World, TArray<AFFZSGameplayZone*>& OutZones,
    TArray<TArray<FVector2D>>& OutPolygons)
{
    OutZones.Reset();
    OutPolygons.Reset();
    for (TActorIterator<AFFZSGameplayZone> It(const_cast<UWorld*>(&World)); It; ++It)
    {
        if (IsValid(*It) && !It->IsTemplate())
        {
            OutZones.Add(*It);
        }
    }
    // 결과가 실행마다 같도록 ID(같으면 이름) 순으로 정렬한다.
    OutZones.Sort([](const AFFZSGameplayZone& A, const AFFZSGameplayZone& B)
    {
        if (A.ZoneId != B.ZoneId)
        {
            return A.ZoneId.LexicalLess(B.ZoneId);
        }
        return A.GetName() < B.GetName();
    });
    OutPolygons.Reserve(OutZones.Num());
    for (const AFFZSGameplayZone* Zone : OutZones)
    {
        GetZoneWorldPolygon(*Zone, OutPolygons.AddDefaulted_GetRef());
    }
}

void UFFZAZoneAuthoringSubsystem::RebuildZoneCellMap(const float CellSizeCm)
{
    CellMapCellSizeCm = FMath::Max(1.0f, CellSizeCm);
    bCellMapDirty = true;
    BuildCellMapIfNeeded();
}

void UFFZAZoneAuthoringSubsystem::BuildCellMapIfNeeded() const
{
    if (!bCellMapDirty)
    {
        return;
    }
    bCellMapDirty = false;
    CellMap.Reset();
    CellMapZoneIds.Reset();
    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    TArray<AFFZSGameplayZone*> Zones;
    TArray<TArray<FVector2D>> Polygons;
    CollectWorldZones(*World, Zones, Polygons);
    for (const AFFZSGameplayZone* Zone : Zones)
    {
        CellMapZoneIds.Add(Zone->ZoneId);
    }
    if (!Zones.IsEmpty())
    {
        CellMap.Build(Polygons, CellMapCellSizeCm);
    }
}

FName UFFZAZoneAuthoringSubsystem::FindZoneIdAtWorldLocation(const FVector& WorldLocation) const
{
    BuildCellMapIfNeeded();
    const int32 Index = CellMap.FindZoneIndexAt(FVector2D(WorldLocation.X, WorldLocation.Y));
    return CellMapZoneIds.IsValidIndex(Index) ? CellMapZoneIds[Index] : NAME_None;
}

const FFFZAZoneCellMap& UFFZAZoneAuthoringSubsystem::GetZoneCellMap() const
{
    BuildCellMapIfNeeded();
    return CellMap;
}

const TArray<FName>& UFFZAZoneAuthoringSubsystem::GetZoneCellMapIds() const
{
    BuildCellMapIfNeeded();
    return CellMapZoneIds;
}

#undef LOCTEXT_NAMESPACE
