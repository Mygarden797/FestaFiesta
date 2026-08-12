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
    FReply ResizeQueueTailToCapacity();
    FReply SetCapacityFromQueueLength();
    FReply FitVisualToFootprint();

    bool HasAnyEditableDefinition() const;
    bool CanSetFootprintFromLoadedVisualMesh() const;
    bool CanResizeQueueTailToCapacity() const;
    bool CanSetCapacityFromQueueLength() const;
    bool CanFitVisualToFootprint() const;
    FText GetVisualSizingSummary() const;

    template <typename MutationType>
    int32 MutateDefinitions(const FText& TransactionDescription, MutationType&& Mutation);

    static void ShowNotification(const FText& Message, bool bSuccess);

    TArray<TWeakObjectPtr<UFFZSFacilityDefinition>> Definitions;
};
