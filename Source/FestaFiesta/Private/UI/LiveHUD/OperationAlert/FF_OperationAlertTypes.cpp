// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/OperationAlert/FF_OperationAlertTypes.h"

#define LOCTEXT_NAMESPACE "FFOperationAlert"

TArray<EFF_OperationAlertKind> FFF_OperationAlertRules::Evaluate(
	const FFF_OperationAlertInputs& Inputs, const FFF_OperationAlertThresholds& Thresholds)
{
	TArray<EFF_OperationAlertKind> Kinds;

	// 퇴장·이탈이 먼저다. 시설 부족은 배치를 바꾸기 전까지 계속 참이라 매번 앞에 서면 새 소식이 묻힌다
	if (Inputs.NoChoiceLeavesDelta >= Thresholds.NoChoiceLeavesAtLeast)
	{
		Kinds.Add(EFF_OperationAlertKind::NoChoiceLeave);
	}
	if (Inputs.QueueAbandonsDelta >= Thresholds.QueueAbandonsAtLeast)
	{
		Kinds.Add(EFF_OperationAlertKind::QueueAbandon);
	}
	if (Inputs.RestroomCoverage < Thresholds.ShortageCoverageBelow)
	{
		Kinds.Add(EFF_OperationAlertKind::RestroomShortage);
	}
	if (Inputs.HungerCoverage < Thresholds.ShortageCoverageBelow)
	{
		Kinds.Add(EFF_OperationAlertKind::FoodShortage);
	}
	if (Inputs.StimulationCoverage < Thresholds.ShortageCoverageBelow)
	{
		Kinds.Add(EFF_OperationAlertKind::FunShortage);
	}
	return Kinds;
}

FText FFF_OperationAlertRules::GetMessage(EFF_OperationAlertKind Kind)
{
	switch (Kind)
	{
	case EFF_OperationAlertKind::FoodShortage:
		return LOCTEXT("FoodShortage", "먹을 곳이 부족해요. 음식 부스를 더 지어 보세요");
	case EFF_OperationAlertKind::RestroomShortage:
		return LOCTEXT("RestroomShortage", "화장실이 부족해 불편해하는 손님이 있어요");
	case EFF_OperationAlertKind::FunShortage:
		return LOCTEXT("FunShortage", "즐길 거리가 부족해요. 체험 부스나 포토존을 더해 보세요");
	case EFF_OperationAlertKind::QueueAbandon:
		return LOCTEXT("QueueAbandon", "줄이 길어 떠나는 손님이 많아요");
	case EFF_OperationAlertKind::NoChoiceLeave:
		return LOCTEXT("NoChoiceLeave", "갈 곳이 없어 떠나는 손님이 늘고 있어요");
	case EFF_OperationAlertKind::IncidentFinished:
		return LOCTEXT("IncidentFinished", "사건이 종료됐어요. 소식에서 결과를 확인하세요");
	default:
		return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
