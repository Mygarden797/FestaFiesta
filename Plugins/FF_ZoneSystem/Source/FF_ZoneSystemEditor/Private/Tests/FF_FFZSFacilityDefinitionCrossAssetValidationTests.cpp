#include "FF_FFZSFacilityDefinitionCrossAssetValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "FF_FFZSFacilityDefinitionValidator.h"
#include "Misc/AutomationTest.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSFacilityDefinitionCrossAssetValidationTest,
    "FF_ZoneSystem.Editor.FacilityDefinitionCrossAssetValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityDefinitionCrossAssetValidationTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    using namespace FFZSFacilityDefinitionCrossAssetValidation;

    UEditorValidatorSubsystem* ValidatorSubsystem =
        GEditor ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() : nullptr;
    TestNotNull(
        TEXT("The Editor Data Validation subsystem is available"),
        ValidatorSubsystem);
    if (ValidatorSubsystem)
    {
        bool bFoundNativeValidator = false;
        ValidatorSubsystem->ForEachEnabledValidator(
            [&bFoundNativeValidator](UEditorValidatorBase* Validator)
            {
                bFoundNativeValidator |=
                    Validator && Validator->IsA<UFFZSFacilityDefinitionValidator>();
                return true;
            });
        TestTrue(
            TEXT("The native Facility Definition validator is automatically registered"),
            bFoundNativeValidator);
    }

    TestTrue(
        TEXT("The required root itself is accepted"),
        IsWithinRequiredDefinitionRoot(
            FName(RequiredDefinitionRoot)));
    TestTrue(
        TEXT("A descendant of the required root is accepted"),
        IsWithinRequiredDefinitionRoot(
            TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Food")));
    TestFalse(
        TEXT("A prefix lookalike outside the root is rejected"),
        IsWithinRequiredDefinitionRoot(
            TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitionsBackup")));

    TestTrue(
        TEXT("Finite valid bounds pass"),
        HasFiniteBounds(FBox(FVector(-1.0), FVector(1.0))));
    TestFalse(
        TEXT("Invalid bounds fail"),
        HasFiniteBounds(FBox(ForceInit)));
    TestFalse(
        TEXT("Infinite bounds fail"),
        HasFiniteBounds(FBox(
            FVector(-1.0),
            FVector(std::numeric_limits<double>::infinity(), 1.0, 1.0))));

    FFFZSFacilityAssetIdentity Current;
    Current.AssetPath = FSoftObjectPath(TEXT("/Game/Defs/A.A"));
    Current.FacilityDefinitionId = TEXT("SharedDefinition");
    Current.PrimaryAssetId = FPrimaryAssetId(
        TEXT("FFZSFacilityDefinition"),
        TEXT("SharedPrimary"));

    TArray<FFFZSFacilityAssetIdentity> Others;
    Others.Add({
        FSoftObjectPath(TEXT("/Game/Defs/Z.Z")),
        TEXT("SharedDefinition"),
        FPrimaryAssetId(TEXT("FFZSFacilityDefinition"), TEXT("OtherPrimary"))});
    Others.Add({
        FSoftObjectPath(TEXT("/Game/Defs/B.B")),
        TEXT("OtherDefinition"),
        FPrimaryAssetId(TEXT("FFZSFacilityDefinition"), TEXT("SharedPrimary"))});
    Others.Add(Current);

    const FFFZSFacilityIdentityCollisions Collisions =
        FindIdentityCollisions(Current, Others);
    TestEqual(
        TEXT("One duplicate FacilityDefinitionId is found"),
        Collisions.DuplicateDefinitionIdAssets.Num(),
        1);
    TestEqual(
        TEXT("The current asset is excluded from ID collisions"),
        Collisions.DuplicateDefinitionIdAssets[0].ToString(),
        FString(TEXT("/Game/Defs/Z.Z")));
    TestEqual(
        TEXT("One duplicate PrimaryAssetId is found"),
        Collisions.DuplicatePrimaryAssetIdAssets.Num(),
        1);
    TestEqual(
        TEXT("The PrimaryAssetId collision path is stable"),
        Collisions.DuplicatePrimaryAssetIdAssets[0].ToString(),
        FString(TEXT("/Game/Defs/B.B")));
    return true;
}

#endif
