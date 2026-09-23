#pragma once

#include "CoreMinimal.h"
#include "FF_StaffPool.generated.h"

/**
 * 스태프 한 명. 개인별 숙련도를 추적한다 — 두 사람이 같은 레벨이어도 "언제 고용됐고
 * 언제 마지막으로 승급했는가"는 서로 다르다.
 * 근거: data/metadata/SiriKim_Docs/staff_event_design.md §4.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_StaffMember
{
	GENERATED_BODY()

	// 이 축제 안에서만 유일하면 된다. 새 축제에서 1부터 다시 발급된다.
	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	int32 StaffId = INDEX_NONE;

	// 1~3. 고용 직후에는 항상 1이다.
	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	int32 TrainingLevel = 1;

	// 고용된 날. 기본 팀은 0(축제 시작 전부터 있던 사람). 승급 가능 = CurrentDay > HiredOnDay.
	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	int32 HiredOnDay = 0;

	// 마지막으로 승급한 날. 아직 한 번도 안 올렸으면 INDEX_NONE.
	// 같은 날 두 번 승급 못 하게 막는 값이다.
	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	int32 LastPromotedDay = INDEX_NONE;

	// 무페이 기본 팀인가, 돈 주고 고용했는가. 표시·상한 계산이 갈라 써야 해서 남긴다.
	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	bool bIsBaseStaff = false;
};

/**
 * Staff 명단.
 * 무페이 기본 팀과 축제 동안 고용한 인원으로 이루어진다.
 * [참고] economy_spec.md §3.7 · §2.5.1 · §5 S6 · staff_event_design.md §2·§4.
 *
 * 고용은 **1회 구매이고 축제가 끝날 때까지 남는다.** 하루 경계에서 줄어들지 않는다 —
 * 영구라서 값어치가 남은 날짜에 비례한다 — 일찍 살수록 이득이고, 그것이 규칙이다.
 * 총원은 MaxHiredStaffCount 로 막는다(예전의 '하루 상한'이 아니라 축제 전체 상한이다).
 *
 * [주의] 스태프는 **시설 성능에 관여하지 않는다.** 시설의 회전율·객단가는 시설 강화가
 *        올리고(UFF_FacilityUpgradeSubsystem), 스태프는 사건 대응에 쓰인다.
 *        두 축을 겹치면 같은 것을 두 번 사게 된다.
 *
 * [참고] 사건 판정(능력치 합·소요시간·승급 가능 여부)은 여기 안 두고 FFF_StaffEventRules
 *        순수 함수로 뺐다 — 이 구조체는 명단만 들고 있는다.
 */
struct FESTAFIESTA_API FFF_StaffPool
{
	void Initialize(int32 InBaseCount);

	// 무페이 기본 팀 인원. 축제 내내 줄지 않는다.
	int32 GetBaseStaffCount() const { return BaseStaffCount; }

	// 이번 축제에 고용한 인원. 하루 경계를 넘어 유지된다.
	int32 GetHiredStaffCount() const { return AllStaff.Num() - BaseStaffCount; }

	// 지금 쓸 수 있는 전체 Staff 인원 = 기본 팀 + 고용 인원.
	int32 GetTotalStaffCount() const { return AllStaff.Num(); }

	// Budget 안에서 고용비를 지불할 수 있는 최대 인원을 계산한다.
	// RemainingCap 을 넘지 않으며 상태는 변경하지 않는다.
	int32 CalcAffordableHireCount(
		int32 HireCostGold,
		int32 Budget,
		int32 RemainingCap) const;

	// 고용 인원을 정확히 이 수로 맞춘다(모자라면 채우고 넘치면 최근 고용분부터 줄인다).
	// 개인 식별이 필요 없는 호출부(테스트·일괄 조정)용 — 새로 채우는 인원은 HiredOnDay 0 이다.
	// 실제 플레이 고용은 날짜를 남기는 HireStaffMembers 를 쓴다.
	void SetHiredStaffCount(int32 InHiredStaffCount);

	// RequestedCount 명을 CurrentDay 에 새로 고용한다. 전부 Lv1, HiredOnDay=CurrentDay 로 시작한다
	// (그래서 그날은 승급할 수 없다 — FFF_StaffEventRules::CanPromote 참고). 실제 플레이 고용 경로.
	void HireStaffMembers(int32 RequestedCount, int32 CurrentDay);

	// 전체 명단(기본 + 고용). UI 목록·저장이 읽는다.
	const TArray<FFF_StaffMember>& GetAllStaff() const { return AllStaff; }

	// 저장에서 명단을 통째로 되돌린다. 다음 StaffId 발급 번호도 충돌 없게 같이 맞춘다.
	void RestoreAllStaff(const TArray<FFF_StaffMember>& InStaff);

	// StaffId 한 명을 승급시킨다. 가능 여부는 FFF_StaffEventRules::CanPromote 를 따른다.
	// 승급했으면 true, 대상이 없거나 오늘 승급할 수 없으면 false(상태 변경 없음).
	bool TryPromoteStaffMember(int32 StaffId, int32 CurrentDay);

	// InStaffCount 명을 고용하는 총 비용을 계산한다. 상태는 변경하지 않는다.
	static int32 CalcHireCost(
		int32 InStaffCount,
		int32 HireCostGold);

private:
	FFF_StaffMember& AddMember(int32 HiredOnDay, bool bIsBase);

	// 무페이 기본 팀 인원 수. AllStaff 안의 bIsBaseStaff==true 개수와 항상 일치한다.
	int32 BaseStaffCount = 0;

	// 기본 팀 + 고용 인원을 함께 담는다. 순서는 의미 없다 — 구분은 bIsBaseStaff 로 한다.
	TArray<FFF_StaffMember> AllStaff;

	// 다음에 발급할 StaffId. 새 축제(Initialize)에서만 1로 되돌아간다.
	int32 NextStaffId = 1;
};
