// FF_BuildFacilityCard.cpp

// 카탈로그 항목 스냅샷을 시설 카드의 텍스트·가격·썸네일·툴팁에 반영한다.
// 흐름: Configure로 데이터 저장 -> 위젯 준비 시 재적용 -> 비동기 썸네일 변경만 선택 갱신.
// 불변식: 동일 스냅샷 재적용은 멱등이며 미로드 썸네일은 동기 로드하지 않는다.
// 실패/성능: 누락 BindWidget은 건너뛰고 썸네일이 메모리에 없으면 디자이너 자리표시자를 유지한다.

#include "Building/UI/FF_BuildFacilityCard.h"

#include "Building/Presentation/FF_BuildFacilityCostTooltip.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FFBuildFacilityCard"

UFF_BuildFacilityCard::UFF_BuildFacilityCard()
{
	// 카탈로그 항목에 전용 썸네일이 없는 경우를 위해 공용 이미지 경로는 보관한다.
	// 카드는 이 참조를 직접 로드하지 않으며, 이미 메모리에 없으면 WBP의 어두운 배경을 자리표시자로 사용한다.
	DefaultThumbnail = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT(
		"/Game/FestaFiesta/Zone/Test_Asset/Image/T_UI_BuildCatalog_FoodTruck.T_UI_BuildCatalog_FoodTruck")));
	// 팀 소유 Zone/UI/Building 자산과 분리된 기능 전용 WBP입니다.
	InstallCostTooltipClass = TSoftClassPtr<UFF_BuildFacilityCostTooltip>(FSoftObjectPath(TEXT(
		"/Game/FestaFiesta/Building/Tooltips/WBP_FF_BuildFacilityCostTooltip."
		"WBP_FF_BuildFacilityCostTooltip_C")));
}

void UFF_BuildFacilityCard::Configure(const FFF_BuildCatalogEntry& InEntry)
{
	if (Entry.FacilityDefinitionId != InEntry.FacilityDefinitionId)
	{
		SetSynergyHighlighted(false);
		bRequiredFacility = false;
		bRequiredFacilitySatisfied = false;
		RequiredFacilityCurrentCount = RequiredFacilityMinimumCount = 0;
		ApplyRequiredFacilityStatus();
	}
	// 원본 배열의 재할당이나 화면 갱신과 무관하게 카드가 안정적으로 데이터를 유지하도록 값을 복사한다.
	Entry = InEntry;

	// 현지화된 표시 이름이 준비되지 않은 데이터도 식별 가능한 텍스트를 갖도록 토큰을 대체 제목으로 쓴다.
	ButtonLabel = Entry.DisplayName.IsEmpty()
		? FText::FromString(Entry.UniqueNameToken)
		: Entry.DisplayName;

	// CreateWidget 이후 Configure되는 일반 경로에서는 BindWidget이 이미 유효하므로 즉시 화면에 반영한다.
	// 생성 순서가 다른 에디터 미리보기 경로는 아래 PreConstruct/Construct에서 같은 함수를 다시 호출한다.
	ApplyEntryToWidgets();
	RefreshFacilityCostTooltip();
}

FText UFF_BuildFacilityCard::MakeFacilityCostToolTip(
	const FFF_BuildCatalogEntry& CatalogEntry)
{
	const FText UnavailableText = LOCTEXT("FacilityCostUnavailable", "가격 정보 없음");
	const FText InstallCostText =
		CatalogEntry.HasValidInstallCost()
		? FText::Format(
			LOCTEXT("FacilityCostGoldFormat", "{0} Gold"),
			FText::AsNumber(CatalogEntry.InstallCost))
		: UnavailableText;
	const FText RecoveryCostText =
		CatalogEntry.bHasResolvedRecoveryCost && CatalogEntry.RecoveryCost >= 0
		? FText::Format(
			LOCTEXT("FacilityCostGoldFormat", "{0} Gold"),
			FText::AsNumber(CatalogEntry.RecoveryCost))
		: UnavailableText;

	const FText BuildLimitText = MakeFacilityBuildLimitText(CatalogEntry);
	const FText BuildLimitReason = CatalogEntry.bBuildLimitReached
		? LOCTEXT("FacilityBuildLimitReachedReason", "이 시설은 건축 한도에 도달해 더 배치할 수 없습니다.")
		: FText::GetEmpty();

	if (CatalogEntry.MaxInstancesPerWorld <= 0)
	{
		// 기존 무제한 카드는 가격 툴팁 문자열 계약을 그대로 유지하고 카드 본문에서 제한 없음을 표시합니다.
		return FText::Format(
			LOCTEXT(
				"FacilityCostsFormat",
				"건설 비용: {0}\n회수 비용: {1}"),
			InstallCostText,
			RecoveryCostText);
	}

	return CatalogEntry.bBuildLimitReached
		? FText::Format(
			LOCTEXT(
				"FacilityCostsAndReachedLimitFormat",
				"건설 비용: {0}\n회수 비용: {1}\n{2}\n{3}"),
			InstallCostText,
			RecoveryCostText,
			BuildLimitText,
			BuildLimitReason)
		: FText::Format(
		LOCTEXT(
			"FacilityCostsAndLimitFormat",
			"건설 비용: {0}\n회수 비용: {1}\n{2}"),
		InstallCostText,
		RecoveryCostText,
		BuildLimitText);
}

FText UFF_BuildFacilityCard::MakeFacilityBuildLimitText(
	const FFF_BuildCatalogEntry& CatalogEntry)
{
	if (CatalogEntry.MaxInstancesPerWorld <= 0)
	{
		return LOCTEXT("FacilityBuildLimitUnlimited", "설치 제한 없음");
	}

	const FText CountText = FText::Format(
		LOCTEXT("FacilityBuildLimitCount", "설치 {0}/{1}"),
		FText::AsNumber(FMath::Max(0, CatalogEntry.CurrentInstanceCount)),
		FText::AsNumber(CatalogEntry.MaxInstancesPerWorld));
	return CatalogEntry.bBuildLimitReached
		? FText::Format(
			LOCTEXT("FacilityBuildLimitCountReached", "{0} · 한도 도달"),
			CountText)
		: CountText;
}

void UFF_BuildFacilityCard::RefreshThumbnail()
{
	ApplyThumbnailToWidget();
}

void UFF_BuildFacilityCard::SetSynergyHighlighted(const bool bHighlighted)
{
	const bool bChanged = bSynergyHighlighted != bHighlighted;
	bSynergyHighlighted = bHighlighted;
	ApplySynergyHighlight();
	if (bChanged) { BP_OnSynergyHighlightChanged(bHighlighted); }
}

void UFF_BuildFacilityCard::ApplySynergyHighlight()
{
	const ESlateVisibility AccentVisibility = bSynergyHighlighted
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Border_SynergyHighlight) { Border_SynergyHighlight->SetVisibility(AccentVisibility); }
	if (Text_SynergyBadge) { Text_SynergyBadge->SetVisibility(AccentVisibility); }
}

FText UFF_BuildFacilityCard::MakeRequiredFacilityToolTip(const bool bSatisfied,
	const int32 CurrentCount, const int32 MinimumCount)
{
	if (MinimumCount <= 0)
	{
		return LOCTEXT("RequiredFacilityInvalidMinimum", "필수 시설 설정 확인 필요 · 최소 설치 개수는 1 이상이어야 합니다.");
	}
	return FText::Format(bSatisfied
		? LOCTEXT("RequiredFacilitySatisfied", "필수 설치 충족 · 현재 {0}개 / 필요 최소 {1}개")
		: LOCTEXT("RequiredFacilityMissing", "필수 설치 부족 · 현재 {0}개 / 필요 최소 {1}개"),
		FText::AsNumber(FMath::Max(0, CurrentCount)), FText::AsNumber(FMath::Max(1, MinimumCount)));
}

void UFF_BuildFacilityCard::SetRequiredFacilityStatus(const bool bRequired, const bool bSatisfied,
	const int32 CurrentCount, const int32 MinimumCount)
{
	const bool bNewRequired = bRequired;
	const bool bNewSatisfied = bNewRequired && MinimumCount > 0 && bSatisfied;
	const int32 NewCurrent = bNewRequired ? FMath::Max(0, CurrentCount) : 0;
	const int32 NewMinimum = bNewRequired ? FMath::Max(0, MinimumCount) : 0;
	const bool bChanged = bRequiredFacility != bNewRequired || bRequiredFacilitySatisfied != bNewSatisfied ||
		RequiredFacilityCurrentCount != NewCurrent || RequiredFacilityMinimumCount != NewMinimum;
	bRequiredFacility = bNewRequired;
	bRequiredFacilitySatisfied = bNewSatisfied;
	RequiredFacilityCurrentCount = NewCurrent;
	RequiredFacilityMinimumCount = NewMinimum;
	ApplyRequiredFacilityStatus();
	if (bChanged)
	{
		RefreshFacilityCostTooltip();
		BP_OnRequiredFacilityStatusChanged(bNewRequired, bNewSatisfied, NewCurrent, NewMinimum);
	}
}

void UFF_BuildFacilityCard::ApplyRequiredFacilityStatus()
{
	const ESlateVisibility MissingVisibility = bRequiredFacility && !bRequiredFacilitySatisfied
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	const ESlateVisibility SatisfiedVisibility = bRequiredFacility && bRequiredFacilitySatisfied
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Border_RequiredMissing) { Border_RequiredMissing->SetVisibility(MissingVisibility); }
	if (Border_RequiredSatisfied) { Border_RequiredSatisfied->SetVisibility(SatisfiedVisibility); }
	if (Text_RequiredMissing) { Text_RequiredMissing->SetVisibility(MissingVisibility); }
	if (Text_RequiredSatisfied) { Text_RequiredSatisfied->SetVisibility(SatisfiedVisibility); }
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
	// 디자이너 프리뷰와 순수 NewObject 테스트에서는 별도 UserWidget을 만들지 않습니다.
	// 텍스트 폴백만 유지하면 WBP가 없는 환경에서도 가격 정보는 사라지지 않습니다.
	if (IsDesignTime())
	{
		ApplyFacilityCostTextFallback();
	}
	else { ApplySynergyHighlight(); ApplyRequiredFacilityStatus(); }
}

void UFF_BuildFacilityCard::NativeConstruct()
{
	Super::NativeConstruct();
	// 재부착 또는 WBP 재구성으로 자식 포인터가 새로 바인딩된 경우까지 보장한다.
	// SetText/SetBrush 연산은 같은 값으로 반복 적용해도 안전하므로 수명주기 훅 양쪽에서 호출한다.
	ApplyEntryToWidgets();
	ApplySynergyHighlight();
	ApplyRequiredFacilityStatus();
	RefreshFacilityCostTooltip();
}

void UFF_BuildFacilityCard::NativeDestruct()
{
	SetSynergyHighlighted(false);
	bRequiredFacility = false;
	bRequiredFacilitySatisfied = false;
	RequiredFacilityCurrentCount = RequiredFacilityMinimumCount = 0;
	ApplyRequiredFacilityStatus();
	SetToolTip(nullptr);
	FacilityCostTooltip = nullptr;
	Super::NativeDestruct();
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
		Text_DefinitionId->SetText(FText::Format(
			LOCTEXT("DefinitionAndBuildLimit", "{0}\n{1}"),
			FText::FromName(Entry.FacilityDefinitionId),
			MakeFacilityBuildLimitText(Entry)));
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

void UFF_BuildFacilityCard::RefreshFacilityCostTooltip()
{
	// The existing authored price tooltip has no requirement row. Preserve its cost
	// information in the shared text fallback while showing the minimum/count here.
	if (bRequiredFacility)
	{
		ApplyFacilityCostTextFallback();
		return;
	}
	// 한도 도달 카드는 클릭을 유지해 사유를 안내하므로, 기존 WBP가 한도 행을 갖지 않아도
	// 반드시 전체 사유가 보이는 지역화 텍스트 툴팁으로 표시합니다.
	if (Entry.bBuildLimitReached)
	{
		ApplyFacilityCostTextFallback();
		return;
	}

	// Native NewObject 기반 단위 테스트에는 WidgetTree가 없습니다. CreateWidget(UWidget owner)는
	// 유효한 부모 UserWidgetTree를 요구하므로 이 경로에서는 의도적으로 텍스트 폴백을 사용합니다.
	if (IsDesignTime() || !WidgetTree)
	{
		ApplyFacilityCostTextFallback();
		return;
	}

	UClass* TooltipClass = InstallCostTooltipClass.Get();
	if (!TooltipClass)
	{
		TooltipClass = InstallCostTooltipClass.LoadSynchronous();
	}

	if (!TooltipClass || !TooltipClass->IsChildOf(UFF_BuildFacilityCostTooltip::StaticClass()))
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 가격 툴팁 WBP를 불러오지 못해 텍스트 툴팁을 사용합니다: %s"),
			*GetName(), *InstallCostTooltipClass.ToSoftObjectPath().ToString());
		ApplyFacilityCostTextFallback();
		return;
	}

	if (!FacilityCostTooltip || !FacilityCostTooltip->IsA(TooltipClass))
	{
		FacilityCostTooltip = CreateWidget<UFF_BuildFacilityCostTooltip>(this, TooltipClass);
	}

	if (!FacilityCostTooltip)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 가격 툴팁 인스턴스 생성에 실패해 텍스트 툴팁을 사용합니다."),
			*GetName());
		ApplyFacilityCostTextFallback();
		return;
	}

	FacilityCostTooltip->Configure(Entry);
	// SetToolTipText가 이미 생성된 Slate 툴팁을 덮을 수 있으므로 텍스트를 먼저 비운 뒤
	// 커스텀 UMG를 최종 툴팁으로 설정합니다.
	SetToolTipText(FText::GetEmpty());
	SetToolTip(FacilityCostTooltip);
}

void UFF_BuildFacilityCard::ApplyFacilityCostTextFallback()
{
	SetToolTip(nullptr);
	const FText CostText = MakeFacilityCostToolTip(Entry);
	SetToolTipText(bRequiredFacility ? FText::Format(LOCTEXT("CostWithRequirement", "{0}\n{1}"), CostText,
		MakeRequiredFacilityToolTip(bRequiredFacilitySatisfied, RequiredFacilityCurrentCount, RequiredFacilityMinimumCount)) : CostText);
}

#undef LOCTEXT_NAMESPACE
