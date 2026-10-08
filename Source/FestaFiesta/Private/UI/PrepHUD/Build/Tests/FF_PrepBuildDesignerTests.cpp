// FF_PrepBuildDesignerTests.cpp
// 역할: 준비 단계 건설 화면의 동작과 저장된 화면 연결을 자동 검증한다.

// Checks Designer edits on a transient instance; never saves or modifies a WBP asset.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildDesignerAppearanceTest,
	"FestaFiesta.UI.PrepBuild.DesignerAppearanceSurvivesRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildDesignerAppearanceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepBuildTileWidget>(nullptr,
		FFBuildUIAssetPaths::PrepBuildTileClass));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Actual Designer tile class"), Class) || !TestNotNull(TEXT("Editor world"), World)) { return false; }
	const bool Previous = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_PrepBuildTileWidget> Tile(CreateWidget<UFF_PrepBuildTileWidget>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = Previous;
	if (!TestNotNull(TEXT("Transient tile instance"), Tile.Get())) { return false; }
	auto* Size = Cast<USizeBox>(Tile->GetWidgetFromName(TEXT("SizeBox_Card")));
	auto* Card = Cast<UBorder>(Tile->GetWidgetFromName(TEXT("Border_Card")));
	auto* Name = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_Name")));
	auto* Cost = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_Cost")));
	auto* ThumbnailFrame = Cast<USizeBox>(Tile->GetWidgetFromName(TEXT("SizeBox_Thumbnail")));
	auto* Thumbnail = Cast<UImage>(Tile->GetWidgetFromName(TEXT("Image_Thumbnail")));
	auto* SynergyHighlight = Cast<UBorder>(Tile->GetWidgetFromName(TEXT("Border_SynergyHighlight")));
	auto* SynergyBadge = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_SynergyBadge")));
	auto* RequiredMissing = Cast<UBorder>(Tile->GetWidgetFromName(TEXT("Border_RequiredMissing")));
	auto* RequiredSatisfied = Cast<UBorder>(Tile->GetWidgetFromName(TEXT("Border_RequiredSatisfied")));
	auto* RequiredMissingText = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_RequiredMissing")));
	auto* RequiredSatisfiedText = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_RequiredSatisfied")));
	auto* ProgramBadge = Cast<UBorder>(Tile->GetWidgetFromName(TEXT("Border_ProgramBadge")));
	auto* ProgramText = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_ProgramBadge")));
	auto* ThumbnailBackdrop = Cast<UBorder>(Tile->GetWidgetFromName(TEXT("Border_ThumbnailBackdrop")));
	const auto* ProgramBrushProperty = FindFProperty<FStructProperty>(Class, TEXT("ProgramThumbnailBrush"));
	const auto* RequiredBrushProperty = FindFProperty<FStructProperty>(Class, TEXT("RequiredThumbnailBrush"));
	auto* StateLabel = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_State")));
	const FObjectPropertyBase* ItemProperty = FindFProperty<FObjectPropertyBase>(Class, TEXT("Item"));
	if (!TestNotNull(TEXT("Designer card SizeBox"), Size) || !TestNotNull(TEXT("Designer card border"), Card) ||
		!TestNotNull(TEXT("Designer name"), Name) || !TestNotNull(TEXT("Designer cost"), Cost) ||
		!TestNotNull(TEXT("Transient view-model slot"), ItemProperty) ||
		!TestNotNull(TEXT("Designer thumbnail frame"), ThumbnailFrame) ||
		!TestNotNull(TEXT("Designer thumbnail image"), Thumbnail) ||
		!TestNotNull(TEXT("Designer synergy outline"), SynergyHighlight) ||
		!TestNotNull(TEXT("Designer synergy badge"), SynergyBadge) ||
		!TestNotNull(TEXT("Designer required missing badge"), RequiredMissing) ||
		!TestNotNull(TEXT("Designer required satisfied badge"), RequiredSatisfied) ||
		!TestNotNull(TEXT("Designer required missing text"), RequiredMissingText) ||
		!TestNotNull(TEXT("Designer required satisfied text"), RequiredSatisfiedText) ||
		!TestNotNull(TEXT("Designer program badge"), ProgramBadge) ||
		!TestNotNull(TEXT("Designer program caption"), ProgramText) ||
		!TestNotNull(TEXT("Designer thumbnail surface"), ThumbnailBackdrop) ||
		!TestNotNull(TEXT("Authored program material property"), ProgramBrushProperty) ||
		!TestNotNull(TEXT("Authored required material property"), RequiredBrushProperty) ||
		!TestNotNull(TEXT("Designer installation state label"), StateLabel)) { return false; }
	const FSlateBrush AuthoredThumbnailBrush = ThumbnailBackdrop->Background;
	const FSlateBrush ProgramBrush = *ProgramBrushProperty->ContainerPtrToValuePtr<FSlateBrush>(Tile.Get());
	const FSlateBrush RequiredBrush = *RequiredBrushProperty->ContainerPtrToValuePtr<FSlateBrush>(Tile.Get());
	const auto* ProgramTintProperty = FindFProperty<FStructProperty>(Class, TEXT("ProgramCardTint"));
	const auto* RequiredTintProperty = FindFProperty<FStructProperty>(Class, TEXT("RequiredCardTint"));
	if (!TestNotNull(TEXT("Authored program color"), ProgramTintProperty) ||
		!TestNotNull(TEXT("Authored required color"), RequiredTintProperty)) return false;
	const FLinearColor ProgramTint = *ProgramTintProperty->ContainerPtrToValuePtr<FLinearColor>(Tile.Get());
	const FLinearColor RequiredTint = *RequiredTintProperty->ContainerPtrToValuePtr<FLinearColor>(Tile.Get());
	// Current WBPs use inexpensive rounded brushes; material brushes remain supported.
	// Both representations must preserve the Designer's base brush and state colors.
	auto AccentSurface = [&AuthoredThumbnailBrush](const FSlateBrush& Accent, const FLinearColor& Tint)
	{
		if (Accent.GetResourceObject()) return Accent;
		FSlateBrush Brush = AuthoredThumbnailBrush;
		Brush.TintColor = Tint;
		return Brush;
	};
	const FSlateBrush ProgramSurface = AccentSurface(ProgramBrush, ProgramTint);
	const FSlateBrush RequiredSurface = AccentSurface(RequiredBrush, RequiredTint);
	TestTrue(TEXT("Required and program surfaces remain visually distinct authored styles"), RequiredSurface != ProgramSurface);
	// These are instance-only edits made before PreConstruct captures the authored appearance.
	Size->SetWidthOverride(196.f);
	Size->SetHeightOverride(188.f);
	FSlateBrush AuthoredBrush = Card->Background;
	AuthoredBrush.TintColor = FLinearColor(0.35f, 0.22f, 0.09f, 1.f);
	Card->SetBrush(AuthoredBrush);
	FSlateFontInfo Font = Name->GetFont();
	Font.Size = 19;
	Name->SetFont(Font);
	const FSlateColor AuthoredCost(FLinearColor(0.18f, 0.12f, 0.62f, 1.f));
	Cost->SetColorAndOpacity(AuthoredCost);
	FSlateBrush AuthoredHighlight = SynergyHighlight->Background;
	AuthoredHighlight.OutlineSettings.Color = FSlateColor(FLinearColor(0.6f, 0.1f, 0.4f, 1.0f));
	AuthoredHighlight.OutlineSettings.Width = 3.0f;
	SynergyHighlight->SetBrush(AuthoredHighlight);
	const FText AuthoredBadge = FText::FromString(TEXT("함께 놓을 시설"));
	SynergyBadge->SetText(AuthoredBadge);
	FSlateFontInfo BadgeFont = SynergyBadge->GetFont();
	BadgeFont.Size = 14;
	SynergyBadge->SetFont(BadgeFont);
	FSlateBrush AuthoredRequiredBrush = RequiredMissing->Background;
	AuthoredRequiredBrush.TintColor = FSlateColor(FLinearColor(0.4f, 0.1f, 0.5f, 1.0f));
	RequiredMissing->SetBrush(AuthoredRequiredBrush);
	const FText AuthoredRequiredLabel = FText::FromString(TEXT("필수 배치"));
	RequiredMissingText->SetText(AuthoredRequiredLabel);
	FSlateFontInfo RequiredFont = RequiredMissingText->GetFont();
	RequiredFont.Size = 13;
	RequiredMissingText->SetFont(RequiredFont);
	FSlateBrush AuthoredProgramBrush = ProgramBadge->Background;
	AuthoredProgramBrush.TintColor = FSlateColor(FLinearColor(0.19f, 0.11f, 0.38f, 1.0f));
	ProgramBadge->SetBrush(AuthoredProgramBrush);
	const FText AuthoredProgramLabel = FText::FromString(TEXT("체험 프로그램"));
	ProgramText->SetText(AuthoredProgramLabel);
	FSlateFontInfo ProgramFont = ProgramText->GetFont();
	ProgramFont.Size = 12;
	ProgramText->SetFont(ProgramFont);
	const FSlateColor AuthoredProgramColor(FLinearColor(0.96f, 0.90f, 1.0f, 1.0f));
	ProgramText->SetColorAndOpacity(AuthoredProgramColor);
	ThumbnailFrame->SetWidthOverride(176.f);
	ThumbnailFrame->SetHeightOverride(88.f);
	TStrongObjectPtr<UTexture2D> WideTexture(UTexture2D::CreateTransient(400, 200));
	TStrongObjectPtr<UTexture2D> PortraitTexture(UTexture2D::CreateTransient(200, 400));
	if (!TestNotNull(TEXT("Wide texture fixture"), WideTexture.Get()) ||
		!TestNotNull(TEXT("Portrait texture fixture"), PortraitTexture.Get())) { return false; }
	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Booth_CraftExperience_DesignerFixture");
	Entry.DisplayName = FText::FromString(TEXT("디자이너 시설"));
	Entry.Category = EFFZSFacilityCategory::Booth;
	Entry.MinorToken = TEXT("CraftExperience");
	Entry.InstallCost = 300;
	Entry.bHasResolvedInstallCost = true;
	Entry.Thumbnail = WideTexture.Get();
	UFF_PrepBuildTileData* Data = NewObject<UFF_PrepBuildTileData>(Tile.Get());
	Data->Initialize(Entry, 1000, true);
	ItemProperty->SetObjectPropertyValue_InContainer(Tile.Get(), Data);
	const TSharedRef<SWidget> Slate = Tile->TakeWidget();
	for (int32 Pass = 0; Pass < 3; ++Pass) { Tile->RefreshItemPresentation(); }
	Slate->SlatePrepass(1.0f);
	TestTrue(TEXT("Native Slate construction preserves the Designer card dimensions"),
		Slate->GetDesiredSize().Equals(FVector2D(196, 188), 0.5));
	TestTrue(TEXT("Runtime refresh preserves the authored normal card brush"),
		Card->Background.TintColor == AuthoredBrush.TintColor);
	TestTrue(TEXT("Runtime refresh preserves the authored normal cost color"), Cost->GetColorAndOpacity() == AuthoredCost);
	TestEqual(TEXT("Runtime refresh preserves the authored name font size"), Name->GetFont().Size, Font.Size);
	TestEqual(TEXT("Designer styling still receives current item data"), Name->GetText().ToString(), Entry.DisplayName.ToString());
	TestTrue(TEXT("Runtime thumbnail fitting uses the Designer's enlarged 176 x 88 frame"),
		Thumbnail->GetDesiredSize().Equals(FVector2D(176, 88), 0.5));
	// Reusing a row for an image with another aspect must read the current WBP frame,
	// while leaving the frame's authoring values untouched.
	ThumbnailFrame->SetWidthOverride(160.f);
	ThumbnailFrame->SetHeightOverride(100.f);
	Entry.Thumbnail = PortraitTexture.Get();
	Data->Initialize(Entry, 1000, true);
	for (int32 Pass = 0; Pass < 3; ++Pass) { Tile->RefreshItemPresentation(); }
	Slate->SlatePrepass(1.0f);
	TestTrue(TEXT("Recycled portrait uses the edited frame and preserves texture aspect"),
		Thumbnail->GetDesiredSize().Equals(FVector2D(50, 100), 0.5));
	TestEqual(TEXT("Data refresh preserves the Designer's thumbnail width"), ThumbnailFrame->GetWidthOverride(), 160.f);
	TestEqual(TEXT("Data refresh preserves the Designer's thumbnail height"), ThumbnailFrame->GetHeightOverride(), 100.f);
	Data->RefreshGold(0, true);
	Tile->RefreshItemPresentation();
	TestFalse(TEXT("An unavailable item can show its distinct error state"), Cost->GetColorAndOpacity() == AuthoredCost);
	Data->RefreshGold(1000, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Returning to available restores the Designer color"), Cost->GetColorAndOpacity() == AuthoredCost);
	// Recommendation is independent of selection/affordability and must never restyle the card.
	Data->SetSynergyHighlighted(true);
	Tile->RefreshItemPresentation();
	Slate->SlatePrepass(1.0f);
	TestTrue(TEXT("Visible entry reads the recommendation from its current model"), Tile->IsSynergyHighlighted());
	TestTrue(TEXT("Synergy accent cannot intercept the tile or favorite button"),
		SynergyHighlight->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("Showing synergy preserves authored outline style"),
		SynergyHighlight->Background.OutlineSettings == AuthoredHighlight.OutlineSettings);
	TestEqual(TEXT("Showing synergy preserves authored badge wording"), SynergyBadge->GetText().ToString(), AuthoredBadge.ToString());
	TestEqual(TEXT("Showing synergy preserves authored badge typography"), SynergyBadge->GetFont().Size, BadgeFont.Size);
	TestTrue(TEXT("Independent synergy layer preserves the original card brush and dimensions"),
		Card->Background.TintColor == AuthoredBrush.TintColor && Slate->GetDesiredSize().Equals(FVector2D(196, 188), 0.5));
	Data->RefreshGold(0, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Related but unaffordable item retains its relation without becoming available"),
		Tile->IsSynergyHighlighted() && !Data->IsAffordable() && Cost->GetColorAndOpacity() != AuthoredCost);
	Data->SetSynergyHighlighted(false);
	Tile->RefreshItemPresentation();
	TestFalse(TEXT("Clearing recommendation clears the displayed state"), Tile->IsSynergyHighlighted());
	TestTrue(TEXT("Cleared badge and outline both collapse"),
		SynergyHighlight->GetVisibility() == ESlateVisibility::Collapsed && SynergyBadge->GetVisibility() == ESlateVisibility::Collapsed);
	Data->SetSynergyHighlighted(true);
	Tile->RefreshItemPresentation();
	// Required progress is independent from combo recommendations. Both states can
	// coexist while the installation instruction changes to its completion label.
	Entry.bRequiredForSetup = true;
	Entry.CompatibleProgramCount = 2;
	Entry.CompatibleProgramNames = FText::FromString(TEXT("바다향기 공방, 선셋 요가"));
	Entry.bRequiredSetupSatisfied = false;
	Entry.RequiredInstalledCount = 0;
	Entry.RequiredMinimumCount = 1;
	Entry.MaxInstancesPerWorld = 1;
	Entry.CurrentInstanceCount = 0;
	Data->Initialize(Entry, 1000, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Missing requirement takes badge priority while program data and combination highlight coexist"),
		Tile->IsRequiredFacility() && !Tile->IsRequiredFacilitySatisfied() && Tile->IsSynergyHighlighted() && Tile->HasCompatiblePrograms() &&
		RequiredMissing->GetVisibility() == ESlateVisibility::HitTestInvisible &&
		RequiredSatisfied->GetVisibility() == ESlateVisibility::Collapsed &&
		ProgramBadge->GetVisibility() == ESlateVisibility::Collapsed &&
		ProgramText->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Program compatibility preserves the Designer badge brush, caption, font and color"),
		ProgramBadge->Background.TintColor == AuthoredProgramBrush.TintColor &&
		ProgramText->GetText().EqualTo(AuthoredProgramLabel) && ProgramText->GetFont().Size == ProgramFont.Size &&
		ProgramText->GetColorAndOpacity() == AuthoredProgramColor);
	TestTrue(TEXT("An unmet requirement takes surface priority while programs remain in the tooltip"),
		ThumbnailBackdrop->Background == RequiredSurface &&
		Card->Background.TintColor != AuthoredBrush.TintColor);
	const FSlateBrush RequiredCardBrush = Card->Background;
	TestTrue(TEXT("Program names are explained alongside the requirement and price"),
		Tile->GetToolTipText().ToString().Contains(TEXT("바다향기 공방")) &&
		Tile->GetToolTipText().ToString().Contains(TEXT("선셋 요가")));
	TestEqual(TEXT("Required facility explains the installation amount in the original state slot"),
		StateLabel->GetText().ToString(), FString(TEXT("1개 설치")));
	TestTrue(TEXT("Missing requirement preserves Designer brush and font with a count instruction"),
		RequiredMissing->Background.TintColor == AuthoredRequiredBrush.TintColor &&
		RequiredMissingText->GetFont().Size == RequiredFont.Size && RequiredMissingText->GetText().ToString() == TEXT("필수 · 1개 설치"));
	TestTrue(TEXT("Requirement minimum is available alongside the original price tooltip"),
		Tile->GetToolTipText().ToString().Contains(TEXT("필수 시설 1개를 설치")) &&
		Tile->GetToolTipText().ToString().Contains(Data->GetCostText().ToString()));
	Entry.bRequiredSetupSatisfied = true;
	Entry.RequiredInstalledCount = 1;
	Entry.CurrentInstanceCount = 1;
	Entry.bBuildLimitReached = true;
	Data->Initialize(Entry, 1000, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Installing required facility swaps badges while preserving combination recommendation"),
		Tile->IsRequiredFacilitySatisfied() && Tile->IsSynergyHighlighted() && Tile->HasCompatiblePrograms() &&
		RequiredMissing->GetVisibility() == ESlateVisibility::Collapsed &&
		RequiredSatisfied->GetVisibility() == ESlateVisibility::HitTestInvisible &&
		ProgramBadge->GetVisibility() == ESlateVisibility::Collapsed);
	TestEqual(TEXT("A capped facility displays a compact lock rather than its full rejection sentence"),
		StateLabel->GetText().ToString(), FString(TEXT("잠김")));
	const FLinearColor LockedTint = Card->Background.TintColor.GetSpecifiedColor();
	TestTrue(TEXT("The capacity lock overrides program and required colors with a neutral gray surface"),
		FMath::Abs(LockedTint.R - LockedTint.G) < .03f && FMath::Abs(LockedTint.G - LockedTint.B) < .04f &&
		ThumbnailBackdrop->Background != ProgramSurface && Card->Background.TintColor != FSlateColor(ProgramTint));
	auto* Favorite = Cast<UButton>(Tile->GetWidgetFromName(TEXT("Button_Favorite")));
	TestTrue(TEXT("Only decorative artwork is disabled; locked rows and favorites remain clickable"),
		!Thumbnail->GetIsEnabled() && Tile->GetIsEnabled() && Favorite && Favorite->GetIsEnabled());
	TestTrue(TEXT("The full limit reason remains available in the tooltip"),
		Tile->GetToolTipText().ToString().Contains(TEXT("1/1")) && Tile->GetToolTipText().ToString().Contains(TEXT("철거")));
	Entry.CurrentInstanceCount = 0;
	Entry.bBuildLimitReached = false;
	Data->Initialize(Entry, 1000, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Removing a capacity lock restores the authored program surface and artwork"),
		Thumbnail->GetIsEnabled() && ThumbnailBackdrop->Background == ProgramSurface &&
		Card->Background.TintColor == FSlateColor(ProgramTint));
	Entry.RequiredMinimumCount = 0;
	Data->Initialize(Entry, 1000, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Invalid minimum remains visibly required and never reports completion"),
		Tile->IsRequiredFacility() && !Tile->IsRequiredFacilitySatisfied() &&
		Tile->GetToolTipText().ToString().Contains(TEXT("설정 확인 필요")));
	Entry.RequiredMinimumCount = 1;
	Entry.bRequiredForSetup = false;
	Data->Initialize(Entry, 1000, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Removing requirement clears both badge surfaces on a reused row"),
		!Tile->IsRequiredFacility() && !Tile->IsRequiredFacilitySatisfied() &&
		RequiredMissing->GetVisibility() == ESlateVisibility::Collapsed && RequiredSatisfied->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Recycling a required program row into a program-only row restores its authored lavender surface"),
		ThumbnailBackdrop->Background == ProgramSurface &&
		Card->Background.TintColor != RequiredCardBrush.TintColor);
	Entry.CompatibleProgramCount = 0;
	Entry.CompatibleProgramNames = FText::GetEmpty();
	Data->Initialize(Entry, 1000, true);
	Tile->RefreshItemPresentation();
	TestTrue(TEXT("Reusing a program row for an ordinary facility clears both program badge surfaces"),
		!Tile->HasCompatiblePrograms() && ProgramBadge->GetVisibility() == ESlateVisibility::Collapsed &&
		ProgramText->GetVisibility() == ESlateVisibility::Collapsed && Tile->IsSynergyHighlighted());
	TestFalse(TEXT("Ordinary facility tooltip cannot retain the previous program names"),
		Tile->GetToolTipText().ToString().Contains(TEXT("바다향기 공방")));
	TestTrue(TEXT("Reusing a program card restores the exact ordinary thumbnail and card style"),
		ThumbnailBackdrop->Background == AuthoredThumbnailBrush && Card->Background == AuthoredBrush);
	Entry.bRequiredForSetup = true;
	Entry.CompatibleProgramCount = 1;
	Entry.CompatibleProgramNames = FText::FromString(TEXT("바다향기 공방"));
	Data->Initialize(Entry, 1000, true);
	Tile->RefreshItemPresentation();
	IUserListEntry::ReleaseEntry(*Tile.Get());
	TestFalse(TEXT("Virtualized row release clears the previous facility recommendation"), Tile->IsSynergyHighlighted());
	TestTrue(TEXT("Released row contains no stale recommendation badge"), SynergyHighlight->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Released row contains no stale required status"),
		!Tile->IsRequiredFacility() && !Tile->IsRequiredFacilitySatisfied() &&
		RequiredMissing->GetVisibility() == ESlateVisibility::Collapsed && RequiredSatisfied->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Released row contains no stale program compatibility badge"),
		!Tile->HasCompatiblePrograms() && ProgramBadge->GetVisibility() == ESlateVisibility::Collapsed &&
		ProgramText->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Released row contains no stale pearl thumbnail or required/program outline"),
		ThumbnailBackdrop->Background == AuthoredThumbnailBrush && Card->Background == AuthoredBrush);
	return true;
}

#endif
