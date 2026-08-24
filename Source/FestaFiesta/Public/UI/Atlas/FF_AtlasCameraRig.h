// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "UI/Core/FF_UIFlowTypes.h"
#include "FF_AtlasCameraRig.generated.h"

class ACameraActor;
class APlayerController;
class UFF_AtlasSettings;

/**
 * 프론트엔드 카메라 연출. **타이틀에서 지구로 다가가는 그 움직임을 소유한다.**
 *
 * === 왜 별도 액터인가 ===
 * 구도는 백종태가 레벨에서 눈으로 잡는 것이고 코드가 만들 수 없다. 그래서 **레벨에 놓은
 * `ACameraActor` 를 참조**로 받고, 이 리그는 그 사이를 옮겨 다니는 일만 한다.
 * 카메라 위치를 코드에 숫자로 박으면 구도를 바꿀 때마다 빌드해야 한다.
 *
 * === 보간을 직접 구현하지 않는다 ===
 * `APlayerController::SetViewTargetWithBlend()` 가 정확히 이 일을 한다 — 두 뷰타깃 사이를
 * 시간·이징으로 섞는다. 직접 위치를 Lerp 하면 FOV·포스트프로세스·종횡비까지 손으로 섞어야 하고
 * 그건 엔진이 이미 하고 있는 일이다.
 *
 * === 🔴 남의 카메라를 건드리지 않는다 ===
 * `AFF_FestivalPlayerController` 는 카메라 담당(장원재·박건영) 경계다
 * (`module-ownership.md`). 프론트엔드 레벨은 엔진 기본 `APlayerController` 를 쓰므로
 * 여기서 뷰타깃을 바꾸는 것은 그 경계를 넘지 않는다. **축제 레벨에서는 이 리그를 쓰지 않는다.**
 *
 * === 🔴 두 카메라를 같은 축에 둔다 ===
 * 지구 액터의 로컬 `+X` 가 "정면" 이라는 규약이다(`FFF_AtlasGeo`). 카메라마다 지구를 보는
 * 방향이 크게 다르면 한쪽 구도에서 규약이 깨져 **핀 앞뒤 판정과 좌우 스냅이 어긋난다.**
 * 지구의 `+X` 쪽 같은 축 위에서 **거리만 다르게** 두는 것이 맞고, 그것이 "뒤에 있던 지구가
 * 앞으로 다가온다" 는 연출 의도와도 같다. 확인은 `FF.Atlas.Status` 의 정면축 진단이다.
 *
 * === 레벨에 놓는다. 코드가 스폰하지 않는다 ===
 * 지구 액터와 같은 규칙이다 — 눈으로 잡아 둔 구도를 존중한다.
 * 리그가 없어도 프론트엔드는 그대로 동작한다 (카메라만 안 움직인다). **선택 사항이다.**
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Atlas Camera Rig"))
class FESTAFIESTA_API AFF_AtlasCameraRig : public AActor
{
	GENERATED_BODY()

public:
	AFF_AtlasCameraRig();

	/**
	 * 이 월드의 카메라 리그를 찾는다. 없으면 nullptr.
	 *
	 * `bLogIfMissing` 이 false 면 조용히 nullptr 을 준다 — 흐름 서브시스템은 화면이 바뀔 때마다
	 * 부르므로, 리그를 놓지 않은 레벨에서 매번 경고가 쌓이면 로그가 쓸모없어진다.
	 * 리그는 **없어도 되는 것**이라 그 침묵은 조용한 실패가 아니다.
	 */
	static AFF_AtlasCameraRig* Find(const UObject* WorldContextObject, bool bLogIfMissing = true);

	/**
	 * 이 화면에 맞는 카메라로 옮긴다.
	 *
	 * 이동 시간은 **두 화면 중 더 깊은 쪽**이 정한다 — 타이틀↔지구본은 `GlobeApproachSeconds`,
	 * 지구본↔국가 지도는 `CountryMapTransitionSeconds`. 그래서 들어갈 때와 나올 때가 같은 길이다.
	 * `움직임 감소` 가 켜져 있으면 항상 즉시다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas|Camera")
	void ViewForScreen(EFF_UIFlowScreen Screen, bool bInstant = false);

	/**
	 * 진행 중인 카메라 이동을 **즉시 끝낸다.** 목표 구도로 붙는다.
	 * 확정 요구사항 — 모든 연출은 입력으로 즉시 Skip 된다. 반복 플레이에서 매번 다 보게 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas|Camera")
	void SkipCameraMove();

	/** 지금 카메라가 두 구도 사이를 이동하는 중인가. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas|Camera")
	bool IsMoving() const;

	/** 콘솔 `FF.Atlas.Camera` 가 찍는 진단 문자열. */
	FString DescribeState() const;

protected:
	virtual void BeginPlay() override;

	/**
	 * 타이틀·플레이 허브·저장 기록·옵션에서 쓰는 구도. **멀리서 보는 지구다.**
	 * 지구를 화면 오른쪽에 두고 왼쪽을 UI 에 비워 두는 것이 확정 방향이다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Camera")
	TObjectPtr<ACameraActor> TitleCamera;

	/** 나라를 고르는 구도. **가까이 다가간 지구다.** 비어 있으면 타이틀 구도를 그대로 쓴다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Camera")
	TObjectPtr<ACameraActor> GlobeCamera;

	/** 국가 지도 구도. 비어 있으면 지구본 구도를 그대로 쓴다 (U3-b 까지는 비워 두면 된다). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Camera")
	TObjectPtr<ACameraActor> CountryMapCamera;

	/**
	 * 이징. `VTBlend_EaseInOut` 이 기본이다 — 등속(`Linear`)으로 다가가면 기계처럼 보이고,
	 * 시작·끝이 부드러워야 "다가온다" 는 느낌이 난다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Camera")
	TEnumAsByte<EViewTargetBlendFunction> BlendFunction = VTBlend_EaseInOut;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Camera", meta = (ClampMin = "1.0", ClampMax = "6.0"))
	float BlendExponent = 2.f;

private:
	/** 그 화면이 쓸 카메라. 비어 있으면 한 단계 얕은 구도로 내려간다. */
	ACameraActor* ResolveCameraForScreen(EFF_UIFlowScreen Screen) const;

	/**
	 * 화면의 "깊이". 이동 시간을 정하는 데만 쓴다.
	 * 0 = 프론트엔드 메뉴, 1 = 지구본, 2 = 국가 지도.
	 */
	static int32 GetScreenDepth(EFF_UIFlowScreen Screen);

	/** 두 화면 사이 이동 시간. `움직임 감소` 를 여기서 걸러 준다. */
	float GetBlendSeconds(EFF_UIFlowScreen FromScreen, EFF_UIFlowScreen ToScreen) const;

	/** 뷰타깃을 실제로 바꾼다. 컨트롤러가 없으면 이유를 남기고 아무것도 하지 않는다. */
	void ApplyViewTarget(ACameraActor* Camera, float BlendSeconds);

	APlayerController* GetOwningController() const;

	const UFF_AtlasSettings& GetAtlasSettings() const;

	/** 지금 보고 있는(또는 가고 있는) 화면. 이동 시간 계산의 출발점이다. */
	EFF_UIFlowScreen CurrentScreen = EFF_UIFlowScreen::None;

	/** Skip 이 붙일 목표. */
	TWeakObjectPtr<ACameraActor> PendingCamera;
};
