// FF_FFZSBuildRangeComponent.cpp
// 역할: 일반 렌더 경로에서 픽셀 두께와 어두운 테두리를 유지해 밝은 바닥에서도 범위를 읽게 한다.
#include "FF_FFZSBuildRangeComponent.h"

#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"

namespace
{
    class FFFZSBuildRangeSceneProxy final : public FPrimitiveSceneProxy
    {
    public:
        explicit FFFZSBuildRangeSceneProxy(const UFFZSBuildRangeComponent* Component)
            : FPrimitiveSceneProxy(Component), Lines(Component->BatchedLines)
        {
            bWillEverBeLit = false;
        }

        virtual SIZE_T GetTypeHash() const override
        {
            static size_t TypeIdentity;
            return reinterpret_cast<size_t>(&TypeIdentity);
        }

        virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
            const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
        {
            for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
            {
                if ((VisibilityMap & (1u << ViewIndex)) == 0) continue;
                // The production PDI avoids the debug compositor's occluded-line dithering.
                FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
                // Lay down every underlay first so a crossing border never erases a primary color.
                for (const FBatchedLine& Line : Lines)
                {
                    const FLinearColor Underlay(0.012f, 0.022f, 0.035f, 0.88f * Line.Color.A);
                    PDI->DrawTranslucentLine(Line.Start, Line.End, Underlay,
                        Line.DepthPriority, Line.Thickness + 2.0f, 0.0f, true);
                }
                for (const FBatchedLine& Line : Lines)
                {
                    PDI->DrawTranslucentLine(Line.Start, Line.End, Line.Color,
                        Line.DepthPriority, Line.Thickness, 0.0f, true);
                }
            }
        }

        virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
        {
            FPrimitiveViewRelevance Relevance;
            Relevance.bDrawRelevance = IsShown(View);
            Relevance.bDynamicRelevance = true;
            Relevance.bSeparateTranslucency = Relevance.bNormalTranslucency = true;
            return Relevance;
        }

        virtual uint32 GetMemoryFootprint() const override
        {
            return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize();
        }

    private:
        const TArray<FBatchedLine> Lines;
    };
}

FPrimitiveSceneProxy* UFFZSBuildRangeComponent::CreateSceneProxy()
{
    return new FFFZSBuildRangeSceneProxy(this);
}

UFFZSBuildRangeComponent::UFFZSBuildRangeComponent()
{
    SetAbsolute(true, true, true);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    PrimaryComponentTick.bCanEverTick = false;
    bCalculateAccurateBounds = true;
}
