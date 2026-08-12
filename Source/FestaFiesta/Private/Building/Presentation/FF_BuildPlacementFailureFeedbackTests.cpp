// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "CommonTextBlock.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"

namespace
{
	const TCHAR* FeedbackPackagePath =
		TEXT("/Game/FestaFiesta/Building/Feedback/WBP_FF_BuildPlacementFailureFeedback");
	const TCHAR* FeedbackObjectPath =
		TEXT("/Game/FestaFiesta/Building/Feedback/WBP_FF_BuildPlacementFailureFeedback."
			"WBP_FF_BuildPlacementFailureFeedback");
	const TCHAR* FeedbackClassPath =
		TEXT("/Game/FestaFiesta/Building/Feedback/WBP_FF_BuildPlacementFailureFeedback."
			"WBP_FF_BuildPlacementFailureFeedback_C");

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
	FFF_BuildPlacementFailureFeedbackAssetContractTest,
	"FestaFiesta.Building.UI.BuildPlacementFailureFeedback.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementFailureFeedbackAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestFalse(TEXT("The failure WBP lives outside the team-owned Zone/UI/Building path"),
		FString(FeedbackPackagePath).StartsWith(TEXT("/Game/FestaFiesta/Zone/UI/Building")));
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, FeedbackObjectPath);
	TestNotNull(TEXT("The independent editable failure WBP loads"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	TestTrue(TEXT("The failure WBP has no compile error"), Blueprint->Status != BS_Error);

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, FeedbackClassPath));
	TestNotNull(TEXT("The failure WBP generated class loads"), GeneratedClass);
	if (!GeneratedClass)
	{
		return false;
	}
	TestTrue(TEXT("The WBP derives from the native failure-feedback contract"),
		GeneratedClass->IsChildOf(UFF_BuildPlacementFailureFeedback::StaticClass()));

	const UWidgetTree* Tree = GeneratedClass->GetWidgetTreeArchetype();
	TestNotNull(TEXT("The failure WBP has a compiled WidgetTree"), Tree);
	if (!Tree)
	{
		return false;
	}
	TestNotNull(TEXT("Designer owns the toast background"),
		Tree->FindWidget(TEXT("Failure_Background")));
	TestNotNull(TEXT("Designer owns the required failure title"),
		Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_FailureTitle"))));
	TestNotNull(TEXT("Designer owns the required failure reason"),
		Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_FailureReason"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildPlacementFailureFeedbackPresentationTest,
	"FestaFiesta.Building.UI.BuildPlacementFailureFeedback.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementFailureFeedbackPresentationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(TEXT("Insufficient funds maps to the dedicated Gold UI state"),
		UFF_BuildPlacementFailureFeedback::ClassifyFailure(
			EFFZSPlacementFailure::InsufficientFunds),
		EFF_BuildPlacementFailureType::InsufficientGold);
	TestEqual(TEXT("Generic policy rejection is not mislabelled as Gold shortage"),
		UFF_BuildPlacementFailureFeedback::ClassifyFailure(
			EFFZSPlacementFailure::ExternalPolicyRejected),
		EFF_BuildPlacementFailureType::PlacementRejected);
	TestEqual(TEXT("The dedicated title is concise and localized"),
		UFF_BuildPlacementFailureFeedback::MakeFailureTitle(
			EFFZSPlacementFailure::InsufficientFunds, false).ToString(),
		FString(TEXT("골드 부족")));

	FFFZSPlacementValidation Validation;
	Validation.bIsValid = true;
	Validation.Summary = FText::FromString(TEXT("배치가 유효합니다."));
	Validation.AddFailure(
		EFFZSPlacementFailure::InsufficientFunds,
		FText::FromString(TEXT("설치비 400 Gold가 필요합니다.")));
	TestEqual(TEXT("A later policy failure replaces the stale success summary"),
		Validation.Summary.ToString(), FString(TEXT("설치비 400 Gold가 필요합니다.")));

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UWidgetBlueprintGeneratedClass* FeedbackClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, FeedbackClassPath));
	TestNotNull(TEXT("Editor world is available for the UMG presentation test"), EditorWorld);
	TestNotNull(TEXT("Editable failure WBP class is available"), FeedbackClass);
	if (!EditorWorld || !FeedbackClass)
	{
		return false;
	}

	const bool bPackageWasDirty = FeedbackClass->GetOutermost()->IsDirty();
	FScopedPlayerlessWidgetInitialization Guard(*FeedbackClass);
	UFF_BuildPlacementFailureFeedback* Feedback =
		CreateWidget<UFF_BuildPlacementFailureFeedback>(EditorWorld, FeedbackClass);
	TestNotNull(TEXT("The failure WBP can be instantiated"), Feedback);
	if (!Feedback)
	{
		return false;
	}
	const TSharedRef<SWidget> Slate = Feedback->TakeWidget();
	const FText Detail = FText::FromString(
		TEXT("시설 설치비 400 Gold가 필요하지만 현재 잔액은 0 Gold입니다."));
	Feedback->PresentFailure(
		EFF_BuildPlacementFailureType::InsufficientGold,
		UFF_BuildPlacementFailureFeedback::MakeFailureTitle(
			EFFZSPlacementFailure::InsufficientFunds, false),
		Detail);
	TestEqual(TEXT("The actual WBP title displays Gold shortage"),
		Feedback->GetDisplayedFailureTitle().ToString(), FString(TEXT("골드 부족")));
	TestEqual(TEXT("The actual WBP preserves the authoritative failure detail"),
		Feedback->GetDisplayedFailureReason().ToString(), Detail.ToString());
	TestEqual(TEXT("Presenting a failure makes the toast visible without blocking input"),
		Feedback->GetVisibility(), ESlateVisibility::HitTestInvisible);

	Feedback->PresentFailure(
		EFF_BuildPlacementFailureType::PlacementRejected,
		UFF_BuildPlacementFailureFeedback::MakeFailureTitle(
			EFFZSPlacementFailure::PlacementCollision, false),
		FText::FromString(TEXT("시설이 다른 시설과 겹칩니다.")));
	TestEqual(TEXT("Reusing the toast replaces stale title data"),
		Feedback->GetDisplayedFailureTitle().ToString(), FString(TEXT("설치할 수 없음")));
	Feedback->DismissFeedback();
	TestEqual(TEXT("Dismiss collapses the non-blocking toast"),
		Feedback->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Runtime presentation preserves the designer WBP package dirty state"),
		FeedbackClass->GetOutermost()->IsDirty(), bPackageWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

