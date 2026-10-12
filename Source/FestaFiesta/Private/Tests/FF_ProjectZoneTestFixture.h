// Shared zone contract for isolated FestaFiesta integration tests.
#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace FFProjectZoneTest
{
    // Keep synthetic worlds on an explicit, currently supported project ID.
    // Applying a preset sets identity metadata only: each test retains its own
    // geometry and real registration/placement rules. Do not disable the ID
    // restriction or pick the first configured ID to hide a retired fixture.
    inline FName ZoneId()
    {
        return FFZSTest::ProjectZoneId();
    }

    inline bool RegisterZone(FAutomationTestBase& Test, AFFZSGameplayZone* Zone,
        UFFZSWorldSubsystem* Zones)
    {
        if (!Test.TestNotNull(TEXT("Project zone fixture actor exists"), Zone) ||
            !Test.TestNotNull(TEXT("Project zone fixture authority exists"), Zones))
        {
            return false;
        }

        const FName Id = ZoneId();
        const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
        if (!Test.TestTrue(*FString::Printf(
                TEXT("Fixture ID %s is explicitly listed in project ProjectZoneIds"), *Id.ToString()),
                Settings && Settings->ProjectZoneIds.Contains(Id)) ||
            !Test.TestTrue(*FString::Printf(TEXT("Project zone preset %s applies"), *Id.ToString()),
                Zone->ApplyAcceptedZonePreset(Id)))
        {
            return false;
        }

        TArray<FText> Errors;
        if (!Zone->ValidateZoneData(Errors))
        {
            Test.AddError(FString::Printf(TEXT("Project zone fixture %s failed validation"), *Id.ToString()));
            for (const FText& Error : Errors)
            {
                Test.AddError(Error.ToString());
            }
            return false;
        }
        return Test.TestTrue(*FString::Printf(TEXT("Project zone fixture %s registers"), *Id.ToString()),
            Zones->RegisterZone(Zone));
    }
}

#endif
