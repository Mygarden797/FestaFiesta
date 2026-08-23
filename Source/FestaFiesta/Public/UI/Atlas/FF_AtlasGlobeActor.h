// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Atlas/FF_AtlasGeo.h"
#include "FF_AtlasGlobeActor.generated.h"

class UArrowComponent;
class UFF_AtlasSettings;
class UFF_FestivalAtlasCatalog;
class UFF_UIAccessibilitySubsystem;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 국가 하나가 포커스되거나 선택될 때. `FName` 은 `CountryId` 다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnAtlasCountryChanged, FName, CountryId);

/**
 * 화면(U3-b)이 한 프레임에 필요한 핀 하나의 상태.
 *
 * 위젯이 액터의 컴포넌트를 직접 뒤지지 않게 한다 — 그러면 UI 가 액터 내부 구조에 묶이고,
 * 컴포넌트 이름을 바꾸는 순간 조용히 핀이 사라진다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasPinState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Atlas")
	FName CountryId;

	/** 월드 위치. 위젯이 `ProjectWorldToScreen` 으로 화면 좌표로 바꾼다. */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas")
	FVector WorldLocation = FVector::ZeroVector;

	/** 지구 앞면인가. 뒤편 핀은 위젯도 숨겨야 한다 — 액터만 숨기면 버튼이 허공에 남는다. */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas")
	bool bFacingViewer = false;

	/**
	 * 고를 수 있는가 (= 국가의 `bUnlocked`).
	 * ★ **잠긴 핀도 목록에 들어온다.** 포커스는 가고 선택만 막는다 — 위젯이 걸러 내지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas")
	bool bSelectable = false;
};

/**
 * 국가마다 하나 생기는 핀·조형물 묶음. 액터 내부용이다.
 *
 * **인덱스로 두 목록을 짝짓지 않는다** — `CountryId` 를 안에 들고 있다.
 * 국가를 하나 추가하면 전원의 자리가 밀리는 종류의 버그를 애초에 만들지 않는다.
 */
USTRUCT()
struct FESTAFIESTA_API FFF_AtlasPinInstance
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FName CountryId;

	UPROPERTY(Transient)
	FFF_AtlasLatLong Location;

	UPROPERTY(Transient)
	bool bSelectable = false;

	/** 지표에 붙은 기준점. 위쪽이 지표 법선이다. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Anchor;

	/** 조형물. 메시 슬롯이 비고 프리미티브 대체도 껐으면 `nullptr` 이다. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Landmark;

	/** 선택됐을 때만 켜지는 조명. 크기 변화만으로 선택을 알리지 않기 위한 두 번째 신호다. */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Light;
};

/**
 * Festival Atlas 의 지구본. **3D 씬 쪽 절반**이다.
 *
 * === 무엇을 하고 무엇을 하지 않는가 ===
 * 이 액터는 **지구를 돌리고 핀을 어디에 둘지 계산한다.** 입력을 읽지 않고, 화면을 만들지 않고,
 * 카메라를 움직이지 않는다.
 *
 * - 입력은 `UFF_AtlasGlobeScreen`(U3-b)이 해석해서 `AddDragInput` · `StepFocus` 로 넘긴다.
 *   `AFF_OverviewCameraPawn` 과 같은 규칙이다 — **입력 차단 판단은 Controller·화면 쪽 책임**이라
 *   씬 액터가 키를 직접 읽으면 UI 가 떠 있는 동안에도 지구가 돌아간다.
 * - 카메라 연출은 U3-c 다. 여기서는 지구의 자전·기울기만 소유한다.
 * - 국가 클릭 판정은 **넓은 UMG 핀 버튼**이 한다 (2026-08-21 확정). 그래서 이 액터의
 *   모든 컴포넌트는 **콜리전이 꺼져 있다** — 켜 두면 지구가 월드 클릭을 먹는다.
 *
 * === 🔴 레벨에 놓는 방법 — 액터의 `+X` 화살표가 카메라를 향해야 한다 ===
 * 좌표 규약은 `FFF_AtlasGeo` 가 갖는다. 정면이 로컬 `+X` 이므로 **카메라를 액터의 +X 쪽에
 * 두거나, 액터를 돌려 +X 가 카메라를 보게 한다.** 에디터에서 보이는 빨간 화살표가 그 방향이다.
 * 이 규약 하나만 지키면 액터를 레벨 어디에 어떤 각도로 놓아도 핀·조형물·앞뒤 판정이 다 맞는다.
 *
 * === 회전은 루트가 아니라 `GlobeRoot` 가 한다 ===
 * 액터 자신을 돌리면 레벨에 놓은 방향(= 정면 규약)이 함께 돌아가 버린다.
 * 그래서 액터 트랜스폼은 놓은 그대로 두고 **자식 `GlobeRoot` 의 상대 회전만** 바꾼다.
 *
 * === 에셋이 없어도 동작한다 ===
 * 지구 텍스처·머티리얼·조형물 메시가 아직 하나도 없다. 슬롯이 비면 엔진 기본 도형(구·원뿔)과
 * 기본 머티리얼로 대신 그린다. 텍스처 임포트와 머티리얼은 백종태가 에디터에서 붙인다.
 *
 * === 핀은 PIE 에서 생긴다 ===
 * 핀·조형물 컴포넌트는 `BeginPlay` 에서 카탈로그를 읽어 만든다. 에디터 뷰포트에는 지구와
 * 구름만 보인다. 구성 스크립트에서 컴포넌트를 동적으로 만들면 **레벨 에셋에 저장되어**
 * `.uasset` 충돌로 남는 사고가 잘 생기는데, 팀 공용 레벨에서 그것은 감수할 만한 이득이 아니다.
 * 카탈로그를 고친 뒤에는 콘솔 `FF.Atlas.Rebuild` 로 다시 만든다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Atlas Globe"))
class FESTAFIESTA_API AFF_AtlasGlobeActor : public AActor
{
	GENERATED_BODY()

public:
	AFF_AtlasGlobeActor();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 지구·구름 메시에 에셋과 크기를 적용한다. **에디터 뷰포트에서 바로 보이게 하려고 여기서 한다** —
	 * `BeginPlay` 에만 두면 PIE 를 눌러야 지구가 나타나서 반지름·텍스처 보정을 눈으로 맞출 수 없다.
	 *
	 * 여기서 컴포넌트를 **만들지는 않는다.** 이미 있는 컴포넌트의 속성만 건드린다 —
	 * 구성 스크립트에서 동적 생성한 컴포넌트는 레벨 에셋에 직렬화되어 `.uasset` 충돌로 남는다.
	 * 핀·조형물은 그래서 `BeginPlay` 에서만 만든다.
	 */
	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * 이 월드의 지구본을 찾는다. 못 찾으면 nullptr 이고 이유를 로그에 남긴다.
	 *
	 * **레벨에 배치한 것을 쓴다.** 코드가 스폰하지 않는다 — 조명·구도를 눈으로 잡아 둔 것을
	 * 존중하는 것이 이 프로젝트의 선례다(`AFF_FestivalGameMode::PrepareVisitorCharacter`).
	 * 위젯이 이 함수를 부르는 것은 읽기 조회라 허용되지만, **위젯이 액터를 스폰하면 안 된다** —
	 * 위젯은 월드와 함께 죽고 액터는 남는다 (안티패턴 7).
	 */
	static AFF_AtlasGlobeActor* Find(const UObject* WorldContextObject);

	/**
	 * 지구를 보이게/숨기게 한다. 끄면 Tick 도 멈춘다.
	 * 국가 지도나 축제 레벨로 넘어갈 때 화면이 부른다. **각도와 선택은 유지한다.**
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void SetGlobeActive(bool bActive);

	// --- 조작. 화면이 부른다 ---

	/**
	 * 드래그 픽셀만큼 돌린다. **관성은 없다.**
	 * 부르면 자동 자전이 멈추고 `AutoSpinResumeDelaySeconds` 뒤에 다시 시작된다.
	 * 진행 중인 전환도 취소한다 — 손으로 돌리는 중에 지구가 스스로 움직이면 조작을 빼앗긴 느낌이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void AddDragInput(const FVector2D& DragPixels);

	/**
	 * 이 국가를 정면으로 돌린다. **거리와 무관하게 같은 시간**이 걸린다.
	 * 없는 `CountryId` 면 아무것도 하지 않고 이유를 로그에 남긴다.
	 * `bInstant` 면 보간 없이 즉시 — `움직임 감소` 가 켜져 있으면 항상 즉시다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void FocusOnCountry(FName CountryId, bool bInstant = false);

	/**
	 * 좌우 입력으로 다음 핀. `ScreenDirection` 은 **화면 기준**이다 — `+1` 이 오른쪽.
	 * 잠긴 국가도 후보에 넣는다 (포커스는 가고 선택만 막는다).
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void StepFocus(int32 ScreenDirection);

	/**
	 * 지금 정면에 가장 가까운 핀으로 다시 붙인다.
	 * 스틱으로 자유 회전한 뒤 손을 뗐을 때 화면이 부른다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void ReSnapToNearestCountry();

	/**
	 * 선택 표시를 옮긴다. 조형물이 커지고 조명이 켜진다.
	 * **잠긴 국가도 넣을 수 있다** — 잠금 판정은 화면이 사유 문구와 함께 처리한다.
	 * `NAME_None` 을 넣으면 선택을 해제한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void SetSelectedCountry(FName CountryId);

	/** 진행 중인 회전을 끝 상태로 즉시 끝낸다. **모든 연출은 입력으로 Skip 된다.** 도착으로 처리한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void SkipTransition();

	/**
	 * 자동 자전을 잠시 멈춘다/재개한다. **텍스처 정렬을 맞출 때 쓴다** —
	 * 지구가 계속 돌면 무엇이 정면인지 볼 수 없다.
	 *
	 * 설정값(`AutoSpinDegreesPerSecond`)을 건드리지 않는다. 그건 팀 공용 ini 라
	 * 혼자 확인하려고 바꾸면 커밋에 섞인다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void SetAutoSpinPaused(bool bPaused);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas")
	bool IsAutoSpinPaused() const { return bAutoSpinPaused; }

	/**
	 * 진행 중인 회전을 **버린다.** 지금 각도에 그대로 멈추고 포커스를 잃는다.
	 *
	 * ★ `SkipTransition` 과 다르다 — 저쪽은 "끝까지 갔다" 고 보고 도착을 알린다.
	 *   드래그로 지구를 돌린 뒤에도 이전 국가가 포커스로 남으면, 왼쪽 패널이 정면에 없는
	 *   나라 이름을 계속 띄운다. 취소와 도착을 같은 함수로 처리하면 그 모순이 생긴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void CancelTransition();

	/** 카탈로그를 다시 읽어 핀·조형물을 새로 만든다. 회전과 선택은 유지한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void RebuildPins();

	// --- 조회 ---

	/**
	 * 지금 정면에 **도착해 있는** 국가. 없으면 `NAME_None`.
	 *
	 * 회전이 진행 중이거나 드래그로 포커스가 풀린 동안은 `NAME_None` 이다 —
	 * "가고 있는 중" 을 도착과 같은 값으로 두면 화면이 아직 오지 않은 나라를 띄운다.
	 * 도착하면 `OnCountryFocused` 가 울린다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas")
	FName GetFocusedCountryId() const { return FocusedCountryId; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas")
	FName GetSelectedCountryId() const { return SelectedCountryId; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas")
	bool IsTransitioning() const { return bTransitioning; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas")
	FFF_AtlasGlobeOrientation GetOrientation() const { return Orientation; }

	/**
	 * 회전을 직접 지정한다. **뒤로 갈 때 지구 각도를 되살리는 데 쓴다** —
	 * 국가 지도에서 지구본으로 돌아왔을 때 각도가 리셋되면 답답하다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void SetOrientation(const FFF_AtlasGlobeOrientation& NewOrientation);

	/** 한 프레임에 필요한 핀 전체. 위젯이 핀 버튼을 배치할 때 부른다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Atlas")
	void GetPinStates(TArray<FFF_AtlasPinState>& OutPins) const;

	/** 핀 하나의 월드 위치. 못 찾으면 false 이고 `OutLocation` 을 건드리지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Atlas")
	bool GetCountryPinWorldLocation(FName CountryId, FVector& OutLocation) const;

	/** 콘솔 `FF.Atlas.Status` 가 찍는 진단 문자열. */
	FString DescribeState() const;

	// --- 알림 ---

	/**
	 * 정면 국가가 바뀌었을 때. 회전이 끝나면 그 국가 이름으로, 드래그 등으로 포커스를 잃으면
	 * `NAME_None` 으로 울린다. 즉시 이동이면 그 프레임에 바로 울린다.
	 * 같은 값으로는 다시 울리지 않는다 — 드래그 중 매 프레임 방송되지 않게 한다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Atlas")
	FFF_OnAtlasCountryChanged OnCountryFocused;

	/** 선택이 바뀌었을 때. 같은 국가를 다시 넣으면 울리지 않는다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Atlas")
	FFF_OnAtlasCountryChanged OnSelectedCountryChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- 형상. 「무엇으로 그리나·얼마나 크나」는 액터가 갖는다 ---

	/** 지구 반지름(uu). 메시 실제 크기와 무관하게 이 값이 되도록 스케일을 계산한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Shape", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float GlobeRadius = 5000.f;

	/** 구름 구체는 지표보다 이 배율만큼 크다. 1.02 면 반지름의 2% 위에 뜬다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Shape", meta = (ClampMin = "1.0", ClampMax = "1.5"))
	float CloudRadiusScale = 1.02f;

	// --- 시작 각도. **에디터 뷰포트와 PIE 가 같아지는 지점이다** ---

	/**
	 * 켜면 카탈로그의 **첫 국가**를 정면으로 시작한다. 끄면 `Start Lat Long` 을 쓴다.
	 *
	 * 켜 두는 것이 기본이다 — 축제가 하나뿐인 지금은 그 나라가 보이는 게 맞다.
	 * 타이틀 배경으로 특정 구도를 잡고 싶으면 끄고 위경도를 직접 넣는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Start")
	bool bStartFacingFirstCountry = true;

	/**
	 * 시작할 때 정면에 올 지점. `b Start Facing First Country` 를 끈 경우에만 쓴다.
	 *
	 * ★ **텍스처 정렬을 맞출 때 이걸 쓴다.** `(0, 0)` 으로 두면 기니 만(아프리카 서해안 적도)이
	 *   정면에 와야 한다. 다른 지역이 오면 그 차이가 곧 `Globe Texture Yaw Offset` 이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Start")
	FFF_AtlasLatLong StartLatLong;

	/** 조형물 높이(uu). 메시가 무엇이든 이 높이가 되도록 등비 스케일을 계산한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Shape", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float LandmarkHeight = 500.f;

	// --- 에셋 슬롯. 비어 있으면 엔진 기본 도형으로 대신 그린다 ---

	/**
	 * 지구 구체 메시. **기본값이 엔진 기본 구**이므로 배치하면 바로 보인다.
	 * 일부러 비우면 지구가 보이지 않고 `BeginPlay` 가 경고를 남긴다 (런타임 대체는 하지 않는다 —
	 * 비운 것을 되살리면 슬롯을 비울 방법이 없어진다).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Assets")
	TObjectPtr<UStaticMesh> GlobeMeshAsset;

	/** 지구 표면 머티리얼. **여기에 임포트한 텍스처를 붙인다.** 비우면 기본 머티리얼이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Assets")
	TObjectPtr<UMaterialInterface> GlobeMaterial;

	/** 구름 머티리얼. 반투명이어야 한다. 비우면 구름 구체를 아예 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Assets")
	TObjectPtr<UMaterialInterface> CloudMaterial;

	/**
	 * 지구 텍스처를 핀에 맞추는 보정 각도(도). **메시만 돌리고 핀은 그대로 둔다.**
	 *
	 * 구체 메시의 UV 배치와 등장방형 텍스처의 경도 0 위치는 에셋마다 다르고 코드로는 알 수 없다.
	 * 좌표 규약을 UV 에 맞춰 고치는 대신 **여기 한 칸으로 텍스처를 돌린다** — 핀 계산이
	 * 언제나 정답이고, 메시를 더 좋은 것으로 갈아도 이 숫자만 다시 맞추면 된다.
	 *
	 * 맞추는 방법: 카탈로그의 국가 핀 하나를 정면으로 돌려 놓고(`FF.Atlas.Focus`),
	 * 텍스처의 그 나라가 정면에 오도록 이 값을 조절한다.
	 *
	 * ⚠️ **이 노브는 경도 오프셋만 보정한다.** 좌표 규약이 동쪽을 `-Y` 로 두어 표준 우수
	 * 구면 매개화의 거울상이므로, 표준 등장방형 텍스처를 표준 UV 구체에 감으면 **동서가
	 * 뒤집혀** 보일 수 있다. 반전은 회전으로 상쇄되지 않는다 — 이 값을 -180~180 전체로
	 * 훑어도 핀이 그 나라에 올라오지 않으면 **머티리얼에서 U 를 반전**(`1 - U`)한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Assets", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float GlobeTextureYawOffset = 0.f;

	/**
	 * 조형물 메시가 비었을 때 대신 세우는 프리미티브. **기본값이 엔진 기본 원뿔**이다.
	 *
	 * ★ 2026-08-21 결정 — 실제 메시가 없으니 **프리미티브로 먼저 확인한다.**
	 *   `bUseFallbackLandmark` 를 끄면 계획서의 `비어 있으면 붙이지 않는다` 대로 동작한다.
	 *   이것까지 비우면 조형물 없이 핀 위치만 남는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Assets")
	TObjectPtr<UStaticMesh> FallbackLandmarkMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Assets")
	bool bUseFallbackLandmark = true;

	// --- 선택 조명 ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Selection", meta = (ClampMin = "0.0"))
	float SelectionLightIntensity = 8000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Selection")
	FLinearColor SelectionLightColor = FLinearColor(1.f, 0.92f, 0.72f);

	// --- 데이터 ---

	/**
	 * 읽을 카탈로그. **비우는 것이 기본이다** —
	 * 비어 있으면 `UFF_UIFlowSettings::AtlasCatalog` 를 쓴다. 그것이 팀 전체의 단일 출처다.
	 * 여기 채우는 것은 다른 카탈로그로 이 액터만 시험해 볼 때뿐이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Atlas|Data")
	TSoftObjectPtr<UFF_FestivalAtlasCatalog> CatalogOverride;

private:
	/** 카탈로그를 찾는다. `CatalogOverride` → 흐름 설정 순서. 없으면 nullptr 이고 로그를 남긴다. */
	const UFF_FestivalAtlasCatalog* ResolveCatalog() const;

	/** `CountryId` 로 핀 인덱스를 찾는다. 없으면 `INDEX_NONE`. */
	int32 FindPinIndex(FName CountryId) const;

	/**
	 * 핀 위경도만 뽑아 온다. **좌우 이동과 재스냅이 반드시 같은 후보 집합을 봐야 한다** —
	 * 두 곳에 루프를 복붙하면 나중에 한쪽만 고쳐도 컴파일이 통과하고, 스틱을 놓았을 때
	 * 방금 좌우로 옮긴 핀이 아닌 다른 핀으로 튄다.
	 */
	void GetPinLocations(TArray<FFF_AtlasLatLong>& OutLocations) const;

	/** 포커스 국가를 바꾸고 값이 실제로 달라졌을 때만 알린다. */
	void SetFocusedCountryInternal(FName CountryId);

	/**
	 * 조작이 들어왔다고 표시한다. 자동 자전 재개 타이머를 되감는다.
	 * **입력을 받는 모든 함수가 이걸 부른다** — 두 값을 따로 건드리면 한 곳이 빠진다.
	 */
	void MarkInputReceived();

	/**
	 * `CountryId` 로 들고 있는 상태(선택·포커스·전환 목표)를 지금 핀 집합과 맞춘다.
	 * **핀을 다시 만드는 모든 경로가 이걸 불러야 한다** — 사라진 국가가 선택으로 남으면
	 * 조형물도 조명도 없는데 화면은 계속 그 나라를 그린다.
	 */
	void ReconcileStateWithPins();

	/** 만든 핀·조형물을 전부 지운다. `RebuildPins` 가 먼저 부른다. */
	void DestroyPins();

	/** 지구·구름 메시에 에셋과 크기를 적용한다. 슬롯이 비면 기본 도형으로 대체한다. */
	void ApplyGlobeVisuals();

	/** 회전 상태를 `GlobeRoot` 에 옮긴다. */
	void ApplyOrientationToScene();

	/**
	 * 시작 회전. `OnConstruction`(에디터)과 `BeginPlay`(런타임)가 **같은 함수를 쓴다** —
	 * 그래서 에디터 뷰포트에 보이는 각도가 PIE 에서 그대로 나온다.
	 * 따로 계산하면 둘이 갈라지고, "에디터랑 실행이 다르다" 가 된다.
	 */
	FFF_AtlasGlobeOrientation ComputeStartOrientation() const;

	/** 뒤편 핀·조형물을 숨기고 선택 표시를 갱신한다. Tick 이 매 프레임 부른다. */
	void UpdatePinPresentation();

	/** 선택된 조형물만 커지고 조명이 켜지게 한다. */
	void ApplySelectionVisuals();

	/** 전환 시간과 자동 자전을 `움직임 감소` 로 걸러 준다. */
	float GetEffectiveTransitionSeconds() const;
	bool IsAutoSpinAllowed() const;

	/**
	 * 관측자(플레이어 카메라) 위치. 없으면 false.
	 * **읽기만 한다** — 카메라를 움직이는 것은 U3-c 이고 소유자도 다르다.
	 */
	bool GetViewerLocation(FVector& OutLocation) const;

	/**
	 * 지금 카메라 거리로 계산한 핀 가시성 기준.
	 * 카메라를 못 찾으면 여유값만 쓴다 — 그때는 원근 보정을 할 근거가 없다.
	 */
	float ComputePinVisibleMinDot() const;

	/** 설정을 못 읽는 상황이 없도록 항상 유효한 참조를 돌려준다. */
	const UFF_AtlasSettings& GetAtlasSettings() const;

	// --- 컴포넌트 ---

	/** 레벨에 놓은 방향을 그대로 유지하는 고정 루트. **여기는 절대 회전시키지 않는다.** */
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Atlas")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 자전·기울기가 들어가는 회전 루트. 핀과 조형물이 여기 매달린다. */
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Atlas")
	TObjectPtr<USceneComponent> GlobeRoot;

	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Atlas")
	TObjectPtr<UStaticMeshComponent> GlobeMesh;

	/** 지표에 대해 천천히 반대로 도는 구름. `GlobeRoot` 의 자식이라 자전을 물려받는다. */
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Atlas")
	TObjectPtr<UStaticMeshComponent> CloudMesh;

#if WITH_EDITORONLY_DATA
	/** 에디터에서 정면(+X)이 어디인지 보여 주는 화살표. 이것을 카메라 쪽으로 돌려 놓는다. */
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Atlas")
	TObjectPtr<UArrowComponent> FrontArrow;
#endif

	// --- 상태 ---

	UPROPERTY(Transient)
	TArray<FFF_AtlasPinInstance> Pins;

	FFF_AtlasGlobeOrientation Orientation;

	FName FocusedCountryId;
	FName SelectedCountryId;

	// --- 전환 ---

	bool bTransitioning = false;
	FFF_AtlasGlobeOrientation TransitionFrom;
	FFF_AtlasGlobeOrientation TransitionTo;
	FName TransitionTargetCountryId;
	float TransitionElapsed = 0.f;
	float TransitionDuration = 0.f;

	/** 마지막 조작 이후 흐른 시간. 이것이 재개 지연을 넘으면 자동 자전이 돌아온다. */
	float SecondsSinceInput = 0.f;

	/**
	 * 한 번이라도 조작이 들어왔나.
	 *
	 * ★ **첫 진입에는 재개 지연을 적용하지 않는다.** 지연은 "손을 뗀 뒤 얼마나 기다리나" 라는
	 *   뜻이므로, 아직 아무도 만지지 않은 상태에 적용하면 지구본에 들어온 직후 몇 초간
	 *   지구가 멈춰 있다 — 배경이 죽은 것처럼 보인다. 처음부터 돌고, 조작한 뒤에만 기다린다.
	 */
	bool bHasReceivedInput = false;

	/** 자동 자전을 손으로 멈춰 둔 상태. 설정값과 별개인 런타임 스위치다. */
	bool bAutoSpinPaused = false;

	/** 구름의 누적 상대 자전각. */
	float CloudSpin = 0.f;

	/**
	 * 접근성 서브시스템. **매 Tick 물어본다.**
	 * 델리게이트를 구독하지 않는 이유는 이 액터에 이미 Tick 이 있어서 구독이 얻는 것이 없고,
	 * 해제를 빼먹어 죽은 액터를 역참조하는 실패 경로만 하나 늘기 때문이다.
	 */
	TWeakObjectPtr<UFF_UIAccessibilitySubsystem> Accessibility;
};
