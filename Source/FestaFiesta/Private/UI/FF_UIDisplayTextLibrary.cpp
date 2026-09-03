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
	case EFF_FestivalStage::Report:				return LOCTEXT("Stage_Report", "리포트");
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
	case EFF_PrepHudTab::Staff:		return LOCTEXT("PrepTab_Staff", "인력");
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
	case EFF_LiveTab::Guests:		return LOCTEXT("LiveTab_Guests", "관객");
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

#undef LOCTEXT_NAMESPACE
