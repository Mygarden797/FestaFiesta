// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Facility/FF_FacilityProgramMarkerSubsystem.h"

#include "Components/WidgetComponent.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/TextBlock.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

namespace
{
	struct FScopedBadgePlayerless
	{
		explicit FScopedBadgePlayerless(UWidgetBlueprintGeneratedClass& InClass)
			: Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
		{ Class.bCanCallInitializedWithoutPlayerContext = true; }
		~FScopedBadgePlayerless() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
		UWidgetBlueprintGeneratedClass& Class;
		bool bPrevious;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityProgramMarkerLifecycleTest,
	"FestaFiesta.UI.FacilityProgramMarker.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityProgramMarkerLifecycleTest::RunTest(const FString& Parameters)
{
	UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> GroundChecks(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> CollisionChecks(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> NavigationChecks(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> ProductionPolicies(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> InitialPhase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World->GetGameInstance();
	UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	UFF_FacilityProgramMarkerSubsystem* Markers = World->GetSubsystem<UFF_FacilityProgramMarkerSubsystem>();
	UFF_FestivalProgressionSubsystem* Progression = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	UFF_ProgramReservationSubsystem* Reservations = Game->GetSubsystem<UFF_ProgramReservationSubsystem>();
	if (!TestNotNull(TEXT("Zone authority"), Zones) || !TestNotNull(TEXT("Automatic marker subsystem"), Markers) ||
		!TestNotNull(TEXT("Progression owner"), Progression) || !TestNotNull(TEXT("Reservation owner"), Reservations))
	{
		return false;
	}
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Gameplay zone"), Zone) ||
		!TestTrue(TEXT("Accepted zone preset"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
		!TestTrue(TEXT("Register zone"), Zones->RegisterZone(Zone))) return false;

	// Bind before festival startup to prove definition changes invalidate the empty initial state.
	Markers->Tick(0.f);
	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(Game);
	UDataTable* Costs = NewObject<UDataTable>(Game);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost;
	Cost.InstallCost = 100;
	Costs->AddRow(TEXT("Cost.Booth.CraftExperience.MarkerA"), Cost);
	Costs->AddRow(TEXT("Cost.Booth.CraftExperience.MarkerB"), Cost);
	Costs->AddRow(TEXT("Cost.Booth.FoodStall.MarkerFood"), Cost);
	Definition->FestivalId = TEXT("FacilityProgramMarkerTest");
	Definition->FacilityCostTable = Costs;
	Definition->DailyVenueRentGold = 0;
	Progression->SetProgramReservations(Reservations);
	Progression->StartFestival(Definition);

	auto Install = [&](const FName DefinitionId, const int32 X)
	{
		FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
		Spec.FacilityDefinitionId = DefinitionId;
		Spec.InstallCostKey = NAME_None;
		FFFZSGridPlacement Placement;
		Placement.OriginCell = FIntPoint(X, 1000);
		const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(
			Spec, ZoneId, Placement, Zones->GetWorldRevision());
		const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
		TestTrue(*FString::Printf(TEXT("Install %s: %s"), *DefinitionId.ToString(),
			*Result.Message.ToString()), Result.bSucceeded);
		return Result.bSucceeded ? Command.FacilityInstanceId : FGuid();
	};
	const FGuid CraftA = Install(TEXT("Booth_CraftExperience_MarkerA"), 1000);
	const FGuid CraftB = Install(TEXT("Booth_CraftExperience_MarkerB"), 3000);
	const FGuid Food = Install(TEXT("Booth_FoodStall_MarkerFood"), 5000);
	if (!CraftA.IsValid() || !CraftB.IsValid() || !Food.IsValid()) return false;
	Markers->Tick(0.f);
	TestEqual(TEXT("Compatible unassigned facility is grey"), Markers->GetMarkerState(CraftA),
		EFF_FacilityProgramMarkerState::Available);
	TestEqual(TEXT("Incompatible installed facility has no marker"), Markers->GetMarkerState(Food),
		EFF_FacilityProgramMarkerState::Hidden);
	FFF_FacilityWorldBadgeData Badge;
	TestTrue(TEXT("Compatible facility exposes its active zero revenue"), Markers->GetBadgeData(CraftA, Badge) &&
		Badge.bShowRevenue && Badge.bEconomyAvailable && Badge.TodayGold == 0);
	Progression->AddFacilityRevenue(37, EFF_EarnReason::FacilityCommission, TEXT("MarkerFood"), Food);
	Progression->AddFacilityRevenue(19, EFF_EarnReason::FacilityCommission, TEXT("MarkerA"), CraftA);
	Markers->Tick(0.f);
	TestTrue(TEXT("A program-incompatible facility still displays its own real ledger"),
		Markers->GetBadgeData(Food, Badge) && Badge.ProgramState == EFF_FacilityProgramMarkerState::Hidden &&
		Badge.bShowRevenue && Badge.TodayGold == 37);
	TestTrue(TEXT("Same-type neighbour does not inherit another facility's revenue"),
		Markers->GetBadgeData(CraftB, Badge) && Badge.TodayGold == 0);

	AFFZSFacilityActor* Preview = World->SpawnActor<AFFZSFacilityActor>();
	Markers->Tick(0.f);
	TestEqual(TEXT("Unregistered preview never acquires a marker"), Markers->GetMarkerState(Preview->GetFacilityInstanceId()),
		EFF_FacilityProgramMarkerState::Hidden);
	TestNull(TEXT("Preview has no display component"), Preview->FindComponentByClass<UWidgetComponent>());

	TestEqual(TEXT("Schedule real program against real reservation owner"),
		Progression->TryScheduleProgram(TEXT("Program_SandArtWorkshop"), CraftA), EFF_ProgramPurchaseResult::Success);
	Markers->Tick(0.f);
	TestEqual(TEXT("Assigned facility turns blue via the schedule event"), Markers->GetMarkerState(CraftA),
		EFF_FacilityProgramMarkerState::Scheduled);
	TestEqual(TEXT("Same-type neighbour remains grey"), Markers->GetMarkerState(CraftB),
		EFF_FacilityProgramMarkerState::Available);
	Progression->SettleScheduledPrograms();
	Markers->Tick(0.f);
	TestEqual(TEXT("Finished program stays blue for the rest of today"), Markers->GetMarkerState(CraftA),
		EFF_FacilityProgramMarkerState::Scheduled);
	Progression->BeginNewDay(2);
	Markers->Tick(0.f);
	TestEqual(TEXT("Next-day reset returns the facility to grey"), Markers->GetMarkerState(CraftA),
		EFF_FacilityProgramMarkerState::Available);
	TestTrue(TEXT("Daily reset refreshes the retained facility ledger to zero"),
		Markers->GetBadgeData(Food, Badge) && Badge.bEconomyAvailable && Badge.TodayGold == 0);

	AFFZSFacilityActor* Facility = Zones->FindFacility(CraftA);
	Zones->UnregisterFacility(Facility);
	Markers->Tick(0.f);
	TestEqual(TEXT("Registry-only removal is detected without a command event"), Markers->GetMarkerState(CraftA),
		EFF_FacilityProgramMarkerState::Hidden);
	TestFalse(TEXT("Registry removal also releases the revenue projection"), Markers->GetBadgeData(CraftA, Badge));
	TestTrue(TEXT("Re-register the installed facility"), Zones->RegisterFacility(Facility));
	Markers->Tick(0.f);
	TestEqual(TEXT("Registered facility regains its marker"), Markers->GetMarkerState(CraftA),
		EFF_FacilityProgramMarkerState::Available);
	FFFZSCommand Remove;
	Remove.CommandId = FGuid::NewGuid();
	Remove.TransactionId = FGuid::NewGuid();
	Remove.Type = EFFZSCommandType::Remove;
	Remove.FacilityInstanceId = CraftA;
	Remove.ExpectedWorldRevision = Zones->GetWorldRevision();
	const FFFZSCommandResult RemovalResult = Zones->ExecuteCommand(Remove);
	TestTrue(*FString::Printf(TEXT("Remove installed facility through authority: %s"),
		*RemovalResult.Message.ToString()), RemovalResult.bSucceeded);
	Markers->Tick(0.f);
	TestEqual(TEXT("Removal deletes its marker state"), Markers->GetMarkerState(CraftA),
		EFF_FacilityProgramMarkerState::Hidden);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityProgramMarkerWidgetTest,
	"FestaFiesta.UI.FacilityProgramMarker.NonInteractiveWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityProgramMarkerWidgetTest::RunTest(const FString& Parameters)
{
	UFF_FacilityProgramMarkerWidget* Widget = NewObject<UFF_FacilityProgramMarkerWidget>();
	Widget->SetMarkerState(EFF_FacilityProgramMarkerState::Available);
	TestEqual(TEXT("Available marker cannot block facility clicks"), Widget->GetVisibility(),
		ESlateVisibility::HitTestInvisible);
	Widget->SetMarkerState(EFF_FacilityProgramMarkerState::Scheduled);
	TestEqual(TEXT("Scheduled marker remains non-interactive"), Widget->GetVisibility(),
		ESlateVisibility::HitTestInvisible);
	Widget->SetMarkerState(EFF_FacilityProgramMarkerState::Hidden);
	TestEqual(TEXT("Hidden marker does not occupy screen space"), Widget->GetVisibility(),
		ESlateVisibility::Collapsed);
	Widget->SetRevenue(true, false, 0);
	TestEqual(TEXT("Uninitialized economy has an explicit unknown amount"), Widget->GetRevenueText().ToString(), FString(TEXT("—")));
	Widget->SetRevenue(true, true, 0);
	TestEqual(TEXT("Active economy zero is a measured zero"), Widget->GetRevenueText().ToString(), FString(TEXT("0 Gold")));
	TestEqual(TEXT("Revenue-only badges do not block world input"), Widget->GetVisibility(), ESlateVisibility::HitTestInvisible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityWorldBadgeLayoutTest,
	"FestaFiesta.UI.FacilityProgramMarker.CollisionLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityWorldBadgeLayoutTest::RunTest(const FString& Parameters)
{
	const FVector2D Size(220, 68);
	const FBox2D Safe(FVector2D(16, 80), FVector2D(1264, 604));
	TArray<FBox2D> Occupied = { FBox2D(FVector2D(900, 80), FVector2D(1264, 600)) };
	FVector2D First, Repeat, Next;
	TestTrue(TEXT("Find position avoiding the detail panel"), FFFacilityWorldBadgeLayout::Place(
		FVector2D(910, 360), Size, Safe, Occupied, 8, 280, First));
	TestTrue(TEXT("Same inputs produce the same placement"), FFFacilityWorldBadgeLayout::Place(
		FVector2D(910, 360), Size, Safe, Occupied, 8, 280, Repeat) && First.Equals(Repeat));
	const FBox2D FirstRect(First, First + Size);
	TestFalse(TEXT("Both rows avoid the detail panel"), FirstRect.Intersect(Occupied[0]));
	Occupied.Add(FirstRect);
	TestTrue(TEXT("Dense neighbours receive a distinct nearby position"), FFFacilityWorldBadgeLayout::Place(
		FVector2D(910, 360), Size, Safe, Occupied, 8, 280, Next));
	TestFalse(TEXT("Program and revenue rectangle never overlaps its neighbour"), FBox2D(Next, Next + Size).Intersect(FirstRect));
	TestTrue(TEXT("Complete badge stays inside the HUD safe area"), Next.X >= Safe.Min.X && Next.Y >= Safe.Min.Y &&
		Next.X + Size.X <= Safe.Max.X && Next.Y + Size.Y <= Safe.Max.Y);
	TestFalse(TEXT("Insufficient space hides the badge instead of covering UI"), FFFacilityWorldBadgeLayout::Place(
		FVector2D(200, 150), Size, FBox2D(FVector2D(0), FVector2D(100)), {}, 8, 280, Next));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityWorldBadgeAuthoredTest,
	"FestaFiesta.UI.FacilityProgramMarker.AuthoredRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityWorldBadgeAuthoredTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(
		UFF_FacilityProgramMarkerSubsystem::GetDefaultWidgetClassPath().TryLoadClass<UUserWidget>());
	if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Authored world badge WBP"), Class)) return false;
	FScopedBadgePlayerless Playerless(*Class);
	TStrongObjectPtr<UFF_FacilityProgramMarkerWidget> Widget(CreateWidget<UFF_FacilityProgramMarkerWidget>(World, Class));
	if (!TestNotNull(TEXT("Authored badge"), Widget.Get())) return false;
	const TSharedRef<SWidget> Slate = Widget->TakeWidget();
	auto* Amount = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_RevenueValue")));
	UWidget* Program = Widget->GetWidgetFromName(TEXT("Panel_Program"));
	UWidget* Revenue = Widget->GetWidgetFromName(TEXT("Panel_Revenue"));
	if (!TestNotNull(TEXT("Authored amount"), Amount) || !TestNotNull(TEXT("Separate program row"), Program) ||
		!TestNotNull(TEXT("Separate revenue row"), Revenue)) return false;
	const FSlateFontInfo Font = Amount->GetFont();
	Widget->SetMarkerState(EFF_FacilityProgramMarkerState::Scheduled);
	Widget->SetRevenue(true, true, MAX_int32);
	Widget->ForceLayoutPrepass();
	TestTrue(TEXT("Updates preserve designer typography"), Amount->GetFont() == Font);
	TestTrue(TEXT("Program and revenue are siblings in the authored vertical layout"), Program->GetParent() == Revenue->GetParent());
	Widget->SetMarkerState(EFF_FacilityProgramMarkerState::Hidden);
	TestEqual(TEXT("Incompatible program hides only its own row"), Program->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Incompatible program retains actual revenue"), Revenue->IsVisible());
	Widget->SetRevenue(true, false, 0);
	TestEqual(TEXT("Authored unavailable text matches native fallback"), Amount->GetText().ToString(), FString(TEXT("—")));
	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		Widget->SetMarkerState(EFF_FacilityProgramMarkerState::Scheduled);
		Widget->SetRevenue(true, true, MAX_int32);
		FWidgetRenderer Renderer(false, true);
		const FVector2D Size(220, 72);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
		if (!TestNotNull(TEXT("Authored badge render target"), Target.Get())) return false;
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Widget->ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Slate, Size, .016f, false);
		}
		TestTrue(TEXT("Rendered program and revenue rows do not overlap"),
			Revenue->GetPaintSpaceGeometry().GetLayoutBoundingRect().Top >= Program->GetPaintSpaceGeometry().GetLayoutBoundingRect().Bottom);
		TestTrue(TEXT("Largest supported amount fits the authored row"),
			Amount->GetDesiredSize().X <= Amount->GetPaintSpaceGeometry().GetLocalSize().X + 1.f);
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Authored rows render"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(220, 72, Pixels, Png);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation");
		IFileManager::Get().MakeDirectory(*Directory, true);
		TestTrue(TEXT("Authored badge preview saved"), FFileHelper::SaveArrayToFile(Png, *(Directory / TEXT("FacilityWorldBadgeLargestAmount.png"))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityProgramMarkerRenderedPreview,
	"FestaFiesta.UI.FacilityProgramMarker.RenderedPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityProgramMarkerRenderedPreview::RunTest(const FString& Parameters)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		AddInfo(TEXT("SKIPPED: badge capture requires a real RHI; use -RenderOffscreen."));
		return true;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), World)) return false;
	TStrongObjectPtr<UFF_FacilityProgramMarkerWidget> Available(
		CreateWidget<UFF_FacilityProgramMarkerWidget>(World, UFF_FacilityProgramMarkerWidget::StaticClass()));
	TStrongObjectPtr<UFF_FacilityProgramMarkerWidget> Scheduled(
		CreateWidget<UFF_FacilityProgramMarkerWidget>(World, UFF_FacilityProgramMarkerWidget::StaticClass()));
	if (!TestNotNull(TEXT("Native available badge"), Available.Get()) ||
		!TestNotNull(TEXT("Native scheduled badge"), Scheduled.Get())) return false;
	Available->SetMarkerState(EFF_FacilityProgramMarkerState::Available);
	Scheduled->SetMarkerState(EFF_FacilityProgramMarkerState::Scheduled);
	const TSharedRef<SWidget> Preview = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(32, 44, 39)))
		.Padding(8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SBox).WidthOverride(156.f).HeightOverride(34.f)[Available->TakeWidget()]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(156.f).HeightOverride(34.f)[Scheduled->TakeWidget()]
			]
		];
	const FIntPoint Size(336, 50);
	FWidgetRenderer Renderer(false, true);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(
		FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true));
	if (!TestNotNull(TEXT("Badge sRGB render target"), Target.Get())) return false;
	Renderer.DrawWidget(Target.Get(), Preview, FVector2D(Size), 0.f, false);
	TArray<FColor> Pixels;
	FReadSurfaceDataFlags ReadFlags;
	ReadFlags.SetLinearToGamma(false);
	if (!TestTrue(TEXT("Native badges render all pixels"),
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags) &&
		Pixels.Num() == Size.X * Size.Y)) return false;
	const FColor Grey = Pixels[38 * Size.X + 85];
	const FColor Blue = Pixels[38 * Size.X + 249];
	TestTrue(TEXT("Unassigned badge renders neutral grey"),
		FMath::Abs(int32(Grey.R) - int32(Grey.G)) < 15 && FMath::Abs(int32(Grey.G) - int32(Grey.B)) < 15);
	TestTrue(TEXT("Assigned badge renders blue"), int32(Blue.B) > int32(Blue.R) + 40);
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = FPaths::Combine(Directory, TEXT("FacilityProgramMarkers.png"));
	const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
	TestTrue(TEXT("Badge preview PNG saved"), bSaved);
	AddInfo(FString::Printf(TEXT("Badge capture: %s"), *FPaths::ConvertRelativePathToFull(Path)));
	return bSaved;
}

#endif
