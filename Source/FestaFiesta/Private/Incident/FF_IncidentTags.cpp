// Fill out your copyright notice in the Description page of Project Settings.

#include "Incident/FF_IncidentTags.h"

namespace FFIncidentTags
{
	namespace Crowd
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Congestion, "Incident.Crowd.Congestion", "문제형. 밀집 셀이 Fruin 임계를 넘음");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Crush, "Incident.Crowd.Crush", "문제형. Congestion 방치 시 EscalateTo 대상");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(MemberMissing, "Incident.Crowd.MemberMissing", "문제형. 파티원이 리더와 일정 시간 이상 떨어짐 (미아)");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(ItemLost, "Incident.Crowd.ItemLost", "문제형. 누군가가 아이템을 떨어뜨림");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(ChildCrying, "Incident.Crowd.ChildCrying", "문제형. 아이가 울기 시작");
	}

	namespace Facility
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Collapse, "Incident.Facility.Collapse", "문제형. 시설 붕괴 (엣지 신호)");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(MinorFault, "Incident.Facility.MinorFault", "문제형. 경미 고장. 가동률이 높을수록 잦음");
	}

	namespace Queue
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Overload, "Incident.Queue.Overload", "문제형. 대기 시간 초과 또는 이탈 급증");
	}

	namespace Economy
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bankrupt, "Incident.Economy.Bankrupt", "문제형. 잔고 음수. 전역 억제 (앵커 없음)");
	}

	namespace Weather
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shower, "Incident.Weather.Shower", "기회형. 소나기");
	}

	namespace Visitor
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(LostItem, "Incident.Visitor.LostItem", "기회형. 분실물");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dispute, "Incident.Visitor.Dispute", "문제형. 방문객 분쟁. 밀집도가 높을수록 잦음");
	}
}
