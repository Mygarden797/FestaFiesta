// FF_FFZSFacilityDefinitionCustomization.cpp
// 역할: 시설 데이터 에셋의 상세 창을 편집 목적별로 정리하고 디자이너용 버튼과 미리보기를 제공한다.
#include "FF_FFZSFacilityDefinitionCustomization.h"
#include "FF_FFZSAudienceCapacityEditor.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityLevelPreview.h"
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
#include "Widgets/Input/SNumericEntryBox.h"
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

    // 목표 크기 입력은 언제나 현재 저장된 건물 크기에서 시작한다. 변경 전 크기가 보정량의 기준이므로
    // 담당자가 Footprint 필드를 직접 고치지 않고 이 입력만 바꾸도록 유도한다.
    PendingFootprintWidthCells.Reset();
    PendingFootprintDepthCells.Reset();
    PendingObstacleHeightCm.Reset();
    if (Definitions.Num() == 1 && Definitions[0].IsValid())
    {
        PendingFootprintWidthCells = Definitions[0]->Spec.FootprintSizeCells.X;
        PendingFootprintDepthCells = Definitions[0]->Spec.FootprintSizeCells.Y;
        PendingObstacleHeightCm = Definitions[0]->Spec.PlacementObstacleHeightCm;
    }

    FFZSFacilityDefinitionCustomizationPrivate::LocalizeCategory(
        DetailBuilder, TEXT("FF Zone System"), LOCTEXT("ZoneSystemCategory", "FF Zone System"),
        LOCTEXT("ZoneSystemCategoryTip", "시설의 단일 원천 사양과 카탈로그 표시 정보를 편집합니다."));
    FFZSFacilityDefinitionCustomizationPrivate::LocalizeCategory(
        DetailBuilder, TEXT("FF Zone System|Presentation"), LOCTEXT("PresentationCategory", "Presentation"),
        LOCTEXT("PresentationCategoryTip", "카탈로그처럼 게임 화면에 표시할 선택적 정보를 편집합니다."));
    FFZSFacilityDefinitionCustomizationPrivate::LocalizeCategory(
        DetailBuilder, TEXT("FF Zone System|Placement"), LOCTEXT("PlacementCategory", "Placement"),
        LOCTEXT("PlacementCategoryTip", "신규 시설의 건설 입력 방식을 설정합니다. 기존 인스턴스나 저장된 시설 사양은 변경하지 않습니다."));
    FFZSFacilityDefinitionCustomizationPrivate::LocalizeCategory(
        DetailBuilder, TEXT("FF Zone System|Authoring"), LOCTEXT("AuthoringCategory", "Authoring"),
        LOCTEXT("AuthoringCategoryTip", "에디터와 절차형 아트 도구 전용 소유권 정보를 편집합니다."));
    FFZSFacilityDefinitionCustomizationPrivate::LocalizeSpecCoordinateChildren(DetailBuilder);

    // Expose the captured Spec settings at the Data Asset's top level without a second copy.
    const TSharedRef<IPropertyHandle> SpecHandle = DetailBuilder.GetProperty(
        GET_MEMBER_NAME_CHECKED(UFFZSFacilityDefinition, Spec));
    IDetailCategoryBuilder& StaffCategory = DetailBuilder.EditCategory(
        TEXT("FF Zone System|Facility Staff"), LOCTEXT("FacilityStaffCategory", "Facility Staff"),
        ECategoryPriority::Important);
    StaffCategory.SetToolTip(LOCTEXT("FacilityStaffCategoryTip",
        "Info 안내 시설의 스태프 배정 정원입니다. 설치로 정원만 늘어나며 실제 인원은 별도 고용으로 추가합니다."));
    if (const TSharedPtr<IPropertyHandle> StaffCapacity = SpecHandle->GetChildHandle(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, StaffCapacity)))
    {
        DetailBuilder.HideProperty(StaffCapacity);
        StaffCategory.AddProperty(StaffCapacity.ToSharedRef());
    }
    IDetailCategoryBuilder& RevenueCategory = DetailBuilder.EditCategory(
        TEXT("FF Zone System|Facility Revenue"), LOCTEXT("FacilityRevenueCategory", "Facility Revenue"),
        ECategoryPriority::Important);
    RevenueCategory.SetToolTip(LOCTEXT("FacilityRevenueCategoryTip",
        "일반 수익 조정은 DT_FacilityRevenues에서 합니다. 아래 값은 표에 해당 시설 행이 없을 때 사용하는 호환 설정입니다."));
    for (const FName PropertyName : {
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, bOverrideRevenuePerUseGold),
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, RevenuePerUseGold) })
    {
        if (const TSharedPtr<IPropertyHandle> RevenueProperty = SpecHandle->GetChildHandle(PropertyName))
        {
            DetailBuilder.HideProperty(RevenueProperty);
            RevenueCategory.AddProperty(RevenueProperty.ToSharedRef());
        }
    }
    RevenueCategory.AddCustomRow(LOCTEXT("FacilityRevenueHelpSearch", "Revenue 매출 수익 이용 금액"))
    .WholeRowContent()
    [
        SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("FacilityRevenueHelp",
            "시설별 수익은 Content/FestaFiesta/Data/Economy/DT_FacilityRevenues에서 수정하세요.\n"
            "Row Name이 이 시설의 Facility Definition ID인 행의 '1인 1회 매출 (Gold)'을 바꾸고 저장합니다. 0은 무료입니다.\n"
            "표의 금액은 아래 값보다 우선하며 기존 설치·저장 시설에도 다음 정산부터 적용됩니다. 축제 수수료·강화·시너지 배율은 별도로 계산합니다.\n"
            "표에 해당 행이 없을 때만 아래 설정을 사용합니다. 이 호환 설정의 변경은 새로 설치하는 시설부터 적용됩니다."))
    ];
    const TSharedPtr<IPropertyHandle> AudienceHandle = SpecHandle->GetChildHandle(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Audience));
    IDetailCategoryBuilder& AudienceCategory = DetailBuilder.EditCategory(
        TEXT("FF Zone System|Facility Audience"), LOCTEXT("AudienceCategory", "Facility Audience"), ECategoryPriority::Important);
    AudienceCategory.SetToolTip(LOCTEXT("AudienceCategoryTip", "공연 관람 공간과 사람별 자리를 설정합니다. 설정은 Spec 안에 한 번만 저장되며 Queue 정원과 별개입니다."));
    if (AudienceHandle)
    {
        DetailBuilder.HideProperty(AudienceHandle);
        AudienceCategory.AddProperty(AudienceHandle.ToSharedRef());
    }
    AudienceCategory.AddCustomRow(LOCTEXT("AudienceCapacitySearch", "Audience Capacity 관람 정원"))
    .WholeRowContent()
    [
        SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]
        {
            if (Definitions.Num() != 1 || !Definitions[0].IsValid())
                return LOCTEXT("AudienceSelectOne", "시설 하나를 선택하면 관람 정원을 표시합니다.");
            const FFFZSAudienceLayout& Audience = Definitions[0]->Spec.Audience;
            return FText::Format(LOCTEXT("AudienceCapacity", "관람 정원: {0}명 · 미리보기: Preview Facility Levels → Refresh Preview\n초록: 관람 자리/예약 영역 · 파랑: 입구 · 주황: 출구 · 흰색: 무대 방향"), FText::AsNumber(Audience.GetCapacity()));
        })
    ];
    AudienceCategory.AddCustomRow(LOCTEXT("AudienceAutoStatusSearch", "Audience Auto Validation 자동 보정 검증 결과 면적"))
    .WholeRowContent()
    [
        SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]
        {
            return FFFZSAudienceCapacityEditor::GetStatus(Definitions.Num() == 1 ? Definitions[0].Get() : nullptr);
        })
    ];
    AudienceCategory.AddCustomRow(LOCTEXT("RetryAudienceSearch", "Apply Requested Audience Values 보관한 요청 인원 다시 적용"))
    .WholeRowContent()
    [
        SNew(SButton).Text(LOCTEXT("RetryAudienceButton", "Apply Requested Audience Values"))
        .ToolTipText(LOCTEXT("RetryAudienceTip", "전체 검증을 막던 오류를 수정한 뒤 누르세요. 보관한 인원을 다시 입력하지 않고 현재 시설 설정으로 재계산해 적용합니다. 요청은 현재 에디터 세션에 보관됩니다."))
        .IsEnabled_Lambda([this] { return Definitions.Num() == 1 && FFFZSAudienceCapacityEditor::CanRetry(Definitions[0].Get()); })
        .OnClicked_Lambda([this]
        {
            if (Definitions.Num() == 1 && Definitions[0].IsValid()) FFFZSAudienceCapacityEditor::Retry(*Definitions[0]);
            return FReply::Handled();
        })
    ];
    AudienceCategory.AddCustomRow(LOCTEXT("ArrangeAudienceSearch", "Arrange Audience 관람 자리 입구 출구 자동 정렬"))
    .WholeRowContent()
    [
        SNew(SButton).Text(LOCTEXT("ArrangeAudienceButton", "Arrange Audience Layout"))
        .ToolTipText(LOCTEXT("ArrangeAudienceTip", "행·열 변경은 자동 보정됩니다. 첫 줄 위치나 간격을 수동 조절한 뒤 이 버튼으로 가운데 정렬과 입퇴장 위치를 다시 맞출 수 있습니다. Ctrl+Z로 되돌릴 수 있습니다."))
        .IsEnabled_Lambda([this] { return Definitions.Num() == 1 && Definitions[0].IsValid() && Definitions[0]->Spec.Audience.bEnabled; })
        .OnClicked(this, &FFFZSFacilityDefinitionCustomization::ArrangeAudienceLayout)
    ];
    const TSharedPtr<IPropertyHandle> UsesLevels = SpecHandle->GetChildHandle(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, bUsesFacilityLevels));
    const TSharedPtr<IPropertyHandle> UpgradeTiers = SpecHandle->GetChildHandle(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, UpgradeTiers));
    IDetailCategoryBuilder& UpgradeCategory = DetailBuilder.EditCategory(
        TEXT("FF Zone System|Facility Upgrade"), LOCTEXT("FacilityUpgradeCategory", "Facility Upgrade"),
        ECategoryPriority::Important);
    UpgradeCategory.SetToolTip(LOCTEXT("FacilityUpgradeTip",
        "이 시설만의 레벨 사용 여부와 단계별 비용·수익·속도·외형·실제 건물 크기를 설정합니다. 레벨을 끄면 UI에 레벨과 강화 버튼이 표시되지 않습니다."));
    if (UsesLevels && UpgradeTiers)
    {
        DetailBuilder.HideProperty(UsesLevels);
        DetailBuilder.HideProperty(UpgradeTiers);
        UpgradeCategory.AddProperty(UsesLevels.ToSharedRef());
        UpgradeCategory.AddProperty(UpgradeTiers.ToSharedRef());
    }
    UpgradeCategory.AddCustomRow(LOCTEXT("PreviewLevelsSearch", "Preview Levels 강화 레벨 외형 크기 미리보기"))
    .WholeRowContent()
    [
        SNew(SButton)
        .Text(LOCTEXT("PreviewLevelsButton", "Preview Facility Levels"))
        .ToolTipText(LOCTEXT("PreviewLevelsTooltip",
            "별도 미리보기 창에서 레벨별 메시와 크기를 비교합니다. 에셋과 현재 작업 중인 맵은 변경하지 않습니다. 레벨 메시가 비어 있으면 언제나 Lv.1 기본 메시를 사용합니다."))
        .IsEnabled_Lambda([this] { return Definitions.Num() == 1 && Definitions[0].IsValid(); })
        .OnClicked(this, &FFFZSFacilityDefinitionCustomization::PreviewFacilityLevels)
    ];

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

    ToolsCategory.AddCustomRow(LOCTEXT("ResizeFootprintSearchText", "건물 크기 변경 대기열 앵커 보정 Resize Footprint Keep Queue Anchors"))
    .WholeRowContent()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .MinDesiredWidth(120.0f)
            [
                SNew(SNumericEntryBox<int32>)
                .MinValue(1)
                .ToolTipText(LOCTEXT(
                    "ResizeFootprintWidthTooltip",
                    "보정 후 적용할 회전 전 건물 가로 폭(cm)입니다. 1 단위가 1cm이므로 6m는 600입니다."))
                .Value(this, &FFFZSFacilityDefinitionCustomization::GetPendingFootprintWidthCells)
                .OnValueChanged(this, &FFFZSFacilityDefinitionCustomization::SetPendingFootprintWidthCells)
                .Label()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ResizeFootprintWidthLabel", "New Width (X)"))
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
        [
            SNew(SBox)
            .MinDesiredWidth(120.0f)
            [
                SNew(SNumericEntryBox<int32>)
                .MinValue(1)
                .ToolTipText(LOCTEXT(
                    "ResizeFootprintDepthTooltip",
                    "보정 후 적용할 회전 전 건물 세로 깊이(cm)입니다. 1 단위가 1cm이므로 3m는 300입니다."))
                .Value(this, &FFFZSFacilityDefinitionCustomization::GetPendingFootprintDepthCells)
                .OnValueChanged(this, &FFFZSFacilityDefinitionCustomization::SetPendingFootprintDepthCells)
                .Label()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ResizeFootprintDepthLabel", "New Depth (Y)"))
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
        [
            SNew(SBox)
            .MinDesiredWidth(120.0f)
            [
                SNew(SNumericEntryBox<float>)
                .MinValue(10.0f)
                .ToolTipText(LOCTEXT(
                    "ResizeFootprintHeightTooltip",
                    "보정 후 적용할 배치 장애물 높이(cm)입니다. 최소 10cm이며, 배치 충돌·보호 영역 검사와 '배치 경계에 정확히 맞춤' 모드의 표시 메시 높이에 사용됩니다. 높이는 대기열·앵커 위치에 영향을 주지 않습니다."))
                .Value(this, &FFFZSFacilityDefinitionCustomization::GetPendingObstacleHeightCm)
                .OnValueChanged(this, &FFFZSFacilityDefinitionCustomization::SetPendingObstacleHeightCm)
                .Label()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ResizeFootprintHeightLabel", "New Height (Z)"))
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
        [
            SNew(SButton)
            .Text(LOCTEXT("ResizeFootprintButton", "Resize Facility (Keep Queue & Anchors)"))
            .ToolTipText(LOCTEXT(
                "ResizeFootprintTooltip",
                "시설 Data Asset 하나만 선택해야 합니다. 위에 입력한 가로·깊이·높이를 한 번에 적용하면서, 대기열 예약 사각형·대기열 경로 점·앵커가 각자 붙어 있던 건물 변까지의 거리를 유지하도록 함께 이동합니다. 예약 사각형은 건물 좌하단 기준이라 중심보다 바깥쪽 경계를 크기 변화량만큼, 경로 점과 앵커는 건물 중심 기준이라 변화량의 절반만큼 이동합니다. 높이는 XY 배치에 영향을 주지 않으므로 값만 교체합니다. 결과 Spec 전체가 검증을 통과할 때만 적용하며, 실패하면 아무 값도 바꾸지 않고 원인을 보여 줍니다. 구버전 Legacy Queue Reserved Cells가 남아 있으면 사용할 수 없습니다."))
            .IsEnabled(this, &FFFZSFacilityDefinitionCustomization::CanResizeFacilityKeepingLayout)
            .OnClicked(this, &FFFZSFacilityDefinitionCustomization::ResizeFacilityKeepingLayout)
        ]
    ];

    ToolsCategory.AddCustomRow(LOCTEXT("QueueRectsSearchText", "대기열 예약 영역 여백 회랑 굴절 Queue Reserved Rects Margin Corridor Bend"))
    .WholeRowContent()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .MinDesiredWidth(120.0f)
            [
                SNew(SNumericEntryBox<int32>)
                .MinValue(1)
                .ToolTipText(LOCTEXT(
                    "QueueMarginTooltip",
                    "대기열 경로 양옆에 확보할 여백(cm)입니다. 손님이 실제로 차지하는 폭을 고려해 정하십시오. 0은 폭이 없는 예약 영역이 되므로 사용할 수 없습니다."))
                .Value(this, &FFFZSFacilityDefinitionCustomization::GetPendingQueueMarginCm)
                .OnValueChanged(this, &FFFZSFacilityDefinitionCustomization::SetPendingQueueMarginCm)
                .Label()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("QueueMarginLabel", "Queue Margin (cm)"))
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
        [
            SNew(SButton)
            .Text(LOCTEXT("QueueRectsFromPathButton", "Queue Rects <- Queue Path"))
            .ToolTipText(LOCTEXT(
                "QueueRectsFromPathTooltip",
                "시설 Data Asset 하나만 선택해야 합니다. 최대 대기 팀 수이 실제로 사용하는 경로 구간만 여백만큼 넓혀 예약 영역을 다시 만듭니다. 경로가 꺾여 있으면 구간마다 사각형을 만들어 회랑처럼 따라가므로 L자·ㄷ자 대기열도 남는 면적 없이 확보합니다. 건물 한 변 바깥을 지나는 구간은 그 변에 붙여 잘라내며, 저작 경로가 필요한 길이보다 길면 남는 꼬리는 예약하지 않습니다. 결과 Spec 전체가 검증을 통과할 때만 적용합니다."))
            .IsEnabled(this, &FFFZSFacilityDefinitionCustomization::CanBuildQueueRectsFromPath)
            .OnClicked(this, &FFFZSFacilityDefinitionCustomization::BuildQueueRectsFromPath)
        ]
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
                "시설 Data Asset 하나만 선택해야 합니다. 점이 2개 이상이고 슬롯 간격이 양수인 대기열에서 마지막 선분 방향으로 꼬리 점을 이동하거나 새 꼬리 점을 추가해 전체 길이를 (최대 대기 팀 수 - 1) × 슬롯 간격으로 맞춥니다. 생성되는 모든 슬롯이 대기열 예약 사각형 안에 남을 때만 적용합니다."))
            .IsEnabled(this, &FFFZSFacilityDefinitionCustomization::CanResizeQueueTailToCapacity)
            .OnClicked(this, &FFFZSFacilityDefinitionCustomization::ResizeQueueTailToCapacity)
        ]
        + SUniformGridPanel::Slot(2, 0)
        [
            SNew(SButton)
            .Text(LOCTEXT("CapacityFromLengthButton", "Max Capacity <- Queue Length"))
            .ToolTipText(LOCTEXT(
                "CapacityFromLengthTooltip",
                "시설 Data Asset 하나만 선택해야 합니다. 점이 2개 이상이고 슬롯 간격이 양수인 현재 대기열에서 들어갈 수 있는 최대 대기 팀 수을 계산합니다. 줄 머리의 0번 슬롯을 포함해 floor(길이 ÷ 간격) + 1이며, 결과 슬롯이 모두 예약 영역 안에 있을 때만 적용합니다."))
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
            "[공연 관람 공간]\n"
            "Facility Audience의 Audience Layout을 펼치고 Enable Audience Area를 켭니다. 총 정원은 Columns × Rows이며 사람 수입니다. 행·열을 확정하면 자리·입퇴장 자동 보정 후 전체 검증합니다.\n"
            "면적 부족 시 모든 사용 레벨에 필요한 최소 관람 면적을 자동 계산해 늘립니다. 보정 이유와 변경 전후 값이 표시됩니다.\n"
            "다른 시설 오류 때문에 적용할 수 없으면 요청 인원을 보관합니다. 해당 오류를 고친 뒤 Apply Requested Audience Values를 누르세요.\n"
            "First Seat는 건물 중심 기준 첫 줄 왼쪽 자리입니다. 뒤쪽 줄은 +Y 방향으로 늘어납니다. 입구와 출구는 관람 영역 가장자리 통로에 놓습니다.\n"
            "Preview Facility Levels에서 초록 자리·파랑 입구·주황 출구를 확인하고, 수정 후 Refresh Preview를 누릅니다. 실제 맵의 NavMesh와 NPC 이동은 별도 확인이 필요합니다.\n"
            "PDA 변경은 새로 설치하는 시설부터 적용됩니다. 기존 저장 시설은 원래 사양을 보존합니다. NPC가 모이는 시각은 시간/NPC 담당자가 연결합니다.\n\n"
            "[시설 강화]\n"
            "Facility Upgrade에서 Use Facility Levels를 켜고 Upgrade Tiers에 단계를 추가합니다. Target Level은 2 이상의 서로 다른 번호로 작성합니다. 번호 누락과 배열 순서를 허용하며 게임은 현재보다 높은 가장 가까운 레벨로 진행합니다. 끄면 레벨 없는 시설입니다.\n"
            "각 단계의 Cost Multiplier는 설치비 배율, Service Duration Multiplier는 원래 서비스 시간 배율, Size Multiplier는 원래 건물의 가로·깊이·높이에 대한 절대 배율입니다.\n"
            "Revenue Multiplier는 Lv.1 수익에 곱하는 배율입니다. 매 강화마다 1.5배라면 Lv.2에 1.5, Lv.3에 2.25를 입력합니다. 실제 이용 완료 정산과 시설 정보의 1명당 예상 수익에 함께 적용됩니다.\n"
            "레벨 수와 배율의 기획상 상한은 없습니다. 비용·수익은 0 이상, 시간·크기는 0보다 크게 작성합니다. 이전 값 유지·감소·증가와 외형만 바꾸는 단계도 허용합니다.\n"
            "실제 실행에는 계산 가능한 숫자, 유효한 건물·대기 공간과 길찾기가 필요합니다. 크기 축소도 이 검사를 통과해야 적용됩니다.\n"
            "단계의 Visual Mesh가 비어 있으면 항상 Lv.1 기본 Visual Mesh를 사용합니다. 커질 공간이 없으면 실제 게임의 강화는 결제 전에 거절됩니다.\n"
            "Preview Facility Levels 창에서 레벨을 바꿔 외형과 크기를 확인할 수 있습니다. 데이터 수정 후 Refresh Preview를 누르십시오.\n\n"
            "[건물 크기]\n"
            "Footprint Size (cm)는 회전 전 바닥 크기이며 1단위는 1cm입니다. 높이는 Placement Obstacle Height (cm)에서 바꿉니다.\n"
            "Visual Sizing Mode는 프로덕션에서 Match Placement Bounds를 사용하고 Visual Offset은 0을 유지하십시오.\n\n"
            "[대기열]\n"
            "Queue Reserved Rects는 건물 왼쪽 아래 기준의 예약 영역이고, Queue Spline Points와 Anchor는 건물 중심 기준 cm 좌표입니다.\n"
            "필요 경로 길이는 (Max Waiting Agents - 1) × Slot Spacing입니다. 모든 슬롯이 예약 사각형 안에 있어야 합니다.\n"
            "줄이 너무 길면 인원이나 간격을 줄이거나, 경로에 굴절점을 넣어 L자·ㄷ자로 접으십시오. 접어도 길이는 그대로라 정원은 유지됩니다.\n"
            "인원·간격·경로를 고친 뒤 Queue Rects <- Queue Path를 누르면 예약 영역이 새 경로를 따라가는 회랑으로 다시 만들어집니다.\n\n"
            "[크기를 바꿀 때]\n"
            "예약 사각형은 건물 좌하단 기준, 대기열 경로 점과 앵커는 건물 중심 기준이라 같은 크기 변경에도 필요한 이동량이 다릅니다.\n"
            "New Width/Depth/Height에 목표 값을 넣고 Resize Facility 버튼을 쓰면 세 값이 각자 붙어 있던 변까지의 거리를 유지한 채 함께 이동합니다.\n"
            "높이는 XY 배치에 영향을 주지 않으므로 같은 버튼에서 값만 교체되며, 최소 10cm입니다.\n"
            "Footprint 필드를 직접 고치면 이 보정이 일어나지 않아 예약 영역 겹침이나 건물 안쪽 앵커가 생길 수 있습니다.\n\n"
            "[버튼]\n"
            "Footprint <- Loaded Mesh: 로드된 원본 메시에서 건물 크기를 계산합니다.\n"
            "Resize Facility (Keep Queue & Anchors): 새 가로·깊이·높이를 적용하고 예약 영역·경로 점·앵커를 함께 보정합니다.\n"
            "Queue Rects <- Queue Path: 현재 경로와 여백에 맞는 예약 회랑을 다시 만듭니다.\n"
            "Queue Tail <- Max Capacity: 인원과 간격에 맞춰 경로 꼬리를 조정합니다.\n"
            "Max Capacity <- Queue Length: 현재 경로 길이에서 최대 인원을 계산합니다.\n"
            "Fit Mesh to Placement Bounds: 메시 표시를 권위 건물 크기와 높이에 맞춥니다.\n"
            "Validate Facility Definition: 내부 규칙·ID 중복·메시 참조까지 전체 검증합니다.\n\n"
            "항상 보이는 명칭은 영어이며, 각 항목에 마우스를 올리면 더 자세한 한국어 설명을 확인할 수 있습니다."),
        LOCTEXT("KoreanAuthoringHelpTitle", "시설 저작 한국어 도움말"));
    return FReply::Handled();
}

FReply FFFZSFacilityDefinitionCustomization::PreviewFacilityLevels()
{
    if (Definitions.Num() == 1)
    {
        OpenFFZSFacilityLevelPreview(Definitions[0].Get());
    }
    return FReply::Handled();
}

FReply FFFZSFacilityDefinitionCustomization::ArrangeAudienceLayout()
{
    TArray<FText> Errors;
    const FProperty* Property = FFZSFacilityDefinitionCustomizationPrivate::FindSpecProperty(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Audience));
    const int32 Changed = MutateDefinitions(LOCTEXT("ArrangeAudienceTransaction", "관람 자리와 입퇴장 위치 정렬"),
        [&](UFFZSFacilityDefinition& Definition)
        {
            FFFZSFacilitySpec Candidate = Definition.Spec;
            if (!Candidate.Audience.TryCenterAndArrangePortals()) return false;
            if (!Candidate.IsValid(Errors)) return false;
            Definition.Spec = MoveTemp(Candidate);
            FFZSFacilityDefinitionCustomizationPrivate::NotifyPropertyChanged(Definition, Property);
            return true;
        });
    ShowNotification(Changed > 0 ? LOCTEXT("AudienceArranged", "관람 자리를 가운데로 정렬하고 입퇴장 위치를 맞췄습니다. 미리보기를 새로고침하세요.")
        : Errors.IsEmpty() ? LOCTEXT("AudienceArrangeFailed", "행·열·간격·가장자리 여백을 확인하세요. 전체 검증을 통과할 때만 정렬됩니다.") : Errors[0], Changed > 0);
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

FReply FFFZSFacilityDefinitionCustomization::ResizeFacilityKeepingLayout()
{
    TArray<const FProperty*> ChangedProperties;
    for (const FName PropertyName : {
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, FootprintSizeCells),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, PlacementObstacleHeightCm),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueReservedRects),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueSplinePointsCm),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Audience),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Anchors) })
    {
        ChangedProperties.Add(FFZSFacilityDefinitionCustomizationPrivate::FindSpecProperty(PropertyName));
    }

    const FIntPoint NewFootprint(
        PendingFootprintWidthCells.Get(0),
        PendingFootprintDepthCells.Get(0));
    const float NewHeightCm = PendingObstacleHeightCm.Get(0.0f);
    TArray<FText> FailureReasons;
    const int32 ChangedCount = MutateDefinitions(
        LOCTEXT("ResizeFootprintTransaction", "건물 크기·높이 변경에 맞춰 대기열·앵커 보정"),
        [&ChangedProperties, NewFootprint, NewHeightCm, &FailureReasons](UFFZSFacilityDefinition& Definition)
        {
            // 높이는 XY 배치에 영향을 주지 않으므로 보정 없이 값만 교체하고,
            // 크기가 그대로여도 높이만 바뀌었다면 한 번의 조작으로 함께 적용한다.
            FFFZSFacilitySpec Candidate;
            const bool bHeightChanged = !FMath::IsNearlyEqual(
                NewHeightCm, Definition.Spec.PlacementObstacleHeightCm);
            if ((NewFootprint == Definition.Spec.FootprintSizeCells && !bHeightChanged) ||
                !FFFZSFacilityEditorHelpers::TryResizeFootprintKeepingLayout(
                    Definition.Spec,
                    NewFootprint,
                    Candidate))
            {
                FailureReasons.Add(LOCTEXT(
                    "ResizeFootprintNotExpressible",
                    "새 가로·깊이·높이가 모두 현재 값과 같거나, 좌표를 표현할 수 없거나, 구버전 Legacy Queue Reserved Cells가 남아 있어 자동 보정을 계산할 수 없습니다."));
                return false;
            }
            Candidate.PlacementObstacleHeightCm = NewHeightCm;
            // 보정 결과가 사양 전체 검증을 통과할 때만 적용한다. 실패한 이유는 그대로 담당자에게 보여 준다.
            if (!Candidate.IsValid(FailureReasons))
            {
                return false;
            }

            Definition.Spec = MoveTemp(Candidate);
            for (const FProperty* ChangedProperty : ChangedProperties)
            {
                if (ChangedProperty)
                {
                    FFZSFacilityDefinitionCustomizationPrivate::NotifyPropertyChanged(Definition, ChangedProperty);
                }
            }
            return true;
        });

    ShowNotification(
        ChangedCount > 0
            ? FText::Format(
                LOCTEXT("FootprintResized", "건물 크기를 {0} x {1}, 높이를 {2}cm로 바꾸고 대기열·앵커를 함께 보정했습니다."),
                NewFootprint.X,
                NewFootprint.Y,
                FText::AsNumber(NewHeightCm))
            : LOCTEXT("FootprintResizeFailed", "건물 크기 보정에 실패했습니다. 상세 내용을 확인하십시오."),
        ChangedCount > 0);
    if (ChangedCount == 0 && !FailureReasons.IsEmpty())
    {
        FString Report;
        for (const FText& Reason : FailureReasons)
        {
            Report += FString::Printf(TEXT("  - %s\n"), *Reason.ToString());
        }
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(Report),
            LOCTEXT("ResizeFootprintFailedTitle", "건물 크기 보정: 수정 필요"));
    }
    return FReply::Handled();
}

FReply FFFZSFacilityDefinitionCustomization::BuildQueueRectsFromPath()
{
    const FProperty* ChangedProperty = FFZSFacilityDefinitionCustomizationPrivate::FindSpecProperty(
        GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueReservedRects));
    const int32 MarginCm = PendingQueueMarginCm.Get(0);
    TArray<FText> FailureReasons;
    const int32 ChangedCount = MutateDefinitions(
        LOCTEXT("QueueRectsFromPathTransaction", "대기열 경로에 맞춰 예약 영역 재생성"),
        [ChangedProperty, MarginCm, &FailureReasons](UFFZSFacilityDefinition& Definition)
        {
            TArray<FFFZSCellRect> Rects;
            if (!FFFZSFacilityEditorHelpers::TryBuildQueueRectsFromPath(
                    Definition.Spec, MarginCm, Rects))
            {
                FailureReasons.Add(LOCTEXT(
                    "QueueRectsNotExpressible",
                    "여백이 0 이하이거나, 대기열 경로·정원·간격이 유효하지 않거나, 구버전 Legacy Queue Reserved Cells가 남아 있어 예약 회랑을 계산할 수 없습니다."));
                return false;
            }
            if (Rects == Definition.Spec.QueueReservedRects)
            {
                FailureReasons.Add(LOCTEXT(
                    "QueueRectsAlreadyMinimal",
                    "예약 영역이 이미 현재 경로와 여백에 맞는 회랑입니다."));
                return false;
            }

            FFFZSFacilitySpec Candidate = Definition.Spec;
            Candidate.QueueReservedRects = MoveTemp(Rects);
            if (!Candidate.IsValid(FailureReasons))
            {
                return false;
            }

            Definition.Spec = MoveTemp(Candidate);
            FFZSFacilityDefinitionCustomizationPrivate::NotifyPropertyChanged(Definition, ChangedProperty);
            return true;
        });

    ShowNotification(
        ChangedCount > 0
            ? LOCTEXT("QueueRectsUpdated", "대기열 예약 영역을 현재 경로에 맞는 회랑으로 다시 만들었습니다.")
            : LOCTEXT("QueueRectsFailed", "대기열 예약 영역 재생성에 실패했습니다. 상세 내용을 확인하십시오."),
        ChangedCount > 0);
    if (ChangedCount == 0 && !FailureReasons.IsEmpty())
    {
        FString Report;
        for (const FText& Reason : FailureReasons)
        {
            Report += FString::Printf(TEXT("  - %s\n"), *Reason.ToString());
        }
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(Report),
            LOCTEXT("QueueRectsFailedTitle", "대기열 예약 영역: 수정 필요"));
    }
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
        FText::Format(LOCTEXT("CapacityUpdated", "시설 정의 {0}개의 최대 대기 팀 수을 갱신했습니다."), ChangedCount),
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

bool FFFZSFacilityDefinitionCustomization::CanResizeFacilityKeepingLayout() const
{
    const UFFZSFacilityDefinition* Definition =
        Definitions.Num() == 1 ? Definitions[0].Get() : nullptr;
    if (!Definition ||
        !Definition->Spec.QueueReservedCells.IsEmpty() ||
        PendingFootprintWidthCells.Get(0) <= 0 ||
        PendingFootprintDepthCells.Get(0) <= 0 ||
        PendingObstacleHeightCm.Get(0.0f) < 10.0f)
    {
        return false;
    }

    // 가로·깊이·높이 중 하나라도 현재 값과 다르면 적용할 변경이 있다.
    return FIntPoint(PendingFootprintWidthCells.Get(0), PendingFootprintDepthCells.Get(0)) !=
            Definition->Spec.FootprintSizeCells ||
        !FMath::IsNearlyEqual(
            PendingObstacleHeightCm.Get(0.0f), Definition->Spec.PlacementObstacleHeightCm);
}

bool FFFZSFacilityDefinitionCustomization::CanBuildQueueRectsFromPath() const
{
    const UFFZSFacilityDefinition* Definition =
        Definitions.Num() == 1 ? Definitions[0].Get() : nullptr;
    return Definition &&
        Definition->Spec.QueueReservedCells.IsEmpty() &&
        Definition->Spec.QueueSplinePointsCm.Num() >= 2 &&
        Definition->Spec.QueueRules.MaxWaitingAgents > 0 &&
        FMath::IsFinite(Definition->Spec.QueueRules.SlotSpacingCm) &&
        Definition->Spec.QueueRules.SlotSpacingCm > 0.0f &&
        PendingQueueMarginCm.Get(0) > 0;
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
