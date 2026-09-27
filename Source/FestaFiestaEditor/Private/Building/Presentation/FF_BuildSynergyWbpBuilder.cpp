// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildSynergyWbpBuilder.cpp
// 역할: 저장된 화면을 보존하면서 누락된 시설 조합 화면을 처음 만든다.

// One-time native UMG scaffold. Designer-owned trees and defaults are never regenerated.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR

#include "Building/Presentation/FF_BuildSynergyWidget.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFBuildSynergyWbpBuilder"

namespace FFBuildSynergyWbpBuilder
{
using namespace FFUIWbpBuilder;
const FString WidgetFolder(FFBuildUIAssetPaths::ControlsFolder);
const FString WidgetPackage(WidgetFolder / TEXT("WBP_FF_BuildSynergy"));

bool ValidateWidgetContract(const UWidgetBlueprint& Blueprint)
{
	if (!Blueprint.GeneratedClass || !Blueprint.GeneratedClass->IsChildOf(UFF_BuildSynergyWidget::StaticClass()) ||
		!Blueprint.WidgetTree || !Blueprint.WidgetTree->RootWidget ||
		!Cast<UBorder>(Blueprint.WidgetTree->FindWidget(TEXT("Border_Synergy"))) ||
		!Cast<UTextBlock>(Blueprint.WidgetTree->FindWidget(TEXT("Text_SynergyLegend"))) ||
		!Cast<UTextBlock>(Blueprint.WidgetTree->FindWidget(TEXT("Text_SynergyFooter")))) return false;
	for (int32 Index = 1; Index <= 3; ++Index)
	{
		if (!Cast<UTextBlock>(Blueprint.WidgetTree->FindWidget(FName(*FString::Printf(TEXT("Text_SynergyTitle_%d"), Index)))) ||
			!Cast<UTextBlock>(Blueprint.WidgetTree->FindWidget(FName(*FString::Printf(TEXT("Text_SynergyDetail_%d"), Index))))) return false;
	}
	return true;
}

bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
{
	const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
	UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Canvas_Synergy"));
	USizeBox* Size = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Size_Synergy"));
	UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Synergy"), true);
	UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Vertical_Synergy"));
	if (!Root || !Size || !Panel || !Rows) return false;
	Tree.RootWidget = Root;
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	Size->SetWidthOverride(344.0f);
	Size->AddChild(Panel);
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Palette.Paper);
	Brush.OutlineSettings = FSlateBrushOutlineSettings(10.0f, FSlateColor(Palette.LineStrong), 1.0f);
	Panel->SetBrush(Brush);
	Panel->SetBrushColor(FLinearColor::White);
	Panel->SetPadding(FMargin(14.0f, 8.0f));
	Panel->AddChild(Rows);
	UCanvasPanelSlot* Position = Root->AddChildToCanvas(Size);
	Position->SetAnchors(FAnchors(1.0f, 0.0f));
	Position->SetAlignment(FVector2D(1.0, 0.0));
	Position->SetPosition(FVector2D(-22.0, 96.0));
	Position->SetAutoSize(true);

	auto AddText = [&](const FName Name, const FText& Label, const int32 FontSize,
		const FLinearColor& Color, const bool bBold, const float BottomPadding, const bool bVariable = true)
	{
		UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, Name, bVariable);
		StyleText(*Text, FontSize, Color, bBold ? TEXT("SemiBold") : TEXT("Regular"));
		Text->SetText(Label);
		Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		Rows->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
	};
	AddText(TEXT("Text_SynergyHeading"), LOCTEXT("Heading", "함께 두면 생기는 수익"), 16, Palette.Ink, true, 4.0f, false);
	AddText(TEXT("Text_SynergyLegend"), LOCTEXT("Legend", "실선: 효과 적용 · 점선: 미적용 · 원: 1번 거리"),
		11, Palette.InkMuted, false, 4.0f);
	for (int32 Index = 1; Index <= 3; ++Index)
	{
		AddText(FName(*FString::Printf(TEXT("Text_SynergyTitle_%d"), Index)),
			FText::Format(LOCTEXT("ExampleTitle", "{0}. 함께 놓을 시설 · 조합 이름"), FText::AsNumber(Index)),
			13, Palette.Ink, true, 0.0f);
		AddText(FName(*FString::Printf(TEXT("Text_SynergyDetail_%d"), Index)),
			LOCTEXT("ExampleDetail", "최대 15m · 조합 보너스 +10%\n대상 수수료 0% → +10%\n설치하면 조합이 완성됩니다"),
			11, Palette.InkMuted, false, 4.0f);
	}
	AddText(TEXT("Text_SynergyFooter"), LOCTEXT("Footer", "중심 간 수평 거리 · 확정 후 수입에 반영"),
		10, Palette.InkMuted, false, 0.0f);
	return true;
}

void CreateBuildSynergyWidget()
{
	// Explicit guard also protects an unsaved in-memory package, including a failed earlier save.
	if (FPackageName::DoesPackageExist(WidgetPackage) || FindPackage(nullptr, *WidgetPackage))
	{
		const UWidgetBlueprint* Existing = LoadExistingBlueprint(WidgetPackage);
		if (!Existing || !ValidateWidgetContract(*Existing))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("FF_BUILD_SYNERGY_UMG_FAIL: existing asset preserved; check parent and named binding widgets: %s"), *WidgetPackage);
			return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_SYNERGY_UMG_REUSE: Designer tree and defaults preserved: %s"), *WidgetPackage);
		return;
	}
	UWidgetBlueprint* Blueprint = EnsureWidgetBlueprint(TEXT("FF_BUILD_SYNERGY_UMG"), WidgetPackage, WidgetFolder,
		*UFF_BuildSynergyWidget::StaticClass(), BuildLayout);
	if (Blueprint && !ValidateWidgetContract(*Blueprint))
	{
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_SYNERGY_UMG_FAIL: created asset has an invalid widget contract: %s"), *WidgetPackage);
		return;
	}
	if (Blueprint)
	{
		// The native fallback starts collapsed. The newly authored WBP must be visible
		// in UMG Designer; runtime presents it only while a relevant placement is active.
		UFF_BuildSynergyWidget* Defaults = Blueprint->GeneratedClass->GetDefaultObject<UFF_BuildSynergyWidget>();
		Defaults->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SaveAssetPackage(TEXT("FF_BUILD_SYNERGY_UMG"), *Blueprint);
	}
}

FAutoConsoleCommand CreateBuildSynergyWidgetCommand(TEXT("FF.CreateBuildSynergyWidget"),
	TEXT("Creates the missing editable facility synergy WBP; preserves all existing Designer changes."),
	FConsoleCommandDelegate::CreateStatic(&CreateBuildSynergyWidget));
} // namespace FFBuildSynergyWbpBuilder

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
