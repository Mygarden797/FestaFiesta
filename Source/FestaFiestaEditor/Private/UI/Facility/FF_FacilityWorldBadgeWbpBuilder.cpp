// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FacilityWorldBadgeWbpBuilder.cpp
// 역할: 시설 위 정보 표시 위젯의 편집용 초기 에셋을 만든다.

// Editor-only authoring. The saved WBP owns all visuals; existing designer edits are preserved.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Facility/FF_FacilityProgramMarkerWidget.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFFacilityWorldBadgeBuilder"
namespace FFFacilityWorldBadgeBuilder
{
	using namespace FFUIWbpBuilder;
	const TCHAR* const Prefix = TEXT("FF_FACILITY_WORLD_BADGE_UMG");
	const FString Folder(FFBuildUIAssetPaths::FacilityRevenueFolder);
	const TCHAR* const AmountFitKey = TEXT("FF.FacilityWorldBadge.AmountFit20260919");
	FSlateBrush Brush(FLinearColor Color, float Radius = 6.f)
	{
		FSlateBrush Result;
		Result.DrawAs = ESlateBrushDrawType::RoundedBox;
		Result.TintColor = FSlateColor(Color);
		Result.OutlineSettings = FSlateBrushOutlineSettings(Radius);
		return Result;
	}
	bool Layout(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		const FFF_UIPaletteInGame& P = GetDefault<UFF_UIPalette>()->InGame;
		auto Text = [&](FName Name, FText Value, int32 Size, FLinearColor Color, bool Variable = false)
		{
			UTextBlock* W = MakeWidget<UTextBlock>(BP, Tree, Name, Variable);
			StyleText(*W, Size, Color, TEXT("SemiBold"));
			W->SetText(Value);
			W->SetAutoWrapText(false);
			return W;
		};
		USizeBox* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_Badge"));
		Bounds->SetWidthOverride(220.f);
		Tree.RootWidget = Bounds;
		UBorder* Surface = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Badge"));
		Surface->SetBrush(Brush(P.Paper));
		Surface->SetPadding(FMargin(1.f));
		Bounds->AddChild(Surface);
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_Rows"));
		Surface->AddChild(Rows);
		USizeBox* Program = MakeWidget<USizeBox>(BP, Tree, TEXT("Panel_Program"), true);
		Program->SetHeightOverride(32.f);
		Rows->AddChildToVerticalBox(Program);
		UWidgetSwitcher* Switcher = MakeWidget<UWidgetSwitcher>(BP, Tree, TEXT("Switcher_Program"), true);
		Program->AddChild(Switcher);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FString Suffix = Index == 0 ? TEXT("Available") : TEXT("Scheduled");
			UBorder* State = MakeWidget<UBorder>(BP, Tree, FName(TEXT("Border_") + Suffix));
			State->SetBrush(Brush(Index == 0 ? P.TealDeep : P.Info));
			State->SetPadding(FMargin(10.f, 6.f));
			Switcher->AddChild(State);
			UHorizontalBox* Row = MakeWidget<UHorizontalBox>(BP, Tree, FName(TEXT("Horizontal_") + Suffix));
			State->AddChild(Row);
			USizeBox* IconSize = MakeWidget<USizeBox>(BP, Tree, FName(TEXT("Size_Icon") + Suffix));
			IconSize->SetWidthOverride(18.f);
			IconSize->SetHeightOverride(18.f);
			UCanvasPanel* Icon = MakeWidget<UCanvasPanel>(BP, Tree, FName(TEXT("Canvas_Icon") + Suffix));
			IconSize->AddChild(Icon);
			for (int32 Stroke = 0; Stroke < 7; ++Stroke)
			{
				UBorder* Line = MakeWidget<UBorder>(BP, Tree, FName(*FString::Printf(TEXT("Icon_%s_%d"), *Suffix, Stroke)));
				Line->SetBrush(Brush(P.White, 0.f));
				Line->SetPadding(FMargin(0));
				const FVector4 Rects[] = {FVector4(1,4,16,2), FVector4(1,16,16,2), FVector4(1,4,2,14),
					FVector4(15,4,2,14), FVector4(1,8,16,2), FVector4(5,1,2,6), FVector4(11,1,2,6)};
				PlaceOnCanvas(*Icon, *Line, Rects[Stroke].X, Rects[Stroke].Y, Rects[Stroke].Z, Rects[Stroke].W);
			}
			UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSize);
			IconSlot->SetPadding(FMargin(0, 0, 8.f, 0));
			IconSlot->SetVerticalAlignment(VAlign_Center);
			Row->AddChildToHorizontalBox(Text(FName(TEXT("Text_") + Suffix), Index == 0
				? LOCTEXT("Available", "프로그램 가능") : LOCTEXT("Scheduled", "프로그램 편성됨"), 13, P.White));
		}
		UBorder* Revenue = MakeWidget<UBorder>(BP, Tree, TEXT("Panel_Revenue"), true);
		Revenue->SetBrush(Brush(P.Paper));
		Revenue->SetPadding(FMargin(8.f, 7.f));
		Rows->AddChildToVerticalBox(Revenue);
		UHorizontalBox* RevenueRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Horizontal_Revenue"));
		Revenue->AddChild(RevenueRow);
		RevenueRow->AddChildToHorizontalBox(Text(TEXT("Text_RevenueLabel"), LOCTEXT("Today", "오늘 매출"), 10, P.InkMuted))
			->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Amount = Text(TEXT("Text_RevenueValue"), LOCTEXT("Unavailable", "—"), 11, P.Ink, true);
		Amount->SetJustification(ETextJustify::Right);
		UHorizontalBoxSlot* AmountSlot = RevenueRow->AddChildToHorizontalBox(Amount);
		AmountSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AmountSlot->SetPadding(FMargin(4.f, 0, 0, 0));
		BP.GetOutermost()->GetMetaData().SetValue(&BP, AmountFitKey, TEXT("1"));
		return true;
	}
	void Build()
	{
		EnsureWidgetBlueprint(Prefix, Folder / TEXT("WBP_FF_FacilityWorldBadge"), Folder,
			*UFF_FacilityProgramMarkerWidget::StaticClass(), Layout);
	}
	void ApplyAmountFitOnce()
	{
		UWidgetBlueprint* BP = LoadExistingBlueprint(Folder / TEXT("WBP_FF_FacilityWorldBadge"));
		if (!BP || !BP->WidgetTree || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(UFF_FacilityProgramMarkerWidget::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, AmountFitKey) == TEXT("1")) return;
		auto* Label = Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("Text_RevenueLabel")));
		auto* Amount = Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("Text_RevenueValue")));
		auto* Row = Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Panel_Revenue")));
		if (!Label || !Amount || !Row) return;
		auto* AmountSlot = Cast<UHorizontalBoxSlot>(Amount->Slot);
		if (!AmountSlot) return;
		// Only seeded typography/spacing changes. The 220-wide outer badge and program row stay intact.
		FSlateFontInfo Font = Label->GetFont();
		if (Font.Size == 11) { Font.Size = 10; Label->SetFont(Font); }
		Font = Amount->GetFont();
		if (Font.Size == 13) { Font.Size = 11; Amount->SetFont(Font); }
		if (Row->GetPadding() == FMargin(10.f, 7.f)) Row->SetPadding(FMargin(8.f, 7.f));
		if (AmountSlot->GetPadding() == FMargin(6.f, 0, 0, 0)) AmountSlot->SetPadding(FMargin(4.f, 0, 0, 0));
		BP->GetOutermost()->GetMetaData().SetValue(BP, AmountFitKey, TEXT("1"));
		SaveNewBlueprint(Prefix, *BP);
	}
	FAutoConsoleCommand Command(TEXT("FF.BuildFacilityWorldBadgeUMG"),
		TEXT("Create the editable program/revenue world badge only when missing. Preserve existing WBP edits."),
		FConsoleCommandDelegate::CreateStatic(&Build));
	FAutoConsoleCommand FitCommand(TEXT("FF.ApplyFacilityWorldBadgeAmountFit20260919"),
		TEXT("One-time internal amount fit repair; preserve the outer badge and unrelated designer values."),
		FConsoleCommandDelegate::CreateStatic(&ApplyAmountFitOnce));
}
#undef LOCTEXT_NAMESPACE
#endif
