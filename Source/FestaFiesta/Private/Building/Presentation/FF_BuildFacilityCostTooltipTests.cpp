// FF_BuildFacilityCostTooltipTests.cpp

// 비용 툴팁 WBP의 BindWidget 계약과 시설 카드 연결·표시 문자열을 통합 검증한다.
// 흐름: 저장된 Blueprint 구조 검사 -> 네이티브/파생 위젯 생성 -> 비용 스냅샷 반영 확인.
// 불변식: 테스트는 저장 에셋을 변경하지 않고 런타임과 같은 Configure 경로를 호출한다.
// 실패/수명: 콘텐츠 검사와 transient 동작 검사를 분리하고 생성한 Slate 래퍼는 범위 끝까지 유지한다.

#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildFacilityCostTooltip.h"
#include "Building/UI/FF_BuildFacilityCard.h"
#include "CommonTextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"

namespace
{
	const TCHAR* TooltipPackagePath =
		FFBuildUIAssetPaths::FacilityCostTooltipPackage;
	const TCHAR* TooltipObjectPath =
		FFBuildUIAssetPaths::FacilityCostTooltipObject;
	const TCHAR* TooltipClassPath =
		FFBuildUIAssetPaths::FacilityCostTooltipClass;
	const TCHAR* FacilityCardClassPath =
		FFBuildUIAssetPaths::FacilityCardClass;

	// Unity 빌드는 여러 cpp의 익명 네임스페이스를 합치므로 보조 함수 이름을 파일 고유하게 유지한다.
	class FScopedBuildFacilityCostTooltipPlayerlessWidgetInitialization
	{
	public:
		explicit FScopedBuildFacilityCostTooltipPlayerlessWidgetInitialization(
			UWidgetBlueprintGeneratedClass& InClass)
			: WidgetClass(InClass)
			, bPreviousValue(InClass.bCanCallInitializedWithoutPlayerContext)
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = true;
		}

		~FScopedBuildFacilityCostTooltipPlayerlessWidgetInitialization()
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
	// WBP의 부모 클래스와 세 BindWidget 이름·타입을 먼저 확인해 런타임 null 바인딩을 에셋 오류로 보고한다.
	(void)Parameters;

	TestFalse(TEXT("The tooltip lives outside the team-owned Zone/UI/Building path"),
		FString(TooltipPackagePath).StartsWith(FFBuildUIAssetPaths::LegacyCatalogFolder));
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
	const UVerticalBox* Content = Cast<UVerticalBox>(Tree->FindWidget(TEXT("Tooltip_Content")));
	const UWidget* CostRow = Tree->FindWidget(TEXT("Cost_Row"));
	const UWidget* RecoveryRow = Tree->FindWidget(TEXT("Recovery_Row"));
	const UCommonTextBlock* InstallCost =
		Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_InstallCost")));
	const UCommonTextBlock* RecoveryLabel =
		Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_RecoveryLabel")));
	const UCommonTextBlock* RecoveryCost =
		Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_RecoveryCost")));
	TestNotNull(TEXT("Designer owns the tooltip content hierarchy"), Content);
	TestNotNull(TEXT("Designer owns the install-cost row"), CostRow);
	TestNotNull(TEXT("Designer owns the required install-cost presentation"), InstallCost);
	TestNotNull(TEXT("Designer owns the recovery-cost row"), RecoveryRow);
	TestNotNull(TEXT("Designer owns the recovery-cost label"), RecoveryLabel);
	TestNotNull(TEXT("Designer owns the required recovery-cost presentation"), RecoveryCost);
	if (Content && CostRow && RecoveryRow)
	{
		TestEqual(TEXT("Recovery cost is directly below the install cost"),
			Content->GetChildIndex(RecoveryRow), Content->GetChildIndex(CostRow) + 1);
	}
	if (InstallCost && RecoveryCost)
	{
		TestTrue(TEXT("The WBP does not serialize a hard-coded install-cost preview"),
			InstallCost->GetText().IsEmpty());
		TestTrue(TEXT("The WBP does not serialize a hard-coded recovery-cost preview"),
			RecoveryCost->GetText().IsEmpty());
	}
	if (RecoveryLabel)
	{
		TestEqual(TEXT("The recovery row uses the requested Korean label"),
			RecoveryLabel->GetText().ToString(), FString(TEXT("회수 비용")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildFacilityCostTooltipCardIntegrationTest,
	"FestaFiesta.Building.UI.BuildFacilityCostTooltip.CardIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFacilityCostTooltipCardIntegrationTest::RunTest(const FString& Parameters)
{
	// 카드가 만든 툴팁까지 실제 Configure 경로로 연결해 미확정/확정 비용 표시를 함께 검증한다.
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
	FScopedBuildFacilityCostTooltipPlayerlessWidgetInitialization TooltipGuard(*TooltipClass);
	FScopedBuildFacilityCostTooltipPlayerlessWidgetInitialization CardGuard(*CardClass);

	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Booth_FoodStall_UMGTooltipTest");
	Entry.DisplayName = FText::FromString(TEXT("Data Pocha Food Truck"));
	Entry.InstallCost = 400;
	Entry.bHasResolvedInstallCost = true;
	Entry.RecoveryCost = 200;
	Entry.bHasResolvedRecoveryCost = true;

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
		TestEqual(TEXT("The WBP displays the resolved 200 Gold recovery cost"),
			StandaloneTooltip->GetDisplayedRecoveryCostText().ToString(),
			FString(TEXT("200 Gold")));
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
	TestEqual(TEXT("The card-attached WBP displays the resolved recovery cost"),
		AttachedTooltip->GetDisplayedRecoveryCostText().ToString(),
		FString(TEXT("200 Gold")));

	Entry.RecoveryCost = 0;
	Entry.bHasResolvedRecoveryCost = true;
	Card->Configure(Entry);
	TestEqual(TEXT("A resolved zero-percent recovery rate is displayed as zero Gold"),
		AttachedTooltip->GetDisplayedRecoveryCostText().ToString(),
		FString(TEXT("0 Gold")));

	Entry.InstallCost = 0;
	Card->Configure(Entry);
	TestTrue(TEXT("The card tooltip recognizes a resolved free installation"),
		AttachedTooltip->HasResolvedInstallCost());
	TestEqual(TEXT("The card-attached WBP displays a resolved free installation as zero Gold"),
		AttachedTooltip->GetDisplayedInstallCostText().ToString(),
		FString(TEXT("0 Gold")));

	Entry.InstallCost = -1;
	Card->Configure(Entry);
	TestFalse(TEXT("A negative price is invalid even with a resolved flag"),
		AttachedTooltip->HasResolvedInstallCost());
	TestEqual(TEXT("A negative price replaces the previous free price with unavailable text"),
		AttachedTooltip->GetDisplayedInstallCostText().ToString(),
		FString(TEXT("가격 정보 없음")));

	Entry.InstallCost = 0;
	Entry.bHasResolvedInstallCost = false;
	Entry.RecoveryCost = 0;
	Entry.bHasResolvedRecoveryCost = false;
	Card->Configure(Entry);
	TestEqual(TEXT("Reconfiguration reuses the card's tooltip instance"),
		Card->GetToolTip(), static_cast<UWidget*>(AttachedTooltip));
	TestEqual(TEXT("Unavailable price never leaves a stale or misleading zero value"),
		AttachedTooltip->GetDisplayedInstallCostText().ToString(),
		FString(TEXT("가격 정보 없음")));
	TestEqual(TEXT("Unavailable recovery cost never leaves a stale or misleading zero value"),
		AttachedTooltip->GetDisplayedRecoveryCostText().ToString(),
		FString(TEXT("가격 정보 없음")));
	TestEqual(TEXT("Runtime attachment preserves the existing card WBP dirty state"),
		CardClass->GetOutermost()->IsDirty(), bCardPackageWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
