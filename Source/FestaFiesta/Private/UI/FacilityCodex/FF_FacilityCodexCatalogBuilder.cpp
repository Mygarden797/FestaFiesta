// Add one authored button without rebuilding the facility catalog or its Designer layout.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "WidgetBlueprint.h"

namespace FFFacilityCodexCatalogBuilder
{
    constexpr const TCHAR* Prefix = TEXT("FF_FACILITY_CODEX_CATALOG");
    constexpr const TCHAR* PanelPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild");
    constexpr const TCHAR* LauncherPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher");
    const FName LauncherName(TEXT("Widget_FacilityCodexCatalogLauncher"));

    void Migrate()
    {
        UWidgetBlueprint* Blueprint = FFUIWbpBuilder::LoadExistingBlueprint(PanelPath);
        UClass* LauncherClass = FFUIWbpBuilder::LoadGeneratedClass(LauncherPath);
        if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->ParentClass ||
            !Blueprint->ParentClass->IsChildOf(UFF_PrepPanelBuild::StaticClass()) ||
            !LauncherClass || !LauncherClass->IsChildOf(UFF_FacilityCodexLauncher::StaticClass()))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: missing/incompatible catalog or launcher; existing assets preserved."), Prefix);
            return;
        }
        UWidgetTree& Tree = *Blueprint->WidgetTree;
        UHorizontalBox* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_CatalogHeader")));
        UEditableTextBox* Search = Cast<UEditableTextBox>(Tree.FindWidget(TEXT("Search_Facilities")));
        if (!Header || !Search || Search->GetParent() != Header)
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: authored search/header seam missing; catalog preserved."), Prefix);
            return;
        }
        if (UWidget* Existing = Tree.FindWidget(LauncherName))
        {
            if (Existing->IsA(LauncherClass) && Existing->GetParent() == Header)
            {
                UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer edits and saved bytes preserved."), Prefix);
            }
            else
            {
                UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: conflicting launcher name/type/parent; catalog preserved."), Prefix);
            }
            return;
        }
        Blueprint->Modify();
        Tree.Modify();
        Header->Modify();
        UUserWidget* Launcher = FFUIWbpBuilder::MakeWidgetOfClass(*Blueprint, Tree, LauncherName, LauncherClass, true);
        if (!Launcher) return;
        UHorizontalBoxSlot* Slot = Header->AddChildToHorizontalBox(Launcher);
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin(0, 0, 10, 0));
        // ShiftChild retains every original widget, slot, binding and property.
        Header->ShiftChild(Header->GetChildIndex(Search) + 1, Launcher);
        if (FFUIWbpBuilder::SaveNewBlueprint(Prefix, *Blueprint))
        {
            UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_ADDED: codex button added beside search; original catalog retained."), Prefix);
        }
    }

    FAutoConsoleCommand Command(TEXT("FF.MigrateFacilityCodexCatalog"),
        TEXT("Add the authored codex button beside catalog search once, preserving existing Designer layout."),
        FConsoleCommandDelegate::CreateStatic(&Migrate));
}
#endif
