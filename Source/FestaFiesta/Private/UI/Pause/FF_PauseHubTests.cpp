// FF_PauseHubTests.cpp
// 역할: 일시정지 메뉴의 입력·화면 연결·재개 동작을 검증한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/Pause/FF_PauseHubScreen.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PauseHub_WidgetContract,
	"FestaFiesta.UI.Pause.WidgetContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PauseHub_WidgetContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr const TCHAR* PauseHubClassPath =
		TEXT("/Game/FestaFiesta/UI/Pause/WBP_FF_PauseHub.WBP_FF_PauseHub_C");

	const UFF_FestivalSettingsDeveloperSettings* Settings =
		UFF_FestivalSettingsDeveloperSettings::Get();
	TestNotNull(TEXT("일시정지 개발 설정을 로드한다"), Settings);
	if (Settings)
	{
		TestEqual(TEXT("ESC/F5 기본 목적지는 1단계 Pause Hub다"),
			Settings->PauseHubScreenClass.ToSoftObjectPath().ToString(), FString(PauseHubClassPath));
	}

	UClass* HubClass = LoadClass<UFF_PauseHubScreen>(nullptr, PauseHubClassPath);
	TestNotNull(TEXT("Pause Hub WBP 클래스를 로드한다"), HubClass);
	TestTrue(TEXT("Pause Hub WBP는 네이티브 PauseHubScreen을 상속한다"),
		HubClass && HubClass->IsChildOf(UFF_PauseHubScreen::StaticClass()));

	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(HubClass);
	UWidgetTree* Tree = GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
	TestNotNull(TEXT("Pause Hub WidgetTree가 저장됐다"), Tree);
	if (!Tree)
	{
		return false;
	}

	struct FRequiredWidget
	{
		const TCHAR* Name;
		UClass* Type;
	};
	const FRequiredWidget RequiredWidgets[] = {
		{ TEXT("Border_ViewportSurface"), UBorder::StaticClass() },
		{ TEXT("SafeZone_Viewport"), USafeZone::StaticClass() },
		{ TEXT("CanvasPanel_Responsive"), UCanvasPanel::StaticClass() },
		{ TEXT("VerticalBox_ResponsiveShell"), UVerticalBox::StaticClass() },
		{ TEXT("HorizontalBox_MainContent"), UHorizontalBox::StaticClass() },
		{ TEXT("SizeBox_MenuRail"), USizeBox::StaticClass() },
		{ TEXT("ScrollBox_Menu"), UScrollBox::StaticClass() },
		{ TEXT("ScrollBox_Context"), UScrollBox::StaticClass() },
		{ TEXT("HorizontalBox_Stats"), UHorizontalBox::StaticClass() },
		{ TEXT("Border_Header"), UBorder::StaticClass() },
		{ TEXT("SizeBox_BrandMark"), USizeBox::StaticClass() },
		{ TEXT("Image_BrandMark"), UImage::StaticClass() },
		{ TEXT("Border_MenuPanel"), UBorder::StaticClass() },
		{ TEXT("Border_ContextPanel"), UBorder::StaticClass() },
		{ TEXT("Border_Footer"), UBorder::StaticClass() },
		{ TEXT("Button_Resume"), UButton::StaticClass() },
		{ TEXT("Button_Settings"), UButton::StaticClass() },
		{ TEXT("Button_ReturnToAtlas"), UButton::StaticClass() },
		{ TEXT("Button_Exit"), UButton::StaticClass() },
		{ TEXT("Button_Close"), UButton::StaticClass() },
		{ TEXT("Image_FocusResume"), UImage::StaticClass() },
		{ TEXT("Image_FocusSettings"), UImage::StaticClass() },
		{ TEXT("Image_FocusReturnToAtlas"), UImage::StaticClass() },
		{ TEXT("Image_FocusExit"), UImage::StaticClass() },
		{ TEXT("Text_ReturnToAtlas"), UTextBlock::StaticClass() },
		{ TEXT("Text_FestivalName"), UTextBlock::StaticClass() },
		{ TEXT("Text_DayStage"), UTextBlock::StaticClass() },
		{ TEXT("Text_Clock"), UTextBlock::StaticClass() },
		{ TEXT("Text_Gold"), UTextBlock::StaticClass() },
		{ TEXT("Text_Visitors"), UTextBlock::StaticClass() },
		{ TEXT("Text_Congestion"), UTextBlock::StaticClass() },
		{ TEXT("Text_SelectionTitle"), UTextBlock::StaticClass() },
		{ TEXT("Text_SelectionDescription"), UTextBlock::StaticClass() },
		{ TEXT("Text_KeyHint"), UTextBlock::StaticClass() },
	};
	for (const FRequiredWidget& Contract : RequiredWidgets)
	{
		UWidget* Widget = Tree->FindWidget(FName(Contract.Name));
		TestNotNull(*FString::Printf(TEXT("필수 Pause Hub 위젯이 존재한다: %s"), Contract.Name), Widget);
		TestTrue(*FString::Printf(TEXT("필수 Pause Hub 위젯 타입이 맞다: %s"), Contract.Name),
			Widget && Widget->IsA(Contract.Type));
	}
	TestNull(TEXT("장식용 생성 히어로는 Pause Hub 계약에서 제거됐다"),
		Tree->FindWidget(TEXT("Image_FestivalHero")));
	for (const TCHAR* RemovedMenuWidget : {
		TEXT("Button_Keyboard"), TEXT("Image_FocusKeyboard"), TEXT("Text_Keyboard"), TEXT("Text_Keyboard_Description"),
		TEXT("Button_Account"), TEXT("Image_FocusAccount"), TEXT("Text_Account"), TEXT("Text_Account_Description"),
		TEXT("Text_HeaderHint") })
	{
		TestNull(FString(TEXT("요청으로 제거한 바로가기 위젯이 남아 있지 않다: ")) + RemovedMenuWidget,
			Tree->FindWidget(FName(RemovedMenuWidget)));
	}
	if (const UImage* BrandMark = Cast<UImage>(Tree->FindWidget(TEXT("Image_BrandMark"))))
	{
		TestNotNull(TEXT("브랜드 옆 꽃 장식에 실제 이미지가 연결돼 있다"), BrandMark->GetBrush().GetResourceObject());
	}
	for (const TCHAR* ForbiddenWidget : {
		TEXT("BackgroundBlur_World"), TEXT("Border_Scrim"), TEXT("ScaleBox_Viewport"),
		TEXT("SizeBox_Design1920x1080"), TEXT("CanvasPanel_Design") })
	{
		TestNull(*FString::Printf(TEXT("고정 프레임/월드 노출 위젯을 사용하지 않는다: %s"),
			ForbiddenWidget), Tree->FindWidget(FName(ForbiddenWidget)));
	}

	const TPair<const TCHAR*, const TCHAR*> MenuLabels[] = {
		{ TEXT("Text_Resume"), TEXT("계속하기") },
		{ TEXT("Text_Settings"), TEXT("시스템 설정") },
		{ TEXT("Text_ReturnToAtlas"), TEXT("축제 선택으로") },
		{ TEXT("Text_Exit"), TEXT("게임 종료") },
	};
	for (const TPair<const TCHAR*, const TCHAR*>& Contract : MenuLabels)
	{
		const UTextBlock* Label = Cast<UTextBlock>(Tree->FindWidget(FName(Contract.Key)));
		TestNotNull(*FString::Printf(TEXT("Pause Hub 메뉴 라벨이 존재한다: %s"), Contract.Key), Label);
		TestEqual(*FString::Printf(TEXT("Pause Hub 메뉴 문구가 목적과 일치한다: %s"), Contract.Key),
			Label ? Label->GetText().ToString() : FString(), FString(Contract.Value));
	}

	if (const UVerticalBox* Menu = Cast<UVerticalBox>(Tree->FindWidget(TEXT("VerticalBox_Menu"))))
	{
		const TArray<FName> MenuOrder = {
			TEXT("Text_MenuTitle"), TEXT("Button_Resume_Size"), TEXT("SizeBox_MenuDivider_0"),
			TEXT("Button_Settings_Size"), TEXT("SizeBox_MenuDivider_1"),
			TEXT("Button_ReturnToAtlas_Size"), TEXT("SizeBox_MenuDivider_2"), TEXT("Button_Exit_Size") };
		TestEqual(TEXT("축제 선택을 포함한 메뉴 항목 수가 맞다"), Menu->GetChildrenCount(), MenuOrder.Num());
		for (int32 Index = 0; Index < MenuOrder.Num() && Index < Menu->GetChildrenCount(); ++Index)
		{
			TestEqual(FString::Printf(TEXT("네 메뉴가 순서대로 각각 구분된다 / %d"), Index),
				Menu->GetChildAt(Index)->GetFName(), MenuOrder[Index]);
		}
	}
	for (const TCHAR* ButtonName : {
		TEXT("Button_Resume"), TEXT("Button_Settings"), TEXT("Button_ReturnToAtlas"), TEXT("Button_Exit") })
	{
		const UButton* Button = Cast<UButton>(Tree->FindWidget(FName(ButtonName)));
		TestTrue(FString(TEXT("메뉴를 키보드로 선택할 수 있다: ")) + ButtonName, Button && Button->GetIsFocusable());
	}
	if (const UScrollBox* MenuScroll = Cast<UScrollBox>(Tree->FindWidget(TEXT("ScrollBox_Menu"))))
	{
		TestEqual(TEXT("좁은 화면에서도 키보드 포커스를 따라 메뉴를 표시한다"),
			MenuScroll->GetScrollWhenFocusChanges(), EScrollWhenFocusChanges::InstantScroll);
	}

	if (const UTextBlock* KeyHint = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_KeyHint"))))
	{
		TestTrue(TEXT("하단 힌트가 ESC를 안내한다"), KeyHint->GetText().ToString().Contains(TEXT("ESC")));
		TestTrue(TEXT("하단 힌트가 F5를 안내한다"), KeyHint->GetText().ToString().Contains(TEXT("F5")));
	}

	auto TestAnchoredRegion = [this, Tree](const TCHAR* WidgetName,
		const FVector2D ExpectedMin, const FVector2D ExpectedMax, const FMargin ExpectedOffsets)
	{
		const UWidget* Widget = Tree->FindWidget(FName(WidgetName));
		const UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s는 반응형 캔버스에 배치됐다"), WidgetName), Slot);
		if (Slot)
		{
			TestEqual(*FString::Printf(TEXT("%s 최소 앵커"), WidgetName),
				Slot->GetAnchors().Minimum, ExpectedMin);
			TestEqual(*FString::Printf(TEXT("%s 최대 앵커"), WidgetName),
				Slot->GetAnchors().Maximum, ExpectedMax);
			TestEqual(*FString::Printf(TEXT("%s 반응형 오프셋"), WidgetName),
				Slot->GetOffsets(), ExpectedOffsets);
		}
	};
	TestAnchoredRegion(TEXT("Border_ViewportSurface"), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
		FMargin(0.f));
	TestAnchoredRegion(TEXT("SafeZone_Viewport"), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
		FMargin(0.f));
	TestAnchoredRegion(TEXT("VerticalBox_ResponsiveShell"), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
		FMargin(0.f));
	if (const UVerticalBox* Shell = Cast<UVerticalBox>(Tree->FindWidget(TEXT("VerticalBox_ResponsiveShell"))))
	{
		const TArray<FName> Regions = { TEXT("SizeBox_Header"), TEXT("HorizontalBox_MainContent"), TEXT("SizeBox_Footer") };
		TestEqual(TEXT("일시정지 셸은 헤더·본문·푸터 세 영역이다"), Shell->GetChildrenCount(), Regions.Num());
		for (int32 Index = 0; Index < Regions.Num() && Index < Shell->GetChildrenCount(); ++Index)
		{
			TestEqual(FString::Printf(TEXT("일시정지 영역 순서 %d"), Index), Shell->GetChildAt(Index)->GetFName(), Regions[Index]);
		}
	}
	if (const UHorizontalBox* Main = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("HorizontalBox_MainContent"))))
	{
		TestEqual(TEXT("본문은 메뉴와 축제 현황 두 영역이다"), Main->GetChildrenCount(), 2);
		if (Main->GetChildrenCount() == 2)
		{
			TestEqual(TEXT("메뉴가 왼쪽에 있다"), Main->GetChildAt(0)->GetFName(), FName(TEXT("SizeBox_MenuRail")));
			TestEqual(TEXT("축제 현황이 오른쪽에 있다"), Main->GetChildAt(1)->GetFName(), FName(TEXT("Border_ContextPanel")));
			const UHorizontalBoxSlot* ContextSlot = Cast<UHorizontalBoxSlot>(Main->GetChildAt(1)->Slot);
			TestTrue(TEXT("축제 현황은 남은 너비를 채운다"), ContextSlot && ContextSlot->GetSize().SizeRule == ESlateSizeRule::Fill);
		}
	}
	if (const UHorizontalBox* Stats = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("HorizontalBox_Stats"))))
	{
		TestEqual(TEXT("자금·방문객·혼잡 세 지표를 한 행에 묶는다"), Stats->GetChildrenCount(), 3);
	}

	const UBorder* ViewportSurface =
		Cast<UBorder>(Tree->FindWidget(TEXT("Border_ViewportSurface")));
	TestTrue(TEXT("Pause Hub 전체 화면 표면은 월드가 비치지 않는 완전 불투명 색이다"),
		ViewportSurface
		&& ViewportSurface->Background.TintColor.GetSpecifiedColor().A >= 0.999f);

	TArray<UWidget*> AllWidgets;
	Tree->GetAllWidgets(AllWidgets);
	FString AllCopy;
	for (const UWidget* Widget : AllWidgets)
	{
		if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			AllCopy += Text->GetText().ToString();
			AllCopy += TEXT("\n");
		}
	}
	for (const TCHAR* UnsupportedCopy : {
		TEXT("상점"), TEXT("Discord"), TEXT("서버"), TEXT("고객센터"),
		TEXT("계정 연동"), TEXT("교환 코드"), TEXT("채팅 번역") })
	{
		TestFalse(*FString::Printf(TEXT("프로젝트에 없는 가짜 기능을 노출하지 않는다: %s"), UnsupportedCopy),
			AllCopy.Contains(UnsupportedCopy));
	}
	return true;
}

namespace FFPauseHubLayoutTest
{
	constexpr float Tolerance = 1.f;

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance
			&& Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	bool CaptureAndCheck(FAutomationTestBase& Test, UFF_PauseHubScreen& Hub,
		const TSharedRef<SWidget>& Surface, const FIntPoint Size, const bool bScrolledToEnd = false)
	{
		const FString Variant = bScrolledToEnd ? TEXT("SampleContextScrolled") : TEXT("SampleContext");
		const FString Context = FString::Printf(TEXT("%s %dx%d"), *Variant, Size.X, Size.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(Context + TEXT(" 렌더 타깃"), Target.Get())) { return false; }
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Hub.ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		FlushRenderingCommands();
		const FSlateRect Viewport(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		auto RectFor = [&](const TCHAR* Name, const bool bRequireOnScreen = true)
		{
			const UWidget* Widget = Hub.GetWidgetFromName(Name);
			if (!Test.TestNotNull(Context + TEXT(" / ") + Name, Widget)) { return FSlateRect(); }
			const FSlateRect Rect = Widget->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(" 표시할 영역 / ") + Name, Widget->IsVisible());
			Test.TestTrue(Context + TEXT(" 실제 크기를 가진 영역 / ") + Name, Rect.GetSize().X > 0.f && Rect.GetSize().Y > 0.f);
			if (bRequireOnScreen) { Test.TestTrue(Context + TEXT(" 화면 안의 영역 / ") + Name, Contains(Viewport, Rect)); }
			return Rect;
		};
		auto CheckText = [&](const TCHAR* Name, const FSlateRect& Container, const int32 MinimumFontSize = 14)
		{
			const UTextBlock* Text = Cast<UTextBlock>(Hub.GetWidgetFromName(Name));
			if (!Test.TestNotNull(Context + TEXT(" 문구 / ") + Name, Text)) { return; }
			const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
			const FVector2D Desired = Text->GetDesiredSize();
			Test.TestTrue(Context + TEXT(" 읽을 수 있는 문구 / ") + Name,
				Text->IsVisible() && !Text->GetText().IsEmpty() && Text->GetFont().Size >= MinimumFontSize);
			Test.TestTrue(Context + TEXT(" 문구가 카드/버튼 안에 있다 / ") + Name, Contains(Container, Rect));
			Test.TestTrue(Context + TEXT(" 문구 높이가 잘리지 않는다 / ") + Name, Rect.GetSize().Y + Tolerance >= Desired.Y);
			Test.TestTrue(Context + TEXT(" 문구 폭이 잘리지 않는다 / ") + Name, Rect.GetSize().X + Tolerance >= Desired.X);
		};

		const FSlateRect Header = RectFor(TEXT("SizeBox_Header"));
		const FSlateRect Brand = RectFor(TEXT("Text_Brand"));
		const FSlateRect BrandMark = RectFor(TEXT("Image_BrandMark"));
		const FSlateRect Close = RectFor(TEXT("Button_Close"));
		Test.TestTrue(Context + TEXT(" 꽃 장식이 헤더 안에서 브랜드·닫기 버튼과 겹치지 않는다"),
			Contains(Header, BrandMark) && Brand.Right <= BrandMark.Left + Tolerance
			&& BrandMark.Right <= Close.Left + Tolerance);
		const FSlateRect Menu = RectFor(TEXT("Border_MenuPanel"));
		const FSlateRect MenuScroll = RectFor(TEXT("ScrollBox_Menu"));
		const FSlateRect Panel = RectFor(TEXT("Border_ContextPanel"));
		const FSlateRect Scroll = RectFor(TEXT("ScrollBox_Context"));
		const FSlateRect Footer = RectFor(TEXT("SizeBox_Footer"));
		Test.TestTrue(Context + TEXT(" 메뉴와 현황 카드가 겹치지 않는다"), Menu.Right <= Panel.Left + Tolerance);
		Test.TestTrue(Context + TEXT(" 헤더·본문·푸터가 겹치지 않는다"),
			Header.Bottom <= FMath::Min(Menu.Top, Panel.Top) + Tolerance
			&& FMath::Max(Menu.Bottom, Panel.Bottom) <= Footer.Top + Tolerance);
		Test.TestTrue(Context + TEXT(" 각 스크롤 영역이 패널 안에 있다"), Contains(Menu, MenuScroll) && Contains(Panel, Scroll));
		Test.TestEqual(Context + TEXT(" 푸터가 화면 바닥에 붙는다"), Footer.Bottom, Viewport.Bottom, Tolerance);

		const TPair<const TCHAR*, const TCHAR*> MenuEntries[] = {
			{ TEXT("Button_Resume"), TEXT("Text_Resume") }, { TEXT("Button_Settings"), TEXT("Text_Settings") },
			{ TEXT("Button_ReturnToAtlas"), TEXT("Text_ReturnToAtlas") },
			{ TEXT("Button_Exit"), TEXT("Text_Exit") }
		};
		float PreviousBottom = MenuScroll.Top;
		for (const auto& Entry : MenuEntries)
		{
			const FSlateRect Button = RectFor(Entry.Key);
			Test.TestTrue(Context + TEXT(" 모든 메뉴 동작이 스크롤 없이 보인다 / ") + Entry.Key, Contains(MenuScroll, Button));
			Test.TestTrue(Context + TEXT(" 메뉴 동작끼리 겹치지 않는다 / ") + Entry.Key, Button.Top >= PreviousBottom - Tolerance);
			Test.TestTrue(Context + TEXT(" 메뉴 클릭 영역을 확보한다 / ") + Entry.Key, Button.GetSize().X >= 44.f && Button.GetSize().Y >= 44.f);
			PreviousBottom = Button.Bottom;
			CheckText(Entry.Value, Button);
			CheckText(*(FString(Entry.Value) + TEXT("_Description")), Button, 13);
		}

		const FSlateRect Festival = RectFor(TEXT("Border_CurrentFestival"), false);
		const FSlateRect Stats = RectFor(TEXT("HorizontalBox_Stats"), false);
		const FSlateRect Selection = RectFor(TEXT("Border_SelectionCard"), false);
		const FSlateRect Guide = RectFor(TEXT("Border_GuideCard"), false);
		Test.TestTrue(Context + TEXT(" 현황·지표·선택 안내·조작 안내가 겹치지 않는다"),
			Festival.Bottom <= Stats.Top + Tolerance && Stats.Bottom <= Selection.Top + Tolerance
			&& Selection.Bottom <= Guide.Top + Tolerance);
		for (const FSlateRect& Card : { Festival, Stats, Selection, Guide })
		{
			Test.TestTrue(Context + TEXT(" 현황 카드가 패널 너비 안에 있다"),
				Card.Left >= Scroll.Left - Tolerance && Card.Right <= Scroll.Right + Tolerance);
		}
		if (bScrolledToEnd)
		{
			Test.TestTrue(Context + TEXT(" 끝까지 스크롤하면 조작 안내 전체를 읽는다"), Contains(Scroll, Guide));
		}
		else
		{
			Test.TestTrue(Context + TEXT(" 첫 화면에서 축제와 핵심 지표를 함께 본다"), Contains(Scroll, Festival) && Contains(Scroll, Stats));
		}
		for (const TCHAR* Name : { TEXT("Text_FestivalName"), TEXT("Text_DayStage"), TEXT("Text_Clock") })
		{
			CheckText(Name, Festival);
		}
		const TPair<const TCHAR*, const TCHAR*> StatEntries[] = {
			{ TEXT("Border_GoldCard"), TEXT("Text_Gold") }, { TEXT("Border_VisitorsCard"), TEXT("Text_Visitors") },
			{ TEXT("Border_CongestionCard"), TEXT("Text_Congestion") }
		};
		float PreviousRight = Stats.Left;
		for (const auto& Entry : StatEntries)
		{
			const FSlateRect Card = RectFor(Entry.Key, false);
			Test.TestTrue(Context + TEXT(" 지표 카드가 행 안에서 겹치지 않는다 / ") + Entry.Key,
				Contains(Stats, Card) && Card.Left >= PreviousRight - Tolerance);
			PreviousRight = Card.Right;
			CheckText(Entry.Value, Card);
			CheckText(*(FString(Entry.Key) + TEXT("_Label")), Card);
		}
		CheckText(TEXT("Text_SelectionTitle"), Selection);
		CheckText(TEXT("Text_SelectionDescription"), Selection);
		CheckText(TEXT("Text_GuideTitle"), Guide);
		CheckText(TEXT("Text_GuideBody"), Guide);
		CheckText(TEXT("Text_KeyHint"), Footer);
		CheckText(TEXT("Text_PausedState"), Footer);
		CheckText(TEXT("Text_SelectHint"), Footer);

		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Context + TEXT(" 실제 GPU 렌더 픽셀을 읽는다"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
			&& Pixels.Num() == Size.X * Size.Y)) { return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PauseHub/Layout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory,
			FString::Printf(TEXT("PauseHub_%s_%dx%d.png"), *Variant, Size.X, Size.Y));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Context + TEXT(" 레이아웃 캡처를 저장한다"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("일시정지 표시값 예시 캡처: %s"), *FPaths::ConvertRelativePathToFull(Path)));
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PauseHub_RenderedLayout,
	"FestaFiesta.UI.Pause.RenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PauseHub_RenderedLayout::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UWidgetBlueprintGeneratedClass* HubClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PauseHubScreen>(
		nullptr, TEXT("/Game/FestaFiesta/UI/Pause/WBP_FF_PauseHub.WBP_FF_PauseHub_C")));
	if (!TestNotNull(TEXT("표시 전용 에디터 월드"), World) || !TestNotNull(TEXT("저장된 Pause Hub WBP"), HubClass)) { return false; }
	if (!TestNull(TEXT("예시 표시값은 게임 인스턴스 없는 월드에서만 사용한다"), World->GetGameInstance())) { return false; }
	const bool bPreviouslyAllowed = HubClass->bCanCallInitializedWithoutPlayerContext;
	const bool bWasDirty = HubClass->GetOutermost()->IsDirty();
	HubClass->bCanCallInitializedWithoutPlayerContext = true;
	ON_SCOPE_EXIT { HubClass->bCanCallInitializedWithoutPlayerContext = bPreviouslyAllowed; };
	TStrongObjectPtr<UFF_PauseHubScreen> Hub(CreateWidget<UFF_PauseHubScreen>(World, HubClass));
	if (!TestNotNull(TEXT("저장된 허브를 재생성 없이 인스턴스화한다"), Hub.Get())) { return false; }
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Hub->TakeWidget()];
	Hub->SetVisibility(ESlateVisibility::Visible);
	// SampleContext는 글자 길이 검증용 표시값이다. 실제 축제 데이터, 세이브, 메뉴 동작을 변경하지 않는다.
	const TPair<const TCHAR*, const TCHAR*> SampleContext[] = {
		{ TEXT("Text_FestivalName"), TEXT("다대포 바다축제 · 가을 야간 불꽃놀이 특별 프로그램") },
		{ TEXT("Text_DayStage"), TEXT("DAY 12  ·  축제 진행") },
		{ TEXT("Text_Clock"), TEXT("현장 시각  23:59") },
		{ TEXT("Text_Gold"), TEXT("12,345,678 Gold") },
		{ TEXT("Text_Visitors"), TEXT("12,345명") },
		{ TEXT("Text_Congestion"), TEXT("혼잡 구역 128") },
		{ TEXT("Text_SelectionTitle"), TEXT("시스템 설정") },
		{ TEXT("Text_SelectionDescription"), TEXT("오디오·카메라·접근성·그래픽을 확인하고 저장 버튼으로 한 번에 적용합니다. 변경 사항은 저장 전까지 미리보기로 유지됩니다.") }
	};
	for (const auto& Entry : SampleContext)
	{
		UTextBlock* Text = Cast<UTextBlock>(Hub->GetWidgetFromName(Entry.Key));
		if (!TestNotNull(FString(TEXT("예시 표시값 필드 / ")) + Entry.Key, Text)) { return false; }
		const FText Value = FText::FromString(Entry.Value);
		Text->SetText(Value);
		Text->SetToolTipText(Value);
	}
	UScrollBox* ContextScroll = Cast<UScrollBox>(Hub->GetWidgetFromName(TEXT("ScrollBox_Context")));
	if (!TestNotNull(TEXT("내용 스크롤 영역"), ContextScroll)) { return false; }
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1080) })
	{
		ContextScroll->ScrollToStart();
		if (!FFPauseHubLayoutTest::CaptureAndCheck(*this, *Hub, Surface, Size)) { return false; }
		if (Size == FIntPoint(1280, 720))
		{
			// 위 캡처의 3프레임으로 작은 화면의 줄바꿈·스크롤 범위를 먼저 확정한다.
			// 큰 화면에서 ScrollToEnd를 요청한 뒤 축소하면 이전 범위의 끝으로 이동할 수 있다.
			ContextScroll->ScrollToEnd();
			if (!FFPauseHubLayoutTest::CaptureAndCheck(*this, *Hub, Surface, Size, true)) { return false; }
		}
	}
	TestEqual(TEXT("표시값 검증이 저장된 허브 패키지를 변경하지 않는다"), HubClass->GetOutermost()->IsDirty(), bWasDirty);
	AddInfo(TEXT("SampleContext는 레이아웃 예시다. 실제 축제 값 갱신·물리 입력·메뉴 동작은 이 검사에서 실행하지 않는다."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
