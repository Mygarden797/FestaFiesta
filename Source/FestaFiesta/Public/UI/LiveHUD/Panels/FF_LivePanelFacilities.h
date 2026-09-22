// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_LivePanelFacilities.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * 시설 하나가 얼마나 급한가.
 *
 * !! 색으로만 구분하지 않는다 !! (AT-011) 단계마다 사유 문구가 함께 붙는다.
 */
UENUM(BlueprintType)
enum class EFF_LiveFacilityAlert : uint8
{
	/** 상태를 못 읽었다. **`정상` 이 아니다.** */
	Unknown		UMETA(DisplayName = "모름"),

	/** 문제 없다. */
	Ok			UMETA(DisplayName = "정상"),

	/** 지켜봐야 한다 — 줄이 길거나 정원이 찼다. */
	Watch		UMETA(DisplayName = "주의"),

	/** 손님을 못 받고 있다. 가장 먼저 봐야 한다. */
	Blocked		UMETA(DisplayName = "막힘")
};

/**
 * 시설 한 줄에 필요한 값 한 벌. 화면은 이 값을 그대로 그린다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveFacilityRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FFF_LiveFacilityView View;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	EFF_LiveFacilityAlert Alert = EFF_LiveFacilityAlert::Unknown;
};

/**
 * 라이브 `시설` 패널의 판단 규칙. **순수 함수만 둔다.**
 *
 * === 왜 위젯 밖으로 뺐나 ===
 * "줄이 막 임계를 넘긴 순간" 이나 "재배치가 필요한 시설" 은 라이브 중 특정 시점에만
 * 나타나서 손으로 밟기 어렵다. 여기 있으면 자동화 테스트가 못으로 박는다.
 */
struct FESTAFIESTA_API FFF_LiveFacilityRules
{
	/**
	 * 시설 하나의 급한 정도를 정한다.
	 *
	 * 🔴 **순서가 뜻을 만든다.** 아래 차례로 보고 먼저 걸리는 것을 답으로 낸다 —
	 *   ① 상태를 모름            → `모름`. 정상으로 치지 않는다
	 *   ② 재배치 필요            → `막힘`. 자리부터 고쳐야 나머지가 의미 있다
	 *   ③ 대기열이 막힘          → `막힘`
	 *   ④ 운영 중이 아님         → `막힘`. 라이브인데 안 열려 있으면 손님을 못 받는다
	 *   ⑤ 정원이 참              → `주의`
	 *   ⑥ 대기 팀 수이 임계 이상  → `주의`
	 *   ⑦ 그 외                  → `정상`
	 *
	 * 순서를 바꾸면 **덜 급한 이유가 더 급한 이유를 가린다.** 예를 들어 ⑤ 를 ② 앞에 두면
	 * 자리가 잘못된 시설이 `정원 참` 으로 보여서, 고쳐야 할 것이 무엇인지 알 수 없게 된다.
	 *
	 * @param WaitingThreshold 이 팀 수 이상이면 `주의`. 0 이하면 대기 팀 수을 보지 않는다
	 */
	static EFF_LiveFacilityAlert ResolveAlert(const FFF_LiveFacilityView& View, int32 WaitingThreshold);

	/**
	 * 급한 것이 위로 오게 정렬한다. 같은 급이면 대기 팀 수이 많은 쪽이 위다.
	 *
	 * ★ 이름순이 아니라 **급한 순**이다. 라이브 중에 목록을 훑을 시간이 없다 —
	 * 맨 위 몇 줄만 봐도 손이 가야 할 곳을 알 수 있어야 한다.
	 */
	static void SortByUrgency(TArray<FFF_LiveFacilityRow>& Rows);

	/** 한 줄에 쓸 사유 문구. `정상` 이면 비어 있다. */
	static FText DescribeAlert(const FFF_LiveFacilityRow& Row);
};

/**
 * 라이브 `시설` 패널.
 *
 * === 무엇을 보여주나 ===
 * 시설 목록을 **급한 순으로** 세운다. 라이브 중에 필요한 판단은 "지금 어디가 막혔나"
 * 하나뿐이고, 이름순으로 늘어놓으면 그 답을 사람이 직접 찾아야 한다.
 *
 * === 🔴 읽기 전용이다 ===
 * 시설을 열고 닫고 옮기는 것은 **건축·존 시스템이 소유한다.** UI 가 명령을 보낼 창구가
 * 아직 없어서 여기서는 상태만 알린다. 무엇을 할 수 있는지는 화면에 적는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Facilities"))
class FESTAFIESTA_API UFF_LivePanelFacilities : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Facilities")
	TArray<FFF_LiveFacilityRow> GetRows() const { return Rows; }

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	/** 라이브 중에는 값이 계속 바뀐다. 이벤트가 없어 주기적으로 다시 읽는다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// --- 요약 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_FacilityCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_Summary;

	// --- 목록 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UVerticalBox> Box_Facilities;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_MissingNote;

	// --- 설정 ---

	/**
	 * 이 팀 수 이상 줄이 서면 `주의` 로 본다.
	 *
	 * `assumption` 이다 — 기획에서 확정된 값이 아니라 화면을 보고 정한 시작점이다.
	 * 확정되면 데이터로 뺀다 (AGENTS.md 작업 관례).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities", meta = (ClampMin = "0"))
	int32 WaitingThreshold = 8;

	/**
	 * 목록을 다시 읽는 간격(초).
	 *
	 * 🔴 매 틱 읽지 않는다. `BuildReadModel()` 은 시설 전부를 훑어 배열을 새로 만드는
	 * 함수라 60fps 로 부르면 라이브 중에 그 비용이 그대로 쌓인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities", meta = (ClampMin = "0.1"))
	float RefreshIntervalSeconds = 1.f;

	/** 한 번에 보여줄 최대 줄 수. 넘치면 아래에 몇 개가 더 있는지 적는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities", meta = (ClampMin = "1"))
	int32 MaxRows = 8;

private:
	void RefreshAll();

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	TArray<FFF_LiveFacilityRow> Rows;

	float TimeSinceRefresh = 0.f;
};
