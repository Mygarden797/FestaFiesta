// 역할: CommonButton 내부 포커스, 반복 확정/뒤로 입력, 열린 지도에서의 잠금 갱신을 검사한다.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Atlas/FF_AtlasCountryScreen.h"
#include "UI/Atlas/FF_AtlasCountryInputTestObserver.h"
#include "UI/Atlas/FF_AtlasPinButton.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSettings.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "ICommonInputModule.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SVirtualWindow.h"

// 구현의 분기를 복사하지 않고 실제 화면 입력과 갱신 함수를 호출한다.
struct FFF_AtlasCountryInputTestAccess
{
	static void Configure(UFF_AtlasCountryScreen& Screen, UFF_AtlasPinButton* Start,
		UFF_AtlasPinButton* Back, UTextBlock* Reason, UImage* Lock)
	{
		Screen.CountryId = TEXT("InputTestCountry");
		Screen.bUseDesignerTextAndColors = true;
		Screen.Button_StartFestival = Start;
		Screen.Button_Back = Back;
		Screen.Text_LockReason = Reason;
		Screen.Image_LockedIcon = Lock;
	}

	static void AddPin(UFF_AtlasCountryScreen& Screen, UFF_AtlasPinButton& Pin)
	{
		Screen.PinButtons.Add(Pin.GetPinId(), &Pin);
		Screen.NavigationFestivalIds.Add(Pin.GetPinId());
		Pin.OnPinFocusReceived.AddDynamic(&Screen, &UFF_AtlasCountryScreen::HandlePinFocusReceived);
	}

	static FReply Preview(UFF_AtlasCountryScreen& Screen, FKey Key, bool bRepeat = false)
	{
		return Screen.NativeOnPreviewKeyDown(FGeometry(),
			FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0));
	}

	static FReply KeyDown(UFF_AtlasCountryScreen& Screen, FKey Key, bool bRepeat = false)
	{
		return Screen.NativeOnKeyDown(FGeometry(),
			FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0));
	}

	static void Refresh(UFF_AtlasCountryScreen& Screen) { Screen.RefreshFestivalAvailability(); }
	static void ConfigureMap(UFF_AtlasCountryScreen& Screen, UCanvasPanel* Map, UClass* PinClass)
	{
		Screen.CanvasPanel_Map = Map;
		Screen.PinButtonClass = PinClass;
	}
	static UWidget* DesiredFocus(const UFF_AtlasCountryScreen& Screen)
	{
		return Screen.NativeGetDesiredFocusTarget();
	}
	static void ReleaseSlate(UFF_AtlasCountryScreen& Screen) { Screen.ReleaseSlateResources(true); }
	static FName Selected(const UFF_AtlasCountryScreen& Screen) { return Screen.SelectedFestivalId; }
	static UFF_AtlasPinButton* Pin(const UFF_AtlasCountryScreen& Screen, FName Id)
	{
		return Screen.PinButtons.FindRef(Id);
	}
};

namespace FFAtlasCountryInputTests
{
	struct FFixture
	{
		TStrongObjectPtr<UFF_FestivalAtlasCatalog> Catalog;
		TStrongObjectPtr<UFF_AtlasCountryScreen> Screen;
		TStrongObjectPtr<UFF_AtlasCountryInputTestObserver> Observer;
		UFF_AtlasPinButton* FirstPin = nullptr;
		UFF_AtlasPinButton* SecondPin = nullptr;
		UFF_AtlasPinButton* Start = nullptr;
		UFF_AtlasPinButton* Back = nullptr;
		UTextBlock* Reason = nullptr;
		UImage* Lock = nullptr;
		TStrongObjectPtr<ULocalPlayer> LocalPlayer;
		TWeakObjectPtr<APlayerController> Controller;
		TSharedPtr<SVirtualWindow> Window;
		TSharedPtr<SWidget> PreviousFocus;
		TUniquePtr<TGuardValue<TSoftObjectPtr<UFF_FestivalAtlasCatalog>>> CatalogSetting;

		~FFixture()
		{
			if (Screen && Screen->IsActivated()) Screen->DeactivateWidget();
			if (Window)
			{
				FSlateApplication::Get().ClearKeyboardFocus();
				FSlateApplication::Get().UnregisterVirtualWindow(Window.ToSharedRef());
				Window->SetContent(SNullWidget::NullWidget);
				Window.Reset();
				if (Screen) FFF_AtlasCountryInputTestAccess::ReleaseSlate(*Screen);
				if (PreviousFocus) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
			}
			if (Controller.IsValid())
			{
				if (UWorld* World = Controller->GetWorld()) World->RemoveController(Controller.Get());
				Controller->Player = nullptr;
				if (LocalPlayer) LocalPlayer->PlayerController = nullptr;
				Controller->Destroy();
			}
		}

		static UFF_AtlasPinButton* MakeButton(UWidgetTree& Owner, FName Name)
		{
			auto* Button = NewObject<UFF_AtlasPinButton>(&Owner, Name);
			Button->WidgetTree = NewObject<UWidgetTree>(Button);
			auto* Label = Button->WidgetTree->ConstructWidget<UCommonTextBlock>(
				UCommonTextBlock::StaticClass(), TEXT("Text_Label"));
			Button->WidgetTree->RootWidget = Label;
			FindFProperty<FObjectPropertyBase>(Button->GetClass(), TEXT("Text_Label"))
				->SetObjectPropertyValue_InContainer(Button, Label);
			// 엔진의 Initialize가 이 트리를 실제 UCommonButtonInternalBase로 감싼다.
			Button->Initialize();
			Button->SetButtonLabel(FText::FromName(Name));
			return Button;
		}

		bool Initialize(FAutomationTestBase& Test, bool bRebuildMapOnActivation = false)
		{
			if (!Test.TestTrue(TEXT("Slate 초기화"), FSlateApplication::IsInitialized())) return false;
			UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			if (!Test.TestNotNull(TEXT("저장·게임 세션을 시작하지 않는 에디터 월드"), World)) return false;
			// 게임 인스턴스가 없는 검사에서도 NativeConstruct의 기본 Back 액션 조회 전에
			// CommonInput의 공식 로드 경로를 먼저 실행한다. LoadData는 중복 호출에 안전하다.
			ICommonInputModule::GetSettings().LoadData();
			Catalog.Reset(NewObject<UFF_FestivalAtlasCatalog>());
			for (int32 Index = 0; Index < 2; ++Index)
			{
				FFF_FestivalAtlasEntry Entry;
				Entry.CountryId = TEXT("InputTestCountry");
				Entry.FestivalId = Index == 0 ? TEXT("InputTestFirst") : TEXT("InputTestSecond");
				Entry.FestivalName = FText::FromName(Entry.FestivalId);
				Entry.bUnlocked = true;
				Entry.MapPosition = FVector2D(Index == 0 ? 0.25 : 0.75, 0.5);
				Entry.Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry")));
				Catalog->Entries.Add(Entry);
			}
			CatalogSetting = MakeUnique<TGuardValue<TSoftObjectPtr<UFF_FestivalAtlasCatalog>>>(
				GetMutableDefault<UFF_UIFlowSettings>()->AtlasCatalog,
				TSoftObjectPtr<UFF_FestivalAtlasCatalog>(Catalog.Get()));
			Screen.Reset(NewObject<UFF_AtlasCountryScreen>(World));
			Screen->WidgetTree = NewObject<UWidgetTree>(Screen.Get());
			UWidgetTree& Tree = *Screen->WidgetTree;
			auto* Root = Tree.ConstructWidget<UVerticalBox>();
			Tree.RootWidget = Root;
			Back = MakeButton(Tree, TEXT("Back"));
			Start = MakeButton(Tree, TEXT("Start"));
			Reason = Tree.ConstructWidget<UTextBlock>();
			Lock = Tree.ConstructWidget<UImage>();
			Root->AddChildToVerticalBox(Back);
			if (bRebuildMapOnActivation)
			{
				// 게임 인스턴스/저장 서브시스템을 시작하지 않고 SetFocus의 실제 소유자만 만든다.
				// 컨트롤러는 현재 에디터 월드에 잠시 존재하는 Transient 액터이며 종료 시 제거한다.
				LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
				LocalPlayer->SetControllerId(0);
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.ObjectFlags = RF_Transient;
				Controller = World->SpawnActor<APlayerController>(SpawnParameters);
				if (!Test.TestTrue(TEXT("포커스용 임시 로컬 컨트롤러"), Controller.IsValid())) return false;
				Controller->Player = LocalPlayer.Get();
				Controller->SetAsLocalPlayerController();
				LocalPlayer->PlayerController = Controller.Get();
				// 에디터 월드는 게임의 액터 초기화 단계를 거치지 않는다. FLocalPlayerContext는
				// PlayerController 필드 대신 월드의 컨트롤러 목록을 조회하므로 직접 등록한다.
				World->AddController(Controller.Get());
				Screen->SetPlayerContext(FLocalPlayerContext(Controller.Get()));
				if (!Test.TestTrue(TEXT("화면 생성 시 실제 로컬 소유 컨트롤러가 유효"),
					Screen->GetOwningPlayer() == Controller.Get())) return false;
				auto* Map = Tree.ConstructWidget<UCanvasPanel>();
				Root->AddChildToVerticalBox(Map);
				UClass* PinClass = LoadClass<UFF_AtlasPinButton>(nullptr,
					TEXT("/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasPin_Natural.WBP_FF_AtlasPin_Natural_C"));
				if (!Test.TestNotNull(TEXT("실제 국가 지도 핀 클래스"), PinClass)) return false;
				FFF_AtlasCountryInputTestAccess::ConfigureMap(*Screen, Map, PinClass);
			}
			else
			{
				FirstPin = MakeButton(Tree, TEXT("FirstPin"));
				SecondPin = MakeButton(Tree, TEXT("SecondPin"));
				Root->AddChildToVerticalBox(FirstPin);
				Root->AddChildToVerticalBox(SecondPin);
			}
			Root->AddChildToVerticalBox(Start);
			Root->AddChildToVerticalBox(Reason);
			Root->AddChildToVerticalBox(Lock);
			FFF_AtlasCountryInputTestAccess::Configure(*Screen, Start, Back, Reason, Lock);
			for (int32 Index = 0; !bRebuildMapOnActivation && Index < Catalog->Entries.Num(); ++Index)
			{
				const FFF_FestivalAtlasEntry& Entry = Catalog->Entries[Index];
				UFF_AtlasPinButton* Pin = Index == 0 ? FirstPin : SecondPin;
				Pin->SetPinData(Entry.FestivalId, Entry.FestivalName, Entry.RegionName,
					Entry.bUnlocked, Entry.LockReason);
				FFF_AtlasCountryInputTestAccess::AddPin(*Screen, *Pin);
			}
			Screen->Initialize();
			Observer.Reset(NewObject<UFF_AtlasCountryInputTestObserver>());
			Screen->OnStartFestivalRequested.AddDynamic(Observer.Get(), &UFF_AtlasCountryInputTestObserver::HandleStart);
			Screen->OnBackRequested.AddDynamic(Observer.Get(), &UFF_AtlasCountryInputTestObserver::HandleBack);
			PreviousFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
			Window = SNew(SVirtualWindow).Size(FVector2D(640, 480));
			Window->SetContent(Screen->TakeWidget());
			FSlateApplication::Get().RegisterVirtualWindow(Window.ToSharedRef());
			Window->SlatePrepass();
			return true;
		}

		void ActivateMap()
		{
			if (Screen->IsActivated()) Screen->DeactivateWidget();
			Screen->ActivateWidget();
			Window->SlatePrepass();
			FirstPin = FFF_AtlasCountryInputTestAccess::Pin(*Screen, Catalog->Entries[0].FestivalId);
			SecondPin = FFF_AtlasCountryInputTestAccess::Pin(*Screen, Catalog->Entries[1].FestivalId);
		}

		bool FocusPin(FAutomationTestBase& Test, UFF_AtlasPinButton& Pin)
		{
			UWidget* InternalButton = Pin.WidgetTree->RootWidget;
			if (!Test.TestTrue(TEXT("CommonUI가 실제 내부 버튼을 생성"),
				InternalButton && InternalButton->IsA<UCommonButtonInternalBase>())) return false;
			FSlateApplication::Get().SetKeyboardFocus(InternalButton->TakeWidget(), EFocusCause::SetDirectly);
			return Test.TestFalse(TEXT("핀의 외부 SObjectWidget 자체는 포커스가 아님"), Pin.HasAnyUserFocus())
				&& Test.TestTrue(TEXT("CommonButton 내부 자식에 실제 Slate 포커스"), Pin.HasFocusedDescendants());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryEnterInputTest,
	"FestaFiesta.UI.AtlasCountryInput.EnterUsesCommonButtonChildFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryEnterInputTest::RunTest(const FString& Parameters)
{
	FFAtlasCountryInputTests::FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.FocusPin(*this, *Fixture.FirstPin)) return false;
	TestEqual(TEXT("실제 포커스 이벤트가 축제를 선택"),
		FFF_AtlasCountryInputTestAccess::Selected(*Fixture.Screen), Fixture.FirstPin->GetPinId());
	const FKey ConfirmKeys[] = { EKeys::Enter, EKeys::Virtual_Gamepad_Accept.GetVirtualKey() };
	for (const FKey Key : ConfirmKeys)
	{
		const int32 Before = Fixture.Observer->StartRequests;
		TestTrue(TEXT("핀 자식 포커스에서 확정 입력을 소비"),
			FFF_AtlasCountryInputTestAccess::Preview(*Fixture.Screen, Key).IsEventHandled());
		TestEqual(TEXT("확정 입력당 시작 요청은 한 번"), Fixture.Observer->StartRequests, Before + 1);
		TestEqual(TEXT("시작 요청은 포커스된 축제 ID"), Fixture.Observer->LastFestivalId, Fixture.FirstPin->GetPinId());
		TestTrue(TEXT("반복 확정 입력도 하위 버튼에 전달하지 않음"),
			FFF_AtlasCountryInputTestAccess::Preview(*Fixture.Screen, Key, true).IsEventHandled());
		TestEqual(TEXT("키를 누른 채 유지해도 추가 시작 요청 없음"), Fixture.Observer->StartRequests, Before + 1);
	}
	TestEqual(TEXT("핀 확정이 뒤로 요청을 만들지 않음"), Fixture.Observer->BackRequests, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryInitialFocusTest,
	"FestaFiesta.UI.AtlasCountryInput.InitialFestivalFocusRoutesKeyboard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryInitialFocusTest::RunTest(const FString& Parameters)
{
	FFAtlasCountryInputTests::FFixture Fixture;
	if (!Fixture.Initialize(*this, /*bRebuildMapOnActivation=*/true)) return false;
	Fixture.Catalog->Entries[0].bUnlocked = false;
	Fixture.Catalog->Entries[0].LockReason = FText::FromString(TEXT("준비 중"));
	FSlateApplication::Get().ClearKeyboardFocus();
	Fixture.ActivateMap();
	if (!TestNotNull(TEXT("Rebuild가 첫 번째 핀을 생성"), Fixture.FirstPin)
		|| !TestNotNull(TEXT("Rebuild가 두 번째 핀을 생성"), Fixture.SecondPin)) return false;
	TestTrue(TEXT("동적으로 생성한 핀도 화면의 로컬 소유자를 상속"),
		Fixture.SecondPin->GetOwningPlayer() == Fixture.Controller.Get());
	TestEqual(TEXT("첫 항목이 잠기면 첫 플레이 가능 축제를 자동 선택"),
		FFF_AtlasCountryInputTestAccess::Selected(*Fixture.Screen), Fixture.SecondPin->GetPinId());
	TestTrue(TEXT("CommonUI 초기 포커스 대상은 선택된 축제 핀"),
		FFF_AtlasCountryInputTestAccess::DesiredFocus(*Fixture.Screen) == Fixture.SecondPin);
	if (!TestTrue(TEXT("마우스 호버나 수동 FocusPin 없이 활성화만으로 실제 핀 자식에 포커스"),
		Fixture.SecondPin->HasFocusedDescendants())) return false;

	// 위젯의 Preview 함수를 직접 부르면 포커스가 끊긴 결함을 놓친다.
	// Slate가 실제 포커스 경로를 찾아 국가 화면까지 키 이벤트를 전달하게 한다.
	const auto RouteKey = [](FKey Key)
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		const bool bHandled = Slate.ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
		Slate.ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
		return bHandled;
	};
	TestTrue(TEXT("진입 직후 A 입력을 실제 Slate 경로에서 처리"), RouteKey(EKeys::A));
	TestEqual(TEXT("첫 A부터 왼쪽 축제로 이동"),
		FFF_AtlasCountryInputTestAccess::Selected(*Fixture.Screen), Fixture.FirstPin->GetPinId());
	TestTrue(TEXT("이동 후 실제 포커스도 왼쪽 핀으로 이동"), Fixture.FirstPin->HasFocusedDescendants());
	TestTrue(TEXT("D 입력도 실제 Slate 경로에서 처리"), RouteKey(EKeys::D));
	TestEqual(TEXT("D로 플레이 가능 축제에 복귀"),
		FFF_AtlasCountryInputTestAccess::Selected(*Fixture.Screen), Fixture.SecondPin->GetPinId());
	TestTrue(TEXT("Enter 입력을 실제 Slate 경로에서 처리"), RouteKey(EKeys::Enter));
	TestEqual(TEXT("선택한 플레이 가능 축제의 시작 요청"), Fixture.Observer->StartRequests, 1);
	TestTrue(TEXT("Escape 입력을 실제 Slate 경로에서 처리"), RouteKey(EKeys::Escape));
	TestEqual(TEXT("실제 Escape 경로의 뒤로 요청"), Fixture.Observer->BackRequests, 1);

	Fixture.Catalog->Entries[1].bUnlocked = false;
	Fixture.Catalog->Entries[1].LockReason = FText::FromString(TEXT("준비 중"));
	Fixture.ActivateMap();
	TestEqual(TEXT("재진입 시 전부 잠겼으면 첫 축제의 안내를 선택"),
		FFF_AtlasCountryInputTestAccess::Selected(*Fixture.Screen), Fixture.FirstPin->GetPinId());
	TestTrue(TEXT("잠긴 첫 축제에도 포커스하여 사유 확인 가능"), Fixture.FirstPin->HasFocusedDescendants());
	TestEqual(TEXT("전부 잠긴 상태에서는 시작 버튼 숨김"), Fixture.Start->GetVisibility(), ESlateVisibility::Collapsed);

	AddExpectedMessage(TEXT("국가 `InputTestEmptyCountry` 에 축제가 없다"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Fixture.Screen->SetCountry(TEXT("InputTestEmptyCountry"));
	TestTrue(TEXT("빈 국가로 전환하면 이전 축제 선택 제거"),
		FFF_AtlasCountryInputTestAccess::Selected(*Fixture.Screen).IsNone());
	TestTrue(TEXT("빈 국가의 포커스 대상은 지구본 복귀 버튼"),
		FFF_AtlasCountryInputTestAccess::DesiredFocus(*Fixture.Screen) == Fixture.Back);
	TestTrue(TEXT("빈 국가에서도 실제 복귀 버튼에 포커스"), Fixture.Back->HasFocusedDescendants());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryBackInputTest,
	"FestaFiesta.UI.AtlasCountryInput.EscapeAndGamepadBackConsumeRepeats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryBackInputTest::RunTest(const FString& Parameters)
{
	FFAtlasCountryInputTests::FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.FocusPin(*this, *Fixture.FirstPin)) return false;
	const FKey BackKeys[] = { EKeys::Escape, EKeys::Virtual_Gamepad_Back.GetVirtualKey() };
	for (const FKey Key : BackKeys)
	{
		const int32 Before = Fixture.Observer->BackRequests;
		TestTrue(TEXT("Back 경로가 입력을 소비"),
			FFF_AtlasCountryInputTestAccess::KeyDown(*Fixture.Screen, Key).IsEventHandled());
		TestEqual(TEXT("최초 Back 입력당 복귀 요청은 한 번"), Fixture.Observer->BackRequests, Before + 1);
		TestTrue(TEXT("반복 Back 입력을 소비"),
			FFF_AtlasCountryInputTestAccess::KeyDown(*Fixture.Screen, Key, true).IsEventHandled());
		TestEqual(TEXT("반복 Back이 추가 복귀 요청을 만들지 않음"), Fixture.Observer->BackRequests, Before + 1);
	}
	TestEqual(TEXT("Back이 시작 요청을 만들지 않음"), Fixture.Observer->StartRequests, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryAvailabilityInputTest,
	"FestaFiesta.UI.AtlasCountryInput.AvailabilityUpdatesInPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryAvailabilityInputTest::RunTest(const FString& Parameters)
{
	FFAtlasCountryInputTests::FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.FocusPin(*this, *Fixture.FirstPin)) return false;
	FFF_FestivalAtlasEntry& Entry = Fixture.Catalog->Entries[0];
	const TSharedPtr<SWidget> FocusBefore = FSlateApplication::Get().GetKeyboardFocusedWidget();
	TestTrue(TEXT("초기 선택 핀 강조"), Fixture.FirstPin->IsSelectedState());
	TestTrue(TEXT("초기 시작 버튼 사용 가능"), Fixture.Start->GetIsEnabled());
	Entry.bUnlocked = false;
	Entry.LockReason = FText::FromString(TEXT("이전 축제를 먼저 완료하세요"));
	Fixture.Catalog->Entries[1].bUnlocked = false;
	FFF_AtlasCountryInputTestAccess::Refresh(*Fixture.Screen);
	TestFalse(TEXT("선택 핀 잠금 반영"), Fixture.FirstPin->IsSelectable());
	TestFalse(TEXT("비선택 핀도 같은 갱신에서 잠김"), Fixture.SecondPin->IsSelectable());
	TestFalse(TEXT("잠긴 핀의 선택 강조 제거"), Fixture.FirstPin->IsSelectedState());
	TestFalse(TEXT("잠금 시 시작 금지"), Fixture.Start->GetIsEnabled());
	TestEqual(TEXT("잠금 시 시작 버튼 숨김"), Fixture.Start->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("자물쇠 표시"), Fixture.Lock->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("잠금 사유가 카드에 표시됨"), Fixture.Reason->GetText().ToString(), Entry.LockReason.ToString());
	Entry.LockReason = FText::FromString(TEXT("축제 콘텐츠를 준비하고 있습니다"));
	FFF_AtlasCountryInputTestAccess::Refresh(*Fixture.Screen);
	TestEqual(TEXT("잠금 값이 같아도 사유 변경을 핀에 반영"), Fixture.FirstPin->GetLockReason().ToString(), Entry.LockReason.ToString());
	TestEqual(TEXT("사유만 바뀌어도 카드 동기화"), Fixture.Reason->GetText().ToString(), Entry.LockReason.ToString());
	Entry.bUnlocked = true;
	Entry.LockReason = FText::GetEmpty();
	FFF_AtlasCountryInputTestAccess::Refresh(*Fixture.Screen);
	TestTrue(TEXT("해금 즉시 핀 선택 가능"), Fixture.FirstPin->IsSelectable());
	TestTrue(TEXT("해금 후 기존 선택 강조 복구"), Fixture.FirstPin->IsSelectedState());
	TestTrue(TEXT("해금 후 시작 버튼 사용 가능"), Fixture.Start->GetIsEnabled());
	TestEqual(TEXT("해금 후 사유 숨김"), Fixture.Reason->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("해금 후 시작 버튼 표시"), Fixture.Start->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("동기화는 선택 축제를 바꾸지 않음"), FFF_AtlasCountryInputTestAccess::Selected(*Fixture.Screen), Entry.FestivalId);
	TestTrue(TEXT("동기화는 핀 객체를 재생성하지 않음"),
		FFF_AtlasCountryInputTestAccess::Pin(*Fixture.Screen, Entry.FestivalId) == Fixture.FirstPin);
	TestTrue(TEXT("동기화 전후 실제 포커스 유지"), FSlateApplication::Get().GetKeyboardFocusedWidget() == FocusBefore);
	TestEqual(TEXT("동기화는 시작 요청을 만들지 않음"), Fixture.Observer->StartRequests, 0);
	return true;
}

#endif
