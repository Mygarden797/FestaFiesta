// FFZAPolygonAreaActor.h
// 역할: "바닥에 그린 다각형 영역" 액터의 공통 부모(구획 Plot, 구역 District가 상속).
//
// Zone과 같은 방식으로 Spline 점을 옮겨 모양을 만든다. Scale/회전을 바꿔도 자동으로 점 좌표에 반영된다.
// 이 영역들은 2D(높이 무관) 규칙이다. 즉 다각형 위·아래 모든 높이에 적용된다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FFZAPolygonAreaActor.generated.h"

class USceneComponent;
class USplineComponent;
class UFFZAZoneAuthoringSubsystem;

UCLASS(Abstract)
class FF_ZONEAUTHORING_API AFFZAPolygonAreaActor : public AActor
{
    GENERATED_BODY()

public:
    AFFZAPolygonAreaActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area|Components", meta = (
        ToolTip = "영역 경계입니다. 점을 선택해 Alt+드래그로 복제, Delete로 삭제할 수 있습니다. 항상 닫힌 직선 다각형으로 유지됩니다."))
    TObjectPtr<USplineComponent> BoundarySpline;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area|Identity", meta = (
        ToolTip = "저장·UI·로그에서 쓰는 고유 ID입니다. 영문/숫자/밑줄만 쓰세요. 예: PLOT_East01"))
    FName AreaId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area|Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area|Presentation", meta = (
        ToolTip = "에디터 경계선과 바닥 오버레이 색입니다."))
    FLinearColor AreaColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Area|Authoring", meta = (
        ToolTip = "고쳐야 할 문제 목록입니다. 비어 있으면 정상입니다."))
    TArray<FText> AuthoringIssues;

    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|Area")
    TArray<FVector2D> GetWorldPolygon() const;

    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|Area")
    bool ContainsWorldPoint(const FVector& WorldPoint) const;

    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|Area")
    float GetAreaSquareMeters() const;

    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|Area")
    FText GetResolvedDisplayName() const;

    // 공통 검사(ID, 단순 다각형) + 자식 클래스 추가 검사. 오류가 없으면 true.
    virtual bool ValidateAreaData(TArray<FText>& OutErrors) const;

    // 오버레이에 그릴 색. 자식이 상태(잠김 등)에 따라 바꿀 수 있다.
    virtual FLinearColor GetOverlayColor() const { return AreaColor; }

    // 오버레이에 그릴지(예: 해제된 구획은 그리지 않음).
    virtual bool ShouldDrawOverlay() const { return true; }

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Area|Authoring")
    void RefreshAuthoringStatus();

protected:
    virtual void PostRegisterAllComponents() override;
    virtual void PostUnregisterAllComponents() override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditMove(bool bFinished) override;
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

    // 자식 클래스가 자기 종류에 맞게 서브시스템에 등록/해제한다.
    virtual void RegisterWithSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem) {}
    virtual void UnregisterFromSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem) {}

    void UpdateEditorPresentation();

private:
    UFFZAZoneAuthoringSubsystem* GetAuthoringSubsystem() const;
    bool bIsNormalizing = false;
};
