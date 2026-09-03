// FF_FFZSZoneAccessSubsystem.cpp
// 설계 근거와 불변식은 FF_FFZSZoneAccessSubsystem.h 상단 주석에 있다.

#include "FF_FFZSZoneAccessSubsystem.h"

#include "FF_FFZSZoneActor.h"

#include "AI/Navigation/NavigationTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY(LogFFZSZoneAccess);

namespace FFZSZoneAccess
{
    // 앵커 투영 범위. UFF_CrowdDecisionSubsystem이 목적지 앵커를 투영할 때 쓰는
    // FFF_CrowdNav::GetAnchorProjectionExtentCm()과 같은 값이어야 한다. 다르면 여기서 고른
    // 지점과 군중이 실제로 걸어가는 지점이 어긋난다. 게임 모듈에 의존하지 않으려고 값을
    // 복제했으므로 그쪽이 바뀌면 여기도 같이 바꾼다.
    static const FVector AnchorProjectionExtentCm(300.0, 300.0, 2000.0);

    static TAutoConsoleVariable<int32> CVarEnabled(
        TEXT("ffzs.ZoneAccess.Enabled"), 1,
        TEXT("1이면 게임 시작 시 존 접근점을 걸어갈 수 있는 지점으로 맞춘다. 0이면 측정만 한다."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVarSearchPasses(
        TEXT("ffzs.ZoneAccess.SearchPasses"), 3,
        TEXT("접근점 탐색 단계 수. 존 크기의 1/8 간격에서 시작해 단계마다 간격을 절반으로 줄인다. ")
        TEXT("같은 원점에서 반으로 줄이므로 뒤 단계는 앞 단계 표본을 모두 포함한다."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVarMaxSamplesPerZone(
        TEXT("ffzs.ZoneAccess.MaxSamplesPerZone"), 2048,
        TEXT("한 존에서 시험할 표본 수 상한. 시작 시 경로 쿼리 비용을 묶어 둔다."),
        ECVF_Default);

    static TAutoConsoleVariable<float> CVarWarnOffsetCm(
        TEXT("ffzs.ZoneAccess.WarnOffsetCm"), 3000.0f,
        TEXT("접근점이 존 중심에서 이 거리보다 멀어지면 경고한다(cm). 존 대부분이 끊긴 지형이라는 신호다."),
        ECVF_Default);

}

bool UFFZSZoneAccessSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    // 에디터 월드에서 존 속성을 바꾸면 레벨 에셋이 dirty해져 소유자의 저장 상태를 흔든다.
    const UWorld* World = Cast<UWorld>(Outer);
    return World != nullptr
        && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UFFZSZoneAccessSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    RepairZoneAccessPoints();
    LogReachabilityReport();
}

bool UFFZSZoneAccessSubsystem::ProjectToNavMesh(const FVector& Point, FVector& OutProjected) const
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (NavSys == nullptr)
    {
        return false;
    }

    FNavLocation Projected;
    if (!NavSys->ProjectPointToNavigation(Point, Projected, FFZSZoneAccess::AnchorProjectionExtentCm))
    {
        return false;
    }

    OutProjected = Projected.Location;
    return true;
}

bool UFFZSZoneAccessSubsystem::HasCompletePath(const FVector& A, const FVector& B) const
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (NavSys == nullptr)
    {
        return false;
    }

    // GetPathLength는 부분 경로를 Fail로 돌려준다. 즉 Success는 '완전 경로가 있다'는 뜻이다.
    FVector::FReal LengthCm = 0.0;
    return NavSys->GetPathLength(A, B, LengthCm) == ENavigationQueryResult::Success;
}

bool UFFZSZoneAccessSubsystem::ResolveExitLocation(FVector& OutExit) const
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return false;
    }

    // 출구는 AFF_SpawnActor의 위치 + Temp_ExitOffsetFromOrigin이다(AFF_SpawnActor::BeginPlay와 동일).
    // 그 클래스는 게임 모듈에 있어 플러그인이 링크할 수 없으므로 리플렉션으로 읽는다.
    static const FName SpawnActorClassName(TEXT("FF_SpawnActor"));
    static const FName ExitOffsetPropertyName(TEXT("Temp_ExitOffsetFromOrigin"));

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        UClass* ActorClass = Actor->GetClass();
        bool bIsSpawnActor = false;
        for (const UClass* Super = ActorClass; Super != nullptr; Super = Super->GetSuperClass())
        {
            if (Super->GetFName() == SpawnActorClassName)
            {
                bIsSpawnActor = true;
                break;
            }
        }

        if (!bIsSpawnActor)
        {
            continue;
        }

        FVector Offset(-4000.0, 0.0, 0.0);   // AFF_SpawnActor의 기본값
        if (const FStructProperty* Prop =
                CastField<FStructProperty>(ActorClass->FindPropertyByName(ExitOffsetPropertyName)))
        {
            if (Prop->Struct == TBaseStructure<FVector>::Get())
            {
                Offset = *Prop->ContainerPtrToValuePtr<FVector>(Actor);
            }
        }
        else
        {
            UE_LOG(LogFFZSZoneAccess, Warning,
                TEXT("%s에서 Temp_ExitOffsetFromOrigin을 못 찾아 기본 오프셋으로 출구를 잡는다"),
                *Actor->GetName());
        }

        OutExit = Actor->GetActorLocation() + Offset;
        return true;
    }

    return false;
}

FVector UFFZSZoneAccessSubsystem::GetZoneRepresentativePoint(const AFFZSGameplayZone& Zone) const
{
    // 소비 측(UFF_CrowdDecisionSubsystem)과 같은 규칙이어야 한다.
    return Zone.EntryExitAnchors.IsEmpty()
        ? Zone.GetActorLocation()
        : Zone.GetActorTransform().TransformPosition(Zone.EntryExitAnchors[0].GetLocation());
}

bool UFFZSZoneAccessSubsystem::IsPointInPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Point)
{
    // 표준 ray casting. 경계에 정확히 걸친 점의 판정은 중요하지 않다 — 후보를 하나 잃을 뿐이다.
    bool bInside = false;
    const int32 Count = Polygon.Num();
    for (int32 Index = 0, Prev = Count - 1; Index < Count; Prev = Index++)
    {
        const FVector2D& A = Polygon[Index];
        const FVector2D& B = Polygon[Prev];
        if (((A.Y > Point.Y) != (B.Y > Point.Y))
            && (Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X))
        {
            bInside = !bInside;
        }
    }
    return bInside;
}

bool UFFZSZoneAccessSubsystem::FindReachableAccessPoint(const AFFZSGameplayZone& Zone,
                                                        const FVector& ExitNav,
                                                        FVector& OutWorld,
                                                        FVector& OutMiddleWorld) const
{
    const TArray<FVector2D> Polygon = Zone.GetBoundaryPolygonLocalCm();
    if (Polygon.Num() < 3)
    {
        return false;
    }

    FVector2D Min = Polygon[0];
    FVector2D Max = Polygon[0];
    FVector2D Middle = FVector2D::ZeroVector;
    for (const FVector2D& Point : Polygon)
    {
        Min.X = FMath::Min(Min.X, Point.X);
        Min.Y = FMath::Min(Min.Y, Point.Y);
        Max.X = FMath::Max(Max.X, Point.X);
        Max.Y = FMath::Max(Max.Y, Point.Y);
        Middle += Point;
    }
    Middle /= static_cast<double>(Polygon.Num());

    const FTransform ToWorld = Zone.GetActorTransform();
    OutMiddleWorld = ToWorld.TransformPosition(FVector(Middle.X, Middle.Y, 0.0));

    const double MaxDimCm = FMath::Max(1.0, FMath::Max(
        static_cast<double>(Max.X - Min.X), static_cast<double>(Max.Y - Min.Y)));
    const int32 Passes = FMath::Clamp(FFZSZoneAccess::CVarSearchPasses.GetValueOnGameThread(), 1, 8);
    const int32 MaxSamples = FMath::Max(1, FFZSZoneAccess::CVarMaxSamplesPerZone.GetValueOnGameThread());

    int32 SamplesUsed = 0;

    // 존 한가운데를 먼저 본다. 존을 가장 잘 대표하는 지점이고, 대부분의 존은 여기서 끝난다.
    {
        FVector Projected;
        ++SamplesUsed;
        if (ProjectToNavMesh(OutMiddleWorld, Projected) && HasCompletePath(Projected, ExitNav))
        {
            OutWorld = Projected;
            return true;
        }
    }

    // 단계마다 같은 원점(Min)에서 간격을 절반으로 줄인다. 그래서 뒤 단계의 표본 집합은 앞
    // 단계를 그대로 포함하고, 결과가 격자 정렬 운에 좌우되지 않는다.
    for (int32 Pass = 0; Pass < Passes; ++Pass)
    {
        const double StepCm = MaxDimCm / (8.0 * static_cast<double>(1 << Pass));
        if (StepCm < 1.0)
        {
            break;
        }

        TArray<FVector2D> Candidates;
        for (double Y = Min.Y; Y <= Max.Y; Y += StepCm)
        {
            for (double X = Min.X; X <= Max.X; X += StepCm)
            {
                const FVector2D Local(X, Y);
                if (IsPointInPolygon(Polygon, Local))
                {
                    Candidates.Add(Local);
                }
            }
        }

        // 접근점이 존 한가운데에서 멀어질수록 그 존을 대표하지 못한다. 걸어갈 수 있는 지점
        // 중 한가운데에 가장 가까운 것을 쓴다.
        Candidates.Sort([&Middle](const FVector2D& A, const FVector2D& B)
        {
            return (A - Middle).SizeSquared() < (B - Middle).SizeSquared();
        });

        for (const FVector2D& Candidate : Candidates)
        {
            if (SamplesUsed >= MaxSamples)
            {
                return false;
            }
            ++SamplesUsed;

            const FVector LocalPoint(Candidate.X, Candidate.Y, 0.0);
            FVector Projected;
            if (!ProjectToNavMesh(ToWorld.TransformPosition(LocalPoint), Projected))
            {
                continue;
            }

            if (HasCompletePath(Projected, ExitNav))
            {
                OutWorld = Projected;
                return true;
            }
        }
    }

    return false;
}

int32 UFFZSZoneAccessSubsystem::RepairZoneAccessPoints()
{
    Repairs.Reset();

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return 0;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (NavSys == nullptr || NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) == nullptr)
    {
        UE_LOG(LogFFZSZoneAccess, Warning,
            TEXT("NavMesh가 없어 접근점을 고칠 수 없다 — 레벨에 NavMeshBoundsVolume과 구운 NavMesh가 있는지 확인할 것"));
        return 0;
    }

    FVector ExitWorld;
    if (!ResolveExitLocation(ExitWorld))
    {
        UE_LOG(LogFFZSZoneAccess, Warning,
            TEXT("AFF_SpawnActor가 없어 퇴장 출구를 못 잡는다 — 접근점을 고치지 않는다"));
        return 0;
    }

    FVector ExitNav;
    if (!ProjectToNavMesh(ExitWorld, ExitNav))
    {
        UE_LOG(LogFFZSZoneAccess, Error,
            TEXT("출구 (%s)가 NavMesh 밖이다 — 출구 위치부터 고쳐야 한다"), *ExitWorld.ToCompactString());
        return 0;
    }

    if (FFZSZoneAccess::CVarEnabled.GetValueOnGameThread() == 0)
    {
        UE_LOG(LogFFZSZoneAccess, Warning,
            TEXT("ffzs.ZoneAccess.Enabled=0이라 측정만 하고 접근점을 고치지 않는다"));
        return 0;
    }

    const double StartSeconds = FPlatformTime::Seconds();
    int32 ZoneCount = 0;
    int32 AlreadyReachable = 0;
    int32 Authored = 0;
    TArray<FString> Stranded;

    for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
    {
        AFFZSGameplayZone* Zone = *It;
        if (!IsValid(Zone))
        {
            continue;
        }
        ++ZoneCount;

        // 사람이 저작한 앵커는 의도가 있으므로 절대 덮어쓰지 않는다.
        if (!Zone->EntryExitAnchors.IsEmpty())
        {
            ++Authored;
            continue;
        }

        // 지금 쓰이는 대표점은 액터 위치다. 그런데 존 경계 폴리곤은 로컬 (0,0)에서 시작하므로
        // 액터 위치는 존의 한가운데가 아니라 모서리다 — 네 존이 만나는 꼭짓점이라 하필 지형
        // 이음새에 걸리기 쉽다. 도달 불가 존이 많았던 직접적인 원인 중 하나다.
        const FVector Corner = Zone->GetActorLocation();
        FVector CornerNav;
        if (ProjectToNavMesh(Corner, CornerNav) && HasCompletePath(CornerNav, ExitNav))
        {
            ++AlreadyReachable;
            continue;
        }

        FVector AccessWorld;
        FVector MiddleWorld;
        if (!FindReachableAccessPoint(*Zone, ExitNav, AccessWorld, MiddleWorld))
        {
            // 경계 전체가 출구와 다른 지형 조각에 있다. 데이터로는 고칠 수 없는 자리다.
            Stranded.Add(FString::Printf(TEXT("%s(%.0f, %.0f)"),
                *Zone->ZoneId.ToString(), Corner.X, Corner.Y));
            continue;
        }

        Zone->EntryExitAnchors.Reset(1);
        Zone->EntryExitAnchors.Add(FTransform(Zone->GetActorTransform().InverseTransformPosition(AccessWorld)));

        FFFZSZoneAccessRepair Repair;
        Repair.ZoneId = Zone->ZoneId;
        Repair.CentreWorld = MiddleWorld;
        Repair.AccessWorld = AccessWorld;
        Repair.OffsetCm = static_cast<float>(FVector::Dist2D(MiddleWorld, AccessWorld));
        Repairs.Add(Repair);

        UE_LOG(LogFFZSZoneAccess, Verbose,
            TEXT("%s 접근점: 모서리 (%.0f, %.0f) -> (%.0f, %.0f, %.0f)  존 한가운데에서 %.0fcm"),
            *Zone->ZoneId.ToString(), Corner.X, Corner.Y,
            AccessWorld.X, AccessWorld.Y, AccessWorld.Z, Repair.OffsetCm);
    }

    UE_LOG(LogFFZSZoneAccess, Log,
        TEXT("존 %d개: 이미 도달 가능 %d, 저작된 앵커 %d, 접근점 고침 %d, 고칠 수 없음 %d (%.0fms)"),
        ZoneCount, AlreadyReachable, Authored, Repairs.Num(), Stranded.Num(),
        (FPlatformTime::Seconds() - StartSeconds) * 1000.0);

    // 접근점이 존 중심에서 크게 밀린 존은 경계 대부분이 끊긴 지형이라는 뜻이다. 통행은
    // 복구됐지만 NPC가 존 가장자리로만 모이므로 레벨에서 확인하는 편이 좋다.
    const float WarnOffsetCm = FMath::Max(0.0f, FFZSZoneAccess::CVarWarnOffsetCm.GetValueOnGameThread());
    for (const FFFZSZoneAccessRepair& Repair : Repairs)
    {
        if (Repair.OffsetCm > WarnOffsetCm)
        {
            UE_LOG(LogFFZSZoneAccess, Warning,
                TEXT("%s의 접근점이 존 한가운데에서 %.0fm 떨어졌다 — 존 대부분이 끊긴 지형이다. ")
                TEXT("한가운데 (%.0f, %.0f) / 접근점 (%.0f, %.0f)"),
                *Repair.ZoneId.ToString(), Repair.OffsetCm * 0.01f,
                Repair.CentreWorld.X, Repair.CentreWorld.Y,
                Repair.AccessWorld.X, Repair.AccessWorld.Y);
        }
    }

    if (!Stranded.IsEmpty())
    {
        // 경계 안에 출구와 이어진 지면이 한 점도 없는 존이다. 레벨에서 단차에 램프를 놓거나
        // 존을 이어진 지형 위로 옮겨야 한다 — 데이터만으로는 해결되지 않는다.
        UE_LOG(LogFFZSZoneAccess, Error,
            TEXT("★ 존 %d개는 경계 전체가 출구와 끊긴 지형 조각에 있다 — 레벨 작업이 필요하다: %s"),
            Stranded.Num(), *FString::Join(Stranded, TEXT(", ")));
    }

    return Repairs.Num();
}

void UFFZSZoneAccessSubsystem::LogReachabilityReport() const
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    FVector ExitWorld;
    FVector ExitNav;
    if (!ResolveExitLocation(ExitWorld) || !ProjectToNavMesh(ExitWorld, ExitNav))
    {
        UE_LOG(LogFFZSZoneAccess, Warning, TEXT("출구를 몰라 도달 리포트를 만들 수 없다"));
        return;
    }

    TArray<FName> ZoneIds;
    TArray<FVector> Points;
    for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
    {
        const AFFZSGameplayZone* Zone = *It;
        if (!IsValid(Zone))
        {
            continue;
        }

        FVector Projected;
        if (!ProjectToNavMesh(GetZoneRepresentativePoint(*Zone), Projected))
        {
            UE_LOG(LogFFZSZoneAccess, Warning,
                TEXT("존 %s의 대표점이 NavMesh 밖이다"), *Zone->ZoneId.ToString());
            continue;
        }

        ZoneIds.Add(Zone->ZoneId);
        Points.Add(Projected);
    }

    // 존 대표점들이 몇 개의 NavMesh 조각에 나뉘어 있는지. 1이면 모두 서로 걸어갈 수 있다.
    TArray<int32> Parent;
    Parent.SetNum(Points.Num());
    for (int32 Index = 0; Index < Parent.Num(); ++Index)
    {
        Parent[Index] = Index;
    }

    TFunction<int32(int32)> FindRoot = [&Parent, &FindRoot](int32 X) -> int32
    {
        while (Parent[X] != X)
        {
            Parent[X] = Parent[Parent[X]];
            X = Parent[X];
        }
        return X;
    };

    for (int32 I = 0; I < Points.Num(); ++I)
    {
        for (int32 J = I + 1; J < Points.Num(); ++J)
        {
            if (FindRoot(I) == FindRoot(J))
            {
                continue;
            }
            if (HasCompletePath(Points[I], Points[J]))
            {
                Parent[FindRoot(J)] = FindRoot(I);
            }
        }
    }

    TSet<int32> Islands;
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        Islands.Add(FindRoot(Index));
    }

    TArray<FString> Unreachable;
    int32 Reachable = 0;
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        if (HasCompletePath(Points[Index], ExitNav))
        {
            ++Reachable;
        }
        else
        {
            Unreachable.Add(ZoneIds[Index].ToString());
        }
    }

    UE_LOG(LogFFZSZoneAccess, Log,
        TEXT("존 대표점 %d개가 NavMesh 조각 %d개에 걸쳐 있다"), Points.Num(), Islands.Num());

    if (Unreachable.IsEmpty())
    {
        UE_LOG(LogFFZSZoneAccess, Log,
            TEXT("★ 존 %d/%d개가 출구까지 완전 경로를 가진다"), Reachable, Points.Num());
    }
    else
    {
        UE_LOG(LogFFZSZoneAccess, Error,
            TEXT("★ 존 %d/%d개만 출구에 닿는다. 남은 %d개: %s"),
            Reachable, Points.Num(), Unreachable.Num(), *FString::Join(Unreachable, TEXT(", ")));
    }
}
