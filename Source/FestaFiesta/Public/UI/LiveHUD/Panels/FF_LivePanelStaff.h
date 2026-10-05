// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelStaff.generated.h"

class UFF_FestivalViewModelSubsystem;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UProgressBar;
class UImage;
class UTexture2D;

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

	/** 출동하지 않은 인원. 실제 출동 가능 여부는 시설 배정 여부도 확인한다. */
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
	int32 Hired = INDEX_NONE;

	/** 이동·사건 대응·복귀 중인 인원. 시설 소속 인원과 별개다. 모르면 INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 Assigned = INDEX_NONE;

	/** 출동하지 않은 인원(미배정 포함). 모르거나 값이 어긋나면 INDEX_NONE. */
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
		int32 TotalStaff, int32 BaseStaff, int32 HiredStaff, int32 AssignedStaff);
};

/**
 * 라이브 `스태프` 패널.
 *
 * === 🔴 읽기 전용이다 ===
 * 인원 고용은 **셋업·개장 준비 전용**이고 축제 끝까지 남는다. 라이브 중에 늘릴 수 있게 하면
 * "부족하면 그때 사면 된다" 가 되어 준비 단계의 결정이 의미를 잃는다.
 * 그래서 여기에는 명령 버튼이 없고, **대신 언제 늘릴 수 있는지 적는다** —
 * 못 하는 것을 흐려 놓기만 하면 왜 못 하는지 알 수 없다 (AT-011 과 같은 이유).
 *
 * === 그럼 왜 필요한가 ===
 * **사건에 대응 중인 인원과 남는 인원**을 보여주는 것이 이 패널의 값어치다.
 * 사건이 몰릴 때 사람이 모자란지가 여기서만 보인다.
 *
 * [주의] 유료 프로그램은 인원을 쓰지 않는다. 배치는 사건 대응 전용이다.
 *
 * 시설별 배정은 안내 시설 정보 패널에서 조절한다. 이 패널은 보유·정원·미배정·출동 인원을 읽는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Staff"))
class FESTAFIESTA_API UFF_LivePanelStaff : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Staff")
	FFF_LiveStaffSummary GetSummary() const { return Summary; }

	// 동적 문구만 클래스 기본값에서 편집한다. 크기·글꼴·색·막대 스타일은 Designer 소유다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaLiveStaff", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text", meta = (ToolTip = "{Count}: 인원"))
	FText CountTextFormat = NSLOCTEXT("FestaFiestaLiveStaff", "CountFormat", "{Count}명");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text", meta = (ToolTip = "{Total}: 보유 인원, {Capacity}: 정원. 모르는 값은 —"))
	FText CapacityTextFormat = NSLOCTEXT("FestaFiestaLiveStaff", "CapacityFormat", "{Total} / {Capacity}명");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text", meta = (ToolTip = "{Base}: 기본 인원, {Hired}: 고용 인원"))
	FText BreakdownTextFormat = NSLOCTEXT("FestaFiestaLiveStaff", "BreakdownFormat", "기본 {Base} · 고용 {Hired}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText IdleNoteText = NSLOCTEXT("FestaFiestaLiveStaff", "IdleNote", "미배정 인원도 포함합니다. 안내 시설에 배정된 대기 인원만 출동할 수 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText AllBusyNoteText = NSLOCTEXT("FestaFiestaLiveStaff", "AllBusyNoteAccurate", "전원이 출동 중입니다. 이동·사건 대응·복귀 인원을 포함합니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText NoStaffNoteText = NSLOCTEXT("FestaFiestaLiveStaff", "NoStaffNote", "현재 보유한 스태프가 없습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText CheckingText = NSLOCTEXT("FestaFiestaLiveStaff", "Checking", "스태프 정보를 확인하고 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text", meta = (ToolTip = "{Busy}: 출동 인원, {Total}: 전체 인원"))
	FText InconsistentTextFormat = NSLOCTEXT("FestaFiestaLiveStaff", "InconsistentFormat", "출동 인원({Busy})이 전체 인원({Total})보다 많습니다. 인원 정보를 확인해야 합니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text", meta = (ToolTip = "{Count}: 이동·대응·복귀 인원"))
	FText ResponseActiveTextFormat = NSLOCTEXT("FestaFiestaLiveStaff", "ResponseActiveFormat", "{Count}명이 이동·사건 대응·복귀 중입니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText ResponseIdleText = NSLOCTEXT("FestaFiestaLiveStaff", "ResponseIdle", "현재 이동·사건 대응·복귀 중인 스태프가 없습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText ResponseUnknownText = NSLOCTEXT("FestaFiestaLiveStaff", "ResponseUnknown", "출동 정보를 확인하고 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text", meta = (ToolTip = "{Cost}: 1회 고용 단가"))
	FText HireCostTextFormat = NSLOCTEXT("FestaFiestaLiveStaff", "HireCostFormat", "1명 고용비 {Cost}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText HireNoteText = NSLOCTEXT("FestaFiestaLiveStaff", "HireNote", "고용은 셋업과 개장 준비에서만 가능합니다. 라이브 중에는 늘릴 수 없습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText AssignmentGuideText = NSLOCTEXT("FestaFiestaLiveStaff", "AssignmentGuide", "안내 시설을 선택해 스태프를 배정하세요. 배정된 시설에서 사건 현장으로 출동합니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text", meta = (ToolTip = "{Count}: 미배정 인원"))
	FText UnassignedGuideTextFormat = NSLOCTEXT("FestaFiestaLiveStaff", "UnassignedGuideFormat", "미배정 {Count}명 · 안내 시설을 선택해 스태프를 배정하세요.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText CapacityNoteText = NSLOCTEXT("FestaFiestaLiveStaff", "CapacityNote", "막대는 정원 대비 보유 인원입니다. 미배정은 보유 인원에 포함됩니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText CapacityUnknownText = NSLOCTEXT("FestaFiestaLiveStaff", "CapacityUnknown", "보유 인원과 정원을 확인하고 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText BreakdownUnknownText = NSLOCTEXT("FestaFiestaLiveStaff", "BreakdownUnknown", "기본·고용 내역을 확인하고 있습니다. 막대는 전체 보유 인원만 표시합니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText BreakdownInconsistentText = NSLOCTEXT("FestaFiestaLiveStaff", "BreakdownInconsistent", "기본·고용 합계와 전체 인원이 다릅니다. 막대는 전체 보유 인원만 표시합니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Text")
	FText CapacityInconsistentText = NSLOCTEXT("FestaFiestaLiveStaff", "CapacityInconsistent", "보유 인원이 정원보다 많아 막대를 표시할 수 없습니다. 인원 정보를 확인해야 합니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Icons")
	TObjectPtr<UTexture2D> ResponseIdleIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Icons")
	TObjectPtr<UTexture2D> ResponseActiveIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff|Icons")
	TObjectPtr<UTexture2D> ResponseUnknownIcon;

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleStaffCountChanged(int32 NewValue);

	UFUNCTION()
	void HandleStaffAssignmentChanged();

protected:
	// --- 요약 3칸 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_TotalStaff;

	/** `기본 6 · 고용 1` 같은 내역. 전체 숫자만으로는 내일 몇 명이 남는지 알 수 없다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_StaffBreakdown;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_AssignedStaff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_IdleStaff;

	/** 남는 인원의 뜻. 숫자만 두면 좋은 것인지 나쁜 것인지 알 수 없다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_IdleNote;

	// --- 인원 구성과 정원. 숫자·비율만 갱신하고 Designer 스타일을 유지한다. ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_Capacity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_BaseStaff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_HiredStaff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_UnassignedStaff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_CapacityNote;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UProgressBar> Progress_StaffCapacity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UProgressBar> Progress_BaseCapacity;

	// --- 사건 대응 ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UImage> Image_ResponseStatus;

	/** 대응 중인 사건 줄이 쌓이는 곳. 비면 통째로 숨긴다. 사건 시스템이 오면 채워진다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UVerticalBox> Box_Assignments;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_AssignmentsEmpty;

	// --- 비용과 안내 ---

	/**
	 * 1명당 고용비. 1회 구매라 하루치가 아니다.
	 * [주의] 위젯 이름이 `DailyWage` 인 것은 일당 알바 시절의 잔재다. 바꾸려면 WBP 를
	 *        다시 구워야 해서 이름만 남겨 뒀다 — 뜻은 이 주석이 가진다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_DailyWage;

	/** 라이브 중에는 왜 못 늘리는지. 버튼을 안 두는 대신 이유를 적는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_HireNote;

	/** 아직 소유자 API 가 없어 못 보여주는 것. 안 적으면 빠진 줄 모른다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Staff")
	TObjectPtr<UTextBlock> Text_MissingNote;

	/** 대응 중인 사건 한 줄을 만드는 데 쓴다. 없으면 목록을 만들지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Staff")
	TSubclassOf<UUserWidget> AssignmentRowClass;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();
	void RefreshAll();
	void RefreshAssignments();
	void RefreshCapacity(int32 Capacity);
	FText FormatCount(int32 Count) const;

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	FFF_LiveStaffSummary Summary;
};
