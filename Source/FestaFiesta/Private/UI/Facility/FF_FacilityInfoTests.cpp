// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoTypes.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSelection.h"
#include "WidgetBlueprint.h"

namespace
{
	const TCHAR* FacilityInfoObjectPath =
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel."
			"WBP_FF_FacilityInfoPanel");
	const TCHAR* FacilityInfoClassPath =
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel."
			"WBP_FF_FacilityInfoPanel_C");

	class FScopedFacilityInfoPlayerlessWidgetInitialization
	{
	public:
		explicit FScopedFacilityInfoPlayerlessWidgetInitialization(
			UWidgetBlueprintGeneratedClass& InClass)
			: WidgetClass(InClass)
			, bPreviousValue(InClass.bCanCallInitializedWithoutPlayerContext)
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = true;
		}

		~FScopedFacilityInfoPlayerlessWidgetInitialization()
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = bPreviousValue;
		}

	private:
		UWidgetBlueprintGeneratedClass& WidgetClass;
		bool bPreviousValue;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoFormattingTest,
	"FestaFiesta.UI.FacilityInfo.Formatting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoFormattingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestEqual(TEXT("부스 분류는 제품용 한글로 표시된다"),
		FFFacilityInfoModel::GetCategoryText(EFFZSFacilityCategory::Booth).ToString(),
		FString(TEXT("부스")));
	TestEqual(TEXT("운영 시설 상태는 제품용 한글로 표시된다"),
		FFFacilityInfoModel::GetFacilityStateText(EFFZSFacilityState::Active).ToString(),
		FString(TEXT("운영 중")));
	TestEqual(TEXT("가격 실패는 0 Gold로 오해되지 않는다"),
		FFFacilityInfoModel::FormatGold(false, 0).ToString(),
		FString(TEXT("가격 정보 없음")));
	TestEqual(TEXT("즉시 이용 가능한 Queue는 대기 0분 대신 의미를 표시한다"),
		FFFacilityInfoModel::FormatExpectedWait(true, 0.0f).ToString(),
		FString(TEXT("바로 이용 가능")));
	TestEqual(TEXT("Unreachable sentinel은 24시간으로 노출하지 않는다"),
		FFFacilityInfoModel::FormatExpectedWait(true, 86400.0f).ToString(),
		FString(TEXT("계산 불가")));
	TestEqual(TEXT("61초는 과소 예측하지 않고 2분으로 올림한다"),
		FFFacilityInfoModel::FormatExpectedWait(true, 61.0f).ToString(),
		FString(TEXT("약 2분")));
	TestEqual(TEXT("미등록 Queue 기본값을 열린 시설로 표시하지 않는다"),
		FFFacilityInfoModel::GetOperationStateText(true, false, true, true).ToString(),
		FString(TEXT("운영 정보 준비 중")));
	TestEqual(TEXT("Queue 시스템이 없는 월드는 영구 로딩 대신 사용 불가로 표시한다"),
		FFFacilityInfoModel::GetOperationStateText(false, false, true, true).ToString(),
		FString(TEXT("운영 정보 사용 불가")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoSelectionTraceDecisionTest,
	"FestaFiesta.UI.FacilityInfo.SelectionTraceDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoSelectionTraceDecisionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using FFFacilityInfoSelection::ETraceDecision;
	using FFFacilityInfoSelection::ResolveTraceDecision;

	TestEqual(TEXT("선택 proxy가 없을 때만 footprint 호환 경로를 사용한다"),
		ResolveTraceDecision(false, false, false), ETraceDecision::UseFootprintFallback);
	TestEqual(TEXT("등록된 proxy가 보이면 proxy 시설을 즉시 사용한다"),
		ResolveTraceDecision(true, true, false), ETraceDecision::UseSelectionProxy);
	TestEqual(TEXT("외부 장애물에 가려진 proxy는 footprint로 관통 선택하지 않는다"),
		ResolveTraceDecision(true, true, true), ETraceDecision::Reject);
	TestEqual(TEXT("전용 채널의 비정상 후보도 footprint fallback을 열지 않는다"),
		ResolveTraceDecision(true, false, false), ETraceDecision::Reject);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoAssetContractTest,
	"FestaFiesta.UI.FacilityInfo.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, FacilityInfoObjectPath);
	TestNotNull(TEXT("시설 정보 WBP 에셋이 로드된다"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	TestTrue(TEXT("시설 정보 WBP에 컴파일 오류가 없다"), Blueprint->Status != BS_Error);

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, FacilityInfoClassPath));
	TestNotNull(TEXT("시설 정보 WBP GeneratedClass가 로드된다"), GeneratedClass);
	if (!GeneratedClass)
	{
		return false;
	}
	TestTrue(TEXT("WBP는 네이티브 시설 정보 패널 계약을 상속한다"),
		GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()));

	const UWidgetTree* Tree = GeneratedClass->GetWidgetTreeArchetype();
	TestNotNull(TEXT("컴파일된 WidgetTree가 존재한다"), Tree);
	if (!Tree)
	{
		return false;
	}

	const UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
	TestNotNull(TEXT("전체 화면 루트는 CanvasPanel이다"), Root);
	if (Root)
	{
		TestEqual(TEXT("카드 밖 월드 입력을 막지 않는 루트 visibility"),
			Root->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}
	const USizeBox* PanelSize = Cast<USizeBox>(Tree->FindWidget(TEXT("Panel_Size")));
	const UCanvasPanelSlot* PanelSlot = PanelSize
		? Cast<UCanvasPanelSlot>(PanelSize->Slot)
		: nullptr;
	TestNotNull(TEXT("카드는 전체 화면 Canvas의 직접 자식이다"), PanelSlot);
	if (PanelSlot)
	{
		const FAnchors Anchors = PanelSlot->GetAnchors();
		TestTrue(TEXT("카드 anchor는 우측 중앙이다"),
			Anchors.Minimum.Equals(FVector2D(1.0f, 0.5f)) &&
			Anchors.Maximum.Equals(FVector2D(1.0f, 0.5f)));
		TestTrue(TEXT("카드 alignment는 우측 중앙이다"),
			PanelSlot->GetAlignment().Equals(FVector2D(1.0f, 0.5f)));
		const FVector2D PanelPosition = PanelSlot->GetPosition();
		TestFalse(*FString::Printf(TEXT("카드 오프셋은 디자이너가 조정할 수 있고 유한한 값이다 (Actual: %s)"),
			*PanelPosition.ToString()),
			PanelPosition.ContainsNaN());
		TestTrue(TEXT("카드 크기는 내용에 맞춰 계산된다"), PanelSlot->GetAutoSize());
	}
	TestNotNull(TEXT("카드가 명시적 Border 계약을 가진다"),
		Cast<UBorder>(Tree->FindWidget(TEXT("Border_Panel"))));
	TestNotNull(TEXT("닫기 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Close"))));
	TestNotNull(TEXT("이동 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Move"))));
	TestNotNull(TEXT("철거 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Remove"))));
	TestNotNull(TEXT("썸네일 계약"), Cast<UImage>(Tree->FindWidget(TEXT("Image_Thumbnail"))));
	TestNotNull(TEXT("건축 섹션 계약"), Tree->FindWidget(TEXT("Panel_BuildSection")));
	TestNotNull(TEXT("라이브 섹션 계약"), Tree->FindWidget(TEXT("Panel_LiveSection")));

	const TArray<FName> RequiredTextNames = {
		TEXT("Text_ThumbnailPlaceholder"), TEXT("Text_Context"), TEXT("Text_FacilityName"),
		TEXT("Text_Category"), TEXT("Text_Zone"), TEXT("Text_FacilityState"),
		TEXT("Text_RelocationWarning"), TEXT("Text_Footprint"), TEXT("Text_Clearance"),
		TEXT("Text_InstallCost"), TEXT("Text_RecoveryCost"), TEXT("Text_QueueCapacity"),
		TEXT("Text_ServiceDuration"), TEXT("Text_OperationState"), TEXT("Text_WaitingParties"),
		TEXT("Text_Occupancy"), TEXT("Text_RemainingService"), TEXT("Text_ExpectedWait"),
		TEXT("Text_Throughput"), TEXT("Text_Feedback")
	};
	for (const FName Name : RequiredTextNames)
	{
		TestNotNull(*FString::Printf(TEXT("필수 텍스트 위젯: %s"), *Name.ToString()),
			Cast<UCommonTextBlock>(Tree->FindWidget(Name)));
	}
	TArray<FName> RequiredVariableNames = {
		TEXT("Border_Panel"), TEXT("Button_Close"), TEXT("Button_Move"),
		TEXT("Button_Remove"), TEXT("Image_Thumbnail"), TEXT("Panel_BuildSection"),
		TEXT("Panel_LiveSection")
	};
	RequiredVariableNames.Append(RequiredTextNames);
	for (const FName Name : RequiredVariableNames)
	{
		const UWidget* Widget = Tree->FindWidget(Name);
		TestTrue(*FString::Printf(TEXT("BindWidget 변수/GUID 계약: %s"), *Name.ToString()),
			Widget && Widget->bIsVariable && Blueprint->WidgetVariableNameToGuidMap.Contains(Name));
	}

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	TestNotNull(TEXT("UMG 인스턴스 검증용 Editor World가 존재한다"), EditorWorld);
	if (!EditorWorld)
	{
		return false;
	}
	FScopedFacilityInfoPlayerlessWidgetInitialization InitializationGuard(*GeneratedClass);
	UFF_FacilityInfoPanel* Panel = CreateWidget<UFF_FacilityInfoPanel>(EditorWorld, GeneratedClass);
	TestNotNull(TEXT("27개 BindWidget 계약으로 WBP 인스턴스를 만들 수 있다"), Panel);
	if (Panel)
	{
		FFF_FacilityInfoViewData ViewData;
		ViewData.FacilityInstanceId = FGuid::NewGuid();
		ViewData.Context = EFF_FacilityInfoContext::Build;
		ViewData.DisplayName = FText::FromString(TEXT("테스트 시설"));
		ViewData.bCanMove = true;
		ViewData.bCanRemove = true;
		Panel->ApplyViewData(ViewData);
		const TSharedRef<SWidget> SlateWidget = Panel->TakeWidget();
		(void)SlateWidget;
		TestEqual(TEXT("적용된 ViewData의 GUID가 보존된다"),
			Panel->GetCurrentViewData().FacilityInstanceId, ViewData.FacilityInstanceId);
		Panel->SetPanelOpen(true);
		TestEqual(TEXT("열린 full-screen wrapper는 카드 밖 클릭을 통과시킨다"),
			Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		Panel->SetPanelOpen(false);
		TestEqual(TEXT("닫힌 패널은 완전히 접힌다"),
			Panel->GetVisibility(), ESlateVisibility::Collapsed);
	}
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
