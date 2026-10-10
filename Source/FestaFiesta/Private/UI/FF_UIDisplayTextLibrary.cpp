// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_UIDisplayTextLibrary.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUIDisplayText"

// 🔴 `default:` 를 쓰지 않는다. 항목을 더했을 때 컴파일 경고로 잡히게 하려는 것이고,
//    그래도 새면 `FestaFiesta.UI.DisplayText.*` 테스트가 영문 이름을 잡는다.

FText UFF_UIDisplayTextLibrary::GetFestivalStageText(EFF_FestivalStage Stage)
{
	switch (Stage)
	{
	case EFF_FestivalStage::Setup:				return LOCTEXT("Stage_Setup", "셋업");
	case EFF_FestivalStage::PromotionPreview:	return LOCTEXT("Stage_PromotionPreview", "홍보 프리뷰");
	case EFF_FestivalStage::SunsetTransition:	return LOCTEXT("Stage_SunsetTransition", "노을 전환");
	case EFF_FestivalStage::MorningCheck:		return LOCTEXT("Stage_MorningCheck", "모닝체크");
	case EFF_FestivalStage::Live:				return LOCTEXT("Stage_Live", "라이브");
	case EFF_FestivalStage::Closing:			return LOCTEXT("Stage_Closing", "마감");
	case EFF_FestivalStage::Report:				return LOCTEXT("Stage_Report", "리포트	");
	case EFF_FestivalStage::Finale:				return LOCTEXT("Stage_Finale", "피날레");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetPrepTabText(EFF_PrepHudTab Tab)
{
	switch (Tab)
	{
	case EFF_PrepHudTab::Build:		return LOCTEXT("PrepTab_Build", "배치");
	case EFF_PrepHudTab::Program:	return LOCTEXT("PrepTab_Program", "프로그램");
	case EFF_PrepHudTab::Staff:		return LOCTEXT("PrepTab_Staff", "스태프");
	case EFF_PrepHudTab::Promotion:	return LOCTEXT("PrepTab_Promotion", "홍보");
	case EFF_PrepHudTab::Finale:	return LOCTEXT("PrepTab_Finale", "피날레");
	case EFF_PrepHudTab::Check:		return LOCTEXT("PrepTab_Check", "점검");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetLiveTabText(EFF_LiveTab Tab)
{
	switch (Tab)
	{
	case EFF_LiveTab::Operations:	return LOCTEXT("LiveTab_Operations", "운영");
	case EFF_LiveTab::Facilities:	return LOCTEXT("LiveTab_Facilities", "시설");
	case EFF_LiveTab::Guests:		return LOCTEXT("LiveTab_Guests", "현황");
	case EFF_LiveTab::RevenueRanking: return LOCTEXT("LiveTab_RevenueRanking", "매출 순위");
	case EFF_LiveTab::Queues:		return LOCTEXT("LiveTab_Queues", "대기열");
	case EFF_LiveTab::SNS:			return LOCTEXT("LiveTab_SNS", "SNS");
	case EFF_LiveTab::Report:		return LOCTEXT("LiveTab_Report", "리포트");
	case EFF_LiveTab::Staff:		return LOCTEXT("LiveTab_Staff", "스태프");
	case EFF_LiveTab::Density:		return LOCTEXT("LiveTab_Density", "밀집");
	case EFF_LiveTab::Finale:		return LOCTEXT("LiveTab_Finale", "피날레");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetLiveHudTabText(EFF_LiveHudTab Tab)
{
	switch (Tab)
	{
	case EFF_LiveHudTab::Operations:	return LOCTEXT("LiveHudTab_Operations", "운영");
	case EFF_LiveHudTab::Facilities:	return LOCTEXT("LiveHudTab_Facilities", "시설");
	case EFF_LiveHudTab::Guests:		return LOCTEXT("LiveHudTab_Guests", "관객");
	case EFF_LiveHudTab::Queues:		return LOCTEXT("LiveHudTab_Queues", "대기열");
	case EFF_LiveHudTab::Density:		return LOCTEXT("LiveHudTab_Density", "밀집도");
	case EFF_LiveHudTab::Events:		return LOCTEXT("LiveHudTab_Events", "사건");
	case EFF_LiveHudTab::SNS:			return LOCTEXT("LiveHudTab_SNS", "SNS");
	case EFF_LiveHudTab::Report:		return LOCTEXT("LiveHudTab_Report", "리포트");
	case EFF_LiveHudTab::Staff:			return LOCTEXT("LiveHudTab_Staff", "스태프");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetOptionCategoryText(EFF_OptionCategory Category)
{
	switch (Category)
	{
	case EFF_OptionCategory::Language:		return LOCTEXT("OptionCategory_Language", "언어");
	case EFF_OptionCategory::Accessibility:	return LOCTEXT("OptionCategory_Accessibility", "접근성");
	case EFF_OptionCategory::Sound:			return LOCTEXT("OptionCategory_Sound", "사운드");
	case EFF_OptionCategory::Graphics:		return LOCTEXT("OptionCategory_Graphics", "그래픽");
	case EFF_OptionCategory::Controls:		return LOCTEXT("OptionCategory_Controls", "조작");
	case EFF_OptionCategory::Credits:		return LOCTEXT("OptionCategory_Credits", "크레딧");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetFacilityStateText(EFF_LiveFacilityState State)
{
	switch (State)
	{
	case EFF_LiveFacilityState::Planned:		return LOCTEXT("FacState_Planned", "계획");
	case EFF_LiveFacilityState::Validated:		return LOCTEXT("FacState_Validated", "검증됨");
	case EFF_LiveFacilityState::Active:			return LOCTEXT("FacState_Active", "운영 중");
	case EFF_LiveFacilityState::Closed:			return LOCTEXT("FacState_Closed", "폐쇄");
	case EFF_LiveFacilityState::MovePending:	return LOCTEXT("FacState_MovePending", "이동 대기");
	case EFF_LiveFacilityState::Removed:		return LOCTEXT("FacState_Removed", "철거됨");
	case EFF_LiveFacilityState::Unknown:		return LOCTEXT("FacState_Unknown", "모름");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetFacilityQueueStateText(EFF_LiveFacilityQueueState State)
{
	switch (State)
	{
	case EFF_LiveFacilityQueueState::Open:		return LOCTEXT("FacQueue_Open", "받는 중");
	case EFF_LiveFacilityQueueState::Full:		return LOCTEXT("FacQueue_Full", "정원 참");
	case EFF_LiveFacilityQueueState::Draining:	return LOCTEXT("FacQueue_Draining", "비우는 중");
	case EFF_LiveFacilityQueueState::Closed:	return LOCTEXT("FacQueue_Closed", "닫힘");
	case EFF_LiveFacilityQueueState::Blocked:	return LOCTEXT("FacQueue_Blocked", "막힘");
	case EFF_LiveFacilityQueueState::Unknown:	return LOCTEXT("FacQueue_Unknown", "모름");
	case EFF_LiveFacilityQueueState::Paused:	return LOCTEXT("FacQueue_Paused", "공연 중 중단");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetNeedTypeText(EFF_NeedType NeedType)
{
	switch (NeedType)
	{
	case EFF_NeedType::Hunger:		return LOCTEXT("Need_Hunger", "배고픔");
	case EFF_NeedType::Thirst:		return LOCTEXT("Need_Thirst", "갈증");
	case EFF_NeedType::Restroom:	return LOCTEXT("Need_Restroom", "화장실");
	case EFF_NeedType::Fatigue:		return LOCTEXT("Need_Fatigue", "피로");
	case EFF_NeedType::Stimulation:	return LOCTEXT("Need_Stimulation", "즐길거리");
	default:						return FText::GetEmpty();
	}
}

FText UFF_UIDisplayTextLibrary::GetNeedRemedyText(EFF_NeedType NeedType)
{
	// ★ 시설 종류 이름을 그대로 쓰지 않고 **할 일**로 적는다.
	//   `푸드 부스` 보다 `먹을 곳을 늘리세요` 가 무엇을 해야 하는지에 가깝다.
	switch (NeedType)
	{
	case EFF_NeedType::Hunger:		return LOCTEXT("NeedFix_Hunger", "먹을 곳을 늘리세요");
	case EFF_NeedType::Thirst:		return LOCTEXT("NeedFix_Thirst", "마실 곳을 늘리세요");
	case EFF_NeedType::Restroom:	return LOCTEXT("NeedFix_Restroom", "화장실을 늘리세요");
	case EFF_NeedType::Fatigue:		return LOCTEXT("NeedFix_Fatigue", "쉴 곳을 늘리세요");
	case EFF_NeedType::Stimulation:	return LOCTEXT("NeedFix_Stimulation", "볼거리를 늘리세요");
	default:						return FText::GetEmpty();
	}
}

FText UFF_UIDisplayTextLibrary::GetIncidentResolveResultText(EFF_IncidentResolveResult Result)
{
	switch (Result)
	{
	case EFF_IncidentResolveResult::Dispatched:			return LOCTEXT("IncidentResult_Dispatched", "스태프가 현장으로 출동했어요");
	case EFF_IncidentResolveResult::Resolved:			return LOCTEXT("IncidentResult_Resolved", "사건을 처리했어요");
	case EFF_IncidentResolveResult::InvalidIncident:	return LOCTEXT("IncidentResult_InvalidIncident", "이미 끝난 사건이에요");
	case EFF_IncidentResolveResult::InvalidOption:		return LOCTEXT("IncidentResult_InvalidOption", "고를 수 없는 선택지예요 · 다른 선택지를 골라 주세요");
	case EFF_IncidentResolveResult::AlreadyResponding:	return LOCTEXT("IncidentResult_AlreadyResponding", "이미 스태프가 출동 중이에요 · 도착할 때까지 기다려 주세요");
	case EFF_IncidentResolveResult::NotEnoughStaff:		return LOCTEXT("IncidentResult_NotEnoughStaff", "출동할 스태프가 부족해요 · 안내 시설에 스태프를 배정해 주세요");
	case EFF_IncidentResolveResult::NoSite:				return LOCTEXT("IncidentResult_NoSite", "사건 현장을 찾지 못했어요");
	case EFF_IncidentResolveResult::DispatchFailed:		return LOCTEXT("IncidentResult_DispatchFailed", "출동하지 못했어요 · 잠시 후 다시 시도해 주세요");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetIncidentResolveResultLabel(EFF_IncidentResolveResult Result)
{
	switch (Result)
	{
	case EFF_IncidentResolveResult::Dispatched:			return LOCTEXT("IncidentLabel_Dispatched", "출동");
	case EFF_IncidentResolveResult::Resolved:			return LOCTEXT("IncidentLabel_Resolved", "처리");
	case EFF_IncidentResolveResult::InvalidIncident:
	case EFF_IncidentResolveResult::InvalidOption:
	case EFF_IncidentResolveResult::AlreadyResponding:
	case EFF_IncidentResolveResult::NotEnoughStaff:
	case EFF_IncidentResolveResult::NoSite:
	case EFF_IncidentResolveResult::DispatchFailed:		return LOCTEXT("IncidentLabel_Rejected", "불가");
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetIncidentResponseStageText(EFF_IncidentResponseStage Stage)
{
	switch (Stage)
	{
	case EFF_IncidentResponseStage::None:		return LOCTEXT("IncidentStage_None", "출동 전");
	case EFF_IncidentResponseStage::Travelling:	return LOCTEXT("IncidentStage_Travelling", "현장으로 가는 중");
	case EFF_IncidentResponseStage::Working:	return LOCTEXT("IncidentStage_Working", "현장 작업 중");
	}

	return FText::GetEmpty();
}

bool UFF_UIDisplayTextLibrary::IsIncidentResolveAccepted(EFF_IncidentResolveResult Result)
{
	return Result == EFF_IncidentResolveResult::Dispatched || Result == EFF_IncidentResolveResult::Resolved;
}

FText UFF_UIDisplayTextLibrary::GetIncidentClosedLabel(EFF_IncidentState State)
{
	switch (State)
	{
	case EFF_IncidentState::Resolved:	return LOCTEXT("IncidentClosedLabel_Resolved", "완료");
	case EFF_IncidentState::Expired:	return LOCTEXT("IncidentClosedLabel_Expired", "소멸");
	case EFF_IncidentState::Ignored:	return LOCTEXT("IncidentClosedLabel_Ignored", "방치");
	case EFF_IncidentState::Missed:		return LOCTEXT("IncidentClosedLabel_Missed", "놓침");
	case EFF_IncidentState::Active:
	case EFF_IncidentState::Acknowledged:
	case EFF_IncidentState::Taken:		return FText::GetEmpty();
	}

	return FText::GetEmpty();
}

FText UFF_UIDisplayTextLibrary::GetIncidentClosedText(EFF_IncidentState State, const FText& Title)
{
	if (Title.IsEmpty())
	{
		switch (State)
		{
		case EFF_IncidentState::Resolved:	return LOCTEXT("IncidentClosedBare_Resolved", "사건 처리를 마쳤어요");
		case EFF_IncidentState::Expired:	return LOCTEXT("IncidentClosedBare_Expired", "사건이 저절로 사라졌어요");
		case EFF_IncidentState::Ignored:	return LOCTEXT("IncidentClosedBare_Ignored", "사건을 손쓰지 못한 채 끝났어요");
		case EFF_IncidentState::Missed:		return LOCTEXT("IncidentClosedBare_Missed", "기회를 놓쳤어요");
		case EFF_IncidentState::Active:
		case EFF_IncidentState::Acknowledged:
		case EFF_IncidentState::Taken:		return FText::GetEmpty();
		}
		return FText::GetEmpty();
	}

	switch (State)
	{
	case EFF_IncidentState::Resolved:	return FText::Format(LOCTEXT("IncidentClosedText_Resolved", "{0} — 처리를 마쳤어요"), Title);
	case EFF_IncidentState::Expired:	return FText::Format(LOCTEXT("IncidentClosedText_Expired", "{0} — 저절로 사라졌어요"), Title);
	case EFF_IncidentState::Ignored:	return FText::Format(LOCTEXT("IncidentClosedText_Ignored", "{0} — 손쓰지 못한 채 끝났어요"), Title);
	case EFF_IncidentState::Missed:		return FText::Format(LOCTEXT("IncidentClosedText_Missed", "{0} — 기회를 놓쳤어요"), Title);
	case EFF_IncidentState::Active:
	case EFF_IncidentState::Acknowledged:
	case EFF_IncidentState::Taken:		return FText::GetEmpty();
	}

	return FText::GetEmpty();
}

EFF_IncidentToastKind UFF_UIDisplayTextLibrary::GetIncidentClosedKind(EFF_IncidentState State)
{
	switch (State)
	{
	case EFF_IncidentState::Resolved:	return EFF_IncidentToastKind::Good;
	case EFF_IncidentState::Ignored:	return EFF_IncidentToastKind::Danger;
	case EFF_IncidentState::Expired:
	case EFF_IncidentState::Missed:
	case EFF_IncidentState::Active:
	case EFF_IncidentState::Acknowledged:
	case EFF_IncidentState::Taken:		return EFF_IncidentToastKind::Neutral;
	}

	return EFF_IncidentToastKind::Neutral;
}

#undef LOCTEXT_NAMESPACE
