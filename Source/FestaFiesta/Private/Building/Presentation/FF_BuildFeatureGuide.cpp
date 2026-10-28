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
            "격자, 위치 간격, 시설 범위, 조합 미리보기 등 건축에 필요한 설정을 한곳에서 조정합니다.\n\n배치 중에는 직접 조정과 회전·이동 도구를 사용할 수 있습니다. 커서 중심 확대와 화면 가장자리 이동도 취향에 맞게 켜거나 끄세요.\n\n설정을 마쳤으면 닫기를 누르거나 건축 설정 버튼을 다시 누르세요."), TEXT("Button_BuildSettings"), TEXT("Size_BuildToolbarLine5")},
        {TEXT("BuildCatalog"), LOCTEXT("CatalogTitle", "시설 목록"), LOCTEXT("CatalogBody",
            "설치할 시설을 고르는 카탈로그를 엽니다. 분류나 검색으로 시설을 찾고, 가격과 필요한 공간을 확인한 뒤 시설을 선택하세요.\n\n시설을 선택하면 배치 화면으로 이동합니다. 목록으로 돌아와 다른 시설을 고를 수 있으며, 별표로 자주 쓰는 시설을 즐겨찾기에 저장할 수 있습니다."), TEXT("Button_ToolbarCatalog"), TEXT("Size_BuildToolbarLine6")},
        {TEXT("BuildControls"), LOCTEXT("ControlsTitle", "조작 안내"), LOCTEXT("ControlsBody",
            "현재 건축 상황에 맞는 조작법과 단축키를 화면에 표시합니다. 시설 선택, 배치, 회전, 이동 중 사용할 수 있는 입력을 확인하세요.\n\n연속 설치 시설은 누른 채 드래그하면 일정 간격의 반투명 미리보기가 나타나고, 마우스를 놓으면 가능한 곳만 한 번에 설치됩니다. 설치 불가 위치는 건너뛰며, Ctrl+Z로 설치한 묶음을 한 번에 취소할 수 있습니다.\n\n대기열을 지원하는 시설은 대기열 모양 조작법도 나타납니다. 안내창의 닫기 × 버튼으로 닫고, 상단 조작 안내 버튼으로 다시 열 수 있습니다."), TEXT("Button_ToolbarHelp"), TEXT("Size_BuildToolbarLine7")},
        {TEXT("LightingControls"), LOCTEXT("LightingTitle", "시설 조명"), LOCTEXT("LightingBody",
            "카탈로그의 전구 아이콘은 조명을 조절할 수 있는 시설을 뜻합니다.\n\n해당 시설을 설치한 뒤 선택하고 시설 정보의 조명 설정을 여세요. 조명을 켜거나 끄고 밝기와 색상을 바꿀 수 있습니다.\n\n변경한 모습을 확인한 뒤 적용으로 저장하세요. 취소로 되돌리거나 원래 색상을 다시 사용할 수 있습니다."), NAME_None},
        {TEXT("FacilityCodex"), LOCTEXT("CodexTitle", "조합 도감"), LOCTEXT("CodexBody",
            "시설을 어떤 조합으로 배치하면 효과를 얻는지 확인하는 도감입니다. 조합 도감에서 필요한 시설과 거리 조건, 발견 여부를 확인하세요.\n\n조건에 맞게 시설을 설치하면 조합이 발견됩니다. 적용 중인 효과를 확인하고 받을 수 있는 발견 보상과 수집 보상도 챙기세요."), NAME_None},
        {TEXT("FacilityRevenueRanking"), LOCTEXT("RevenueTitle", "매출 순위"), LOCTEXT("RevenueBody",
            "현재 설치된 수익 시설의 매출을 비교합니다. 매출 순위를 열고 오늘 또는 축제 누적 기준을 선택하세요.\n\n시설별 순위, 매출과 전체 매출에서 차지하는 비중을 확인할 수 있습니다. 목록에서 시설을 선택한 뒤 해당 시설로 이동해 운영 상태를 살펴보세요."), NAME_None},
        {TEXT("BuildFavorites"), LOCTEXT("FavoritesTitle", "즐겨찾기"), LOCTEXT("FavoritesBody",
            "카탈로그에서 자주 쓰는 시설의 별표를 눌러 즐겨찾기에 저장하세요. 별표를 다시 누르면 해제됩니다.\n\n즐겨찾기 필터를 켜면 저장한 시설만 모아 볼 수 있습니다. 즐겨찾기는 개인 설정에 저장되어 다음 실행에서도 유지됩니다."), NAME_None}
    };
    return Entries;
}

const FFBuildFeatureGuide::FTopic* FFBuildFeatureGuide::Find(const FName Id)
{
    return Topics().FindByPredicate([Id](const FTopic& Topic) { return Topic.Id == Id; });
}

#undef LOCTEXT_NAMESPACE
