// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Engine/GameInstance.h"
#include "FF_FFZSWorldSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "UI/FF_UILog.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepCheck"

// =============================================================================
// FFF_PrepCheckRules — 순수 함수
// =============================================================================

EFF_PrepCapacityRisk FFF_PrepCheckRules::ClassifyCapacityRisk(
	int32 ExpectedVisitors,
	int32 SoftCapacity,
	float CautionRatio,
	float DangerRatio)
{
	// ★ 모르면 `여유` 가 아니라 `판정 불가` 다.
	// 모르는 것을 안전하다고 알리면 플레이어가 그걸 믿고 개장한다.
	if (ExpectedVisitors == INDEX_NONE || SoftCapacity == INDEX_NONE || SoftCapacity <= 0)
	{
		return EFF_PrepCapacityRisk::Unknown;
	}

	const float Ratio = static_cast<float>(ExpectedVisitors) / static_cast<float>(SoftCapacity);

	// 위험을 먼저 본다. 두 경계가 뒤집혀 설정돼도 더 심한 판정이 살아남는다.
	if (Ratio >= DangerRatio)
	{
		return EFF_PrepCapacityRisk::Danger;
	}
	if (Ratio >= CautionRatio)
	{
		return EFF_PrepCapacityRisk::Caution;
	}
	return EFF_PrepCapacityRisk::Comfortable;
}

FFF_PrepDayEstimate FFF_PrepCheckRules::BuildDayEstimate(
	int32 RentCost,
	int32 TemporaryStaffCount,
	int32 WagePerStaff,
	int32 ExpectedVisitors,
	int32 AdmissionFee,
	int32 DailyVisitorGoal,
	int32 DailyGoalBonus)
{
	FFF_PrepDayEstimate Result;
	Result.RentCost = RentCost;

	if (TemporaryStaffCount >= 0 && WagePerStaff != INDEX_NONE)
	{
		Result.StaffWageCost = TemporaryStaffCount * WagePerStaff;
	}

	// 둘 중 하나라도 모르면 합계를 만들지 않는다. 절반만 더한 값은 틀린 사실이다.
	if (Result.RentCost != INDEX_NONE && Result.StaffWageCost != INDEX_NONE)
	{
		Result.FixedCostTotal = Result.RentCost + Result.StaffWageCost;
	}

	if (ExpectedVisitors != INDEX_NONE && AdmissionFee != INDEX_NONE)
	{
		Result.ExpectedAdmissionRevenue = ExpectedVisitors * AdmissionFee;
	}

	// 목표 보너스의 0 은 `모름` 이 아니라 **못 받는다**는 확정 정보다.
	// 그래서 예상 방문객과 목표를 둘 다 아는 경우에만 값을 채운다.
	if (ExpectedVisitors != INDEX_NONE && DailyVisitorGoal != INDEX_NONE && DailyGoalBonus != INDEX_NONE)
	{
		Result.ExpectedGoalBonus = (ExpectedVisitors >= DailyVisitorGoal) ? DailyGoalBonus : 0;
	}
	return Result;
}

// =============================================================================
// 수명
// =============================================================================

UFF_PrepPanelCheck::UFF_PrepPanelCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// `EFF_WeatherType` 에 `UMETA(DisplayName)` 이 없어서 표시 문구를 UI 가 갖는다.
	// 자세한 이유는 `WeatherLabels` 선언 주석에.
	WeatherLabels.Add(EFF_WeatherType::Clear, LOCTEXT("PrepCheck_WeatherClear", "맑음"));
	WeatherLabels.Add(EFF_WeatherType::Rain, LOCTEXT("PrepCheck_WeatherRain", "비"));
	WeatherLabels.Add(EFF_WeatherType::Windy, LOCTEXT("PrepCheck_WeatherWindy", "바람"));
}

void UFF_PrepPanelCheck::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (EstimateNoticeText.IsEmpty())
	{
		EstimateNoticeText = LOCTEXT("PrepCheck_EstimateNotice",
			"부스 수수료와 미니게임 보상은 운영 결과에 따라 달라지므로 이 표에 넣지 않았습니다.");
	}

	if (Text_EstimateNotice)
	{
		Text_EstimateNotice->SetText(EstimateNoticeText);
	}
}

void UFF_PrepPanelCheck::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	CollectWeatherChoices();

	if (Button_ContinueBuild)
	{
		Button_ContinueBuild->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleContinueBuildClicked);
	}
	if (Button_OpenGate)
	{
		Button_OpenGate->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleOpenGateClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Button_OpenGate 가 없다. 하단 바의 CTA 로만 개장할 수 있다."), *GetName());
	}
}

void UFF_PrepPanelCheck::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	BindViewModel();
	RefreshAll();
}

void UFF_PrepPanelCheck::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

// =============================================================================
// 구독
// =============================================================================

UFF_FestivalViewModelSubsystem* UFF_PrepPanelCheck::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_PrepPanelCheck::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 견적서가 전부 비어 보인다."), *GetName());
		return;
	}

	CachedViewModel->OnWeatherChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleWeatherChanged);
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleGoldChanged);
	CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleStaffCountChanged);
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleStageViewChanged);

	// 프로그램 편성은 전체 인원을 바꾸지 않으므로 위 구독만으로는 `사용 가능` 이 안 바뀐다.
	CachedViewModel->OnAvailableStaffCountChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleStaffCountChanged);

	// 피날레는 다른 패널에서 산다. 이 요약 줄이 갱신되려면 결과 이벤트를 받아야 한다.
	CachedViewModel->OnPurchasedFinaleGradeChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandlePurchasedFinaleGradeChanged);
}

void UFF_PrepPanelCheck::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnWeatherChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleWeatherChanged);
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleGoldChanged);
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleStaffCountChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleStageViewChanged);
		CachedViewModel->OnAvailableStaffCountChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleStaffCountChanged);
		CachedViewModel->OnPurchasedFinaleGradeChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandlePurchasedFinaleGradeChanged);
	}
	CachedViewModel = nullptr;
}

// =============================================================================
// 날씨
// =============================================================================

FText UFF_PrepPanelCheck::GetWeatherLabel(EFF_WeatherType Weather) const
{
	if (const FText* Label = WeatherLabels.Find(Weather))
	{
		if (!Label->IsEmpty())
		{
			return *Label;
		}
	}

	// 표에 빠진 값이 있으면 조용히 빈칸으로 두지 않는다. enum 이름이라도 띄우고 로그를 남긴다.
	const UEnum* WeatherEnum = StaticEnum<EFF_WeatherType>();
	const FString Fallback = WeatherEnum
		? WeatherEnum->GetNameStringByIndex(WeatherEnum->GetIndexByValue(static_cast<int64>(Weather)))
		: TEXT("?");

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] `%s` 날씨의 표시 문구가 WeatherLabels 에 없다."), *GetName(), *Fallback);
	return FText::FromString(Fallback);
}

void UFF_PrepPanelCheck::CollectWeatherChoices()
{
	WeatherChoices.Reset();

	// 필드와 enum 값을 한 곳에서 짝짓는다. 날씨가 늘면 이 표에 한 줄만 추가한다.
	const TPair<EFF_WeatherType, UFF_PrepChoiceButton*> Bindings[] = {
		{ EFF_WeatherType::Clear, Choice_Clear },
		{ EFF_WeatherType::Rain,  Choice_Rain },
		{ EFF_WeatherType::Windy, Choice_Windy }
	};

	const UEnum* WeatherEnum = StaticEnum<EFF_WeatherType>();
	TArray<FString> Missing;

	for (const TPair<EFF_WeatherType, UFF_PrepChoiceButton*>& Binding : Bindings)
	{
		const int32 Index = WeatherEnum
			? WeatherEnum->GetIndexByValue(static_cast<int64>(Binding.Key))
			: INDEX_NONE;

		if (!Binding.Value)
		{
			Missing.Add(WeatherEnum ? WeatherEnum->GetNameStringByIndex(Index) : TEXT("?"));
			continue;
		}

		// 문구는 코드가 채운다. WBP 에 박으면 enum 과 화면이 갈라진다.
		// `GetDisplayNameTextByIndex` 를 쓰지 않는 이유는 `WeatherLabels` 주석에.
		Binding.Value->ConfigureChoice(static_cast<int32>(Binding.Key), GetWeatherLabel(Binding.Key));

		Binding.Value->OnHudTabButtonClicked.BindUObject(this, &UFF_PrepPanelCheck::HandleWeatherChoiceClicked);
		WeatherChoices.Add(Binding.Value);
	}

	if (!Missing.IsEmpty())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] WBP 에 없는 날씨 선택지 %d개: %s — 위젯 이름이 Choice_<enum 이름>과 같아야 한다."),
			*GetName(), Missing.Num(), *FString::Join(Missing, TEXT(", ")));
	}
}

void UFF_PrepPanelCheck::HandleWeatherChoiceClicked(int32 ChoiceId)
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// !! 낙관적 갱신을 하지 않는다 !!
	// 허용 판정은 Stage 시스템이 갖는다. 화면은 `OnWeatherChanged` 결과 이벤트로만 바뀐다.
	ViewModel->RequestSetWeather(static_cast<EFF_WeatherType>(ChoiceId));

	// 거부되면 이벤트가 오지 않으므로 여기서 한 번 다시 그린다 — 그러지 않으면
	// 눌렀는데 아무 반응이 없는 화면이 된다.
	RefreshWeather();
}

void UFF_PrepPanelCheck::RefreshWeather()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const EFF_WeatherType Current = ViewModel ? ViewModel->GetSelectedWeather() : EFF_WeatherType::Clear;
	const bool bCanChange = ViewModel ? ViewModel->CanChangeWeather() : false;

	SetTextOrHide(Text_Weather, GetWeatherLabel(Current));

	for (UFF_PrepChoiceButton* Choice : WeatherChoices)
	{
		if (!Choice)
		{
			continue;
		}

		// 색만 바꾸지 않고 배경 브러시가 함께 바뀐다 (AT-011). 베이스가 처리한다.
		Choice->SetSelected(Choice->GetChoiceId() == static_cast<int32>(Current));
		Choice->SetIsEnabled(bCanChange);
	}

	// 못 바꾸는 이유를 적는다. 흐려짐만으로는 알 수 없다.
	SetTextOrHide(Text_WeatherLockReason, bCanChange
		? FText::GetEmpty()
		: LOCTEXT("PrepCheck_WeatherLocked", "지금 단계에서는 날씨를 바꿀 수 없습니다."));
}

// =============================================================================
// 갱신
// =============================================================================

void UFF_PrepPanelCheck::RefreshAll()
{
	RefreshWeather();
	RefreshCapacity();
	RefreshSummary();
	RefreshEstimate();

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const int32 Gold = ViewModel ? ViewModel->GetGold() : INDEX_NONE;
	SetTextOrHide(Text_Gold, FormatGoldOrUnknown(Gold));

	const int32 Goal = ViewModel ? ViewModel->GetDailyVisitorGoal() : INDEX_NONE;
	const int32 Bonus = ViewModel ? ViewModel->GetDailyGoalBonusGold() : INDEX_NONE;
	SetTextOrHide(Text_DailyGoal, (Goal != INDEX_NONE && Bonus != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepCheck_DailyGoal", "{0}명 · 달성 시 +{1}"),
			FText::AsNumber(Goal), FText::AsNumber(Bonus))
		: LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다"));

	const bool bHasRange = ExpectedVisitorsMin != INDEX_NONE
		&& ExpectedVisitorsMax >= ExpectedVisitorsMin;
	SetTextOrHide(Text_ExpectedVisitors, bHasRange
		? FText::Format(
			LOCTEXT("PrepCheck_Expected", "{0}~{1}명"),
			FText::AsNumber(ExpectedVisitorsMin), FText::AsNumber(ExpectedVisitorsMax))
		: LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다"));
}

void UFF_PrepPanelCheck::RefreshCapacity()
{
	const int32 SoftCapacity = GetTotalSoftCapacity();

	// 위험 판정은 **범위의 위쪽**으로 한다. 평균으로 판정하면 최악의 날에 정원을 넘긴다.
	const int32 WorstCaseVisitors = ExpectedVisitorsMax;

	const EFF_PrepCapacityRisk Risk = FFF_PrepCheckRules::ClassifyCapacityRisk(
		WorstCaseVisitors, SoftCapacity, CapacityCautionRatio, CapacityDangerRatio);

	// !! 색만으로 구분하지 않는다 !! (AT-011) 아이콘 문자와 문구를 함께 넣는다.
	FText RiskText;
	switch (Risk)
	{
	case EFF_PrepCapacityRisk::Comfortable:
		RiskText = LOCTEXT("PrepCheck_RiskComfortable", "○ 여유");
		break;
	case EFF_PrepCapacityRisk::Caution:
		RiskText = LOCTEXT("PrepCheck_RiskCaution", "△ 주의 — 정원에 가깝습니다");
		break;
	case EFF_PrepCapacityRisk::Danger:
		RiskText = LOCTEXT("PrepCheck_RiskDanger", "✕ 위험 — 정원을 넘습니다");
		break;
	default:
		RiskText = LOCTEXT("PrepCheck_RiskUnknown", "? 판정 불가 — 예상 방문객이나 정원을 아직 읽지 못했습니다");
		break;
	}
	SetTextOrHide(Text_CapacityRisk, RiskText);

	// 판정의 근거를 함께 보여준다. 결론만 있으면 왜 그런지 알 수 없다.
	FText Detail;
	if (SoftCapacity == INDEX_NONE)
	{
		Detail = LOCTEXT("PrepCheck_CapacityNoZone", "Zone 정원을 읽지 못했습니다. 시설을 놓으면 계산됩니다.");
	}
	else if (WorstCaseVisitors == INDEX_NONE)
	{
		Detail = FText::Format(
			LOCTEXT("PrepCheck_CapacityNoVisitors", "정원 {0}명 · 예상 방문객 미정"),
			FText::AsNumber(SoftCapacity));
	}
	else
	{
		Detail = FText::Format(
			LOCTEXT("PrepCheck_CapacityDetail", "정원 {0}명 대비 최대 예상 {1}명"),
			FText::AsNumber(SoftCapacity), FText::AsNumber(WorstCaseVisitors));
	}
	SetTextOrHide(Text_CapacityDetail, Detail);
}

void UFF_PrepPanelCheck::RefreshSummary()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();

	const int32 Total = ViewModel ? ViewModel->GetTotalStaffCount() : INDEX_NONE;
	const int32 Base = ViewModel ? ViewModel->GetBaseStaffCount() : INDEX_NONE;
	const int32 Temporary = ViewModel ? ViewModel->GetTemporaryStaffCount() : INDEX_NONE;
	const int32 Available = ViewModel ? ViewModel->GetAvailableStaffCount() : INDEX_NONE;

	// 개장 직전 요약이라 **Event 에 쓸 수 있는 인원**이 전체보다 중요하다.
	// 전체만 띄우면 프로그램에 묶인 인원까지 쓸 수 있다고 읽힌다.
	SetTextOrHide(Text_StaffSummary, (Total >= 0 && Base >= 0 && Temporary >= 0)
		? (Available >= 0
			? FText::Format(
				LOCTEXT("PrepCheck_StaffWithAvailable", "{0}명 (기본 {1} · 알바 {2}) · {3}명 사용 가능"),
				FText::AsNumber(Total), FText::AsNumber(Base), FText::AsNumber(Temporary), FText::AsNumber(Available))
			: FText::Format(
				LOCTEXT("PrepCheck_Staff", "{0}명 (기본 {1} · 알바 {2})"),
				FText::AsNumber(Total), FText::AsNumber(Base), FText::AsNumber(Temporary)))
		: LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다"));

	const int32 ProgramCount = GetProgramCount();
	SetTextOrHide(Text_ProgramSummary, (ProgramCount != INDEX_NONE)
		? FText::Format(LOCTEXT("PrepCheck_Program", "{0}개 편성"), FText::AsNumber(ProgramCount))
		: LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다"));

	// 피날레는 마지막 날에만 의미가 있다. 그 전에는 줄째 숨긴다 —
	// `미구매` 를 계속 띄우면 살 수 있는데 안 산 것처럼 읽힌다.
	const bool bIsFinalDay = ViewModel ? ViewModel->IsFinalDay() : false;
	if (!bIsFinalDay)
	{
		SetGroupTextOrHide(Box_FinaleSummaryRow, Text_FinaleSummary, FText::GetEmpty());
		return;
	}

	const EFF_FinaleGrade Grade = ViewModel ? ViewModel->GetPurchasedFinaleGrade() : EFF_FinaleGrade::None;
	const int32 Bonus = ViewModel ? ViewModel->GetFinaleSatisfactionBonus() : 0;

	// 🔴 **enum 의 `DisplayName` 을 직접 읽지 않는다.** 패키징 빌드에서 영문이 뜬다
	// (2026-08-19 김시리 정정). 표시 문구는 `UFF_EconomyDisplayTextLibrary` 가 소유한다.
	SetGroupTextOrHide(Box_FinaleSummaryRow, Text_FinaleSummary,
		(Grade == EFF_FinaleGrade::None)
			? LOCTEXT("PrepCheck_FinaleNone", "미구매 — 최소 연출만 실행됩니다")
			: FText::Format(
				LOCTEXT("PrepCheck_FinalePurchased", "{0} 등급 · 만족도 +{1}"),
				UFF_EconomyDisplayTextLibrary::GetFinaleGradeText(Grade),
				FText::AsNumber(Bonus)));
}

void UFF_PrepPanelCheck::RefreshEstimate()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();

	const FFF_PrepDayEstimate Estimate = FFF_PrepCheckRules::BuildDayEstimate(
		ViewModel ? ViewModel->GetDailyVenueRentGold() : INDEX_NONE,
		ViewModel ? ViewModel->GetTemporaryStaffCount() : INDEX_NONE,
		ViewModel ? ViewModel->GetDailyStaffWageGold() : INDEX_NONE,
		// 수입은 **범위의 아래쪽**으로 잡는다. 위쪽으로 잡으면 개장 판단이 낙관적으로 기운다.
		ExpectedVisitorsMin,
		ViewModel ? ViewModel->GetAdmissionFeeGold() : INDEX_NONE,
		ViewModel ? ViewModel->GetDailyVisitorGoal() : INDEX_NONE,
		ViewModel ? ViewModel->GetDailyGoalBonusGold() : INDEX_NONE);

	SetTextOrHide(Text_RentCost, FormatGoldOrUnknown(Estimate.RentCost));
	SetTextOrHide(Text_StaffWageCost, FormatGoldOrUnknown(Estimate.StaffWageCost));
	SetTextOrHide(Text_FixedCostTotal, FormatGoldOrUnknown(Estimate.FixedCostTotal));
	SetTextOrHide(Text_ExpectedAdmission, FormatGoldOrUnknown(Estimate.ExpectedAdmissionRevenue));
	SetTextOrHide(Text_ExpectedGoalBonus, FormatGoldOrUnknown(Estimate.ExpectedGoalBonus));
}

// =============================================================================
// 외부 값 조회
// =============================================================================

int32 UFF_PrepPanelCheck::GetTotalSoftCapacity() const
{
	// !! 못 읽었을 때 0 으로 두지 않는다 !!
	// 0 이면 `ClassifyCapacityRisk` 가 `판정 불가` 를 돌려주므로 결과는 같지만,
	// 근거 문구가 "정원 0명" 이 되어 틀린 사실을 적게 된다.
	const UWorld* World = GetWorld();
	if (!World)
	{
		return INDEX_NONE;
	}

	const UFFZSWorldSubsystem* Zone = World->GetSubsystem<UFFZSWorldSubsystem>();
	if (!Zone)
	{
		return INDEX_NONE;
	}

	const FFFZSReadModel ReadModel = Zone->BuildReadModel();
	if (ReadModel.Zones.IsEmpty())
	{
		return INDEX_NONE;
	}

	int32 Total = 0;
	for (const FFFZSZoneReadModel& ZoneModel : ReadModel.Zones)
	{
		Total += ZoneModel.SoftCapacity;
	}
	return Total;
}

int32 UFF_PrepPanelCheck::GetProgramCount() const
{
	const UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return INDEX_NONE;
	}

	const UFF_ProgramReservationSubsystem* Reservations =
		GI->GetSubsystem<UFF_ProgramReservationSubsystem>();
	if (!Reservations)
	{
		return INDEX_NONE;
	}

	// 구매 화면이 아직 없어서 **0 개인 것이 정상**이다 (2026-08-18 김시리 안내).
	// 그래서 0 을 `모름` 으로 바꾸지 않는다 — 여기서 0 은 확정 정보다.
	return Reservations->GetReservationIds().Num();
}

// =============================================================================
// 버튼 · 결과 이벤트
// =============================================================================

void UFF_PrepPanelCheck::HandleContinueBuildClicked()
{
	// 이 패널을 닫고 배치 탭을 연다. **패널이 스스로 숨지 않는다** — 바에 요청한다.
	if (UFF_PrepHudBar* Bar = GetOwningBar())
	{
		Bar->SelectTab(EFF_PrepHudTab::Build);
		return;
	}

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 소유 바가 없어 배치 탭으로 갈 수 없다."), *GetName());
}

void UFF_PrepPanelCheck::HandleOpenGateClicked()
{
	// 개장은 하단 바의 CTA 와 **같은 요청**이다. 여기서 Stage 를 직접 바꾸지 않는다 —
	// 두 곳이 각자 판정하면 한쪽이 막힌 상태에서 다른 쪽이 통과하는 화면이 된다.
	if (UFF_PrepHudBar* Bar = GetOwningBar())
	{
		Bar->RequestStageAdvance();
		return;
	}

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 소유 바가 없어 개장을 요청할 수 없다."), *GetName());
}

void UFF_PrepPanelCheck::HandleWeatherChanged(EFF_WeatherType NewWeather)
{
	RefreshWeather();
}

void UFF_PrepPanelCheck::HandleGoldChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleStaffCountChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_PrepPanelCheck::HandlePurchasedFinaleGradeChanged(EFF_FinaleGrade NewGrade)
{
	// 등급뿐 아니라 잔액·견적도 함께 바뀐다. 요약 한 줄만 고치지 않고 전체를 다시 그린다.
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	// 단계가 바뀌면 날씨 잠금과 피날레 요약이 함께 바뀐다.
	RefreshAll();
}

void UFF_PrepPanelCheck::Debug_SetExpectedVisitors(int32 InMin, int32 InMax)
{
	ExpectedVisitorsMin = InMin;
	ExpectedVisitorsMax = InMax;
	RefreshAll();
}

// =============================================================================
// 표시 헬퍼
// =============================================================================

void UFF_PrepPanelCheck::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}

	TextBlock->SetText(Value);
	TextBlock->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

void UFF_PrepPanelCheck::SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value)
{
	SetTextOrHide(TextBlock, Value);

	if (!Group)
	{
		return;
	}

	Group->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

FText UFF_PrepPanelCheck::FormatGoldOrUnknown(int32 Amount)
{
	// 개장 판단의 근거가 되는 화면이라 빈칸으로 두지 않는다.
	// 빈칸은 "문제 없음"으로 읽히고, 0 은 "공짜"로 읽힌다. 둘 다 사실이 아니다.
	return (Amount == INDEX_NONE)
		? LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다")
		: FText::AsNumber(Amount);
}

#undef LOCTEXT_NAMESPACE
