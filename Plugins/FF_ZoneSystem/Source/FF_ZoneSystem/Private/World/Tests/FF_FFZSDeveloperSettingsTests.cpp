// FF_FFZSDeveloperSettingsTests.cpp
// 역할: 설치 판정과 표시 격자가 공유하는 지면 설정이 잘못된 ini 값에서도 안전한지 검증한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSDeveloperSettings.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSGroundSettingsResolutionTest,
    "FF_ZoneSystem.Unit.Settings.GroundValidationResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSGroundSettingsResolutionTest::RunTest(const FString& Parameters)
{
    // 전역 CDO를 변경하지 않는 개별 설정 객체로 유효 값과 잘못된 값의 계약을 함께 확인한다.
    UFFZSDeveloperSettings* Settings = NewObject<UFFZSDeveloperSettings>();
    Settings->GroundSampleSpacingCm = 25.0f;
    Settings->GroundTraceAboveCm = 1800.0f;
    Settings->GroundTraceBelowCm = 1200.0f;
    Settings->MaxGroundSlopeDegrees = 12.0f;
    Settings->MaxGroundHeightVarianceCm = 30.0f;
    FFFZSGroundValidationSettings Resolved = Settings->ResolveGroundValidationSettings();
    TestEqual(TEXT("Authored sample spacing is preserved"), Resolved.SampleSpacingCm, 25.0);
    TestEqual(TEXT("Authored upper trace is preserved"), Resolved.TraceAboveCm, 1800.0);
    TestEqual(TEXT("Authored lower trace is preserved"), Resolved.TraceBelowCm, 1200.0);
    TestEqual(TEXT("Authored slope is preserved"), Resolved.MaxSlopeDegrees, 12.0);
    TestEqual(TEXT("Authored height variation is preserved"), Resolved.MaxHeightVarianceCm, 30.0);
    TestFalse(TEXT("Legacy callers retain normal facility support height"), Resolved.bUseHighestSupport);

    Settings->GroundSampleSpacingCm = std::numeric_limits<float>::quiet_NaN();
    Settings->GroundTraceAboveCm = std::numeric_limits<float>::infinity();
    Settings->GroundTraceBelowCm = -2.0f;
    Settings->MaxGroundSlopeDegrees = 100.0f;
    Settings->MaxGroundHeightVarianceCm = -1.0f;
    Resolved = Settings->ResolveGroundValidationSettings();
    TestEqual(TEXT("NaN spacing uses the documented default"), Resolved.SampleSpacingCm,
        static_cast<double>(UFFZSDeveloperSettings::DefaultGroundSampleSpacingCm));
    TestEqual(TEXT("Infinite trace uses the documented default"), Resolved.TraceAboveCm,
        static_cast<double>(UFFZSDeveloperSettings::DefaultGroundTraceDistanceCm));
    TestTrue(TEXT("Reversed trace cannot escape below the minimum distance"), Resolved.TraceBelowCm >= 1.0);
    TestTrue(TEXT("Slope obeys the same range as Project Settings"), Resolved.MaxSlopeDegrees <= 89.0);
    TestEqual(TEXT("Negative height variation becomes zero"), Resolved.MaxHeightVarianceCm, 0.0);
    TestTrue(TEXT("Resolving never silently rewrites the authored config"),
        FMath::IsNaN(Settings->GroundSampleSpacingCm) && Settings->GroundTraceBelowCm == -2.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSTileGroundSettingsResolutionTest,
    "FF_ZoneSystem.Unit.Settings.TileGroundValidationResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSTileGroundSettingsResolutionTest::RunTest(const FString& Parameters)
{
    UFFZSDeveloperSettings* Settings = NewObject<UFFZSDeveloperSettings>();
    Settings->MaxGroundSlopeDegrees = 12.0f;
    Settings->MaxGroundHeightVarianceCm = 30.0f;
    Settings->MaxTileGroundSlopeDegrees = 75.0f;
    Settings->MaxTileGroundHeightVarianceCm = 650.0f;
    Settings->GroundSampleSpacingCm = 50.0f;
    Settings->GroundTraceAboveCm = 900.0f;
    Settings->GroundTraceBelowCm = 800.0f;

    const FFFZSGroundValidationSettings Road = Settings->ResolveGroundValidationSettings(EFFZSFacilityCategory::Road);
    FFFZSGroundValidationSettings Tile = Settings->ResolveGroundValidationSettings(EFFZSFacilityCategory::Tile);
    TestEqual(TEXT("Road retains the regular slope limit"), Road.MaxSlopeDegrees, 12.0);
    TestEqual(TEXT("Road retains the regular height difference limit"), Road.MaxHeightVarianceCm, 30.0);
    TestFalse(TEXT("Road does not inherit tile support height"), Road.bUseHighestSupport);
    TestEqual(TEXT("Tile uses its independent slope limit"), Tile.MaxSlopeDegrees, 75.0);
    TestEqual(TEXT("Tile uses its independent height difference limit"), Tile.MaxHeightVarianceCm, 650.0);
    TestTrue(TEXT("Only tile resolves to highest ground support"), Tile.bUseHighestSupport);
    TestEqual(TEXT("Both policies share the configured sample spacing"), Tile.SampleSpacingCm, Road.SampleSpacingCm);
    TestEqual(TEXT("Both policies share the upper trace band"), Tile.TraceAboveCm, Road.TraceAboveCm);
    TestEqual(TEXT("Both policies share the lower trace band"), Tile.TraceBelowCm, Road.TraceBelowCm);

    Settings->MaxTileGroundSlopeDegrees = std::numeric_limits<float>::quiet_NaN();
    Settings->MaxTileGroundHeightVarianceCm = std::numeric_limits<float>::infinity();
    Tile = Settings->ResolveGroundValidationSettings(EFFZSFacilityCategory::Tile);
    TestEqual(TEXT("Non-finite tile slope uses the tile default"), Tile.MaxSlopeDegrees,
        static_cast<double>(UFFZSDeveloperSettings::DefaultMaxTileGroundSlopeDegrees));
    TestEqual(TEXT("Non-finite tile variance uses the tile default"), Tile.MaxHeightVarianceCm,
        static_cast<double>(UFFZSDeveloperSettings::DefaultMaxTileGroundHeightVarianceCm));
    Settings->MaxTileGroundSlopeDegrees = 100.0f;
    Settings->MaxTileGroundHeightVarianceCm = -1.0f;
    Tile = Settings->ResolveGroundValidationSettings(EFFZSFacilityCategory::Tile);
    TestEqual(TEXT("Tile slope obeys the Project Settings upper limit"), Tile.MaxSlopeDegrees, 89.0);
    TestEqual(TEXT("Negative tile variance is clamped"), Tile.MaxHeightVarianceCm, 0.0);
    TestEqual(TEXT("Sanitizing tile rules cannot change ordinary facility rules"),
        Settings->ResolveGroundValidationSettings().MaxHeightVarianceCm, 30.0);
    return true;
}

#endif
