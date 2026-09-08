// FF_FFZSFacilityDefinition.cpp

// 시설 Data Asset 검증과 안정적인 Primary Asset ID 생성을 구현.
//흐름: Spec 검증 오류를 에디터 데이터 검증에 전달하고 유효한 정의 ID를 Primary Asset 이름으로 사용.
// 불변식: Primary Asset Type 문자열은 FFZSFacilityDefinition이며 런타임 사양 의미와 일치해야 한다.
// 실패/성능: 모든 검증 오류를 모아 제작자에게 보여 주며 에셋이나 메시를 강제 로드하지 않는다.
 

#include "FF_FFZSFacilityDefinition.h"

#include "Engine/AssetManager.h"
#include "FF_FFZSGridLibrary.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// 원본과 활성화된 각 단계의 실제 배치를 같은 Spec 검증으로 확인한다.
// 잘못된 원본 배열은 먼저 거절해 불필요한 파생 사양 계산을 피한다.
bool UFFZSFacilityDefinition::IsDefinitionValid(TArray<FText>& OutErrors) const
{
    const int32 ErrorCountBefore = OutErrors.Num();
    TArray<FText> BaseErrors;
    if (!Spec.IsValid(BaseErrors))
    {
        OutErrors.Append(BaseErrors);
        return false;
    }
    if (Spec.bUsesFacilityLevels)
    {
        // Tier structure was validated once above. Do not copy and revalidate the whole
        // authoring array for every geometry candidate as the number of tiers grows.
        FFFZSFacilitySpec GeometryBase = Spec;
        GeometryBase.UpgradeTiers.Reset();
        GeometryBase.bUsesFacilityLevels = false;
        for (const FFFZSFacilityUpgradeTier& Tier : Spec.UpgradeTiers)
        {
            FFFZSFacilitySpec EffectiveSpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
                GeometryBase, Tier.TargetLevel, Tier.SizeMultiplier);
            if (!Tier.VisualMesh.IsNull())
            {
                EffectiveSpec.VisualMesh = Tier.VisualMesh;
            }
            TArray<FText> TierErrors;
            if (!EffectiveSpec.IsValid(TierErrors))
            {
                for (const FText& Error : TierErrors)
                {
                    OutErrors.Add(FText::Format(
                        NSLOCTEXT("FFZSFacilityDefinition", "InvalidLevelGeometry", "Lv.{0}: {1}"),
                        FText::AsNumber(Tier.TargetLevel), Error));
                }
            }
        }
    }
    return OutErrors.Num() == ErrorCountBefore;
}

// 유효한 안정 DefinitionId가 있으면 프로젝트 전체에서 같은 Primary Asset Type을 사용한다.
// ID가 비어 있으면 부모 규칙을 사용해 에디터가 자산을 표시할 수 있게 하되 Spec 검증은 별도로 실패한다.
const FPrimaryAssetType& UFFZSFacilityDefinition::GetFacilityDefinitionAssetType()
{
    static const FPrimaryAssetType FacilityDefinitionAssetType(TEXT("FFZSFacilityDefinition"));
    return FacilityDefinitionAssetType;
}

bool UFFZSFacilityDefinition::DoesSpecMatchLoadedDefinition(
    const FFFZSFacilitySpec& Spec)
{
    UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
    if (!AssetManager || Spec.FacilityDefinitionId.IsNone())
    {
        return false;
    }

    const FPrimaryAssetId AssetId(
        GetFacilityDefinitionAssetType(),
        Spec.FacilityDefinitionId);
    const UFFZSFacilityDefinition* Definition =
        AssetManager->GetPrimaryAssetObject<UFFZSFacilityDefinition>(AssetId);
    TArray<FText> Errors;
    if (!Definition || !Definition->IsDefinitionValid(Errors))
    {
        return false;
    }

    FFFZSCommand Candidate;
    Candidate.TransactionId = FGuid(1, 2, 3, 4);
    Candidate.Type = EFFZSCommandType::Install;
    Candidate.FacilityInstanceId = FGuid(5, 6, 7, 8);
    Candidate.ZoneId = TEXT("CanonicalSpecComparison");
    Candidate.FacilitySpec = Spec;
    FFFZSCommand Canonical = Candidate;
    Canonical.FacilitySpec = Definition->Spec;
    return UFFZSGridLibrary::BuildCommandFingerprint(Candidate) ==
        UFFZSGridLibrary::BuildCommandFingerprint(Canonical);
}

FPrimaryAssetId UFFZSFacilityDefinition::GetPrimaryAssetId() const
{
    if (!Spec.FacilityDefinitionId.IsNone())
    {
        return FPrimaryAssetId(GetFacilityDefinitionAssetType(), Spec.FacilityDefinitionId);
    }
    return Super::GetPrimaryAssetId();
}

#if WITH_EDITOR
// 에디터 저장·검증 메뉴가 부모 오류와 Spec 오류를 모두 보게 한다.
// 런타임 빌드에는 이 경로가 없으므로 설치 명령도 같은 Spec 검증을 다시 수행한다.
void UFFZSFacilityDefinition::PostInitProperties()
{
    Super::PostInitProperties();
    // SourceMeshSize remains the tagged-serialization fallback for legacy data,
    // while new authoring assets start with Definition-authoritative sizing.
    if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad | RF_WasLoaded) &&
        GetOutermost() != GetTransientPackage())
    {
        Spec.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
    }
}

EDataValidationResult UFFZSFacilityDefinition::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult ParentResult = Super::IsDataValid(Context);
    TArray<FText> Errors;
    if (!IsDefinitionValid(Errors))
    {
        for (const FText& Error : Errors)
        {
            // 모든 오류를 전달해 담당자가 한 번의 검증으로 수정 항목 전체를 확인하게 한다.
            Context.AddError(Error);
        }
        return EDataValidationResult::Invalid;
    }

    return ParentResult == EDataValidationResult::Invalid
        ? EDataValidationResult::Invalid
        : EDataValidationResult::Valid;
}
#endif
