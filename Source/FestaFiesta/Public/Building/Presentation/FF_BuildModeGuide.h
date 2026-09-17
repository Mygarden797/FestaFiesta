// FF_BuildModeGuide.h

// 건축 세션 동안 좌하단에 표시되는 조작 안내 WBP의 네이티브 클래스 계약을 선언한다.
// 흐름: BuildModeSubsystem이 파생 클래스를 로드 -> 위젯 생성·Viewport 부착 -> 세션 종료 시 제거.
// 불변식: 레이아웃은 파생 WBP가 소유하며 네이티브는 표시용 상태·공통 안내 문구만 관리한다.
// 실패/수명: WBP가 없으면 안내만 생략하며 건축 세션의 권위 상태와 입력은 계속 유지한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "FF_BuildModeGuide.generated.h"

/**
 * 건축 세션(BuildMode/CatalogOpen/Placing/Moving) 동안 좌하단에 고정 표시되는 조작 안내 패널입니다.
 * 배경·배치는 파생 WBP가 소유하고, 현재 조작과 격자 의미는 저장된 텍스트 행에 반영합니다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Mode Guide"))
class FESTAFIESTA_API UFF_BuildModeGuide : public UUserWidget
{
	GENERATED_BODY()

public:
	// 저장된 WBP를 다시 만들지 않고 기존 TXT_GuideRow_0~4가 있을 때만 현재 배치 조작을 반영한다.
	// 잘못된 디자이너 계층이나 호출 시점은 조용히 건너뛰고 다음 Construct에서 캐시값을 다시 적용한다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Guide")
	void SetPlacementGuideState(
		bool bInPlacementPreviewActive,
		bool bInMoving,
		float CurrentYawDegrees,
		float RotationSnapDegrees,
		int32 PositionSnapCm,
		bool bInPlacementGridVisible);

	// Separate setter preserves existing Blueprint calls to SetPlacementGuideState.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Guide")
	void SetDragPlacementEnabled(bool bInDragPlacementEnabled);

	// 건축 전용 안내·도구 UI에서 사용할 공통 도움말.
	// 격자의 공간 안내와 시설 전체의 배치 판정을 구분하며 실제 입력 처리는 소유하지 않는다.
	static FText MakePlacementHelpText(bool bInMoving, bool bInDragPlacementEnabled = false);
	static FText MakeBuildOverviewHelpText();

protected:
	virtual void NativeConstruct() override;

private:
	void ApplyGuideText();

	bool bPlacementPreviewActive = false;
	bool bMoving = false;
	bool bDragPlacementEnabled = false;
	bool bPlacementGridVisible = true;
	float CachedYawDegrees = 0.0f;
	float CachedSnapDegrees = 45.0f;
	int32 CachedPositionSnapCm = 100;
};
