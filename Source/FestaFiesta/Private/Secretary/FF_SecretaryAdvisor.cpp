// Copyright Epic Games, Inc. All Rights Reserved.

#include "Secretary/FF_SecretaryAdvisor.h"

#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"

#define LOCTEXT_NAMESPACE "FestaFiestaSecretary"

namespace
{
	int32 SecretaryNeedIndex(const EFF_NeedType Need)
	{
		return static_cast<int32>(Need);
	}

	// 운영 중: 지금 손님 상태 + 내일 아침에 할 일. 운영 중에는 시설을 지을 수 없다.
	FText SecretaryLiveNeedText(const EFF_NeedType Need)
	{
		switch (Need)
		{
		case EFF_NeedType::Hunger:
			return LOCTEXT("LiveHunger", "배고픈 손님이 많아요. 내일 아침에 먹을 곳을 늘려 보세요.");
		case EFF_NeedType::Thirst:
			return LOCTEXT("LiveThirst", "목마른 손님이 많아요. 내일 아침에 마실 곳을 늘려 보세요.");
		case EFF_NeedType::Restroom:
			return LOCTEXT("LiveRestroom", "화장실을 찾는 손님이 많아요. 내일 아침에 화장실을 늘려 보세요.");
		case EFF_NeedType::Fatigue:
			return LOCTEXT("LiveFatigue", "지친 손님이 많아요. 내일 아침에 쉴 곳을 늘려 보세요.");
		case EFF_NeedType::Stimulation:
			return LOCTEXT("LiveStimulation", "심심해하는 손님이 많아요. 내일 아침에 볼거리를 늘려 보세요.");
		default:
			return FText::GetEmpty();
		}
	}

	// 아침: 어제 일 + 지금(모닝체크) 할 일. 모닝체크에서는 지을 수 있다.
	FText SecretaryMorningNeedText(const EFF_NeedType Need)
	{
		switch (Need)
		{
		case EFF_NeedType::Hunger:
			return LOCTEXT("MorningHunger", "배고픈 손님이 많았어요. 먹을 곳을 늘려 보세요.");
		case EFF_NeedType::Thirst:
			return LOCTEXT("MorningThirst", "목마른 손님이 많았어요. 마실 곳을 늘려 보세요.");
		case EFF_NeedType::Restroom:
			return LOCTEXT("MorningRestroom", "화장실을 찾는 손님이 많았어요. 화장실을 늘려 보세요.");
		case EFF_NeedType::Fatigue:
			return LOCTEXT("MorningFatigue", "지친 손님이 많았어요. 쉴 곳을 늘려 보세요.");
		case EFF_NeedType::Stimulation:
			return LOCTEXT("MorningStimulation", "심심해하는 손님이 많았어요. 볼거리를 늘려 보세요.");
		default:
			return FText::GetEmpty();
		}
	}

	// "‘시설’이었어요 / ‘시설’였어요".
	FText SecretaryWasText(const FText& Name)
	{
		return FFSecretaryAdvisor::HasFinalConsonant(Name.ToString())
			? FText::Format(LOCTEXT("WasConsonant", "‘{0}’이었어요"), Name)
			: FText::Format(LOCTEXT("WasVowel", "‘{0}’였어요"), Name);
	}
}

namespace FFSecretaryAdvisor
{
	bool HasFinalConsonant(const FString& Word)
	{
		// 끝에 붙은 따옴표 · 괄호 · 공백은 건너뛴다. 받침은 마지막 낱말 글자로 본다.
		for (int32 Index = Word.Len() - 1; Index >= 0; --Index)
		{
			const TCHAR Char = Word[Index];
			if (FChar::IsWhitespace(Char) || Char == TEXT('\'') || Char == TEXT('"') || Char == TEXT(')')
				|| Char == 0x2019 || Char == 0x201D)
			{
				continue;
			}
			// 완성형 한글 가(AC00) ~ 힣(D7A3). 28 개마다 받침 없는 글자가 하나씩 온다.
			if (Char >= 0xAC00 && Char <= 0xD7A3)
			{
				return (Char - 0xAC00) % 28 != 0;
			}
			return false;
		}
		return false;
	}

	const TArray<EFFZSFacilityCategory>& GetIntroductionOrder()
	{
		static const TArray<EFFZSFacilityCategory> Order = {
			EFFZSFacilityCategory::Toilet,
			EFFZSFacilityCategory::Rest,
			EFFZSFacilityCategory::Info,
			EFFZSFacilityCategory::PhotoSpot,
			EFFZSFacilityCategory::Safety,
			EFFZSFacilityCategory::Lighting,
			EFFZSFacilityCategory::Booth,
			EFFZSFacilityCategory::Performance,
		};
		return Order;
	}

	FFF_SecretaryTip ChooseLiveTip(
		const FFF_SecretaryLiveSignals& Signals,
		const FFF_SecretaryThresholds& Thresholds,
		const TFunctionRef<bool(EFF_SecretaryTipKind)> IsResting)
	{
		FFF_SecretaryTip Tip;
		const auto Offer = [&Tip, &IsResting](const EFF_SecretaryTipKind Kind, const bool bCondition, const FText& Text)
		{
			if (Tip.IsValid() || !bCondition || Text.IsEmpty() || IsResting(Kind))
			{
				return;
			}
			Tip.Kind = Kind;
			Tip.Text = Text;
		};

		const bool bIdleIncident = Signals.IdleIncidentCount > 0;
		Offer(EFF_SecretaryTipKind::NoStaffForIncident, bIdleIncident && Signals.DispatchableStaffCount <= 0,
			LOCTEXT("NoStaff", "사건이 났는데 출동할 스태프가 없어요. 안내 시설을 눌러 '스태프' 탭에서 '+ 배정'을 눌러 주세요."));
		Offer(EFF_SecretaryTipKind::IdleIncident, bIdleIncident && Signals.DispatchableStaffCount > 0,
			LOCTEXT("IdleIncident", "아직 처리 안 된 사건이 있어요. 지도에 뜬 표시를 눌러 스태프를 보내 주세요."));

		if (Signals.bNeedSampled)
		{
			const float RestroomUnmet = Signals.NeedUnmetPercent[SecretaryNeedIndex(EFF_NeedType::Restroom)];
			Offer(EFF_SecretaryTipKind::MissingToilet,
				RestroomUnmet >= Thresholds.UnmetNeedPercent && Signals.ToiletCount <= 0,
				LOCTEXT("MissingToilet", "화장실이 하나도 없어요. 내일 아침엔 화장실을 꼭 놓아 주세요."));

			// 가장 많이 못 채운 필요 하나.
			int32 WorstIndex = INDEX_NONE;
			for (int32 Index = 0; Index < static_cast<int32>(EFF_NeedType::Count); ++Index)
			{
				if (Signals.NeedUnmetPercent[Index] >= Thresholds.UnmetNeedPercent
					&& (WorstIndex == INDEX_NONE || Signals.NeedUnmetPercent[Index] > Signals.NeedUnmetPercent[WorstIndex]))
				{
					WorstIndex = Index;
				}
			}
			if (WorstIndex != INDEX_NONE)
			{
				Offer(EFF_SecretaryTipKind::UnmetNeed, true, SecretaryLiveNeedText(static_cast<EFF_NeedType>(WorstIndex)));
			}
		}

		Offer(EFF_SecretaryTipKind::LongQueue,
			Signals.LongestQueue >= Thresholds.LongQueueCount && !Signals.LongestQueueFacility.IsEmpty(),
			FText::Format(LOCTEXT("LongQueue", "‘{0}’ 줄이 길어요. 내일 아침에 같은 시설을 하나 더 두는 것도 방법이에요."),
				Signals.LongestQueueFacility));
		Offer(EFF_SecretaryTipKind::Crowded, Signals.CongestedCellCount >= Thresholds.CrowdedCellCount,
			LOCTEXT("Crowded", "사람이 한곳에 몰리고 있어요. 내일은 시설을 조금 떨어뜨려 놓아 보세요."));

		return Tip;
	}

	const TArray<FFF_SecretaryGeneralTip>& GetGeneralTips()
	{
		// 운영 중에 바로 쓸 만한 것부터.
		static const TArray<FFF_SecretaryGeneralTip> Tips = {
			{LOCTEXT("GeneralIncident", "사건 표시가 뜨면 눌러서 스태프를 보내 주세요. 표시 테두리가 남은 시간이에요."),
				EFF_SecretaryTipKind::IdleIncident},
			{LOCTEXT("GeneralZoom", "마우스 휠로 멀리서 보면 손님 흐름이 한눈에 보여요.")},
			{LOCTEXT("GeneralRotate", "Q 나 E 를 누르면 시점이 45도씩 돌아가요. 건물에 가려진 곳을 볼 때 써 보세요.")},
			{LOCTEXT("GeneralDragPan", "마우스 오른쪽 버튼을 누른 채 끌면 화면을 옮길 수 있어요.")},
			{LOCTEXT("GeneralCharacterView", "Tab 을 누르면 캐릭터 시점으로 축제장을 걸어 다닐 수 있어요. 다시 누르면 돌아와요.")},
			{LOCTEXT("GeneralLostChild", "미아는 제때 찾아 주지 않으면 울기 시작해요. 울음은 옆 손님들 만족도를 떨어뜨려요.")},
			{LOCTEXT("GeneralCloseFacility", "급하면 운영 중에 시설을 닫을 수 있어요. 다만 그 앞에 서 있던 손님은 다 빠져요.")},
			{LOCTEXT("GeneralCodexGift", "처음 발견한 조합은 도감에서 선물 버튼을 눌러 보상을 받을 수 있어요.")},
			{LOCTEXT("GeneralEmptySpot", "사람이 안 가는 시설은 위치가 아쉬운 거예요. 내일 아침에 옮겨 보세요.")},
		};
		return Tips;
	}

	int32 FindNextGeneralTip(const int32 From, const TFunctionRef<bool(EFF_SecretaryTipKind)> WasSpoken)
	{
		const TArray<FFF_SecretaryGeneralTip>& Tips = GetGeneralTips();
		for (int32 Index = FMath::Max(From, 0); Index < Tips.Num(); ++Index)
		{
			const EFF_SecretaryTipKind Topic = Tips[Index].SameTopic;
			if (Topic == EFF_SecretaryTipKind::None || !WasSpoken(Topic))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	TArray<FText> BuildMorningBriefing(const FFF_SecretaryBriefingInputs& Inputs)
	{
		TArray<FText> Lines;
		Lines.Add(FText::Format(LOCTEXT("MorningHello", "{0}일차 아침이에요. 어제를 짧게 정리해 드릴게요."),
			FText::AsNumber(Inputs.Day)));

		if (Inputs.YesterdaySatisfaction != INDEX_NONE)
		{
			Lines.Add(FText::Format(LOCTEXT("Satisfaction", "어제 만족도는 {0}점이었어요."),
				FText::AsNumber(Inputs.YesterdaySatisfaction)));
		}

		// 어제 결산을 못 봤으면(이어하기로 아침에 들어온 경우 등) 아쉬운 점 얘기를 아예 하지 않는다.
		// "아쉬운 점이 없었어요"는 값을 보고 나서야 할 수 있는 말이다.
		bool bSpokeAboutRestroom = false;
		switch (Inputs.bJudgedYesterday ? Inputs.Weakness : EFF_SecretaryWeakness::None)
		{
		case EFF_SecretaryWeakness::Wait:
			Lines.Add(Inputs.WorstWaitFacility.IsEmpty()
				? LOCTEXT("WeakWait", "줄이 가장 아쉬웠어요. 오래 기다린 시설은 하나 더 두는 것도 방법이에요.")
				: FText::Format(LOCTEXT("WeakWaitFacility",
					"줄이 가장 아쉬웠어요. 특히 ‘{0}’ 앞이 오래 걸렸어요. 같은 시설을 하나 더 두는 것도 방법이에요."),
					Inputs.WorstWaitFacility));
			break;
		case EFF_SecretaryWeakness::Density:
			Lines.Add(LOCTEXT("WeakDensity", "사람이 한곳에 몰린 게 가장 아쉬웠어요. 시설을 조금 떨어뜨려 놓아 보세요."));
			break;
		case EFF_SecretaryWeakness::Preference:
			Lines.Add(LOCTEXT("WeakPreference", "손님 취향에 맞는 시설이 부족했어요. 다른 종류의 시설도 놓아 보세요."));
			break;
		case EFF_SecretaryWeakness::Need:
			Lines.Add(Inputs.bHasWorstNeed
				? SecretaryMorningNeedText(Inputs.WorstNeed)
				: LOCTEXT("WeakNeed", "필요한 걸 못 채운 손님이 많았어요. 먹을 곳, 쉴 곳, 화장실이 넉넉한지 둘러보세요."));
			bSpokeAboutRestroom = Inputs.bHasWorstNeed && Inputs.WorstNeed == EFF_NeedType::Restroom;
			break;
		default:
			if (Inputs.bJudgedYesterday)
			{
				Lines.Add(LOCTEXT("WeakNone", "어제는 크게 아쉬운 점이 없었어요."));
			}
			break;
		}

		if (Inputs.TopRevenueGold > 0 && !Inputs.TopRevenueFacility.IsEmpty())
		{
			Lines.Add(FText::Format(LOCTEXT("TopRevenue", "어제 매출 1등은 {0}. {1} Gold를 벌었어요."),
				SecretaryWasText(Inputs.TopRevenueFacility), FText::AsNumber(Inputs.TopRevenueGold)));
		}

		// 화장실이 하나도 없으면 꼭 짚는다(이미 화장실 얘기를 했으면 되풀이하지 않는다).
		const bool bRecommendToilet = Inputs.ToiletCount == 0 && !bSpokeAboutRestroom;
		if (bRecommendToilet)
		{
			Lines.Add(LOCTEXT("RecommendToilet", "그리고 화장실은 꼭 설치해 두세요. 없으면 손님들이 금방 불편해해요."));
		}

		// 아직 안 놓아 본 분류 하나. 방금 화장실을 권했으면 그다음 분류를 소개한다.
		for (const EFFZSFacilityCategory Category : Inputs.UnbuiltCategories)
		{
			if ((bRecommendToilet || bSpokeAboutRestroom) && Category == EFFZSFacilityCategory::Toilet)
			{
				continue;
			}
			Lines.Add(FText::Format(LOCTEXT("Unbuilt", "‘{0}’ 시설은 아직 안 놓아 보셨죠? 배치 탭에서 한번 둘러보세요."),
				FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(Category)));
			break;
		}

		Lines.Add(LOCTEXT("MorningBye", "오늘도 잘 부탁드려요."));
		return Lines;
	}

	TArray<FText> BuildFirstDayGreeting()
	{
		return {
			LOCTEXT("FirstDay1", "오늘이 첫날이에요. 운영 중에 눈에 띄는 게 있으면 옆에서 짧게 말씀드릴게요."),
			LOCTEXT("FirstDay2", "아침마다 전날 운영도 짧게 정리해 드릴게요."),
		};
	}
}

#undef LOCTEXT_NAMESPACE
