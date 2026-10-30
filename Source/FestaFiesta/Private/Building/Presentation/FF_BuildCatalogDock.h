#pragma once

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"

namespace FFBuildCatalogDock
{
// Convert the painted catalog edge into the inspector's own coordinates. This
// works across different HUD parents and DPI scales without screen-size guesses.
inline bool Align(UUserWidget& Owner, const FGeometry& Geometry, USizeBox* Inspector,
    TWeakObjectPtr<UWidget>& CachedCatalog, const float Gap = 24.f, USizeBox* Shadow = nullptr)
{
    if (!Inspector || Geometry.GetLocalSize().X <= 1.f) return false;
    if (!CachedCatalog.IsValid())
    {
        TArray<UUserWidget*> Panels;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(&Owner, Panels, UFF_PrepPanelBuild::StaticClass(), false);
        for (UUserWidget* Panel : Panels)
            if (Panel && Panel->GetOwningPlayer() == Owner.GetOwningPlayer() && Panel->IsVisible())
            {
                CachedCatalog = Panel->GetWidgetFromName(TEXT("Size_ClarityCatalogDock"));
                break;
            }
    }
    const UWidget* Catalog = CachedCatalog.Get();
    auto* Slot = Cast<UCanvasPanelSlot>(Inspector->Slot);
    if (!Catalog || !Catalog->IsVisible() || !Slot) return false;
    const FGeometry& Bounds = Catalog->GetCachedGeometry();
    if (Bounds.GetLocalSize().X <= 1.f || Bounds.GetLocalSize().Y <= 1.f) return false;
    const FVector2D BottomRight = Geometry.AbsoluteToLocal(Bounds.LocalToAbsolute(Bounds.GetLocalSize()));
    const FVector2D Viewport = Geometry.GetLocalSize();
    const float Width = Inspector->IsWidthOverride() ? Inspector->GetWidthOverride() : Slot->GetSize().X;
    const FVector2D Position(
        FMath::Min(BottomRight.X + Gap + Width, Viewport.X - 8.0) - Viewport.X,
        FMath::Min(BottomRight.Y, Viewport.Y - 8.0) - Viewport.Y);
    // Keep bottom-right anchors so lighting-page layout and authored sizes remain valid.
    if (!Slot->GetPosition().Equals(Position, .5f)) Slot->SetPosition(Position);
    if (Shadow)
        if (auto* ShadowSlot = Cast<UCanvasPanelSlot>(Shadow->Slot))
            if (!ShadowSlot->GetPosition().Equals(Position + FVector2D(4, 4), .5f))
                ShadowSlot->SetPosition(Position + FVector2D(4, 4));
    return true;
}
}
