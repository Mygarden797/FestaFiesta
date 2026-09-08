#if WITH_DEV_AUTOMATION_TESTS

#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSTypes.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

namespace FFZSFacilityKoreanTooltipsTestsPrivate
{
    bool ContainsHangul(const FString& Text)
    {
        for (const TCHAR Character : Text)
        {
            if (Character >= 0xAC00 && Character <= 0xD7A3)
            {
                return true;
            }
        }
        return false;
    }

    bool ContainsLatinLetter(const FString& Text)
    {
        for (const TCHAR Character : Text)
        {
            if ((Character >= TEXT('A') && Character <= TEXT('Z')) ||
                (Character >= TEXT('a') && Character <= TEXT('z')))
            {
                return true;
            }
        }
        return false;
    }

    bool IsEnglishDisplayName(const FString& Text)
    {
        return ContainsLatinLetter(Text) && !ContainsHangul(Text);
    }

    void TestEnglishDisplayNameAndKoreanTooltip(
        FAutomationTestBase& Test,
        const UStruct* Owner,
        const TArray<FName>& PropertyNames)
    {
        for (const FName PropertyName : PropertyNames)
        {
            const FProperty* Property = Owner->FindPropertyByName(PropertyName);
            Test.TestNotNull(
                *FString::Printf(TEXT("%s.%s property exists"), *Owner->GetName(), *PropertyName.ToString()),
                Property);
            if (!Property)
            {
                continue;
            }
            const FString DisplayName = Property->GetMetaData(TEXT("DisplayName"));
            const FString ToolTip = Property->GetMetaData(TEXT("ToolTip"));
            Test.TestTrue(
                *FString::Printf(TEXT("%s.%s has an English display name"), *Owner->GetName(), *PropertyName.ToString()),
                IsEnglishDisplayName(DisplayName));
            Test.TestTrue(
                *FString::Printf(TEXT("%s.%s has a Korean hover tooltip"), *Owner->GetName(), *PropertyName.ToString()),
                ContainsHangul(ToolTip));
        }
    }

    void TestAllVisibleEnumEntries(FAutomationTestBase& Test, const UEnum* Enum)
    {
        Test.TestNotNull(TEXT("Required editor enum is reflected"), Enum);
        if (!Enum)
        {
            return;
        }

        int32 TestedEntryCount = 0;
        for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
        {
            const FString EntryName = Enum->GetNameStringByIndex(Index);
            const bool bIsGeneratedMax =
                Index == Enum->NumEnums() - 1 && EntryName.EndsWith(TEXT("_MAX"));
            if (bIsGeneratedMax || Enum->HasMetaData(TEXT("Hidden"), Index))
            {
                continue;
            }

            ++TestedEntryCount;
            Test.TestTrue(
                *FString::Printf(TEXT("%s.%s has an English editor label"), *Enum->GetName(), *EntryName),
                IsEnglishDisplayName(Enum->GetDisplayNameTextByIndex(Index).ToString()));
            Test.TestTrue(
                *FString::Printf(TEXT("%s.%s has a Korean hover tooltip"), *Enum->GetName(), *EntryName),
                ContainsHangul(Enum->GetToolTipTextByIndex(Index).ToString()));
        }

        Test.TestTrue(
            *FString::Printf(TEXT("%s exposes at least one visible entry"), *Enum->GetName()),
            TestedEntryCount > 0);
    }

    bool TryReadLocTextLiteral(const FString& Source, const FString& Key, FString& OutLiteral)
    {
        const int32 KeyIndex = Source.Find(
            FString::Printf(TEXT("\"%s\""), *Key),
            ESearchCase::CaseSensitive);
        const int32 ValueStart = KeyIndex == INDEX_NONE
            ? INDEX_NONE
            : Source.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, KeyIndex);
        if (ValueStart == INDEX_NONE)
        {
            return false;
        }

        bool bInsideString = false;
        bool bEscaped = false;
        for (int32 Index = ValueStart + 1; Index < Source.Len(); ++Index)
        {
            const TCHAR Character = Source[Index];
            if (bInsideString)
            {
                if (bEscaped)
                {
                    OutLiteral.AppendChar(Character);
                    bEscaped = false;
                }
                else if (Character == TEXT('\\'))
                {
                    bEscaped = true;
                }
                else if (Character == TEXT('"'))
                {
                    bInsideString = false;
                }
                else
                {
                    OutLiteral.AppendChar(Character);
                }
            }
            else if (Character == TEXT('"'))
            {
                bInsideString = true;
            }
            else if (Character == TEXT(')'))
            {
                return !OutLiteral.IsEmpty();
            }
        }
        return false;
    }

    void TestCustomizationLocText(
        FAutomationTestBase& Test,
        const FString& Source,
        const TCHAR* Key,
        const TCHAR* ExpectedEnglishText = nullptr)
    {
        FString Literal;
        const bool bFound = TryReadLocTextLiteral(Source, Key, Literal);
        Test.TestTrue(*FString::Printf(TEXT("Customization LOCTEXT %s exists"), Key), bFound);
        if (!bFound)
        {
            return;
        }

        if (ExpectedEnglishText)
        {
            Test.TestEqual(
                *FString::Printf(TEXT("Customization LOCTEXT %s stays English"), Key),
                Literal,
                FString(ExpectedEnglishText));
            Test.TestFalse(
                *FString::Printf(TEXT("Customization LOCTEXT %s contains no Hangul"), Key),
                ContainsHangul(Literal));
        }
        else
        {
            Test.TestTrue(
                *FString::Printf(TEXT("Customization LOCTEXT %s contains Korean help"), Key),
                ContainsHangul(Literal));
        }
    }

    void TestCustomizationSourceContract(FAutomationTestBase& Test)
    {
        const FString SourcePath = FPaths::Combine(
            FPaths::ProjectDir(),
            TEXT("Plugins/FF_ZoneSystem/Source/FF_ZoneSystemEditor/Private/FF_FFZSFacilityDefinitionCustomization.cpp"));

        FString Source;
        Test.TestTrue(
            TEXT("Facility definition customization source is readable"),
            FFileHelper::LoadFileToString(Source, *SourcePath));
        if (Source.IsEmpty())
        {
            return;
        }

        TestCustomizationLocText(Test, Source, TEXT("ZoneSystemCategory"), TEXT("FF Zone System"));
        TestCustomizationLocText(Test, Source, TEXT("PresentationCategory"), TEXT("Presentation"));
        TestCustomizationLocText(Test, Source, TEXT("AuthoringCategory"), TEXT("Authoring"));
        TestCustomizationLocText(Test, Source, TEXT("AuthoringToolsCategory"), TEXT("Facility Authoring Tools"));
        TestCustomizationLocText(Test, Source, TEXT("ShowKoreanHelpButton"), TEXT("Show Korean Help"));
        TestCustomizationLocText(Test, Source, TEXT("ValidateDefinitionButton"), TEXT("Validate Facility Definition"));
        TestCustomizationLocText(Test, Source, TEXT("FootprintFromMeshButton"), TEXT("Footprint <- Loaded Mesh"));
        TestCustomizationLocText(
            Test, Source, TEXT("ResizeFootprintButton"), TEXT("Resize Facility (Keep Queue & Anchors)"));
        TestCustomizationLocText(Test, Source, TEXT("ResizeFootprintWidthLabel"), TEXT("New Width (X)"));
        TestCustomizationLocText(Test, Source, TEXT("ResizeFootprintDepthLabel"), TEXT("New Depth (Y)"));
        TestCustomizationLocText(Test, Source, TEXT("ResizeFootprintHeightLabel"), TEXT("New Height (Z)"));
        TestCustomizationLocText(
            Test, Source, TEXT("QueueRectsFromPathButton"), TEXT("Queue Rects <- Queue Path"));
        TestCustomizationLocText(Test, Source, TEXT("QueueMarginLabel"), TEXT("Queue Margin (cm)"));
        TestCustomizationLocText(Test, Source, TEXT("TailFromCapacityButton"), TEXT("Queue Tail <- Max Capacity"));
        TestCustomizationLocText(Test, Source, TEXT("CapacityFromLengthButton"), TEXT("Max Capacity <- Queue Length"));
        TestCustomizationLocText(Test, Source, TEXT("FitVisualButton"), TEXT("Fit Mesh to Placement Bounds"));
        TestCustomizationLocText(Test, Source, TEXT("QueueRectElement"), TEXT("Reserved Rectangle {0}"));
        TestCustomizationLocText(Test, Source, TEXT("QueuePointElement"), TEXT("Queue Point {0}"));
        TestCustomizationLocText(Test, Source, TEXT("AnchorElement"), TEXT("Anchor {0}"));

        TestCustomizationLocText(Test, Source, TEXT("ZoneSystemCategoryTip"));
        TestCustomizationLocText(Test, Source, TEXT("PresentationCategoryTip"));
        TestCustomizationLocText(Test, Source, TEXT("AuthoringCategoryTip"));
        TestCustomizationLocText(Test, Source, TEXT("AuthoringToolsCategoryTip"));
        TestCustomizationLocText(Test, Source, TEXT("ShowKoreanHelpTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("ValidateDefinitionTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("FootprintFromMeshTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("ResizeFootprintTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("ResizeFootprintWidthTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("ResizeFootprintDepthTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("ResizeFootprintHeightTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("QueueRectsFromPathTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("QueueMarginTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("TailFromCapacityTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("CapacityFromLengthTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("FitVisualTooltip"));
        TestCustomizationLocText(Test, Source, TEXT("QueueRectElementTip"));
        TestCustomizationLocText(Test, Source, TEXT("QueueHeadPointTip"));
        TestCustomizationLocText(Test, Source, TEXT("AnchorElementTip"));
        TestCustomizationLocText(Test, Source, TEXT("KoreanAuthoringHelpBody"));
        TestCustomizationLocText(Test, Source, TEXT("KoreanAuthoringHelpTitle"));

        FString CompactSource = Source;
        CompactSource.ReplaceInline(TEXT("\r"), TEXT(""));
        CompactSource.ReplaceInline(TEXT("\n"), TEXT(""));
        CompactSource.ReplaceInline(TEXT("\t"), TEXT(""));
        CompactSource.ReplaceInline(TEXT(" "), TEXT(""));
        Test.TestTrue(
            TEXT("Array element count changes bind a details refresh callback"),
            CompactSource.Contains(TEXT("Array->SetOnNumElementsChanged(")) &&
            CompactSource.Contains(TEXT("PinnedUtilities->RequestForceRefresh();")));
        Test.TestTrue(
            TEXT("Show Korean Help button opens the Korean help action"),
            CompactSource.Contains(TEXT(
                ".OnClicked(this,&FFFZSFacilityDefinitionCustomization::ShowKoreanHelp)")));
        Test.TestTrue(
            TEXT("Queue reserved rectangle additions refresh their dynamic labels"),
            CompactSource.Contains(TEXT("RefreshDetailsWhenArraySizeChanges(QueueReservedRects,PropertyUtilities);")));
        Test.TestTrue(
            TEXT("Queue point additions refresh their dynamic labels"),
            CompactSource.Contains(TEXT("RefreshDetailsWhenArraySizeChanges(QueueSplinePoints,PropertyUtilities);")));
        Test.TestTrue(
            TEXT("Anchor additions refresh their dynamic labels"),
            CompactSource.Contains(TEXT("RefreshDetailsWhenArraySizeChanges(Anchors,PropertyUtilities);")));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSFacilityKoreanTooltipsTest,
    "FF_ZoneSystem.Editor.FacilityKoreanTooltips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityKoreanTooltipsTest::RunTest(const FString& Parameters)
{
    using namespace FFZSFacilityKoreanTooltipsTestsPrivate;

    TestEnglishDisplayNameAndKoreanTooltip(*this, FFFZSFacilityUpgradeTier::StaticStruct(),
        {
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityUpgradeTier, TargetLevel),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityUpgradeTier, CostMultiplier),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityUpgradeTier, ServiceDurationMultiplier),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityUpgradeTier, RevenueMultiplier),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityUpgradeTier, SizeMultiplier),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityUpgradeTier, VisualMesh),
        });

    TestEnglishDisplayNameAndKoreanTooltip(
        *this,
        FFFZSFacilitySpec::StaticStruct(),
        {
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, SchemaVersion),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, FacilityDefinitionId),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, DisplayName),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Category),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, FootprintSizeCells),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, ClearanceCells),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueReservedRects),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueReservedCells),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueSplinePointsCm),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, QueueRules),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, bUsesFacilityLevels),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, UpgradeTiers),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, Anchors),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, AllowedZoneIds),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, InstallCostKey),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, RevenueRuleKey),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, ExperienceEffectKey),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, VisualMesh),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, VisualSizingMode),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, VisualOffsetCm),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, VisualRotation),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, AudioSettings),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilitySpec, PlacementObstacleHeightCm),
        });

    TestEnglishDisplayNameAndKoreanTooltip(
        *this,
        FFFZSFacilityAudioSettings::StaticStruct(),
        {
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, AmbientSound),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, Attenuation),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, bOverrideAudibleRange),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, FullVolumeRadiusCm),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, MaxAudibleDistanceCm),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, Concurrency),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, VolumeMultiplier),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, PitchMultiplier),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, LocalOffsetCm),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, FadeInDurationSeconds),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, FadeOutDurationSeconds),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, bPlayOnlyWhileActive),
            GET_MEMBER_NAME_CHECKED(FFFZSFacilityAudioSettings, bLoop),
        });

    TestEnglishDisplayNameAndKoreanTooltip(
        *this,
        FFFZSQueueRules::StaticStruct(),
        {
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, MaxWaitingAgents),
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, SlotSpacingCm),
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, ServiceCapacity),
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, ServiceDurationSeconds),
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, AbandonAfterSeconds),
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, SlotArrivalToleranceCm),
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, NavigationProjectionExtentCm),
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, BlockedTimeoutSeconds),
            GET_MEMBER_NAME_CHECKED(FFFZSQueueRules, MaxRepathAttempts),
        });

    TestEnglishDisplayNameAndKoreanTooltip(
        *this,
        FFFZSAnchorDefinition::StaticStruct(),
        {
            GET_MEMBER_NAME_CHECKED(FFFZSAnchorDefinition, Type),
            GET_MEMBER_NAME_CHECKED(FFFZSAnchorDefinition, Index),
            GET_MEMBER_NAME_CHECKED(FFFZSAnchorDefinition, LocalTransform),
        });

    TestEnglishDisplayNameAndKoreanTooltip(
        *this,
        FFFZSCellRect::StaticStruct(),
        {
            GET_MEMBER_NAME_CHECKED(FFFZSCellRect, Min),
            GET_MEMBER_NAME_CHECKED(FFFZSCellRect, Max),
        });

    TestEnglishDisplayNameAndKoreanTooltip(
        *this,
        UFFZSFacilityDefinition::StaticClass(),
        {
            GET_MEMBER_NAME_CHECKED(UFFZSFacilityDefinition, Spec),
            GET_MEMBER_NAME_CHECKED(UFFZSFacilityDefinition, CatalogThumbnail),
#if WITH_EDITORONLY_DATA
            GET_MEMBER_NAME_CHECKED(UFFZSFacilityDefinition, bProcedurallyManagedVisualMesh),
#endif
        });

    TestAllVisibleEnumEntries(*this, StaticEnum<EFFZSAnchorType>());
    TestAllVisibleEnumEntries(*this, StaticEnum<EFFZSFacilityCategory>());
    TestAllVisibleEnumEntries(*this, StaticEnum<EFFZSVisualSizingMode>());

    const UFunction* IsDefinitionValidFunction =
        UFFZSFacilityDefinition::StaticClass()->FindFunctionByName(TEXT("IsDefinitionValid"));
    TestNotNull(TEXT("IsDefinitionValid is reflected"), IsDefinitionValidFunction);
    if (IsDefinitionValidFunction)
    {
        TestTrue(
            TEXT("IsDefinitionValid has an English display name"),
            IsEnglishDisplayName(IsDefinitionValidFunction->GetMetaData(TEXT("DisplayName"))));
        TestTrue(
            TEXT("IsDefinitionValid has the correct Korean hover tooltip"),
            ContainsHangul(IsDefinitionValidFunction->GetMetaData(TEXT("ToolTip"))) &&
            !IsDefinitionValidFunction->GetMetaData(TEXT("ToolTip")).Contains(TEXT("카탈로그 카드")));
    }

    TestCustomizationSourceContract(*this);

    return true;
}

#endif
