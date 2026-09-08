#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Festival/FF_RequiredFacilityTypes.h"
#include "FF_RequiredFacilitiesSubsystem.generated.h"

class UFF_FestivalDefinition;
class UFF_FestivalProgressionSubsystem;
class UFFZSWorldSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnRequiredFacilitiesChanged);
DECLARE_MULTICAST_DELEGATE(FFF_OnRequiredFacilitiesChangedNative);

// 필수 시설의 설치 조건과 UI 조회 경계. 단계 전환도 같은 판정을 사용한다.
UCLASS()
class FESTAFIESTA_API UFF_RequiredFacilitiesSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	// 현재 권위 상태를 조회한다. UI 생성 시 한 번, 변경 알림 뒤에 다시 읽는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Festival|Setup")
	FFF_RequiredFacilitiesStatus GetRequiredFacilitiesStatus() const;

	// 부족한 시설이 있는 카테고리 탭의 느낌표 표시용.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Festival|Setup")
	bool HasMissingRequiredFacilitiesInCategory(EFFZSFacilityCategory Category) const;

	// 카드별 필수 개수/부족 개수/안내 조회. 필수 시설이 아니면 false.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Festival|Setup")
	bool TryGetRequiredFacilityRow(FName FacilityDefinitionId, FFF_RequiredFacilityRow& OutRow) const;

	// 설치/철거/수명주기/복원/축제 정의 교체 뒤 재조회 신호. 부족 개수가 같아도 발행할 수 있다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Festival|Setup")
	FFF_OnRequiredFacilitiesChanged OnRequiredFacilitiesChanged;
	// C++ 위젯/구독자용 동일한 갱신 신호.
	FFF_OnRequiredFacilitiesChangedNative OnRequiredFacilitiesChangedNative;

	// 월드 없이도 동일한 조건을 회귀 검증할 수 있는 값 기반 판정.
	// ★ 판정 규칙의 유일한 출처다. GetRequiredFacilitiesStatus()는 현재 월드를 물어다 이 함수에
	//   넘기는 wrapper이므로, 규칙을 바꿀 때는 여기만 고친다.
	static FFF_RequiredFacilitiesStatus EvaluateRequirements(
		const UFF_FestivalDefinition* Definition, const FFFZSReadModel& Model, bool bWorldAvailable = true);

private:
	void BindProgression();
	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);
	UFUNCTION()
	void HandleRequirementsChanged();
	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;
	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalProgressionSubsystem> Progression;
};
