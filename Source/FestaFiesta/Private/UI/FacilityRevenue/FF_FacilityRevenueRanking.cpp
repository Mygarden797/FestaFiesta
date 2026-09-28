// FF_FacilityRevenueRanking.cpp
// 역할: 시설별 수입 순위를 표시하고 선택 시설로 이동하는 요청을 전달한다.

#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Reply.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/FacilityCodex/FF_FacilityCodexImage.h"

#define LOCTEXT_NAMESPACE "FFFacilityRevenueRanking"
int64 FFFacilityRevenueRanking::Rank(TArray<UFF_FacilityRevenueRankItem *> &Items, bool bCumulative)
{
	// 표시용 복사본에만 순위와 비율을 기록한다. 원본 수입 장부는 변경하지 않는다.
	for (UFF_FacilityRevenueRankItem* Item : Items)
		Item->Amount = FMath::Max(0, bCumulative ? Item->TotalGold : Item->TodayGold);
	Items.Sort([](const UFF_FacilityRevenueRankItem &A, const UFF_FacilityRevenueRankItem &B) {
		if (A.Amount != B.Amount)
			return A.Amount > B.Amount;
		const int32 Names = A.Name.ToString().Compare(B.Name.ToString());
		// 같은 이름과 수입인 시설은 고유 번호로 정렬해 매번 같은 순서를 유지한다.
		// 번호를 직접 비교하면 비교할 때마다 임시 문자열을 만들 필요가 없다.
		return Names != 0 ? Names < 0 : A.FacilityId < B.FacilityId;
	});
	int64 Sum = 0;
	for (const auto *Item : Items)
		Sum += Item->Amount;
	int32 Rank = 0, Previous = INDEX_NONE;
	const int32 Maximum = Items.IsEmpty() ? 0 : Items[0]->Amount;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		auto *Item = Items[Index];
		if (Item->Amount != Previous)
			Rank = Index + 1;
		Item->Rank = Rank;
		Previous = Item->Amount;
		Item->Share = Sum > 0 ? double(Item->Amount) / double(Sum) : 0.;
		Item->bHasShare = Sum > 0;
		Item->RelativeRevenue = Maximum > 0 ? float(Item->Amount) / Maximum : 0.f;
	}
	return Sum;
}
void UFF_FacilityRevenueRankRow::NativeOnListItemObjectSet(UObject *Object)
{
	ApplyItem(Cast<UFF_FacilityRevenueRankItem>(Object));
	IUserObjectListEntry::NativeOnListItemObjectSet(Object);
}
void UFF_FacilityRevenueRankRow::ApplyItem(const UFF_FacilityRevenueRankItem *Item)
{
	if (!Item)
		return;
	if (!bCapturedTint && Border_Row)
	{
		AuthoredTint = Border_Row->GetBrushColor();
		bCapturedTint = true;
	}
	Text_Rank->SetText(FText::AsNumber(Item->Rank));
	Text_Name->SetText(Item->Name);
	Text_Location->SetText(Item->Location);
	Text_Amount->SetText(FText::Format(LOCTEXT("Gold", "{0} Gold"), FText::AsNumber(Item->Amount)));
	if (Text_Share)
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = Format.MaximumFractionalDigits = 1;
		Text_Share->SetText(Item->bHasShare
			? FText::Format(LOCTEXT("RowShare", "비중 {0}%"), FText::AsNumber(Item->Share * 100., &Format))
			: LOCTEXT("RowShareUnavailable", "비중 —"));
	}
	Bar_Revenue->SetPercent(Item->RelativeRevenue);
	Image_Thumbnail->ShowThumbnail(Item->Thumbnail, Text_ThumbnailFallback);
}
void UFF_FacilityRevenueRankRow::NativeOnItemSelectionChanged(bool bSelected)
{
	if (Border_Row)
		Border_Row->SetBrushColor(bSelected ? SelectedTint : AuthoredTint);
	IUserObjectListEntry::NativeOnItemSelectionChanged(bSelected);
}
void UFF_FacilityRevenueRankRow::NativeOnEntryReleased()
{
	if (Border_Row)
		Border_Row->SetBrushColor(AuthoredTint);
	if (Image_Thumbnail)
		Image_Thumbnail->ShowThumbnail({}, Text_ThumbnailFallback);
	IUserObjectListEntry::NativeOnEntryReleased();
}
void UFF_FacilityRevenueRankingPanel::NativeConstruct()
{
	Super::NativeConstruct();
	Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseClicked);
	Button_Today->OnClicked.AddUniqueDynamic(this, &ThisClass::Today);
	Button_Total->OnClicked.AddUniqueDynamic(this, &ThisClass::Total);
	Button_Inspect->OnClicked.AddUniqueDynamic(this, &ThisClass::InspectClicked);
	List_Ranking->OnItemSelectionChanged().RemoveAll(this);
	List_Ranking->OnItemSelectionChanged().AddUObject(this, &ThisClass::SelectionChanged);
	UpdatePeriod();
}
void UFF_FacilityRevenueRankingPanel::NativeDestruct()
{
	Button_Close->OnClicked.RemoveAll(this);
	Button_Today->OnClicked.RemoveAll(this);
	Button_Total->OnClicked.RemoveAll(this);
	Button_Inspect->OnClicked.RemoveAll(this);
	List_Ranking->OnItemSelectionChanged().RemoveAll(this);
	Super::NativeDestruct();
}
void UFF_FacilityRevenueRankingPanel::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	UpdateFrameSize(Geometry.GetLocalSize());
}
void UFF_FacilityRevenueRankingPanel::UpdateFrameSize(const FVector2D& Viewport)
{
	if (!Size_ClarityFrame || Viewport.GetMin() <= 1.f) return;
	// The frame is a right-side floating surface. Leave the upper and lower HUD clear;
	// only the virtualized list uses the remaining space when there are many facilities.
	const float Width = FMath::Min(600.f, FMath::Max(1.f, Viewport.X - 48.f));
	const float Height = FMath::Min(DesiredFrameHeight, FMath::Max(1.f, Viewport.Y - 176.f));
	if (!FMath::IsNearlyEqual(Size_ClarityFrame->GetWidthOverride(), Width)) Size_ClarityFrame->SetWidthOverride(Width);
	if (!FMath::IsNearlyEqual(Size_ClarityFrame->GetHeightOverride(), Height)) Size_ClarityFrame->SetHeightOverride(Height);
}
void UFF_FacilityRevenueRankingPanel::Today()
{
	if (!bCumulative) return;
	bCumulative = false;
	UpdatePeriod();
	OnPeriodChanged.Broadcast();
}
void UFF_FacilityRevenueRankingPanel::Total()
{
	if (bCumulative) return;
	bCumulative = true;
	UpdatePeriod();
	OnPeriodChanged.Broadcast();
}
void UFF_FacilityRevenueRankingPanel::UpdatePeriod()
{
	for (UButton* Button : {Button_Today.Get(), Button_Total.Get()})
	{
		if (!Button) continue;
		const bool bSelected = Button == (bCumulative ? Button_Total.Get() : Button_Today.Get());
		const FButtonStyle& Authored = AuthoredPeriodStyles.FindOrAdd(Button->GetFName(), Button->GetStyle());
		FButtonStyle Style = Authored;
		// Use the saved selected surface without Slate's disabled-content desaturation.
		// Repeated activation is ignored by Today/Total while focus stays accessible.
		if (bSelected) Style.Normal = Style.Hovered = Style.Pressed = Authored.Disabled;
		Button->SetStyle(Style);
		Button->SetIsEnabled(true);
		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
			Label->SetColorAndOpacity(FSlateColor(bSelected ? SelectedPeriodTextColor : InactivePeriodTextColor));
	}
}
void UFF_FacilityRevenueRankingPanel::CloseClicked()
{
	OnCloseRequested.Broadcast();
}
void UFF_FacilityRevenueRankingPanel::InspectClicked()
{
	if (GetSelectedFacilityId().IsValid())
		OnInspectRequested.Broadcast();
}
void UFF_FacilityRevenueRankingPanel::Present(const TArray<UFF_FacilityRevenueRankItem *> &Rows, int64 Sum,
											  bool bAvailable, FGuid Preferred)
{
	TArray<UObject *> Objects;
	Objects.Reserve(Rows.Num());
	for (auto *Item : Rows)
		Objects.Add(Item);
	List_Ranking->SetListItems(Objects);
	// Virtualized rows may keep the same item UObject when only its ledger changes.
	for (UUserWidget *Entry : List_Ranking->GetDisplayedEntryWidgets())
		if (auto *Row = Cast<UFF_FacilityRevenueRankRow>(Entry))
			Row->ApplyItem(Row->GetListItem<UFF_FacilityRevenueRankItem>());
	Text_Total->SetText(bAvailable ? FText::Format(LOCTEXT("Gold", "{0} Gold"), FText::AsNumber(Sum))
								   : LOCTEXT("Unavailable", "조회 불가"));
	Text_Count->SetText(FText::Format(bCumulative ? LOCTEXT("TotalScope", "현재 설치 시설 {0}개 · 축제 누적")
												  : LOCTEXT("TodayScope", "현재 설치 시설 {0}개 · 오늘"),
									  FText::AsNumber(Rows.Num())));
	Text_Empty->SetText(bAvailable ? LOCTEXT("Empty", "설치된 수익 시설이 없습니다.")
								   : LOCTEXT("NotReady", "매출 정보를 준비 중입니다."));
	Text_Empty->SetVisibility(Rows.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (Border_FirstRevenue)
		Border_FirstRevenue->SetVisibility(bAvailable && !Rows.IsEmpty() && Sum == 0
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	// The Pearl title, tabs, total, padding, and inspect action use 300 logical
	// pixels. Leave two full 88px rows and a small rounding reserve in sparse lists.
	DesiredFrameHeight = FMath::Min(600.f, 302.f + FMath::Clamp(Rows.Num(), 1, 4) * 88.f
		+ (bAvailable && !Rows.IsEmpty() && Sum == 0 ? 60.f : 0.f));
	UpdateFrameSize(GetCachedGeometry().GetLocalSize());
	UFF_FacilityRevenueRankItem *Selected = nullptr;
	for (auto *Item : Rows)
		if (Item->FacilityId == Preferred)
		{
			Selected = Item;
			break;
		}
	if (!Selected && !Rows.IsEmpty())
		Selected = Rows[0];
	if (Selected)
		List_Ranking->SetSelectedItem(Selected);
	else
		List_Ranking->ClearSelection();
	SelectionChanged(Selected);
}
FGuid UFF_FacilityRevenueRankingPanel::GetSelectedFacilityId() const
{
	auto *Item = List_Ranking ? Cast<UFF_FacilityRevenueRankItem>(List_Ranking->GetSelectedItem()) : nullptr;
	return Item ? Item->FacilityId : FGuid();
}
void UFF_FacilityRevenueRankingPanel::SelectionChanged(UObject *Object)
{
	auto *Item = Cast<UFF_FacilityRevenueRankItem>(Object);
	Text_SelectedName->SetText(Item ? Item->Name : LOCTEXT("Select", "시설을 선택하세요"));
	Text_SelectedRank->SetText(Item ? FText::Format(LOCTEXT("Rank", "{0}위"), FText::AsNumber(Item->Rank))
									: FText::FromString(TEXT("—")));
	FNumberFormattingOptions Format;
	Format.MinimumFractionalDigits = 1;
	Format.MaximumFractionalDigits = 1;
	Text_SelectedShare->SetText(Item && Item->bHasShare ? FText::Format(LOCTEXT("Percent", "{0}%"),
																		FText::AsNumber(Item->Share * 100., &Format))
														: FText::FromString(TEXT("—")));
	Button_Inspect->SetIsEnabled(Item != nullptr);
}
bool UFF_FacilityRevenueRankingPanel::IsPointerOverPanel() const
{
	return IsVisible() && Border_Panel && FSlateApplication::IsInitialized() &&
		   Border_Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}
FReply UFF_FacilityRevenueRankingPanel::NativeOnKeyDown(const FGeometry &Geometry, const FKeyEvent &Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		CloseClicked();
		return FReply::Handled();
	}
	// This is the bubble phase: a focused child button can consume Enter/Space first.
	// Unhandled editing keys must not reach the world's retained build selection while
	// this replacement presentation owns keyboard focus. Preview/tunnel interception
	// would also suppress the row and button's ordinary keyboard activation.
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::M ||
		Key == EKeys::B || Key == EKeys::G || Key == EKeys::R || Key == EKeys::Q || Key == EKeys::E)
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}
bool UFF_FacilityRevenueRankingSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE || Type == EWorldType::GamePreview;
}
void UFF_FacilityRevenueRankingSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Collection.InitializeDependency<UFF_FacilityInfoSubsystem>();
	Super::Initialize(Collection);
	auto *Info = GetWorld()->GetSubsystem<UFF_FacilityInfoSubsystem>();
	Info->OnRevenueRankingRequested.AddUObject(this, &ThisClass::Open);
	Info->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SelectionChanged);
	auto *Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	Zones->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::WorldChanged);
	Zones->OnWorldSnapshotRestored.AddUniqueDynamic(this, &ThisClass::Changed);
	BindEconomy();
}
void UFF_FacilityRevenueRankingSubsystem::BindEconomy()
{
	auto *Game = GetWorld()->GetGameInstance();
	auto *Current = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (Current == Progression)
		return;
	if (Progression)
	{
		Progression->OnFacilityRevenueChanged.RemoveAll(this);
		Progression->OnFestivalDefinitionChanged.RemoveAll(this);
		Progression->OnTodayRevenueChanged.RemoveAll(this);
	}
	Progression = Current;
	if (Progression)
	{
		Progression->OnFacilityRevenueChanged.AddUniqueDynamic(this, &ThisClass::RevenueChanged);
		Progression->OnFestivalDefinitionChanged.AddUniqueDynamic(this, &ThisClass::Changed);
		Progression->OnTodayRevenueChanged.AddUniqueDynamic(this, &ThisClass::AmountChanged);
	}
}
void UFF_FacilityRevenueRankingSubsystem::Deinitialize()
{
	Close();
	if (auto *Info = GetWorld()->GetSubsystem<UFF_FacilityInfoSubsystem>())
	{
		Info->OnRevenueRankingRequested.RemoveAll(this);
		Info->OnSelectionChanged.RemoveAll(this);
	}
	if (auto *Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
	{
		Zones->OnWorldChanged.RemoveAll(this);
		Zones->OnWorldSnapshotRestored.RemoveAll(this);
	}
	if (Progression)
	{
		Progression->OnFacilityRevenueChanged.RemoveAll(this);
		Progression->OnFestivalDefinitionChanged.RemoveAll(this);
		Progression->OnTodayRevenueChanged.RemoveAll(this);
	}
	Panel = nullptr;
	Progression = nullptr;
	Items.Reset();
	Super::Deinitialize();
}
void UFF_FacilityRevenueRankingSubsystem::Open()
{
	auto *PC = GetWorld()->GetFirstPlayerController();
	auto *Info = GetWorld()->GetSubsystem<UFF_FacilityInfoSubsystem>();
	if (!PC || !PC->GetLocalPlayer() || !Info || Info->IsPauseMenuPresentationSuppressed())
		return;
	if (auto *Build = GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>())
	{
		if (Build->GetBuildModeState() == EFF_BuildModeState::Placing ||
			Build->GetBuildModeState() == EFF_BuildModeState::Moving)
			return;
		if (Build->GetBuildModeState() == EFF_BuildModeState::CatalogOpen)
			Build->EnterBuildMode();
		Build->OnBuildModeStateChanged.AddUniqueDynamic(this, &ThisClass::BuildStateChanged);
	}
	BindEconomy();
	if (!Panel)
	{
		UClass *Class = FSoftClassPath(FFBuildUIAssetPaths::FacilityRevenueRankingClass)
							.TryLoadClass<UFF_FacilityRevenueRankingPanel>();
		if (!Class)
		{
			UE_LOG(LogTemp, Error, TEXT("FF_REVENUE_UI: ranking WBP missing; run FF.BuildFacilityRevenueUMG"));
			return;
		}
		Panel = CreateWidget<UFF_FacilityRevenueRankingPanel>(PC, Class);
		if (!Panel)
			return;
		Panel->OnCloseRequested.AddUObject(this, &ThisClass::Close);
		Panel->OnPeriodChanged.AddUObject(this, &ThisClass::Refresh);
		Panel->OnInspectRequested.AddUObject(this, &ThisClass::Inspect);
	}
	if (!Panel->IsInViewport())
		Panel->AddToViewport(71);
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Info->SetRevenueRankingPresentationOpen(true);
	if (!Catalog)
	{
		Catalog = GetWorld()->GetGameInstance()->GetSubsystem<UFF_BuildCatalogSubsystem>();
		if (Catalog)
		{
			Catalog->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::Changed);
			Catalog->OnCatalogThumbnailsChanged.AddUniqueDynamic(this, &ThisClass::Changed);
			Catalog->AcquireThumbnailPreload();
		}
	}
	Refresh();
	Panel->SetKeyboardFocus();
}
void UFF_FacilityRevenueRankingSubsystem::Close()
{
	if (Panel)
	{
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		Panel->RemoveFromParent();
	}
	if (auto *Build = GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>())
		Build->OnBuildModeStateChanged.RemoveAll(this);
	if (Catalog)
	{
		Catalog->OnCatalogChanged.RemoveAll(this);
		Catalog->OnCatalogThumbnailsChanged.RemoveAll(this);
		Catalog->ReleaseThumbnailPreload();
		Catalog = nullptr;
	}
	if (auto *Info = GetWorld()->GetSubsystem<UFF_FacilityInfoSubsystem>())
		Info->SetRevenueRankingPresentationOpen(false);
	Items.Reset();
}
bool UFF_FacilityRevenueRankingSubsystem::IsOpen() const
{
	return Panel && Panel->IsInViewport() && Panel->IsVisible();
}
bool UFF_FacilityRevenueRankingSubsystem::IsPointerOverPanel() const
{
	return IsOpen() && Panel->IsPointerOverPanel();
}
FGuid UFF_FacilityRevenueRankingSubsystem::GetSelectedFacilityId() const
{
	return IsOpen() ? Panel->GetSelectedFacilityId() : FGuid();
}
void UFF_FacilityRevenueRankingSubsystem::Refresh()
{
	if (!IsOpen())
		return;
	auto *Info = GetWorld()->GetSubsystem<UFF_FacilityInfoSubsystem>();
	if (Info && Info->IsPauseMenuPresentationSuppressed())
	{
		Close();
		return;
	}
	FGuid Selected = Panel->GetSelectedFacilityId();
	if (!Selected.IsValid() && Info)
		Selected = Info->GetSelectedFacilityId();
	const bool bAvailable = Progression && Progression->GetDefinition();
	// 기존 행 객체를 재사용하면 수입이 바뀔 때 선택 상태와 가상 목록을 유지할 수 있다.
	TMap<FGuid, UFF_FacilityRevenueRankItem *> Existing;
	Existing.Reserve(Items.Num());
	for (UFF_FacilityRevenueRankItem* Item : Items)
		Existing.Add(Item->FacilityId, Item);
	TArray<UFF_FacilityRevenueRankItem *> Rows;
	Rows.Reserve(Items.Num());
	auto *Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	if (bAvailable && Zones)
		for (const auto &Facility : Zones->BuildReadModel().Facilities)
		{
			if (Facility.State == EFFZSFacilityState::Removed || Facility.State == EFFZSFacilityState::Planned)
				continue;
			// Booth income includes both service fees and any attributable program payment.
			// Include another category if its instance already has a revenue ledger.
			FFF_FacilityInstanceRevenue Revenue;
			const bool bHasLedger = Progression->GetFacilityInstanceRevenue(Facility.FacilityInstanceId, Revenue);
			if (Facility.Category != EFFZSFacilityCategory::Booth && !bHasLedger)
				continue;
			auto *Item = Existing.FindRef(Facility.FacilityInstanceId);
			if (!Item)
				Item = NewObject<UFF_FacilityRevenueRankItem>(this);
			Item->FacilityId = Facility.FacilityInstanceId;
			Item->Name =
				Facility.DisplayName.IsEmpty() ? FText::FromName(Facility.FacilityDefinitionId) : Facility.DisplayName;
			Item->Location = FText::Format(LOCTEXT("PlayerLocation", "위치 {0}, {1} m"),
										   FText::AsNumber(FMath::RoundToInt(Facility.WorldLocation.X / 100.)),
										   FText::AsNumber(FMath::RoundToInt(Facility.WorldLocation.Y / 100.)));
			Item->TodayGold = Revenue.TodayGold;
			Item->TotalGold = Revenue.TotalGold;
			FFF_BuildCatalogEntry Entry;
			if (Catalog && Catalog->FindEntry(Facility.FacilityDefinitionId, Entry))
			{
				Item->Thumbnail = Entry.Thumbnail;
				Item->Name = Entry.DisplayName;
			}
			Rows.Add(Item);
		}
	const int64 Sum = FFFacilityRevenueRanking::Rank(Rows, Panel->IsCumulative());
	Items.Reset();
	for (auto *Item : Rows)
		Items.Add(Item);
	Panel->Present(Rows, Sum, bAvailable, Selected);
}
void UFF_FacilityRevenueRankingSubsystem::Inspect()
{
	const FGuid Id = GetSelectedFacilityId();
	auto *Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	auto *Actor = Zones ? Zones->FindFacility(Id) : nullptr;
	auto *Info = GetWorld()->GetSubsystem<UFF_FacilityInfoSubsystem>();
	const auto Context = Info ? Info->GetSelectionContext() : EFF_FacilityInfoContext::None;
	Close();
	if (!Actor)
		return;
	auto *Build = GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>();
	if (Build && Context != EFF_FacilityInfoContext::Live)
		Build->InspectFacility(Actor);
	else if (Info)
		Info->SelectFacility(GetWorld()->GetFirstPlayerController(), Actor, EFF_FacilityInfoContext::Live);
}
void UFF_FacilityRevenueRankingSubsystem::Changed()
{
	Refresh();
}
void UFF_FacilityRevenueRankingSubsystem::RevenueChanged(FGuid Id)
{
	Refresh();
}
void UFF_FacilityRevenueRankingSubsystem::AmountChanged(int32 Value)
{
	Refresh();
}
void UFF_FacilityRevenueRankingSubsystem::WorldChanged(const FFFZSWorldChangeEvent &Event)
{
	Refresh();
}
void UFF_FacilityRevenueRankingSubsystem::SelectionChanged(FGuid Id, EFF_FacilityInfoContext Context)
{
	if (IsOpen() && Context == EFF_FacilityInfoContext::None)
		Close();
}
void UFF_FacilityRevenueRankingSubsystem::BuildStateChanged(EFF_BuildModeState Previous, EFF_BuildModeState Current)
{
	if (Current != EFF_BuildModeState::BuildMode)
		Close();
}
#undef LOCTEXT_NAMESPACE
