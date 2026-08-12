// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildFacilityCostTooltip.h"
#include "Building/UI/FF_BuildFacilityCard.h"
#include "CommonTextBlock.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"

namespace
{
	const TCHAR* TooltipPackagePath =
		TEXT("/Game/FestaFiesta/Building/Tooltips/WBP_FF_BuildFacilityCostTooltip");
	const TCHAR* TooltipObjectPath =
		TEXT("/Game/FestaFiesta/Building/Tooltips/WBP_FF_BuildFacilityCostTooltip."
			"WBP_FF_BuildFacilityCostTooltip");
	const TCHAR* TooltipClassPath =
		TEXT("/Game/FestaFiesta/Building/Tooltips/WBP_FF_BuildFacilityCostTooltip."
			"WBP_FF_BuildFacilityCostTooltip_C");
	const TCHAR* FacilityCardClassPath =
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildFacilityCard."
			"WBP_FF_BuildFacilityCard_C");

	class FScopedPlayerlessWidgetInitialization
	{
	public:
		explicit FScopedPlayerlessWidgetInitialization(UWidgetBlueprintGeneratedClass& InClass)
			: WidgetClass(InClass)
			, bPreviousValue(InClass.bCanCallInitializedWithoutPlayerContext)
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = true;
		}

		~FScopedPlayerlessWidgetInitialization()
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = bPreviousValue;
		}

	private:
		UWidgetBlueprintGeneratedClass& WidgetClass;
		bool bPreviousValue;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildFacilityCostTooltipAssetContractTest,
	"FestaFiesta.Building.UI.BuildFacilityCostTooltip.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFacilityCostTooltipAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestFalse(TEXT("The tooltip lives outside the team-owned Zone/UI/Building path"),
		FString(TooltipPackagePath).StartsWith(TEXT("/Game/FestaFiesta/Zone/UI/Building")));
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, TooltipObjectPath);
	TestNotNull(TEXT("The independent editable tooltip WBP loads"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	TestTrue(TEXT("The tooltip WBP has no compile error"), Blueprint->Status != BS_Error);

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, TooltipClassPath));
	TestNotNull(TEXT("The tooltip generated class loads"), GeneratedClass);
	if (!GeneratedClass)
	{
		return false;
	}
	TestTrue(TEXT("The WBP derives from the native cost-tooltip contract"),
		GeneratedClass->IsChildOf(UFF_BuildFacilityCostTooltip::StaticClass()));

	const UWidgetTree* Tree = GeneratedClass->GetWidgetTreeArchetype();
	TestNotNull(TEXT("The tooltip WBP has a compiled WidgetTree"), Tree);
	if (!Tree)
	{
		return false;
	}
	TestNotNull(TEXT("Designer owns the tooltip background"),
		Tree->FindWidget(TEXT("Tooltip_Background")));
	TestNotNull(TEXT("Designer owns the facility-name presentation"),
		Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_FacilityName"))));
	TestNotNull(TEXT("Designer owns the required install-cost presentation"),
		Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_InstallCost"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildFacilityCostTooltipCardIntegrationTest,
	"FestaFiesta.Building.UI.BuildFacilityCostTooltip.CardIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFacilityCostTooltipCardIntegrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UWidgetBlueprintGeneratedClass* TooltipClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, TooltipClassPath));
	UWidgetBlueprintGeneratedClass* CardClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, FacilityCardClassPath));
	TestNotNull(TEXT("Editor world is available for the UMG integration test"), EditorWorld);
	TestNotNull(TEXT("Editable tooltip class is available"), TooltipClass);
	TestNotNull(TEXT("Existing production card class is available"), CardClass);
	if (!EditorWorld || !TooltipClass || !CardClass)
	{
		return false;
	}

	const bool bCardPackageWasDirty = CardClass->GetOutermost()->IsDirty();
	FScopedPlayerlessWidgetInitialization TooltipGuard(*TooltipClass);
	FScopedPlayerlessWidgetInitialization CardGuard(*CardClass);

	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Booth_FoodStall_UMGTooltipTest");
	Entry.DisplayName = FText::FromString(TEXT("Data Pocha Food Truck"));
	Entry.InstallCost = 400;
	Entry.bHasResolvedInstallCost = true;

	UFF_BuildFacilityCostTooltip* StandaloneTooltip =
		CreateWidget<UFF_BuildFacilityCostTooltip>(EditorWorld, TooltipClass);
	TestNotNull(TEXT("The tooltip WBP can be instantiated"), StandaloneTooltip);
	if (StandaloneTooltip)
	{
		StandaloneTooltip->Configure(Entry);
		const TSharedRef<SWidget> TooltipSlate = StandaloneTooltip->TakeWidget();
		TestEqual(TEXT("The WBP displays the resolved 400 Gold price"),
			StandaloneTooltip->GetDisplayedInstallCostText().ToString(),
			FString(TEXT("400 Gold")));
	}

	UFF_BuildFacilityCard* Card = CreateWidget<UFF_BuildFacilityCard>(EditorWorld, CardClass);
	TestNotNull(TEXT("The existing production card can be instantiated without modification"), Card);
	if (!Card)
	{
		return false;
	}
	Card->Configure(Entry);
	const TSharedRef<SWidget> CardSlate = Card->TakeWidget();
	UFF_BuildFacilityCostTooltip* AttachedTooltip =
		Cast<UFF_BuildFacilityCostTooltip>(Card->GetToolTip());
	TestNotNull(TEXT("The existing card receives the custom UMG tooltip at runtime"), AttachedTooltip);
	TestTrue(TEXT("The Slate card has a registered custom tooltip"),
		CardSlate->GetToolTip().IsValid());
	TestTrue(TEXT("Custom UMG replaces the plain text tooltip on the production path"),
		Card->GetToolTipText().IsEmpty());
	if (!AttachedTooltip)
	{
		return false;
	}
	TestEqual(TEXT("The card-attached WBP displays the resolved price"),
		AttachedTooltip->GetDisplayedInstallCostText().ToString(),
		FString(TEXT("400 Gold")));

	Entry.InstallCost = 0;
	Entry.bHasResolvedInstallCost = false;
	Card->Configure(Entry);
	TestEqual(TEXT("Reconfiguration reuses the card's tooltip instance"),
		Card->GetToolTip(), static_cast<UWidget*>(AttachedTooltip));
	TestEqual(TEXT("Unavailable price never leaves a stale or misleading zero value"),
		AttachedTooltip->GetDisplayedInstallCostText().ToString(),
		FString(TEXT("가격 정보 없음")));
	TestEqual(TEXT("Runtime attachment preserves the existing card WBP dirty state"),
		CardClass->GetOutermost()->IsDirty(), bCardPackageWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
