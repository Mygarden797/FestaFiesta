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

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "FF_FFZSCollision.h"
#include "FF_FFZSFacilityAnchorComponent.h"
#include "FF_FFZSFacilityLightingRig.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSNavArea_QueueLane.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSVisualGeometry.h"
#include "FF_FFZSWorldSubsystem.h"
#include "SceneManagement.h"

namespace
{
    const FColor LiveSelectionIndicatorColor(20, 242, 224);
    constexpr float HoveredSelectionIndicatorThickness = 3.0f;
    constexpr float SelectedSelectionIndicatorThickness = 5.0f;

    FVector MakeFacilityFootprintExtent(const FFFZSFacilitySpec& Spec)
    {
        const float CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
        return FVector(
            Spec.FootprintSizeCells.X * CellSizeCm * 0.5f,
            Spec.FootprintSizeCells.Y * CellSizeCm * 0.5f,
            Spec.PlacementObstacleHeightCm * 0.5f);
    }

    // 예약된 Queue 사각형 전체를 감싸는 외접 박스를 로컬 cm 공간의 (중심, 반치수)로 돌려준다.
    // 예약 사각형이 하나도 없으면 false다.
    //
    // 셀 좌표는 footprint 좌하단을 원점으로 세지만 Actor 원점은 footprint 중심이다. 그래서
    // 셀->로컬 변환에서 footprint 반치수를 빼야 두 좌표계가 맞는다 — 이 뺄셈이 빠지면 레인이
    // 시설 크기의 절반만큼 어긋난 자리에 생긴다.
    //
    // 두 곳에서 부른다: 표현 재구축(RebuildRuntimePresentation)과 등록 시점 기하 복구
    // (SetNavigationObstacleEnabled). 레벨에 미리 배치된 시설은 후자만 거치므로 한쪽만
    // 계산하면 그 시설의 레인 비용이 영원히 켜지지 않는다.
    bool TryMakeQueueLaneLocalBox(const FFFZSFacilitySpec& Spec, FVector& OutCenterCm, FVector& OutExtentCm)
    {
        const TArray<FFFZSCellRect> QueueRects = UFFZSGridLibrary::GetQueueRects(Spec, FFFZSGridPlacement{});
        if (QueueRects.IsEmpty())
        {
            return false;
        }

        FIntPoint Min = QueueRects[0].Min;
        FIntPoint Max = QueueRects[0].Max;
        for (const FFFZSCellRect& Rect : QueueRects)
        {
            Min.X = FMath::Min(Min.X, Rect.Min.X);
            Min.Y = FMath::Min(Min.Y, Rect.Min.Y);
            Max.X = FMath::Max(Max.X, Rect.Max.X);
            Max.Y = FMath::Max(Max.Y, Rect.Max.Y);
        }

        const float CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
        const FVector FootprintExtent = MakeFacilityFootprintExtent(Spec);
        const FVector LowerLeftCm(
            Min.X * CellSizeCm - FootprintExtent.X,
            Min.Y * CellSizeCm - FootprintExtent.Y,
            0.0f);
        const FVector UpperRightCm(
            Max.X * CellSizeCm - FootprintExtent.X,
            Max.Y * CellSizeCm - FootprintExtent.Y,
            Spec.PlacementObstacleHeightCm);

        OutCenterCm = (LowerLeftCm + UpperRightCm) * 0.5f;
        OutExtentCm = (UpperRightCm - LowerLeftCm) * 0.5f;
        return true;
    }
}

//이 파일의 책임
 // 시설 한 개의 런타임 상태(배치, 운영 상태, 대기열)와 화면 표현(메시, 경계, 앵커)을 함께 유지한다.
 // 월드 전체의 권위 데이터는 UFFZSWorldSubsystem의 시설/압축 영역 레지스트리이다. 이 Actor는 명령을
 //  임의로 확정하는 주체가 아니며, 설치·이동·제거는 Subsystem의 검증 및 트랜잭션 흐름을 거쳐야 한다.

//핵심 불변식
 // 등록 가능한 시설은 유효한 InstanceId/ZoneId/Spec을 가지며 Yaw는 항상 0..35999 centidegree로 정규화한다.
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
    RefreshPlayerCollision();

    FacilityLightingComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("FacilityLighting"));
    FacilityLightingComponent->SetupAttachment(VisualMesh);
    FacilityLightingComponent->SetMobility(EComponentMobility::Movable);
    FacilityLightingComponent->SetCanEverAffectNavigation(false);

    FacilityAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("FacilityAudio"));
    FacilityAudioComponent->SetupAttachment(SceneRoot);
    FacilityAudioComponent->SetAutoActivate(false);
    FacilityAudioComponent->bAutoDestroy = false;
    FacilityAudioComponent->bStopWhenOwnerDestroyed = true;
    FacilityAudioComponent->bAllowSpatialization = true;

    SelectionProxy = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionProxy"));
    SelectionProxy->SetupAttachment(SceneRoot);
    // 유효한 FacilitySpec이 적용되기 전의 기본 Box가 선택되는 짧은 구간을 없앤다.
    SelectionProxy->SetBoxExtent(FVector::ZeroVector);
    SelectionProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SelectionProxy->SetGenerateOverlapEvents(false);
    SelectionProxy->SetCanEverAffectNavigation(false);
    SelectionProxy->SetSimulatePhysics(false);
    SelectionProxy->SetEnableGravity(false);
    SelectionProxy->SetNotifyRigidBodyCollision(false);
    SelectionProxy->bDrawOnlyIfSelected = false;
    SelectionProxy->SetVisibility(true);
    SelectionProxy->SetHiddenInGame(true);

    FootprintBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("FootprintBounds"));
    FootprintBounds->SetupAttachment(SceneRoot);
    FootprintBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FootprintBounds->SetGenerateOverlapEvents(false);
    FootprintBounds->SetCanEverAffectNavigation(false);
    FootprintBounds->ShapeColor = FColor::Green;
    FootprintBounds->SetLineThickness(2.0f);
    FootprintBounds->SetHiddenInGame(true);

    // 물리 충돌·선택 범위와 분리한 동적 Nav modifier다. 유효한 Spec과 등록이
    // 완료되기 전에 기본 Box가 NavMesh를 오염하지 않도록 처음에는 nav relevance를 끈다.
    NavigationObstacleBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("FacilityNavigationObstacleBounds"));
    NavigationObstacleBounds->SetupAttachment(SceneRoot);
    NavigationObstacleBounds->SetBoxExtent(FVector::ZeroVector);
    NavigationObstacleBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NavigationObstacleBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    NavigationObstacleBounds->SetGenerateOverlapEvents(false);
    NavigationObstacleBounds->bDynamicObstacle = true;
    NavigationObstacleBounds->SetAreaClassOverride(UNavArea_Null::StaticClass());
    NavigationObstacleBounds->SetCanEverAffectNavigation(false);
    NavigationObstacleBounds->SetVisibility(false, true);
    NavigationObstacleBounds->SetHiddenInGame(true);

    // 통행 NPC가 대기열을 관통하지 않게 레인을 '비싼 길'로 표시하는 Nav 전용 Box다.
    // NavigationObstacleBounds와 같은 이유로 표현용 QueueAreaBounds와 분리한다 — 하나로 겸하면
    // 디버그 표시를 끄는 순간 통행 제어까지 같이 꺼진다.
    QueueLaneNavCostBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("FacilityQueueLaneNavCostBounds"));
    QueueLaneNavCostBounds->SetupAttachment(SceneRoot);
    QueueLaneNavCostBounds->SetBoxExtent(FVector::ZeroVector);
    QueueLaneNavCostBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    QueueLaneNavCostBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    QueueLaneNavCostBounds->SetGenerateOverlapEvents(false);
    QueueLaneNavCostBounds->bDynamicObstacle = true;
    QueueLaneNavCostBounds->SetAreaClassOverride(UFFZSNavArea_QueueLane::StaticClass());
    QueueLaneNavCostBounds->SetCanEverAffectNavigation(false);
    QueueLaneNavCostBounds->SetVisibility(false, true);
    QueueLaneNavCostBounds->SetHiddenInGame(true);

    QueueAreaBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("QueueAreaBounds"));
    QueueAreaBounds->SetupAttachment(SceneRoot);
    QueueAreaBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    QueueAreaBounds->SetGenerateOverlapEvents(false);
    QueueAreaBounds->SetCanEverAffectNavigation(false);
    QueueAreaBounds->SetHiddenInGame(true);

    // 범위 사각형마다 UBoxComponent를 만드는 대신 모든 선을 한 proxy에 배치한다. 선 수명은
    // 명시적으로 Flush할 때까지 유지하므로 이 컴포넌트 자체의 매 프레임 Tick도 필요 없다.
    BuildModeRangeLines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("BuildModeRangeLines"));
    BuildModeRangeLines->SetupAttachment(SceneRoot);
    BuildModeRangeLines->SetAbsolute(true, true, true);
    BuildModeRangeLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BuildModeRangeLines->SetGenerateOverlapEvents(false);
    BuildModeRangeLines->SetCanEverAffectNavigation(false);
    BuildModeRangeLines->PrimaryComponentTick.bCanEverTick = false;
    BuildModeRangeLines->SetHiddenInGame(true);

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
    // Blueprint에 저장된 예전 응답/물리 설정도 등록 전에 현재 정책으로 복구한다.
    RefreshPlayerCollision();
    if (BaseFacilitySpec.FacilityDefinitionId.IsNone() && !FacilitySpec.FacilityDefinitionId.IsNone())
    {
        // 이전에 레벨에 배치·저장된 Actor는 원본을 기존 FacilitySpec에만 가지고 있다.
        BaseFacilitySpec = FacilitySpec;
    }
    RequestFacilityAudio();
    RequestFacilityLighting();
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
    SetPlayerCollisionEnabled(false);
    SetNavigationObstacleEnabled(false);
    DestroyBuildModeRangePresentation();
    DestroyFacilityLighting();
    ++AudioLoadGeneration;
    if (AudioLoadHandle.IsValid())
    {
        AudioLoadHandle->CancelHandle();
        AudioLoadHandle.Reset();
    }
    if (FacilityAudioComponent)
    {
        FacilityAudioComponent->OnAudioFinishedNative.RemoveAll(this);
    }
    StopFacilityAudio(false);
    ++VisualLoadGeneration;
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
bool AFFZSFacilityActor::InitializeFacility(const FGuid NewInstanceId, const FFFZSFacilitySpec& NewSpec, const FName NewZoneId, FFFZSGridPlacement NewPlacement, const FTransform& WorldTransform, const EFFZSFacilityState InitialState, const int32 InitialUpgradeLevel, const float InitialServiceDurationMultiplier, const float InitialSizeMultiplier, const float InitialRevenueMultiplier)
{
    TArray<FText> Errors;
    if (!NewInstanceId.IsValid() || NewZoneId.IsNone() || !NewSpec.IsValid(Errors) ||
        !IsValidUpgradeState(InitialUpgradeLevel, InitialServiceDurationMultiplier, InitialSizeMultiplier, InitialRevenueMultiplier))
    {
        return false;
    }

    FFFZSFacilitySpec EffectiveSpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
        NewSpec, InitialUpgradeLevel, InitialSizeMultiplier);
    if (!EffectiveSpec.IsValid(Errors))
    {
        return false;
    }

    SetPlayerCollisionEnabled(false);
    SetNavigationObstacleEnabled(false);
    FacilityInstanceId = NewInstanceId;
    BaseFacilitySpec = NewSpec;
    FacilitySpec = MoveTemp(EffectiveSpec);
    bHasLightingOverride = false;
    LightingState = NewSpec.LightingSettings.GetDefaultState();
    UpgradeLevel = InitialUpgradeLevel;
    UpgradeServiceDurationMultiplier = InitialServiceDurationMultiplier;
    UpgradeSizeMultiplier = InitialSizeMultiplier;
    UpgradeRevenueMultiplier = InitialRevenueMultiplier;
    ZoneId = NewZoneId;
    // [하드코딩 제거] 회전 정규화 식을 이 파일에 다시 적지 않고 격자 계약을 소유한 라이브러리를 쓴다.
    // 같은 식이 여러 곳에 복사돼 있으면 정규화 규칙이 바뀔 때 일부만 갱신될 수 있다.
    UFFZSGridLibrary::NormalizePlacement(NewPlacement);
    GridPlacement = NewPlacement;
    FacilityState = InitialState;
    bNeedsRelocation = false;
    SetActorTransform(WorldTransform);
    SetActorScale3D(FVector::OneVector);
    RebuildRuntimePresentation();
    QueueComponent->Configure(GetEffectiveQueueRules());
    return true;
}

const FFFZSFacilitySpec& AFFZSFacilityActor::GetBaseFacilitySpec() const
{
    // 읽기만 하는 에디터·등록 경로에서도 예전 배치 Actor를 지원한다.
    return BaseFacilitySpec.FacilityDefinitionId.IsNone() ? FacilitySpec : BaseFacilitySpec;
}

bool AFFZSFacilityActor::IsValidUpgradeState(const int32 Level, const float ServiceDurationMultiplier, const float SizeMultiplier, const float RevenueMultiplier)
{
    return Level >= 1 && FMath::IsFinite(ServiceDurationMultiplier) &&
        ServiceDurationMultiplier > 0.0f && FMath::IsFinite(SizeMultiplier) &&
        SizeMultiplier > 0.0f && FMath::IsFinite(RevenueMultiplier) && RevenueMultiplier >= 0.0f &&
        (Level > 1 || (ServiceDurationMultiplier == 1.0f && SizeMultiplier == 1.0f && RevenueMultiplier == 1.0f));
}

bool AFFZSFacilityActor::ApplyUpgradeState(const int32 NewLevel, const float NewServiceDurationMultiplier, const float NewSizeMultiplier, const float NewRevenueMultiplier)
{
    if (!IsValidUpgradeState(NewLevel, NewServiceDurationMultiplier, NewSizeMultiplier, NewRevenueMultiplier)
        || !QueueComponent || QueueComponent->IsOccupied()
        || (FacilityState != EFFZSFacilityState::Validated && FacilityState != EFFZSFacilityState::Closed
            && FacilityState != EFFZSFacilityState::MovePending))
    {
        return false;
    }
    FFFZSFacilitySpec EffectiveSpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
        GetBaseFacilitySpec(), NewLevel, NewSizeMultiplier);
    TArray<FText> Errors;
    if (!EffectiveSpec.IsValid(Errors))
    {
        return false;
    }

    const bool bNavigationWasEnabled = NavigationObstacleBounds && NavigationObstacleBounds->CanEverAffectNavigation();
    SetNavigationObstacleEnabled(false);
    if (BaseFacilitySpec.FacilityDefinitionId.IsNone())
    {
        BaseFacilitySpec = FacilitySpec;
    }
    UpgradeLevel = NewLevel;
    UpgradeServiceDurationMultiplier = NewServiceDurationMultiplier;
    UpgradeSizeMultiplier = NewSizeMultiplier;
    UpgradeRevenueMultiplier = NewRevenueMultiplier;
    FacilitySpec = MoveTemp(EffectiveSpec);
    SetActorScale3D(FVector::OneVector);
    RebuildRuntimePresentation();
    QueueComponent->Configure(GetEffectiveQueueRules());
    SetNavigationObstacleEnabled(bNavigationWasEnabled);
    return true;
}

FFFZSQueueRules AFFZSFacilityActor::BuildEffectiveQueueRules(
    const FFFZSQueueRules& BaseRules, const float ServiceDurationMultiplier)
{
    FFFZSQueueRules Result = BaseRules;
    // 배율에는 게임 규칙상 상한을 두지 않는다. 곱셈은 double로 계산하고 최종 float 시간만 포화한다.
    // 하한은 QueueComponent::Configure의 실행 정밀도와 동일하게 유지한다.
    const double Duration = static_cast<double>(BaseRules.ServiceDurationSeconds) * ServiceDurationMultiplier;
    Result.ServiceDurationSeconds = static_cast<float>(FMath::Clamp(Duration, 0.01, static_cast<double>(MAX_flt)));
    return Result;
}

FFFZSQueueRules AFFZSFacilityActor::GetEffectiveQueueRules() const
{
    return BuildEffectiveQueueRules(FacilitySpec.QueueRules, UpgradeServiceDurationMultiplier);
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
    UFFZSGridLibrary::NormalizePlacement(NewPlacement);
    GridPlacement = NewPlacement;
    bNeedsRelocation = false;
    SetActorTransform(WorldTransform);
    SetActorScale3D(FVector::OneVector);
    FacilityState = EFFZSFacilityState::Validated;
    bSetMovePendingWhenDrained = false;
    if (bBuildModeRangeVisible)
    {
        RebuildBuildModeRangePresentation();
    }
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
    UpdateFacilityAudioPlayback();
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
    UpdateFacilityAudioPlayback();
}

void AFFZSFacilityActor::MarkRemoved()
{
    if (QueueComponent->IsOccupied())
    {
        return;
    }
    QueueComponent->EmergencyCancel();
    SetPlayerCollisionEnabled(false);
    SetNavigationObstacleEnabled(false);
    FacilityState = EFFZSFacilityState::Removed;
    DestroyFacilityLighting();
    UpdateFacilityAudioPlayback();
    SetSelectionIndicatorState(EFFZSFacilitySelectionIndicatorState::Hidden);
    SetBuildModeActionSelected(false);
    SetBuildModeRangeVisible(false);
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

void AFFZSFacilityActor::SetBuildModeRangeVisible(const bool bVisible)
{
    const bool bShouldShow = bVisible && FacilityState != EFFZSFacilityState::Removed;
    if (bBuildModeRangeVisible == bShouldShow)
    {
        // 이동 명령은 표시 여부를 바꾸지 않지만 월드 좌표로 배치한 선 캐시는 다시 만들어야 한다.
        if (bShouldShow)
        {
            RebuildBuildModeRangePresentation();
        }
        return;
    }

    bBuildModeRangeVisible = bShouldShow;
    FootprintBounds->SetHiddenInGame(!bShouldShow);
    // QueueAreaBounds는 여러 비정형 Queue 사각형의 외접 박스이므로 제품 범위로 사용하지 않는다.
    QueueAreaBounds->SetHiddenInGame(true);

    if (bShouldShow)
    {
        RebuildBuildModeRangePresentation();
    }
    else
    {
        DestroyBuildModeRangePresentation();
    }
}

void AFFZSFacilityActor::SetBuildModeActionSelected(const bool bSelected)
{
    bBuildModeActionSelected = bSelected && FacilityState != EFFZSFacilityState::Removed;
    if (!FootprintBounds)
    {
        return;
    }

    FootprintBounds->ShapeColor = bBuildModeActionSelected ? FColor(64, 200, 255) : FColor::Green;
    FootprintBounds->SetLineThickness(bBuildModeActionSelected ? 5.0f : 2.0f);
    FootprintBounds->MarkRenderStateDirty();
}

void AFFZSFacilityActor::SetSelectionIndicatorState(
    const EFFZSFacilitySelectionIndicatorState NewState)
{
    const EFFZSFacilitySelectionIndicatorState ResolvedState =
        FacilityState == EFFZSFacilityState::Removed
            ? EFFZSFacilitySelectionIndicatorState::Hidden
            : NewState;
    if (!SelectionProxy || SelectionIndicatorState == ResolvedState)
    {
        return;
    }

    SelectionIndicatorState = ResolvedState;
    if (SelectionIndicatorState == EFFZSFacilitySelectionIndicatorState::Hidden)
    {
        SelectionProxy->SetHiddenInGame(true);
        return;
    }

    // 라이브 시설 정보 UI의 Accent(#14F2E0)를 사용한다. 실제 클릭 Box의 표시 속성만
    // 바꾸며 Extent·Transform·Collision 계약은 그대로 유지한다.
    SelectionProxy->ShapeColor = LiveSelectionIndicatorColor;
    SelectionProxy->SetLineThickness(
        SelectionIndicatorState == EFFZSFacilitySelectionIndicatorState::Selected
            ? SelectedSelectionIndicatorThickness
            : HoveredSelectionIndicatorThickness);
    SelectionProxy->SetHiddenInGame(false);
    SelectionProxy->MarkRenderStateDirty();
}

void AFFZSFacilityActor::SetBuildModeRemovalSelected(const bool bSelected)
{
    SetBuildModeActionSelected(bSelected);
}

bool AFFZSFacilityActor::IsWorldPointInsideFootprint(const FVector& WorldPoint) const
{
    if (!FootprintBounds || FacilityState == EFFZSFacilityState::Removed)
    {
        return false;
    }

    const FVector LocalPoint = FootprintBounds->GetComponentTransform().InverseTransformPosition(WorldPoint);
    const FVector Extent = FootprintBounds->GetUnscaledBoxExtent();
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    // Actor 높이는 footprint 지면 표본의 평균이다. 허용된 지면 높이 편차만큼은 선택에 포함하되,
    // Zone 필터와 함께 수직 거리를 제한해 같은 XY의 다른 층 시설을 잘못 고르지 않는다.
    constexpr double HorizontalSelectionToleranceCm = 2.0;
    const double VerticalSelectionToleranceCm = 2.0 +
        (Settings ? FMath::Max(0.0f, Settings->MaxGroundHeightVarianceCm) : 20.0f);
    return FMath::Abs(LocalPoint.X) <= Extent.X + HorizontalSelectionToleranceCm &&
        FMath::Abs(LocalPoint.Y) <= Extent.Y + HorizontalSelectionToleranceCm &&
        FMath::Abs(LocalPoint.Z) <= Extent.Z + VerticalSelectionToleranceCm;
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
    Snapshot.FacilitySpec = GetBaseFacilitySpec();
    Snapshot.bHasLightingOverride = bHasLightingOverride;
    Snapshot.LightingState = GetLightingState();
    Snapshot.UpgradeLevel = UpgradeLevel;
    Snapshot.UpgradeServiceDurationMultiplier = UpgradeServiceDurationMultiplier;
    Snapshot.UpgradeSizeMultiplier = UpgradeSizeMultiplier;
    Snapshot.UpgradeRevenueMultiplier = UpgradeRevenueMultiplier;
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
    Result.UpgradeLevel = UpgradeLevel;
    Result.UpgradeServiceDurationMultiplier = UpgradeServiceDurationMultiplier;
    Result.UpgradeSizeMultiplier = UpgradeSizeMultiplier;
    Result.UpgradeRevenueMultiplier = UpgradeRevenueMultiplier;
    Result.bUsesFacilityLevels = HasFacilityLevels();
    Result.EffectiveQueueRules = GetEffectiveQueueRules();
    Result.ZoneId = ZoneId;
    Result.WorldLocation = GetActorLocation();
    Result.bHasApproachWorldTransform = GetAnchorWorldTransform(
        EFFZSAnchorType::Entry,
        0,
        Result.ApproachWorldTransform);
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
    const FVector FootprintExtent = MakeFacilityFootprintExtent(FacilitySpec);
    FootprintBounds->SetBoxExtent(FootprintExtent);
    FootprintBounds->SetRelativeLocation(FVector(0.0f, 0.0f, FootprintExtent.Z));
    // Clearance·Queue 영역은 의도적 보행 공간이므로 확장 없이 고형 footprint만 복제한다.
    NavigationObstacleBounds->SetBoxExtent(FootprintExtent);
    NavigationObstacleBounds->SetRelativeLocation(FVector(0.0f, 0.0f, FootprintExtent.Z));

    // 선택 여백은 전역 Config 정책에서 한 번 읽어 시설별 Footprint 반치수에 더한다. 시설 데이터나
    // SaveGame에 파생 Box를 중복 저장하지 않으므로 새 설치와 Snapshot 복원이 같은 계산 경로를 쓴다.
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const float ConfiguredSelectionPaddingCm = Settings
        ? Settings->FacilitySelectionPaddingCm
        : UFFZSDeveloperSettings::DefaultFacilitySelectionPaddingCm;
    const float SelectionPaddingCm = FMath::IsFinite(ConfiguredSelectionPaddingCm)
        ? FMath::Clamp(
            ConfiguredSelectionPaddingCm,
            0.0f,
            UFFZSDeveloperSettings::MaxFacilitySelectionPaddingCm)
        : UFFZSDeveloperSettings::DefaultFacilitySelectionPaddingCm;
    const FVector SelectionExtent(
        FootprintExtent.X + SelectionPaddingCm,
        FootprintExtent.Y + SelectionPaddingCm,
        FootprintExtent.Z);
    RebuildSelectionProxy(SelectionExtent);

    RequestVisualMesh();
    RequestFacilityAudio();
    RequestFacilityLighting();

    QueueSpline->ClearSplinePoints(false);
    for (int32 Index = 0; Index < FacilitySpec.QueueSplinePointsCm.Num(); ++Index)
    {
        QueueSpline->AddSplinePoint(FacilitySpec.QueueSplinePointsCm[Index], ESplineCoordinateSpace::Local, false);
        QueueSpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    QueueSpline->UpdateSpline();

    FVector QueueLaneCenterCm = FVector::ZeroVector;
    FVector QueueLaneExtentCm = FVector::ZeroVector;
    if (TryMakeQueueLaneLocalBox(FacilitySpec, QueueLaneCenterCm, QueueLaneExtentCm))
    {
        QueueAreaBounds->SetRelativeLocation(QueueLaneCenterCm);
        QueueAreaBounds->SetBoxExtent(QueueLaneExtentCm);
    }
    else
    {
        // 재구성 전의 Queue Box가 남지 않게 명시적으로 비운다. 권위 점유에는 사용하지 않는다.
        QueueAreaBounds->SetRelativeLocation(FVector::ZeroVector);
        QueueAreaBounds->SetBoxExtent(FVector::ZeroVector);
    }
    ApplyQueueLaneNavCostGeometry();

    RebuildAnchors();

    if (bBuildModeRangeVisible)
    {
        RebuildBuildModeRangePresentation();
    }
}

void AFFZSFacilityActor::RebuildSelectionProxy(const FVector& SelectionExtent)
{
    if (!SelectionProxy)
    {
        return;
    }

    // Blueprint 파생 클래스의 컴포넌트 기본값과 무관하게 초기화 때 선택 계약을 다시 확정한다.
    SelectionProxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SelectionProxy->SetCollisionObjectType(ECC_WorldDynamic);
    SelectionProxy->SetCollisionResponseToAllChannels(ECR_Ignore);
    SelectionProxy->SetCollisionResponseToChannel(
        FFZSCollision::FacilitySelectionTraceChannel,
        ECR_Block);
    SelectionProxy->SetGenerateOverlapEvents(false);
    SelectionProxy->SetCanEverAffectNavigation(false);
    SelectionProxy->SetSimulatePhysics(false);
    SelectionProxy->SetEnableGravity(false);
    SelectionProxy->SetNotifyRigidBodyCollision(false);
    SelectionProxy->SetBoxExtent(SelectionExtent);
    SelectionProxy->SetRelativeLocation(FVector(0.0f, 0.0f, SelectionExtent.Z));
}

void AFFZSFacilityActor::SetPlayerCollisionEnabled(const bool bEnabled)
{
    bPlayerCollisionEnabled = bEnabled;
    RefreshPlayerCollision();
}

void AFFZSFacilityActor::RefreshPlayerCollision()
{
    if (!VisualMesh)
    {
        return;
    }

    // 공유 StaticMesh/BodySetup을 변경하거나 충돌 형상을 새로 만들지 않는다.
    // CharacterMovement의 기존 capsule sweep가 에셋에 저장된 collision을 사용한다.
    // 업데이트는 등록/메시 교체/철거 시에만 실행되며 Tick·Overlap/Hit 이벤트는 필요 없다.
    VisualMesh->SetCanEverAffectNavigation(false);
    VisualMesh->SetSimulatePhysics(false);
    VisualMesh->SetEnableGravity(false);
    VisualMesh->SetGenerateOverlapEvents(false);
    VisualMesh->SetNotifyRigidBodyCollision(false);
    VisualMesh->SetCollisionObjectType(FFZSCollision::FacilityPlayerObjectChannel);
    VisualMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    VisualMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

    const bool bShouldEnable = bPlayerCollisionEnabled && FacilityInstanceId.IsValid()
        && FacilityState != EFFZSFacilityState::Removed && !IsActorBeingDestroyed()
        && VisualMesh->GetStaticMesh() != nullptr;
    VisualMesh->SetCollisionEnabled(bShouldEnable
        ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AFFZSFacilityActor::SetNavigationObstacleEnabled(const bool bEnabled)
{
    if (!NavigationObstacleBounds)
    {
        return;
    }

    // 레벨에 이미 배치된 Actor는 현 세션에서 InitializeFacility를 거치지 않을 수 있다.
    // 따라서 등록 시점에 권위 Spec으로 기하를 먼저 복구하고 nav relevance를 마지막에 켠다.
    if (bEnabled)
    {
        const FVector ExpectedExtent = MakeFacilityFootprintExtent(FacilitySpec);
        const FVector ExpectedLocation(0.0f, 0.0f, ExpectedExtent.Z);
        if (!NavigationObstacleBounds->GetUnscaledBoxExtent().Equals(ExpectedExtent))
        {
            NavigationObstacleBounds->SetBoxExtent(ExpectedExtent);
        }
        if (!NavigationObstacleBounds->GetRelativeLocation().Equals(ExpectedLocation))
        {
            NavigationObstacleBounds->SetRelativeLocation(ExpectedLocation);
        }
    }

    const FVector Extent = NavigationObstacleBounds->GetUnscaledBoxExtent();
    const bool bHasValidFootprint = Extent.X > UE_KINDA_SMALL_NUMBER &&
        Extent.Y > UE_KINDA_SMALL_NUMBER && Extent.Z > UE_KINDA_SMALL_NUMBER;
    const bool bShouldEnable = bEnabled && FacilityInstanceId.IsValid() &&
        FacilityState != EFFZSFacilityState::Removed && bHasValidFootprint;
    if (NavigationObstacleBounds->CanEverAffectNavigation() != bShouldEnable)
    {
        NavigationObstacleBounds->SetCanEverAffectNavigation(bShouldEnable);
    }

    // Queue 레인 비용 Box도 같은 게이트에 묶는다. 함수를 나누지 않는 이유는 두 Box가 갈리면
    // "시설은 사라졌는데 레인만 비싼 채로 남는" 상태가 생기고, 그건 호출부가 아니라 여기서만
    // 막을 수 있기 때문이다. 호출 지점은 이미 시설 수명주기 전체를 덮고 있다.
    if (!QueueLaneNavCostBounds)
    {
        return;
    }

    // footprint와 같은 이유로 기하를 먼저 복구한다. 레벨에 미리 배치된 시설은
    // RebuildRuntimePresentation을 거치지 않고 등록될 수 있다.
    if (bEnabled)
    {
        ApplyQueueLaneNavCostGeometry();
    }

    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const FVector QueueExtent = QueueLaneNavCostBounds->GetUnscaledBoxExtent();
    // Queue 레인이 없는 시설(예약 사각형 0개)은 Extent가 0이라 여기서 자연히 걸러진다.
    const bool bHasQueueLane = QueueExtent.X > UE_KINDA_SMALL_NUMBER &&
        QueueExtent.Y > UE_KINDA_SMALL_NUMBER && QueueExtent.Z > UE_KINDA_SMALL_NUMBER;
    const bool bShouldEnableQueueLane = bShouldEnable && bHasQueueLane &&
        (!Settings || Settings->bEnableQueueLaneNavCost);
    if (QueueLaneNavCostBounds->CanEverAffectNavigation() != bShouldEnableQueueLane)
    {
        QueueLaneNavCostBounds->SetCanEverAffectNavigation(bShouldEnableQueueLane);
    }
}

// 비용 Box는 예약 레인과 같은 외접 박스를 쓰되 XY로만 여유를 준다. 경로추종은 웨이포인트를
// 일정 반경 앞에서 넘기며 모서리를 자르므로, 레인에 딱 맞추면 그 컷이 레인 안으로 들어온다.
// Z는 넓히지 않는다 — 위아래로 키우면 다른 층의 NavMesh까지 비싸진다.
void AFFZSFacilityActor::ApplyQueueLaneNavCostGeometry()
{
    if (!QueueLaneNavCostBounds)
    {
        return;
    }

    FVector CenterCm = FVector::ZeroVector;
    FVector ExtentCm = FVector::ZeroVector;
    if (!TryMakeQueueLaneLocalBox(FacilitySpec, CenterCm, ExtentCm))
    {
        QueueLaneNavCostBounds->SetRelativeLocation(FVector::ZeroVector);
        QueueLaneNavCostBounds->SetBoxExtent(FVector::ZeroVector);
        return;
    }

    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const float PaddingCm = Settings ? FMath::Max(0.0f, Settings->QueueLaneNavCostPaddingCm) : 0.0f;

    QueueLaneNavCostBounds->SetRelativeLocation(CenterCm);
    QueueLaneNavCostBounds->SetBoxExtent(ExtentCm + FVector(PaddingCm, PaddingCm, 0.0f));
}

void AFFZSFacilityActor::RebuildBuildModeRangePresentation()
{
    DestroyBuildModeRangePresentation();
    if (!bBuildModeRangeVisible || !BuildModeRangeLines || !FootprintBounds)
    {
        return;
    }

    constexpr float RangeHeightCm = 10.0f;
    // 비정상적으로 조각난 콘텐츠도 건축 모드 진입을 멈추지 않게 한다. 일반 콘텐츠는 정확한
    // canonical rect를 모두 그리고, 상한을 넘는 그룹만 합집합 외접 박스로 안전하게 축약한다.
    // [하드코딩 제거] 배치 Preview와 같은 상한을 공유해 두 화면의 표시 규칙이 갈라지지 않게 한다.
    constexpr int32 MaxDisplayedRectsPerGroup =
        UFFZSDeveloperSettings::MaxDisplayedRangeRectsPerGroup;
    const float CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const FVector FootprintExtent = FootprintBounds->GetUnscaledBoxExtent();
    const FTransform ActorTransform = GetActorTransform();
    BuildModeRangeLines->SetHiddenInGame(false);

    const auto DrawRect = [this, CellSizeCm, FootprintExtent, RangeHeightCm, ActorTransform](
        const FFFZSCellRect& Rect,
        const FLinearColor& Color)
    {
        const FVector LowerLeft(
            Rect.Min.X * CellSizeCm - FootprintExtent.X,
            Rect.Min.Y * CellSizeCm - FootprintExtent.Y,
            0.0f);
        const FVector UpperRight(
            Rect.Max.X * CellSizeCm - FootprintExtent.X,
            Rect.Max.Y * CellSizeCm - FootprintExtent.Y,
            RangeHeightCm);
        BuildModeRangeLines->DrawBox(
            ActorTransform.TransformPosition((LowerLeft + UpperRight) * 0.5),
            (UpperRight - LowerLeft) * 0.5,
            ActorTransform.GetRotation(),
            Color,
            0.0f,
            SDPG_World,
            2.0f);
    };

    const auto AddRangeGroup = [&DrawRect](
        const TArray<FFFZSCellRect>& Rects,
        const FColor Color)
    {
        if (Rects.Num() <= MaxDisplayedRectsPerGroup)
        {
            for (const FFFZSCellRect& Rect : Rects)
            {
                if (Rect.IsValid())
                {
                    DrawRect(Rect, FLinearColor(Color));
                }
            }
            return;
        }

        FFFZSCellRect Bounds;
        bool bHasBounds = false;
        for (const FFFZSCellRect& Rect : Rects)
        {
            if (!Rect.IsValid())
            {
                continue;
            }
            if (!bHasBounds)
            {
                Bounds = Rect;
                bHasBounds = true;
            }
            else
            {
                Bounds.Min.X = FMath::Min(Bounds.Min.X, Rect.Min.X);
                Bounds.Min.Y = FMath::Min(Bounds.Min.Y, Rect.Min.Y);
                Bounds.Max.X = FMath::Max(Bounds.Max.X, Rect.Max.X);
                Bounds.Max.Y = FMath::Max(Bounds.Max.Y, Rect.Max.Y);
            }
        }
        if (bHasBounds)
        {
            DrawRect(Bounds, FLinearColor(Color));
        }
    };

    const FFFZSGridPlacement LocalPlacement;
    AddRangeGroup(UFFZSGridLibrary::GetClearanceRects(FacilitySpec, LocalPlacement), FColor::Cyan);
    AddRangeGroup(UFFZSGridLibrary::GetQueueRects(FacilitySpec, LocalPlacement), FColor::Yellow);
}

void AFFZSFacilityActor::DestroyBuildModeRangePresentation()
{
    if (BuildModeRangeLines)
    {
        BuildModeRangeLines->Flush();
        BuildModeRangeLines->SetHiddenInGame(true);
    }
}

// 비동기 경합 방어: 새 요청 전에 기존 Handle을 취소하고 화면을 비운다. 취소는 자원 절약과 수명
// 관리 목적이며, 완료 순서가 뒤바뀌더라도 콜백은 요청 당시 포인터를 캡처하지 않고 현재 Spec의
// SoftObjectPtr을 다시 읽는다. 따라서 오래된 요청이 완료되어도 오래된 메시를 직접 설치하지 않는다.
void AFFZSFacilityActor::RequestVisualMesh()
{
    const uint32 LoadGeneration = ++VisualLoadGeneration;
    if (VisualLoadHandle.IsValid())
    {
        VisualLoadHandle->CancelHandle();
        VisualLoadHandle.Reset();
    }

    if (!VisualMesh)
    {
        return;
    }

    // 새 메시를 읽는 동안 현재 표현을 유지하되 크기는 새 권위 기하에 즉시 맞춘다.
    // 해당 단계의 빈 메시 선택은 BuildEffectiveSpec이 이미 Lv.1 메시로 결정했다.
    ApplyVisualMesh(VisualMesh->GetStaticMesh());
    RequestVisualMeshAsset(FacilitySpec.VisualMesh.ToSoftObjectPath(), LoadGeneration, true);
}

void AFFZSFacilityActor::RequestVisualMeshAsset(const FSoftObjectPath& AssetPath, const uint32 LoadGeneration, const bool bAllowBaseFallback)
{
    if (LoadGeneration != VisualLoadGeneration || !VisualMesh || IsActorBeingDestroyed()
        || FacilityState == EFFZSFacilityState::Removed)
    {
        return;
    }
    if (AssetPath.IsNull())
    {
        ApplyVisualMesh(nullptr);
        return;
    }
    if (UStaticMesh* LoadedMesh = Cast<UStaticMesh>(AssetPath.ResolveObject()))
    {
        ApplyVisualMesh(LoadedMesh);
        return;
    }
    VisualLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        AssetPath,
        FStreamableDelegate::CreateUObject(this, &AFFZSFacilityActor::ApplyLoadedVisualMesh,
            LoadGeneration, AssetPath, bAllowBaseFallback));
    if (!VisualLoadHandle.IsValid())
    {
        ApplyLoadedVisualMesh(LoadGeneration, AssetPath, bAllowBaseFallback);
    }
}

// 취소 직전에 예약된 이전 콜백도 세대 번호가 다르면 새 handle이나 표현을 만질 수 없다.
void AFFZSFacilityActor::ApplyLoadedVisualMesh(const uint32 LoadGeneration, FSoftObjectPath AssetPath, const bool bAllowBaseFallback)
{
    if (LoadGeneration != VisualLoadGeneration || !VisualMesh || IsActorBeingDestroyed()
        || FacilityState == EFFZSFacilityState::Removed)
    {
        return;
    }
    UStaticMesh* LoadedMesh = Cast<UStaticMesh>(AssetPath.ResolveObject());
    VisualLoadHandle.Reset();
    if (LoadedMesh)
    {
        ApplyVisualMesh(LoadedMesh);
        return;
    }
    const FSoftObjectPath BasePath = GetBaseFacilitySpec().VisualMesh.ToSoftObjectPath();
    if (bAllowBaseFallback && AssetPath != BasePath)
    {
        // 레벨 메시가 없거나 실패하면 바로 원본 메시를 사용한다. 이전 단계는 탐색하지 않는다.
        RequestVisualMeshAsset(BasePath, LoadGeneration, false);
        return;
    }
    ApplyVisualMesh(nullptr);
}

void AFFZSFacilityActor::ApplyVisualMesh(UStaticMesh* Mesh)
{
    if (!VisualMesh)
    {
        return;
    }
    if (VisualMesh->GetStaticMesh() != Mesh)
    {
        // 다른 단계에서 남은 override가 새 메시 고유 재질을 덮어쓰지 않게 한다.
        VisualMesh->EmptyOverrideMaterials();
        VisualMesh->SetStaticMesh(Mesh);
    }
    FTransform VisualTransform = FFZSVisualGeometry::ResolveVisualMeshTransform(FacilitySpec, Mesh);
    if (FacilitySpec.VisualSizingMode == EFFZSVisualSizingMode::SourceMeshSize)
    {
        // 맞춤 모드는 확장된 footprint에서 이미 크기를 계산한다. 원본 크기 모드만 한 번 곱한다.
        VisualTransform.SetScale3D(VisualTransform.GetScale3D() * UpgradeSizeMultiplier);
    }
    VisualMesh->SetRelativeTransform(VisualTransform);
    RefreshPlayerCollision();
    UpdateFacilityLighting();
}

FFFZSFacilityLightingState AFFZSFacilityActor::GetLightingState() const
{
    return bHasLightingOverride ? LightingState : GetBaseFacilitySpec().LightingSettings.GetDefaultState();
}

AFFZSFacilityLightingRig* AFFZSFacilityActor::GetLightingRig() const
{
    return FacilityLightingComponent ? Cast<AFFZSFacilityLightingRig>(FacilityLightingComponent->GetChildActor()) : nullptr;
}

void AFFZSFacilityActor::SetActorHiddenInGame(const bool bNewHidden)
{
    Super::SetActorHiddenInGame(bNewHidden);
    UpdateFacilityLighting();
}

void AFFZSFacilityActor::ApplyFacilityLightingOverride(const FFFZSFacilityLightingState& State, const bool bHasOverride)
{
    bHasLightingOverride = bHasOverride;
    LightingState = State;
    UpdateFacilityLighting();
}

void AFFZSFacilityActor::UpdateFacilityLighting()
{
    if (AFFZSFacilityLightingRig* Rig = GetLightingRig())
    {
        const bool bShow = !IsHidden() && FacilityState != EFFZSFacilityState::Removed && !IsActorBeingDestroyed();
        Rig->SetActorHiddenInGame(!bShow);
        Rig->ApplyLightingState(GetLightingState(), bShow && VisualMesh && VisualMesh->GetStaticMesh());
    }
}

void AFFZSFacilityActor::DestroyFacilityLighting()
{
    ++LightingLoadGeneration;
    if (LightingLoadHandle.IsValid())
    {
        LightingLoadHandle->CancelHandle();
        LightingLoadHandle.Reset();
    }
    if (FacilityLightingComponent)
    {
        FacilityLightingComponent->SetChildActorClass(nullptr);
    }
}

void AFFZSFacilityActor::RequestFacilityLighting()
{
    const uint32 Generation = ++LightingLoadGeneration;
    if (LightingLoadHandle.IsValid())
    {
        LightingLoadHandle->CancelHandle();
        LightingLoadHandle.Reset();
    }
    if (!FacilityLightingComponent || IsActorBeingDestroyed() || FacilityState == EFFZSFacilityState::Removed) return;
    if (!HasLightingRig())
    {
        FacilityLightingComponent->SetChildActorClass(nullptr);
        return;
    }
    UClass* Loaded = FacilitySpec.LightingSettings.LightingRigClass.Get();
    if (Loaded)
    {
        ApplyLoadedFacilityLighting(Generation);
        return;
    }
    // 이전 다른 클래스의 광원이 비동기 적재 중 남지 않도록 정리한다.
    FacilityLightingComponent->SetChildActorClass(nullptr);
    LightingLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        FacilitySpec.LightingSettings.LightingRigClass.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &AFFZSFacilityActor::ApplyLoadedFacilityLighting, Generation));
}

void AFFZSFacilityActor::ApplyLoadedFacilityLighting(const uint32 LoadGeneration)
{
    if (LoadGeneration != LightingLoadGeneration || !FacilityLightingComponent || IsActorBeingDestroyed()
        || FacilityState == EFFZSFacilityState::Removed) return;
    LightingLoadHandle.Reset();
    UClass* RigClass = FacilitySpec.LightingSettings.LightingRigClass.Get();
    if (!RigClass || !RigClass->IsChildOf(AFFZSFacilityLightingRig::StaticClass()) || RigClass->HasAnyClassFlags(CLASS_Abstract))
    {
        FacilityLightingComponent->SetChildActorClass(nullptr);
        return;
    }
    if (FacilityLightingComponent->GetChildActorClass() != RigClass)
    {
        FacilityLightingComponent->SetChildActorClass(RigClass);
    }
    FacilityLightingComponent->SetRelativeTransform(FTransform::Identity);
    UpdateFacilityLighting();
}

// 오디오 표현은 권위 배치·큐와 독립적이다. 현재 Spec의 soft path만 묶어 비동기 로드하고,
// 세대 번호로 취소 직전 요청의 늦은 콜백이 새 요청 상태를 덮어쓰지 못하게 한다.
void AFFZSFacilityActor::RequestFacilityAudio()
{
    const uint32 LoadGeneration = ++AudioLoadGeneration;
    if (AudioLoadHandle.IsValid())
    {
        AudioLoadHandle->CancelHandle();
        AudioLoadHandle.Reset();
    }

    StopFacilityAudio(false);
    if (!FacilityAudioComponent)
    {
        return;
    }

    FacilityAudioComponent->SetSound(nullptr);
    FacilityAudioComponent->SetAttenuationSettings(nullptr);
    FacilityAudioComponent->SetOverrideAttenuation(false);
    FacilityAudioComponent->ConcurrencySet.Reset();
    FacilityAudioComponent->SetRelativeLocation(FacilitySpec.AudioSettings.LocalOffsetCm);
    FacilityAudioComponent->SetVolumeMultiplier(FacilitySpec.AudioSettings.VolumeMultiplier);
    FacilityAudioComponent->SetPitchMultiplier(FacilitySpec.AudioSettings.PitchMultiplier);

    if (FacilitySpec.AudioSettings.AmbientSound.IsNull())
    {
        return;
    }

    TArray<FSoftObjectPath> AudioPaths;
    AudioPaths.Reserve(3);
    AudioPaths.Add(FacilitySpec.AudioSettings.AmbientSound.ToSoftObjectPath());
    if (!FacilitySpec.AudioSettings.Attenuation.IsNull())
    {
        AudioPaths.Add(FacilitySpec.AudioSettings.Attenuation.ToSoftObjectPath());
    }
    if (!FacilitySpec.AudioSettings.Concurrency.IsNull())
    {
        AudioPaths.Add(FacilitySpec.AudioSettings.Concurrency.ToSoftObjectPath());
    }

    AudioLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        AudioPaths,
        FStreamableDelegate::CreateUObject(
            this,
            &AFFZSFacilityActor::ApplyLoadedFacilityAudio,
            LoadGeneration));
}

void AFFZSFacilityActor::ApplyLoadedFacilityAudio(const uint32 LoadGeneration)
{
    if (LoadGeneration != AudioLoadGeneration || !FacilityAudioComponent)
    {
        return;
    }
    AudioLoadHandle.Reset();

    USoundBase* Sound = FacilitySpec.AudioSettings.AmbientSound.Get();
    if (!Sound)
    {
        FacilityAudioComponent->SetSound(nullptr);
        return;
    }

    USoundAttenuation* LoadedAttenuation =
        FacilitySpec.AudioSettings.Attenuation.Get();
    FacilityAudioComponent->SetAttenuationSettings(LoadedAttenuation);
    if (FacilitySpec.AudioSettings.bOverrideAudibleRange)
    {
        // Attenuation 에셋의 공간화·리버브·오클루전 정책은 보존하되 시설별 가청 거리 계약이
        // 항상 우선하도록 구형 선형 거리 설정만 교체한다. Max 밖에서는 Linear 결과가 0이다.
        FSoundAttenuationSettings RangeSettings = LoadedAttenuation
            ? LoadedAttenuation->Attenuation
            : FSoundAttenuationSettings{};
        RangeSettings.bAttenuate = true;
        RangeSettings.bSpatialize = true;
        RangeSettings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
        RangeSettings.AttenuationShape = EAttenuationShape::Sphere;
        RangeSettings.AttenuationShapeExtents = FVector(
            FacilitySpec.AudioSettings.FullVolumeRadiusCm,
            0.0f,
            0.0f);
        RangeSettings.FalloffDistance =
            FacilitySpec.AudioSettings.MaxAudibleDistanceCm -
            FacilitySpec.AudioSettings.FullVolumeRadiusCm;
        FacilityAudioComponent->AdjustAttenuation(RangeSettings);
    }
    else
    {
        FacilityAudioComponent->SetOverrideAttenuation(false);
    }
    FacilityAudioComponent->ConcurrencySet.Reset();
    if (USoundConcurrency* Concurrency = FacilitySpec.AudioSettings.Concurrency.Get())
    {
        FacilityAudioComponent->ConcurrencySet.Add(Concurrency);
    }
    FacilityAudioComponent->SetSound(Sound);
    FacilityAudioComponent->OnAudioFinishedNative.RemoveAll(this);
    FacilityAudioComponent->OnAudioFinishedNative.AddUObject(
        this,
        &AFFZSFacilityActor::HandleFacilityAudioFinished);
    UpdateFacilityAudioPlayback(true);
}

bool AFFZSFacilityActor::IsFacilityAudioPlaybackAllowed() const
{
    return FacilityState != EFFZSFacilityState::Removed &&
        (!FacilitySpec.AudioSettings.bPlayOnlyWhileActive ||
            FacilityState == EFFZSFacilityState::Active);
}

void AFFZSFacilityActor::UpdateFacilityAudioPlayback(const bool bAllowFade)
{
    if (!FacilityAudioComponent || !FacilityAudioComponent->GetSound() ||
        FacilitySpec.AudioSettings.VolumeMultiplier <= 0.0f ||
        !IsFacilityAudioPlaybackAllowed())
    {
        StopFacilityAudio(bAllowFade);
        return;
    }

    if (bAudioStopRequested)
    {
        FacilityAudioComponent->Stop();
    }
    if (!FacilityAudioComponent->IsPlaying() || bAudioStopRequested)
    {
        bAudioStopRequested = false;
        if (bAllowFade && FacilitySpec.AudioSettings.FadeInDurationSeconds > 0.0f)
        {
            FacilityAudioComponent->FadeIn(
                FacilitySpec.AudioSettings.FadeInDurationSeconds,
                1.0f);
        }
        else
        {
            FacilityAudioComponent->Play();
        }
    }
}

void AFFZSFacilityActor::StopFacilityAudio(const bool bAllowFade)
{
    if (!FacilityAudioComponent)
    {
        return;
    }

    bAudioStopRequested = true;
    if (!FacilityAudioComponent->IsPlaying())
    {
        return;
    }
    if (bAllowFade && FacilitySpec.AudioSettings.FadeOutDurationSeconds > 0.0f)
    {
        FacilityAudioComponent->FadeOut(
            FacilitySpec.AudioSettings.FadeOutDurationSeconds,
            0.0f);
    }
    else
    {
        FacilityAudioComponent->Stop();
    }
}

void AFFZSFacilityActor::HandleFacilityAudioFinished(UAudioComponent* FinishedComponent)
{
    if (FinishedComponent != FacilityAudioComponent || bAudioStopRequested ||
        !FacilitySpec.AudioSettings.bLoop || !IsFacilityAudioPlaybackAllowed() ||
        FacilitySpec.AudioSettings.VolumeMultiplier <= 0.0f)
    {
        return;
    }

    FacilityAudioComponent->Play();
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
