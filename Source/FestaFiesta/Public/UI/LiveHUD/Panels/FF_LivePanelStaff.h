// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelStaff.generated.h"

class UFF_FestivalViewModelSubsystem;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * 인력이 남아도는 정도.
 *
 * !! 색으로만 구분하지 않는다 !! (AT-011) 상태마다 문구와 숫자가 함께 붙는다.
 */
UENUM(BlueprintType)
enum class EFF_LiveStaffIdleState : uint8
{
	/** 전체 인원이나 배치 인원을 아직 못 읽었다. **`0명 놀고 있음` 이 아니다.** */
	Unknown		UMETA(DisplayName = "모름"),

	/** 전원이 어딘가에 묶여 있다. */
	AllBusy		UMETA(DisplayName = "전원 배치"),

	/** 놀고 있는 인원이 있다. 임금은 나가는데 쓰이지 않는다. */
	HasIdle		UMETA(DisplayName = "남는 인원 있음"),

	/**
	 * 배치 인원이 전체보다 많다. **데이터가 어긋난 것이다.**
	 *
	 * 조용히 0 으로 깎지 않는다 — 그러면 `전원 배치` 처럼 보여서 버그가 화면에서 사라진다.
	 */
	Inconsistent	UMETA(DisplayName = "값 불일치")
};

/**
 * 인력 현황 한 벌. 화면은 이 값을 그대로 그리고 계산을 하지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveStaffSummary
{
	GENERATED_BODY()

	/** 전체 인원. 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 Total = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 Base = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 Temporary = INDEX_NONE;

	/** 프로그램에 묶인 인원. 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 Assigned = INDEX_NONE;

	/** 놀고 있는 인원. 모르거나 값이 어긋나면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 Idle = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	EFF_LiveStaffIdleState IdleState = EFF_LiveStaffIdleState::Unknown;

	bool IsKnown() const { return IdleState != EFF_LiveStaffIdleState::Unknown; }
};

/**
 * 라이브 `스태프` 패널의 판단 규칙. **순수 함수만 둔다.**
 */
struct FESTAFIESTA_API FFF_LiveStaffRules
{
	/**
	 * 인력 현황을 만든다.
	 *
	 * 🔴 **못 읽은 값을 0 으로 바꾸지 않는다.** 인력 API 는 축제가 시작되기 전과
	 * 소유자 서브시스템이 없을 때 `INDEX_NONE` 을 돌려준다. 그것을 0 으로 치면 화면이
	 * `0명` 이라는 **틀린 사실**을 말한다.
	 */
	static FFF_LiveStaffSummary BuildSummary(
		int32 TotalStaff, int32 BaseStaff, int32 TemporaryStaff, int32 AssignedStaff);
};

/**
 * 라이브 `스태프` 패널.
 *
 * === 🔴 읽기 전용이다 ===
 * 알바 고용은 **셋업·개장 준비 전용**이고 하루짜리다. 라이브 중에 늘릴 수 있게 하면
 * "부족하면 그때 사면 된다" 가 되어 준비 단계의 결정이 의미를 잃는다.
 * 그래서 여기에는 명령 버튼이 없고, **대신 언제 늘릴 수 있는지 적는다** —
 * 못 하는 것을 흐려 놓기만 하면 왜 못 하는지 알 수 없다 (AT-011 과 같은 이유).
 *
 * === 그럼 왜 필요한가 ===
 * **놀고 있는 인원**을 보여주는 것이 이 패널의 값어치다. 임금은 인원 수대로 나가는데
 * 프로그램에 안 묶인 인원은 아무것도 하지 않는다. 그 낭비는 다른 어느 화면에도 안 뜬다.
 *
 * === 없는 것 ===
 * 시설별 배치·개인 상태(피로도·위치)는 소유자 API 가 없다. 화면에 그 사실을 적는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Staff"))
class FESTAFIESTA_API UFF_LivePanelStaff : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Staff")
	FFF_LiveStaffSummary GetSummary() const { return Summary; }

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	UFUNCTION()
	void HandleStaffCountChanged(int32 NewValue);

	UFUNCTION()
	void HandleScheduledProgramsChanged();

protected:
	// --- 요약 3칸 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_TotalStaff;

	/** `기본 6 · 알바 1` 같은 내역. 전체 숫자만으로는 내일 몇 명이 남는지 알 수 없다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_StaffBreakdown;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_AssignedStaff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_IdleStaff;

	/** 남는 인원의 뜻. 숫자만 두면 좋은 것인지 나쁜 것인지 알 수 없다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_IdleNote;

	// --- 오늘 편성 ---

	/** 프로그램 줄이 쌓이는 곳. 비면 통째로 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UVerticalBox> Box_Assignments;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_AssignmentsEmpty;

	// --- 비용과 안내 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_DailyWage;

	/** 라이브 중에는 왜 못 늘리는지. 버튼을 안 두는 대신 이유를 적는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_HireNote;

	/** 아직 소유자 API 가 없어 못 보여주는 것. 안 적으면 빠진 줄 모른다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_MissingNote;

	/** 편성 한 줄을 만드는 데 쓴다. 없으면 목록을 만들지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff")
	TSubclassOf<UUserWidget> AssignmentRowClass;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();
	void RefreshAll();
	void RefreshAssignments();

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	FFF_LiveStaffSummary Summary;
};
