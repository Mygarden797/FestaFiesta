#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "UI/PrepHUD/Build/FF_PrepBuildInputBridgeSubsystem.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"

namespace FFPrepBuildInputBridgeTest
{
	constexpr const TCHAR* HudBarClassPath =
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.WBP_FF_PrepHudBar_C");

	UWidgetTree* ResolveWidgetTree(UClass* WidgetClass)
	{
		const UWidgetBlueprintGeneratedClass* GeneratedClass =
			Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
		return GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
	}

	class FScopedPrepBuildHudPlayerlessWidgetInitialization
	{
	public:
		explicit FScopedPrepBuildHudPlayerlessWidgetInitialization(
			UWidgetBlueprintGeneratedClass& InClass)
			: WidgetClass(InClass)
			, bPreviousValue(InClass.bCanCallInitializedWithoutPlayerContext)
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = true;
		}

		~FScopedPrepBuildHudPlayerlessWidgetInitialization()
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = bPreviousValue;
		}

	private:
		UWidgetBlueprintGeneratedClass& WidgetClass;
		bool bPreviousValue;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildInputBridgeRoutingTest,
	"FestaFiesta.UI.PrepHUD.Build.InputBridge.Routing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildInputBridgeRoutingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepBuildInputBridgeModel;

	TestTrue(TEXT("CatalogOpen에서 단일 PrepHUD의 Build 탭을 연다"),
		ResolveCatalogRoute(
			EFF_BuildModeState::CatalogOpen, EFF_BuildModeState::CatalogOpen,
			false, false, true, false) ==
		ERouteAction::OpenBuildTab);
	TestTrue(TEXT("선택 표시만 남은 Build 탭은 닫고 다시 연다"),
		ResolveCatalogRoute(
			EFF_BuildModeState::CatalogOpen, EFF_BuildModeState::CatalogOpen,
			false, false, true, true) ==
		ERouteAction::ReopenBuildTab);
	TestTrue(TEXT("BuildMode 상태에서는 표현을 선점하지 않는다"),
		ResolveCatalogRoute(
			EFF_BuildModeState::BuildMode, EFF_BuildModeState::BuildMode,
			false, false, true, false) ==
		ERouteAction::None);
	TestTrue(TEXT("앞선 상태 리스너가 세션을 닫았으면 오래된 CatalogOpen 방송을 무시한다"),
		ResolveCatalogRoute(
			EFF_BuildModeState::CatalogOpen, EFF_BuildModeState::Closed,
			false, false, true, false) ==
		ERouteAction::None);
	TestTrue(TEXT("외부 표현이 이미 활성화되면 기존 소유자를 존중한다"),
		ResolveCatalogRoute(
			EFF_BuildModeState::CatalogOpen, EFF_BuildModeState::CatalogOpen,
			false, true, true, false) ==
		ERouteAction::None);
	TestTrue(TEXT("재진입 중에는 탭을 다시 토글하지 않는다"),
		ResolveCatalogRoute(
			EFF_BuildModeState::CatalogOpen, EFF_BuildModeState::CatalogOpen,
			true, false, true, false) ==
		ERouteAction::None);
	TestTrue(TEXT("활성 PrepHUD가 정확히 하나가 아니면 레거시 폴백을 유지한다"),
		ResolveCatalogRoute(
			EFF_BuildModeState::CatalogOpen, EFF_BuildModeState::CatalogOpen,
			false, false, false, false) ==
		ERouteAction::None);
	TestTrue(TEXT("상태·선택·external opt-in이 모두 맞아야 준비 완료다"),
		IsPresentationReady(EFF_BuildModeState::CatalogOpen, true, true));
	TestFalse(TEXT("external opt-in이 없으면 레거시 화면을 억제하지 않는다"),
		IsPresentationReady(EFF_BuildModeState::CatalogOpen, true, false));

	const UFF_PrepBuildInputBridgeSubsystem* BridgeDefaults =
		GetDefault<UFF_PrepBuildInputBridgeSubsystem>();
	TestTrue(TEXT("브리지는 게임 월드를 지원한다"),
		BridgeDefaults->DoesSupportWorldType(EWorldType::Game));
	TestTrue(TEXT("브리지는 PIE 월드를 지원한다"),
		BridgeDefaults->DoesSupportWorldType(EWorldType::PIE));
	TestTrue(TEXT("브리지는 GamePreview 월드를 지원한다"),
		BridgeDefaults->DoesSupportWorldType(EWorldType::GamePreview));
	TestFalse(TEXT("브리지는 에디터 월드에 생성되지 않는다"),
		BridgeDefaults->DoesSupportWorldType(EWorldType::Editor));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildInputBridgeWidgetContractTest,
	"FestaFiesta.UI.PrepHUD.Build.InputBridge.WidgetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildInputBridgeWidgetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepBuildInputBridgeTest;

	UClass* HudBarClass = LoadClass<UFF_PrepHudBar>(nullptr, HudBarClassPath);
	TestNotNull(TEXT("브리지가 사용할 PrepHUD Bar WBP를 로드한다"), HudBarClass);
	TestTrue(TEXT("PrepHUD Bar WBP는 전용 네이티브 Bar를 상속한다"),
		HudBarClass && HudBarClass->IsChildOf(UFF_PrepHudBar::StaticClass()));

	UWidgetTree* HudBarTree = ResolveWidgetTree(HudBarClass);
	TestNotNull(TEXT("PrepHUD Bar의 WidgetTree가 저장됐다"), HudBarTree);
	if (HudBarTree)
	{
		TestNotNull(TEXT("B 입력이 선택할 Build 탭이 존재한다"),
			HudBarTree->FindWidget(TEXT("Tab_Build")));
		TestNotNull(TEXT("Build 패널을 붙일 PanelHost가 존재한다"),
			HudBarTree->FindWidget(TEXT("Panel_PanelHost")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildInputBridgeWorkspaceHitTestContractTest,
	"FestaFiesta.UI.PrepHUD.Build.InputBridge.WorkspaceHitTestContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildInputBridgeWorkspaceHitTestContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepBuildInputBridgeTest;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UWidgetBlueprintGeneratedClass* HudBarClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepHudBar>(nullptr, HudBarClassPath));
	TestNotNull(TEXT("PrepHUD 런타임 위젯 검증용 Editor World가 존재한다"), EditorWorld);
	TestNotNull(TEXT("PrepHUD Bar GeneratedClass를 로드한다"), HudBarClass);
	if (!EditorWorld || !HudBarClass)
	{
		return false;
	}

	FScopedPrepBuildHudPlayerlessWidgetInitialization InitializationGuard(*HudBarClass);
	UFF_PrepHudBar* HudBar = CreateWidget<UFF_PrepHudBar>(EditorWorld, HudBarClass);
	TestNotNull(TEXT("PrepHUD Bar 인스턴스를 만들 수 있다"), HudBar);
	if (!HudBar)
	{
		return false;
	}

	TestEqual(TEXT("PrepHUD 루트의 빈 영역은 시설/지면 월드 클릭을 통과시킨다"),
		HudBar->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

	const UPanelWidget* PanelHost = Cast<UPanelWidget>(
		HudBar->GetWidgetFromName(TEXT("Panel_PanelHost")));
	TestNotNull(TEXT("단일 패널 호스트가 존재한다"), PanelHost);
	if (PanelHost)
	{
		TestEqual(TEXT("열린 패널이 없을 때 단일 패널 호스트는 입력을 가로채지 않는다"),
			PanelHost->GetVisibility(), ESlateVisibility::Collapsed);
	}

	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
