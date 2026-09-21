// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/UserInterfaceSettings.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"
#include "Widgets/SVirtualWindow.h"

namespace
{
	const TCHAR* RevenuePanelPath = TEXT("/Game/FestaFiesta/UI/FacilityRevenue/WBP_FF_FacilityRevenueRanking.WBP_FF_FacilityRevenueRanking");
	const TCHAR* RevenueRowPath = TEXT("/Game/FestaFiesta/UI/FacilityRevenue/WBP_FF_FacilityRevenueRankRow.WBP_FF_FacilityRevenueRankRow");

	struct FRevenueFixture
	{
		TArray<TStrongObjectPtr<UFF_FacilityRevenueRankItem>> Owned;
		UFF_FacilityRevenueRankItem* Add(const FGuid Id, const TCHAR* Name, const int32 Today, const int32 Total)
		{
			TStrongObjectPtr<UFF_FacilityRevenueRankItem> Item(NewObject<UFF_FacilityRevenueRankItem>());
			Item->FacilityId = Id;
			Item->Name = FText::FromString(Name);
			Item->Location = FText::FromString(TEXT("자동 테스트 데이터"));
			Item->TodayGold = Today;
			Item->TotalGold = Total;
			UFF_FacilityRevenueRankItem* Result = Item.Get();
			Owned.Add(MoveTemp(Item));
			return Result;
		}
	};

	struct FScopedRevenuePlayerless
	{
		explicit FScopedRevenuePlayerless(UWidgetBlueprintGeneratedClass& InClass)
			: Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
		{ Class.bCanCallInitializedWithoutPlayerContext = true; }
		~FScopedRevenuePlayerless() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
		UWidgetBlueprintGeneratedClass& Class;
		bool bPrevious;
	};

	FText RevenueGold(const int64 Value)
	{
		return FText::Format(FText::FromString(TEXT("{0} Gold")), FText::AsNumber(Value));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueRankModelTest,
	"FestaFiesta.UI.FacilityRevenue.RankModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityRevenueRankModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FRevenueFixture Fixture;
	// Two installations with the same definition/name remain individual GUID ledger rows.
	auto* First = Fixture.Add(FGuid(0, 0, 0, 1), TEXT("같은 시설"), 40, 1000);
	auto* Second = Fixture.Add(FGuid(0, 0, 0, 2), TEXT("같은 시설"), 40, 500);
	auto* Highest = Fixture.Add(FGuid(0, 0, 0, 3), TEXT("다른 시설"), 80, 2000);
	auto* NoToday = Fixture.Add(FGuid(0, 0, 0, 4), TEXT("오늘 매출 없음"), 0, 100);
	TArray<UFF_FacilityRevenueRankItem*> Rows = {Second, NoToday, First, Highest};
	TestEqual(TEXT("Today sum is based on per-instance today ledger values"), FFFacilityRevenueRanking::Rank(Rows, false), int64(160));
	TestEqual(TEXT("Separate installations are not grouped by display name"), Rows.Num(), 4);
	TestTrue(TEXT("Today descending order and stable GUID tie order"),
		Rows[0] == Highest && Rows[1] == First && Rows[2] == Second && Rows[3] == NoToday);
	TestTrue(TEXT("Competition ranks retain ties and skip the occupied ordinal"),
		Highest->Rank == 1 && First->Rank == 2 && Second->Rank == 2 && NoToday->Rank == 4);
	TestTrue(TEXT("Share uses the same period's aggregate"), Highest->bHasShare && FMath::IsNearlyEqual(Highest->Share, .5));
	TestTrue(TEXT("A zero earner with a nonzero denominator has a valid zero share"), NoToday->bHasShare && NoToday->Share == 0.);
	TestTrue(TEXT("Bar proportions are relative to the leading facility, not total share"),
		Highest->RelativeRevenue == 1.f && FMath::IsNearlyEqual(First->RelativeRevenue, .5f));
	TestEqual(TEXT("Cumulative period has an independent sum"), FFFacilityRevenueRanking::Rank(Rows, true), int64(3600));
	TestTrue(TEXT("Cumulative ranking reads cumulative values"), Rows[0] == Highest && Rows[1] == First && Rows[2] == Second);
	TestTrue(TEXT("Period switch recalculates formerly tied ranks"), First->Rank == 2 && Second->Rank == 3);
	TestTrue(TEXT("Period switch recalculates share"), FMath::IsNearlyEqual(First->Share, 1000. / 3600.));
	TestTrue(TEXT("Ranking never rewrites authoritative input amounts"), First->TodayGold == 40 && First->TotalGold == 1000);

	First->TodayGold = Second->TodayGold = 0;
	Rows = {First, Second};
	TestEqual(TEXT("Zero-earning facilities have a zero period sum"), FFFacilityRevenueRanking::Rank(Rows, false), int64(0));
	for (const auto* Item : Rows)
		TestTrue(TEXT("Zero denominator is unavailable share, finite zero bar and shared rank"),
			!Item->bHasShare && Item->Share == 0. && FMath::IsFinite(Item->Share) && Item->RelativeRevenue == 0.f && Item->Rank == 1);
	Rows.Reset();
	TestEqual(TEXT("An empty ledger list has zero sum"), FFFacilityRevenueRanking::Rank(Rows, false), int64(0));

	Rows = {First, Second, Highest};
	for (auto* Item : Rows) Item->TodayGold = MAX_int32;
	TestEqual(TEXT("Aggregate cannot overflow a 32-bit facility amount"),
		FFFacilityRevenueRanking::Rank(Rows, false), int64(MAX_int32) * 3);
	TestTrue(TEXT("Large aggregate keeps a finite exact share"), First->bHasShare &&
		FMath::IsFinite(First->Share) && FMath::IsNearlyEqual(First->Share, 1. / 3.));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueAuthoredWidgetsTest,
	"FestaFiesta.UI.FacilityRevenue.AuthoredWidgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityRevenueAuthoredWidgetsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWidgetBlueprint* PanelBlueprint = LoadObject<UWidgetBlueprint>(nullptr, RevenuePanelPath);
	UWidgetBlueprint* RowBlueprint = LoadObject<UWidgetBlueprint>(nullptr, RevenueRowPath);
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Generated revenue panel WBP"), PanelBlueprint) ||
		!TestNotNull(TEXT("Generated revenue row WBP"), RowBlueprint) || !TestNotNull(TEXT("Editor world"), World)) return false;
	TestTrue(TEXT("Both revenue blueprints compiled successfully"), PanelBlueprint->Status != BS_Error && RowBlueprint->Status != BS_Error);
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(PanelBlueprint->GeneratedClass);
	auto* RowClass = Cast<UWidgetBlueprintGeneratedClass>(RowBlueprint->GeneratedClass);
	if (!TestNotNull(TEXT("Panel generated class"), PanelClass) || !TestNotNull(TEXT("Row generated class"), RowClass)) return false;
	TestTrue(TEXT("Panel and row use their native read-only presentation classes"),
		PanelClass->IsChildOf(UFF_FacilityRevenueRankingPanel::StaticClass()) && RowClass->IsChildOf(UFF_FacilityRevenueRankRow::StaticClass()));
	for (const FName Name : {FName(TEXT("Border_Panel")), FName(TEXT("List_Ranking")), FName(TEXT("Text_Total")),
		FName(TEXT("Text_Count")), FName(TEXT("Text_Empty")), FName(TEXT("Text_SelectedName")),
		FName(TEXT("Text_SelectedRank")), FName(TEXT("Text_SelectedShare")), FName(TEXT("Button_Today")),
		FName(TEXT("Button_Total")), FName(TEXT("Button_Close")), FName(TEXT("Button_Inspect"))})
	{
		UWidget* Widget = PanelBlueprint->WidgetTree ? PanelBlueprint->WidgetTree->FindWidget(Name) : nullptr;
		if (!TestTrue(*FString::Printf(TEXT("Panel Designer binding %s"), *Name.ToString()),
			Widget && Widget->bIsVariable && PanelBlueprint->WidgetVariableNameToGuidMap.Contains(Name))) return false;
	}
	for (const FName Name : {FName(TEXT("Border_Row")), FName(TEXT("Text_Rank")), FName(TEXT("Text_Name")),
		FName(TEXT("Text_Location")), FName(TEXT("Text_Amount")), FName(TEXT("Bar_Revenue")),
		FName(TEXT("Image_Thumbnail")), FName(TEXT("Text_ThumbnailFallback"))})
	{
		UWidget* Widget = RowBlueprint->WidgetTree ? RowBlueprint->WidgetTree->FindWidget(Name) : nullptr;
		if (!TestTrue(*FString::Printf(TEXT("Row Designer binding %s"), *Name.ToString()),
			Widget && Widget->bIsVariable && RowBlueprint->WidgetVariableNameToGuidMap.Contains(Name))) return false;
	}
	FScopedRevenuePlayerless PanelContext(*PanelClass), RowContext(*RowClass);
	TStrongObjectPtr<UFF_FacilityRevenueRankingPanel> Panel(CreateWidget<UFF_FacilityRevenueRankingPanel>(World, PanelClass));
	if (!TestNotNull(TEXT("Actual panel instance"), Panel.Get())) return false;
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	(void)Slate;
	auto* List = Cast<UFF_FacilityRevenueListView>(Panel->GetWidgetFromName(TEXT("List_Ranking")));
	auto* Today = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Today")));
	auto* Total = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Total")));
	auto* Inspect = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Inspect")));
	auto* Close = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Close")));
	auto* SumText = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Total")));
	auto* ShareText = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_SelectedShare")));
	auto* EmptyText = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Empty")));
	if (!TestNotNull(TEXT("Virtualized list"), List) || !Today || !Total || !Inspect || !Close || !SumText || !ShareText || !EmptyText) return false;
	TestEqual(TEXT("Authored entry class is retained"), List->GetEntryWidgetClass().Get(), static_cast<UClass*>(RowClass));
	FSlateFontInfo DesignerFont = SumText->GetFont();
	DesignerFont.Size += 1;
	SumText->SetFont(DesignerFont);
	const FButtonStyle DesignerTodayStyle = Today->GetStyle();
	FRevenueFixture Fixture;
	auto* First = Fixture.Add(FGuid(0, 1, 0, 1), TEXT("동일 시설"), 100, 150);
	auto* Second = Fixture.Add(FGuid(0, 1, 0, 2), TEXT("동일 시설"), 50, 800);
	TArray<UFF_FacilityRevenueRankItem*> Rows = {First, Second};
	Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows, false), true, Second->FacilityId);
	TestEqual(TEXT("Requested installation is selected by GUID even with equal names"), Panel->GetSelectedFacilityId(), Second->FacilityId);
	TestEqual(TEXT("Both installations remain separate list items"), List->GetNumItems(), 2);
	int32 PeriodRequests = 0;
	const FDelegateHandle PeriodHandle = Panel->OnPeriodChanged.AddLambda([&]()
	{
		++PeriodRequests;
		const FGuid Selected = Panel->GetSelectedFacilityId();
		Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows, Panel->IsCumulative()), true, Selected);
	});
	Total->OnClicked.Broadcast();
	TestTrue(TEXT("Cumulative tab is selected without disabling either readable focus target"), Panel->IsCumulative() && Total->GetIsEnabled() && Today->GetIsEnabled());
	TestTrue(TEXT("Selected cumulative label retains its full bright color"),
		CastChecked<UTextBlock>(Total->GetContent())->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::White));
	TestTrue(TEXT("Selected cumulative tab uses the authored selected surface"), Total->GetStyle().Normal == Total->GetStyle().Disabled);
	TestEqual(TEXT("Period refresh keeps selected installation through reordering"), Panel->GetSelectedFacilityId(), Second->FacilityId);
	TestEqual(TEXT("Cumulative UI total is recomputed from its ledger column"), SumText->GetText().ToString(), RevenueGold(950).ToString());
	Total->OnClicked.Broadcast();
	TestEqual(TEXT("Activating the selected cumulative tab does not refresh its data again"),PeriodRequests,1);
	Today->OnClicked.Broadcast();
	TestTrue(TEXT("Today tab returns to its ledger while both tabs stay readable"), !Panel->IsCumulative() && Today->GetIsEnabled() && Total->GetIsEnabled());
	Today->OnClicked.Broadcast();
	TestEqual(TEXT("Each period button dispatches one refresh"), PeriodRequests, 2);
	TestEqual(TEXT("Today amount is not the cumulative amount"), SumText->GetText().ToString(), RevenueGold(150).ToString());
	TestTrue(TEXT("Data and period refresh preserve Designer typography"), SumText->GetFont() == DesignerFont);
	TestTrue(TEXT("Returning to a period restores its original authored selected brushes"), Today->GetStyle().Normal == DesignerTodayStyle.Normal &&
		Today->GetStyle().Disabled == DesignerTodayStyle.Disabled);
	Rows = {First};
	Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows, false), true, Second->FacilityId);
	TestEqual(TEXT("Removed selection falls back to a currently installed row"), Panel->GetSelectedFacilityId(), First->FacilityId);
	First->TodayGold = 0;
	Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows, false), true, First->FacilityId);
	TestEqual(TEXT("Zero total share is unavailable, not invented zero percent"), ShareText->GetText().ToString(), FString(TEXT("—")));
	auto* FirstRevenue = Panel->GetWidgetFromName(TEXT("Border_FirstRevenue"));
	if (!TestNotNull(TEXT("Approved first-revenue explanation"), FirstRevenue)) return false;
	TestTrue(TEXT("Installed zero earners show the first-revenue explanation"), FirstRevenue->IsVisible());
	Rows.Reset();
	Panel->Present(Rows, 0, true, First->FacilityId);
	TestFalse(TEXT("No facilities uses the empty state instead of the first-revenue explanation"), FirstRevenue->IsVisible());
	TestFalse(TEXT("An empty list immediately clears stale selected GUID"), Panel->GetSelectedFacilityId().IsValid());
	TestFalse(TEXT("An empty list disables inspect"), Inspect->GetIsEnabled());
	TestEqual(TEXT("Known empty ledger still reports actual zero Gold"), SumText->GetText().ToString(), RevenueGold(0).ToString());
	TestTrue(TEXT("Known empty state is visible"), EmptyText->IsVisible() && EmptyText->GetText().ToString().Contains(TEXT("없습니다")));
	Panel->Present(Rows, 0, false, FGuid());
	TestEqual(TEXT("Unavailable ledger is distinct from zero"), SumText->GetText().ToString(), FString(TEXT("조회 불가")));
	TestTrue(TEXT("Unavailable ledger explains loading state"), EmptyText->GetText().ToString().Contains(TEXT("준비 중")));
	int32 InspectRequests = 0;
	const FDelegateHandle InspectHandle = Panel->OnInspectRequested.AddLambda([&]() { ++InspectRequests; });
	Inspect->OnClicked.Broadcast();
	TestEqual(TEXT("A stale disabled inspect event cannot open a removed facility"), InspectRequests, 0);
	Rows = {First};
	Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows, false), true, First->FacilityId);
	Inspect->OnClicked.Broadcast();
	TestEqual(TEXT("A selected live row can request its inspector"), InspectRequests, 1);
	int32 CloseRequests = 0;
	const FDelegateHandle CloseHandle = Panel->OnCloseRequested.AddLambda([&]() { ++CloseRequests; });
	Close->OnClicked.Broadcast();
	TestEqual(TEXT("Actual close button dispatches once"), CloseRequests, 1);
	// Exercise the actual SObjectWidget/SButton key handlers. In Slate, an unhandled
	// leaf event bubbles to its parent; a handled child activation stops before it.
	const FGeometry KeyboardGeometry = FGeometry::MakeRoot(FVector2D(420, 40), FSlateLayoutTransform());
	for (const FKey Key : {EKeys::Enter, EKeys::SpaceBar, EKeys::M, EKeys::B, EKeys::G, EKeys::R, EKeys::Q, EKeys::E})
	{
		const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
		TestTrue(*FString::Printf(TEXT("Panel consumes unhandled world-edit key %s"), *Key.ToString()),
			Slate->OnKeyDown(KeyboardGeometry, Event).IsEventHandled());
	}
	TestEqual(TEXT("Panel-level Enter/Space do not activate a hidden world command or inspector"), InspectRequests, 1);
	const TSharedRef<SWidget> InspectSlate = Inspect->TakeWidget();
	for (const FKey Key : {EKeys::Enter, EKeys::SpaceBar})
	{
		const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
		const FReply ChildReply = InspectSlate->OnKeyDown(KeyboardGeometry, Event);
		TestTrue(TEXT("A child inspect button consumes its own keyboard activation before parent bubbling"), ChildReply.IsEventHandled());
		if (!ChildReply.IsEventHandled()) Slate->OnKeyDown(KeyboardGeometry, Event);
		InspectSlate->OnKeyUp(KeyboardGeometry, Event);
	}
	TestEqual(TEXT("Enter and Space still activate the focused inspect button once each"), InspectRequests, 3);
	const FKeyEvent MoveKey(EKeys::M, FModifierKeysState(), 0, false, 0, 0);
	const FReply MoveChildReply = InspectSlate->OnKeyDown(KeyboardGeometry, MoveKey);
	TestFalse(TEXT("The normal button does not own the world move key"), MoveChildReply.IsEventHandled());
	TestTrue(TEXT("A child-unhandled move key is stopped when it bubbles to the ranking panel"),
		MoveChildReply.IsEventHandled() || Slate->OnKeyDown(KeyboardGeometry, MoveKey).IsEventHandled());
	const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(TEXT("Escape remains handled by the ranking screen"), Slate->OnKeyDown(KeyboardGeometry, Escape).IsEventHandled());
	TestEqual(TEXT("Escape closes the screen exactly once"), CloseRequests, 2);
	Panel->OnPeriodChanged.Remove(PeriodHandle);
	Panel->OnInspectRequested.Remove(InspectHandle);
	Panel->OnCloseRequested.Remove(CloseHandle);
	Panel->ReleaseSlateResources(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueVirtualizedPreviewTest,
	"FestaFiesta.UI.FacilityRevenue.VirtualizedPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityRevenueVirtualizedPreviewTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		AddInfo(TEXT("SKIPPED: revenue virtualization and pixels require real RHI with -RenderOffscreen."));
		return true;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(FString(RevenuePanelPath) + TEXT("_C")).TryLoadClass<UUserWidget>());
	auto* RowClass = Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(FString(RevenueRowPath) + TEXT("_C")).TryLoadClass<UUserWidget>());
	if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Panel WBP"), PanelClass) || !TestNotNull(TEXT("Row WBP"), RowClass)) return false;
	FScopedRevenuePlayerless PanelContext(*PanelClass), RowContext(*RowClass);
	TStrongObjectPtr<UFF_FacilityRevenueRankingPanel> Panel(CreateWidget<UFF_FacilityRevenueRankingPanel>(World, PanelClass));
	if (!TestNotNull(TEXT("Revenue panel instance"), Panel.Get())) return false;
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	auto* List = Cast<UFF_FacilityRevenueListView>(Panel->GetWidgetFromName(TEXT("List_Ranking")));
	auto* Body = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("Border_Panel")));
	if (!TestNotNull(TEXT("ListView"), List) || !TestNotNull(TEXT("Panel surface"), Body)) return false;
	if (!TestNotNull(TEXT("Adaptive compact frame is an authored binding"),
		Cast<USizeBox>(Panel->GetWidgetFromName(TEXT("Size_ClarityFrame"))))) return false;
	// Standalone captures must wait for the shared UI materials, just like BuildTools.
	TSet<UMaterialInterface*> Materials;
	auto Gather = [&Materials](const FSlateBrush& Brush)
	{ if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) Materials.Add(Material); };
	Panel->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (auto* B = Cast<UBorder>(W)) Gather(B->Background);
		if (auto* B = Cast<UButton>(W))
		{ Gather(B->GetStyle().Normal); Gather(B->GetStyle().Hovered); Gather(B->GetStyle().Pressed); Gather(B->GetStyle().Disabled); }
	});
	for (auto* Material : Materials) Material->EnsureIsComplete();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	FlushRenderingCommands();
	FRevenueFixture Fixture;
	TArray<UFF_FacilityRevenueRankItem*> Rows;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		const FString Name = FString::Printf(TEXT("테스트 시설 %03d"), Index + 1);
		Rows.Add(Fixture.Add(FGuid(0, 2, 0, Index + 1), *Name, (100 - Index) * 7, (Index + 1) * 11));
	}
	Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows, false), true, Rows[0]->FacilityId);
	List->ScrollToTop();
	auto Draw = [&](const int32 Width, const int32 Height, const TCHAR* Filename)
	{
		const FVector2D Size(Width, Height);
		const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
		if (!Target) return false;
		auto Window = SNew(SVirtualWindow).Size(Size);
		Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
		FHittestGrid Grid;
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			Panel->ForceLayoutPrepass();
			Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
		}
		const FSlateRect BodyRect = Body->GetPaintSpaceGeometry().GetLayoutBoundingRect();
		TestTrue(TEXT("Revenue panel stays above its 70-logical-unit bottom HUD reserve at project DPI"), BodyRect.Top >= 0 && BodyRect.Bottom <= Height - 69.5f * Dpi &&
			BodyRect.Left >= 0 && BodyRect.Right <= Width && BodyRect.GetSize().GetMin() > 0);
		TestTrue(TEXT("Ranking stays a compact 500-wide floating surface capped at 600 logical pixels"),
			FMath::IsNearlyEqual(BodyRect.GetSize().X / Dpi, 500.f, 1.f) && BodyRect.GetSize().Y / Dpi <= 601.f);
		TestTrue(TEXT("Ranking leaves the center of the world clear"), BodyRect.Left > Width * .65f);
		if (Rows.Num() <= 2)
			TestTrue(TEXT("Sparse and empty rankings shrink instead of reserving a full-height blank list"), BodyRect.GetSize().Y / Dpi < 480.f);
		TestEqual(TEXT("Every installation remains in the data source"), List->GetNumItems(), Rows.Num());
		if (Rows.Num() > 4)
			TestTrue(TEXT("Only viewport rows are generated"), List->GetDisplayedEntryWidgets().Num() > 0 && List->GetDisplayedEntryWidgets().Num() < Rows.Num());
		if (Rows.Num() >= 2)
		if (UUserWidget* FirstRow = List->GetEntryWidgetFromItem(Rows[0]))
			TestTrue(TEXT("The viewport has room for at least two complete revenue rows"),
				List->GetPaintSpaceGeometry().GetLocalSize().Y + 1.f >= 2.f * FirstRow->GetPaintSpaceGeometry().GetLocalSize().Y);
		const FSlateRect ListRect = List->GetPaintSpaceGeometry().GetLayoutBoundingRect();
		for (int32 Index = 0; Index < FMath::Min(2,Rows.Num()); ++Index)
		{
			UUserWidget* Row = List->GetEntryWidgetFromItem(Rows[Index]);
			if (!TestNotNull(TEXT("Both leading revenue rows are generated"), Row)) continue;
			const FSlateRect RowRect = Row->GetPaintSpaceGeometry().GetLayoutBoundingRect();
			TestTrue(TEXT("Each leading revenue row is fully visible at project DPI"),
				RowRect.Top >= ListRect.Top - 1.f && RowRect.Bottom <= ListRect.Bottom + 1.f);
			TestTrue(TEXT("Revenue rows remain compact and readable"),
				Row->GetPaintSpaceGeometry().GetLocalSize().Y <= 90.f);
			const FSlateRect NameRect = Row->GetWidgetFromName(TEXT("Text_Name"))->GetPaintSpaceGeometry().GetLayoutBoundingRect();
			const FSlateRect AmountRect = Row->GetWidgetFromName(TEXT("Text_Amount"))->GetPaintSpaceGeometry().GetLayoutBoundingRect();
			TestTrue(TEXT("Row name and amount have distinct horizontal regions"), NameRect.Right <= AmountRect.Left + 1.f);
		}
		TArray<FSlateRect> Controls;
		for (const TCHAR* Name : {TEXT("Button_Today"),TEXT("Button_Total"),TEXT("Button_Close"),TEXT("Button_Inspect")})
		{
			UWidget* W = Panel->GetWidgetFromName(FName(Name));
			const FSlateRect Rect = W->GetPaintSpaceGeometry().GetLayoutBoundingRect();
			TestTrue(TEXT("Every ranking action has a consistent 40px hit target"), FMath::IsNearlyEqual(Rect.GetSize().Y / Dpi,40.f,1.f));
			TestTrue(TEXT("Ranking actions remain within the paper"), Rect.Left >= BodyRect.Left && Rect.Right <= BodyRect.Right &&
				Rect.Top >= BodyRect.Top && Rect.Bottom <= BodyRect.Bottom);
			for (const FSlateRect& Other : Controls)
				TestFalse(TEXT("No ranking action overlaps another action"), Rect.Left < Other.Right -.5f && Rect.Right > Other.Left +.5f &&
					Rect.Top < Other.Bottom -.5f && Rect.Bottom > Other.Top +.5f);
			Controls.Add(Rect);
		}
		AddInfo(FString::Printf(TEXT("Revenue viewport %dx%d uses project DPI %.3f; list height %.1f logical units."),
			Width, Height, Dpi, List->GetPaintSpaceGeometry().GetLocalSize().Y));
		if (!Filename) return true;
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityRevenue");
		IFileManager::Get().MakeDirectory(*Directory, true);
		return FFileHelper::SaveArrayToFile(Png, *(Directory / Filename));
	};
	TestTrue(TEXT("720p fixture preview"), Draw(1280, 720, TEXT("RevenueRankingFixture720.png")));
	TestTrue(TEXT("1080p fixture preview"), Draw(1920, 1080, TEXT("RevenueRankingFixture1080.png")));
	UUserWidget* FirstRow = List->GetEntryWidgetFromItem(Rows[0]);
	if (TestNotNull(TEXT("Actual first visible row"), FirstRow))
	{
		auto* Amount = Cast<UTextBlock>(FirstRow->GetWidgetFromName(TEXT("Text_Amount")));
		auto* Bar = Cast<UProgressBar>(FirstRow->GetWidgetFromName(TEXT("Bar_Revenue")));
		if (TestNotNull(TEXT("Row amount field"), Amount) && TestNotNull(TEXT("Row bar"), Bar))
		{
			FSlateFontInfo AuthoredFont = Amount->GetFont();
			AuthoredFont.Size += 1;
			Amount->SetFont(AuthoredFont);
			Rows[0]->TodayGold += 9;
			Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows, false), true, Rows[0]->FacilityId);
			TestTrue(TEXT("Refreshed viewport draws"), Draw(1920, 1080, nullptr));
			TestEqual(TEXT("An existing item's changed revenue updates its visible row"), Amount->GetText().ToString(), RevenueGold(Rows[0]->TodayGold).ToString());
			TestTrue(TEXT("A live amount refresh preserves the authored row font"), Amount->GetFont() == AuthoredFont);
			TestTrue(TEXT("Updated leading bar is normalized"), FMath::IsNearlyEqual(Bar->GetPercent(), 1.f));
		}
	}
	// Sparse and zero-income installations are the case that exposed the original
	// full-height empty panel. Exercise actual data/state changes at both viewport sizes.
	Rows.SetNum(2);
	Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows,false),true,Rows[1]->FacilityId);
	TestTrue(TEXT("Two-row compact ranking at 720p"),Draw(1280,720,TEXT("RevenueRankingTwoRows720.png")));
	TestTrue(TEXT("Two-row compact ranking at 1080p"),Draw(1920,1080,TEXT("RevenueRankingTwoRows1080.png")));
	TestEqual(TEXT("Resizing retains the selected installation"),Panel->GetSelectedFacilityId(),Rows[1]->FacilityId);
	Rows.SetNum(1); Rows[0]->TodayGold = 0;
	Panel->Present(Rows, FFFacilityRevenueRanking::Rank(Rows,false),true,Rows[0]->FacilityId);
	TestTrue(TEXT("One zero-income row at 720p"),Draw(1280,720,TEXT("RevenueRankingFirstRevenue720.png")));
	TestTrue(TEXT("One zero-income row at 1080p"),Draw(1920,1080,TEXT("RevenueRankingFirstRevenue1080.png")));
	Rows.Reset(); Panel->Present(Rows,0,true,FGuid());
	TestTrue(TEXT("Empty ranking shrinks to its empty-state content"),Draw(1280,720,TEXT("RevenueRankingEmpty720.png")));
	Panel->ReleaseSlateResources(true);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
