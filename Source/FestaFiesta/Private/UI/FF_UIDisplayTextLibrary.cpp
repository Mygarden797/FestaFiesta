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
	case EFF_FestivalStage::Live:				return LOCTEXT("Stage_Live", "라이브 운영");
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

#undef LOCTEXT_NAMESPACE
