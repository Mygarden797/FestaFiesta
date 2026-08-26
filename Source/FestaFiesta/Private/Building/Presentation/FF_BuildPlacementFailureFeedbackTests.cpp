// FF_BuildPlacementFailureFeedbackTests.cpp

// 배치 실패 피드백 WBP의 구조와 실패 코드별 제목·상세 표시 계약을 검증한다.
// 흐름: 저장된 Blueprint 검사 -> transient 위젯 구성 -> 분류/표시/해제 상태 확인.
// 불변식: 저장 에셋을 수정하지 않고 런타임과 같은 PresentFailure/DismissFeedback 경로를 호출한다.
// 실패/수명: 에셋 누락과 로직 회귀를 별도 assertion으로 보고하고 위젯 래퍼를 검사 동안 유지한다.

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

	// Unity 빌드는 여러 cpp의 익명 네임스페이스를 합치므로 보조 함수 이름을 파일 고유하게 유지한다.
	class FScopedBuildPlacementFailureFeedbackPlayerlessWidgetInitialization
	{
	public:
		explicit FScopedBuildPlacementFailureFeedbackPlayerlessWidgetInitialization(
			UWidgetBlueprintGeneratedClass& InClass)
			: WidgetClass(InClass)
			, bPreviousValue(InClass.bCanCallInitializedWithoutPlayerContext)
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = true;
		}

		~FScopedBuildPlacementFailureFeedbackPlayerlessWidgetInitialization()
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
	// 토스트의 부모 클래스와 제목·상세 BindWidget을 검사해 디자이너 변경의 영향 범위를 고정한다.
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
	// 각 권위 실패 코드의 UI 분류와 표시·해제 수명주기를 동일 위젯 인스턴스에서 확인한다.
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
		FString(TEXT("골드가 부족해요")));
	const FString CollisionReason =
		UFF_BuildPlacementFailureFeedback::MakeUserFacingReason(
			EFFZSPlacementFailure::PlacementCollision,
			FText::FromString(TEXT("Placement collision in World Cell")),
			false).ToString();
	TestTrue(TEXT("Collision guidance explains the cause in everyday Korean"),
		CollisionReason.Contains(TEXT("겹쳐요")));
	TestTrue(TEXT("Collision guidance includes the next action"),
		CollisionReason.Contains(TEXT("옮겨 주세요")));
	TestFalse(TEXT("Technical validation terms are not exposed to players"),
		CollisionReason.Contains(TEXT("World")) || CollisionReason.Contains(TEXT("Cell")) ||
		CollisionReason.Contains(TEXT("Placement")));
	TestTrue(TEXT("Slope guidance points the player toward flatter ground"),
		UFF_BuildPlacementFailureFeedback::MakeUserFacingReason(
			EFFZSPlacementFailure::ExcessSlope, FText::GetEmpty(), false)
			.ToString().Contains(TEXT("평평한 곳")));
	TestTrue(TEXT("Outside-zone guidance tells the player where to move"),
		UFF_BuildPlacementFailureFeedback::MakeUserFacingReason(
			EFFZSPlacementFailure::OutsideZone, FText::GetEmpty(), false)
			.ToString().Contains(TEXT("경계선 안쪽")));

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
	FScopedBuildPlacementFailureFeedbackPlayerlessWidgetInitialization PlayerlessInitializationGuard(
		*FeedbackClass);
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
		Feedback->GetDisplayedFailureTitle().ToString(), FString(TEXT("골드가 부족해요")));
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
		Feedback->GetDisplayedFailureTitle().ToString(), FString(TEXT("여기에 설치할 수 없어요")));
	Feedback->DismissFeedback();
	TestEqual(TEXT("Dismiss collapses the non-blocking toast"),
		Feedback->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Runtime presentation preserves the designer WBP package dirty state"),
		FeedbackClass->GetOutermost()->IsDirty(), bPackageWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

