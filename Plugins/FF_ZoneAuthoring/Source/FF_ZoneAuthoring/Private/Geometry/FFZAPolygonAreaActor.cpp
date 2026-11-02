// FFZAPolygonAreaActor.cpp
// 역할: 다각형 영역 액터의 경계 계산, 자동 정리, 검증, 서브시스템 등록.
#include "FFZAPolygonAreaActor.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FFZAPolygonMath.h"
#include "FFZASplineAuthoring.h"
#include "FFZAZoneAuthoringSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "ScopedTransaction.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZAPolygonAreaActor)

#define LOCTEXT_NAMESPACE "FFZAPolygonAreaActor"

namespace
{
    const FLinearColor InvalidAreaColor(1.00f, 0.08f, 0.05f, 1.0f);

    // 구획/구역은 FF_ZoneSystem 격자 계약을 따를 필요는 없지만, 예측 가능한 편집을 위해
    // Scale만 점 좌표에 굽고 회전은 자유롭게 둔다(점 위치는 월드 기준으로 보존).
    FFZASplineAuthoring::FNormalizeOptions MakeAreaNormalizeOptions()
    {
        FFZASplineAuthoring::FNormalizeOptions Options;
        Options.bResetScale = true;
        Options.bClearPitchRoll = true;
        Options.bSnapYawTo90 = false;
        Options.bSnapOriginToWholeCm = false;
        return Options;
    }
}

AFFZAPolygonAreaActor::AFFZAPolygonAreaActor()
{
    PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
    // World Partition 맵에서도 항상 로드되어야 규칙이 빠지지 않는다.
    bIsSpatiallyLoaded = false;
#endif

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BoundarySpline"));
    BoundarySpline->SetupAttachment(SceneRoot);
    // 기본 20 m × 20 m 사각형. 레벨에 놓은 뒤 점을 옮겨 실제 모양으로 바꾼다.
    FFZASplineAuthoring::SetLocalRectangle(*BoundarySpline, FVector2D(2000.0, 2000.0));
}

TArray<FVector2D> AFFZAPolygonAreaActor::GetWorldPolygon() const
{
    TArray<FVector2D> Polygon;
    if (BoundarySpline)
    {
        FFZASplineAuthoring::GetWorldPolygon(*BoundarySpline, Polygon);
    }
    return Polygon;
}

bool AFFZAPolygonAreaActor::ContainsWorldPoint(const FVector& WorldPoint) const
{
    return FFZAPolygonMath::IsPointInside(GetWorldPolygon(), FVector2D(WorldPoint.X, WorldPoint.Y));
}

float AFFZAPolygonAreaActor::GetAreaSquareMeters() const
{
    return static_cast<float>(FFZAPolygonMath::Area(GetWorldPolygon()) / 10000.0);
}

FText AFFZAPolygonAreaActor::GetResolvedDisplayName() const
{
    return DisplayName.IsEmpty() ? FText::FromName(AreaId) : DisplayName;
}

bool AFFZAPolygonAreaActor::ValidateAreaData(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();
    if (AreaId.IsNone())
    {
        OutErrors.Add(LOCTEXT("MissingId", "Area Id가 비어 있습니다."));
    }
    if (!BoundarySpline || BoundarySpline->GetNumberOfSplinePoints() < 3)
    {
        OutErrors.Add(LOCTEXT("TooFewPoints", "경계 점이 3개 이상 필요합니다."));
    }
    else if (!FFZAPolygonMath::IsSimplePolygon(GetWorldPolygon()))
    {
        OutErrors.Add(LOCTEXT("NotSimple", "경계가 스스로 교차하거나 겹친 점이 있습니다. 선이 꼬이지 않게 점을 옮기세요."));
    }
    return OutErrors.IsEmpty();
}

void AFFZAPolygonAreaActor::RefreshAuthoringStatus()
{
    UpdateEditorPresentation();
}

void AFFZAPolygonAreaActor::UpdateEditorPresentation()
{
    ValidateAreaData(AuthoringIssues);
    if (BoundarySpline)
    {
        FFZASplineAuthoring::SetEditorColor(*BoundarySpline, AuthoringIssues.IsEmpty() ? AreaColor : InvalidAreaColor);
    }
}

UFFZAZoneAuthoringSubsystem* AFFZAPolygonAreaActor::GetAuthoringSubsystem() const
{
    const UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UFFZAZoneAuthoringSubsystem>() : nullptr;
}

void AFFZAPolygonAreaActor::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
    if (IsTemplate())
    {
        return;
    }
    if (UFFZAZoneAuthoringSubsystem* Subsystem = GetAuthoringSubsystem())
    {
        RegisterWithSubsystem(*Subsystem);
    }
}

void AFFZAPolygonAreaActor::PostUnregisterAllComponents()
{
    // GC로 파괴되는 중에는 이벤트를 보내지 않는다(남은 약한 참조는 서브시스템이 스스로 정리).
    if (!IsTemplate() && !HasAnyFlags(RF_BeginDestroyed) && !IsUnreachable())
    {
        if (UFFZAZoneAuthoringSubsystem* Subsystem = GetAuthoringSubsystem())
        {
            UnregisterFromSubsystem(*Subsystem);
        }
    }
    Super::PostUnregisterAllComponents();
}

void AFFZAPolygonAreaActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    const UWorld* World = GetWorld();
    if (!World || World->IsGameWorld() || IsTemplate())
    {
        return;
    }
    if (BoundarySpline)
    {
        FFZASplineAuthoring::EnsureClosedLinear(*BoundarySpline);
    }
    UpdateEditorPresentation();
}

#if WITH_EDITOR
void AFFZAPolygonAreaActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    if (GIsTransacting)
    {
        // Undo/Redo 중에는 값을 바꾸지 않고 표시만 갱신한다(되돌린 상태를 그대로 존중).
        UpdateEditorPresentation();
        return;
    }
    if (bIsNormalizing)
    {
        return;
    }
    TGuardValue<bool> Guard(bIsNormalizing, true);
    if (BoundarySpline)
    {
        FFZASplineAuthoring::EnsureClosedLinear(*BoundarySpline);
        if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
        {
            FFZASplineAuthoring::NormalizeActorTransformPreservingSpline(*this, *BoundarySpline, MakeAreaNormalizeOptions());
        }
    }
    UpdateEditorPresentation();
}

void AFFZAPolygonAreaActor::PostEditMove(const bool bFinished)
{
    Super::PostEditMove(bFinished);
    if (!bFinished || bIsNormalizing || GIsTransacting)
    {
        return;
    }
    TGuardValue<bool> Guard(bIsNormalizing, true);
    if (BoundarySpline)
    {
        const FScopedTransaction Transaction(LOCTEXT("AutoNormalizeArea", "영역 Transform 자동 정리"));
        FFZASplineAuthoring::NormalizeActorTransformPreservingSpline(*this, *BoundarySpline, MakeAreaNormalizeOptions());
    }
    UpdateEditorPresentation();
}

EDataValidationResult AFFZAPolygonAreaActor::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TArray<FText> Errors;
    if (!ValidateAreaData(Errors))
    {
        for (const FText& Error : Errors)
        {
            Context.AddError(FText::Format(LOCTEXT("AreaIssue", "{0}: {1}"), FText::FromString(GetActorNameOrLabel()), Error));
        }
        return EDataValidationResult::Invalid;
    }
    return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
