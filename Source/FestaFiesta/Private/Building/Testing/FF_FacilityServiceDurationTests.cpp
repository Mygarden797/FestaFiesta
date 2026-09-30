#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/AssetManager.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Queue/FF_FacilityQueue.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityServiceDuration_ContentCoverage,
    "FestaFiesta.Building.ServiceDuration.ContentCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityServiceDuration_ContentCoverage::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UAssetManager& Assets = UAssetManager::Get();
    TArray<FPrimaryAssetId> Definitions;
    Assets.GetPrimaryAssetIdList(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Definitions);
    if (!TestTrue(TEXT("Production facility definitions are registered"), !Definitions.IsEmpty())) return false;

    for (const FPrimaryAssetId& Id : Definitions)
    {
        const UFFZSFacilityDefinition* Definition = Cast<UFFZSFacilityDefinition>(Assets.GetPrimaryAssetPath(Id).TryLoad());
        if (!TestNotNull(*FString::Printf(TEXT("Facility loads: %s"), *Id.ToString()), Definition)) continue;
        const FFFZSFacilitySpec& Spec = Definition->Spec;
        const auto CheckRuntimeTiming = [this, &Id, &Spec](const int32 Level, const float DurationMultiplier)
        {
            const FFFZSQueueRules Rules = AFFZSFacilityActor::BuildEffectiveQueueRules(Spec.QueueRules, DurationMultiplier);
            TestEqual(*FString::Printf(TEXT("Level %d effective service lasts five seconds: %s"), Level, *Id.ToString()),
                Rules.ServiceDurationSeconds, 5.f);
            const FFF_QueueConfig Config = UFF_ZoneSystemQueueBridgeSubsystem::ToProjectQueueConfig(Rules, FVector::ZeroVector);
            FFF_FacilityQueue Queue;
            Queue.Configure(1, Config, FFF_QueueLaneGeometry());
            TestEqual(*FString::Printf(TEXT("Level %d runtime queue preserves five seconds per team: %s"), Level, *Id.ToString()),
                Queue.GetConfig().ServiceSecondsPerParty, 5.f);
            const int32 ActiveSlots = FMath::Min(Queue.GetConfig().ConcurrentServiceSlots, Queue.GetConfig().MaxCapacityParties);
            TestTrue(*FString::Printf(TEXT("Level %d runtime throughput is twelve teams per minute per active slot: %s"), Level, *Id.ToString()),
                FMath::IsNearlyEqual(Queue.GetServiceRate() * 60.f, ActiveSlots * 12.f));
        };
        CheckRuntimeTiming(1, 1.f);
        for (const FFFZSFacilityUpgradeTier& Tier : Spec.UpgradeTiers)
        {
            CheckRuntimeTiming(Tier.TargetLevel, Tier.ServiceDurationMultiplier);
        }
    }
    return true;
}

#endif
