// Seed only this plugin's new WBP. The game's toolbar asset is read-only style data;
// no game module headers/classes or teammate widget trees are modified or copied.
#include "FF_MapWholeGridButtonWidget.h"

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFWholeGridAuthoring, Log, All);

namespace FFMapAuthoring
{
namespace WholeGridSeed
{
    constexpr const TCHAR* AssetName = TEXT("WBP_FF_WholeGridButton");
    constexpr const TCHAR* AssetFolder = TEXT("/FF_MapOverview/UI");
    constexpr const TCHAR* AssetObject = TEXT("/FF_MapOverview/UI/WBP_FF_WholeGridButton.WBP_FF_WholeGridButton");
    // Exact BuildToolsObject from Building/Presentation/FF_BuildUIAssetPaths.h.
    // This is an asset lookup only; the optional plugin never links game C++ symbols.
    constexpr const TCHAR* SourceObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools.WBP_FF_BuildTools");

    template<class T> T* NewWidget(UWidgetBlueprint& BP, const TCHAR* Name, const bool bVariable = false)
    {
        T* Result = BP.WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
        Result->bIsVariable = bVariable;
        BP.OnVariableAdded(Name);
        return Result;
    }

    void CopyBounds(const USizeBox& Source, USizeBox& Destination)
    {
        if (Source.IsWidthOverride()) Destination.SetWidthOverride(Source.GetWidthOverride());
        if (Source.IsHeightOverride()) Destination.SetHeightOverride(Source.GetHeightOverride());
        if (Source.IsMinDesiredWidthOverride()) Destination.SetMinDesiredWidth(Source.GetMinDesiredWidth());
        if (Source.IsMinDesiredHeightOverride()) Destination.SetMinDesiredHeight(Source.GetMinDesiredHeight());
        if (Source.IsMaxDesiredWidthOverride()) Destination.SetMaxDesiredWidth(Source.GetMaxDesiredWidth());
        if (Source.IsMaxDesiredHeightOverride()) Destination.SetMaxDesiredHeight(Source.GetMaxDesiredHeight());
        if (Source.IsMinAspectRatioOverride()) Destination.SetMinAspectRatio(Source.GetMinAspectRatio());
        if (Source.IsMaxAspectRatioOverride()) Destination.SetMaxAspectRatio(Source.GetMaxAspectRatio());
    }

    void CopyVerticalSlot(const UWidget& Source, UVerticalBoxSlot& Destination)
    {
        if (const auto* SourceSlot = Cast<UVerticalBoxSlot>(Source.Slot))
        {
            Destination.SetPadding(SourceSlot->GetPadding());
            Destination.SetHorizontalAlignment(SourceSlot->GetHorizontalAlignment());
            Destination.SetVerticalAlignment(SourceSlot->GetVerticalAlignment());
            Destination.SetSize(SourceSlot->GetSize());
        }
        else
        {
            Destination.SetHorizontalAlignment(HAlign_Center);
            Destination.SetVerticalAlignment(VAlign_Center);
        }
    }

    bool SaveBlueprint(UWidgetBlueprint& BP)
    {
        UPackage* Package = BP.GetOutermost();
        Package->MarkPackageDirty();
        const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, &BP, *File, Args);
    }
}

void EnsureWholeGridButton()
{
    using namespace WholeGridSeed;
    if (LoadObject<UWidgetBlueprint>(nullptr, AssetObject))
    {
        UE_LOG(LogFFWholeGridAuthoring, Display, TEXT("FF_MAP_TOOLBAR: existing whole-grid WBP preserved."));
        return;
    }
    const UWidgetBlueprint* Source = LoadObject<UWidgetBlueprint>(nullptr, SourceObject);
    const UWidgetTree* SourceTree = Source ? Source->WidgetTree.Get() : nullptr;
    const UButton* SourceButton = SourceTree ? Cast<UButton>(SourceTree->FindWidget(TEXT("Button_Grid"))) : nullptr;
    const UImage* SourceIcon = SourceTree ? Cast<UImage>(SourceTree->FindWidget(TEXT("Image_ToolbarF2"))) : nullptr;
    const USizeBox* SourceVectorIcon = SourceTree ? Cast<USizeBox>(SourceTree->FindWidget(TEXT("Size_BuildToolbarLine2"))) : nullptr;
    const UHorizontalBox* SourceRow = SourceTree ? Cast<UHorizontalBox>(SourceTree->FindWidget(TEXT("Row_BuildReferenceTopBar"))) : nullptr;
    const UTextBlock* SourceCaption = nullptr;
    if (SourceTree)
        for (const TCHAR* Name : {TEXT("Label_PearlTool2"), TEXT("Caption_ToolbarF2"), TEXT("Text_Grid")})
            if (!SourceCaption) SourceCaption = Cast<UTextBlock>(SourceTree->FindWidget(Name));
    const UWidget* SourceCell = SourceButton;
    while (SourceCell && SourceCell->GetParent() && SourceCell->GetParent() != SourceRow)
        SourceCell = SourceCell->GetParent();
    const USizeBox* SourceBounds = Cast<USizeBox>(SourceCell);
    if (!SourceButton || (!SourceIcon && !SourceVectorIcon) || !SourceCaption || !SourceRow || !SourceBounds || SourceBounds->GetParent() != SourceRow ||
        !SourceBounds->IsWidthOverride() || !SourceBounds->IsHeightOverride())
    {
        UE_LOG(LogFFWholeGridAuthoring, Error, TEXT("FF_MAP_TOOLBAR: source toolbar/grid cell/icon/caption contract missing; no assets changed."));
        return;
    }
    const USizeBox* SourceIconBounds = nullptr;
    for (const UWidget* Parent = SourceIcon ? SourceIcon->GetParent() : nullptr; Parent && Parent != SourceButton; Parent = Parent->GetParent())
        if (!SourceIconBounds) SourceIconBounds = Cast<USizeBox>(Parent);

    auto* Factory = NewObject<UWidgetBlueprintFactory>();
    Factory->ParentClass = UFFMapWholeGridButtonWidget::StaticClass();
    auto* BP = Cast<UWidgetBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
        AssetName, AssetFolder, UWidgetBlueprint::StaticClass(), Factory));
    if (!BP || !BP->WidgetTree) return;
    USizeBox* Bounds = NewWidget<USizeBox>(*BP, TEXT("Bounds"));
    CopyBounds(*SourceBounds, *Bounds);
    Bounds->SetClipping(EWidgetClipping::ClipToBounds);
    BP->WidgetTree->RootWidget = Bounds;
    UButton* Button = NewWidget<UButton>(*BP, TEXT("WholeGridButton"), true);
    Button->SetStyle(SourceButton->GetStyle());
    Button->SetBackgroundColor(SourceButton->GetBackgroundColor());
    Button->SetColorAndOpacity(SourceButton->GetColorAndOpacity());
    Button->SetClipping(SourceButton->GetClipping());
    Button->SetToolTipText(FText::FromString(TEXT("맵 전체의 건축 격자를 표시합니다.")));
    if (auto* Focusable = FindFProperty<FBoolProperty>(UButton::StaticClass(), TEXT("IsFocusable")))
        Focusable->SetPropertyValue_InContainer(Button, Focusable->GetPropertyValue_InContainer(SourceButton));
    Bounds->SetContent(Button);
    if (const auto* SourceSizeSlot = Cast<USizeBoxSlot>(SourceButton->Slot))
        if (auto* SizeSlot = Cast<USizeBoxSlot>(Button->Slot))
        {
            SizeSlot->SetPadding(SourceSizeSlot->GetPadding());
            SizeSlot->SetHorizontalAlignment(SourceSizeSlot->GetHorizontalAlignment());
            SizeSlot->SetVerticalAlignment(SourceSizeSlot->GetVerticalAlignment());
        }

    UVerticalBox* Content = NewWidget<UVerticalBox>(*BP, TEXT("Content"));
    Button->SetContent(Content);
    if (const UWidget* SourceContent = SourceButton->GetContent())
        if (const auto* SourceContentSlot = Cast<UButtonSlot>(SourceContent->Slot))
            if (auto* ContentSlot = Cast<UButtonSlot>(Content->Slot))
            {
                ContentSlot->SetPadding(SourceContentSlot->GetPadding());
                ContentSlot->SetHorizontalAlignment(SourceContentSlot->GetHorizontalAlignment());
                ContentSlot->SetVerticalAlignment(SourceContentSlot->GetVerticalAlignment());
            }
    UWidget* AuthoredIcon = nullptr;
    if (SourceIcon)
    {
        UImage* Icon = NewWidget<UImage>(*BP, TEXT("Icon"), true);
        Icon->SetBrush(SourceIcon->GetBrush());
        Icon->SetColorAndOpacity(SourceIcon->GetColorAndOpacity());
        Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
        USizeBox* IconBounds = NewWidget<USizeBox>(*BP, TEXT("IconBounds"));
        if (SourceIconBounds) CopyBounds(*SourceIconBounds, *IconBounds);
        else
        {
            IconBounds->SetWidthOverride(SourceIcon->GetBrush().ImageSize.X);
            IconBounds->SetHeightOverride(SourceIcon->GetBrush().ImageSize.Y);
        }
        IconBounds->SetContent(Icon);
        AuthoredIcon = IconBounds;
        CopyVerticalSlot(SourceIconBounds ? static_cast<const UWidget&>(*SourceIconBounds) : static_cast<const UWidget&>(*SourceIcon),
            *Content->AddChildToVerticalBox(AuthoredIcon));
    }
    else
    {
        // The current toolbar authors its grid glyph as UMG borders/canvas lines.
        // Duplicate that editable subtree instead of assuming a texture icon.
        // Widget descendants are owned by WidgetTree, not their panel. Duplicate
        // the transient tree first so copied slots cannot refer to source widgets.
        TStrongObjectPtr<UWidgetTree> CopiedTree(CastChecked<UWidgetTree>(StaticDuplicateObject(SourceTree, GetTransientPackage())));
        AuthoredIcon = CopiedTree->FindWidget(SourceVectorIcon->GetFName());
        CopiedTree->RemoveWidget(AuthoredIcon);
        TArray<UWidget*> GlyphWidgets;
        UWidgetTree::GetChildWidgets(AuthoredIcon, GlyphWidgets);
        AuthoredIcon->Rename(TEXT("IconBounds"), BP->WidgetTree);
        AuthoredIcon->bIsVariable = false;
        BP->OnVariableAdded(AuthoredIcon->GetFName());
        for (UWidget* Child : GlyphWidgets)
        {
            Child->Rename(*FString::Printf(TEXT("WholeGridGlyph_%s"), *Child->GetName()), BP->WidgetTree);
            Child->bIsVariable = false;
            BP->OnVariableAdded(Child->GetFName());
        }
        AuthoredIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        CopyVerticalSlot(*SourceVectorIcon, *Content->AddChildToVerticalBox(AuthoredIcon));
    }
    UTextBlock* Label = NewWidget<UTextBlock>(*BP, TEXT("Label"), true);
    Label->SetText(FText::FromString(TEXT("전체 격자")));
    Label->SetFont(SourceCaption->GetFont());
    Label->SetColorAndOpacity(SourceCaption->GetColorAndOpacity());
    Label->SetShadowOffset(SourceCaption->GetShadowOffset());
    Label->SetShadowColorAndOpacity(SourceCaption->GetShadowColorAndOpacity());
    if (const auto* Justification = FindFProperty<FByteProperty>(SourceCaption->GetClass(), TEXT("Justification")))
        Label->SetJustification(static_cast<ETextJustify::Type>(Justification->GetPropertyValue_InContainer(SourceCaption)));
    Label->SetAutoWrapText(SourceCaption->GetAutoWrapText());
    Label->SetTextOverflowPolicy(SourceCaption->GetTextOverflowPolicy());
    Label->SetClipping(SourceCaption->GetClipping());
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    CopyVerticalSlot(*SourceCaption, *Content->AddChildToVerticalBox(Label));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error || !BP->GeneratedClass)
    {
        UE_LOG(LogFFWholeGridAuthoring, Error, TEXT("FF_MAP_TOOLBAR: whole-grid WBP failed to compile."));
        return;
    }
    UFFMapWholeGridButtonWidget* Defaults = BP->GeneratedClass->GetDefaultObject<UFFMapWholeGridButtonWidget>();
    Defaults->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (SourceIcon) Defaults->SelectedIconColor = SourceIcon->GetColorAndOpacity();
    Defaults->SelectedTextColor = SourceCaption->GetColorAndOpacity();
    if (const auto* Flag = FindFProperty<FBoolProperty>(SourceButton->GetClass(), TEXT("bUseSelectionStyle")))
        Defaults->bUseSelectionStyle = Flag->GetPropertyValue_InContainer(SourceButton);
    if (const auto* Style = FindFProperty<FStructProperty>(SourceButton->GetClass(), TEXT("SelectionStyle"));
        Style && Style->Struct == FButtonStyle::StaticStruct())
        Defaults->SelectionStyle = *Style->ContainerPtrToValuePtr<FButtonStyle>(SourceButton);
    if (const auto* Color = FindFProperty<FStructProperty>(SourceButton->GetClass(), TEXT("SelectionTextColor"));
        Color && Color->Struct == FSlateColor::StaticStruct())
    {
        Defaults->SelectedTextColor = *Color->ContainerPtrToValuePtr<FSlateColor>(SourceButton);
        if (Defaults->SelectedTextColor.IsColorSpecified())
            Defaults->SelectedIconColor = Defaults->SelectedTextColor.GetSpecifiedColor();
    }
    if (SaveBlueprint(*BP))
    {
        UE_LOG(LogFFWholeGridAuthoring, Display, TEXT("FF_MAP_TOOLBAR: saved editable whole-grid WBP using read-only toolbar bounds/styles/glyph/font."));
    }
    else
    {
        UE_LOG(LogFFWholeGridAuthoring, Error, TEXT("FF_MAP_TOOLBAR: whole-grid WBP save failed."));
    }
}
}
