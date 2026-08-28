// FF_ReportScreenWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// Report 화면 UMG 에셋을 코드로 부트스트랩하는 에디터 전용 도구다.
//
// === 이 파일의 계약 ===
// `FF.ReportUMG`는 **없는 WBP만 만든다.** 이미 있으면 레이아웃 콜백을 실행하지 않으므로
// 디자이너에서 손댄 계층·색·패딩이 그대로 보존된다. 덮어쓰는 명령은 만들지 않는다.
// 규칙 원본: docs/memory/wbp-codegen-workflow.md
//
// === 왜 코드로 만드는가 ===
// `.uasset`은 바이너리라 diff·머지가 안 된다. 골격을 코드로 두면 진짜 원본이 `.cpp`가 되어
// 리뷰·되돌리기가 가능하고, `BindWidget` 이름이 헤더와 어긋날 수 없다.
//
// === 골격은 코드, 마감은 에디터 ===
// 계층·위젯 이름·좌표·`BindWidget` 대상까지가 여기 담당이다.
// 색 미세조정·애니메이션·폰트 교체는 디자이너에서 손으로 한다. 패딩 4를 6으로 바꾸려고
// 컴파일 루프를 도는 것은 낭비다.
//
// 방식은 `Private/Building/UI/FF_BuildCatalogWbpBuilder.cpp`(한광훈)에서 가져왔다.
// 그 에셋은 한광훈 담당이므로 건드리지 않고 방식만 베낀다 (AGENTS.md 규칙 3).

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/Widgets/FF_MetricRowWidget.h"
#include "UI/Report/FF_ReportScreen.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "WidgetBlueprint.h"

namespace FFReportWbpBuilder
{
	// 생성 대상 경로. 런타임 soft reference·자동화·이 생성기 세 곳이 같은 문자열을 계약으로 쓰므로
	// 이름을 바꾸면 세 곳을 함께 고쳐야 한다.
	const FString UiRoot(TEXT("/Game/FestaFiesta/UI"));
	const FString CommonFolder(UiRoot / TEXT("Common"));
	const FString ReportFolder(UiRoot / TEXT("Report"));

	const FString MetricRowPackage(CommonFolder / TEXT("WBP_FF_MetricRow"));
	const FString ReportScreenPackage(ReportFolder / TEXT("WBP_FF_ReportScreen"));
	const FString RootLayoutPackage(UiRoot / TEXT("WBP_FF_UIRootLayout"));

	// 이미 만들어져 있는 타이틀 버튼을 Report의 `다음 날` 버튼으로 재사용한다.
	// 새로 만들면 CBS_ 스타일·포커스 링·아이콘 틴트가 전부 빠진다.
	const FString TitleButtonPackage(UiRoot / TEXT("Title/GlassOrbit/WBP_TitleMenuButton_GlassOrbit"));

	// === 목업 레이아웃 상수 ===
	// 원본은 SourceArt/UI/Mockups/ReportScreen/manifest.json이다.
	// 목업과 WBP가 따로 놀지 않게 하려고 같은 숫자를 여기 옮겨 둔다.
	// 목업을 다시 뽑으면 manifest를 보고 이 값을 갱신한다.
	constexpr float CanvasW = 1920.f;
	constexpr float CanvasH = 1080.f;
	constexpr float MarginX = 160.f;
	constexpr float TitleY = 104.f;
	constexpr float SubY = 146.f;
	constexpr float BodyTop = 328.f;
	constexpr float ColTop = 232.f;
	constexpr float ColBottom = 892.f;
	constexpr float RowH = 92.f;
	constexpr float RowGap = 14.f;
	constexpr float Col1X = 160.f;
	constexpr float Col1W = 620.f;
	constexpr float Col2X = 820.f;
	constexpr float Col2W = 470.f;
	constexpr float Col3X = 1330.f;
	constexpr float Col3W = 430.f;
	constexpr float BtnX = 1480.f;
	constexpr float BtnY = 958.f;
	constexpr float BtnW = 280.f;
	constexpr float BtnH = 68.f;

	// 구간 아이콘 표시 크기. **이 값을 브러시 Image Size에 반드시 넣는다.**
	// 텍스처 기본값(2048×512)으로 두면 줄 전체가 부풀어 오른다 (2026-08-10에 버튼에서 겪은 함정).
	constexpr float BandIconSize = 24.f;

	// 인게임 팔레트. 타이틀보다 graphite 비중을 높인다.
	// 근거: docs/memory/ingame-operations-ui-flow-2026-08-07.md
	const FLinearColor ColorBackdrop(0.016f, 0.020f, 0.026f, 0.96f);
	const FLinearColor ColorRowPanel(0.035f, 0.043f, 0.055f, 0.90f);
	const FLinearColor ColorTextPrimary = FLinearColor::FromSRGBColor(FColor(0xF4, 0xF7, 0xFB));
	const FLinearColor ColorTextMuted = FLinearColor::FromSRGBColor(FColor(0xBF, 0xD0, 0xE5));
	const FLinearColor ColorTextBody = FLinearColor::FromSRGBColor(FColor(0xEC, 0xF4, 0xFF));

	// 헬퍼는 FF_UIWbpBuilderCommon 을 그대로 쓴다. 이 파일이 들고 있던 사본은 2026-08-20 에 옮겼다.
	// 세 빌더에 같은 코드가 복붙돼 있었고 앞으로 생성기가 여섯 개 더 생긴다.
	using namespace FFUIWbpBuilder;

	/**
	 * 로그 접두어. 카테고리는 `LogFFUIWbpBuilder` 하나를 공유하고 어느 빌더인지는 이걸로 구분한다.
	 * 접두어를 공용 헬퍼에 박아두면 다른 화면에서 그대로 써서 로그가 거짓말을 한다.
	 */
	const TCHAR* const ReportLogPrefix = TEXT("FF_REPORT_UMG");

	// =========================================================================
	// WBP_FF_MetricRow — 캡션 + 숫자 + 구간 라벨 + 구간 아이콘
	// =========================================================================
	bool BuildMetricRow(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* Backdrop = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_RowPanel"));
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		UVerticalBox* LeftColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Left"));
		UHorizontalBox* BandBox = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Band"));

		// bIsVariable = true인 것만 BindWidget 대상이다.
		UTextBlock* Caption = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Caption"), true);
		UTextBlock* Value = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Value"), true);
		UTextBlock* Band = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Band"), true);
		UImage* BandIcon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Band"), true);

		if (!Root || !Backdrop || !Content || !LeftColumn || !BandBox || !Caption || !Value || !Band || !BandIcon)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Backdrop->SetBrushTintColor(FSlateColor(ColorRowPanel));
		if (UOverlaySlot* Slot = Root->AddChildToOverlay(Backdrop))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		if (UOverlaySlot* Slot = Root->AddChildToOverlay(Content))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetPadding(FMargin(24.f, 14.f, 24.f, 14.f));
		}

		// 왼쪽: 캡션 위, 구간 표시 아래
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(LeftColumn))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		// 화면 문구는 코드가 채운다. 여기서 미리보기 텍스트를 넣지 않는다 —
		// PIE 전까지 더미가 보여 "값이 안 들어온다"는 오진을 만든다.
		StyleText(*Caption, 20, ColorTextMuted, TEXT("Medium"));
		LeftColumn->AddChildToVerticalBox(Caption);

		if (UVerticalBoxSlot* Slot = LeftColumn->AddChildToVerticalBox(BandBox))
		{
			Slot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		// AT-011: 구간을 색으로만 알리지 않는다. 아이콘과 라벨을 함께 놓는다.
		// 아이콘 텍스처와 틴트는 UFF_MetricRowWidget::ApplyBand가 넣는다. 여기서 지정하지 않는다.
		FSlateBrush BandIconBrush = BandIcon->GetBrush();
		BandIconBrush.SetImageSize(FVector2D(BandIconSize, BandIconSize));
		BandIcon->SetBrush(BandIconBrush);
		if (UHorizontalBoxSlot* Slot = BandBox->AddChildToHorizontalBox(BandIcon))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}

		StyleText(*Band, 20, ColorTextBody, TEXT("SemiBold"));
		if (UHorizontalBoxSlot* Slot = BandBox->AddChildToHorizontalBox(Band))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		// 오른쪽: 큰 숫자
		StyleText(*Value, 44, ColorTextPrimary, TEXT("SemiBold"));
		Value->SetJustification(ETextJustify::Right);
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Value))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_ReportScreen — 3단 구조 (관측 사실 → 지표 영향 → 내일 바꿀 행동)
	// =========================================================================
	bool BuildReportScreen(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass* MetricRowClass, UClass* ButtonClass)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UImage* Backdrop = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Backdrop"));
		UVerticalBox* Column1 = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Observed"));
		UVerticalBox* Column2 = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Impact"));

		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Title"), true);
		UTextBlock* Breakdown = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_SatisfactionBreakdown"), true);
		UTextBlock* FinaleBonus = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_FinaleBonus"), true);
		UTextBlock* NextDayHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_NextDayHint"), true);
		UVerticalBox* Drilldown = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_CauseDrilldown"), true);

		if (!Root || !Backdrop || !Column1 || !Column2 || !Title || !Breakdown || !FinaleBonus || !NextDayHint || !Drilldown)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Backdrop->SetBrushTintColor(FSlateColor(ColorBackdrop));
		StretchOnCanvas(*Root, *Backdrop);

		StyleText(*Title, 44, ColorTextPrimary, TEXT("SemiBold"));
		PlaceOnCanvas(*Root, *Title, MarginX, TitleY, Col1W, 60.f);

		// 1단 — 관측 사실. 지표 4줄
		PlaceOnCanvas(*Root, *Column1, Col1X, BodyTop, Col1W, ColBottom - BodyTop);

		const TCHAR* RowNames[] = {
			TEXT("Row_SimSatisfaction"),
			TEXT("Row_ScoreSatisfaction"),
			TEXT("Row_Safety"),
			TEXT("Row_TodayRevenue")
		};

		for (const TCHAR* RowName : RowNames)
		{
			UUserWidget* Row = MakeWidgetOfClass(Blueprint, Tree, FName(RowName), MetricRowClass);
			if (!Row)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("FF_REPORT_UMG_FAIL: %s 을 만들 수 없다. WBP_FF_MetricRow가 먼저 컴파일돼야 한다."), RowName);
				return false;
			}

			// 목업의 행 높이(92)를 SizeBox로 고정한다.
			// `SetDesiredSizeInViewport`는 뷰포트에 직접 붙인 위젯에만 적용되므로 컨테이너 자식에는 쓸 수 없다.
			// VerticalBox에 Automatic으로만 넣으면 높이가 내용에 따라 흔들려 목업과 어긋난다.
			USizeBox* RowSizeBox = MakeWidget<USizeBox>(
				Blueprint, Tree, FName(*FString::Printf(TEXT("SizeBox_%s"), RowName)));
			if (!RowSizeBox)
			{
				return false;
			}

			RowSizeBox->SetHeightOverride(RowH);
			RowSizeBox->AddChild(Row);

			if (UVerticalBoxSlot* Slot = Column1->AddChildToVerticalBox(RowSizeBox))
			{
				Slot->SetPadding(FMargin(0.f, 0.f, 0.f, RowGap));
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}

		// 2단 — 지표 영향. 분해식
		PlaceOnCanvas(*Root, *Column2, Col2X, BodyTop, Col2W, ColBottom - BodyTop);

		// 두 지표를 왜 나눴는지가 이 줄로 설명되므로 툴팁을 두지 않는다.
		// 문구는 UFF_ReportScreen::RefreshBreakdown이 채운다.
		StyleText(*Breakdown, 28, ColorTextPrimary, TEXT("SemiBold"));
		Column2->AddChildToVerticalBox(Breakdown);

		StyleText(*FinaleBonus, 28, ColorTextMuted, TEXT("Medium"));
		FinaleBonus->SetJustification(ETextJustify::Right);
		if (UVerticalBoxSlot* Slot = Column2->AddChildToVerticalBox(FinaleBonus))
		{
			Slot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		}

		// 3단 — 내일 바꿀 행동. **자리만 잡고 비워 둔다.**
		// CauseId 롤업이 없어 채울 수 없다. 자리를 안 잡으면 드릴다운(AT-005) 붙일 때 다시 짜야 한다.
		PlaceOnCanvas(*Root, *Drilldown, Col3X, BodyTop, Col3W, ColBottom - BodyTop);

		// 마지막 날에 버튼이 왜 비활성인지 알려주는 문구. 코드가 채운다.
		StyleText(*NextDayHint, 20, ColorTextMuted, TEXT("Medium"));
		PlaceOnCanvas(*Root, *NextDayHint, MarginX, BtnY + 12.f, 700.f, 40.f);

		// `다음 날` 버튼 — 이 화면의 존재 이유다.
		// 이 버튼이 없으면 Day 2로 넘어갈 방법이 없다 (Stage 시스템이 Report만 자동 전환하지 않는다).
		if (ButtonClass)
		{
			UUserWidget* Button = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_NextDay"), ButtonClass);
			if (!Button)
			{
				return false;
			}
			PlaceOnCanvas(*Root, *Button, BtnX, BtnY, BtnW, BtnH);
		}
		else
		{
			// 버튼 WBP를 못 찾아도 화면 생성 자체는 통과시킨다. 없으면 에디터에서 직접 넣으면 되고,
			// 여기서 실패시키면 나머지 골격까지 못 만든다. 다만 조용히 넘기지 않는다.
			UE_LOG(LogFFUIWbpBuilder, Warning,
				TEXT("FF_REPORT_UMG_WARN: %s 를 찾지 못해 Button_NextDay를 만들지 못했다. ")
				TEXT("에디터에서 그 버튼 WBP를 Button_NextDay 이름으로 직접 넣어야 한다."),
				*TitleButtonPackage);
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_UIRootLayout — HUD / Screen / Panel / Toast 4개 레이어
	// =========================================================================
	bool BuildRootLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UOverlay* HudLayer = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("HUDLayer"), true);
		UCommonActivatableWidgetStack* ScreenStack =
			MakeWidget<UCommonActivatableWidgetStack>(Blueprint, Tree, TEXT("ScreenStack"), true);
		UCommonActivatableWidgetStack* PanelStack =
			MakeWidget<UCommonActivatableWidgetStack>(Blueprint, Tree, TEXT("PanelStack"), true);
		UOverlay* ToastLayer = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("ToastLayer"), true);

		if (!Root || !HudLayer || !ScreenStack || !PanelStack || !ToastLayer)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// !! HUDLayer·ToastLayer는 반드시 SelfHitTestInvisible !!
		// 둘은 화면 전체를 덮는 컨테이너다. Visible로 두면 화면 전체가 월드 클릭을 먹어서
		// 건축 배치 같은 월드 조작이 전부 죽는다. 2026-08-12에 WBP_TimeHud에서 실제로 발생했다
		// (한광훈 커밋 0f6eb33). ScreenStack·PanelStack은 입력을 받아야 하므로 Visible이다.
		HudLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ToastLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// 추가 순서가 곧 Z 순서다. HUD < Screen < Panel < Toast.
		const TArray<UWidget*> Layers = { HudLayer, ScreenStack, PanelStack, ToastLayer };
		for (UWidget* Layer : Layers)
		{
			if (UOverlaySlot* Slot = Root->AddChildToOverlay(Layer))
			{
				Slot->SetHorizontalAlignment(HAlign_Fill);
				Slot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		return true;
	}

	void RunReportUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_REPORT_UMG_BEGIN: 없는 WBP만 생성한다."));

		// MetricRow가 먼저다. ReportScreen이 그 GeneratedClass를 자식으로 넣기 때문이다.
		UWidgetBlueprint* MetricRow = EnsureWidgetBlueprint(
			ReportLogPrefix,
			MetricRowPackage,
			CommonFolder,
			*UFF_MetricRowWidget::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildMetricRow(Blueprint, Tree); });

		if (!MetricRow || !MetricRow->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("FF_REPORT_UMG_ABORT: WBP_FF_MetricRow가 없어 Report 화면을 만들 수 없다."));
			return;
		}

		UClass* ButtonClass = LoadGeneratedClass(TitleButtonPackage);

		EnsureWidgetBlueprint(
			ReportLogPrefix,
			ReportScreenPackage,
			ReportFolder,
			*UFF_ReportScreen::StaticClass(),
			[MetricRowClass = MetricRow->GeneratedClass.Get(), ButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildReportScreen(Blueprint, Tree, MetricRowClass, ButtonClass);
			});

		EnsureWidgetBlueprint(
			ReportLogPrefix,
			RootLayoutPackage,
			UiRoot,
			*UFF_UIRootLayout::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildRootLayout(Blueprint, Tree); });

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("FF_REPORT_UMG_END: 끝났다. 색·패딩·폰트 마감은 디자이너에서 한다."));
	}

	FAutoConsoleCommand ReportUmgCommand(
		TEXT("FF.ReportUMG"),
		TEXT("Report 화면 WBP 3종(WBP_FF_MetricRow / WBP_FF_ReportScreen / WBP_FF_UIRootLayout)을 만든다. "
			 "이미 있는 에셋은 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunReportUmg));
}

#endif // WITH_EDITOR
