// FFZAAuthoredZone.h
// 역할: 기존 Gameplay Zone(AFFZSGameplayZone)을 "상속"해서 디자이너 편의 기능만 더한 Zone 액터.
//
// 기존 Zone과 똑같이 FF_ZoneSystem에 등록되고 배치 판정에 쓰인다(부모 클래스 기능을 그대로 사용).
// 더해진 것:
//  1) Zone Definition 에셋을 고르면 ID·이름·허용 시설 규칙이 자동으로 채워진다(오타 불가).
//  2) Scale/회전/위치를 아무렇게나 바꿔도, 손을 떼는 순간 보이는 경계 모양은 유지한 채
//     Scale (1,1,1)·Yaw 90도 단위·1 cm 원점으로 자동 정리된다.
//  3) 편집할 때마다 검증을 돌려 오류 목록을 디테일 패널에 보여 주고, 경계선 색을 바꾼다(정상=Zone 색, 오류=빨강).
//
// FF_ZoneSystem 소스는 수정하지 않는다. 기존 AFFZSGameplayZone 액터도 계속 그대로 쓸 수 있다.
#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSZoneActor.h"
#include "FFZAAuthoredZone.generated.h"

class UFFZAZoneDefinition;

UCLASS(Blueprintable, meta = (DisplayName = "FF Authored Zone"))
class FF_ZONEAUTHORING_API AFFZAAuthoredZone : public AFFZSGameplayZone
{
    GENERATED_BODY()

public:
    AFFZAAuthoredZone();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Definition", meta = (
        ToolTip = "이 Zone의 종류입니다. 고르면 Zone ID·표시 이름·허용 시설 규칙이 자동으로 채워지고 직접 수정할 수 없게 잠깁니다."))
    TObjectPtr<UFFZAZoneDefinition> ZoneDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Authoring", meta = (
        ToolTip = "켜면 이동/회전/크기 조절을 끝낸 순간 Scale (1,1,1), Yaw 90도 단위, 정수 cm 원점으로 자동 정리합니다. 경계 모양은 그대로 유지됩니다."))
    bool bAutoNormalizeTransform = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Authoring", meta = (
        ToolTip = "켜면 경계 Spline을 항상 '닫힌 직선 다각형'으로 맞춥니다(곡선 점 → 직선, 열린 선 → 닫힘)."))
    bool bAutoFixBoundarySpline = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Zone Authoring", meta = (
        ToolTip = "마지막 편집 기준으로 검증을 통과했는지 표시합니다."))
    bool bAuthoringDataValid = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Zone Authoring", meta = (
        ToolTip = "고쳐야 할 문제 목록입니다. 비어 있으면 정상입니다."))
    TArray<FText> AuthoringIssues;

    // 정의 에셋 값을 Zone에 다시 복사한다. 이미 게임 중 등록된 Zone의 ID는 바꾸지 않는다.
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Zone Authoring")
    void ApplyDefinition();

    // 지금 즉시 Transform을 정리한다(자동 정리를 꺼 둔 경우 수동 실행용).
    UFUNCTION(CallInEditor, Category = "Zone Authoring")
    void NormalizeTransformNow();

    // 검증을 다시 실행하고 AuthoringIssues를 갱신한다.
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Zone Authoring")
    void RefreshAuthoringStatus();

    // 오버레이/에디터 선 색. 정의가 없으면 기본 파랑.
    UFUNCTION(BlueprintPure, Category = "Zone Authoring")
    FLinearColor GetZoneColor() const;

    // 경계 다각형의 월드 XY 좌표(cm).
    UFUNCTION(BlueprintPure, Category = "Zone Authoring")
    TArray<FVector2D> GetBoundaryPolygonWorldCm() const;

    // 부모 검증(ValidateZoneData) + 정의 에셋 관련 검사를 모두 모은다. 액터를 바꾸지 않는다.
    void CollectAuthoringIssues(TArray<FText>& OutIssues) const;

    // 정의 에셋의 ID와 액터의 ZoneId가 다르면 true(레벨을 저장한 뒤 정의 에셋을 바꾼 경우 등).
    bool IsOutOfSyncWithDefinition() const;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditMove(bool bFinished) override;
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
    // 정의 에셋 값을 부모 필드에 복사한다. 바뀐 것이 있으면 true.
    bool CopyDefinitionToZone();
    void UpdateEditorPresentation();

    // 재진입 방지: 정규화가 Transform을 바꾸면서 다시 편집 콜백이 들어오는 것을 막는다.
    bool bIsNormalizing = false;
};
