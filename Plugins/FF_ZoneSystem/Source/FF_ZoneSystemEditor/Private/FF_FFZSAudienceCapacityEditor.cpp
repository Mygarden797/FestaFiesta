// FF_FFZSAudienceCapacityEditor.cpp
// 역할: 에디터에서 관람 영역을 기준으로 시설 수용량을 갱신하고 관련 편집 이벤트를 관리한다.
#include "FF_FFZSAudienceCapacityEditor.h"

#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityEditorHelpers.h"
#include "FF_FFZSGridLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/DataValidation.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FFZSAudienceCapacityEditor"

namespace
{
    struct FPendingAudienceEdit
    {
        FFFZSAudienceLayout Before;
        FName PropertyName;
    };
    struct FRequestedAudienceEdit
    {
        int32 Columns;
        int32 Rows;
        float AreaLimitM2;
    };
    TMap<TWeakObjectPtr<UFFZSFacilityDefinition>, FPendingAudienceEdit> PendingEdits;
    TMap<TWeakObjectPtr<const UFFZSFacilityDefinition>, FText> StatusMessages;
    TMap<TWeakObjectPtr<const UFFZSFacilityDefinition>, FRequestedAudienceEdit> RequestedEdits;
    FDelegateHandle PreEditHandle;
    FDelegateHandle PostEditHandle;
    bool bApplying = false;

    bool IsManagedProperty(const FProperty* Property)
    {
        return Property && Property->GetOwnerStruct() == FFFZSAudienceLayout::StaticStruct()
            && (Property->GetFName() == GET_MEMBER_NAME_CHECKED(FFFZSAudienceLayout, Columns)
                || Property->GetFName() == GET_MEMBER_NAME_CHECKED(FFFZSAudienceLayout, Rows)
                || Property->GetFName() == GET_MEMBER_NAME_CHECKED(FFFZSAudienceLayout, AreaLimitM2));
    }

    void BeforeEdit(UObject* Object, const FEditPropertyChain& Chain)
    {
        if (bApplying || GIsTransacting) return;
        UFFZSFacilityDefinition* Definition = Cast<UFFZSFacilityDefinition>(Object);
        const FProperty* Property = Chain.GetActiveNode() ? Chain.GetActiveNode()->GetValue() : nullptr;
        if (!Definition || Definition->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
            || !Definition->Spec.Audience.bEnabled || !IsManagedProperty(Property)) return;
        for (auto It = PendingEdits.CreateIterator(); It; ++It)
            if (!It.Key().IsValid()) It.RemoveCurrent();
        for (auto It = StatusMessages.CreateIterator(); It; ++It)
            if (!It.Key().IsValid()) It.RemoveCurrent();
        for (auto It = RequestedEdits.CreateIterator(); It; ++It)
            if (!It.Key().IsValid()) It.RemoveCurrent();
        const TWeakObjectPtr<UFFZSFacilityDefinition> Key(Definition);
        // Keep the first value throughout a drag. Full validation runs only on commit.
        const FPendingAudienceEdit* Existing = PendingEdits.Find(Key);
        if (!Existing || Existing->PropertyName != Property->GetFName())
            PendingEdits.Add(Key, {Definition->Spec.Audience, Property->GetFName()});
    }

    bool ApplyRequested(UFFZSFacilityDefinition& Asset, const FFFZSAudienceLayout& Before)
    {
        UFFZSFacilityDefinition* Definition = &Asset;
        const FRequestedAudienceEdit Requested{Asset.Spec.Audience.Columns, Asset.Spec.Audience.Rows, Asset.Spec.Audience.AreaLimitM2};
        TGuardValue<bool> Guard(bApplying, true);
        FFFZSAudienceLayout Fitted;
        TArray<FText> Errors;
        TArray<FText> Warnings;
        TArray<FText> Adjustments;
        bool bValid = FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Definition->Spec, Fitted, Errors, &Adjustments);
        if (bValid)
        {
            RequestedEdits.Remove(Definition);
            Definition->Spec.Audience = Fitted;
            // Includes every enabled upgrade, then the exact same cross-asset validation as
            // Validate Facility Definition. Related assets are read for identity/reference checks only.
            bValid = Definition->IsDefinitionValid(Errors);
            if (bValid)
            {
                UEditorValidatorSubsystem* Validator = GEditor ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() : nullptr;
                bValid = Validator && Validator->IsObjectValid(Definition, Errors, Warnings,
                    EDataValidationUsecase::Manual) == EDataValidationResult::Valid;
                if (!bValid && Errors.IsEmpty())
                    Errors.Add(LOCTEXT("ValidatorUnavailable", "전체 시설 검증을 완료할 수 없습니다. 에셋 검색 완료 후 다시 시도하세요."));
            }
        }
        FText Status;
        if (bValid)
        {
            FFFZSCellRect Bounds;
            UFFZSGridLibrary::TryGetAudienceReservedRect(Definition->Spec, Bounds);
            Status = FText::Format(LOCTEXT("Applied", "자동 보정 및 전체 검증 통과 · 요청 {0}명 적용 · 관람 영역 {1}m²\n입력한 행·열을 유지했습니다. 저장 후 테스트 시설에서 실제 경로를 확인하세요."),
                FText::AsNumber(Fitted.GetCapacity()), FText::AsNumber(Bounds.GetArea() / 10000.));
            for (const FText& Adjustment : Adjustments)
                Status = FText::Format(LOCTEXT("Adjustment", "{0}\n• {1}"), Status, Adjustment);
        }
        else
        {
            // Keep the requested numbers separately rather than losing them on an unrelated error.
            // Only fully validated geometry enters Spec; retry uses fresh geometry after the fix.
            RequestedEdits.Add(Definition, Requested);
            Definition->Spec.Audience = Before;
            FString Details;
            for (const FText& Error : Errors) Details += TEXT("\n") + Error.ToString();
            Status = FText::Format(LOCTEXT("Pending", "적용 대기: 요청 {0}열 × {1}줄 = {2}명을 보관했습니다.\n자동 보정만으로 해결할 수 없는 원인:{3}\n해당 문제를 고친 뒤 Apply Requested Audience Values를 누르세요. 다시 입력할 필요가 없습니다.\n현재 설치 데이터에는 검증 전 변경을 적용하지 않았습니다."),
                FText::AsNumber(Requested.Columns), FText::AsNumber(Requested.Rows),
                FText::AsNumber(static_cast<int64>(Requested.Columns) * Requested.Rows), FText::FromString(Details));
            if (!Adjustments.IsEmpty())
            {
                Status = FText::Format(LOCTEXT("ProposedRepairs", "{0}\n자동 계산한 보정안 (전체 검증 전이므로 아직 미적용):"), Status);
                for (const FText& Adjustment : Adjustments)
                    Status = FText::Format(LOCTEXT("Adjustment", "{0}\n• {1}"), Status, Adjustment);
            }
        }
        for (const FText& Warning : Warnings)
            Status = FText::Format(LOCTEXT("Warning", "{0}\n경고: {1}"), Status, Warning);
        StatusMessages.Add(Definition, Status);
        UE_LOG(LogTemp, Display, TEXT("[Facility Audience] %s: %s"), *Definition->GetName(), *Status.ToString());
        if (!IsRunningCommandlet() && FSlateApplication::IsInitialized())
        {
            FNotificationInfo Info(bValid ? LOCTEXT("SuccessToast", "관람 배치 자동 보정 · 전체 검증 통과")
                : LOCTEXT("FailureToast", "요청 인원을 보관했습니다. Facility Audience의 원인과 보정안을 확인하세요."));
            Info.ExpireDuration = 5.f;
            if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
                Item->SetCompletionState(bValid ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
        }
        return bValid;
    }

    void AfterEdit(UObject* Object, FPropertyChangedEvent& Event)
    {
        UFFZSFacilityDefinition* Definition = Cast<UFFZSFacilityDefinition>(Object);
        if (!Definition || bApplying) return;
        const TWeakObjectPtr<UFFZSFacilityDefinition> Key(Definition);
        if (GIsTransacting)
        {
            PendingEdits.Remove(Key);
            StatusMessages.Remove(Definition);
            RequestedEdits.Remove(Definition);
            return;
        }
        if (!IsManagedProperty(Event.Property))
        {
            PendingEdits.Remove(Key);
            return;
        }
        FPendingAudienceEdit* Pending = PendingEdits.Find(Key);
        if (!Pending || Pending->PropertyName != Event.Property->GetFName()) return;
        if ((Event.ChangeType & EPropertyChangeType::Interactive) != 0) return;
        const FFFZSAudienceLayout Before = Pending->Before;
        PendingEdits.Remove(Key);
        ApplyRequested(*Definition, Before);
    }
}

void FFFZSAudienceCapacityEditor::Register()
{
    if (PreEditHandle.IsValid()) return;
    PreEditHandle = FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddStatic(&BeforeEdit);
    PostEditHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddStatic(&AfterEdit);
}

void FFFZSAudienceCapacityEditor::Unregister()
{
    FCoreUObjectDelegates::OnPreObjectPropertyChanged.Remove(PreEditHandle);
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PostEditHandle);
    PreEditHandle.Reset();
    PostEditHandle.Reset();
    PendingEdits.Reset();
    StatusMessages.Reset();
    RequestedEdits.Reset();
}

FText FFFZSAudienceCapacityEditor::GetStatus(const UFFZSFacilityDefinition* Definition)
{
    const FText* Status = StatusMessages.Find(Definition);
    return Status ? *Status : LOCTEXT("Instructions", "Columns / Rows를 확정하면 요청 인원을 유지하며 자리·입퇴장·부족한 면적을 자동 보정합니다.\n전체 검증 결과와 보정 이유·변경 전후 값을 여기에 표시합니다.");
}

bool FFFZSAudienceCapacityEditor::CanRetry(const UFFZSFacilityDefinition* Definition)
{
    return Definition && Definition->Spec.Audience.bEnabled && RequestedEdits.Contains(Definition);
}

bool FFFZSAudienceCapacityEditor::Retry(UFFZSFacilityDefinition& Definition)
{
    if (!CanRetry(&Definition) || bApplying) return false;
    const FRequestedAudienceEdit Request = RequestedEdits.FindChecked(&Definition);
    const FFFZSAudienceLayout Before = Definition.Spec.Audience;
    FScopedTransaction Transaction(LOCTEXT("RetryTransaction", "보관한 관람 인원 적용"));
    Definition.Modify();
    Definition.Spec.Audience.Columns = Request.Columns;
    Definition.Spec.Audience.Rows = Request.Rows;
    Definition.Spec.Audience.AreaLimitM2 = Request.AreaLimitM2;
    const bool bApplied = ApplyRequested(Definition, Before);
    FPropertyChangedEvent Event(FindFProperty<FProperty>(FFFZSFacilitySpec::StaticStruct(),
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Audience)), EPropertyChangeType::ValueSet);
    Event.SetActiveMemberProperty(FindFProperty<FProperty>(UFFZSFacilityDefinition::StaticClass(),
        GET_MEMBER_NAME_CHECKED(UFFZSFacilityDefinition, Spec)));
    Definition.PostEditChangeProperty(Event);
    if (!bApplied) Transaction.Cancel();
    return bApplied;
}

#undef LOCTEXT_NAMESPACE
