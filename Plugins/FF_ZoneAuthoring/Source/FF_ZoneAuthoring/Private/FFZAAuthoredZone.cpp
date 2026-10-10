// FFZAAuthoredZone.cpp
// 역할: 정의 에셋 → Zone 값 복사, 편집 후 Transform 자동 정리, 실시간 검증 표시.
#include "FFZAAuthoredZone.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_ZoneAuthoringModule.h"
#include "FFZASplineAuthoring.h"
#include "FFZAZoneAuthoringSubsystem.h"
#include "FFZAZoneDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "ScopedTransaction.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZAAuthoredZone)

#define LOCTEXT_NAMESPACE "FFZAAuthoredZone"

namespace
{
    const FLinearColor DefaultZoneColor(0.10f, 0.60f, 1.00f, 1.0f);
    const FLinearColor InvalidZoneColor(1.00f, 0.08f, 0.05f, 1.0f);

    // FF_ZoneSystem Zone 규칙(Scale 1, Pitch/Roll 0, Yaw 90도 단위, 정수 cm 원점)과 같은 정리 옵션.
    FFZASplineAuthoring::FNormalizeOptions MakeZoneNormalizeOptions()
    {
        FFZASplineAuthoring::FNormalizeOptions Options;
        Options.bResetScale = true;
        Options.bClearPitchRoll = true;
        Options.bSnapYawTo90 = true;
        Options.bSnapOriginToWholeCm = true;
        return Options;
    }

    bool AreCategoryListsEqual(const TArray<EFFZSFacilityCategory>& A, const TArray<EFFZSFacilityCategory>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }
        for (const EFFZSFacilityCategory Category : A)
        {
            if (!B.Contains(Category))
            {
                return false;
            }
        }
        return true;
    }
}

AFFZAAuthoredZone::AFFZAAuthoredZone()
{
    if (BoundarySpline)
    {
        FFZASplineAuthoring::SetEditorColor(*BoundarySpline, DefaultZoneColor);
    }
}

FLinearColor AFFZAAuthoredZone::GetZoneColor() const
{
    return ZoneDefinition ? ZoneDefinition->ZoneColor : DefaultZoneColor;
}

TArray<FVector2D> AFFZAAuthoredZone::GetBoundaryPolygonWorldCm() const
{
    TArray<FVector2D> Polygon;
    if (BoundarySpline)
    {
        FFZASplineAuthoring::GetWorldPolygon(*BoundarySpline, Polygon);
    }
    return Polygon;
}

bool AFFZAAuthoredZone::IsOutOfSyncWithDefinition() const
{
    if (!ZoneDefinition)
    {
        return false;
    }
    return ZoneDefinition->ZoneId != ZoneId ||
        ZoneDefinition->bAllowAllFacilityCategories != bAllowAllFacilityCategories ||
        (!bAllowAllFacilityCategories && !AreCategoryListsEqual(ZoneDefinition->AllowedFacilityCategories, AllowedFacilityCategories));
}

bool AFFZAAuthoredZone::CopyDefinitionToZone()
{
    if (!ZoneDefinition)
    {
        return false;
    }
    bool bChanged = false;
    if (ZoneId != ZoneDefinition->ZoneId)
    {
        ZoneId = ZoneDefinition->ZoneId;
        bChanged = true;
    }
    const FText ResolvedName = ZoneDefinition->GetResolvedDisplayName();
    if (!DisplayName.EqualTo(ResolvedName))
    {
        DisplayName = ResolvedName;
        bChanged = true;
    }
    if (bAllowAllFacilityCategories != ZoneDefinition->bAllowAllFacilityCategories)
    {
        bAllowAllFacilityCategories = ZoneDefinition->bAllowAllFacilityCategories;
        bChanged = true;
    }
    if (!AreCategoryListsEqual(AllowedFacilityCategories, ZoneDefinition->AllowedFacilityCategories))
    {
        AllowedFacilityCategories = ZoneDefinition->AllowedFacilityCategories;
        bChanged = true;
    }
    return bChanged;
}

void AFFZAAuthoredZone::ApplyDefinition()
{
    if (!ZoneDefinition)
    {
        return;
    }
    // 게임 중 이미 등록된 Zone의 ID를 바꾸면 FF_ZoneSystem 레지스트리 키가 어긋난다. 그 경우는 거부한다.
    if (const UWorld* World = GetWorld(); World && World->IsGameWorld())
    {
        const UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
        if (Subsystem && !ZoneId.IsNone() && Subsystem->FindZone(ZoneId) == this && ZoneId != ZoneDefinition->ZoneId)
        {
            UE_LOG(LogFFZoneAuthoring, Warning,
                TEXT("%s: 이미 '%s'로 등록된 Zone이라 정의 ID '%s'를 적용하지 않습니다. 에디터에서 Apply Definition 후 레벨을 저장하세요."),
                *GetName(), *ZoneId.ToString(), *ZoneDefinition->ZoneId.ToString());
            return;
        }
    }
#if WITH_EDITOR
    const FScopedTransaction Transaction(LOCTEXT("ApplyDefinition", "Zone 정의 적용"));
#endif
    Modify();
    CopyDefinitionToZone();
    UpdateEditorPresentation();
}

void AFFZAAuthoredZone::NormalizeTransformNow()
{
    if (!BoundarySpline)
    {
        return;
    }
#if WITH_EDITOR
    const FScopedTransaction Transaction(LOCTEXT("NormalizeZone", "Zone Transform 정리"));
#endif
    TGuardValue<bool> Guard(bIsNormalizing, true);
    FFZASplineAuthoring::NormalizeActorTransformPreservingSpline(*this, *BoundarySpline, MakeZoneNormalizeOptions());
    UpdateEditorPresentation();
}

void AFFZAAuthoredZone::RefreshAuthoringStatus()
{
    UpdateEditorPresentation();
}

void AFFZAAuthoredZone::CollectAuthoringIssues(TArray<FText>& OutIssues) const
{
    OutIssues.Reset();

    // 1) 부모(FF_ZoneSystem)의 Zone 규칙 검증을 그대로 사용한다. 규칙을 복제하지 않는다.
    TArray<FText> BaseErrors;
    ValidateZoneData(BaseErrors);
    OutIssues.Append(BaseErrors);

    // 2) 정의 에셋 관련 검사.
    if (!ZoneDefinition)
    {
        OutIssues.Add(LOCTEXT("NoDefinition", "Zone Definition 에셋이 지정되지 않았습니다. 'Zone Definition'에서 종류를 고르세요."));
    }
    else
    {
        TArray<FText> DefinitionErrors;
        if (!ZoneDefinition->ValidateDefinition(DefinitionErrors))
        {
            for (const FText& Error : DefinitionErrors)
            {
                OutIssues.Add(FText::Format(LOCTEXT("DefinitionError", "[정의 에셋 {0}] {1}"),
                    FText::FromString(ZoneDefinition->GetName()), Error));
            }
        }
        if (IsOutOfSyncWithDefinition())
        {
            OutIssues.Add(LOCTEXT("OutOfSync", "정의 에셋과 Zone 값이 다릅니다. 'Apply Definition'을 누르고 레벨을 저장하세요."));
        }
        const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
        if (Settings && Settings->bRestrictZoneIdsToProjectPresets && !ZoneDefinition->ZoneId.IsNone() &&
            !Settings->ProjectZoneIds.Contains(ZoneDefinition->ZoneId))
        {
            OutIssues.Add(LOCTEXT("SyncHint", "해결 방법: 레벨의 'FF Zone Layout Manager'에서 'Sync Project Zone Ids'를 누르면 Project Settings에 자동 등록됩니다."));
        }
    }

    // 3) 같은 레벨에 같은 ID를 쓰는 다른 Zone이 있는지(에디터에서 미리 발견).
    if (const UWorld* World = GetWorld(); World && !World->IsGameWorld() && !ZoneId.IsNone())
    {
        for (TActorIterator<AFFZSGameplayZone> It(const_cast<UWorld*>(World)); It; ++It)
        {
            const AFFZSGameplayZone* Other = *It;
            if (Other && Other != this && IsValid(Other) && Other->ZoneId == ZoneId)
            {
                OutIssues.Add(FText::Format(LOCTEXT("DuplicateId", "같은 Zone ID '{0}'를 쓰는 다른 Zone이 있습니다: {1}"),
                    FText::FromName(ZoneId), FText::FromString(Other->GetActorNameOrLabel())));
                break;
            }
        }
    }

#if WITH_EDITOR
    // 4) Cook에서 제거되는 조상에 의존하면 런타임 Zone의 월드 경계가 달라질 수 있다.
    // Actor 부모만 보지 않고 중간 SceneComponent까지 전체 attachment 체인을 검사한다.
    if (!IsEditorOnly())
    {
        const USceneComponent* ZoneRoot = GetRootComponent();
        for (const USceneComponent* Parent = ZoneRoot ? ZoneRoot->GetAttachParent() : nullptr;
            Parent; Parent = Parent->GetAttachParent())
        {
            const AActor* ParentOwner = Parent->GetOwner();
            if (Parent->IsEditorOnly() || (ParentOwner && ParentOwner->IsEditorOnly()))
            {
                OutIssues.Add(FText::Format(LOCTEXT("EditorOnlyAttachmentParent",
                    "런타임 Zone이 에디터 전용 부모 컴포넌트 또는 액터에 연결되어 있습니다: {0}. "
                    "패키징 시 부모가 제거되면 Zone의 월드 위치와 이동 경계가 달라질 수 있습니다. "
                    "Zone을 'Keep World'로 연결 해제하여 월드 Transform을 보존하고, 정리에는 Outliner 폴더를 사용하세요."),
                    FText::FromString(Parent->GetPathName())));
                break;
            }
        }
    }
#endif
}

void AFFZAAuthoredZone::UpdateEditorPresentation()
{
    // 경계/ID가 바뀌었을 수 있으므로 좌표→Zone 조회 지도를 다시 만들게 한다.
    if (const UWorld* World = GetWorld())
    {
        if (UFFZAZoneAuthoringSubsystem* Authoring = World->GetSubsystem<UFFZAZoneAuthoringSubsystem>())
        {
            Authoring->MarkZoneCellMapDirty();
        }
    }
    CollectAuthoringIssues(AuthoringIssues);
    bAuthoringDataValid = AuthoringIssues.IsEmpty();
    if (BoundarySpline)
    {
        FFZASplineAuthoring::SetEditorColor(*BoundarySpline, bAuthoringDataValid ? GetZoneColor() : InvalidZoneColor);
    }
}

void AFFZAAuthoredZone::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    const UWorld* World = GetWorld();
    if (!World || World->IsGameWorld() || IsTemplate())
    {
        return;
    }
    if (bAutoFixBoundarySpline && BoundarySpline)
    {
        FFZASplineAuthoring::EnsureClosedLinear(*BoundarySpline);
    }
    CopyDefinitionToZone();
    UpdateEditorPresentation();
}

void AFFZAAuthoredZone::BeginPlay()
{
    // 등록(부모 BeginPlay) 전에 정의 값을 반영한다. 단, 월드 초기 스캔으로 이미 다른 ID로 등록됐다면
    // 키를 바꾸지 않고 경고만 남긴다(에디터에서 Apply Definition 후 저장하면 해결).
    if (ZoneDefinition && IsOutOfSyncWithDefinition())
    {
        const UFFZSWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
        const bool bAlreadyRegistered = Subsystem && !ZoneId.IsNone() && Subsystem->FindZone(ZoneId) == this;
        if (!bAlreadyRegistered || ZoneId == ZoneDefinition->ZoneId)
        {
            CopyDefinitionToZone();
        }
        else
        {
            UE_LOG(LogFFZoneAuthoring, Warning,
                TEXT("%s: 레벨에 저장된 Zone ID '%s'가 정의 '%s'와 다릅니다. 에디터에서 Apply Definition 후 레벨을 저장하세요."),
                *GetName(), *ZoneId.ToString(), *ZoneDefinition->ZoneId.ToString());
        }
    }
    Super::BeginPlay();
}

#if WITH_EDITOR
void AFFZAAuthoredZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
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
    CopyDefinitionToZone();
    if (bAutoFixBoundarySpline && BoundarySpline)
    {
        FFZASplineAuthoring::EnsureClosedLinear(*BoundarySpline);
    }
    // 슬라이더를 끄는 중(Interactive)에는 건드리지 않고, 값이 확정됐을 때만 정리한다.
    if (bAutoNormalizeTransform && BoundarySpline && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
    {
        FFZASplineAuthoring::NormalizeActorTransformPreservingSpline(*this, *BoundarySpline, MakeZoneNormalizeOptions());
    }
    UpdateEditorPresentation();
}

void AFFZAAuthoredZone::PostEditMove(const bool bFinished)
{
    Super::PostEditMove(bFinished);
    if (!bFinished || bIsNormalizing || GIsTransacting)
    {
        return;
    }
    TGuardValue<bool> Guard(bIsNormalizing, true);
    if (bAutoNormalizeTransform && BoundarySpline)
    {
        const FScopedTransaction Transaction(LOCTEXT("AutoNormalizeZone", "Zone Transform 자동 정리"));
        FFZASplineAuthoring::NormalizeActorTransformPreservingSpline(*this, *BoundarySpline, MakeZoneNormalizeOptions());
    }
    UpdateEditorPresentation();
}

EDataValidationResult AFFZAAuthoredZone::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TArray<FText> Issues;
    CollectAuthoringIssues(Issues);
    if (!Issues.IsEmpty())
    {
        for (const FText& Issue : Issues)
        {
            Context.AddError(FText::Format(LOCTEXT("ZoneIssue", "{0}: {1}"), FText::FromString(GetActorNameOrLabel()), Issue));
        }
        return EDataValidationResult::Invalid;
    }
    return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
