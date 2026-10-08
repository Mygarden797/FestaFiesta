// FF_FFZSAudienceCapacityTests.cpp
// 역할: 에디터에서 관객 수용량이 관람 영역 변경을 따라 갱신되는지 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FF_FFZSAudienceCapacityEditor.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityEditorHelpers.h"
#include "FF_FFZSGridLibrary.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
    const TCHAR* SunsetPath = TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage.PDA_FFZS_Performance_MainStage_SunsetMainStage");

    // Pure fixture: facility authors can change the real PDA's population, size or area limit
    // without changing the expected outcome of these geometry/transaction regression tests.
    FFFZSFacilitySpec MakeCapacitySpec()
    {
        FFFZSFacilitySpec Spec;
        Spec.FacilityDefinitionId = TEXT("AudienceCapacityFixture");
        Spec.DisplayName = FText::FromString(TEXT("Audience Capacity Fixture"));
        Spec.FootprintSizeCells = FIntPoint(1200, 2000);
        Spec.ClearanceCells = 100;
        Spec.QueueReservedRects = { FFFZSCellRect(FIntPoint(1100, 2000), FIntPoint(1200, 2100)) };
        Spec.QueueSplinePointsCm = { FVector(550, 1050, 0), FVector(550, 1050, 0) };
        Spec.QueueRules.MaxWaitingAgents = 1;
        Spec.QueueRules.ServiceCapacity = 1;
        const auto AddAnchor = [&](EFFZSAnchorType Type, FVector Point)
        {
            FFFZSAnchorDefinition& Anchor = Spec.Anchors.AddDefaulted_GetRef();
            Anchor.Type = Type;
            Anchor.LocalTransform.SetTranslation(Point);
        };
        AddAnchor(EFFZSAnchorType::Entry, FVector(750, 1050, 0));
        AddAnchor(EFFZSAnchorType::Service, FVector(625, 1000, 0));
        AddAnchor(EFFZSAnchorType::Exit, FVector(-550, 1050, 0));
        Spec.Audience.bEnabled = true;
        Spec.Audience.FirstSlotLocalCm = FVector(-760, 1400, 0);
        Spec.Audience.EntryLocalCm = FVector(-850, 2570, 0);
        Spec.Audience.ExitLocalCm = FVector(850, 2570, 0);
        Spec.Audience.FocusLocalCm = FVector(0, 500, 200);
        return Spec;
    }

    void EditAudienceProperty(UFFZSFacilityDefinition& Asset, FName Name, double Value,
        EPropertyChangeType::Type ChangeType = EPropertyChangeType::ValueSet)
    {
        FProperty* Member = FindFProperty<FProperty>(UFFZSFacilityDefinition::StaticClass(), GET_MEMBER_NAME_CHECKED(UFFZSFacilityDefinition, Spec));
        FProperty* Audience = FindFProperty<FProperty>(FFFZSFacilitySpec::StaticStruct(), GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Audience));
        FProperty* Leaf = FindFProperty<FProperty>(FFFZSAudienceLayout::StaticStruct(), Name);
        FEditPropertyChain Chain;
        Chain.AddTail(Member);
        Chain.AddTail(Audience);
        Chain.AddTail(Leaf);
        Chain.SetActiveMemberPropertyNode(Member);
        Chain.SetActivePropertyNode(Leaf);
        Asset.PreEditChange(Chain);
        if (FIntProperty* IntProperty = CastField<FIntProperty>(Leaf))
            IntProperty->SetPropertyValue_InContainer(&Asset.Spec.Audience, static_cast<int32>(Value));
        else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Leaf))
            FloatProperty->SetPropertyValue_InContainer(&Asset.Spec.Audience, static_cast<float>(Value));
        FPropertyChangedEvent Event(Leaf, ChangeType);
        Event.SetActiveMemberProperty(Member);
        FPropertyChangedChainEvent ChainEvent(Chain, Event);
        Asset.PostEditChangeChainProperty(ChainEvent);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceAutoFitTest, "FF_ZoneSystem.Audience.CapacityAutoFit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSAudienceAutoFitTest::RunTest(const FString& Parameters)
{
    // Keep the repair threshold independent of production tuning and retired clearance.
    // The Lv.2 body/Queue fits 1,000 m²; adding its 610 m² audience requires a separate allowance.
    TGuardValue<int32> Budget(GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility, 10'000'000);
    FFFZSFacilitySpec Spec = MakeCapacitySpec();
    FFFZSAudienceLayout Fitted;
    TArray<FText> Errors;
    Spec.Audience.Columns = 20;
    Spec.Audience.Rows = 15;
    TestTrue(TEXT("Changed count fits without manually editing portals"),
        FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors));
    TestEqual(TEXT("Requested count is retained exactly"), Fitted.GetCapacity(), 300);
    Spec.Audience = Fitted;
    TestTrue(TEXT("Corrected base spec is valid"), Spec.IsValid(Errors));
    TestTrue(TEXT("Fitting is idempotent"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors));
    TestTrue(TEXT("Already corrected geometry does not drift"), FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&Spec.Audience, &Fitted, 0));

    Spec.Audience.Columns = 1;
    Spec.Audience.Rows = 1;
    Spec.Audience.ColumnSpacingCm = 1;
    Spec.Audience.RowSpacingCm = 1;
    Spec.Audience.PerimeterMarginCm = 1;
    Spec.Audience.FirstSlotLocalCm.Y = 0;
    TestTrue(TEXT("Single person still gets distinct valid entry and exit"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors));
    TestEqual(TEXT("One remains one person"), Fitted.GetCapacity(), 1);
    TestTrue(TEXT("Unsafe spacing is repaired"), Fitted.ColumnSpacingCm >= 120.f && Fitted.RowSpacingCm >= 120.f);
    TestTrue(TEXT("Stage overlap is repaired"), Fitted.FirstSlotLocalCm.Y > Spec.FootprintSizeCells.Y * .5 + Fitted.PerimeterMarginCm);

    Spec = MakeCapacitySpec();
    Spec.Audience.FirstSlotLocalCm.Y = 1400;
    // A valid existing Queue can occupy space exposed by a wider audience grid.
    Spec.QueueReservedRects.Add(FFFZSCellRect(FIntPoint(1300, 2200), FIntPoint(1600, 2600)));
    Spec.Audience.Columns = 20;
    TestTrue(TEXT("Wider grid clears existing Queue geometry"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors));
    TestTrue(TEXT("Grid moves past overlapping reservation"), Fitted.FirstSlotLocalCm.Y - Fitted.PerimeterMarginCm > 1600.);
    Spec.Audience = Fitted;
    TestTrue(TEXT("Queue and audience validate together"), Spec.IsValid(Errors));

    Spec = MakeCapacitySpec();
    Spec.Audience.AreaLimitM2 = 1;
    TArray<FText> Adjustments;
    TestTrue(TEXT("Insufficient explicit area is repaired without changing count"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors, &Adjustments));
    TestEqual(TEXT("Area grows to minimum whole square metres"), Fitted.AreaLimitM2, 271.f);
    TestEqual(TEXT("Requested people remain unchanged"), Fitted.GetCapacity(), 120);
    TestTrue(TEXT("Repair explains the area change and its reason"), Adjustments.ContainsByPredicate([](const FText& Note)
        { return Note.ToString().Contains(TEXT("Audience Area Limit")) && Note.ToString().Contains(TEXT("이유")); }));
    const FFFZSAudienceLayout Sentinel = Fitted;
    Spec.Audience.Rows = 513;
    TestFalse(TEXT("Unsupported dimensions are explained, not silently clamped"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors, &Adjustments));
    TestTrue(TEXT("Failure leaves output unchanged"), FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&Sentinel, &Fitted, 0));

    Spec = MakeCapacitySpec();
    Spec.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier LargeTier;
    LargeTier.TargetLevel = 2;
    LargeTier.SizeMultiplier = 1.5f;
    Spec.UpgradeTiers.Add(LargeTier);
    TestTrue(TEXT("Area repair covers largest enabled level"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors, &Adjustments));
    TestEqual(TEXT("Uses upgraded quantized area rather than base area"), Fitted.AreaLimitM2, 610.f);
    Spec.Audience = Fitted;
    TestTrue(TEXT("Largest level passes with repaired allowance"), FFZSFacilityUpgradeRules::BuildEffectiveSpec(Spec, 2, 1.5f).IsValid(Errors));

    Spec = MakeCapacitySpec();
    Spec.Audience.Columns = 128;
    Spec.Audience.Rows = 64;
    TestTrue(TEXT("Maximum supported population can fit with automatic area allowance"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors));
    TestEqual(TEXT("Full supported population retained"), Fitted.GetCapacity(), 8192);
    Spec.Audience = Fitted;
    TestTrue(TEXT("Repeat repair succeeds"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors, &Adjustments));
    TestTrue(TEXT("Already valid settings have no spurious repair explanations"), Adjustments.IsEmpty());

    Spec = MakeCapacitySpec();
    Spec.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier& Tier = Spec.UpgradeTiers.AddDefaulted_GetRef();
    Tier.TargetLevel = 2;
    Tier.SizeMultiplier = .5f;
    Spec.Audience.AreaLimitM2 = 2000;
    TestTrue(TEXT("Fitting accounts for the smallest enabled upgrade"), FFFZSFacilityEditorHelpers::TryFitAudienceAfterCapacityEdit(Spec, Fitted, Errors));
    Fitted.ApplySizeMultiplier(.5f);
    TestTrue(TEXT("People and portals remain separated after upgrade shrink"), Fitted.IsValid(Errors));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceCapacityPropertyTest, "FF_ZoneSystem.Audience.CapacityPropertyEditsAndUndo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSAudienceCapacityPropertyTest::RunTest(const FString& Parameters)
{
    UFFZSFacilityDefinition* Source = LoadObject<UFFZSFacilityDefinition>(nullptr, SunsetPath);
    if (!TestNotNull(TEXT("Source definition"), Source) || !TestNotNull(TEXT("Editor"), GEditor)) return false;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().SearchAllAssets(true);
    TArray<FText> SourceErrors, SourceWarnings;
    UEditorValidatorSubsystem* Validator = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
    TestTrue(TEXT("Real PDA passes full validation without being modified"), Validator && Validator->IsObjectValid(
        Source, SourceErrors, SourceWarnings, EDataValidationUsecase::Manual) == EDataValidationResult::Valid);
    // Validate production content under its own budget above, then isolate the synthetic fixture.
    // At 50 rows its 1,174 m² audience exceeds this shared limit and exercises automatic repair.
    TGuardValue<int32> Budget(GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility, 10'000'000);
    // Unsaved, uniquely identified test object in the required validator root. Never edits the PDA.
    const FString Name = TEXT("AudienceCapacityTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/") + Name));
    UFFZSFacilityDefinition* Asset = NewObject<UFFZSFacilityDefinition>(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
    Asset->Spec = MakeCapacitySpec();
    Asset->Spec.FacilityDefinitionId = FName(*Name);
    const FFFZSFacilitySpec Original = Asset->Spec;
    const FName Rows = GET_MEMBER_NAME_CHECKED(FFFZSAudienceLayout, Rows);
    const FName Columns = GET_MEMBER_NAME_CHECKED(FFFZSAudienceLayout, Columns);
    const FName Limit = GET_MEMBER_NAME_CHECKED(FFFZSAudienceLayout, AreaLimitM2);
    const int32 ChangedRows = Original.Audience.Rows == 15 ? 14 : 15;
    {
        FScopedTransaction Transaction(NSLOCTEXT("FFZSAudienceTest", "Capacity", "Audience count automation"));
        EditAudienceProperty(*Asset, Rows, ChangedRows, EPropertyChangeType::Interactive);
        TestTrue(TEXT("Drag does not move portals or run full validation"), Asset->Spec.Audience.EntryLocalCm.Equals(Original.Audience.EntryLocalCm));
        EditAudienceProperty(*Asset, Rows, ChangedRows);
    }
    TestEqual(TEXT("Property commit keeps requested rows"), Asset->Spec.Audience.Rows, ChangedRows);
    TestTrue(TEXT("Property commit updates portals"), !Asset->Spec.Audience.EntryLocalCm.Equals(Original.Audience.EntryLocalCm));
    TestTrue(TEXT("Full validation ran after automatic fitting"), FFFZSAudienceCapacityEditor::GetStatus(Asset).ToString().Contains(TEXT("전체 검증 통과")));
    const FFFZSAudienceLayout After = Asset->Spec.Audience;
    TestTrue(TEXT("One undo is available"), GEditor->UndoTransaction());
    TestTrue(TEXT("One undo restores count and all dependent fields"), FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&Original.Audience, &Asset->Spec.Audience, 0));
    TestTrue(TEXT("One redo is available"), GEditor->RedoTransaction());
    TestTrue(TEXT("Redo restores the validated geometry"), FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&After, &Asset->Spec.Audience, 0));

    EditAudienceProperty(*Asset, Rows, 50);
    TestEqual(TEXT("Insufficient default area no longer discards requested rows"), Asset->Spec.Audience.Rows, 50);
    TestEqual(TEXT("Area is automatically raised to requested geometry"), Asset->Spec.Audience.AreaLimitM2, 1174.f);
    TestTrue(TEXT("Status tells the user why and how the limit changed"), FFFZSAudienceCapacityEditor::GetStatus(Asset).ToString().Contains(TEXT("Audience Area Limit: 0 →")));
    EditAudienceProperty(*Asset, Limit, 2000);
    TestEqual(TEXT("User can raise only this audience area allowance"), Asset->Spec.Audience.AreaLimitM2, 2000.f);
    EditAudienceProperty(*Asset, Rows, 50);
    TestEqual(TEXT("Larger count passes after area is raised"), Asset->Spec.Audience.Rows, 50);
    TestTrue(TEXT("Full cross-asset validation still passes"), FFFZSAudienceCapacityEditor::GetStatus(Asset).ToString().Contains(TEXT("전체 검증 통과")));
    EditAudienceProperty(*Asset, Limit, 1);
    TestEqual(TEXT("Too-small area is corrected to required area"), Asset->Spec.Audience.AreaLimitM2, 1174.f);
    TestEqual(TEXT("Area correction keeps requested population"), Asset->Spec.Audience.Rows, 50);
    const FFFZSAudienceLayout Larger = Asset->Spec.Audience;
    EditAudienceProperty(*Asset, Columns, 512);
    TestTrue(TEXT("Exceeding person cap restores all audience fields"), FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&Larger, &Asset->Spec.Audience, 0));
    TestTrue(TEXT("Unsupported requested values remain available to the user"), FFFZSAudienceCapacityEditor::CanRetry(Asset));
    TestTrue(TEXT("Status preserves the unsupported requested count"), FFFZSAudienceCapacityEditor::GetStatus(Asset).ToString().Contains(TEXT("512")));
    FFFZSFacilitySpec NonAudience = Asset->Spec;
    NonAudience.Audience = Original.Audience;
    FFFZSFacilitySpec NormalizedOriginal = Original;
    NormalizedOriginal.ClearanceCells = 0;
    TestTrue(TEXT("No Queue, mesh, identity or other active fields were changed"), FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&NormalizedOriginal, &NonAudience, 0));
    // A cancelled drag followed by another field must not re-use the old rollback state.
    EditAudienceProperty(*Asset, Rows, 51, EPropertyChangeType::Interactive);
    Asset->Spec.Audience = Larger; // Simulate the editor restoring the cancelled value.
    Asset->PostEditChange();
    Asset->Spec.Audience.FirstSlotLocalCm.Y += 50.;
    EditAudienceProperty(*Asset, Rows, 513);
    TestEqual(TEXT("Cancelled edit cannot restore stale geometry later"), Asset->Spec.Audience.FirstSlotLocalCm.Y, Larger.FirstSlotLocalCm.Y + 50.);

    Asset->Spec.DisplayName = FText::GetEmpty();
    EditAudienceProperty(*Asset, Rows, 60);
    TestEqual(TEXT("Unrelated validation error does not partially apply geometry"), Asset->Spec.Audience.Rows, 50);
    TestTrue(TEXT("User does not have to retype the requested rows after a fix"), FFFZSAudienceCapacityEditor::CanRetry(Asset));
    TestTrue(TEXT("Explanation includes the actual unrelated validation error"), FFFZSAudienceCapacityEditor::GetStatus(Asset).ToString().Contains(TEXT("DisplayName")));
    Asset->Spec.DisplayName = Original.DisplayName;
    Asset->Spec.Audience.FirstSlotLocalCm.Y += 75.;
    const FFFZSAudienceLayout BeforeRetry = Asset->Spec.Audience;
    TestTrue(TEXT("Saved request applies after the unrelated error is fixed"), FFFZSAudienceCapacityEditor::Retry(*Asset));
    TestEqual(TEXT("Retry applies original requested rows without retyping"), Asset->Spec.Audience.Rows, 60);
    TestEqual(TEXT("Retry uses freshly authored geometry rather than stale snapshot"), Asset->Spec.Audience.FirstSlotLocalCm.Y, BeforeRetry.FirstSlotLocalCm.Y);
    TestFalse(TEXT("Successful retry clears pending request"), FFFZSAudienceCapacityEditor::CanRetry(Asset));
    TestTrue(TEXT("Retry is one undoable operation"), GEditor->UndoTransaction());
    TestTrue(TEXT("Undo retry restores all derived fields together"), FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&BeforeRetry, &Asset->Spec.Audience, 0));
    Asset->Spec = Original;
    Asset->ClearFlags(RF_Public | RF_Standalone);
    Package->SetDirtyFlag(false);
    return true;
}

#endif
