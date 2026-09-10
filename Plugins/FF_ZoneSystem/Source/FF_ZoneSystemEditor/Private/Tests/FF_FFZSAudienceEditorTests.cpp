#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "FF_FFZSFacilityEditorHelpers.h"
#include "FF_FFZSTypes.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceEditorTest, "FF_ZoneSystem.Audience.EditorResizeAndTooltips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSAudienceEditorTest::RunTest(const FString& Parameters)
{
    FFFZSFacilitySpec Spec;
    Spec.FootprintSizeCells = FIntPoint(1200, 2000);
    Spec.Audience.bEnabled = true;
    Spec.Audience.FirstSlotLocalCm = FVector(-760, 1400, 0);
    Spec.Audience.EntryLocalCm = FVector(-850, 2570, 0);
    Spec.Audience.ExitLocalCm = FVector(850, 2570, 0);
    Spec.Audience.FocusLocalCm = FVector(0, 500, 200);
    FFFZSFacilitySpec Resized;
    TestTrue(TEXT("Authoring resize accepts audience layout"),
        FFFZSFacilityEditorHelpers::TryResizeFootprintKeepingLayout(Spec, FIntPoint(1500, 2600), Resized));
    TestTrue(TEXT("All seats move together, preserving their offset from the stage front"),
        Resized.Audience.FirstSlotLocalCm.Equals(FVector(-760, 1700, 0)));
    TestTrue(TEXT("Entrance follows the grid"), Resized.Audience.EntryLocalCm.Equals(FVector(-850, 2870, 0)));
    TestEqual(TEXT("Authoring resize does not stretch people spacing"), Resized.Audience.ColumnSpacingCm, 120.f);
    TArray<FText> Errors;
    TestTrue(TEXT("Resized audience still validates"), Resized.Audience.IsValid(Errors));
    for (TFieldIterator<FProperty> It(FFFZSAudienceLayout::StaticStruct()); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_Edit))
        {
            TestFalse(*FString::Printf(TEXT("%s has visible label"), *It->GetName()), It->GetMetaData(TEXT("DisplayName")).IsEmpty());
            const FString Tip = It->GetMetaData(TEXT("ToolTip"));
            bool Korean = false;
            for (const TCHAR Ch : Tip) Korean |= Ch >= 0xAC00 && Ch <= 0xD7A3;
            TestTrue(*FString::Printf(TEXT("%s has Korean help"), *It->GetName()), Korean);
            TestTrue(*FString::Printf(TEXT("%s is saved"), *It->GetName()), It->HasAnyPropertyFlags(CPF_SaveGame));
        }
    }
    return true;
}
#endif
