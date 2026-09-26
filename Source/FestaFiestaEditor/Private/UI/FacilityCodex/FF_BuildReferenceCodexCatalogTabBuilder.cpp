// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// Move the existing, functional codex launcher into a pinned tab in the build catalog.
// The 11 facility filters and every existing widget binding remain intact.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace FFBuildReferenceCodexCatalogTab
{
using namespace FFUIWbpBuilder;

constexpr const TCHAR* Version = TEXT("FF.BuildReference.CodexCatalogTab.20260923.v1");

void Apply()
{
    UWidgetBlueprint* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildPanelPackage);
    if (!BP || !BP->WidgetTree || !BP->ParentClass ||
        !BP->ParentClass->IsChildOf(UFF_PrepPanelBuild::StaticClass()))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_REFERENCE_CODEX_TAB_FAIL: catalog WBP missing"));
        return;
    }
    auto& Metadata = BP->GetOutermost()->GetMetaData();
    if (Metadata.GetValue(BP, Version) == TEXT("1"))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_REFERENCE_CODEX_TAB_REUSE"));
        return;
    }

    UWidgetTree& Tree = *BP->WidgetTree;
    auto* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_CatalogHeader")));
    auto* RowSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_CategoryRow")));
    auto* Scroll = Cast<UScrollBox>(Tree.FindWidget(TEXT("ScrollBox_Categories")));
    auto* Filters = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Categories")));
    auto* Launcher = Cast<UFF_FacilityCodexLauncher>(Tree.FindWidget(TEXT("Widget_FacilityCodexCatalogLauncher")));
    if (!Header || !RowSize || !Scroll || !Filters || !Launcher ||
        Launcher->GetParent() != Header || RowSize->GetContent() != Scroll ||
        Filters->GetParent() != Scroll)
    {
        UE_LOG(LogFFUIWbpBuilder, Error,
            TEXT("FF_BUILD_REFERENCE_CODEX_TAB_FAIL: expected header launcher and category row were not found"));
        return;
    }

    BP->Modify();
    Tree.Modify();
    Header->Modify();
    RowSize->Modify();
    Scroll->Modify();
    Launcher->Modify();
    Scroll->RemoveFromParent();
    Launcher->RemoveFromParent();

    auto* Row = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("Row_BuildCodexCategory"));
    auto* Viewport = MakeWidget<USizeBox>(*BP, Tree, TEXT("Size_BuildCodexCategoryViewport"));
    Viewport->SetMinDesiredWidth(0.0f);
    Viewport->SetClipping(EWidgetClipping::ClipToBoundsAlways);
    Viewport->AddChild(Scroll);
    auto* FilterSlot = Row->AddChildToHorizontalBox(Viewport);
    FilterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    FilterSlot->SetVerticalAlignment(VAlign_Center);
    auto* CodexSlot = Row->AddChildToHorizontalBox(Launcher);
    CodexSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    CodexSlot->SetVerticalAlignment(VAlign_Center);
    CodexSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
    RowSize->SetHeightOverride(42.0f);
    RowSize->AddChild(Row);

    Metadata.SetValue(BP, Version, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_CODEX_TAB"), *BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_REFERENCE_CODEX_TAB_PASS"));
    }
    else
    {
        Metadata.RemoveValue(BP, Version);
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_REFERENCE_CODEX_TAB_FAIL: save failed"));
    }
}

FAutoConsoleCommand Command(TEXT("FF.ApplyBuildReferenceCodexCatalogTab"),
    TEXT("Place the existing facility codex launcher beside the build category filters."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
