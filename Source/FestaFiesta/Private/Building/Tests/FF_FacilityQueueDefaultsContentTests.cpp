#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSQueueGeometry.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueueDefaultsContentTest,
	"FestaFiesta.Content.FacilityQueues.AllProductionDefaultsAreUnified",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityQueueDefaultsContentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Asset paths include retired definitions too. A PrimaryAssetId lookup alone
	// could silently omit one of two assets whose authoring IDs collide.
	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game/FestaFiesta/Building/FF_Data_Building"));
	Filter.ClassPaths.Add(UFFZSFacilityDefinition::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	UAssetManager::Get().GetAssetRegistry().GetAssets(Filter, Assets);
	if (!TestTrue(TEXT("The bounded production authoring root contains definitions"), Assets.Num() > 0 && Assets.Num() <= 1024)) return false;
	Assets.Sort([](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.PackageName.LexicalLess(Right.PackageName);
	});
	int32 QueueFacilityCount = 0;
	int32 NonServiceCount = 0;
	for (const FAssetData& Asset : Assets)
	{
		const FString Label = Asset.GetSoftObjectPath().ToString();
		const UFFZSFacilityDefinition* Definition = Cast<UFFZSFacilityDefinition>(Asset.GetAsset());
		if (!TestNotNull(*FString::Printf(TEXT("Production definition loads: %s"), *Label), Definition)) continue;
		const FFFZSFacilitySpec& Base = Definition->Spec;
		if (!Base.UsesServiceQueue())
		{
			++NonServiceCount;
			TestFalse(*FString::Printf(TEXT("Non-service content has no generated visitor queue: %s"), *Label), Base.QueueSettings.bEnabled);
			continue;
		}
		++QueueFacilityCount;
		TestTrue(*FString::Printf(TEXT("Every service facility uses the common queue system: %s"), *Label), Base.QueueSettings.bEnabled);
		TestEqual(*FString::Printf(TEXT("Default team capacity is unified: %s"), *Label), Base.QueueSettings.MaxWaitingTeams, 12);
		TestEqual(*FString::Printf(TEXT("Default team spacing is unified: %s"), *Label), Base.QueueSettings.SlotSpacingCm, 115.f);
		TestEqual(*FString::Printf(TEXT("Default lane width is unified: %s"), *Label), Base.QueueSettings.LaneWidthCm, 600.f);
		const int32 ServiceCapacity = Base.QueueRules.ServiceCapacity;
		const float ServiceDuration = Base.QueueRules.ServiceDurationSeconds;
		FFFZSFacilitySpec DirectLayout = Base;
		FText LayoutError;
		const bool bLayoutGenerated = FFFZSQueueGeometry::ApplySelectedLayout(DirectLayout, LayoutError);
		TestTrue(*FString::Printf(TEXT("Common queue generation succeeds: %s. %s"), *Label, *LayoutError.ToString()), bLayoutGenerated);
		if (!bLayoutGenerated)
		{
			AddInfo(FString::Printf(TEXT("Queue input %s: footprint=%s layout=%d settings=(enabled=%d teams=%d spacing=%.2f width=%.2f) authored=(waiting=%d spacing=%.2f service=%d duration=%.2f tolerance=%.2f)."),
				*Label, *Base.FootprintSizeCells.ToString(), static_cast<int32>(Base.QueueLayout),
				Base.QueueSettings.bEnabled, Base.QueueSettings.MaxWaitingTeams, Base.QueueSettings.SlotSpacingCm,
				Base.QueueSettings.LaneWidthCm, Base.QueueRules.MaxWaitingAgents, Base.QueueRules.SlotSpacingCm,
				Base.QueueRules.ServiceCapacity, Base.QueueRules.ServiceDurationSeconds, Base.QueueRules.SlotArrivalToleranceCm));
			for (int32 Index = 0; Index < Base.QueueSplinePointsCm.Num(); ++Index)
				AddInfo(FString::Printf(TEXT("Queue input point[%d]=%s"), Index, *Base.QueueSplinePointsCm[Index].ToString()));
			for (const FFFZSAnchorDefinition& Anchor : Base.Anchors)
				AddInfo(FString::Printf(TEXT("Queue input anchor(type=%d index=%d)=%s"), static_cast<int32>(Anchor.Type),
					Anchor.Index, *Anchor.LocalTransform.GetLocation().ToString()));
			const FFFZSAudienceLayout& Audience = Base.Audience;
			FBox2D AudienceBounds(ForceInit);
			const bool bAudienceBounds = Audience.TryGetLocalBounds(AudienceBounds);
			AddInfo(FString::Printf(TEXT("Queue input audience=(enabled=%d capacity=%d first=%s rows=%d columns=%d spacing=%.2f/%.2f margin=%.2f boundsValid=%d min=%s max=%s entry=%s exit=%s)."),
				Audience.bEnabled, Audience.GetCapacity(), *Audience.FirstSlotLocalCm.ToString(), Audience.Rows, Audience.Columns,
				Audience.ColumnSpacingCm, Audience.RowSpacingCm, Audience.PerimeterMarginCm, bAudienceBounds,
				*AudienceBounds.Min.ToString(), *AudienceBounds.Max.ToString(), *Audience.EntryLocalCm.ToString(), *Audience.ExitLocalCm.ToString()));
		}
		const FFFZSFacilitySpec Effective = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 1, 1.f);
		TArray<FText> Errors;
		if (!Effective.IsValid(Errors))
		{
			AddError(FString::Printf(TEXT("The common level-one queue is invalid: %s"), *Label));
			for (const FText& Error : Errors) AddError(Error.ToString());
		}
		TestEqual(*FString::Printf(TEXT("Runtime waiting slots use the common capacity: %s"), *Label), Effective.QueueRules.MaxWaitingAgents, 12);
		TestEqual(*FString::Printf(TEXT("Runtime waiting slots use the common spacing: %s"), *Label), Effective.QueueRules.SlotSpacingCm, 115.f);
		TestEqual(*FString::Printf(TEXT("Queue unification preserves the authored concurrent service capacity: %s"), *Label), Effective.QueueRules.ServiceCapacity, ServiceCapacity);
		TestEqual(*FString::Printf(TEXT("Queue unification preserves the authored service duration: %s"), *Label), Effective.QueueRules.ServiceDurationSeconds, ServiceDuration);
		FFF_QueueLaneGeometry Lane;
		TestTrue(*FString::Printf(TEXT("Every unified queue projects into live Party Queue geometry: %s"), *Label),
			UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(Effective, FTransform::Identity, Lane));
		TestEqual(*FString::Printf(TEXT("Every projected queue has exactly twelve team slots: %s"), *Label), Lane.ExplicitSlotTransforms.Num(), 12);
	}
	TestTrue(TEXT("At least one actual production queue was checked"), QueueFacilityCount > 0);
	AddInfo(FString::Printf(TEXT("Checked all %d production definitions: %d service queues, %d facilities without queues."),
		Assets.Num(), QueueFacilityCount, NonServiceCount));
	return true;
}

#endif
