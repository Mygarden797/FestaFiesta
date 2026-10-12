// FFZAZoneDefinition.h
// 역할: "Zone 종류"를 에셋 하나로 정의한다(Data Asset).
//
// 예전 방식: Project Settings 문자열 목록(ProjectZoneIds) + 레벨 액터에 같은 문자열을 손으로 입력.
//   → 오타 한 글자로 검증 실패, 이름/허용 시설 규칙이 액터마다 흩어짐.
// 새 방식: Zone 정의 에셋을 만들고, 레벨의 Zone 액터는 그 에셋을 "드롭다운으로 고르기만" 한다.
//   → ID·이름·색·허용 시설 규칙의 단일 원천(Single Source of Truth)이 에셋이 된다.
//
// 현업 건축 시뮬레이션의 "Zone Type 정의(데이터) / Zone 인스턴스(레벨 배치)" 분리와 같은 구조다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FF_FFZSTypes.h"
#include "FFZAZoneDefinition.generated.h"

UCLASS(BlueprintType, meta = (DisplayName = "FF Zone Definition"))
class FF_ZONEAUTHORING_API UFFZAZoneDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Asset Manager에서 이 에셋 묶음을 부르는 이름.
    static const FName PrimaryAssetTypeName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category = "Identity", meta = (
        DisplayName = "Zone ID",
        ToolTip = "저장·NPC·밀도·인접 그래프가 쓰는 영구 ID입니다. 예: GZ_SunsetBeachClub. 출시 후 바꾸면 저장 데이터가 끊깁니다. 영문/숫자/밑줄만 사용하세요."))
    FName ZoneId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
        ToolTip = "UI에 보여 줄 이름입니다. 비워 두면 Zone ID에서 자동으로 만듭니다."))
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true,
        ToolTip = "기획 메모용 설명입니다. 게임 규칙에는 영향이 없습니다."))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (
        ToolTip = "에디터 경계선과 건축 모드 바닥 오버레이에 쓰는 색입니다. 알파는 무시됩니다."))
    FLinearColor ZoneColor = FLinearColor(0.10f, 0.60f, 1.00f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (
        ToolTip = "켜면 모든 시설 종류를 이 Zone에 지을 수 있습니다."))
    bool bAllowAllFacilityCategories = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (
        EditCondition = "!bAllowAllFacilityCategories",
        ToolTip = "위 옵션을 끈 경우에만 사용합니다. 여기에 있는 시설 종류만 지을 수 있습니다."))
    TArray<EFFZSFacilityCategory> AllowedFacilityCategories;

    // ZoneId가 비어 있으면 에셋 이름을 쓰지 않고 NAME_None을 그대로 반환한다(오류는 검증에서 알린다).
    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring")
    FText GetResolvedDisplayName() const;

    // 에셋 자체의 규칙 오류를 모두 모은다. 오류가 없으면 true.
    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring")
    bool ValidateDefinition(TArray<FText>& OutErrors) const;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
