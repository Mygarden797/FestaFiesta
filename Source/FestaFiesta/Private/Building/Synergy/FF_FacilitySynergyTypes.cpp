// FF_FacilitySynergyTypes.cpp
// 역할: 조합 표의 행·미리보기 자료형과 작성 데이터의 유효성 규칙이다.

#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Engine/AssetManager.h"
#include "FF_FFZSFacilityDefinition.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UFF_FacilitySynergySettings::UFF_FacilitySynergySettings()
{
	SynergyTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/FestaFiesta/Data/Economy/DT_FacilitySynergies.DT_FacilitySynergies")));
}

TArray<FString> UFF_FacilitySynergySettings::GetFacilityIdOptions()
{
	TArray<FString> Result;
	if (const UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		TArray<FPrimaryAssetId> Ids;
		Manager->GetPrimaryAssetIdList(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Ids);
		for (const FPrimaryAssetId& Id : Ids) Result.Add(Id.PrimaryAssetName.ToString());
		Result.Sort();
	}
	return Result;
}

bool FFF_FacilitySynergyRow::IsUsable(FString* OutError) const
{
	auto Reject = [OutError](const TCHAR* Reason) { if (OutError) *OutError = Reason; return false; };
	if (TargetFacilityId.IsNone() || PartnerFacilityId.IsNone()) return Reject(TEXT("수익 시설 ID와 함께 놓을 시설 ID를 모두 선택해 주세요."));
	if (!FMath::IsFinite(MaxDistanceMeters) || MaxDistanceMeters <= 0.f || MaxDistanceMeters > 1000.f)
		return Reject(TEXT("최대 거리는 0보다 크고 1000 이하인 미터 값이어야 합니다."));
	if (!FMath::IsFinite(RevenueBonusPercent) || RevenueBonusPercent <= 0.f || RevenueBonusPercent > 1000.f)
		return Reject(TEXT("추가 수익은 0보다 크고 1000 이하인 퍼센트 값이어야 합니다."));
	if (DiscoveryRewardGold < 0)
		return Reject(TEXT("최초 발견 보상은 0 이상의 골드 값이어야 합니다."));
	return true;
}

#if WITH_EDITOR
EDataValidationResult FFF_FacilitySynergyRow::IsDataValid(FDataValidationContext& Context) const
{
	if (!bEnabled) return EDataValidationResult::Valid;
	FString Error;
	if (!IsUsable(&Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	if (DisplayName.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("플레이어에게 보여 줄 조합 이름을 입력해 주세요.")));
		return EDataValidationResult::Invalid;
	}
	if (const UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		for (const FName Id : {TargetFacilityId, PartnerFacilityId})
		{
			if (!Manager->GetPrimaryAssetPath(FPrimaryAssetId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Id)).IsValid())
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("시설 ID '%s'를 찾을 수 없습니다. ID 선택 목록에서 다시 선택해 주세요."), *Id.ToString())));
				return EDataValidationResult::Invalid;
			}
		}
	}
	return EDataValidationResult::Valid;
}
#endif
