// FF_FFZSFacilityActor.cpp

// 설치된 시설 한 개의 런타임 상태, Queue 컴포넌트, 비권위 화면 표현을 구현한다.
// 흐름: Subsystem이 승인한 Spec·Transform을 Actor와 컴포넌트에 반영한다.
// 불변식: 전역 점유와 명령 확정 권한은 UFFZSWorldSubsystem에만 있으며 Actor Scale은 1을 유지한다.
// 실패/성능: 표현 자산은 비동기로 준비하며 실패해도 권위 점유 상태를 바꾸지 않는다.
 

#include "FF_FFZSFacilityActor.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/StaticMesh.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FF_FFZSFacilityAnchorComponent.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"

//이 파일의 책임
 // 시설 한 개의 런타임 상태(배치, 운영 상태, 대기열)와 화면 표현(메시, 경계, 앵커)을 함께 유지한다.
 // 월드 전체의 권위 데이터는 UFFZSWorldSubsystem의 시설/압축 영역 레지스트리이다. 이 Actor는 명령을
 //  임의로 확정하는 주체가 아니며, 설치·이동·제거는 Subsystem의 검증 및 트랜잭션 흐름을 거쳐야 한다.

//핵심 불변식
 // 등록 가능한 시설은 유효한 InstanceId/ZoneId/Spec을 가지며 QuarterTurns는 항상 0~3으로 정규화한다.
 // Actor Scale은 항상 1이다. 크기는 FacilitySpec과 컴포넌트 Extent로 표현해야 그리드 계산과 일치한다.
 // 대기열에 Agent가 남아 있으면 이동/제거할 수 없다. 일반 Close는 먼저 Drain하고, 비워진 뒤
 // MovePending으로 전환한다. Emergency Close만 즉시 취소한다.
 // RuntimeAnchors와 VisualMesh는 파생된 표시 데이터다. 이를 바꿔도 Subsystem의 점유 영역은 바뀌지 않는다.
 

AFFZSFacilityActor::AFFZSFacilityActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(SceneRoot);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FootprintBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("FootprintBounds"));
    FootprintBounds->SetupAttachment(SceneRoot);
    FootprintBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FootprintBounds->SetGenerateOverlapEvents(false);
    FootprintBounds->SetHiddenInGame(true);

    QueueAreaBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("QueueAreaBounds"));
    QueueAreaBounds->SetupAttachment(SceneRoot);
    QueueAreaBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    QueueAreaBounds->SetGenerateOverlapEvents(false);
    QueueAreaBounds->SetHiddenInGame(true);

    QueueSpline = CreateDefaultSubobject<USplineComponent>(TEXT("QueueSpline"));
    QueueSpline->SetupAttachment(SceneRoot);
    QueueSpline->SetClosedLoop(false);

    QueueComponent = CreateDefaultSubobject<UFFZSFacilityQueueComponent>(TEXT("QueueComponent"));
}

// 복원 과정의 숨겨진 스테이징 Actor처럼 아직 ID가 없는 객체는 자동 등록하지 않는다.
// 유효한 객체만 등록하고, Subsystem은 같은 ID가 다른 Actor를 가리키는 중복을 거부한다.
void AFFZSFacilityActor::BeginPlay()
{
    Super::BeginPlay();
    if (FacilityInstanceId.IsValid())
    {
        if (UFFZSWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
        {
            Subsystem->RegisterFacility(this);
        }
    }
}

// 비동기 로드 완료가 수명 종료 뒤 표시 컴포넌트를 갱신하지 않도록 요청을 먼저 취소한다.
// 이어지는 등록 해제는 ID뿐 아니라 Actor 동일성도 확인하므로 교체된 Actor를 잘못 지우지 않는다.
void AFFZSFacilityActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (VisualLoadHandle.IsValid())
    {
        VisualLoadHandle->CancelHandle();
        VisualLoadHandle.Reset();
    }
    if (UWorld* World = GetWorld())
    {
        if (UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>())
        {
            Subsystem->UnregisterFacility(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

// 실패 가능한 데이터 검사를 어떤 멤버도 바꾸기 전에 끝낸다. 성공 뒤의 설정은 새로 생성되거나
// 스냅샷 복원용으로 스테이징된 Actor를 완성하는 단계이며, 월드 레지스트리 등록은 별도 단계다.
bool AFFZSFacilityActor::InitializeFacility(const FGuid NewInstanceId, const FFFZSFacilitySpec& NewSpec, const FName NewZoneId, FFFZSGridPlacement NewPlacement, const FTransform& WorldTransform, const EFFZSFacilityState InitialState)
{
    TArray<FText> Errors;
    if (!NewInstanceId.IsValid() || NewZoneId.IsNone() || !NewSpec.IsValid(Errors))
    {
        return false;
    }

    FacilityInstanceId = NewInstanceId;
    FacilitySpec = NewSpec;
    ZoneId = NewZoneId;
    NewPlacement.QuarterTurns = ((NewPlacement.QuarterTurns % 4) + 4) % 4;
    GridPlacement = NewPlacement;
    FacilityState = InitialState;
    bNeedsRelocation = false;
    SetActorTransform(WorldTransform);
    SetActorScale3D(FVector::OneVector);
    RebuildRuntimePresentation();
    QueueComponent->Configure(FacilitySpec.QueueRules);
    return true;
}

// 이 함수만 호출하면 Subsystem의 OccupiedFacilityRegions는 갱신되지 않는다. 정상 흐름에서는 ExecuteMove가
// 이전 셀 해제 -> 이 함수 -> 새 셀 점유 순서를 책임지고, 실패 시 이전 배치와 셀을 되돌린다.
bool AFFZSFacilityActor::ApplyPlacement(const FName NewZoneId, FFFZSGridPlacement NewPlacement, const FTransform& WorldTransform)
{
    if (NewZoneId.IsNone() || !CanMoveOrRemove())
    {
        return false;
    }
    ZoneId = NewZoneId;
    NewPlacement.QuarterTurns = ((NewPlacement.QuarterTurns % 4) + 4) % 4;
    GridPlacement = NewPlacement;
    bNeedsRelocation = false;
    SetActorTransform(WorldTransform);
    SetActorScale3D(FVector::OneVector);
    FacilityState = EFFZSFacilityState::Validated;
    bSetMovePendingWhenDrained = false;
    return true;
}

bool AFFZSFacilityActor::OpenFacility()
{
    if (FacilityState != EFFZSFacilityState::Validated && FacilityState != EFFZSFacilityState::Closed && FacilityState != EFFZSFacilityState::MovePending)
    {
        return false;
    }
    bSetMovePendingWhenDrained = false;
    FacilityState = EFFZSFacilityState::Active;
    QueueComponent->OpenQueue();
    return true;
}

// 일반 Close는 현재 이용자를 보존한 채 Drain을 시작하고, Emergency Close만 즉시 취소한다.
// Queue의 동기 콜백이 재진입할 수 있으므로 아래 코드는 전환 의도 플래그를 먼저 기록.
void AFFZSFacilityActor::CloseFacility(const bool bEmergency)
{
    if (FacilityState == EFFZSFacilityState::Removed)
    {
        return;
    }
    if (bEmergency)
    {
        bSetMovePendingWhenDrained = false;
        QueueComponent->EmergencyCancel();
        FacilityState = EFFZSFacilityState::Closed;
    }
    else
    {
        bSetMovePendingWhenDrained = true;
        FacilityState = EFFZSFacilityState::Closed;
        QueueComponent->BeginDrain();
        if (!QueueComponent->IsOccupied())
        {
            FacilityState = EFFZSFacilityState::MovePending;
        }
    }
}

void AFFZSFacilityActor::MarkRemoved()
{
    if (QueueComponent->IsOccupied())
    {
        return;
    }
    QueueComponent->EmergencyCancel();
    FacilityState = EFFZSFacilityState::Removed;
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
}

bool AFFZSFacilityActor::CanMoveOrRemove() const
{
    if (QueueComponent && QueueComponent->IsOccupied())
    {
        return false;
    }
    return FacilityState == EFFZSFacilityState::Planned ||
        FacilityState == EFFZSFacilityState::Validated ||
        FacilityState == EFFZSFacilityState::Closed ||
        FacilityState == EFFZSFacilityState::MovePending;
}

bool AFFZSFacilityActor::GetAnchorWorldTransform(const EFFZSAnchorType Type, const int32 Index, FTransform& OutTransform) const
{
    for (const UFFZSFacilityAnchorComponent* Anchor : RuntimeAnchors)
    {
        if (Anchor && Anchor->AnchorType == Type && Anchor->AnchorIndex == Index)
        {
            OutTransform = Anchor->GetComponentTransform();
            return true;
        }
    }
    return false;
}

// Actor와 Queue 한 개의 구성요소 스냅샷이다. 월드 전체의 원자적 저장/복원 경계는
// UFFZSWorldSubsystem::CaptureWorldSnapshot/RestoreWorldSnapshot이 제공.
FFFZSFacilityInstanceSnapshot AFFZSFacilityActor::CaptureInstanceSnapshot() const
{
    FFFZSFacilityInstanceSnapshot Snapshot;
    Snapshot.FacilityInstanceId = FacilityInstanceId;
    Snapshot.FacilitySpec = FacilitySpec;
    Snapshot.ZoneId = ZoneId;
    Snapshot.Placement = GridPlacement;
    Snapshot.State = FacilityState;
    Snapshot.Queue = QueueComponent->CaptureSnapshot();
    Snapshot.WorldTransform = GetActorTransform();
    Snapshot.bNeedsRelocation = bNeedsRelocation;
    return Snapshot;
}

FFFZSFacilityReadModel AFFZSFacilityActor::GetReadModel() const
{
    FFFZSFacilityReadModel Result;
    Result.FacilityInstanceId = FacilityInstanceId;
    Result.FacilityDefinitionId = FacilitySpec.FacilityDefinitionId;
    Result.ZoneId = ZoneId;
    Result.Category = FacilitySpec.Category;
    Result.State = FacilityState;
    Result.QueueState = QueueComponent->GetQueueState();
    Result.bNeedsRelocation = bNeedsRelocation;
    Result.WaitingCount = QueueComponent->GetWaitingCount();
    Result.ActiveServiceCount = QueueComponent->GetActiveServiceCount();
    Result.AverageCompletedWaitSeconds = QueueComponent->GetAverageCompletedWaitSeconds();
    return Result;
}

// 아래 컴포넌트들은 FacilitySpec에서 재생성할 수 있는 표현 캐시다. 권위 배치 판정은 이 Box/Spline의
// 충돌 이벤트가 아니라 Subsystem의 그리드, 보호 구역, 월드 충돌 검사를 사용.
void AFFZSFacilityActor::RebuildRuntimePresentation()
{
    const float CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const FVector FootprintExtent(
        FacilitySpec.FootprintSizeCells.X * CellSizeCm * 0.5f,
        FacilitySpec.FootprintSizeCells.Y * CellSizeCm * 0.5f,
        FacilitySpec.PlacementObstacleHeightCm * 0.5f);
    FootprintBounds->SetBoxExtent(FootprintExtent);
    FootprintBounds->SetRelativeLocation(FVector(0.0f, 0.0f, FootprintExtent.Z));

    RequestVisualMesh();

    QueueSpline->ClearSplinePoints(false);
    for (int32 Index = 0; Index < FacilitySpec.QueueSplinePointsCm.Num(); ++Index)
    {
        QueueSpline->AddSplinePoint(FacilitySpec.QueueSplinePointsCm[Index], ESplineCoordinateSpace::Local, false);
        QueueSpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    QueueSpline->UpdateSpline();

    const TArray<FFFZSCellRect> QueueRects = UFFZSGridLibrary::GetQueueRects(
        FacilitySpec, FFFZSGridPlacement{});
    if (!QueueRects.IsEmpty())
    {
        FIntPoint Min = QueueRects[0].Min;
        FIntPoint Max = QueueRects[0].Max;
        for (const FFFZSCellRect& Rect : QueueRects)
        {
            Min.X = FMath::Min(Min.X, Rect.Min.X);
            Min.Y = FMath::Min(Min.Y, Rect.Min.Y);
            Max.X = FMath::Max(Max.X, Rect.Max.X);
            Max.Y = FMath::Max(Max.Y, Rect.Max.Y);
        }
        const FVector LowerLeftCm(
            Min.X * CellSizeCm - FootprintExtent.X,
            Min.Y * CellSizeCm - FootprintExtent.Y,
            0.0f);
        const FVector UpperRightCm(
            Max.X * CellSizeCm - FootprintExtent.X,
            Max.Y * CellSizeCm - FootprintExtent.Y,
            FacilitySpec.PlacementObstacleHeightCm);
        QueueAreaBounds->SetRelativeLocation((LowerLeftCm + UpperRightCm) * 0.5f);
        QueueAreaBounds->SetBoxExtent((UpperRightCm - LowerLeftCm) * 0.5f);
    }
    else
    {
        // 재구성 전의 Queue Box가 남지 않게 명시적으로 비운다. 권위 점유에는 사용하지 않는다.
        QueueAreaBounds->SetRelativeLocation(FVector::ZeroVector);
        QueueAreaBounds->SetBoxExtent(FVector::ZeroVector);
    }

    RebuildAnchors();
}

// 비동기 경합 방어: 새 요청 전에 기존 Handle을 취소하고 화면을 비운다. 취소는 자원 절약과 수명
// 관리 목적이며, 완료 순서가 뒤바뀌더라도 콜백은 요청 당시 포인터를 캡처하지 않고 현재 Spec의
// SoftObjectPtr을 다시 읽는다. 따라서 오래된 요청이 완료되어도 오래된 메시를 직접 설치하지 않는다.
void AFFZSFacilityActor::RequestVisualMesh()
{
    if (VisualLoadHandle.IsValid())
    {
        VisualLoadHandle->CancelHandle();
        VisualLoadHandle.Reset();
    }

    VisualMesh->SetStaticMesh(nullptr);
    if (FacilitySpec.VisualMesh.IsNull())
    {
        return;
    }
    if (UStaticMesh* LoadedMesh = FacilitySpec.VisualMesh.Get())
    {
        VisualMesh->SetStaticMesh(LoadedMesh);
        return;
    }

    VisualLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        FacilitySpec.VisualMesh.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &AFFZSFacilityActor::ApplyLoadedVisualMesh));
}

// 현재 FacilitySpec이 가리키는 메시만 적용한다. CreateUObject 델리게이트와 EndPlay의 취소가
// Actor 수명 종료 후 접근을 막고, 이전 요청의 로드 결과를 그대로 사용하는 코드는 두지 않는다.
void AFFZSFacilityActor::ApplyLoadedVisualMesh()
{
    VisualLoadHandle.Reset();
    if (UStaticMesh* LoadedMesh = FacilitySpec.VisualMesh.Get())
    {
        VisualMesh->SetStaticMesh(LoadedMesh);
    }
}

// 앵커 정의가 바뀌었을 때 이름/인덱스가 남지 않도록 기존 파생 컴포넌트를 모두 폐기한 뒤 재구성한다.
// RuntimeAnchors 배열은 조회 가속용 캐시이며 FacilitySpec.Anchors가 원본 데이터다.
void AFFZSFacilityActor::RebuildAnchors()
{
    for (UFFZSFacilityAnchorComponent* Anchor : RuntimeAnchors)
    {
        if (Anchor)
        {
            Anchor->DestroyComponent();
        }
    }
    RuntimeAnchors.Reset();

    for (const FFFZSAnchorDefinition& Definition : FacilitySpec.Anchors)
    {
        const FString TypeName = StaticEnum<EFFZSAnchorType>()->GetNameStringByValue(static_cast<int64>(Definition.Type));
        const FName ComponentName(*FString::Printf(TEXT("%sAnchor_%d"), *TypeName, Definition.Index));
        UFFZSFacilityAnchorComponent* Anchor = NewObject<UFFZSFacilityAnchorComponent>(this, ComponentName);
        AddInstanceComponent(Anchor);
        Anchor->SetupAttachment(SceneRoot);
        Anchor->AnchorType = Definition.Type;
        Anchor->AnchorIndex = Definition.Index;
        Anchor->SetRelativeTransform(Definition.LocalTransform);
        Anchor->RegisterComponent();
        RuntimeAnchors.Add(Anchor);
    }
}

void AFFZSFacilityActor::HandleQueueStateChanged(const EFFZSQueueState PreviousState, const EFFZSQueueState NewState)
{
    if (bSetMovePendingWhenDrained && NewState == EFFZSQueueState::Closed && !QueueComponent->IsOccupied())
    {
        FacilityState = EFFZSFacilityState::MovePending;
    }
}
