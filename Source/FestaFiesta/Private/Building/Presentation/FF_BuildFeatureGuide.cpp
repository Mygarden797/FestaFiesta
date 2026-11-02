#include "Building/Presentation/FF_BuildFeatureGuide.h"

#define LOCTEXT_NAMESPACE "FFBuildFeatureGuide"

const TArray<FFBuildFeatureGuide::FTopic>& FFBuildFeatureGuide::Topics()
{
    static const TArray<FTopic> Entries = {
        {TEXT("MultiSelection"), LOCTEXT("MultiTitle", "다중 선택"), LOCTEXT("MultiBody",
            "여러 시설을 한 번에 선택하고 함께 이동하거나 철거할 수 있습니다.\n\n다중 선택을 켠 뒤 시설을 클릭하거나 바닥을 드래그해 선택하세요. 선택한 시설과 사용할 작업은 다중 선택 패널에서 확인할 수 있습니다.\n\n다중 선택 버튼을 다시 누르면 모드를 종료합니다."), TEXT("Button_MultiSelect"), TEXT("Size_BuildToolbarLine1")},
        {TEXT("BuildGrid"), LOCTEXT("GridTitle", "격자"), LOCTEXT("GridBody",
            "바닥의 격자를 켜거나 끕니다. 시설을 나란히 배치하고 방향을 확인할 때 사용하세요.\n\n격자의 표시 여부와 시설 위치의 맞춤 간격은 별개입니다. 위치가 움직이는 단위는 상단의 간격 버튼에서 바꿀 수 있습니다."), TEXT("Button_Grid"), TEXT("Size_BuildToolbarLine2")},
        {TEXT("BuildSnap"), LOCTEXT("SnapTitle", "간격"), LOCTEXT("SnapBody",
            "시설의 위치를 맞추는 간격을 정합니다. 버튼을 누를 때마다 100cm → 25cm → 1cm 순서로 바뀝니다.\n\n100cm는 일정한 배치, 25cm는 정밀 조정, 1cm는 자유로운 배치에 사용하세요. 건축 설정에서도 원하는 간격을 직접 선택할 수 있습니다."), TEXT("Button_SnapCycle"), TEXT("Size_BuildToolbarLine3")},
        {TEXT("FacilityRanges"), LOCTEXT("RangesTitle", "시설 범위"), LOCTEXT("RangesBody",
            "설치한 시설의 범위 표시를 켜거나 끕니다. 주변 시설과의 위치 관계를 확인하며 배치할 때 사용하세요.\n\n버튼을 다시 누르면 범위를 숨깁니다. 새 시설의 배치 가능 여부와 필요한 공간은 배치 안내에서 함께 확인하세요."), TEXT("Button_ToolbarRanges"), TEXT("Size_BuildToolbarLine4")},
        {TEXT("BuildSettings"), LOCTEXT("SettingsTitle", "건축 설정"), LOCTEXT("SettingsBody",
            "격자, 위치 간격, 시설 범위 등 건축에 필요한 설정을 한곳에서 조정합니다.\n\n배치 중에는 직접 조정과 회전·이동 도구를 사용할 수 있습니다. 커서 중심 확대와 화면 가장자리 이동도 취향에 맞게 켜거나 끄세요.\n\n설정을 마쳤으면 닫기를 누르거나 건축 설정 버튼을 다시 누르세요."), TEXT("Button_BuildSettings"), TEXT("Size_BuildToolbarLine5")},
        {TEXT("BuildCatalog"), LOCTEXT("CatalogTitle", "시설 목록"), LOCTEXT("CatalogBody",
            "설치할 시설을 고르는 카탈로그를 엽니다. 분류나 검색으로 시설을 찾고 이름·가격을 확인한 뒤 카드를 선택하세요. 목록을 유지한 채 오른쪽 시설 설치 패널에서 비용·크기·시설 조합을 확인할 수 있습니다.\n\n아이템 필터에서 필수 시설·배치 가능·프로그램 지원·조명 조건을 함께 선택할 수 있습니다. 배치 가능은 보유 금화와 설치 수 제한을 기준으로 합니다. 추천순·이름순·가격순으로 정렬하고, 초기화로 전체 목록을 다시 볼 수 있습니다.\n\n목록은 위아래로 드래그하거나 휠·스크롤바로 더 볼 수 있습니다. 카드의 별은 즐겨찾기입니다. 빨간 별은 필수 시설, 전구는 조명, 보라색 아이콘은 프로그램 지원을 뜻합니다."), TEXT("Button_ToolbarCatalog"), TEXT("Size_BuildToolbarLine6")},
        {TEXT("BuildControls"), LOCTEXT("ControlsTitle", "조작 안내"), LOCTEXT("ControlsBody",
            "좌클릭 · 시설 선택 / 설치\n우클릭 · 취소 / 선택 해제\nQ / E · 시설 회전\nB · 시설 목록\nCtrl + Z · 실행 취소\nW A S D · 카메라 이동\n\n시설 목록 위에는 자주 쓰는 여섯 가지 조작을 표시합니다. 실제 키 표시는 현재 키 설정을 따릅니다.\n\n연속 설치 시설은 누른 채 드래그하면 일정 간격으로 미리보기가 나타나고, 마우스를 놓으면 가능한 곳에 설치됩니다. Ctrl+Z로 설치한 묶음을 취소할 수 있습니다.\n\n대기열 설정은 설치된 시설을 선택한 뒤 시설 정보의 대기열 탭에서 변경하세요."), TEXT("Button_ToolbarHelp"), TEXT("Size_BuildToolbarLine7")},
        {TEXT("LightingControls"), LOCTEXT("LightingTitle", "시설 조명"), LOCTEXT("LightingBody",
            "카탈로그의 전구 아이콘은 조명을 조절할 수 있는 시설을 뜻합니다.\n\n해당 시설을 설치한 뒤 선택하고 시설 정보의 조명 설정을 여세요. 조명을 켜거나 끄고 밝기와 색상을 바꿀 수 있습니다.\n\n변경한 모습을 확인한 뒤 적용으로 저장하세요. 취소로 되돌리거나 원래 색상을 다시 사용할 수 있습니다."), NAME_None},
        {TEXT("FacilityCodex"), LOCTEXT("CodexTitle", "조합 도감"), LOCTEXT("CodexBody",
            "시설을 어떤 조합으로 배치하면 효과를 얻는지 확인하는 도감입니다. 조합 도감에서 필요한 시설과 거리 조건, 발견 여부를 확인하세요.\n\n조건에 맞게 시설을 설치하면 조합이 발견됩니다. 적용 중인 효과를 확인하고 받을 수 있는 발견 보상과 수집 보상도 챙기세요."), NAME_None},
        {TEXT("FacilityRevenueRanking"), LOCTEXT("RevenueTitle", "매출 순위"), LOCTEXT("RevenueBody",
            "현재 설치된 수익 시설의 매출을 비교합니다. 매출 순위를 열고 오늘 또는 축제 누적 기준을 선택하세요.\n\n시설별 순위, 매출과 전체 매출에서 차지하는 비중을 확인할 수 있습니다. 목록에서 시설을 선택한 뒤 해당 시설로 이동해 운영 상태를 살펴보세요."), NAME_None},
        {TEXT("BuildFavorites"), LOCTEXT("FavoritesTitle", "즐겨찾기"), LOCTEXT("FavoritesBody",
            "카탈로그에서 자주 쓰는 시설의 별표를 눌러 즐겨찾기에 저장하세요. 별표를 다시 누르면 해제됩니다.\n\n즐겨찾기 필터를 켜면 저장한 시설만 모아 볼 수 있습니다. 즐겨찾기는 개인 설정에 저장되어 다음 실행에서도 유지됩니다."), NAME_None},
        // Informational topics have no ToolbarButton: viewing these must not
        // change the existing first-use behavior of the seven action buttons.
        {TEXT("FacilitySelection"), LOCTEXT("SelectionTitle", "시설 선택·정보"), LOCTEXT("SelectionBody",
            "설치된 시설을 선택하면 해당 시설의 정보와 사용할 수 있는 작업을 확인할 수 있습니다.\n\n시설의 종류와 축제 상태에 따라 운영, 대기열, 조합, 강화, 매출, 조명, 프로그램, 스태프 정보가 달라집니다."), NAME_None},
        {TEXT("GroupEditing"), LOCTEXT("GroupEditingTitle", "그룹 편집"), LOCTEXT("GroupEditingBody",
            "다중 선택한 시설을 묶어서 이동하거나 복제·철거할 수 있습니다.\n\n이동 또는 복제 미리보기 중에는 묶음을 회전하고 위치를 확인한 뒤 적용하거나 취소할 수 있습니다. 복제에는 설치 비용이 적용됩니다."), NAME_None},
        {TEXT("GroupPreset"), LOCTEXT("GroupPresetTitle", "그룹 프리셋"), LOCTEXT("GroupPresetBody",
            "다중 선택한 시설 배치를 이름으로 저장하고 나중에 불러와 배치할 수 있습니다.\n\n저장된 프리셋은 목록에서 불러오거나 삭제할 수 있습니다. 불러온 시설을 설치할 때에는 설치 비용과 배치 조건이 적용됩니다."), NAME_None},
        {TEXT("BuildRotation"), LOCTEXT("RotationTitle", "시설 회전"), LOCTEXT("RotationBody",
            "시설 배치 또는 이동 미리보기 중 Q와 E로 방향을 회전합니다. Shift를 누른 상태에서 Q 또는 E를 유지하면 각도를 자유롭게 조정할 수 있습니다.\n\n회전한 모양과 필요한 공간을 확인한 뒤 배치를 확정하세요."), NAME_None},
        {TEXT("DirectAdjustment"), LOCTEXT("AdjustmentTitle", "직접 조정"), LOCTEXT("AdjustmentBody",
            "시설 배치 또는 이동 미리보기에서 위치를 정한 뒤 T를 누르면 현재 위치를 고정하고 직접 조정할 수 있습니다.\n\n다시 누르면 커서를 따라 위치를 선택하는 방식으로 돌아갑니다. 직접 조정은 배치를 확정하지 않습니다."), NAME_None},
        {TEXT("SynergyPreview"), LOCTEXT("SynergyPreviewTitle", "조합 미리보기"), LOCTEXT("SynergyPreviewBody",
            "시설 배치 중 주변 시설과 성립할 수 있는 조합을 미리 확인합니다.\n\n표시 설정에서 조합 미리보기를 켜거나 끌 수 있으며, 실제 효과는 시설과 거리 등의 조건을 확인한 뒤 적용됩니다."), NAME_None},
        {TEXT("FacilityQueue"), LOCTEXT("QueueTitle", "대기열"), LOCTEXT("QueueBody",
            "대기열을 사용하는 설치 시설을 선택하면 시설 정보의 대기열 탭에서 대기 정원, 자리 간격과 통로 폭을 확인할 수 있습니다.\n\n변경은 시설과 축제 상태가 허용할 때 적용할 수 있습니다. 대기열 형태는 지원되는 시설의 배치 중에도 선택할 수 있습니다."), NAME_None},
        {TEXT("ContinuousPlacement"), LOCTEXT("ContinuousTitle", "연속 배치"), LOCTEXT("ContinuousBody",
            "연속 배치를 지원하는 시설은 배치 중 마우스를 누른 채 드래그해 여러 위치를 미리 볼 수 있습니다.\n\n마우스를 놓으면 배치 가능한 위치에 설치됩니다. 시설 사이 간격은 설정할 수 있으며, 모든 시설이 이 조작을 지원하는 것은 아닙니다."), NAME_None},
        {TEXT("FacilityPrograms"), LOCTEXT("ProgramsTitle", "시설 프로그램"), LOCTEXT("ProgramsBody",
            "프로그램과 호환되는 시설을 설치하고 선택하면 시설 정보에서 관련 프로그램 상태를 확인할 수 있습니다.\n\n편성 가능한 경우 시설 정보의 프로그램 작업으로 이동할 수 있습니다. 사용 가능한 프로그램과 편성 조건은 시설과 축제 상태에 따라 달라집니다."), NAME_None},
        {TEXT("BuildUndo"), LOCTEXT("UndoTitle", "실행 취소"), LOCTEXT("UndoBody",
            "되돌릴 수 있는 마지막 건축 편집을 취소합니다. Ctrl+Z를 사용하거나 건축 도구의 실행 취소를 선택하세요.\n\n취소할 기록과 현재 축제 상태에 따라 사용할 수 없는 경우가 있습니다."), NAME_None},
        {TEXT("BuildRedo"), LOCTEXT("RedoTitle", "다시 실행"), LOCTEXT("RedoBody",
            "실행 취소한 건축 편집을 다시 적용합니다. Ctrl+Y 또는 Ctrl+Shift+Z를 사용하거나 건축 도구의 다시 실행을 선택하세요.\n\n다시 실행할 기록과 현재 축제 상태에 따라 사용할 수 없는 경우가 있습니다."), NAME_None},
        {TEXT("BuildCamera"), LOCTEXT("CameraTitle", "카메라"), LOCTEXT("CameraBody",
            "카메라 이동 키와 마우스 휠을 사용해 축제 공간을 살펴봅니다. 이동 키는 사용자가 변경한 키 설정을 따릅니다.\n\n건축 설정에서 커서 중심 확대와 화면 가장자리 이동을 켜거나 끌 수 있습니다."), NAME_None}
    };
    return Entries;
}

const FFBuildFeatureGuide::FTopic* FFBuildFeatureGuide::Find(const FName Id)
{
    return Topics().FindByPredicate([Id](const FTopic& Topic) { return Topic.Id == Id; });
}

#undef LOCTEXT_NAMESPACE
