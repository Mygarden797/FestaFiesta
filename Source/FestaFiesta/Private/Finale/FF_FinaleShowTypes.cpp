// Copyright Epic Games, Inc. All Rights Reserved.

#include "Finale/FF_FinaleShowTypes.h"

DEFINE_LOG_CATEGORY(LogFestaFiestaFinale);

const FFF_FinaleGradeSpec& UFF_FinaleShowData::GetSpec(EFF_FinaleGrade Grade) const
{
	switch (Grade)
	{
	case EFF_FinaleGrade::C: return GradeC;
	case EFF_FinaleGrade::B: return GradeB;
	case EFF_FinaleGrade::A: return GradeA;
	case EFF_FinaleGrade::S: return GradeS;
	default:                 return NoneGrade;
	}
}

namespace FFF_FinaleDefaults
{
	FFF_FinaleGradeSpec MakeSpec(const EFF_FinaleGrade Grade)
	{
		FFF_FinaleGradeSpec Spec;

		// 아래 숫자는 시작점이지 결론이 아니다. 화면을 보고 정해야 하는 값이라
		// 데이터 애셋으로 덮어쓸 수 있게 해뒀다.
		switch (Grade)
		{
		case EFF_FinaleGrade::S:
			Spec.BurstScale = 1.8f;
			Spec.TrailLength = 1.6f;
			Spec.BurstCount = 120;
			Spec.DurationSeconds = 70.f;
			Spec.SimultaneousShells = 4;
			Spec.SpreadRadius = 9000.f;
			Spec.BurstHeightMin = 14000.f;
			Spec.BurstHeightMax = 34000.f;
			Spec.Colors = {
				FLinearColor(1.f, 0.25f, 0.25f), FLinearColor(1.f, 0.75f, 0.2f),
				FLinearColor(0.35f, 1.f, 0.45f), FLinearColor(0.3f, 0.6f, 1.f),
				FLinearColor(0.85f, 0.4f, 1.f),  FLinearColor(1.f, 1.f, 1.f)
			};
			break;

		case EFF_FinaleGrade::A:
			Spec.BurstScale = 1.5f;
			Spec.TrailLength = 1.4f;
			Spec.BurstCount = 60;
			Spec.DurationSeconds = 50.f;
			Spec.SimultaneousShells = 3;
			Spec.SpreadRadius = 7000.f;
			Spec.BurstHeightMin = 15000.f;
			Spec.BurstHeightMax = 30000.f;
			Spec.Colors = {
				FLinearColor(1.f, 0.3f, 0.3f), FLinearColor(1.f, 0.8f, 0.25f),
				FLinearColor(0.35f, 0.7f, 1.f), FLinearColor(1.f, 1.f, 1.f)
			};
			break;

		case EFF_FinaleGrade::B:
			Spec.BurstScale = 1.2f;
			Spec.TrailLength = 1.2f;
			Spec.BurstCount = 30;
			Spec.DurationSeconds = 35.f;
			Spec.SimultaneousShells = 2;
			Spec.SpreadRadius = 5000.f;
			Spec.BurstHeightMin = 16000.f;
			Spec.BurstHeightMax = 26000.f;
			Spec.Colors = {
				FLinearColor(1.f, 0.4f, 0.3f), FLinearColor(1.f, 0.85f, 0.3f),
				FLinearColor(0.4f, 0.7f, 1.f)
			};
			break;

		case EFF_FinaleGrade::C:
			Spec.BurstScale = 1.0f;
			Spec.TrailLength = 1.0f;
			Spec.BurstCount = 12;
			Spec.DurationSeconds = 20.f;
			Spec.SimultaneousShells = 1;
			Spec.SpreadRadius = 3000.f;
			Spec.BurstHeightMin = 16000.f;
			Spec.BurstHeightMax = 22000.f;
			Spec.Colors = {
				FLinearColor(1.f, 0.5f, 0.3f), FLinearColor(1.f, 0.9f, 0.4f)
			};
			break;

		default:
			// 미구매. 축제가 끝났다는 신호는 남긴다.
			Spec.BurstScale = 0.7f;
			Spec.TrailLength = 0.8f;
			Spec.BurstCount = 3;
			Spec.DurationSeconds = 10.f;
			Spec.SimultaneousShells = 1;
			Spec.SpreadRadius = 1200.f;
			Spec.BurstHeightMin = 15000.f;
			Spec.BurstHeightMax = 18000.f;
			Spec.Colors = { FLinearColor(1.f, 0.85f, 0.6f) };
			break;
		}

		return Spec;
	}
}
