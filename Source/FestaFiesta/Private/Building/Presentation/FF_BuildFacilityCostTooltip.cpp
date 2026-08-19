// FF_BuildFacilityCostTooltip.cpp

// 카탈로그 항목의 시설명과 설치·복구 비용을 툴팁 표시 값으로 변환한다.
// 흐름: 항목 스냅샷 저장 -> 비용 해석 여부에 따른 문구 계산 -> 준비된 BindWidget에 적용.
// 불변식: 비용 미확정 상태와 유효한 0 Gold 복구 비용을 서로 다른 조건으로 판정한다.
// 실패/수명: Configure가 위젯 생성 전 호출되어도 상태를 보존하고 PreConstruct에서 다시 적용한다.

#include "Building/Presentation/FF_BuildFacilityCostTooltip.h"

#include "CommonTextBlock.h"

#define LOCTEXT_NAMESPACE "FFBuildFacilityCostTooltip"

void UFF_BuildFacilityCostTooltip::Configure(const FFF_BuildCatalogEntry& InEntry)
{
	Entry = InEntry;
	bConfigured = true;
	ApplyEntryToWidgets();
}

FText UFF_BuildFacilityCostTooltip::GetFacilityNameText() const
{
	// 저작 DisplayName을 우선하고, 비어 있을 때만 안정 토큰과 DefinitionId 순서로 내려간다.
	if (!Entry.DisplayName.IsEmpty())
	{
		return Entry.DisplayName;
	}
	if (!Entry.UniqueNameToken.IsEmpty())
	{
		return FText::FromString(Entry.UniqueNameToken);
	}
	return Entry.FacilityDefinitionId.IsNone()
		? FText::GetEmpty()
		: FText::FromName(Entry.FacilityDefinitionId);
}

FText UFF_BuildFacilityCostTooltip::GetInstallCostText() const
{
	if (!HasResolvedInstallCost())
	{
		return LOCTEXT("InstallCostUnavailable", "가격 정보 없음");
	}

	return FText::Format(
		LOCTEXT("InstallCostFormat", "{0} Gold"),
		FText::AsNumber(Entry.InstallCost));
}

FText UFF_BuildFacilityCostTooltip::GetDisplayedInstallCostText() const
{
	return TXT_InstallCost ? TXT_InstallCost->GetText() : FText::GetEmpty();
}

FText UFF_BuildFacilityCostTooltip::GetRecoveryCostText() const
{
	if (!HasResolvedRecoveryCost())
	{
		return LOCTEXT("RecoveryCostUnavailable", "가격 정보 없음");
	}

	return FText::Format(
		LOCTEXT("RecoveryCostFormat", "{0} Gold"),
		FText::AsNumber(Entry.RecoveryCost));
}

FText UFF_BuildFacilityCostTooltip::GetDisplayedRecoveryCostText() const
{
	return TXT_RecoveryCost ? TXT_RecoveryCost->GetText() : FText::GetEmpty();
}

bool UFF_BuildFacilityCostTooltip::HasResolvedInstallCost() const
{
	return bConfigured && Entry.bHasResolvedInstallCost && Entry.InstallCost > 0;
}

bool UFF_BuildFacilityCostTooltip::HasResolvedRecoveryCost() const
{
	// 철거 환급은 0 Gold도 유효하지만 설치 비용 0은 잘못된 비용표이므로 두 조건을 분리한다.
	return bConfigured && Entry.bHasResolvedRecoveryCost && Entry.RecoveryCost >= 0;
}

void UFF_BuildFacilityCostTooltip::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyEntryToWidgets();
}

void UFF_BuildFacilityCostTooltip::ApplyEntryToWidgets()
{
	// 디자이너 미리보기는 런타임 Configure 전용 값과 섞지 않아 WBP 편집 화면을 항상 읽을 수 있게 한다.
	const bool bUseDesignerPreview = IsDesignTime() && !bConfigured;
	const FText FacilityName = bUseDesignerPreview
		? DesignerPreviewFacilityName
		: GetFacilityNameText();
	const FText DesignerCostPlaceholder = LOCTEXT("DesignerCostPlaceholder", "— Gold");
	const FText InstallCostText = bUseDesignerPreview
		? DesignerCostPlaceholder
		: GetInstallCostText();
	const FText RecoveryCostText = bUseDesignerPreview
		? DesignerCostPlaceholder
		: GetRecoveryCostText();

	if (TXT_FacilityName)
	{
		TXT_FacilityName->SetText(FacilityName);
	}
	if (TXT_InstallCost)
	{
		TXT_InstallCost->SetText(InstallCostText);
	}
	if (TXT_RecoveryCost)
	{
		TXT_RecoveryCost->SetText(RecoveryCostText);
	}

	BP_OnTooltipDataChanged(
		!bUseDesignerPreview && HasResolvedInstallCost(),
		bUseDesignerPreview ? 0 : Entry.InstallCost);
}

#undef LOCTEXT_NAMESPACE
