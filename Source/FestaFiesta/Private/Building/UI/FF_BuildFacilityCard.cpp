// FF_BuildFacilityCard.cpp

#include "Building/UI/FF_BuildFacilityCard.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

UFF_BuildFacilityCard::UFF_BuildFacilityCard()
{
	// 카탈로그 항목에 전용 썸네일이 없는 경우를 위해 공용 이미지 경로는 보관한다.
	// 카드는 이 참조를 직접 로드하지 않으며, 이미 메모리에 없으면 WBP의 어두운 배경을 자리표시자로 사용한다.
	DefaultThumbnail = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT(
		"/Game/FestaFiesta/Zone/Test_Asset/Image/T_UI_BuildCatalog_FoodTruck.T_UI_BuildCatalog_FoodTruck")));
}

void UFF_BuildFacilityCard::Configure(const FFF_BuildCatalogEntry& InEntry)
{
	// 원본 배열의 재할당이나 화면 갱신과 무관하게 카드가 안정적으로 데이터를 유지하도록 값을 복사한다.
	Entry = InEntry;

	// 현지화된 표시 이름이 준비되지 않은 데이터도 식별 가능한 텍스트를 갖도록 토큰을 대체 제목으로 쓴다.
	ButtonLabel = Entry.DisplayName.IsEmpty()
		? FText::FromString(Entry.UniqueNameToken)
		: Entry.DisplayName;

	// CreateWidget 이후 Configure되는 일반 경로에서는 BindWidget이 이미 유효하므로 즉시 화면에 반영한다.
	// 생성 순서가 다른 에디터 미리보기 경로는 아래 PreConstruct/Construct에서 같은 함수를 다시 호출한다.
	ApplyEntryToWidgets();
}

void UFF_BuildFacilityCard::RefreshThumbnail()
{
	ApplyThumbnailToWidget();
}

UTexture2D* UFF_BuildFacilityCard::ResolveLoadedThumbnail(
	const TSoftObjectPtr<UTexture2D>& RequestedThumbnail,
	const TSoftObjectPtr<UTexture2D>& FallbackThumbnail)
{
	if (UTexture2D* Requested = RequestedThumbnail.Get())
	{
		return Requested;
	}
	return FallbackThumbnail.Get();
}

void UFF_BuildFacilityCard::NativePreConstruct()
{
	Super::NativePreConstruct();
	// 디자이너 미리보기와 런타임 첫 렌더 직전에 직렬화/설정된 값을 자식 위젯에 동기화한다.
	ApplyEntryToWidgets();
}

void UFF_BuildFacilityCard::NativeConstruct()
{
	Super::NativeConstruct();
	// 재부착 또는 WBP 재구성으로 자식 포인터가 새로 바인딩된 경우까지 보장한다.
	// SetText/SetBrush 연산은 같은 값으로 반복 적용해도 안전하므로 수명주기 훅 양쪽에서 호출한다.
	ApplyEntryToWidgets();
}

void UFF_BuildFacilityCard::ApplyEntryToWidgets()
{
	// BindWidget은 WBP 컴파일 계약이지만, 생성 중간 단계나 잘못된 에셋에서도 크래시하지 않도록
	// 각 포인터를 독립적으로 확인한다. 한 필드가 없더라도 나머지 데이터는 가능한 범위까지 표시한다.
	if (Text_Label)
	{
		Text_Label->SetText(ButtonLabel);
	}
	if (Text_DefinitionId)
	{
		Text_DefinitionId->SetText(FText::FromName(Entry.FacilityDefinitionId));
	}
	ApplyThumbnailToWidget();
}

void UFF_BuildFacilityCard::ApplyThumbnailToWidget()
{
	if (Image_Thumbnail)
	{
		// Asset Manager가 세션 범위에서 미리 올린 텍스처만 조회한다. 카드 생성 중에는 디스크 I/O를 시작하지 않는다.
		UTexture2D* Thumbnail = ResolveLoadedThumbnail(Entry.Thumbnail, DefaultThumbnail);
		if (Thumbnail)
		{
			Image_Thumbnail->SetBrushFromTexture(Thumbnail, true);
			Image_Thumbnail->SetColorAndOpacity(FLinearColor::White);
			return;
		}

		// 이전 항목의 브러시가 재사용되지 않도록 비우면 WBP의 어두운 ThumbnailBackdrop이 자리표시자로 보인다.
		Image_Thumbnail->SetBrush(FSlateBrush());
	}
}
