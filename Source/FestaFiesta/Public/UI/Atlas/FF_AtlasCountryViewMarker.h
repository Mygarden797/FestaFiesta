#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Atlas/FF_AtlasGeo.h"
#include "FF_AtlasCountryViewMarker.generated.h"

class AFF_AtlasGlobeActor;
class UFF_FestivalAtlasCatalog;

/** 나라별 보기 지점의 에디터 전용 손잡이. 런타임은 카탈로그에 저장된 위경도를 사용한다. */
UCLASS(NotBlueprintable, meta = (DisplayName = "FF Atlas Country View Marker"))
class FESTAFIESTA_API AFF_AtlasCountryViewMarker : public AActor
{

	GENERATED_BODY()
public:
	AFF_AtlasCountryViewMarker();
	virtual bool IsEditorOnly() const override { return true; }

	UPROPERTY(EditInstanceOnly, Category = "Country View", meta = (DisplayName = "대상 지구"))
	TObjectPtr<AFF_AtlasGlobeActor> Globe;

	UPROPERTY(EditInstanceOnly, Category = "Country View", meta = (DisplayName = "국가 카탈로그"))
	TObjectPtr<UFF_FestivalAtlasCatalog> Catalog;

	UPROPERTY(EditInstanceOnly, Category = "Country View", meta = (DisplayName = "나라", GetOptions = "GetCountryOptions"))
	FName CountryId;

	/** 0은 기본 카메라 거리. 0.2는 지구 중심까지 거리의 20%만큼 접근한다. */
	UPROPERTY(EditAnywhere, Category = "Country View", meta = (DisplayName = "카메라 접근 비율", ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0", UIMax = "0.5"))
	float ApproachFraction = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Country View", meta = (DisplayName = "마커 표시 높이", ClampMin = "0.0"))
	float MarkerSurfaceOffset = 100.f;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Country View", meta = (DisplayName = "작업 결과"))
	FString LastResult;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Country View", meta = (DisplayName = "1. 저장된 보기 불러오기"))
	void LoadCountryView();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Country View", meta = (DisplayName = "2. 현재 위치와 거리 저장"))
	void SaveCountryView();

	/** 에디터 뷰포트만 이동한다. 지구·게임 카메라·카탈로그는 변경하지 않는다. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Country View", meta = (DisplayName = "3. 지형 미리보기"))
	void PreviewCountryView();

	UFUNCTION(BlueprintPure, Category = "Country View")
	FFF_AtlasLatLong GetMarkerLocation() const;

	/** 메모리의 카탈로그에 반영한다. 디스크 저장은 SaveCountryView에서만 한다. */
	UFUNCTION(BlueprintCallable, Category = "Country View")
	bool ApplyCountryView();

	UFUNCTION()
	TArray<FString> GetCountryOptions() const;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif

private:
	USceneComponent* FindGlobeRoot() const;
	void SnapToSurface();
};
