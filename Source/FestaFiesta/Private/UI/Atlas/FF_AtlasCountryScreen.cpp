// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasCountryScreen.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "UI/Atlas/FF_AtlasPinButton.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_AtlasCountryScreen::UFF_AtlasCountryScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CommonUI 기본 Deactivate가 아니라 UI Flow의 화면 히스토리로 지구본에 돌아간다.
	bIsBackHandler = true;
}

void UFF_AtlasCountryScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_StartFestival)
	{
		Button_StartFestival->SetButtonLabel(LOCTEXT("AtlasCountryStart", "축제 시작"));
	}
	if (Button_Back)
	{
		Button_Back->SetButtonLabel(LOCTEXT("AtlasCountryBack", "지구본"));
	}
	if (Text_EmptyHint)
	{
		Text_EmptyHint->SetText(LOCTEXT("AtlasCountryEmptyHint", "지도에서 축제를 고르세요"));
	}
}

void UFF_AtlasCountryScreen::NativeConstruct()
{
	// 기존 WBP CDO에 예전 false가 저장되어 있어도 Super가 Back 입력을 등록하기 전에 바로잡는다.
	bIsBackHandler = true;
	Super::NativeConstruct();

	// 구독은 `NativeConstruct` 다 — CommonUI 스택이 위젯을 풀에서 재사용한다.
	if (Button_StartFestival)
	{
		Button_StartFestival->OnFFButtonClicked.AddUniqueDynamic(
			this, &UFF_AtlasCountryScreen::HandleStartClicked);
	}
	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_AtlasCountryScreen::HandleBackClicked);
	}
}

void UFF_AtlasCountryScreen::NativeDestruct()
{
	if (Button_StartFestival)
	{
		Button_StartFestival->OnFFButtonClicked.RemoveDynamic(
			this, &UFF_AtlasCountryScreen::HandleStartClicked);
	}
	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.RemoveDynamic(this, &UFF_AtlasCountryScreen::HandleBackClicked);
	}

	ClearPinButtons();
	LastCanvasSize = FVector2D::ZeroVector;

	Super::NativeDestruct();
}

void UFF_AtlasCountryScreen::NativeOnActivated()
{
	// 재진입 때 이전 축제의 카드가 잠깐 보이지 않게 현재 국가의 첫 플레이 가능 핀부터 다시 잡는다.
	SelectedFestivalId = NAME_None;

	// 활성화될 때마다 다시 만든다. 국가가 바뀌었을 수 있고 위젯이 풀에서 재사용된다.
	RebuildForCountry();

	// 포커스는 베이스가 `NativeGetDesiredFocusTarget` 으로 잡는다.
	// 그 함수가 선택 상태를 보므로 **반드시 RebuildForCountry 뒤에** Super 를 부른다.
	Super::NativeOnActivated();
}

void UFF_AtlasCountryScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 창 크기가 바뀔 때만 다시 배치한다. 매 프레임 계산할 이유가 없다 —
	// 지구본 화면과 달리 지도는 스스로 움직이지 않는다.
	if (CanvasPanel_Map)
	{
		const FVector2D CanvasSize = CanvasPanel_Map->GetCachedGeometry().GetLocalSize();
		if (!CanvasSize.Equals(LastCanvasSize, 0.5))
		{
			LastCanvasSize = CanvasSize;
			UpdateMapLayout();
		}
	}
}

FReply UFF_AtlasCountryScreen::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	FVector2D Direction = FVector2D::ZeroVector;

	if (Key == EKeys::W || Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
	{
		Direction = FVector2D(0.0, -1.0);
	}
	else if (Key == EKeys::A || Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		Direction = FVector2D(-1.0, 0.0);
	}
	else if (Key == EKeys::S || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
	{
		Direction = FVector2D(0.0, 1.0);
	}
	else if (Key == EKeys::D || Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		Direction = FVector2D(1.0, 0.0);
	}

	if (!Direction.IsNearlyZero())
	{
		// 한 번 누를 때 한 핀만 간다. 키 반복은 소비하되 여러 핀을 건너뛰지 않는다.
		if (!InKeyEvent.IsRepeat())
		{
			MovePinFocus(Direction);
		}
		return FReply::Handled();
	}

	const bool bConfirmKey = Key == EKeys::Enter
		|| Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey()
		|| Key == EKeys::Gamepad_FaceButton_Bottom;
	if (bConfirmKey)
	{
		if (UFF_AtlasPinButton* FocusedPin = FindFocusedPin())
		{
			SelectFestival(FocusedPin->GetPinId(), /*bMoveFocus=*/false);
			if (CanStartSelectedFestival())
			{
				OnStartFestivalRequested.Broadcast(SelectedFestivalId);
			}
			return FReply::Handled();
		}
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

bool UFF_AtlasCountryScreen::NativeOnHandleBackAction()
{
	OnBackRequested.Broadcast();
	return true;
}

UWidget* UFF_AtlasCountryScreen::NativeGetDesiredFocusTarget() const
{
	if (!SelectedFestivalId.IsNone())
	{
		if (const TObjectPtr<UFF_AtlasPinButton>* SelectedPin = PinButtons.Find(SelectedFestivalId);
			SelectedPin && *SelectedPin)
		{
			return SelectedPin->Get();
		}
	}

	for (const FName FestivalId : NavigationFestivalIds)
	{
		if (const TObjectPtr<UFF_AtlasPinButton>* Pin = PinButtons.Find(FestivalId); Pin && *Pin)
		{
			return Pin->Get();
		}
	}

	// 핀이 없거나 핀 WBP가 빠져도 화면에서 탈출할 수 있어야 한다.
	if (Button_Back)
	{
		return Button_Back;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UFF_AtlasCountryScreen::SetCountry(FName InCountryId)
{
	if (CountryId == InCountryId)
	{
		return;
	}

	CountryId = InCountryId;

	// 국가가 바뀌면 이전 축제 선택은 의미가 없다.
	SelectedFestivalId = NAME_None;

	// 아직 화면에 올라오지 않았으면 `NativeOnActivated` 가 만든다.
	if (IsActivated())
	{
		RebuildForCountry();
		if (UWidget* FocusTarget = NativeGetDesiredFocusTarget())
		{
			FocusTarget->SetFocus();
		}
	}
}

// --- 조립 ---

void UFF_AtlasCountryScreen::ClearPinButtons()
{
	for (const TPair<FName, TObjectPtr<UFF_AtlasPinButton>>& Pair : PinButtons)
	{
		if (Pair.Value)
		{
			Pair.Value->OnFFButtonClicked.RemoveDynamic(this, &UFF_AtlasCountryScreen::HandlePinClicked);
			Pair.Value->RemoveFromParent();
		}
	}
	PinButtons.Reset();
	NavigationFestivalIds.Reset();

	if (!CanvasPanel_Map)
	{
		return;
	}

	// CanvasPanel_Map에는 Image_Map도 있으므로 ClearChildren()을 쓰면 지도가 함께 사라진다.
	// 이전 버전이 추적 맵을 먼저 비워 남긴 UFF_AtlasPinButton만 역순으로 걷어 낸다.
	for (int32 ChildIndex = CanvasPanel_Map->GetChildrenCount() - 1; ChildIndex >= 0; --ChildIndex)
	{
		if (UFF_AtlasPinButton* StalePin = Cast<UFF_AtlasPinButton>(CanvasPanel_Map->GetChildAt(ChildIndex)))
		{
			StalePin->OnFFButtonClicked.RemoveDynamic(this, &UFF_AtlasCountryScreen::HandlePinClicked);
			CanvasPanel_Map->RemoveChildAt(ChildIndex);
		}
	}
}

void UFF_AtlasCountryScreen::RebuildForCountry()
{
	// 조건 검사 전에 지운다. 카탈로그나 클래스가 잠시 비어도 옛 국가의 핀이 남으면 안 된다.
	ClearPinButtons();

	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();

	if (Text_CountryName)
	{
		const FFF_AtlasCountryEntry* Country = (Catalog && !CountryId.IsNone())
			? Catalog->FindCountry(CountryId)
			: nullptr;
		Text_CountryName->SetText(Country ? Country->DisplayName : FText::GetEmpty());
	}

	// --- 핀 ---

	if (!CanvasPanel_Map)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasCountryScreen: CanvasPanel_Map 바인딩이 없어 지도를 그리지 못한다."));
		return;
	}

	if (!PinButtonClass)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasCountryScreen: PinButtonClass 가 없어 축제 핀이 뜨지 않는다. "
				 "WBP 에서 WBP_FF_AtlasPin 을 지정한다."));
	}

	if (Catalog && !CountryId.IsNone() && PinButtonClass)
	{
		TArray<const FFF_FestivalAtlasEntry*> Festivals;
		Catalog->GetFestivalsInCountry(CountryId, Festivals);

		if (Festivals.Num() == 0)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("UFF_AtlasCountryScreen: 국가 `%s` 에 축제가 없다. 빈 지도가 뜬다."),
				*CountryId.ToString());
		}

		for (const FFF_FestivalAtlasEntry* Festival : Festivals)
		{
			if (!Festival)
			{
				continue;
			}

			UFF_AtlasPinButton* Pin = CreateWidget<UFF_AtlasPinButton>(this, PinButtonClass);
			if (!Pin)
			{
				UE_LOG(LogFestaFiestaUI, Error,
					TEXT("UFF_AtlasCountryScreen: 핀 위젯 생성 실패 (%s)."), *PinButtonClass->GetName());
				continue;
			}

			// 지역 라벨은 배지로 보여준다. **지역 선택 단계를 만들지 않는다** —
			// 핀 자체가 축제를 가리킨다 (2026-08-21 확정).
			Pin->SetPinData(
				Festival->FestivalId,
				Festival->FestivalName,
				Festival->RegionName,
				Festival->bUnlocked,
				Festival->LockReason);

			Pin->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_AtlasCountryScreen::HandlePinClicked);

			if (UCanvasPanelSlot* PinSlot = CanvasPanel_Map->AddChildToCanvas(Pin))
			{
				PinSlot->SetAnchors(FAnchors(0.f, 0.f));
				// 핀 바닥 중앙이 실제 지도 좌표에 닿는다. 자연지도 마커의 뾰족한 끝 기준이다.
				PinSlot->SetAlignment(FVector2D(0.5, 1.0));
				PinSlot->SetAutoSize(true);
			}

			PinButtons.Add(Festival->FestivalId, Pin);
			NavigationFestivalIds.Add(Festival->FestivalId);
		}
	}
	else if (Catalog && !CountryId.IsNone())
	{
		// 핀 클래스가 빠져도 카드는 데이터로 채우고 뒤로 갈 수 있게 목록 순서는 보존한다.
		TArray<const FFF_FestivalAtlasEntry*> Festivals;
		Catalog->GetFestivalsInCountry(CountryId, Festivals);
		for (const FFF_FestivalAtlasEntry* Festival : Festivals)
		{
			if (Festival)
			{
				NavigationFestivalIds.Add(Festival->FestivalId);
			}
		}
	}

	// 지도 사각형과 핀 좌표를 함께 잡는다.
	LastCanvasSize = FVector2D::ZeroVector;	// 다음 Tick 에 강제로 다시 계산되게 한다
	UpdateMapLayout();

	// 첫 진입은 해당 국가의 첫 플레이 가능 축제를 선택한다. 전부 잠겼으면 첫 항목의
	// 잠금 사유를 카드에 보여준다. 국가·도시 이름을 비교하지 않고 데이터만 읽는다.
	FName DefaultFestivalId = NAME_None;
	if (Catalog)
	{
		for (const FName FestivalId : NavigationFestivalIds)
		{
			const FFF_FestivalAtlasEntry* Festival = Catalog->FindEntry(FestivalId);
			if (!Festival)
			{
				continue;
			}

			if (DefaultFestivalId.IsNone())
			{
				DefaultFestivalId = FestivalId;
			}
			if (Festival->bUnlocked && !Festival->Level.IsNull())
			{
				DefaultFestivalId = FestivalId;
				break;
			}
		}
	}

	SelectFestival(DefaultFestivalId, /*bMoveFocus=*/false);
}

void UFF_AtlasCountryScreen::UpdateMapLayout()
{
	if (!CanvasPanel_Map)
	{
		return;
	}

	const FVector2D CanvasSize = CanvasPanel_Map->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 1.0 || CanvasSize.Y <= 1.0)
	{
		// 레이아웃이 아직 확정되지 않았다. 다음 Tick 에 다시 온다.
		return;
	}

	// 🔴 지도 사각형을 **한 번만** 계산해 이미지와 핀이 같은 값을 쓰게 한다.
	//    UMG `UImage` 는 위젯 크기에 그냥 늘려 그리므로, 이미지 슬롯도 이 사각형으로 맞춰야
	//    레터박스가 생기고 핀이 지도 위에 얹힌다.
	MapRect = FFF_AtlasScreenGeo::ComputeImageRect(CanvasSize, GetMapSourceSize(), MapFit);

	if (Image_Map)
	{
		if (UCanvasPanelSlot* MapSlot = Cast<UCanvasPanelSlot>(Image_Map->Slot))
		{
			MapSlot->SetAnchors(FAnchors(0.f, 0.f));
			MapSlot->SetAlignment(FVector2D::ZeroVector);
			MapSlot->SetAutoSize(false);
			MapSlot->SetPosition(MapRect.Offset);
			MapSlot->SetSize(MapRect.Size);
		}
	}

	if (Image_SelectedRegionOverlay)
	{
		if (UCanvasPanelSlot* OverlaySlot = Cast<UCanvasPanelSlot>(Image_SelectedRegionOverlay->Slot))
		{
			// 지도와 Crop·비율이 같은 투명 이미지이므로 사각형도 반드시 완전히 같아야 한다.
			OverlaySlot->SetAnchors(FAnchors(0.f, 0.f));
			OverlaySlot->SetAlignment(FVector2D::ZeroVector);
			OverlaySlot->SetAutoSize(false);
			OverlaySlot->SetPosition(MapRect.Offset);
			OverlaySlot->SetSize(MapRect.Size);
		}
	}

	// 핀을 그 사각형 위에 얹는다.
	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	if (!Catalog)
	{
		return;
	}

	for (const TPair<FName, TObjectPtr<UFF_AtlasPinButton>>& Pair : PinButtons)
	{
		if (!Pair.Value)
		{
			continue;
		}

		const FFF_FestivalAtlasEntry* Festival = Catalog->FindEntry(Pair.Key);
		if (!Festival)
		{
			continue;
		}

		if (UCanvasPanelSlot* PinSlot = Cast<UCanvasPanelSlot>(Pair.Value->Slot))
		{
			// 정규화 좌표 → 위젯 로컬. 비율·레터박스가 바뀌어도 같은 지점이다.
			PinSlot->SetPosition(
				FFF_AtlasScreenGeo::NormalizedToWidgetLocal(Festival->MapPosition, MapRect));
		}
	}
}

void UFF_AtlasCountryScreen::RefreshFestivalCard()
{
	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	const FFF_FestivalAtlasEntry* Festival = (Catalog && !SelectedFestivalId.IsNone())
		? Catalog->FindEntry(SelectedFestivalId)
		: nullptr;

	const bool bHasFestival = (Festival != nullptr);

	// 고르기 전에는 카드를 비우고 안내만 남긴다. 빈 칸에 `-` 를 넣지 않는다.
	if (Text_EmptyHint)
	{
		Text_EmptyHint->SetVisibility(bHasFestival ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (Text_FestivalName)
	{
		Text_FestivalName->SetText(bHasFestival ? Festival->FestivalName : FText::GetEmpty());
	}

	if (Text_RegionName)
	{
		Text_RegionName->SetText(bHasFestival ? Festival->RegionName : FText::GetEmpty());
		Text_RegionName->SetVisibility(
			(bHasFestival && !Festival->RegionName.IsEmpty())
				? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_Difficulty)
	{
		// ★ `잠정` 을 함께 띄운다 — 난이도 산정 기준이 아직 없다 (`assumption`).
		//   숫자만 두면 확정된 값처럼 읽힌다.
		Text_Difficulty->SetText(bHasFestival
			? FText::Format(LOCTEXT("AtlasCountryDifficulty", "난이도 {0} (잠정)"),
				FText::AsNumber(Festival->Difficulty))
			: FText::GetEmpty());
		Text_Difficulty->SetVisibility(bHasFestival ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_OperatingDays)
	{
		Text_OperatingDays->SetText(bHasFestival
			? FText::Format(LOCTEXT("AtlasCountryDays", "운영 {0}일"), FText::AsNumber(Festival->OperatingDays))
			: FText::GetEmpty());
		Text_OperatingDays->SetVisibility(bHasFestival ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_Description)
	{
		Text_Description->SetText(bHasFestival ? Festival->Description : FText::GetEmpty());
		Text_Description->SetVisibility(
			(bHasFestival && !Festival->Description.IsEmpty())
				? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Image_Preview)
	{
		UTexture2D* Preview = (bHasFestival && !Festival->PreviewImage.IsNull())
			? Festival->PreviewImage.LoadSynchronous()
			: nullptr;

		if (Preview)
		{
			Image_Preview->SetBrushFromTexture(Preview);
		}
		Image_Preview->SetVisibility(Preview ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Image_SelectedRegionOverlay)
	{
		UTexture2D* Overlay = (bHasFestival && !Festival->SelectionOverlayImage.IsNull())
			? Festival->SelectionOverlayImage.LoadSynchronous()
			: nullptr;

		Image_SelectedRegionOverlay->SetBrushFromTexture(Overlay);
		Image_SelectedRegionOverlay->SetColorAndOpacity(FLinearColor::White);
		Image_SelectedRegionOverlay->SetVisibility(
			Overlay ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 잠금 — 자물쇠 + 사유를 함께 띄운다 (AT-011). 비활성만 두면 왜 못 누르는지 모른다.
	const bool bLocked = bHasFestival && !Festival->bUnlocked;

	if (Image_LockedIcon)
	{
		Image_LockedIcon->SetVisibility(bLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_LockReason)
	{
		Text_LockReason->SetText(bLocked ? Festival->LockReason : FText::GetEmpty());
		Text_LockReason->SetVisibility(bLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_Availability)
	{
		FText Availability;
		if (bHasFestival)
		{
			Availability = bLocked
				? LOCTEXT("AtlasCountryUnavailable", "준비 중")
				: Festival->Level.IsNull()
					? LOCTEXT("AtlasCountryMissingLevel", "시작 정보 확인 필요")
					: LOCTEXT("AtlasCountryAvailable", "플레이 가능");
		}

		Text_Availability->SetText(Availability);
		Text_Availability->SetVisibility(
			bHasFestival ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Button_StartFestival)
	{
		Button_StartFestival->SetIsEnabled(CanStartSelectedFestival());
	}
}

void UFF_AtlasCountryScreen::SelectFestival(FName FestivalId, bool bMoveFocus)
{
	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	const FFF_FestivalAtlasEntry* Festival = (Catalog && !FestivalId.IsNone())
		? Catalog->FindEntry(FestivalId)
		: nullptr;

	if (!Festival || Festival->CountryId != CountryId)
	{
		FestivalId = NAME_None;
	}

	SelectedFestivalId = FestivalId;

	for (const TPair<FName, TObjectPtr<UFF_AtlasPinButton>>& Pair : PinButtons)
	{
		if (Pair.Value)
		{
			Pair.Value->SetSelectedState(!SelectedFestivalId.IsNone() && Pair.Key == SelectedFestivalId);
		}
	}

	RefreshFestivalCard();

	if (bMoveFocus && !SelectedFestivalId.IsNone())
	{
		if (const TObjectPtr<UFF_AtlasPinButton>* Pin = PinButtons.Find(SelectedFestivalId); Pin && *Pin)
		{
			Pin->Get()->SetFocus();
		}
	}
}

bool UFF_AtlasCountryScreen::MovePinFocus(const FVector2D& ScreenDirection)
{
	if (NavigationFestivalIds.Num() < 2)
	{
		// 핀이 하나뿐이어도 WASD를 소비하되 선택 상태는 유지한다.
		return false;
	}

	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	if (!Catalog)
	{
		return false;
	}

	int32 CurrentIndex = NavigationFestivalIds.IndexOfByKey(SelectedFestivalId);
	if (UFF_AtlasPinButton* FocusedPin = FindFocusedPin())
	{
		const int32 FocusedIndex = NavigationFestivalIds.IndexOfByKey(FocusedPin->GetPinId());
		if (FocusedIndex != INDEX_NONE)
		{
			CurrentIndex = FocusedIndex;
		}
	}
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}

	TArray<FVector2D> ScreenPositions;
	ScreenPositions.Reserve(NavigationFestivalIds.Num());

	const FVector2D MapScale(
		MapRect.Size.X > 1.0 ? MapRect.Size.X : 1.0,
		MapRect.Size.Y > 1.0 ? MapRect.Size.Y : 1.0);
	for (const FName FestivalId : NavigationFestivalIds)
	{
		const FFF_FestivalAtlasEntry* Festival = Catalog->FindEntry(FestivalId);
		ScreenPositions.Add(Festival ? Festival->MapPosition * MapScale : FVector2D::ZeroVector);
	}

	const int32 NextIndex = FFF_AtlasScreenGeo::FindDirectionalPinIndex(
		ScreenPositions, CurrentIndex, ScreenDirection);
	if (!NavigationFestivalIds.IsValidIndex(NextIndex))
	{
		return false;
	}

	SelectFestival(NavigationFestivalIds[NextIndex], /*bMoveFocus=*/true);
	return true;
}

bool UFF_AtlasCountryScreen::CanStartSelectedFestival() const
{
	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	const FFF_FestivalAtlasEntry* Festival = (Catalog && !SelectedFestivalId.IsNone())
		? Catalog->FindEntry(SelectedFestivalId)
		: nullptr;

	return Festival
		&& Festival->CountryId == CountryId
		&& Festival->bUnlocked
		&& !Festival->Level.IsNull();
}

UFF_AtlasPinButton* UFF_AtlasCountryScreen::FindFocusedPin() const
{
	for (const FName FestivalId : NavigationFestivalIds)
	{
		if (const TObjectPtr<UFF_AtlasPinButton>* Pin = PinButtons.Find(FestivalId);
			Pin && *Pin && Pin->Get()->HasAnyUserFocus())
		{
			return Pin->Get();
		}
	}
	return nullptr;
}

// --- 핸들러 ---

void UFF_AtlasCountryScreen::HandlePinClicked(UFF_CommonButtonBase* Button)
{
	UFF_AtlasPinButton* Pin = Cast<UFF_AtlasPinButton>(Button);
	if (!Pin)
	{
		return;
	}

	// 잠긴 축제도 **카드는 보여준다.** 포커스는 가고 시작만 막는 것이 확정안이다 —
	// 그래야 왜 못 하는지 카드에서 읽을 수 있다.
	SelectFestival(Pin->GetPinId(), /*bMoveFocus=*/false);

	if (!Pin->IsSelectable())
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_AtlasCountryScreen: 잠긴 축제 `%s` 를 눌렀다 — %s"),
			*Pin->GetPinId().ToString(),
			Pin->GetLockReason().IsEmpty() ? TEXT("(사유 없음)") : *Pin->GetLockReason().ToString());
	}
}

void UFF_AtlasCountryScreen::HandleStartClicked(UFF_CommonButtonBase* Button)
{
	if (!CanStartSelectedFestival())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_AtlasCountryScreen: 현재 선택 `%s` 는 이 국가에서 시작할 수 없어 요청하지 않는다."),
			*SelectedFestivalId.ToString());
		return;
	}

	OnStartFestivalRequested.Broadcast(SelectedFestivalId);
}

void UFF_AtlasCountryScreen::HandleBackClicked(UFF_CommonButtonBase* Button)
{
	OnBackRequested.Broadcast();
}

// --- 조회 ---

const UFF_FestivalAtlasCatalog* UFF_AtlasCountryScreen::GetCatalog() const
{
	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	if (!Settings || Settings->AtlasCatalog.IsNull())
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_AtlasCountryScreen: Atlas 카탈로그가 지정되지 않았다. "
				 "Project Settings > Game > FestaFiesta UI Flow 에서 지정한다."));
		return nullptr;
	}

	return Settings->AtlasCatalog.LoadSynchronous();
}

FVector2D UFF_AtlasCountryScreen::GetMapSourceSize() const
{
	const UFF_FestivalAtlasCatalog* Catalog = GetCatalog();
	const FFF_AtlasCountryEntry* Country = (Catalog && !CountryId.IsNone())
		? Catalog->FindCountry(CountryId)
		: nullptr;

	if (Country && !Country->MapImage.IsNull())
	{
		if (UTexture2D* MapTexture = Country->MapImage.LoadSynchronous())
		{
			if (Image_Map)
			{
				Image_Map->SetBrushFromTexture(MapTexture);
				// 풀에서 재사용되며 이전 국가의 placeholder tint가 남을 수 있으므로 원본색으로 복구한다.
				Image_Map->SetColorAndOpacity(FLinearColor::White);
			}
			return FVector2D(MapTexture->GetSizeX(), MapTexture->GetSizeY());
		}
	}

	// 지도 이미지가 없다. 네모 박스 + 팔레트 색으로 대신 그린다 —
	// 이미지가 생길 때까지 흐름을 확인할 수 없으면 안 된다.
	if (Image_Map)
	{
		Image_Map->SetBrushFromTexture(nullptr);

		if (const UFF_UIPalette* Palette = UFF_UIPalette::Get())
		{
			// 색은 팔레트에서 읽는다. 위젯 안에 하드코딩하지 않는다.
			Image_Map->SetColorAndOpacity(Palette->Frontend.GlassRaised);
		}
	}

	return PlaceholderMapSize;
}

#undef LOCTEXT_NAMESPACE
