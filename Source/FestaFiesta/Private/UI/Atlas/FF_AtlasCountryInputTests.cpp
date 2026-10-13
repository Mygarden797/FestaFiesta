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
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
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
		UTextBlock* Reason = nullptr;
		UImage* Lock = nullptr;
		TSharedPtr<SVirtualWindow> Window;
		TSharedPtr<SWidget> PreviousFocus;
		TUniquePtr<TGuardValue<TSoftObjectPtr<UFF_FestivalAtlasCatalog>>> CatalogSetting;

		~FFixture()
		{
			if (Window)
			{
				FSlateApplication::Get().ClearKeyboardFocus();
				FSlateApplication::Get().UnregisterVirtualWindow(Window.ToSharedRef());
				Window->SetContent(SNullWidget::NullWidget);
				Window.Reset();
				if (Screen) FFF_AtlasCountryInputTestAccess::ReleaseSlate(*Screen);
				if (PreviousFocus) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
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

		bool Initialize(FAutomationTestBase& Test)
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
			auto* Back = MakeButton(Tree, TEXT("Back"));
			Start = MakeButton(Tree, TEXT("Start"));
			FirstPin = MakeButton(Tree, TEXT("FirstPin"));
			SecondPin = MakeButton(Tree, TEXT("SecondPin"));
			Reason = Tree.ConstructWidget<UTextBlock>();
			Lock = Tree.ConstructWidget<UImage>();
			Root->AddChildToVerticalBox(Back);
			Root->AddChildToVerticalBox(FirstPin);
			Root->AddChildToVerticalBox(SecondPin);
			Root->AddChildToVerticalBox(Start);
			Root->AddChildToVerticalBox(Reason);
			Root->AddChildToVerticalBox(Lock);
			FFF_AtlasCountryInputTestAccess::Configure(*Screen, Start, Back, Reason, Lock);
			for (int32 Index = 0; Index < Catalog->Entries.Num(); ++Index)
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
