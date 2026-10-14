// Initial authoring only. Saved WBP trees/materials are the source of truth for designers.
#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "FF_MapOverviewWidget.h"
#include "FF_MapOverviewMarkerWidget.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFMapAuthoring, Log, All);

namespace FFMapAuthoring
{
    void EnsureWholeGridButton();
    const FString Root = TEXT("/FF_MapOverview/UI/");

    template<class T> T* Existing(const FString& Name)
    {
        return LoadObject<T>(nullptr, *(Root + Name + TEXT(".") + Name));
    }

    bool Save(UObject& Asset)
    {
        UPackage* Package = Asset.GetOutermost();
        Package->MarkPackageDirty();
        const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, &Asset, *File, Args);
    }

    // Neutral transparent glyphs. Their brushes and tints remain editable in the WBP.
    UTexture2D* EnsureGlyph(const FString& Name, int32 Kind)
    {
        if (auto* Found = Existing<UTexture2D>(Name)) return Found;
        constexpr int32 Size = 64;
        TArray<FColor> Pixels;
        Pixels.SetNumZeroed(Size * Size);
        for (int32 Y = 0; Y < Size; ++Y)
        for (int32 X = 0; X < Size; ++X)
        {
            const double Dx = FMath::Abs(X + 0.5 - 32), Dy = FMath::Abs(Y + 0.5 - 32);
            bool bFill = false;
            if (Kind == 0) bFill = Dx < 28 && Dy < 29 && Dy + Dx * 0.57735 < 29;
            if (Kind == 1) // House: roof, walls and an open doorway.
                bFill = ((Y >= 17 && Y <= 31 && FMath::Abs(X - 32) <= Y - 16) ||
                    (X >= 21 && X <= 43 && Y >= 29 && Y <= 48)) &&
                    !(X >= 29 && X <= 35 && Y >= 37 && Y <= 49);
            if (Kind == 2) bFill = Y >= 9 && Y <= 51 && FMath::Abs(X - 32) <= (Y - 9) * 0.42 &&
                !(Y > 33 && FMath::Abs(X - 32) < (Y - 33) * 0.5);
            if (Kind == 3) bFill = true; // Terrain texture parameter fallback.
            if (bFill) Pixels[Y * Size + X] = Kind == 3 ? FColor(51, 64, 62, 255) : FColor::White;
        }
        UPackage* Package = CreatePackage(*(Root + Name));
        auto* Texture = NewObject<UTexture2D>(Package, *Name, RF_Public | RF_Standalone);
        Texture->Source.Init(Size, Size, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
        Texture->CompressionSettings = TC_EditorIcon;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->LODGroup = TEXTUREGROUP_UI;
        Texture->SRGB = true;
        Texture->PostEditChange();
        FAssetRegistryModule::AssetCreated(Texture);
        return Save(*Texture) ? Texture : nullptr;
    }

    template<class T> T* Expression(UMaterial& Material)
    {
        T* Result = NewObject<T>(&Material);
        Material.GetExpressionCollection().AddExpression(Result);
        return Result;
    }

    UMaterialExpressionScalarParameter* Scalar(UMaterial& Material, const TCHAR* Name, float Value)
    {
        auto* Result = Expression<UMaterialExpressionScalarParameter>(Material);
        Result->ParameterName = Name;
        Result->DefaultValue = Value;
        Result->Group = TEXT("Map appearance");
        return Result;
    }

    void Input(UMaterialExpressionCustom& Custom, const TCHAR* Name, UMaterialExpression* Expr)
    {
        FCustomInput& Item = Custom.Inputs.AddDefaulted_GetRef();
        Item.InputName = Name;
        Item.Input.Connect(0, Expr);
    }

    UMaterialInstanceConstant* EnsureMaterial(const FString& Name, bool bRim, UTexture2D* Fallback)
    {
        const FString InstanceName = TEXT("MI_") + Name;
        if (auto* Found = Existing<UMaterialInstanceConstant>(InstanceName)) return Found;
        const FString MaterialName = TEXT("M_") + Name;
        auto* Material = Existing<UMaterial>(MaterialName);
        if (!Material)
        {
            UPackage* Package = CreatePackage(*(Root + MaterialName));
            Material = NewObject<UMaterial>(Package, *MaterialName, RF_Public | RF_Standalone);
            Material->MaterialDomain = MD_UI;
            Material->BlendMode = BLEND_Translucent;
            Material->TwoSided = true;
            auto* UV = Expression<UMaterialExpressionTextureCoordinate>(*Material);
            auto* Radius = Scalar(*Material, TEXT("CircleRadius"), bRim ? 0.475f : 0.45f);
            auto* Opacity = Scalar(*Material, TEXT("Opacity"), bRim ? 0.58f : 0.9f);
            auto* Tint = Expression<UMaterialExpressionVectorParameter>(*Material);
            Tint->ParameterName = TEXT("Tint");
            Tint->Group = TEXT("Map appearance");
            Tint->DefaultValue = bRim ? FLinearColor(0.5, 0.55, 0.52, 1) : FLinearColor::White;
            auto* Mask = Expression<UMaterialExpressionCustom>(*Material);
            Mask->Description = bRim ? TEXT("Designer double compass rim") : TEXT("Circular terrain mask");
            Mask->OutputType = CMOT_Float1;
            Input(*Mask, TEXT("UV"), UV);
            Input(*Mask, TEXT("Radius"), Radius);
            Input(*Mask, TEXT("Opacity"), Opacity);
            if (bRim)
            {
                Input(*Mask, TEXT("LineWidth"), Scalar(*Material, TEXT("LineWidth"), 0.0018f));
                Input(*Mask, TEXT("RingGap"), Scalar(*Material, TEXT("RingGap"), 0.018f));
                Mask->Code = TEXT("float d = length(UV - 0.5); float aa = max(fwidth(d), 0.001); "
                    "float a = 1 - smoothstep(LineWidth, LineWidth + aa, abs(d - Radius)); "
                    "float b = 1 - smoothstep(LineWidth, LineWidth + aa, abs(d - Radius + RingGap)); "
                    "return saturate(a + b) * Opacity;");
                Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Tint);
            }
            else
            {
                Input(*Mask, TEXT("Feather"), Scalar(*Material, TEXT("EdgeFeather"), 0.003f));
                Mask->Code = TEXT("return (1 - smoothstep(Radius - Feather, Radius + Feather, length(UV - 0.5))) * Opacity;");
                auto* Sample = Expression<UMaterialExpressionTextureSampleParameter2D>(*Material);
                Sample->ParameterName = TEXT("MapTexture");
                Sample->Texture = Fallback;
                Sample->SamplerType = SAMPLERTYPE_Color;
                Sample->Coordinates.Connect(0, UV);
                auto* Center = Expression<UMaterialExpressionVectorParameter>(*Material);
                Center->ParameterName = TEXT("MapUVCenter");
                Center->DefaultValue = FLinearColor(0.5, 0.5, 0, 0);
                auto* LocalUV = Expression<UMaterialExpressionCustom>(*Material);
                LocalUV->Description = TEXT("Player-centered local map crop");
                LocalUV->OutputType = CMOT_Float2;
                Input(*LocalUV, TEXT("UV"), UV);
                Input(*LocalUV, TEXT("Center"), Center);
                Input(*LocalUV, TEXT("Scale"), Scalar(*Material, TEXT("MapUVScale"), 1.f));
                LocalUV->Code = TEXT("return Center.rg + (UV - 0.5) * Scale;");
                Sample->Coordinates.Connect(0, LocalUV);
                auto* Vignette = Expression<UMaterialExpressionCustom>(*Material);
                Vignette->OutputType = CMOT_Float3;
                Vignette->Description = TEXT("Terrain contrast, editable vignette");
                Input(*Vignette, TEXT("UV"), UV);
                Input(*Vignette, TEXT("Color"), Sample);
                Input(*Vignette, TEXT("Tint"), Tint);
                Input(*Vignette, TEXT("Strength"), Scalar(*Material, TEXT("VignetteStrength"), 0.35f));
                Vignette->Code = TEXT("return Color.rgb * Tint.rgb * (1 - Strength * smoothstep(0.15, 0.47, length(UV - 0.5)));");
                Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Vignette);
            }
            Material->GetEditorOnlyData()->Opacity.Connect(0, Mask);
            Material->PostEditChange();
            FAssetRegistryModule::AssetCreated(Material);
            if (!Save(*Material)) return nullptr;
        }
        auto* Instance = NewObject<UMaterialInstanceConstant>(CreatePackage(*(Root + InstanceName)), *InstanceName, RF_Public | RF_Standalone);
        Instance->SetParentEditorOnly(Material);
        Instance->PostEditChange();
        FAssetRegistryModule::AssetCreated(Instance);
        return Save(*Instance) ? Instance : nullptr;
    }

    template<class T> T* Widget(UWidgetBlueprint& BP, const TCHAR* Name, bool bVariable = false)
    {
        auto* Result = BP.WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
        Result->bIsVariable = bVariable;
        BP.OnVariableAdded(Name);
        return Result;
    }

    UCanvasPanelSlot* Place(UCanvasPanel& Canvas, UWidget& Child, float X, float Y, float W, float H, int32 Z = 0)
    {
        auto* Slot = Canvas.AddChildToCanvas(&Child);
        Slot->SetOffsets(FMargin(X, Y, W, H));
        Slot->SetZOrder(Z);
        return Slot;
    }

    void Stretch(UCanvasPanel& Canvas, UWidget& Child, int32 Z = 0)
    {
        auto* Slot = Canvas.AddChildToCanvas(&Child);
        Slot->SetAnchors(FAnchors(0, 0, 1, 1));
        Slot->SetOffsets(FMargin(0));
        Slot->SetZOrder(Z);
    }

    void Font(UTextBlock& Text, const FString& Caption, int32 Size, FLinearColor Color)
    {
        FSlateFontInfo Style = Text.GetFont();
        if (auto* Face = LoadObject<UFont>(nullptr, TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard")))
            Style.FontObject = Face;
        Style.Size = Size;
        Style.TypefaceFontName = TEXT("Regular");
        Text.SetFont(Style);
        Text.SetText(FText::FromString(Caption));
        Text.SetColorAndOpacity(Color);
        Text.SetJustification(ETextJustify::Center);
        Text.SetShadowOffset(FVector2D(0, 1));
        Text.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.75));
        Text.SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    UWidgetBlueprint* NewBlueprint(const FString& Name, UClass* Parent)
    {
        auto* Factory = NewObject<UWidgetBlueprintFactory>();
        Factory->ParentClass = Parent;
        auto* BP = Cast<UWidgetBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
            Name, Root.LeftChop(1), UWidgetBlueprint::StaticClass(), Factory));
        return BP;
    }

    bool CompileSave(UWidgetBlueprint& BP)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&BP);
        FKismetEditorUtilities::CompileBlueprint(&BP);
        if (BP.Status == BS_Error) return false;
        return Save(BP);
    }

    UClass* EnsureMarker(UTexture2D* Hex, UTexture2D* House, UTexture2D* Arrow)
    {
        if (auto* Found = Existing<UWidgetBlueprint>(TEXT("WBP_FF_MapMarker"))) return Found->GeneratedClass;
        auto* BP = NewBlueprint(TEXT("WBP_FF_MapMarker"), UFFMapOverviewMarkerWidget::StaticClass());
        if (!BP) return nullptr;
        auto* Canvas = Widget<UCanvasPanel>(*BP, TEXT("MarkerRoot"));
        Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
        BP->WidgetTree->RootWidget = Canvas;
        auto* Background = Widget<UImage>(*BP, TEXT("Background"), true);
        Background->SetBrushFromTexture(Hex);
        Background->SetColorAndOpacity(FLinearColor(0.08, 0.7, 0.57, 1));
        Stretch(*Canvas, *Background);
        auto* Icon = Widget<UImage>(*BP, TEXT("Icon"), true);
        Icon->SetBrushFromTexture(House);
        Stretch(*Canvas, *Icon, 1);
        if (!CompileSave(*BP)) return nullptr;
        auto* Defaults = BP->GeneratedClass->GetDefaultObject<UFFMapOverviewMarkerWidget>();
        Defaults->MarkerSize = FVector2D(26, 30);
        Defaults->FacilityColor = FLinearColor(0.08, 0.65, 0.52, 0.95);
        Defaults->AttractionColor = FLinearColor(0.6, 0.55, 0.23, 0.95);
        Defaults->ServiceColor = FLinearColor(0.23, 0.49, 0.62, 0.95);
        Defaults->CameraColor = FLinearColor(0.13, 0.8, 0.66, 1);
        Defaults->IconColor = FLinearColor(0.88, 0.9, 0.8, 1);
        Defaults->FacilityBrush.SetResourceObject(House);
        Defaults->AttractionBrush.SetResourceObject(House);
        Defaults->ServiceBrush.SetResourceObject(House);
        Defaults->CameraBrush.SetResourceObject(Arrow);
        // Push changed defaults into the Blueprint before recompilation/reopening.
        Save(*BP);
        return BP->GeneratedClass;
    }

    void EnsureOverview(UClass* MarkerClass, UMaterialInterface* Terrain, UMaterialInterface* Rim)
    {
        if (Existing<UWidgetBlueprint>(TEXT("WBP_FF_MapOverview")))
        {
            UE_LOG(LogFFMapAuthoring, Display, TEXT("FF_MAP_UMG: existing designer WBP preserved."));
            return;
        }
        auto* BP = NewBlueprint(TEXT("WBP_FF_MapOverview"), UFFMapOverviewWidget::StaticClass());
        if (!BP) return;
        auto* RootCanvas = Widget<UCanvasPanel>(*BP, TEXT("RootCanvas"));
        RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        BP->WidgetTree->RootWidget = RootCanvas;
        auto* Panel = Widget<UCanvasPanel>(*BP, TEXT("MapPanel"), true);
        Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        auto* PanelSlot = Place(*RootCanvas, *Panel, -20, 96, 284, 284);
        PanelSlot->SetAnchors(FAnchors(1, 0));
        PanelSlot->SetAlignment(FVector2D(1, 0));
        auto* TerrainImage = Widget<UImage>(*BP, TEXT("TerrainImage"), true);
        TerrainImage->SetBrushFromMaterial(Terrain);
        TerrainImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(*Panel, *TerrainImage, 0, 0, 284, 284);
        auto* Surface = Widget<UFFMapOverviewCanvas>(*BP, TEXT("MapSurface"), true);
        Surface->SurfaceSize = FVector2D(284, 284);
        Surface->bDrawVectorLandFallback = false;
        Surface->bDrawGroundFootprints = false;
        Surface->bDrawBoundary = false;
        Surface->bDrawViewFootprint = true;
        Surface->ViewFootprintColor = FLinearColor(0.66, 0.86, 0.79, 0.45);
        Surface->ViewFootprintThickness = 1.0f;
        Place(*Panel, *Surface, 0, 0, 284, 284, 1);
        auto* Markers = Widget<UCanvasPanel>(*BP, TEXT("MarkerLayer"), true);
        Markers->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Place(*Panel, *Markers, 0, 0, 284, 284, 2);
        auto* RimImage = Widget<UImage>(*BP, TEXT("CompassRim"));
        RimImage->SetBrushFromMaterial(Rim);
        RimImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(*Panel, *RimImage, 0, 0, 284, 284, 3);
        const FLinearColor CompassColor(0.64, 0.67, 0.61, 0.88);
        struct FDirection { const TCHAR* Name; const TCHAR* Caption; float X, Y; };
        for (const FDirection& D : { FDirection{TEXT("North"), TEXT("N"), 130, 4},
            FDirection{TEXT("East"), TEXT("E"), 257, 131}, FDirection{TEXT("South"), TEXT("S"), 130, 257},
            FDirection{TEXT("West"), TEXT("W"), 4, 131} })
        {
            auto* Text = Widget<UTextBlock>(*BP, D.Name);
            Font(*Text, D.Caption, 20, CompassColor);
            Place(*Panel, *Text, D.X, D.Y, 24, 25, 4);
        }
        if (!CompileSave(*BP)) return;
        auto* Defaults = BP->GeneratedClass->GetDefaultObject<UFFMapOverviewWidget>();
        Defaults->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Defaults->FacilityMarkerClass = MarkerClass;
        Defaults->CameraMarkerClass = MarkerClass;
        Save(*BP);
        UE_LOG(LogFFMapAuthoring, Display, TEXT("FF_MAP_UMG: saved editable WBP_FF_MapOverview and WBP_FF_MapMarker."));
    }

    void EnsureAssets()
    {
        auto* Hex = EnsureGlyph(TEXT("T_MapHex"), 0);
        auto* House = EnsureGlyph(TEXT("T_MapHouse"), 1);
        auto* Arrow = EnsureGlyph(TEXT("T_MapCamera"), 2);
        auto* Fallback = EnsureGlyph(TEXT("T_MapFallback"), 3);
        auto* Terrain = EnsureMaterial(TEXT("MapTerrain"), false, Fallback);
        auto* Rim = EnsureMaterial(TEXT("MapCompassRim"), true, Fallback);
        if (!Hex || !House || !Arrow || !Terrain || !Rim)
        {
            UE_LOG(LogFFMapAuthoring, Error, TEXT("FF_MAP_UMG: initial appearance asset creation failed."));
            return;
        }
        if (auto* Marker = EnsureMarker(Hex, House, Arrow)) EnsureOverview(Marker, Terrain, Rim);
        EnsureWholeGridButton();
    }

    // Explicit development migration for the new assets made in this task. Never run
    // during startup or EnsureAssets; designer layouts/brushes are preserved.
    void AddLocalTerrainUV()
    {
        auto* Material = Existing<UMaterial>(TEXT("M_MapTerrain"));
        if (!Material) return;
        UMaterialExpressionTextureSampleParameter2D* Sample = nullptr;
        for (UMaterialExpression* Expr : Material->GetExpressions())
        {
            if (auto* Vector = Cast<UMaterialExpressionVectorParameter>(Expr);
                Vector && Vector->ParameterName == TEXT("MapUVCenter")) return;
            if (auto* Found = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr);
                Found && Found->ParameterName == TEXT("MapTexture")) Sample = Found;
            if (auto* Radius = Cast<UMaterialExpressionScalarParameter>(Expr);
                Radius && Radius->ParameterName == TEXT("CircleRadius")) Radius->DefaultValue = 0.45f;
        }
        if (!Sample) return;
        Material->Modify();
        auto* UV = Sample->Coordinates.Expression;
        auto* Center = Expression<UMaterialExpressionVectorParameter>(*Material);
        Center->ParameterName = TEXT("MapUVCenter");
        Center->DefaultValue = FLinearColor(0.5, 0.5, 0, 0);
        auto* LocalUV = Expression<UMaterialExpressionCustom>(*Material);
        LocalUV->Description = TEXT("Player-centered local map crop");
        LocalUV->OutputType = CMOT_Float2;
        Input(*LocalUV, TEXT("UV"), UV);
        Input(*LocalUV, TEXT("Center"), Center);
        Input(*LocalUV, TEXT("Scale"), Scalar(*Material, TEXT("MapUVScale"), 1.f));
        LocalUV->Code = TEXT("return Center.rg + (UV - 0.5) * Scale;");
        Sample->Coordinates.Connect(0, LocalUV);
        Material->PostEditChange();
        Save(*Material);
        UE_LOG(LogFFMapAuthoring, Display, TEXT("FF_MAP_UMG: added local terrain UV parameters; WBP designer tree preserved."));
    }

    // An explicit, narrow migration for this task's own WBP. Keep the remaining
    // widget objects, brushes, fonts, materials, marker classes and designer tree.
    void PruneMapControls()
    {
        auto* BP = Existing<UWidgetBlueprint>(TEXT("WBP_FF_MapOverview"));
        if (!BP || !BP->WidgetTree)
        {
            UE_LOG(LogFFMapAuthoring, Error, TEXT("FF_MAP_UMG: map control migration needs the existing designer WBP."));
            return;
        }
        auto* Panel = Cast<UCanvasPanel>(BP->WidgetTree->FindWidget(TEXT("MapPanel")));
        if (!Panel)
        {
            UE_LOG(LogFFMapAuthoring, Error, TEXT("FF_MAP_UMG: minimap migration stopped because the expected map panel differs."));
            return;
        }
        auto* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot);
        if (!PanelSlot)
        {
            UE_LOG(LogFFMapAuthoring, Error, TEXT("FF_MAP_UMG: map control migration needs the original canvas slots."));
            return;
        }
        // Collect only obsolete controls/footer and their descendants.
        // The map, compass, material brushes and marker assets are untouched.
        TSet<FName> RemovedNames = {FName(TEXT("NearbyButton")), FName(TEXT("NearbyLabel")),
            FName(TEXT("OverviewButton")), FName(TEXT("OverviewLabel")),
            FName(TEXT("WholeButton")), FName(TEXT("WholeLabel")),
            FName(TEXT("GridButton")), FName(TEXT("GridLabel")), FName(TEXT("BuildControls")), FName(TEXT("ScaleLabel"))};
        TArray<UWidget*> RemovedWidgets;
        const TArray<FName> ExplicitNames = RemovedNames.Array();
        for (const FName Name : ExplicitNames)
        {
            if (UWidget* Removed = BP->WidgetTree->FindWidget(Name))
            {
                RemovedWidgets.AddUnique(Removed);
                TArray<UWidget*> Children;
                UWidgetTree::GetChildWidgets(Removed, Children);
                for (UWidget* Child : Children)
                {
                    RemovedWidgets.AddUnique(Child);
                    RemovedNames.Add(Child->GetFName());
                }
            }
        }
        BP->Modify();
        BP->WidgetTree->Modify();
        Panel->Modify();
        for (UWidget* Removed : RemovedWidgets)
        {
            Removed->Modify();
            if (UPanelWidget* Parent = Removed->GetParent()) Parent->Modify();
            BP->WidgetTree->RemoveWidget(Removed);
            Removed->Rename(nullptr, GetTransientPackage());
        }
        BP->Bindings.RemoveAll([&RemovedNames](const FDelegateEditorBinding& Binding)
        {
            return RemovedNames.Contains(FName(*Binding.ObjectName)) || RemovedNames.Contains(Binding.SourceProperty);
        });
        // Clear known orphan metadata as well, making repeated migrations safe.
        for (const FName Name : RemovedNames)
        {
            FBlueprintEditorUtils::RemoveVariableNodes(BP, Name);
            BP->OnVariableRemoved(Name);
        }
        PanelSlot->Modify();
        PanelSlot->SetSize(FVector2D(PanelSlot->GetSize().X, 284));
        if (CompileSave(*BP))
        {
            UE_LOG(LogFFMapAuthoring, Display, TEXT("FF_MAP_UMG: removed all minimap controls/footer and their metadata; preserved map/compass widgets and designer materials."));
        }
        else
        {
            UE_LOG(LogFFMapAuthoring, Error, TEXT("FF_MAP_UMG: map control migration failed to compile/save."));
        }
    }
}

class FFFMapOverviewEditorModule : public IModuleInterface
{
    IConsoleObject* EnsureCommand = nullptr;
    IConsoleObject* LocalTerrainCommand = nullptr;
    IConsoleObject* PruneControlsCommand = nullptr;
public:
    virtual void StartupModule() override
    {
        EnsureCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("FF.MapOverview.EnsureDesignerAssets"),
            TEXT("Create missing minimap WBP/material assets only. Existing designer assets are never rebuilt."),
            FConsoleCommandDelegate::CreateStatic(&FFMapAuthoring::EnsureAssets), ECVF_Default);
        LocalTerrainCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("FF.MapOverview.AddLocalTerrainUV"),
            TEXT("Add missing player-local UV parameters to this plugin's initial terrain material; preserves WBP layout."),
            FConsoleCommandDelegate::CreateStatic(&FFMapAuthoring::AddLocalTerrainUV), ECVF_Default);
        PruneControlsCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("FF.MapOverview.PruneMapControls"),
            TEXT("Remove all minimap controls/footer from this plugin's WBP; preserve map/compass widgets and all designer materials."),
            FConsoleCommandDelegate::CreateStatic(&FFMapAuthoring::PruneMapControls), ECVF_Default);
    }
    virtual void ShutdownModule() override
    {
        if (EnsureCommand) IConsoleManager::Get().UnregisterConsoleObject(EnsureCommand);
        if (LocalTerrainCommand) IConsoleManager::Get().UnregisterConsoleObject(LocalTerrainCommand);
        if (PruneControlsCommand) IConsoleManager::Get().UnregisterConsoleObject(PruneControlsCommand);
    }
};

IMPLEMENT_MODULE(FFFMapOverviewEditorModule, FF_MapOverviewEditor)
