#pragma once

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"

namespace FFBuildCatalogDock
{
// 카탈로그와 정보 패널이 서로 다른 HUD에 있어도 절대 좌표를 거쳐 같은 좌표계로 정렬한다.
// 패널 크기나 DPI를 고정값으로 추측하지 않는다. 캐시는 약한 참조라 HUD 수명을 늘리지 않는다.
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
    // 제작한 조명 페이지의 크기를 보존하도록 오른쪽 아래 앵커는 유지하고 위치만 갱신한다.
    if (!Slot->GetPosition().Equals(Position, .5f)) Slot->SetPosition(Position);
    if (Shadow)
        if (auto* ShadowSlot = Cast<UCanvasPanelSlot>(Shadow->Slot))
            if (!ShadowSlot->GetPosition().Equals(Position + FVector2D(4, 4), .5f))
                ShadowSlot->SetPosition(Position + FVector2D(4, 4));
    return true;
}
}
