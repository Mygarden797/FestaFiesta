// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"

#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Engine/GameInstance.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"
#include "UI/FF_UILog.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepRecommendedFacilityRow.h"
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
	int32 TodayStaffHireCost,
	int32 ExpectedVisitors,
	int32 ExpectedAdmissionRevenue,
	int32 DailyVisitorGoal,
	int32 DailyGoalBonus)
{
	FFF_PrepDayEstimate Result;

	if (TodayStaffHireCost >= 0)
	{
		// 영구 고용 인원이나 현재 단가로 과거 지출을 다시 계산하지 않는다.
		Result.StaffHireCost = TodayStaffHireCost;
	}

	if (ExpectedVisitors >= 0 && ExpectedAdmissionRevenue >= 0)
	{
		Result.ExpectedAdmissionRevenue = ExpectedAdmissionRevenue;
	}

	// 목표 보너스의 0 은 `모름` 이 아니라 **못 받는다**는 확정 정보다.
	// 그래서 예상 방문객과 목표를 둘 다 아는 경우에만 값을 채운다.
	if (ExpectedVisitors != INDEX_NONE && DailyVisitorGoal != INDEX_NONE && DailyGoalBonus != INDEX_NONE)
	{
		Result.ExpectedGoalBonus = (ExpectedVisitors >= DailyVisitorGoal) ? DailyGoalBonus : 0;
	}
	return Result;
}

FText FFF_PrepCheckRules::FormatRequiredFacilities(const FFF_RequiredFacilitiesStatus& Status,
	const FText& RowFormat, const FText& NoRequirementsText)
{
	if (Status.Facilities.IsEmpty())
	{
		return Status.bCanProceed ? NoRequirementsText
			: (Status.BlockingReason.IsEmpty()
				? LOCTEXT("PrepCheck_RequiredUnknown", "필수 시설 상태를 확인할 수 없습니다.")
				: Status.BlockingReason);
	}

	TArray<FText> Lines;
	for (const FFF_RequiredFacilityRow& Row : Status.Facilities)
	{
		const FText Name = !Row.DisplayName.IsEmpty() ? Row.DisplayName
			: (!Row.FacilityDefinitionId.IsNone() ? FText::FromName(Row.FacilityDefinitionId)
				: LOCTEXT("PrepCheck_RequiredUnnamed", "필수 시설"));
		if (!Row.bDefinitionValid)
		{
			Lines.Add(FText::Format(LOCTEXT("PrepCheck_RequiredInvalid", "! {0} · 설정 확인 필요\n{1}"),
				Name, Row.Guidance));
			continue;
		}
		// 배정까지 요구하는 행은 설치 → 배정 순서로 무엇이 남았는지 한 줄에 적는다.
		// 판정은 하지 않는다. 소유자가 준 부족 개수(MissingCount·MissingStaffCount)를 그대로 읽는다.
		FText State;
		if (Row.MissingCount > 0)
		{
			State = Row.RequiresStaffAssignment()
				? FText::Format(LOCTEXT("PrepCheck_RequiredMissingThenStaff", "! 추가 설치 필요 · 설치 후 스태프 {0}명 배정"),
					FText::AsNumber(Row.RequiredStaffCount))
				: LOCTEXT("PrepCheck_RequiredMissing", "! 추가 설치 필요");
		}
		else if (Row.MissingStaffCount > 0)
		{
			State = FText::Format(LOCTEXT("PrepCheck_RequiredStaffMissing", "! 스태프 배정 필요 (배정 {0}/{1}명)"),
				FText::AsNumber(Row.AssignedStaffCount), FText::AsNumber(Row.RequiredStaffCount));
		}
		else
		{
			State = Row.RequiresStaffAssignment()
				? FText::Format(LOCTEXT("PrepCheck_RequiredStaffSatisfied", "✓ 설치·배정 완료 (스태프 {0}명)"),
					FText::AsNumber(Row.AssignedStaffCount))
				: LOCTEXT("PrepCheck_RequiredSatisfied", "✓ 설치 완료");
		}
		FFormatNamedArguments Args;
		Args.Add(TEXT("Name"), Name);
		Args.Add(TEXT("Installed"), FText::AsNumber(Row.InstalledCount));
		Args.Add(TEXT("Required"), FText::AsNumber(Row.MinimumCount));
		Args.Add(TEXT("State"), State);
		// 문구를 바꾸는 WBP 가 배정 인원을 따로 놓을 수 있게 같이 넘긴다.
		Args.Add(TEXT("AssignedStaff"), FText::AsNumber(Row.AssignedStaffCount));
		Args.Add(TEXT("RequiredStaff"), FText::AsNumber(Row.RequiredStaffCount));
		Lines.Add(FText::Format(RowFormat, Args));
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
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
	DayLabelFormat = LOCTEXT("PrepCheck_DayLabelFormat", "{Day}일차");
	DailyGoalValueFormat = LOCTEXT("PrepCheck_DailyGoalValueFormat", "{Count}명");
	DailyGoalBonusFormat = LOCTEXT("PrepCheck_DailyGoalBonusFormat", "달성 시 +{Amount} Gold");
	GoldValueFormat = LOCTEXT("PrepCheck_GoldValueFormat", "{Amount} Gold");
	HeldGoldValueFormat = LOCTEXT("PrepCheck_HeldGoldValueFormat", "{Amount}");
	RequiredFacilityFormat = LOCTEXT("PrepCheck_RequiredFacilityFormat", "{Name} · {Required}개 설치 · {State}");
	NoRequiredFacilitiesText = LOCTEXT("PrepCheck_NoRequiredFacilities", "필수 시설 조건 없음");
	OpenGateReadyText = LOCTEXT("PrepCheck_OpenGateReady", "✓ 준비가 완료되었습니다.");
	AdmissionValueFormat = LOCTEXT("AdmissionValue", "{Amount}");
	AppliedAdmissionFormat = LOCTEXT("AdmissionAppliedValue", "현재 적용: {Amount}");
	AdmissionRangeFormat = LOCTEXT("AdmissionRange", "{Min} ~ {Max} · 기준 입장료 {Reference}");
	AdmissionAppliedText = LOCTEXT("AdmissionApplied", "현재 적용된 입장료입니다.");
	AdmissionDraftText = LOCTEXT("AdmissionDraft", "미리보기 · 적용을 눌러 변경하세요.");
	AdmissionLockedText = LOCTEXT("AdmissionLocked", "운영 중에는 입장료를 바꿀 수 없습니다.");
	AdmissionUnavailableText = LOCTEXT("AdmissionUnavailable", "입장료 정보를 확인할 수 없습니다.");
	AdmissionGateDraftText = LOCTEXT("AdmissionGateDraft", "입장료 변경을 적용하거나 되돌린 뒤 시작하세요.");
	AdmissionDeltaFormat = LOCTEXT("AdmissionDelta", "현재 적용값 대비 {Sign}{Count}명");
	AdmissionNoChangeText = LOCTEXT("AdmissionNoChange", "현재 적용값 대비 변동 없음");
}

void UFF_PrepPanelCheck::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (EstimateNoticeText.IsEmpty())
	{
		EstimateNoticeText = LOCTEXT("PrepCheck_EstimateNotice",
			"부스 매출과 미니게임 보상은 운영 결과에 따라 달라지므로 이 표에 넣지 않았습니다.");
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
	if (VerticalBox_RecommendedRows)
	{
		for (UWidget* Child : VerticalBox_RecommendedRows->GetAllChildren())
		{
			if (auto* Row = Cast<UFF_PrepRecommendedFacilityRow>(Child))
			{
				Row->OnPlaceRequested.AddUObject(this, &ThisClass::HandleRecommendedPlaceRequested);
			}
		}
	}
	if (Button_RequiredFacilities) Button_RequiredFacilities->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStaffAssignmentGuidanceClicked);
	if (Button_OpenGateReason) Button_OpenGateReason->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStaffAssignmentGuidanceClicked);
	if (Slider_AdmissionFee) Slider_AdmissionFee->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleAdmissionValueChanged);
	if (Button_AdmissionDecrease) Button_AdmissionDecrease->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAdmissionDecrease);
	if (Button_AdmissionIncrease) Button_AdmissionIncrease->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAdmissionIncrease);
	if (Button_AdmissionApply) Button_AdmissionApply->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAdmissionApply);
	if (Button_AdmissionReset) Button_AdmissionReset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAdmissionReset);

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
			TEXT("[%s] Button_OpenGate 가 없어 점검 패널에서 개장을 요청할 수 없다."), *GetName());
	}
}

void UFF_PrepPanelCheck::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// 바가 CTA 버튼을 접은 뒤로 개장 수단은 이 패널뿐이다. 시설을 놓거나 철거하면
	// 판정이 바뀌므로 바의 알림을 받아야 버튼이 제때 켜진다.
	if (UFF_PrepHudBar* Bar = GetOwningBar())
	{
		Bar->OnCtaStateChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleCtaStateChanged);
	}

	BindViewModel();
	DraftAdmissionFee = AppliedAdmissionFee = INDEX_NONE;
	AdmissionResultText = FText::GetEmpty();
	RefreshAll();
}

void UFF_PrepPanelCheck::NativeOnPanelClosed()
{
	// 해제는 조건부, 정리는 무조건.
	if (UFF_PrepHudBar* Bar = GetOwningBar())
	{
		Bar->OnCtaStateChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleCtaStateChanged);
	}

	UnbindViewModel();
	DraftAdmissionFee = AppliedAdmissionFee = INDEX_NONE;
	AdmissionResultText = FText::GetEmpty();
	Super::NativeOnPanelClosed();
}

void UFF_PrepPanelCheck::NativeDestruct()
{
	if (UFF_PrepHudBar* Bar = GetOwningBar()) Bar->OnCtaStateChanged.RemoveDynamic(this, &ThisClass::HandleCtaStateChanged);
	UnbindViewModel();
	Super::NativeDestruct();
}

void UFF_PrepPanelCheck::HandleCtaStateChanged()
{
	RefreshOpenGate();
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
	if (UWorld* World = GetWorld())
	{
		BoundRequiredFacilities = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>();
		if (UFF_RequiredFacilitiesSubsystem* Required = BoundRequiredFacilities.Get())
		{
			Required->OnRequiredFacilitiesChanged.AddUniqueDynamic(this, &ThisClass::HandleRequiredFacilitiesChanged);
		}
	}

	UGameInstance* GI = GetGameInstance();
	BoundRecommendedCatalog = GI ? GI->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	if (auto* Catalog = BoundRecommendedCatalog.Get())
	{
		Catalog->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::HandleRecommendedCatalogChanged);
	}
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 견적서가 전부 비어 보인다."), *GetName());
		return;
	}

	CachedViewModel->OnWeatherChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleWeatherChanged);
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleGoldChanged);

	// 홍보를 사면 예상 방문객이 오른다. 그 값이 수입·목표 보너스·혼잡 판정까지 바꾼다.
	CachedViewModel->OnExpectedVisitorsChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleGoldChanged);
	// 입장료를 바꾸면 예상 입장 수입이 바뀐다. 예상 방문객이 그대로여도(배수 하한) 다시 그려야 한다.
	CachedViewModel->OnAdmissionFeeChanged.AddUniqueDynamic(this, &ThisClass::HandleAdmissionFeeChanged);
	CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleStaffCountChanged);
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandleStageViewChanged);
	CachedViewModel->OnScheduledProgramsChanged.AddUniqueDynamic(this, &ThisClass::HandleScheduledProgramsChanged);

	// 피날레는 다른 패널에서 산다. 이 요약 줄이 갱신되려면 결과 이벤트를 받아야 한다.
	CachedViewModel->OnPurchasedFinaleGradeChanged.AddUniqueDynamic(this, &UFF_PrepPanelCheck::HandlePurchasedFinaleGradeChanged);
}

void UFF_PrepPanelCheck::UnbindViewModel()
{
	if (auto* Catalog = BoundRecommendedCatalog.Get())
	{
		Catalog->OnCatalogChanged.RemoveDynamic(this, &ThisClass::HandleRecommendedCatalogChanged);
	}
	BoundRecommendedCatalog.Reset();
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnWeatherChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleWeatherChanged);
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleGoldChanged);
		CachedViewModel->OnExpectedVisitorsChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleGoldChanged);
		CachedViewModel->OnAdmissionFeeChanged.RemoveDynamic(this, &ThisClass::HandleAdmissionFeeChanged);
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleStaffCountChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandleStageViewChanged);
		CachedViewModel->OnScheduledProgramsChanged.RemoveDynamic(this, &ThisClass::HandleScheduledProgramsChanged);
		CachedViewModel->OnPurchasedFinaleGradeChanged.RemoveDynamic(this, &UFF_PrepPanelCheck::HandlePurchasedFinaleGradeChanged);
	}
	CachedViewModel = nullptr;
	if (UFF_RequiredFacilitiesSubsystem* Required = BoundRequiredFacilities.Get())
	{
		Required->OnRequiredFacilitiesChanged.RemoveDynamic(this, &ThisClass::HandleRequiredFacilitiesChanged);
	}
	BoundRequiredFacilities.Reset();
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

bool UFF_PrepPanelCheck::HasAdmissionDraft() const
{
	return bAdmissionRangeValid && DraftAdmissionFee != AppliedAdmissionFee;
}

void UFF_PrepPanelCheck::RefreshAdmission()
{
	TGuardValue<bool> RefreshGuard(bRefreshingAdmission, true);
	const UFF_FestivalViewModelSubsystem* VM = GetViewModel();
	int32 Reference = INDEX_NONE;
	AdmissionMin = AdmissionMax = INDEX_NONE;
	if (VM) VM->GetAdmissionFeeRange(AdmissionMin, AdmissionMax, Reference);
	const int32 ActualFee = VM ? VM->GetAdmissionFeeGold() : INDEX_NONE;
	bAdmissionRangeValid = ActualFee >= 0 && AdmissionMin >= 0 && AdmissionMax >= AdmissionMin && Reference >= 0;
	const bool bCanEdit = bAdmissionRangeValid && VM && VM->CanSetAdmissionFeeNow() && !bAdmissionRequestInFlight;
	if (!bAdmissionRangeValid)
	{
		AppliedAdmissionFee = DraftAdmissionFee = INDEX_NONE;
		ExpectedAdmissionRevenue = INDEX_NONE;
		if (VM) ExpectedVisitorsMin = ExpectedVisitorsMax = INDEX_NONE;
	}
	else
	{
		// 외부 변경·로드는 원본을 따른다. 금화/예측 이벤트만으로 작성 중인 초안을 덮지 않는다.
		if (DraftAdmissionFee == INDEX_NONE || (!bAdmissionRequestInFlight && ActualFee != AppliedAdmissionFee)
			|| !VM->CanSetAdmissionFeeNow())
		{
			DraftAdmissionFee = ActualFee;
		}
		AppliedAdmissionFee = ActualFee;
		DraftAdmissionFee = FMath::Clamp(DraftAdmissionFee, AdmissionMin, AdmissionMax);
	}
	if (Slider_AdmissionFee)
	{
		Slider_AdmissionFee->SetMinValue(0.f); Slider_AdmissionFee->SetMaxValue(1.f);
		const int32 Span = bAdmissionRangeValid ? AdmissionMax - AdmissionMin : 0;
		Slider_AdmissionFee->SetStepSize(Span > 0 ? 1.f / Span : 1.f);
		Slider_AdmissionFee->SetValue(Span > 0 ? static_cast<float>(DraftAdmissionFee - AdmissionMin) / Span : 0.f);
		Slider_AdmissionFee->SetIsEnabled(bCanEdit && Span > 0);
	}
	if (Button_AdmissionDecrease) Button_AdmissionDecrease->SetIsEnabled(bCanEdit && DraftAdmissionFee > AdmissionMin);
	if (Button_AdmissionIncrease) Button_AdmissionIncrease->SetIsEnabled(bCanEdit && DraftAdmissionFee < AdmissionMax);
	if (Button_AdmissionApply) Button_AdmissionApply->SetIsEnabled(bCanEdit && HasAdmissionDraft());
	if (Button_AdmissionReset) Button_AdmissionReset->SetIsEnabled(bCanEdit && HasAdmissionDraft());
	const FText Unknown = LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다");
	SetTextOrHide(Text_AdmissionFeeValue, bAdmissionRangeValid
		? FText::Format(AdmissionValueFormat, FFormatNamedArguments{{TEXT("Amount"), FText::AsNumber(DraftAdmissionFee)}}) : Unknown);
	SetTextOrHide(Text_AppliedAdmissionFee, bAdmissionRangeValid
		? FText::Format(AppliedAdmissionFormat, FFormatNamedArguments{{TEXT("Amount"), FText::AsNumber(AppliedAdmissionFee)}}) : Unknown);
	SetTextOrHide(Text_AdmissionFeeRange, bAdmissionRangeValid
		? FText::Format(AdmissionRangeFormat, FFormatNamedArguments{
			{TEXT("Min"), FText::AsNumber(AdmissionMin)}, {TEXT("Max"), FText::AsNumber(AdmissionMax)},
			{TEXT("Reference"), FText::AsNumber(Reference)}}) : FText::GetEmpty());
	SetTextOrHide(Text_AdmissionFeeState, !bAdmissionRangeValid ? AdmissionUnavailableText
		: (!AdmissionResultText.IsEmpty() ? AdmissionResultText
			: (!VM->CanSetAdmissionFeeNow() ? AdmissionLockedText : (HasAdmissionDraft() ? AdmissionDraftText : AdmissionAppliedText))));
	if (bAdmissionRangeValid)
	{
		// 입장료 이벤트 직후의 VM 방문객 캐시는 아직 이전 값일 수 있어 원본 예보를 읽는다.
		const FFF_VisitorForecast Applied = VM->GetVisitorForecast();
		const FFF_VisitorForecast Preview = HasAdmissionDraft()
			? VM->PreviewVisitorForecast(VM->GetPurchasedPromotion(), DraftAdmissionFee) : Applied;
		ExpectedVisitorsMin = ExpectedVisitorsMax = Preview.ExpectedVisitors;
		ExpectedAdmissionRevenue = Preview.ExpectedAdmissionRevenue;
		const int32 Delta = Preview.ExpectedVisitors - Applied.ExpectedVisitors;
		SetTextOrHide(Text_AdmissionPreviewDelta, Delta == 0 ? AdmissionNoChangeText
			: FText::Format(AdmissionDeltaFormat, FFormatNamedArguments{
				{TEXT("Sign"), FText::FromString(Delta > 0 ? TEXT("+") : TEXT("−"))},
				{TEXT("Count"), FText::AsNumber(FMath::Abs(Delta))}}));
	}
	else SetTextOrHide(Text_AdmissionPreviewDelta, Unknown);
	SetTextOrHide(Text_AdmissionPreviewRevenue, ExpectedAdmissionRevenue >= 0
		? FText::AsNumber(ExpectedAdmissionRevenue) : Unknown);
}

void UFF_PrepPanelCheck::SetAdmissionDraft(int32 NewFee)
{
	const UFF_FestivalViewModelSubsystem* VM = GetViewModel();
	if (bRefreshingAdmission || bAdmissionRequestInFlight) return;
	if (!bAdmissionRangeValid || !VM || !VM->CanSetAdmissionFeeNow()) { RefreshAll(); return; }
	DraftAdmissionFee = FMath::Clamp(NewFee, AdmissionMin, AdmissionMax);
	AdmissionResultText = FText::GetEmpty();
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleAdmissionValueChanged(float NewValue)
{
	if (!bAdmissionRangeValid || !FMath::IsFinite(NewValue)) return;
	SetAdmissionDraft(AdmissionMin + FMath::RoundToInt(FMath::Clamp(NewValue, 0.f, 1.f) * (AdmissionMax - AdmissionMin)));
}

void UFF_PrepPanelCheck::HandleAdmissionDecrease() { SetAdmissionDraft(DraftAdmissionFee - 1); }
void UFF_PrepPanelCheck::HandleAdmissionIncrease() { SetAdmissionDraft(DraftAdmissionFee + 1); }

void UFF_PrepPanelCheck::HandleAdmissionReset()
{
	if (bAdmissionRequestInFlight) return;
	DraftAdmissionFee = INDEX_NONE;
	AdmissionResultText = FText::GetEmpty();
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleAdmissionApply()
{
	UFF_FestivalViewModelSubsystem* VM = GetViewModel();
	if (!VM || bAdmissionRequestInFlight || !HasAdmissionDraft()) return;
	const int32 RequestedFee = DraftAdmissionFee;
	bAdmissionRequestInFlight = true;
	RefreshAdmission();
	RefreshOpenGate();
	const EFF_AdmissionFeeResult Result = VM->RequestSetAdmissionFee(RequestedFee);
	bAdmissionRequestInFlight = false;
	// 동일 값은 성공해도 이벤트가 없으므로 반환 뒤 확정 값을 다시 읽는다.
	AppliedAdmissionFee = DraftAdmissionFee = VM->GetAdmissionFeeGold();
	AdmissionResultText = UFF_EconomyDisplayTextLibrary::GetAdmissionFeeResultText(Result);
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleAdmissionFeeChanged(int32 NewValue)
{
	if (!bAdmissionRequestInFlight) AdmissionResultText = FText::GetEmpty();
	RefreshAll();
}

void UFF_PrepPanelCheck::RefreshAll()
{
	// 미리보기/적용 예보를 먼저 읽고 수용 판정과 견적도 같은 원본 예측값을 표시한다.
	RefreshAdmission();

	RefreshWeather();
	RefreshCapacity();
	RefreshSummary();
	RefreshRequiredFacilities();
	RefreshEstimate();
	RefreshRecommendedFacilities();
	RefreshOpenGate();

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	FFormatNamedArguments DayArgs;
	DayArgs.Add(TEXT("Day"), FText::AsNumber(ViewModel ? ViewModel->GetCurrentDay() : 1));
	SetTextOrHide(Text_DayLabel, FText::Format(DayLabelFormat, DayArgs));
	const int32 Gold = ViewModel ? ViewModel->GetGold() : INDEX_NONE;
	SetTextOrHide(Text_Gold, FormatGoldOrUnknown(Gold, HeldGoldValueFormat));

	const int32 Goal = ViewModel ? ViewModel->GetDailyVisitorGoal() : INDEX_NONE;
	const int32 Bonus = ViewModel ? ViewModel->GetDailyGoalBonusGold() : INDEX_NONE;
	FFormatNamedArguments GoalArgs;
	GoalArgs.Add(TEXT("Count"), FText::AsNumber(Goal));
	FFormatNamedArguments BonusArgs;
	BonusArgs.Add(TEXT("Amount"), FText::AsNumber(Bonus));
	SetTextOrHide(Text_DailyGoalValue, Goal != INDEX_NONE
		? FText::Format(DailyGoalValueFormat, GoalArgs) : LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다"));
	SetTextOrHide(Text_DailyGoalBonus, Bonus != INDEX_NONE
		? FText::Format(DailyGoalBonusFormat, BonusArgs) : LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다"));
	SetTextOrHide(Text_DailyGoal, (Goal != INDEX_NONE && Bonus != INDEX_NONE)
		? FText::Format(
			LOCTEXT("PrepCheck_DailyGoal", "{0}명 · 달성 시 +{1}"),
			FText::AsNumber(Goal), FText::AsNumber(Bonus))
		: LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다"));

	const bool bHasRange = ExpectedVisitorsMin != INDEX_NONE
		&& ExpectedVisitorsMax >= ExpectedVisitorsMin;

	// ★ 둘이 같으면 `500~500명` 이 아니라 `500명` 이다. 지금 경제가 단일값만 주므로
	//   보통 이쪽으로 온다 — 범위처럼 써 두면 없는 정밀도를 있는 것처럼 보이게 한다.
	SetTextOrHide(Text_ExpectedVisitors, bHasRange
		? ((ExpectedVisitorsMin == ExpectedVisitorsMax)
			? FText::Format(LOCTEXT("PrepCheck_ExpectedSingle", "{0}명"),
				FText::AsNumber(ExpectedVisitorsMin))
			: FText::Format(LOCTEXT("PrepCheck_Expected", "{0}~{1}명"),
				FText::AsNumber(ExpectedVisitorsMin), FText::AsNumber(ExpectedVisitorsMax)))
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
			LOCTEXT("PrepCheck_CapacityDetail", "정원 {0}명 대비 예상 {1}명"),
			FText::AsNumber(SoftCapacity), FText::AsNumber(WorstCaseVisitors));
	}
	SetTextOrHide(Text_CapacityDetail, Detail);
}

void UFF_PrepPanelCheck::RefreshSummary()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();

	const int32 Total = ViewModel ? ViewModel->GetTotalStaffCount() : INDEX_NONE;
	const int32 Base = ViewModel ? ViewModel->GetBaseStaffCount() : INDEX_NONE;
	const int32 Temporary = ViewModel ? ViewModel->GetHiredStaffCount() : INDEX_NONE;

	// 프로그램은 인원을 묶지 않는다. 다만 사건에 출동하는 것은 안내 시설에 배정된 인원뿐이라
	// 배정 수를 함께 적는다 — 보유 인원만 보이면 배정이 필요하다는 것을 알 수 없다.
	const int32 Unassigned = ViewModel ? ViewModel->GetUnassignedStaffCount() : INDEX_NONE;
	FText StaffSummary = LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다");
	if (Total >= 0 && Base >= 0 && Temporary >= 0)
	{
		StaffSummary = (Unassigned >= 0 && Unassigned <= Total)
			? FText::Format(
				LOCTEXT("PrepCheck_StaffWithAssignment", "{0}명 (기본 {1} · 고용 {2}) · 배정 {3}명"),
				FText::AsNumber(Total), FText::AsNumber(Base), FText::AsNumber(Temporary),
				FText::AsNumber(Total - Unassigned))
			: FText::Format(
				LOCTEXT("PrepCheck_Staff", "{0}명 (기본 {1} · 고용 {2})"),
				FText::AsNumber(Total), FText::AsNumber(Base), FText::AsNumber(Temporary));
	}
	SetTextOrHide(Text_StaffSummary, StaffSummary);

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

void UFF_PrepPanelCheck::RefreshRequiredFacilities()
{
	const UWorld* World = GetWorld();
	const UFF_RequiredFacilitiesSubsystem* Required = World
		? World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>() : nullptr;
	SetTextOrHide(Text_RequiredFacilities, Required
		? FFF_PrepCheckRules::FormatRequiredFacilities(Required->GetRequiredFacilitiesStatus(),
			RequiredFacilityFormat, NoRequiredFacilitiesText)
		: LOCTEXT("PrepCheck_RequiredUnknown", "필수 시설 상태를 확인할 수 없습니다."));
	RefreshStaffAssignmentLinks();
}

void UFF_PrepPanelCheck::RefreshStaffAssignmentLinks()
{
	const UWorld* World = GetWorld();
	const UFF_RequiredFacilitiesSubsystem* Required = World
		? World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>() : nullptr;
	const bool bHasTarget = Required && Required->FindStaffAssignmentTarget();
	const FText Hint = LOCTEXT("PrepCheck_AssignStaffLink", "클릭하면 해당 시설의 스태프 탭으로 이동합니다.");
	if (Button_RequiredFacilities)
	{
		Button_RequiredFacilities->SetIsEnabled(bHasTarget);
		Button_RequiredFacilities->SetCursor(bHasTarget ? EMouseCursor::Hand : EMouseCursor::Default);
		Button_RequiredFacilities->SetToolTipText(bHasTarget ? Hint : FText::GetEmpty());
	}
	if (Button_OpenGateReason)
	{
		const UFF_PrepHudBar* Bar = GetOwningBar();
		const bool bCanFollow = bHasTarget && Bar && Bar->GetCtaState() != EFF_PrepCtaState::Hidden &&
			!HasAdmissionDraft() && !bAdmissionRequestInFlight;
		Button_OpenGateReason->SetIsEnabled(bCanFollow);
		Button_OpenGateReason->SetVisibility(bCanFollow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_OpenGateReason->SetCursor(bCanFollow ? EMouseCursor::Hand : EMouseCursor::Default);
		Button_OpenGateReason->SetToolTipText(bCanFollow ? Hint : FText::GetEmpty());
	}
}

void UFF_PrepPanelCheck::RefreshOpenGate()
{
	RefreshStaffAssignmentLinks();
	const UFF_PrepHudBar* Bar = GetOwningBar();
	if (!Bar)
	{
		// 바 없이 이 패널이 떠 있을 수 없다. 여기 오면 배선이 끊긴 것이므로 남긴다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 소유 바가 없어 개장 버튼 상태를 정할 수 없다."), *GetName());
		if (Button_OpenGate) { Button_OpenGate->SetIsEnabled(false); }
		SetTextOrHide(Text_OpenGateReason, LOCTEXT("PrepCheck_GateUnavailable", "시작 조건을 확인할 수 없습니다."));
		return;
	}

	const EFF_PrepCtaState State = Bar->GetCtaState();
	const bool bDraftBlocks = HasAdmissionDraft() || bAdmissionRequestInFlight;
	SetTextOrHide(Text_OpenGateReason, bDraftBlocks ? AdmissionGateDraftText : (State == EFF_PrepCtaState::Ready
		? (MissingRecommendedCount > 0 ? RequiredReadyWithWarningsText : OpenGateReadyText) : Bar->GetCtaReason()));

	// 문구는 단계마다 다르다. 셋업에서 `개장하기` 를 띄우면 하루를 건너뛰는 것처럼 읽힌다.
	const FText Label = State == EFF_PrepCtaState::Ready && !bDraftBlocks && MissingRecommendedCount > 0
		? FText::Format(OpenWithWarningsFormat, FFormatNamedArguments{
			{ TEXT("Day"), FText::AsNumber(GetViewModel() ? GetViewModel()->GetCurrentDay() : 1) } })
		: Bar->GetCtaLabelText();
	if (Text_OpenGateLabel && !Label.IsEmpty())
	{
		Text_OpenGateLabel->SetText(Label);
	}

	if (Button_OpenGate)
	{
		// 버튼과 사유가 하단 바의 동일한 판정을 사용한다.
		Button_OpenGate->SetIsEnabled(State == EFF_PrepCtaState::Ready && !bDraftBlocks);
		Button_OpenGate->SetVisibility(State == EFF_PrepCtaState::Hidden
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}
}

void UFF_PrepPanelCheck::RefreshEstimate()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();

	const FFF_PrepDayEstimate Estimate = FFF_PrepCheckRules::BuildDayEstimate(
		ViewModel ? ViewModel->GetTodayStaffHireCostGold() : INDEX_NONE,
		ExpectedVisitorsMin,
		ExpectedAdmissionRevenue,
		ViewModel ? ViewModel->GetDailyVisitorGoal() : INDEX_NONE,
		ViewModel ? ViewModel->GetDailyGoalBonusGold() : INDEX_NONE);

	SetTextOrHide(Text_StaffWageCost, FormatGoldOrUnknown(Estimate.StaffHireCost));
	SetTextOrHide(Text_ExpectedAdmission, FormatGoldOrUnknown(Estimate.ExpectedAdmissionRevenue));
	SetTextOrHide(Text_ExpectedGoalBonus, FormatGoldOrUnknown(Estimate.ExpectedGoalBonus));
}

void UFF_PrepPanelCheck::RefreshRecommendedFacilities()
{
	MissingRecommendedCount = 0;
	// 기존 WBP를 사용할 때는 보이지 않는 경고가 개장 문구에만 나타나지 않게 한다.
	if (!VerticalBox_RecommendedRows || !Border_RecommendedFacilities) return;
	const auto* VM = GetViewModel();
	const auto View = VM ? VM->GetPrepRecommendedFacilities(RecommendedFacilityRules) : FFF_PrepRecommendedFacilitiesView();
	TSet<EFF_PrepRecommendedFacilityKind> VisibleKinds;
	for (UWidget* Child : VerticalBox_RecommendedRows->GetAllChildren())
	{
		auto* Row = Cast<UFF_PrepRecommendedFacilityRow>(Child);
		if (!Row) continue;
		const bool bMissing = Row->bEnabled && View.bAvailable && View.MissingKinds.Contains(Row->Kind);
		Row->SetVisibility(bMissing ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bMissing)
		{
			VisibleKinds.Add(Row->Kind);
			const bool bCanPlace = !FindRecommendedCatalogTarget(Row->Kind).IsNone();
			Row->SetPlacementEnabled(bCanPlace, bCanPlace ? FText::GetEmpty() : RecommendedCatalogUnavailableText);
		}
	}
	MissingRecommendedCount = VisibleKinds.Num();
	Border_RecommendedFacilities->SetVisibility(MissingRecommendedCount > 0
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	FFormatNamedArguments Args{{TEXT("Count"), FText::AsNumber(MissingRecommendedCount)}};
	SetTextOrHide(Text_RecommendedCount, FText::Format(RecommendedCountFormat, Args));
	SetTextOrHide(Text_RecommendedSummary, !View.bAvailable ? RecommendedUnknownText
		: MissingRecommendedCount > 0 ? FText::Format(RecommendedSummaryFormat, Args) : FText::GetEmpty());
}

FName UFF_PrepPanelCheck::FindRecommendedCatalogTarget(EFF_PrepRecommendedFacilityKind Kind) const
{
	const auto* Catalog = BoundRecommendedCatalog.Get();
	if (!Catalog || !Catalog->IsCatalogReady()) return NAME_None;
	const FFF_BuildCatalogEntry* Target = nullptr;
	for (const auto& Entry : Catalog->GetEntriesView())
	{
		if (!RecommendedFacilityRules.Matches(Kind, Entry.Category, Entry.FacilityDefinitionId)) continue;
		// 기존 카탈로그에서 후보를 보여 주기만 한다. 자동 구매·설치는 하지 않는다.
		if (!Target || (Entry.HasValidInstallCost() && (!Target->HasValidInstallCost() || Entry.InstallCost < Target->InstallCost)))
			Target = &Entry;
	}
	return Target ? Target->FacilityDefinitionId : NAME_None;
}

void UFF_PrepPanelCheck::HandleRecommendedCatalogChanged()
{
	RefreshRecommendedFacilities();
	RefreshOpenGate();
}

void UFF_PrepPanelCheck::HandleRecommendedPlaceRequested(EFF_PrepRecommendedFacilityKind Kind)
{
	const FName Target = FindRecommendedCatalogTarget(Kind);
	auto* Build = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	// 프로그램의 시설 바로가기와 같은 입력 브리지를 사용한다. 성공하면 HUD가 배치 탭으로 전환한다.
	if (!Build || Target.IsNone() || !Build->OpenCatalogForFacility(Target))
		SetTextOrHide(Text_RecommendedSummary, RecommendedNavigationFailedText);
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
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	// 예약 그룹 수가 아니라 프로그램 패널과 같은 당일 편성 목록을 읽는다.
	return ViewModel ? ViewModel->GetScheduledProgramIds().Num() : INDEX_NONE;
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

void UFF_PrepPanelCheck::HandleStaffAssignmentGuidanceClicked()
{
	UWorld* World = GetWorld();
	UFF_RequiredFacilitiesSubsystem* Required = World
		? World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>() : nullptr;
	TWeakObjectPtr<AFFZSFacilityActor> Target = Required ? Required->FindStaffAssignmentTarget() : nullptr;
	UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	UFF_FacilityInfoSubsystem* Info = World ? World->GetSubsystem<UFF_FacilityInfoSubsystem>() : nullptr;
	UFF_PrepHudBar* Bar = GetOwningBar();
	if (!Target.IsValid() || !Build || !Info || !Bar)
	{
		RefreshRequiredFacilities();
		RefreshOpenGate();
		return;
	}
	// 기존 건축 선택 경로로 전환해야 선택 강조와 입력 소유권도 함께 정리된다.
	Bar->SelectTab(EFF_PrepHudTab::Build);
	if (!Target.IsValid() || !Build->InspectFacility(Target.Get()) || !Info->ShowSelectedStaffPage()) { return; }
	if (const AFF_FestivalPlayerController* Controller = Cast<AFF_FestivalPlayerController>(GetOwningPlayer()))
	{
		if (AFF_OverviewCameraPawn* Camera = Controller->GetOverviewCameraPawn())
		{
			Camera->FocusOnLocation(Target->GetActorLocation());
		}
	}
}

void UFF_PrepPanelCheck::HandleOpenGateClicked()
{
	if (HasAdmissionDraft() || bAdmissionRequestInFlight) { RefreshOpenGate(); return; }
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
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleGoldChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleStaffCountChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleRequiredFacilitiesChanged()
{
	// 전체 설치/철거 이벤트이므로 필수 조건을 만족한 상태에서도 수용량이 바뀔 수 있다.
	RefreshCapacity();
	// 스태프 배정·해제도 이 신호로 온다. 인력 줄의 배정 수를 같이 갱신한다.
	RefreshSummary();
	RefreshRequiredFacilities();
	RefreshRecommendedFacilities();
	RefreshOpenGate();
}

void UFF_PrepPanelCheck::HandleScheduledProgramsChanged()
{
	RefreshSummary();
}

void UFF_PrepPanelCheck::HandlePurchasedFinaleGradeChanged(EFF_FinaleGrade NewGrade)
{
	// 등급뿐 아니라 잔액·견적도 함께 바뀐다. 요약 한 줄만 고치지 않고 전체를 다시 그린다.
	RefreshAll();
}

void UFF_PrepPanelCheck::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	// 단계가 바뀌면 날씨 잠금과 피날레 요약이 함께 바뀐다.
	DraftAdmissionFee = AppliedAdmissionFee = INDEX_NONE;
	AdmissionResultText = FText::GetEmpty();
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

FText UFF_PrepPanelCheck::FormatGoldOrUnknown(int32 Amount) const
{
	return FormatGoldOrUnknown(Amount, GoldValueFormat);
}

FText UFF_PrepPanelCheck::FormatGoldOrUnknown(int32 Amount, const FText& Format) const
{
	// 개장 판단의 근거가 되는 화면이라 빈칸으로 두지 않는다.
	// 빈칸은 "문제 없음"으로 읽히고, 0 은 "공짜"로 읽힌다. 둘 다 사실이 아니다.
	if (Amount == INDEX_NONE) { return LOCTEXT("PrepCheck_Unknown", "아직 알 수 없습니다"); }
	FFormatNamedArguments Args;
	Args.Add(TEXT("Amount"), FText::AsNumber(Amount));
	return FText::Format(Format, Args);
}

#undef LOCTEXT_NAMESPACE
