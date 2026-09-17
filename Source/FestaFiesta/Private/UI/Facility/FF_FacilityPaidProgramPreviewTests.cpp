// Real UMG/WBP rendering with source festival/facility data and isolated sample resources.
// No scheduling command, economy transaction, festival save or personal preference is written.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "FF_FFZSFacilityDefinition.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "UObject/StrongObjectPtr.h"

namespace FFFacilityPaidProgramPreview
{
	class FPlayerlessInitialization
	{
	public:
		explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
			: Class(InClass), bPrevious(Class.bCanCallInitializedWithoutPlayerContext)
		{
			Class.bCanCallInitializedWithoutPlayerContext = true;
		}
		~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
	private:
		UWidgetBlueprintGeneratedClass& Class;
		bool bPrevious;
	};

	bool SaveCapture(FAutomationTestBase& Test, FWidgetRenderer& Renderer,
		const TSharedRef<SWidget>& Widget, const FIntPoint Size, const FString& Name)
	{
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(TEXT("Paid-program sRGB render target"), Target.Get())) { return false; }
		Renderer.DrawWidget(Target.Get(), Widget, FVector2D(Size.X, Size.Y), 0.0f, false);
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags;
		ReadFlags.SetLinearToGamma(false);
		if (!Test.TestTrue(TEXT("Real paid-program WBP supplies complete pixels"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == Size.X * Size.Y)) { return false; }
		int32 VisiblePixels = 0;
		for (const FColor& Pixel : Pixels) { VisiblePixels += Pixel.A > 0 && (Pixel.R > 30 || Pixel.G > 30 || Pixel.B > 30) ? 1 : 0; }
		Test.TestTrue(TEXT("Paid-program screenshot contains visible UI"), VisiblePixels > 10000);
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, Name + TEXT(".png"));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(TEXT("Paid-program PNG saved"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("Paid-program WBP capture: %s"), *FPaths::ConvertRelativePathToFull(Path)));
		return bSaved;
	}

	void CheckControl(FAutomationTestBase& Test, UWidget& Control, const FSlateRect& PanelRect)
	{
		const FSlateRect Rect = Control.GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(*FString::Printf(TEXT("%s retains a usable control size"), *Control.GetName()),
			Rect.GetSize().X >= 24 && Rect.GetSize().Y >= 24);
		Test.TestTrue(*FString::Printf(TEXT("%s remains inside the right dock"), *Control.GetName()),
			Rect.Left >= PanelRect.Left - 0.5 && Rect.Right <= PanelRect.Right + 0.5 &&
			Rect.Top >= PanelRect.Top - 0.5 && Rect.Bottom <= PanelRect.Bottom + 0.5);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityPaidProgramRenderedPreview,
	"FestaFiesta.UI.FacilityInfo.PaidProgramRenderedPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityPaidProgramRenderedPreview::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFFacilityPaidProgramPreview;
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		AddInfo(TEXT("SKIPPED: paid-program screenshots require a real RHI; use -RenderOffscreen."));
		return true;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* PickerClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityPaidProgramPicker>(nullptr,
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramPicker.WBP_FF_FacilityPaidProgramPicker_C")));
	auto* CardClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityPaidProgramCard>(nullptr,
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard.WBP_FF_FacilityPaidProgramCard_C")));
	const auto* Festival = LoadObject<UFF_FestivalDefinition>(nullptr,
		TEXT("/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan.DA_Festival_Busan"));
	const auto* Facility = LoadObject<UFFZSFacilityDefinition>(nullptr,
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_CraftExperience_SandArt.PDA_FFZS_Booth_CraftExperience_SandArt"));
	if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Actual picker WBP"), PickerClass) ||
		!TestNotNull(TEXT("Actual program-card WBP"), CardClass) || !TestNotNull(TEXT("Actual Busan festival definition"), Festival) ||
		!TestNotNull(TEXT("Actual SandArt facility definition"), Facility)) { return false; }
	const FFF_ProgramCard* Source = Festival->ProgramCatalog.FindByPredicate(
		[](const FFF_ProgramCard& Card) { return Card.ProgramId == TEXT("Program_SandArtWorkshop"); });
	if (!TestNotNull(TEXT("Source festival contains the compatible workshop"), Source)) { return false; }
	FPlayerlessInitialization PickerInitialization(*PickerClass);
	FPlayerlessInitialization CardInitialization(*CardClass);
	FFF_FacilityPaidProgramPickerViewData Data;
	Data.FacilityInstanceId = FGuid::NewGuid();
	Data.FacilityName = Facility->Spec.DisplayName;
	Data.AvailableGold = FMath::Max(7600, Source->CostGold);
	Data.AvailableStaff = FMath::Max(6, Source->RequiredStaff);
	FFF_FacilityPaidProgramCardViewData& CardData = Data.Cards.AddDefaulted_GetRef();
	CardData.ProgramId = Source->ProgramId;
	CardData.DisplayName = Source->DisplayName;
	CardData.Description = FText::Format(NSLOCTEXT("PaidProgramPreview", "Revenue", "당일 한정 프로그램 · 최대 매출 {0} Gold"), FText::AsNumber(Source->CalcMaxRevenue()));
	CardData.TimeText = FFFacilityInfoModel::FormatProgramTimeRange(Source->StartMinutes, Source->EndMinutes);
	CardData.CostGold = Source->CostGold;
	CardData.RequiredStaff = Source->RequiredStaff;
	CardData.CapacityPeople = Source->Capacity;
	CardData.TicketFeeGold = Source->FeePerPerson;
	CardData.MaxRevenueGold = Source->CalcMaxRevenue();
	CardData.CostText = FFFacilityInfoModel::FormatGold(true, Source->CostGold);
	CardData.StaffText = FText::Format(NSLOCTEXT("PaidProgramPreview", "Staff", "필요 인원 {0}명"), FText::AsNumber(Source->RequiredStaff));
	CardData.CapacityText = FText::Format(NSLOCTEXT("PaidProgramPreview", "Capacity", "정원 {0}명"), FText::AsNumber(Source->Capacity));
	CardData.TicketFeeText = FText::Format(NSLOCTEXT("PaidProgramPreview", "Fee", "참가비 {0} Gold/인"), FText::AsNumber(Source->FeePerPerson));
	CardData.AvailabilityText = FText::FromString(TEXT("구매·편성 가능"));
	CardData.bCanSchedule = true;
	bool bAllSaved = true;
	for (const FIntPoint Resolution : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		TStrongObjectPtr<UFF_FacilityPaidProgramPicker> Picker(CreateWidget<UFF_FacilityPaidProgramPicker>(World, PickerClass));
		if (!TestNotNull(TEXT("Playerless picker instance"), Picker.Get())) { return false; }
		const TSharedRef<SWidget> Slate = Picker->TakeWidget();
		Picker->ApplyViewData(Data);
		Picker->SetPickerOpen(true);
		auto* List = Cast<UScrollBox>(Picker->GetWidgetFromName(TEXT("ScrollBox_Programs")));
		auto* Confirm = Cast<UButton>(Picker->GetWidgetFromName(TEXT("Button_Confirm")));
		auto* Return = Cast<UButton>(Picker->GetWidgetFromName(TEXT("Button_ReturnToFacility")));
		auto* Border = Cast<UBorder>(Picker->GetWidgetFromName(TEXT("Border_Picker")));
		if (!TestNotNull(TEXT("Program list"), List) || !TestNotNull(TEXT("Purchase action"), Confirm) ||
			!TestNotNull(TEXT("Receipt return action"), Return) || !TestNotNull(TEXT("Right dock"), Border)) { return false; }
		auto* Card = Cast<UFF_FacilityPaidProgramCard>(List->GetChildAt(0));
		auto* Select = Card ? Cast<UButton>(Card->GetWidgetFromName(TEXT("Button_Select"))) : nullptr;
		if (!TestNotNull(TEXT("Source workshop is rendered in its actual card WBP"), Select)) { return false; }
		Select->OnClicked.Broadcast();
		TestTrue(TEXT("Review enables its separate explicit purchase action"), Confirm->GetIsEnabled());
		const FString Prefix = FString::Printf(TEXT("PaidProgramPurchase_%dx%d_"), Resolution.X, Resolution.Y);
		FWidgetRenderer Renderer(false, true);
		Picker->ForceLayoutPrepass();
		bAllSaved &= SaveCapture(*this, Renderer, Slate, Resolution, Prefix + TEXT("Review"));
		const FSlateRect PanelRect = Border->GetCachedGeometry().GetLayoutBoundingRect();
		TestTrue(TEXT("Purchase dock leaves the existing HUD and compact build shelf visible"),
			PanelRect.Left >= 0 && PanelRect.Right <= Resolution.X && PanelRect.Top >= 0 && PanelRect.Bottom <= Resolution.Y - 125.5);
		CheckControl(*this, *Confirm, PanelRect);
		CheckControl(*this, *Select, PanelRect);
		const FIntPoint DetailSize(FMath::RoundToInt(PanelRect.GetSize().X), FMath::RoundToInt(PanelRect.GetSize().Y));
		bAllSaved &= SaveCapture(*this, Renderer, Border->TakeWidget(), DetailSize, Prefix + TEXT("ReviewDetail"));
		if (auto* Details = Cast<UScrollBox>(Picker->GetWidgetFromName(TEXT("ScrollBox_PurchaseDetails"))))
		{
			Details->ScrollToEnd();
			Picker->ForceLayoutPrepass();
			bAllSaved &= SaveCapture(*this, Renderer, Slate, Resolution, Prefix + TEXT("ReviewScrolled"));
			CheckControl(*this, *Confirm, Border->GetCachedGeometry().GetLayoutBoundingRect());
		}
		// The fixture supplies an authoritative-success result without submitting a purchase.
		Picker->ShowPurchaseComplete(Source->ProgramId, Data.AvailableGold - Source->CostGold,
			Data.AvailableStaff - Source->RequiredStaff);
		Picker->ForceLayoutPrepass();
		bAllSaved &= SaveCapture(*this, Renderer, Slate, Resolution, Prefix + TEXT("Complete"));
		CheckControl(*this, *Return, Border->GetCachedGeometry().GetLayoutBoundingRect());
		bAllSaved &= SaveCapture(*this, Renderer, Border->TakeWidget(), DetailSize, Prefix + TEXT("CompleteDetail"));
	}
	return bAllSaved;
}

#endif
