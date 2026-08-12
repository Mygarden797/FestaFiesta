#include "FF_FFZSFacilityDefinitionCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityEditorHelpers.h"
#include "FF_FFZSVisualGeometry.h"
#include "Engine/StaticMesh.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IPropertyUtilities.h"
#include "Misc/MessageDialog.h"
#include "Misc/DataValidation.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FFZSFacilityDefinitionCustomization"

namespace FFZSFacilityDefinitionCustomizationPrivate
{
    void LocalizeCategory(
        IDetailLayoutBuilder& DetailBuilder,
        const FName CategoryName,
        const FText& DisplayName,
        const FText& ToolTip)
    {
        IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, DisplayName);
        Category.SetToolTip(ToolTip);
    }

    void OverrideChildLabel(
        const TSharedPtr<IPropertyHandle>& Parent,
        const FName ChildName,
        const FText& DisplayName,
        const FText& ToolTip)
    {
        if (!Parent.IsValid())
        {
            return;
        }
        if (const TSharedPtr<IPropertyHandle> Child = Parent->GetChildHandle(ChildName))
        {
            Child->SetPropertyDisplayName(DisplayName);
            Child->SetToolTipText(ToolTip);
        }
    }

    void OverrideVectorChildren(
        const TSharedPtr<IPropertyHandle>& Vector,
        const FText& XName,
        const FText& XToolTip,
        const FText& YName,
        const FText& YToolTip,
        const FText& ZName,
        const FText& ZToolTip)
    {
        OverrideChildLabel(Vector, TEXT("X"), XName, XToolTip);
        OverrideChildLabel(Vector, TEXT("Y"), YName, YToolTip);
        OverrideChildLabel(Vector, TEXT("Z"), ZName, ZToolTip);
    }

    void RefreshDetailsWhenArraySizeChanges(
        const TSharedPtr<IPropertyHandle>& Property,
        const TSharedRef<IPropertyUtilities>& PropertyUtilities)
    {
        const TSharedPtr<IPropertyHandleArray> Array = Property.IsValid() ? Property->AsArray() : nullptr;
        if (!Array.IsValid())
        {
            return;
        }

        const TWeakPtr<IPropertyUtilities> WeakPropertyUtilities = PropertyUtilities;
        Array->SetOnNumElementsChanged(FSimpleDelegate::CreateLambda(
            [WeakPropertyUtilities]()
            {
                if (const TSharedPtr<IPropertyUtilities> PinnedUtilities = WeakPropertyUtilities.Pin())
                {
                    // Re-run this customization so newly added array elements receive the same
                    // English labels and Korean hover help as the elements that were initially visible.
                    PinnedUtilities->RequestForceRefresh();
                }
            }));
    }

    void LocalizeRectArray(const TSharedPtr<IPropertyHandle>& Rects)
    {
        const TSharedPtr<IPropertyHandleArray> Array = Rects.IsValid() ? Rects->AsArray() : nullptr;
        uint32 NumElements = 0;
        if (!Array.IsValid() || Array->GetNumElements(NumElements) != FPropertyAccess::Success)
        {
            return;
        }
        for (uint32 Index = 0; Index < NumElements; ++Index)
        {
            const TSharedRef<IPropertyHandle> Rect = Array->GetElement(Index);
            Rect->SetPropertyDisplayName(FText::Format(
                LOCTEXT("QueueRectElement", "Reserved Rectangle {0}"), static_cast<int32>(Index)));
            Rect->SetToolTipText(LOCTEXT(
                "QueueRectElementTip",
                "건물 왼쪽 아래를 원점으로 한 1cm 반열림 대기열 예약 사각형입니다."));
            const TSharedPtr<IPropertyHandle> Min = Rect->GetChildHandle(
                GET_MEMBER_NAME_CHECKED(FFFZSCellRect, Min));
            const TSharedPtr<IPropertyHandle> Max = Rect->GetChildHandle(
                GET_MEMBER_NAME_CHECKED(FFFZSCellRect, Max));
            OverrideChildLabel(Min, TEXT("X"), LOCTEXT("RectMinX", "Left Bound X (Inclusive)"),
                LOCTEXT("RectMinXTip", "예약 영역에 포함되는 왼쪽 1cm 셀 경계입니다."));
            OverrideChildLabel(Min, TEXT("Y"), LOCTEXT("RectMinY", "Bottom Bound Y (Inclusive)"),
                LOCTEXT("RectMinYTip", "예약 영역에 포함되는 아래쪽 1cm 셀 경계입니다."));
            OverrideChildLabel(Max, TEXT("X"), LOCTEXT("RectMaxX", "Right Bound X (Exclusive)"),
                LOCTEXT("RectMaxXTip", "예약 영역에 포함되지 않는 오른쪽 경계입니다. 폭은 Max.X - Min.X입니다."));
            OverrideChildLabel(Max, TEXT("Y"), LOCTEXT("RectMaxY", "Top Bound Y (Exclusive)"),
                LOCTEXT("RectMaxYTip", "예약 영역에 포함되지 않는 위쪽 경계입니다. 높이는 Max.Y - Min.Y입니다."));
        }
    }

    void LocalizeQueuePointArray(const TSharedPtr<IPropertyHandle>& Points)
    {
        const TSharedPtr<IPropertyHandleArray> Array = Points.IsValid() ? Points->AsArray() : nullptr;
        uint32 NumElements = 0;
        if (!Array.IsValid() || Array->GetNumElements(NumElements) != FPropertyAccess::Success)
        {
            return;
        }
        for (uint32 Index = 0; Index < NumElements; ++Index)
        {
            const TSharedRef<IPropertyHandle> Point = Array->GetElement(Index);
            Point->SetPropertyDisplayName(FText::Format(
                LOCTEXT("QueuePointElement", "Queue Point {0}"), static_cast<int32>(Index)));
            Point->SetToolTipText(Index == 0
                ? LOCTEXT("QueueHeadPointTip", "줄 머리·서비스 쪽의 첫 번째 시설 로컬 cm 경로 점입니다.")
                : LOCTEXT("QueuePathPointTip", "앞 점과 직선으로 연결되는 시설 로컬 cm 경로 점입니다."));
            OverrideVectorChildren(
                Point,
                LOCTEXT("QueuePointX", "Lateral Position (X)"),
                LOCTEXT("QueuePointXTip", "건물 중심 기준 좌우 위치(cm)입니다."),
                LOCTEXT("QueuePointY", "Forward Position (Y)"),
                LOCTEXT("QueuePointYTip", "건물 중심 기준 앞뒤 위치(cm)입니다. 시설의 손님 방향은 +Y입니다."),
                LOCTEXT("QueuePointZ", "Height (Z)"),
                LOCTEXT("QueuePointZTip", "경로 점의 높이(cm)입니다. 일반적인 평면 대기열은 0을 사용합니다."));
        }
    }

    void LocalizeAnchorArray(const TSharedPtr<IPropertyHandle>& Anchors)
    {
        const TSharedPtr<IPropertyHandleArray> Array = Anchors.IsValid() ? Anchors->AsArray() : nullptr;
        uint32 NumElements = 0;
        if (!Array.IsValid() || Array->GetNumElements(NumElements) != FPropertyAccess::Success)
        {
            return;
        }
        for (uint32 Index = 0; Index < NumElements; ++Index)
        {
            const TSharedRef<IPropertyHandle> Anchor = Array->GetElement(Index);
            Anchor->SetPropertyDisplayName(FText::Format(
                LOCTEXT("AnchorElement", "Anchor {0}"), static_cast<int32>(Index)));
            Anchor->SetToolTipText(LOCTEXT(
                "AnchorElementTip",
                "진입·서비스·퇴장 중 하나의 역할과 시설 로컬 위치·방향을 정의합니다."));
            const TSharedPtr<IPropertyHandle> Transform = Anchor->GetChildHandle(
                GET_MEMBER_NAME_CHECKED(FFFZSAnchorDefinition, LocalTransform));
            OverrideChildLabel(Transform, TEXT("Translation"), LOCTEXT("AnchorLocation", "Facility Local Position"),
                LOCTEXT("AnchorLocationTip", "건물 중심 기준 이동 목표 위치(cm)입니다."));
            OverrideChildLabel(Transform, TEXT("Rotation"), LOCTEXT("AnchorRotation", "Arrival Direction"),
                LOCTEXT("AnchorRotationTip", "NPC가 이 지점에서 바라볼 시설 로컬 방향입니다."));
            OverrideChildLabel(Transform, TEXT("Scale3D"), LOCTEXT("AnchorScale", "Scale (Always 1)"),
                LOCTEXT("AnchorScaleTip", "앵커 좌표 계약을 지키려면 X·Y·Z 모두 반드시 1이어야 합니다."));
        }
    }

    void LocalizeSpecCoordinateChildren(IDetailLayoutBuilder& DetailBuilder)
    {
        const TSharedRef<IPropertyHandle> Spec = DetailBuilder.GetProperty(
            GET_MEMBER_NAME_CHECKED(UFFZSFacilityDefinition, Spec));
        const TSharedPtr<IPropertyHandle> Footprint = Spec->GetChildHandle(
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, FootprintSizeCells));
        OverrideChildLabel(Footprint, TEXT("X"), LOCTEXT("FootprintX", "Width (X)"),
            LOCTEXT("FootprintXTip", "회전 전 건물의 가로 폭입니다. 1 단위는 1cm입니다."));
        OverrideChildLabel(Footprint, TEXT("Y"), LOCTEXT("FootprintY", "Depth (Y)"),
            LOCTEXT("FootprintYTip", "회전 전 건물의 세로 깊이입니다. 1 단위는 1cm입니다."));

        const TSharedPtr<IPropertyHandle> VisualOffset = Spec->GetChildHandle(
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, VisualOffsetCm));
        OverrideChildLabel(VisualOffset, TEXT("X"), LOCTEXT("VisualOffsetX", "Width Offset (X)"),
            LOCTEXT("VisualOffsetXTip", "건물 중심 기준 좌우 메시 보정값(cm)입니다."));
        OverrideChildLabel(VisualOffset, TEXT("Y"), LOCTEXT("VisualOffsetY", "Depth Offset (Y)"),
            LOCTEXT("VisualOffsetYTip", "건물 중심 기준 앞뒤 메시 보정값(cm)입니다. 시설의 손님 방향은 +Y입니다."));
        OverrideChildLabel(VisualOffset, TEXT("Z"), LOCTEXT("VisualOffsetZ", "Height Offset (Z)"),
            LOCTEXT("VisualOffsetZTip", "지면 정렬 뒤 추가할 위아래 메시 보정값(cm)입니다."));

        const TSharedPtr<IPropertyHandle> VisualRotation = Spec->GetChildHandle(
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, VisualRotation));
        OverrideChildLabel(VisualRotation, TEXT("Roll"), LOCTEXT("VisualRoll", "Roll (X Axis)"),
            LOCTEXT("VisualRollTip", "메시의 X축 기울기입니다. 배치 경계 정확 맞춤에서는 0이어야 합니다."));
        OverrideChildLabel(VisualRotation, TEXT("Pitch"), LOCTEXT("VisualPitch", "Pitch (Y Axis)"),
            LOCTEXT("VisualPitchTip", "메시의 Y축 기울기입니다. 배치 경계 정확 맞춤에서는 0이어야 합니다."));
        OverrideChildLabel(VisualRotation, TEXT("Yaw"), LOCTEXT("VisualYaw", "Yaw (Z Axis)"),
            LOCTEXT("VisualYawTip", "위에서 본 메시 방향 보정입니다. 정확 맞춤에서는 0·90·180·270도만 사용합니다."));

        const TSharedPtr<IPropertyHandle> QueueReservedRects = Spec->GetChildHandle(
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueReservedRects));
        const TSharedPtr<IPropertyHandle> QueueSplinePoints = Spec->GetChildHandle(
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueSplinePointsCm));
        const TSharedPtr<IPropertyHandle> Anchors = Spec->GetChildHandle(
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Anchors));

        LocalizeRectArray(QueueReservedRects);
        LocalizeQueuePointArray(QueueSplinePoints);
        LocalizeAnchorArray(Anchors);

        const TSharedRef<IPropertyUtilities> PropertyUtilities = DetailBuilder.GetPropertyUtilities();
        RefreshDetailsWhenArraySizeChanges(QueueReservedRects, PropertyUtilities);
        RefreshDetailsWhenArraySizeChanges(QueueSplinePoints, PropertyUtilities);
        RefreshDetailsWhenArraySizeChanges(Anchors, PropertyUtilities);
    }

    FText JoinValidationMessages(
        const UFFZSFacilityDefinition& Definition,
        const TArray<FText>& Errors,
        const TArray<FText>& Warnings)
    {
        FString Message = FString::Printf(TEXT("%s\n"), *Definition.GetName());
        for (const FText& Error : Errors)
        {
            Message += FString::Printf(TEXT("  - %s\n"), *Error.ToString());
        }
        for (const FText& Warning : Warnings)
        {
            Message += FString::Printf(TEXT("  - 경고: %s\n"), *Warning.ToString());
        }
        return FText::FromString(Message);
    }

    bool IsValidCandidate(const FFFZSFacilitySpec& Candidate)
    {
        TArray<FText> Errors;
        return Candidate.IsValid(Errors);
    }

    const FProperty* FindSpecProperty(const FName PropertyName)
    {
        return FFFZSFacilitySpec::StaticStruct()->FindPropertyByName(PropertyName);
    }

    void NotifyPropertyChanged(UFFZSFacilityDefinition& Definition, const FProperty* ChangedProperty)
    {
        FPropertyChangedEvent PropertyChangedEvent(
            const_cast<FProperty*>(ChangedProperty),
            EPropertyChangeType::ValueSet);
        Definition.PostEditChangeProperty(PropertyChangedEvent);
        Definition.MarkPackageDirty();
    }

}

TSharedRef<IDetailCustomization> FFFZSFacilityDefinitionCustomization::MakeInstance()
{
    return MakeShared<FFFZSFacilityDefinitionCustomization>();
}

void FFFZSFacilityDefinitionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    Definitions.Reset();
    TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
    DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);
    for (const TWeakObjectPtr<UObject>& Object : CustomizedObjects)
    {
        if (UFFZSFacilityDefinition* Definition = Cast<UFFZSFacilityDefinition>(Object.Get()))
        {
            Definitions.Add(Definition);
        }
    }

    FFZSFacilityDefinitionCustomizationPrivate::LocalizeCategory(
        DetailBuilder, TEXT("FF Zone System"), LOCTEXT("ZoneSystemCategory", "FF Zone System"),
        LOCTEXT("ZoneSystemCategoryTip", "시설의 단일 원천 사양과 카탈로그 표시 정보를 편집합니다."));
    FFZSFacilityDefinitionCustomizationPrivate::LocalizeCategory(
        DetailBuilder, TEXT("FF Zone System|Presentation"), LOCTEXT("PresentationCategory", "Presentation"),
        LOCTEXT("PresentationCategoryTip", "카탈로그처럼 게임 화면에 표시할 선택적 정보를 편집합니다."));
    FFZSFacilityDefinitionCustomizationPrivate::LocalizeCategory(
        DetailBuilder, TEXT("FF Zone System|Authoring"), LOCTEXT("AuthoringCategory", "Authoring"),
        LOCTEXT("AuthoringCategoryTip", "에디터와 절차형 아트 도구 전용 소유권 정보를 편집합니다."));
    FFZSFacilityDefinitionCustomizationPrivate::LocalizeSpecCoordinateChildren(DetailBuilder);

    IDetailCategoryBuilder& ToolsCategory = DetailBuilder.EditCategory(
        TEXT("FF Zone System|Authoring Tools"),
        LOCTEXT("AuthoringToolsCategory", "Facility Authoring Tools"),
        ECategoryPriority::Important);
    ToolsCategory.SetToolTip(LOCTEXT(
        "AuthoringToolsCategoryTip",
        "시설 크기·대기열·표시 메시를 안전하게 계산하고 검증하는 제작 도구입니다. 각 버튼에 마우스를 올리면 자세한 한국어 설명을 볼 수 있습니다."));

    ToolsCategory.AddCustomRow(LOCTEXT("KoreanHelpSearchText", "Korean Help 한국어 도움말"))
    .WholeRowContent()
    [
        SNew(SButton)
        .Text(LOCTEXT("ShowKoreanHelpButton", "Show Korean Help"))
        .ToolTipText(LOCTEXT(
            "ShowKoreanHelpTooltip",
            "시설 크기·대기열·표시 메시와 저작 버튼의 핵심 사용법을 한국어 대화상자로 엽니다."))
        .OnClicked(this, &FFFZSFacilityDefinitionCustomization::ShowKoreanHelp)
    ];

    ToolsCategory.AddCustomRow(LOCTEXT("ValidationSearchText", "시설 정의 검증 Validate Definition"))
    .WholeRowContent()
    [
        SNew(SButton)
        .Text(LOCTEXT("ValidateDefinitionButton", "Validate Facility Definition"))
        .ToolTipText(LOCTEXT(
            "ValidateDefinitionTooltip",
            "현재 선택한 시설 정의에 등록된 전체 데이터 검증기를 실행합니다. Spec 내부 규칙, 필수 폴더, FacilityDefinitionId·PrimaryAssetId 중복, VisualMesh 참조와 Bounds를 확인하며 어떤 값도 바꾸지 않습니다."))
        .IsEnabled(this, &FFFZSFacilityDefinitionCustomization::HasAnyEditableDefinition)
        .OnClicked(this, &FFFZSFacilityDefinitionCustomization::ValidateDefinitions)
    ];

    ToolsCategory.AddCustomRow(LOCTEXT("SizingActionsSearchText", "건물 크기 메시 대기열 꼬리 정원 Footprint Visual Mesh Queue Tail Capacity"))
    .WholeRowContent()
    [
        SNew(SUniformGridPanel)
        .SlotPadding(FMargin(2.0f))
        + SUniformGridPanel::Slot(0, 0)
        [
            SNew(SButton)
            .Text(LOCTEXT("FootprintFromMeshButton", "Footprint <- Loaded Mesh"))
            .ToolTipText(LOCTEXT(
                "FootprintFromMeshTooltip",
                "시설 Data Asset 하나만 선택하고 원본 메시 크기 모드일 때 사용합니다. 현재 메모리에 로드된 VisualMesh에 방향·위치 보정을 적용한 뒤, 메시 전체를 포함하는 보수적인 1cm 단위 건물 크기를 계산합니다. 결과 Spec 전체가 유효할 때만 적용하며 메시를 강제로 로드하지 않습니다."))
            .IsEnabled(this, &FFFZSFacilityDefinitionCustomization::CanSetFootprintFromLoadedVisualMesh)
            .OnClicked(this, &FFFZSFacilityDefinitionCustomization::SetFootprintFromLoadedVisualMesh)
        ]
        + SUniformGridPanel::Slot(1, 0)
        [
            SNew(SButton)
            .Text(LOCTEXT("TailFromCapacityButton", "Queue Tail <- Max Capacity"))
            .ToolTipText(LOCTEXT(
                "TailFromCapacityTooltip",
                "시설 Data Asset 하나만 선택해야 합니다. 점이 2개 이상이고 슬롯 간격이 양수인 대기열에서 마지막 선분 방향으로 꼬리 점을 이동하거나 새 꼬리 점을 추가해 전체 길이를 (최대 대기 인원 - 1) × 슬롯 간격으로 맞춥니다. 생성되는 모든 슬롯이 대기열 예약 사각형 안에 남을 때만 적용합니다."))
            .IsEnabled(this, &FFFZSFacilityDefinitionCustomization::CanResizeQueueTailToCapacity)
            .OnClicked(this, &FFFZSFacilityDefinitionCustomization::ResizeQueueTailToCapacity)
        ]
        + SUniformGridPanel::Slot(2, 0)
        [
            SNew(SButton)
            .Text(LOCTEXT("CapacityFromLengthButton", "Max Capacity <- Queue Length"))
            .ToolTipText(LOCTEXT(
                "CapacityFromLengthTooltip",
                "시설 Data Asset 하나만 선택해야 합니다. 점이 2개 이상이고 슬롯 간격이 양수인 현재 대기열에서 들어갈 수 있는 최대 대기 인원을 계산합니다. 줄 머리의 0번 슬롯을 포함해 floor(길이 ÷ 간격) + 1이며, 결과 슬롯이 모두 예약 영역 안에 있을 때만 적용합니다."))
            .IsEnabled(this, &FFFZSFacilityDefinitionCustomization::CanSetCapacityFromQueueLength)
            .OnClicked(this, &FFFZSFacilityDefinitionCustomization::SetCapacityFromQueueLength)
        ]
    ];

    ToolsCategory.AddCustomRow(LOCTEXT("VisualSizingSearchText", "메시 크기 배치 경계 비율 맞춤 Visual Sizing Placement Bounds Uniform Fit"))
    .WholeRowContent()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text(this, &FFFZSFacilityDefinitionCustomization::GetVisualSizingSummary)
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
        [
            SNew(SButton)
            .Text(LOCTEXT("FitVisualButton", "Fit Mesh to Placement Bounds"))
            .ToolTipText(LOCTEXT(
                "FitVisualTooltip",
                "시설 Data Asset 하나만 선택해야 합니다. 메시 크기 적용 방식을 '배치 경계에 정확히 맞춤'으로 바꾸고 위치 보정을 0으로 초기화합니다. 방향 보정이 0·90·180·270도일 때만 사용할 수 있습니다. 런타임이 표시 컴포넌트 Scale을 계산하며 원본 메시 자산은 수정하지 않습니다."))
            .IsEnabled(this, &FFFZSFacilityDefinitionCustomization::CanFitVisualToFootprint)
            .OnClicked(this, &FFFZSFacilityDefinitionCustomization::FitVisualToFootprint)
        ]
    ];
}

FReply FFFZSFacilityDefinitionCustomization::ShowKoreanHelp()
{
    FMessageDialog::Open(
        EAppMsgType::Ok,
        LOCTEXT(
            "KoreanAuthoringHelpBody",
            "[건물 크기]\n"
            "Footprint Size (cm)는 회전 전 바닥 크기이며 1단위는 1cm입니다. 높이는 Placement Obstacle Height (cm)에서 바꿉니다.\n"
            "Visual Sizing Mode는 프로덕션에서 Match Placement Bounds를 사용하고 Visual Offset은 0을 유지하십시오.\n\n"
            "[대기열]\n"
            "Queue Reserved Rects는 건물 왼쪽 아래 기준의 예약 영역이고, Queue Spline Points와 Anchor는 건물 중심 기준 cm 좌표입니다.\n"
            "필요 경로 길이는 (Max Waiting Agents - 1) × Slot Spacing입니다. 모든 슬롯이 예약 사각형 안에 있어야 합니다.\n\n"
            "[버튼]\n"
            "Footprint <- Loaded Mesh: 로드된 원본 메시에서 건물 크기를 계산합니다.\n"
            "Queue Tail <- Max Capacity: 인원과 간격에 맞춰 경로 꼬리를 조정합니다.\n"
            "Max Capacity <- Queue Length: 현재 경로 길이에서 최대 인원을 계산합니다.\n"
            "Fit Mesh to Placement Bounds: 메시 표시를 권위 건물 크기와 높이에 맞춥니다.\n"
            "Validate Facility Definition: 내부 규칙·ID 중복·메시 참조까지 전체 검증합니다.\n\n"
            "항상 보이는 명칭은 영어이며, 각 항목에 마우스를 올리면 더 자세한 한국어 설명을 확인할 수 있습니다."),
        LOCTEXT("KoreanAuthoringHelpTitle", "시설 저작 한국어 도움말"));
    return FReply::Handled();
}

FReply FFFZSFacilityDefinitionCustomization::ValidateDefinitions()
{
    bool bAllValid = true;
    FString Report;
    const UEditorValidatorSubsystem* ValidatorSubsystem =
        GEditor ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() : nullptr;
    for (const TWeakObjectPtr<UFFZSFacilityDefinition>& WeakDefinition : Definitions)
    {
        const UFFZSFacilityDefinition* Definition = WeakDefinition.Get();
        if (!Definition)
        {
            continue;
        }

        TArray<FText> Errors;
        TArray<FText> Warnings;
        const EDataValidationResult Result = ValidatorSubsystem
            ? ValidatorSubsystem->IsObjectValid(
                const_cast<UFFZSFacilityDefinition*>(Definition),
                Errors,
                Warnings,
                EDataValidationUsecase::Manual)
            : EDataValidationResult::NotValidated;
        if (Result == EDataValidationResult::Valid)
        {
            Report += FString::Printf(TEXT("%s: Valid\n"), *Definition->GetName());
            if (!Warnings.IsEmpty())
            {
                Report += FFZSFacilityDefinitionCustomizationPrivate::JoinValidationMessages(
                    *Definition, {}, Warnings).ToString();
            }
        }
        else
        {
            bAllValid = false;
            if (!ValidatorSubsystem)
            {
                Errors.Add(LOCTEXT(
                    "ValidatorSubsystemUnavailable",
                    "에디터 검증 Subsystem을 사용할 수 없어 안전하게 검증 실패로 처리했습니다."));
            }
            else if (Result == EDataValidationResult::NotValidated)
            {
                Errors.Add(LOCTEXT(
                    "DefinitionNotValidated",
                    "등록된 검증기가 이 시설 정의를 처리하지 않아 안전하게 검증 실패로 처리했습니다."));
            }
            Report += FFZSFacilityDefinitionCustomizationPrivate::JoinValidationMessages(
                *Definition, Errors, Warnings).ToString();
        }
    }

    if (Report.IsEmpty())
    {
        Report = LOCTEXT("NoDefinitionsSelected", "검증할 시설 정의가 없습니다.").ToString();
        bAllValid = false;
    }

    const FText ReportText = FText::FromString(Report);
    ShowNotification(
        bAllValid
            ? LOCTEXT("ValidationPassed", "시설 정의 전체 검증을 통과했습니다.")
            : LOCTEXT("ValidationFailed", "시설 정의 검증에 실패했습니다. 상세 보고서를 확인하십시오."),
        bAllValid);
    FMessageDialog::Open(
        EAppMsgType::Ok,
        ReportText,
        bAllValid
            ? LOCTEXT("ValidationPassedTitle", "시설 정의: 유효함")
            : LOCTEXT("ValidationFailedTitle", "시설 정의: 수정 필요"));
    return FReply::Handled();
}

FReply FFFZSFacilityDefinitionCustomization::SetFootprintFromLoadedVisualMesh()
{
    const FProperty* ChangedProperty = FFZSFacilityDefinitionCustomizationPrivate::FindSpecProperty(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, FootprintSizeCells));
    const int32 ChangedCount = MutateDefinitions(
        LOCTEXT("SetFootprintTransaction", "로드된 메시에서 건물 크기 계산"),
        [ChangedProperty](UFFZSFacilityDefinition& Definition)
        {
            UStaticMesh* Mesh = Definition.Spec.VisualMesh.Get();
            FIntPoint NewFootprint = FIntPoint::ZeroValue;
            if (Definition.Spec.VisualSizingMode != EFFZSVisualSizingMode::SourceMeshSize ||
                !Mesh ||
                !FFFZSFacilityEditorHelpers::TryCalculateFootprintCellsFromSourceBounds(
                    Mesh->GetBoundingBox(),
                    Definition.Spec.VisualRotation,
                    Definition.Spec.VisualOffsetCm,
                    NewFootprint) ||
                NewFootprint == Definition.Spec.FootprintSizeCells)
            {
                return false;
            }

            FFFZSFacilitySpec Candidate = Definition.Spec;
            Candidate.FootprintSizeCells = NewFootprint;
            if (!FFZSFacilityDefinitionCustomizationPrivate::IsValidCandidate(Candidate))
            {
                return false;
            }

            Definition.Spec = MoveTemp(Candidate);
            FFZSFacilityDefinitionCustomizationPrivate::NotifyPropertyChanged(Definition, ChangedProperty);
            return true;
        });
    ShowNotification(
        FText::Format(LOCTEXT("FootprintUpdated", "시설 정의 {0}개의 건물 크기를 갱신했습니다."), ChangedCount),
        ChangedCount > 0);
    return FReply::Handled();
}

FReply FFFZSFacilityDefinitionCustomization::ResizeQueueTailToCapacity()
{
    const FProperty* ChangedProperty = FFZSFacilityDefinitionCustomizationPrivate::FindSpecProperty(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueSplinePointsCm));
    const int32 ChangedCount = MutateDefinitions(
        LOCTEXT("ResizeQueueTailTransaction", "최대 인원에 맞춰 대기열 꼬리 조정"),
        [ChangedProperty](UFFZSFacilityDefinition& Definition)
        {
            double RequiredLengthCm = 0.0;
            TArray<FVector> ResizedPoints;
            if (!FFFZSFacilityEditorHelpers::TryCalculateRequiredQueueLengthCm(
                    Definition.Spec.QueueRules.MaxWaitingAgents,
                    Definition.Spec.QueueRules.SlotSpacingCm,
                    RequiredLengthCm) ||
                !FFFZSFacilityEditorHelpers::TryResizeQueueTail(
                    Definition.Spec.QueueSplinePointsCm,
                    RequiredLengthCm,
                    ResizedPoints) ||
                ResizedPoints == Definition.Spec.QueueSplinePointsCm)
            {
                return false;
            }

            FFFZSFacilitySpec Candidate = Definition.Spec;
            Candidate.QueueSplinePointsCm = MoveTemp(ResizedPoints);
            if (!FFZSFacilityDefinitionCustomizationPrivate::IsValidCandidate(Candidate))
            {
                return false;
            }

            Definition.Spec = MoveTemp(Candidate);
            FFZSFacilityDefinitionCustomizationPrivate::NotifyPropertyChanged(Definition, ChangedProperty);
            return true;
        });
    ShowNotification(
        FText::Format(LOCTEXT("QueueTailUpdated", "시설 정의 {0}개의 대기열 꼬리를 갱신했습니다."), ChangedCount),
        ChangedCount > 0);
    return FReply::Handled();
}

FReply FFFZSFacilityDefinitionCustomization::SetCapacityFromQueueLength()
{
    const FProperty* ChangedProperty = FFFZSQueueRules::StaticStruct()->FindPropertyByName(
        GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, MaxWaitingAgents));
    const int32 ChangedCount = MutateDefinitions(
        LOCTEXT("SetCapacityTransaction", "대기열 길이에서 최대 인원 계산"),
        [ChangedProperty](UFFZSFacilityDefinition& Definition)
        {
            int32 NewCapacity = 0;
            if (!FFFZSFacilityEditorHelpers::TryCalculateMaxWaitingAgents(
                    Definition.Spec.QueueSplinePointsCm,
                    Definition.Spec.QueueRules.SlotSpacingCm,
                    NewCapacity) ||
                NewCapacity == Definition.Spec.QueueRules.MaxWaitingAgents)
            {
                return false;
            }

            FFFZSFacilitySpec Candidate = Definition.Spec;
            Candidate.QueueRules.MaxWaitingAgents = NewCapacity;
            if (!FFZSFacilityDefinitionCustomizationPrivate::IsValidCandidate(Candidate))
            {
                return false;
            }

            Definition.Spec = MoveTemp(Candidate);
            FFZSFacilityDefinitionCustomizationPrivate::NotifyPropertyChanged(Definition, ChangedProperty);
            return true;
        });
    ShowNotification(
        FText::Format(LOCTEXT("CapacityUpdated", "시설 정의 {0}개의 최대 대기 인원을 갱신했습니다."), ChangedCount),
        ChangedCount > 0);
    return FReply::Handled();
}

FReply FFFZSFacilityDefinitionCustomization::FitVisualToFootprint()
{
    const FProperty* SizingModeProperty = FFZSFacilityDefinitionCustomizationPrivate::FindSpecProperty(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, VisualSizingMode));
    const FProperty* OffsetProperty = FFZSFacilityDefinitionCustomizationPrivate::FindSpecProperty(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, VisualOffsetCm));
    const int32 ChangedCount = MutateDefinitions(
        LOCTEXT("FitVisualTransaction", "메시를 권위 배치 크기에 맞춤"),
        [SizingModeProperty, OffsetProperty](UFFZSFacilityDefinition& Definition)
        {
            if (!SizingModeProperty || !OffsetProperty ||
                !FFZSVisualGeometry::IsCardinalYawRotation(Definition.Spec.VisualRotation) ||
                (Definition.Spec.VisualSizingMode == EFFZSVisualSizingMode::MatchPlacementBounds &&
                    Definition.Spec.VisualOffsetCm == FVector::ZeroVector))
            {
                return false;
            }

            FFFZSFacilitySpec Candidate = Definition.Spec;
            Candidate.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
            Candidate.VisualOffsetCm = FVector::ZeroVector;
            if (!FFZSFacilityDefinitionCustomizationPrivate::IsValidCandidate(Candidate))
            {
                return false;
            }

            Definition.Spec = MoveTemp(Candidate);
            FFZSFacilityDefinitionCustomizationPrivate::NotifyPropertyChanged(Definition, SizingModeProperty);
            FFZSFacilityDefinitionCustomizationPrivate::NotifyPropertyChanged(Definition, OffsetProperty);
            return true;
        });
    ShowNotification(
        FText::Format(LOCTEXT("VisualSizingUpdated", "시설 정의 {0}개의 메시를 권위 배치 크기에 맞췄습니다."), ChangedCount),
        ChangedCount > 0);
    return FReply::Handled();
}

bool FFFZSFacilityDefinitionCustomization::HasAnyEditableDefinition() const
{
    return Definitions.ContainsByPredicate(
        [](const TWeakObjectPtr<UFFZSFacilityDefinition>& Definition)
        {
            return Definition.IsValid();
        });
}

bool FFFZSFacilityDefinitionCustomization::CanSetFootprintFromLoadedVisualMesh() const
{
    const UFFZSFacilityDefinition* Definition =
        Definitions.Num() == 1 ? Definitions[0].Get() : nullptr;
    return Definition &&
        Definition->Spec.VisualSizingMode == EFFZSVisualSizingMode::SourceMeshSize &&
        Definition->Spec.VisualMesh.Get() != nullptr;
}

bool FFFZSFacilityDefinitionCustomization::CanResizeQueueTailToCapacity() const
{
    const UFFZSFacilityDefinition* Definition =
        Definitions.Num() == 1 ? Definitions[0].Get() : nullptr;
    return Definition &&
        Definition->Spec.QueueSplinePointsCm.Num() >= 2 &&
        Definition->Spec.QueueRules.MaxWaitingAgents > 0 &&
        FMath::IsFinite(Definition->Spec.QueueRules.SlotSpacingCm) &&
        Definition->Spec.QueueRules.SlotSpacingCm > 0.0f;
}

bool FFFZSFacilityDefinitionCustomization::CanSetCapacityFromQueueLength() const
{
    const UFFZSFacilityDefinition* Definition =
        Definitions.Num() == 1 ? Definitions[0].Get() : nullptr;
    return Definition &&
        Definition->Spec.QueueSplinePointsCm.Num() >= 2 &&
        FMath::IsFinite(Definition->Spec.QueueRules.SlotSpacingCm) &&
        Definition->Spec.QueueRules.SlotSpacingCm > 0.0f;
}

bool FFFZSFacilityDefinitionCustomization::CanFitVisualToFootprint() const
{
    const UFFZSFacilityDefinition* Definition =
        Definitions.Num() == 1 ? Definitions[0].Get() : nullptr;
    return Definition &&
        FFZSVisualGeometry::IsCardinalYawRotation(Definition->Spec.VisualRotation) &&
        (Definition->Spec.VisualSizingMode != EFFZSVisualSizingMode::MatchPlacementBounds ||
            Definition->Spec.VisualOffsetCm != FVector::ZeroVector);
}

FText FFFZSFacilityDefinitionCustomization::GetVisualSizingSummary() const
{
    return LOCTEXT(
        "VisualSizingAvailable",
        "Adjust the display using Visual Sizing Mode, Visual Offset, and Visual Rotation below. Source Mesh Size is for legacy exceptions; Uniform Fit preserves aspect ratio; Match Placement Bounds uses the footprint and obstacle height as the sole target dimensions.");
}

template <typename MutationType>
int32 FFFZSFacilityDefinitionCustomization::MutateDefinitions(
    const FText& TransactionDescription,
    MutationType&& Mutation)
{
    FScopedTransaction Transaction(TransactionDescription);
    if (Definitions.Num() != 1 || !Definitions[0].IsValid())
    {
        Transaction.Cancel();
        return 0;
    }
    int32 ChangedCount = 0;
    for (const TWeakObjectPtr<UFFZSFacilityDefinition>& WeakDefinition : Definitions)
    {
        UFFZSFacilityDefinition* Definition = WeakDefinition.Get();
        if (!Definition)
        {
            continue;
        }

        Definition->Modify(false);
        if (Mutation(*Definition))
        {
            ++ChangedCount;
        }
    }

    if (ChangedCount == 0)
    {
        Transaction.Cancel();
    }
    return ChangedCount;
}

void FFFZSFacilityDefinitionCustomization::ShowNotification(const FText& Message, const bool bSuccess)
{
    FNotificationInfo NotificationInfo(Message);
    NotificationInfo.ExpireDuration = 4.0f;
    NotificationInfo.bUseLargeFont = false;

    const TSharedPtr<SNotificationItem> Notification =
        FSlateNotificationManager::Get().AddNotification(NotificationInfo);
    if (Notification.IsValid())
    {
        Notification->SetCompletionState(
            bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
    }
}

#undef LOCTEXT_NAMESPACE
