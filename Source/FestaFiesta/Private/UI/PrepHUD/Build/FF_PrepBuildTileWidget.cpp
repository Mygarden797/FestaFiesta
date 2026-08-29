// PrepBuild TileView entry presentation.

#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"

void UFF_PrepBuildTileView::ConfigureEntryClass(TSubclassOf<UUserWidget> InEntryClass)
{
	if (!InEntryClass || !InEntryClass->ImplementsInterface(UUserObjectListEntry::StaticClass()))
	{
		return;
	}

	EntryWidgetClass = InEntryClass;
	SetSelectionMode(ESelectionMode::Single);
	SetEntryWidth(174.0f);
	SetEntryHeight(194.0f);
	SetHorizontalEntrySpacing(10.0f);
	SetVerticalEntrySpacing(10.0f);
}

void UFF_PrepBuildTileWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Border_Card)
	{
		// TileView 엔트리는 고정 높이이므로 긴 번역이나 이름이 생겨도 다음 행까지 그리지 않습니다.
		Border_Card->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	}
}

void UFF_PrepBuildTileWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	Item = Cast<UFF_PrepBuildTileData>(ListItemObject);
	RefreshFromItem();
	RefreshVisualState(IsListItemSelected());
}

void UFF_PrepBuildTileWidget::NativeOnItemSelectionChanged(const bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
	RefreshVisualState(bIsSelected);
}

void UFF_PrepBuildTileWidget::NativeOnEntryReleased()
{
	Item = nullptr;
	// 가상화 풀에 반환되는 즉시 이전 항목의 텍스트·이미지·툴팁을 제거합니다.
	RefreshFromItem();
	IUserObjectListEntry::NativeOnEntryReleased();
}

void UFF_PrepBuildTileWidget::RefreshFromItem()
{
	if (!Item)
	{
		if (Text_Name)
		{
			Text_Name->SetText(FText::GetEmpty());
		}
		if (Text_Cost)
		{
			Text_Cost->SetText(FText::GetEmpty());
		}
		if (Text_State)
		{
			Text_State->SetText(FText::GetEmpty());
		}
		if (Image_Thumbnail)
		{
			Image_Thumbnail->SetBrush(FSlateBrush());
		}
		SetToolTipText(FText::GetEmpty());
		return;
	}

	if (Text_Name)
	{
		Text_Name->SetText(Item->GetDisplayName());
	}
	if (Text_Cost)
	{
		// 고정 크기 가상화 타일에는 높이가 예측 가능한 두 줄만 둡니다.
		// 전체 실패 사유는 아래 툴팁과 패널 상태 영역에서 제공됩니다.
		Text_Cost->SetAutoWrapText(false);
		Text_Cost->SetText(FText::Format(
			NSLOCTEXT("FestaFiestaPrepBuild", "CostAndBuildLimit", "{0}\n{1}"),
			Item->GetCostText(),
			Item->GetBuildLimitText()));
	}
	const FText StateText = Item->GetStateText();
	SetToolTipText(StateText.IsEmpty()
		? Item->GetBuildLimitText()
		: FText::Format(
			NSLOCTEXT("FestaFiestaPrepBuild", "BuildLimitAndBlockingTooltip", "{0}\n{1}"),
			Item->GetBuildLimitText(),
			StateText));
	if (Text_State)
	{
		// 카드에는 한 줄 상태만 남겨 색에 의존하지 않고도 선택 불가 원인을 구분합니다.
		Text_State->SetAutoWrapText(false);
		Text_State->SetVisibility(ESlateVisibility::HitTestInvisible);
		Text_State->SetText(Item->GetStateLabelText());
	}
	if (Image_Thumbnail)
	{
		// Asset Manager/BuildCatalogSubsystem owns async loading. A tile only reads an already-loaded texture.
		if (UTexture2D* LoadedThumbnail = Item->GetThumbnail().Get())
		{
			Image_Thumbnail->SetBrushFromTexture(LoadedThumbnail, true);
			Image_Thumbnail->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Image_Thumbnail->SetBrush(FSlateBrush());
		}
	}
}

void UFF_PrepBuildTileWidget::RefreshVisualState(const bool bIsSelected)
{
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return;
	}

	const bool bAvailable = Item && Item->IsAffordable();
	if (Border_Card)
	{
		Border_Card->SetBrushColor(bIsSelected
			? Palette->InGame.TealSoft
			: (bAvailable ? Palette->InGame.Paper : Palette->InGame.PaperSunken));
	}
	if (Text_Name)
	{
		Text_Name->SetColorAndOpacity(FSlateColor(bAvailable
			? Palette->InGame.Ink
			: Palette->InGame.InkFaint));
	}
	if (Text_Cost)
	{
		Text_Cost->SetColorAndOpacity(FSlateColor(bAvailable
			? Palette->InGame.TealDeep
			: Palette->InGame.Danger));
	}
	if (Text_State)
	{
		Text_State->SetColorAndOpacity(FSlateColor(bAvailable
			? Palette->InGame.TealDeep
			: Palette->InGame.Danger));
	}
}
