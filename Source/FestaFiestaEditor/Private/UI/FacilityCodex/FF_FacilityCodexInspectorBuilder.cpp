// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FacilityCodexInspectorBuilder.cpp
// 역할: 시설 정보 화면에 도감으로 이동하는 버튼을 추가한다.

// The only existing asset changed by this migration is the facility owner's inspector.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "WidgetBlueprint.h"

namespace FFFacilityCodexInspectorBuilder
{
    constexpr const TCHAR* Prefix = TEXT("FF_FACILITY_CODEX_INSPECTOR");
    constexpr const TCHAR* InspectorPath = FFBuildUIAssetPaths::FacilityInfoPanelPackage;
    constexpr const TCHAR* LauncherPath = FFBuildUIAssetPaths::FacilityCodexLauncherPackage;
    const FName LauncherName(TEXT("Widget_FacilityCodexLauncher"));

    void Migrate()
    {
        UWidgetBlueprint* Blueprint = FFUIWbpBuilder::LoadExistingBlueprint(InspectorPath);
        UClass* LauncherClass = FFUIWbpBuilder::LoadGeneratedClass(LauncherPath);
        if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->ParentClass ||
            !Blueprint->ParentClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()) ||
            !LauncherClass || !LauncherClass->IsChildOf(UFF_FacilityCodexLauncher::StaticClass()))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: missing/incompatible inspector or launcher. Run FF.BuildFacilityCodexUMG first; no existing asset changed."), Prefix);
            return;
        }
        UWidgetTree& Tree = *Blueprint->WidgetTree;
        UVerticalBox* Section = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_SynergySection")));
        if (!Section)
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: authored synergy section missing; inspector preserved."), Prefix);
            return;
        }
        if (UWidget* Existing = Tree.FindWidget(LauncherName))
        {
            if (Existing->IsA(LauncherClass) && Existing->GetParent() == Section)
            {
                UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer edits and saved bytes preserved."), Prefix);
            }
            else
            {
                UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: conflicting launcher name/type/parent; inspector preserved."), Prefix);
            }
            return;
        }

        Blueprint->Modify();
        Tree.Modify();
        Section->Modify();
        UUserWidget* Launcher = FFUIWbpBuilder::MakeWidgetOfClass(*Blueprint, Tree, LauncherName, LauncherClass, true);
        if (!Launcher) return;
        UVerticalBoxSlot* Slot = Section->AddChildToVerticalBox(Launcher);
        Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
        Slot->SetHorizontalAlignment(HAlign_Fill);
        // Insert before the summary. ShiftChild preserves all original slots and designer styling.
        Section->ShiftChild(0, Launcher);
        if (FFUIWbpBuilder::SaveNewBlueprint(Prefix, *Blueprint))
            UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_ADDED: authored launcher added; original inspector tree retained."), Prefix);
    }

    FAutoConsoleCommand Command(TEXT("FF.MigrateFacilityCodexInspector"),
        TEXT("Adds the authored facility codex launcher to the existing synergy section once; never rebuilds existing Designer layout."),
        FConsoleCommandDelegate::CreateStatic(&Migrate));
}
#endif
