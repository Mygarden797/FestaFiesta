// FF_FFZSFacilityDefinitionCustomization.h
// 역할: 시설 데이터 에셋의 상세 창을 편집 목적별로 정리하고 디자이너용 버튼과 미리보기를 제공한다.
#pragma once

#include "IDetailCustomization.h"

class UFFZSFacilityDefinition;

/** Adds safe, transactional facility-authoring actions above the ordinary Spec properties. */
class FFFZSFacilityDefinitionCustomization final : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    FReply ShowKoreanHelp();
    FReply ValidateDefinitions();
    FReply SetFootprintFromLoadedVisualMesh();
    FReply ResizeFacilityKeepingLayout();
    FReply BuildQueueRectsFromPath();
    FReply ResizeQueueTailToCapacity();
    FReply SetCapacityFromQueueLength();
    FReply FitVisualToFootprint();
    FReply PreviewFacilityLevels();
    FReply ArrangeAudienceLayout();

    bool HasAnyEditableDefinition() const;
    bool CanSetFootprintFromLoadedVisualMesh() const;
    bool CanResizeFacilityKeepingLayout() const;
    bool CanBuildQueueRectsFromPath() const;
    bool CanResizeQueueTailToCapacity() const;
    bool CanSetCapacityFromQueueLength() const;
    bool CanFitVisualToFootprint() const;
    FText GetVisualSizingSummary() const;

    TOptional<int32> GetPendingFootprintWidthCells() const { return PendingFootprintWidthCells; }
    TOptional<int32> GetPendingFootprintDepthCells() const { return PendingFootprintDepthCells; }
    void SetPendingFootprintWidthCells(int32 NewValue) { PendingFootprintWidthCells = NewValue; }
    void SetPendingFootprintDepthCells(int32 NewValue) { PendingFootprintDepthCells = NewValue; }

    TOptional<float> GetPendingObstacleHeightCm() const { return PendingObstacleHeightCm; }
    void SetPendingObstacleHeightCm(float NewValue) { PendingObstacleHeightCm = NewValue; }

    TOptional<int32> GetPendingQueueMarginCm() const { return PendingQueueMarginCm; }
    void SetPendingQueueMarginCm(int32 NewValue) { PendingQueueMarginCm = NewValue; }

    template <typename MutationType>
    int32 MutateDefinitions(const FText& TransactionDescription, MutationType&& Mutation);

    static void ShowNotification(const FText& Message, bool bSuccess);

    TArray<TWeakObjectPtr<UFFZSFacilityDefinition>> Definitions;

    // 크기 보정 버튼의 목표 크기 입력값. Details 패널 수명 동안만 유지하며 에셋에는 저장하지 않는다.
    // 높이는 XY 배치에 영향을 주지 않지만 같은 한 번의 조작으로 바꾸도록 함께 받는다.
    TOptional<int32> PendingFootprintWidthCells;
    TOptional<int32> PendingFootprintDepthCells;
    TOptional<float> PendingObstacleHeightCm;

    // 대기열 회랑을 다시 만들 때 경로 양옆에 확보할 여백. 시설마다 통행 폭이 달라 입력으로 받는다.
    TOptional<int32> PendingQueueMarginCm = 50;
};
