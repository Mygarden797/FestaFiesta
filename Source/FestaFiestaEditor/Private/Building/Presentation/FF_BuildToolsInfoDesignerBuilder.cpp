// Finalize the current authored layouts without rebuilding their controls or event graphs.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Animation/WidgetAnimation.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFBuildToolsInfoDesigner
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_TOOLS_INFO_DESIGNER");
constexpr const TCHAR* PreviousVersion = TEXT("FF.ToolsInfoDesigner.20260929.v1");
constexpr const TCHAR* VisibilityVersion = TEXT("FF.ToolsInfoDesigner.20260929.v2");
constexpr const TCHAR* Version = TEXT("FF.ToolsInfoDesigner.20260929.v3");

void Show(UWidgetTree& Tree, const TCHAR* Name, bool bShow)
{
    if (UWidget* Widget = Tree.FindWidget(Name))
        Widget->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

bool CanRetireBranches(UWidgetBlueprint& BP, bool bTools)
{
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1") ||
        BP.GetOutermost()->GetMetaData().GetValue(&BP, VisibilityVersion) == TEXT("1") ||
        BP.GetOutermost()->GetMetaData().GetValue(&BP, PreviousVersion) == TEXT("1")) return true;
    TSet<UWidget*> Candidates;
    auto AddBranch = [&](const TCHAR* Name, bool bKeepMarker = false)
    {
        if (UWidget* Branch = BP.WidgetTree->FindWidget(Name))
        {
            TArray<UWidget*> Children;
            UWidgetTree::GetChildWidgets(Branch, Children);
            if (!bKeepMarker) Candidates.Add(Branch);
            for (UWidget* Child : Children) Candidates.Add(Child);
        }
    };
    if (bTools)
    {
        for (const TCHAR* Name : {TEXT("Row_RetiredToolbarSnap"), TEXT("Row_BuildToolbarKeycaps"),
            TEXT("Text_ControlHelp"), TEXT("Border_BuildToolbarGold"), TEXT("Panel_Radial"), TEXT("Button_Radial")})
            AddBranch(Name);
    }
    else
    {
        AddBranch(TEXT("Aurora_InfoV7"), true);
        AddBranch(TEXT("Clarity_InfoV1"), true);
        for (const TCHAR* Name : {TEXT("Summary_Card"), TEXT("Panel_InspectorTabs"), TEXT("Clarity_SecondaryTabs")})
            AddBranch(Name);
    }
    // Match RemoveLegacyBranch: native mandatory endpoints and their children
    // keep their names and are reparented, so they are not deletion candidates.
    TSet<UWidget*> Retained;
    for (UWidget* Widget : Candidates)
    {
        const FProperty* Property = FindFProperty<FProperty>(BP.ParentClass.Get(), Widget->GetFName());
        if (!Property || !Property->HasMetaData(TEXT("BindWidget"))) continue;
        Retained.Add(Widget);
        TArray<UWidget*> Children;
        UWidgetTree::GetChildWidgets(Widget, Children);
        for (UWidget* Child : Children) Retained.Add(Child);
    }
    TSet<FName> DeletedNames;
    for (UWidget* Widget : Candidates)
    {
        if (Retained.Contains(Widget)) continue;
        DeletedNames.Add(Widget->GetFName());
        if (FBlueprintEditorUtils::IsVariableUsed(&BP, Widget->GetFName()))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL %s: retired widget %s is used by a Blueprint graph"),
                Prefix, *BP.GetName(), *Widget->GetName());
            return false;
        }
    }
    for (const UWidgetAnimation* Animation : BP.Animations)
    {
        if (!Animation) continue;
        for (const FWidgetAnimationBinding& Binding : Animation->GetBindings())
        {
            // Slot animation bindings also resolve their target by WidgetName.
            if (!Binding.bIsRootWidget && DeletedNames.Contains(Binding.WidgetName))
            {
                UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL %s: animation %s targets retired widget %s"),
                    Prefix, *BP.GetName(), *Animation->GetName(), *Binding.WidgetName.ToString());
                return false;
            }
        }
    }
    return true;
}

// Retain mandatory native binding endpoints, but discard their obsolete presentation
// wrappers. Current conditional pages never enter this explicitly named retirement list.
int32 RemoveLegacyBranch(UWidgetBlueprint& BP, const TCHAR* Name, bool bKeepEmptyMarker = false)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    UWidget* Branch = Tree.FindWidget(Name);
    if (!Branch) return 0;
    TArray<UWidget*> Nodes;
    UWidgetTree::GetChildWidgets(Branch, Nodes);
    if (!bKeepEmptyMarker) Nodes.Add(Branch);
    TSet<UWidget*> Retained;
    for (UWidget* Widget : Nodes)
    {
        const FProperty* Binding = FindFProperty<FProperty>(BP.ParentClass, Widget->GetFName());
        if (!Binding || !Binding->HasMetaData(TEXT("BindWidget"))) continue;
        auto* Compatibility = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_RequiredBindings")));
        if (!Compatibility)
        {
            auto* Shell = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Shell")));
            if (!Shell) continue;
            Compatibility = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Panel_RequiredBindings"));
            Compatibility->SetVisibility(ESlateVisibility::Collapsed);
            Shell->AddChildToVerticalBox(Compatibility);
        }
        Widget->RemoveFromParent();
        Compatibility->AddChildToVerticalBox(Widget);
        Retained.Add(Widget);
        TArray<UWidget*> Children;
        UWidgetTree::GetChildWidgets(Widget, Children);
        for (UWidget* Child : Children) Retained.Add(Child);
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_TOOLS_INFO_DESIGNER retained required binding %s"), *Widget->GetName());
    }
    int32 Removed = 0;
    for (int32 Index = Nodes.Num() - 1; Index >= 0; --Index)
    {
        UWidget* Widget = Nodes[Index];
        if (Retained.Contains(Widget)) continue;
        if (auto* Panel = Cast<UPanelWidget>(Widget)) Panel->ClearChildren();
        BP.OnVariableRemoved(Widget->GetFName());
        Widget->RemoveFromParent();
        Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        ++Removed;
    }
    return Removed;
}

bool Finalize(UWidgetBlueprint& BP, bool bTools)
{
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    if (bTools ? !Tree.FindWidget(TEXT("Column_ReadablePlacementHeader")) : !Tree.FindWidget(TEXT("Row_ReadableInspectorTabs")))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_TOOLS_INFO_DESIGNER_FAIL latest readability layout required: %s"), *BP.GetName()); return false; }
    BP.Modify(); Tree.Modify();
    int32 Removed = 0;
    const bool bExistingVisibility = BP.GetOutermost()->GetMetaData().GetValue(&BP, VisibilityVersion) == TEXT("1");
    const bool bExistingLayout = bExistingVisibility || BP.GetOutermost()->GetMetaData().GetValue(&BP, PreviousVersion) == TEXT("1");
    if (bTools && !bExistingLayout)
    {
        for (const TCHAR* Name : {TEXT("Row_RetiredToolbarSnap"), TEXT("Row_BuildToolbarKeycaps"),
            TEXT("Text_ControlHelp"), TEXT("Border_BuildToolbarGold"), TEXT("Panel_Radial"), TEXT("Button_Radial")})
            Removed += RemoveLegacyBranch(BP, Name);
        // Bake the previously runtime-only preferred placement into the Designer.
        if (auto* Help = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildWorldHelp"))))
            if (auto* Slot = Cast<UCanvasPanelSlot>(Help->Slot))
                Slot->SetPosition(FVector2D(Slot->GetPosition().X, 525.f));
        if (auto* Settings = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildSettings"))))
            Settings->SetHeightOverride(950.f);
        Show(Tree, TEXT("Border_PlacementHelp"), true);
        Show(Tree, TEXT("Row_BuildHelpQueue"), true);
        Show(Tree, TEXT("Panel_BuildSettings"), false);
    }
    else if (!bExistingLayout)
    {
        // Empty migration markers are structural version guards used by the older
        // opt-in builders. Retain those guards, never their former visual trees.
        for (const TCHAR* Name : {TEXT("Aurora_InfoV7"), TEXT("Clarity_InfoV1")})
            Removed += RemoveLegacyBranch(BP, Name, true);
        for (const TCHAR* Name : {TEXT("Summary_Card"), TEXT("Panel_InspectorTabs"), TEXT("Clarity_SecondaryTabs")})
            Removed += RemoveLegacyBranch(BP, Name);
        // Store the same team-based captions as the current runtime, then leave
        // future Designer caption edits alone after this one-time migration.
        const struct { const TCHAR* Name; const TCHAR* Caption; } Labels[] = {
            {TEXT("Clarity_Label_Text_ServiceDuration"), TEXT("팀당 이용 시간")},
            {TEXT("Clarity_Label_Text_Occupancy"), TEXT("대기·처리 중 팀")},
            {TEXT("Clarity_Label_Text_RemainingService"), TEXT("남은 서비스 팀")},
            {TEXT("Clarity_Label_Text_ExpectedWait"), TEXT("한 팀 예상 대기")},
            {TEXT("V2_UpgradeLabelTime"), TEXT("팀당 시간")}
        };
        for (const auto& Label : Labels)
            if (auto* Text = Cast<UCommonTextBlock>(Tree.FindWidget(Label.Name)))
                Text->SetText(FText::FromString(Label.Caption));
        if (UWidget* Occupancy = Tree.FindWidget(TEXT("Text_Occupancy")))
            Occupancy->SetToolTipText(FText::FromString(TEXT("대기·처리 중인 팀 수와 최대 팀 수입니다. 인원수와 관계없이 한 팀이 한 자리를 사용합니다.")));
        Show(Tree, TEXT("Panel_BuildSection"), true);
        Show(Tree, TEXT("Panel_Actions"), true);
        for (const TCHAR* Name : {TEXT("Panel_LiveSection"), TEXT("Panel_SynergySection"),
            TEXT("Panel_UpgradeSection"), TEXT("Panel_UpgradeActions"), TEXT("Panel_RevenueSection"),
            TEXT("Panel_ProgramSection"), TEXT("Widget_LightingControls"), TEXT("Button_LightingMinimize"),
            TEXT("Panel_LightingCompact"), TEXT("Panel_RemoveConfirmation"), TEXT("Panel_RemoveActions"),
            TEXT("Button_TabLighting"), TEXT("Button_TabPrograms")}) Show(Tree, Name, false);
    }
    if (!bExistingVisibility)
    {
        if (!bTools)
            if (auto* Favorite = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_Favorite"))))
                if (Favorite->GetText().ToString() == TEXT("☆ 즐겨찾기"))
                    Favorite->SetText(FText::FromString(TEXT("☆")));
        // Unreal Designer ignores serialized Visibility and uses this separate flag.
        // Apply once so later Designer eye-state edits stay authored.
        Tree.ForEachWidget([](UWidget* Widget)
        {
            const ESlateVisibility Visibility = Widget->GetVisibility();
            Widget->bHiddenInDesigner = Visibility == ESlateVisibility::Collapsed || Visibility == ESlateVisibility::Hidden;
        });
    }
    if (bTools)
    {
        // Match runtime's compact state captions. Only replace known old defaults;
        // custom captions, layout, fonts and the earlier visibility stage stay intact.
        const struct { const TCHAR* Name; const TCHAR* Old; const TCHAR* Current; } Toggles[] = {
            {TEXT("Text_SettingsGrid"), TEXT("건축 격자 켜짐"), TEXT("켜짐")},
            {TEXT("Text_SettingsFacilityRanges"), TEXT("시설 범위 켜짐"), TEXT("켜짐")},
            {TEXT("Text_SettingsSynergyPreview"), TEXT("조합 미리보기 켜짐"), TEXT("켜짐")},
            {TEXT("Text_SettingsDirectAdjustment"), TEXT("직접 조정 꺼짐"), TEXT("꺼짐")},
            {TEXT("Text_SettingsZoomToCursor"), TEXT("커서 기준 확대 켜짐"), TEXT("켜짐")},
            {TEXT("Text_SettingsEdgeScroll"), TEXT("가장자리 이동 켜짐"), TEXT("켜짐")}
        };
        for (const auto& Toggle : Toggles)
            if (auto* Label = Cast<UTextBlock>(Tree.FindWidget(Toggle.Name)))
                if (Label->GetText().ToString() == Toggle.Old)
                    Label->SetText(FText::FromString(Toggle.Current));
    }
    BP.GetOutermost()->GetMetaData().SetValue(&BP, Version, TEXT("1"));
    if (!SaveNewBlueprint(Prefix, BP))
    { BP.GetOutermost()->GetMetaData().RemoveValue(&BP, Version); return false; }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_TOOLS_INFO_DESIGNER_CLEAN %s removed=%d"), *BP.GetName(), Removed);
    return true;
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_TOOLS_INFO_DESIGNER_FAIL requires idle Entry editor")); return; }
    auto* Tools = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildToolsPackage);
    auto* Info = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    if (!Tools || !Info || !Tools->WidgetTree || !Info->WidgetTree ||
        Tools->ParentClass != UFF_BuildToolsWidget::StaticClass() || Info->ParentClass != UFF_FacilityInfoPanel::StaticClass())
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_TOOLS_INFO_DESIGNER_FAIL missing authored assets")); return; }
    // Validate both assets before changing either tree or saving a package.
    if (!CanRetireBranches(*Tools, true) || !CanRetireBranches(*Info, false)) return;
    if (!Finalize(*Tools, true) || !Finalize(*Info, false)) return;
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_TOOLS_INFO_DESIGNER_PASS assets=2"));
}

FAutoConsoleCommand Command(TEXT("FF.FinalizeBuildToolsInfoDesigner"),
    TEXT("Remove retired BuildTools/FacilityInfo visual trees and save the current editable Designer layout."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
