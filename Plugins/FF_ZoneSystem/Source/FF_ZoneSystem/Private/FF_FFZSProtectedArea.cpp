// FF_FFZSProtectedArea.cpp

///보호구역의 회전된 경계와 시설 OBB 겹침 판정을 구현한다.
// 흐름: Actor Transform과 Extent를 평면 OBB로 만들고 SAT 축 투영으로 교차를 검사한다.
// 불변식: 판정은 보호구역 의미를 저장할 뿐 실제 법률·현장 안전 인증을 뜻하지 않는다.
// 실패/성능: 네 개 축의 상수 크기 검사라 개별 판정 비용은 일정하다.
 

#include "FF_FFZSProtectedArea.h"
#include "Engine/World.h"

#include "Components/BoxComponent.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"

#define LOCTEXT_NAMESPACE "FFZSProtectedArea"
// 이 Actor는 건설 금지 구역, 일반/비상/접근성 동선, 관람 구역의 공간 규칙을 데이터로 제공.
// 충돌·Overlap 이벤트를 발생시키는 물리 Actor가 아니라, UFFZSWorldSubsystem이 배치 검증 때 직접
// 질의하는 정책 경계다. ProtectedAreaId는 월드 안에서 유일해야 하며 LinkedZoneId는 존재하는 존만
// 가리켜야 한다. 정확한 기하 판정을 위해 Actor Scale은 1, 회전은 Yaw만 허용.
 

AFFZSProtectedArea::AFFZSProtectedArea()
{
    PrimaryActorTick.bCanEverTick = false;
    AreaBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBounds"));
    SetRootComponent(AreaBounds);
    AreaBounds->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
    AreaBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AreaBounds->SetGenerateOverlapEvents(false);
}

// 권위 목록은 Subsystem이 보유한다. BeginPlay/EndPlay는 Actor 수명과 약한 참조 레지스트리를 동기화하고,
// 등록 시 데이터 유효성 및 ProtectedAreaId 중복을 다시 검사한다.
void AFFZSProtectedArea::BeginPlay()
{
    Super::BeginPlay();
    if (UFFZSWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
    {
        Subsystem->RegisterProtectedArea(this);
    }
}

void AFFZSProtectedArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>())
        {
            Subsystem->UnregisterProtectedArea(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

// 양의 유한 Actor 값이 있으면 현장별 Override, 0이면 AreaType에 대응하는 프로젝트 기본값을 사용한다.
// NoBuild는 추가 폭이 필요하지 않으므로 기본 0을 반환할 수 있지만, 동선/관람 유형은 검증 시 양수여야 한다.
float AFFZSProtectedArea::GetEffectiveRequiredClearWidthCm() const
{
    if (FMath::IsFinite(RequiredClearWidthCm) && RequiredClearWidthCm > 0.0f)
    {
        return RequiredClearWidthCm;
    }

    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    switch (AreaType)
    {
    case EFFZSProtectedAreaType::GeneralRoute:
        return Settings->GeneralRouteWidthCm;
    case EFFZSProtectedAreaType::EmergencyServiceRoute:
        return Settings->EmergencyServiceRouteWidthCm;
    case EFFZSProtectedAreaType::AccessibilityRoute:
    case EFFZSProtectedAreaType::ViewingArea:
        return Settings->AccessibilityRouteWidthCm;
    default:
        return 0.0f;
    }
}

// 현재 프로젝트 기본값을 Actor의 명시적 값으로 복사한다. 호출 이후에는 프로젝트 기본값이 바뀌어도
// 이 Actor의 폭이 자동으로 따라가지 않는다. 계속 상속하려면 RequiredClearWidthCm을 0으로 유지.
void AFFZSProtectedArea::ApplyDefaultWidthForType()
{
    RequiredClearWidthCm = 0.0f;
    RequiredClearWidthCm = GetEffectiveRequiredClearWidthCm();
}

//  보호 구역은 Yaw가 적용된 OBB(Oriented Bounding Box), 시설 셀은 월드축 AABB로 비교.
 // 1) 유효하지 않은 입력을 거르고, 요구 통과 폭이 로컬 X/Y 중 짧은 축보다 크면 그 축만 확장.
 // 2) Z 구간이 분리되면 즉시 종료.
 // 3) XY 평면에서 월드 X/Y와 보호 OBB의 로컬 X/Y, 총 네 축으로 SAT를 수행.
 // 어느 축에서도 분리되지 않으면 교차이며, 경계가 맞닿은 경우도 안전하게 교차로 취급.
  
 // GetScaledBoxExtent와 Forward/Right 축의 조합은 Scale=1, Pitch/Roll=0 불변식에 기대고 있다.
 // 그래서 ValidateProtectedAreaData는 Actor 스케일이나 기울기를 허용하지 않고, 크기는 Box Extent로만
 // 편집하게 한다. 이 함수는 월드 AABB 축으로 단순 팽창하는 검사가 아니므로 회전된 보호 동선도 보존.
 
bool AFFZSProtectedArea::IntersectsWorldBox(const FBox& WorldBox) const
{
    return WorldBox.IsValid && IntersectsWorldOrientedBox(
        WorldBox.GetCenter(), WorldBox.GetExtent(), FQuat::Identity);
}

bool AFFZSProtectedArea::IntersectsWorldOrientedBox(
    const FVector& WorldCenter,
    const FVector& WorldHalfExtent,
    const FQuat& WorldRotation) const
{
    if (!AreaBounds || WorldCenter.ContainsNaN() || WorldHalfExtent.ContainsNaN() ||
        WorldRotation.ContainsNaN() || WorldHalfExtent.GetMin() < 0.0)
    {
        return false;
    }

    FVector LocalExtent = AreaBounds->GetScaledBoxExtent();
    const double RequiredHalfWidth = FMath::Max(0.0, static_cast<double>(GetEffectiveRequiredClearWidthCm()) * 0.5);
    // RequiredClearWidth는 World AABB 축이 아니라 보호 Actor의 Local 단축 방향을 확장.
    if (LocalExtent.X <= LocalExtent.Y)
    {
        LocalExtent.X = FMath::Max(LocalExtent.X, RequiredHalfWidth);
    }
    else
    {
        LocalExtent.Y = FMath::Max(LocalExtent.Y, RequiredHalfWidth);
    }

    const FVector ProtectedCenter = AreaBounds->GetComponentLocation();
    if (FMath::Abs(ProtectedCenter.Z - WorldCenter.Z) >
        LocalExtent.Z + WorldHalfExtent.Z)
    {
        return false;
    }

    const FVector2D CenterDelta(
        WorldCenter.X - ProtectedCenter.X,
        WorldCenter.Y - ProtectedCenter.Y);
    const FVector Forward3D = AreaBounds->GetForwardVector();
    const FVector Right3D = AreaBounds->GetRightVector();
    const FVector2D LocalX = FVector2D(Forward3D.X, Forward3D.Y).GetSafeNormal();
    const FVector2D LocalY = FVector2D(Right3D.X, Right3D.Y).GetSafeNormal();
    const FVector FacilityForward3D = WorldRotation.GetForwardVector();
    const FVector FacilityRight3D = WorldRotation.GetRightVector();
    const FVector2D FacilityX =
        FVector2D(FacilityForward3D.X, FacilityForward3D.Y).GetSafeNormal();
    const FVector2D FacilityY =
        FVector2D(FacilityRight3D.X, FacilityRight3D.Y).GetSafeNormal();
    const FVector2D Axes[] = {
        FacilityX,
        FacilityY,
        LocalX,
        LocalY
    };

    for (const FVector2D& Axis : Axes)
    {
        const double CenterDistance = FMath::Abs(FVector2D::DotProduct(CenterDelta, Axis));
        const double ProtectedRadius =
            LocalExtent.X * FMath::Abs(FVector2D::DotProduct(LocalX, Axis)) +
            LocalExtent.Y * FMath::Abs(FVector2D::DotProduct(LocalY, Axis));
        const double BoxRadius =
            WorldHalfExtent.X * FMath::Abs(FVector2D::DotProduct(FacilityX, Axis)) +
            WorldHalfExtent.Y * FMath::Abs(FVector2D::DotProduct(FacilityY, Axis));
        if (CenterDistance > ProtectedRadius + BoxRadius)
        {
            return false;
        }
    }
    return true;
}

void AFFZSProtectedArea::ApplyAccessibleFireworksViewingPreset()
{
    ProtectedAreaId = TEXT("PA_AccessibleFireworksViewing");
    LinkedZoneId = TEXT("GZ_Operations");
    AreaType = EFFZSProtectedAreaType::ViewingArea;
    RequiredClearWidthCm = 0.0f;
}

// 저장/등록/배치 검증이 공유하는 데이터 계약이다. 오류를 모두 수집하므로 에디터 담당자는 첫 오류만
// 고치는 반복 대신 ID, 경계, Transform, 폭, 프로젝트 전용 소유 규칙을 한 번에 확인할 수 있다.
bool AFFZSProtectedArea::ValidateProtectedAreaData(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();
    if (ProtectedAreaId.IsNone())
    {
        OutErrors.Add(LOCTEXT("MissingProtectedAreaId", "ProtectedAreaId가 필요합니다."));
    }
    if (!AreaBounds || AreaBounds->GetUnscaledBoxExtent().X <= 0.0f || AreaBounds->GetUnscaledBoxExtent().Y <= 0.0f)
    {
        OutErrors.Add(LOCTEXT("InvalidProtectedBounds", "Protected Area의 Bounds는 수평 Extent가 양수여야 합니다."));
    }
    if (!GetActorScale3D().Equals(FVector::OneVector, 0.001f))
    {
        OutErrors.Add(LOCTEXT("ScaledProtectedArea", "Protected Area Actor의 Scale은 1이어야 합니다. 대신 Box Extent를 편집하십시오."));
    }
    const FRotator BoundsRotation = AreaBounds ? AreaBounds->GetComponentRotation() : GetActorRotation();
    if (!FMath::IsNearlyZero(BoundsRotation.Pitch, 0.01) || !FMath::IsNearlyZero(BoundsRotation.Roll, 0.01))
    {
        OutErrors.Add(LOCTEXT("TiltedProtectedArea", "Protected Area는 Yaw 회전만 지원합니다. Pitch와 Roll은 0이어야 합니다."));
    }
    if (!FMath::IsFinite(RequiredClearWidthCm) || RequiredClearWidthCm < 0.0f)
    {
        OutErrors.Add(LOCTEXT("InvalidProtectedWidth", "RequiredClearWidthCm은 유한한 0 이상의 값이어야 합니다."));
    }
    if (AreaType != EFFZSProtectedAreaType::NoBuild && GetEffectiveRequiredClearWidthCm() <= 0.0f)
    {
        OutErrors.Add(LOCTEXT(
            "MissingProtectedWidth",
            "Route 및 Viewing Protected Area에는 양수인 Actor Override 또는 Project Default Width가 필요합니다."));
    }
    if (ProtectedAreaId == TEXT("PA_AccessibleFireworksViewing") &&
        (LinkedZoneId != TEXT("GZ_Operations") || AreaType != EFFZSProtectedAreaType::ViewingArea))
    {
        OutErrors.Add(LOCTEXT("AccessibleViewingOwnership", "PA_AccessibleFireworksViewing은 GZ_Operations에 연결된 ViewingArea여야 합니다."));
    }
    return OutErrors.IsEmpty();
}

#undef LOCTEXT_NAMESPACE
